#!/usr/bin/env python3
"""把 lib_music_simple 的简谱数组渲染成 FLAC，用来听辨/核对曲谱。

流程：
  1. build.sh 用 gcc 把真实曲谱源码编成 libmusic_host.so（缺库/源码更新时自动重建）
  2. ctypes 读取 MusicSequence_t（就是固件里那份数组）
  3. 逐脉冲合成带限方波 + 线性衰减包络，模拟 drv_buzzer_simple 的 decay_ms
  4. 4 kHz 二阶谐振带通（模拟无源蜂鸣器），归一化后写 FLAC

环境（pyenv 3.12.6，见同目录 .python-version）：
    python3 -m pip install -r requirements.txt     # 不要加 --user

示例：
    python3 render_music.py --list
    python3 render_music.py --sequence MUSIC_SEE_YOU_AGAIN_GUITAR
    python3 render_music.py -s twinkle_star --no-resonance -o /tmp/twinkle_dry.flac
    python3 render_music.py --all
"""

from __future__ import annotations

import argparse
import ctypes
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

try:
    import soundfile as sf
except ImportError:
    sys.exit(
        "缺少 soundfile。先安装依赖（不要加 --user）：\n"
        f"  python3 -m pip install -r {Path(__file__).resolve().parent / 'requirements.txt'}"
    )

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]  # host -> lib_music_simple -> lib -> 仓库根
LIB_PATH = HERE / "build" / "libmusic_host.so"
DEFAULT_OUT_DIR = HERE / "build" / "audio"

# 与固件 app/half_rudder_{chassis,gimbal} 的 BuzzerConfig 对齐
DEFAULT_DECAY_MS = 500.0
DEFAULT_SR = 48000
DEFAULT_RESONANCE_HZ = 4000.0
DEFAULT_Q = 6.0
DEFAULT_BANDLIMIT_HZ = 20000.0
DEFAULT_FADE_MS = 0.0
DEFAULT_AMP = 0.25
DEFAULT_VOLUME = 1.0


# ============================================================
#  C 库 + 数据结构
# ============================================================
class MusicPulse(ctypes.Structure):
    _fields_ = [
        ("freq_hz", ctypes.c_uint16),
        ("duration_ms", ctypes.c_uint16),
        ("volume_pct", ctypes.c_uint8),
    ]


class MusicSequence(ctypes.Structure):
    _fields_ = [
        ("pulses", ctypes.POINTER(MusicPulse)),
        ("count", ctypes.c_uint16),
    ]


def _source_paths() -> list[Path]:
    """参与 .so 构建的源文件/头文件，用于判断是否需要重建。"""
    music = ROOT / "lib" / "lib_music_simple"
    return [
        HERE / "app_cfg.h",
        HERE / "build.sh",
        HERE / "music_host.c",
        HERE / "music_host.h",
        ROOT / "drv" / "drv_buzzer_simple" / "drv_buzzer_note.h",
        music / "lib_music_simple.c",
        music / "lib_music_simple.h",
        music / "music_custom" / "music_custom.c",
        music / "music_custom" / "music_custom.h",
        music / "twinkle_star" / "twinkle_star.c",
        music / "twinkle_star" / "twinkle_star.h",
        music / "happy_birthday" / "happy_birthday.c",
        music / "happy_birthday" / "happy_birthday.h",
        music / "see_you_again" / "see_you_again_guitar" / "see_you_again_guitar.c",
        music / "see_you_again" / "see_you_again_guitar" / "see_you_again_guitar.h",
    ]


def _library_is_fresh() -> bool:
    if not LIB_PATH.exists():
        return False
    built = LIB_PATH.stat().st_mtime
    return all(
        (not p.exists()) or p.stat().st_mtime <= built for p in _source_paths()
    )


def build_library(force: bool = False) -> None:
    if not force and _library_is_fresh():
        return
    print(f"[build] {HERE / 'build.sh'}", file=sys.stderr)
    subprocess.run(["sh", str(HERE / "build.sh")], check=True)


def load_library(force_build: bool = False) -> ctypes.CDLL:
    build_library(force_build)
    lib = ctypes.CDLL(str(LIB_PATH))
    lib.MusicHostCount.argtypes = []
    lib.MusicHostCount.restype = ctypes.c_uint32
    lib.MusicHostName.argtypes = [ctypes.c_uint32]
    lib.MusicHostName.restype = ctypes.c_char_p
    lib.MusicHostSequence.argtypes = [ctypes.c_uint32]
    lib.MusicHostSequence.restype = ctypes.POINTER(MusicSequence)
    return lib


def read_sequences(lib: ctypes.CDLL) -> list[tuple[str, list[tuple[int, int, int]]]]:
    """返回 [(名称, [(freq_hz, duration_ms, volume_pct), ...]), ...]。"""
    out: list[tuple[str, list[tuple[int, int, int]]]] = []
    for i in range(int(lib.MusicHostCount())):
        name = lib.MusicHostName(i)
        seq_ptr = lib.MusicHostSequence(i)
        if not name or not seq_ptr:
            continue
        seq = seq_ptr.contents
        count = int(seq.count)
        pulses = [
            (int(p.freq_hz), int(p.duration_ms), int(p.volume_pct))
            for p in seq.pulses[:count]
        ]
        out.append((name.decode("ascii"), pulses))
    return out


# ============================================================
#  选中序列
# ============================================================
def resolve_names(available: list[str], wanted: list[str]) -> list[str]:
    result: list[str] = []
    for raw in wanted:
        key = raw.strip().upper()
        if key in available:
            result.append(key)
            continue
        prefixed = key if key.startswith("MUSIC_") else f"MUSIC_{key}"
        if prefixed in available:
            result.append(prefixed)
            continue
        matches = [n for n in available if key in n]
        if len(matches) == 1:
            result.append(matches[0])
            continue
        if not matches:
            sys.exit(f"找不到序列 {raw!r}；可用：\n  " + "\n  ".join(available))
        sys.exit(f"序列 {raw!r} 匹配到多个：{', '.join(matches)}")
    return result


def describe(pulses: list[tuple[int, int, int]]) -> tuple[int, list[int], int, str]:
    total_ms = sum(duration for _, duration, _ in pulses)
    freqs = [freq for freq, _, _ in pulses if freq]
    rests = len(pulses) - len(freqs)
    freq_text = f"{min(freqs)}-{max(freqs)} Hz" if freqs else "-"
    return total_ms, freqs, rests, freq_text


# ============================================================
#  蜂鸣器模型：方波 + 线性衰减
# ============================================================
def synth_sequence(
    pulses: list[tuple[int, int, int]],
    *,
    sr: int,
    decay_ms: float,
    speed: float,
    default_volume: float,
    fade_ms: float,
    amp: float,
    bandlimit_hz: float,
) -> np.ndarray:
    chunks: list[np.ndarray] = []
    for freq, duration_ms, volume_pct in pulses:
        n = max(1, int(round(duration_ms / 1000.0 * sr / speed)))
        if freq == 0:
            chunks.append(np.zeros(n, dtype=np.float64))
            continue

        volume = (volume_pct / 100.0) if volume_pct else default_volume
        volume = min(max(volume, 0.0), 1.0)

        # 与固件一致：衰减时长 = min(decay_ms, 本拍时长)，速度对两者同比缩放
        duration_s = n / sr
        if decay_ms > 0.0:
            env_decay_s = min((decay_ms / 1000.0) / speed, duration_s)
            if env_decay_s > 0.0:
                env = 1.0 - np.minimum(
                    np.arange(n, dtype=np.float64) / (env_decay_s * sr), 1.0
                )
            else:
                env = np.ones(n, dtype=np.float64)
        else:
            env = np.ones(n, dtype=np.float64)

        # 固件音量 0~1 → 占空比 0~0.5；方波去掉直流（电脑声卡是 AC 耦合）
        duty = volume * 0.5 * env

        if bandlimit_hz > 0.0:
            # 带限脉冲串：傅里叶级数到 bandlimit，避免 48k 采样的高次谐波折叠。
            # 高电平在每周期开头（与固件 UG 重置计数器后的 PWM1 输出一致）：
            #   x(t) = Σ 2/(πn) · sin(πnd) · cos(2πnft - πnd)
            n_max = int(min(bandlimit_hz, 0.49 * sr) // freq)
            n_max = max(n_max, 1)  # 基频超过带限时至少保留基频
            harmonics = np.arange(1.0, n_max + 1.0)
            t = np.arange(n, dtype=np.float64) / sr
            theta = (2.0 * math.pi * freq * t)[:, None] * harmonics[None, :]
            phase_shift = math.pi * harmonics[None, :] * duty[:, None]
            coeff = (2.0 / (math.pi * harmonics))[None, :] * np.sin(phase_shift)
            wave = np.sum(coeff * np.cos(theta - phase_shift), axis=1)
        else:
            # 原始采样方波（会混叠，用于对比）
            phase = (np.arange(n, dtype=np.float64) * (freq / sr)) % 1.0
            wave = np.where(phase < duty, 1.0 - duty, -duty)

        # 淡入淡出默认关闭；decay_ms = 0 时可手动打开，避免频率切换“咔哒”
        nf = min(int(round(fade_ms / 1000.0 * sr)), n // 2)
        if nf > 0:
            wave = wave.copy()
            ramp = np.linspace(0.0, 1.0, nf, endpoint=False)
            wave[:nf] *= ramp
            wave[-nf:] *= ramp[::-1]

        chunks.append(wave * amp)

    if not chunks:
        return np.zeros(0, dtype=np.float64)
    return np.concatenate(chunks)


# ============================================================
#  二阶 IIR：用冲激响应做 FFT 卷积（numpy-only，避免逐样本 Python 循环）
# ============================================================
def _normalize_coeffs(
    b: tuple[float, float, float], a: tuple[float, float, float]
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    a0 = a[0]
    return tuple(x / a0 for x in b), tuple(x / a0 for x in a)


def bandpass_coeffs(
    sr: int, f0: float, q: float
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """RBJ 二阶带通（峰值增益 0 dB）。"""
    if not 0.0 < f0 < sr / 2.0:
        sys.exit(f"谐振频率必须在 (0, {sr / 2}) 内：{f0}")
    if q <= 0.0:
        sys.exit("Q 必须 > 0")
    w0 = 2.0 * math.pi * f0 / sr
    alpha = math.sin(w0) / (2.0 * q)
    b = (alpha, 0.0, -alpha)
    a = (1.0 + alpha, -2.0 * math.cos(w0), 1.0 - alpha)
    return _normalize_coeffs(b, a)


def lowpass_coeffs(
    sr: int, fc: float, q: float = 1.0 / math.sqrt(2.0)
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """RBJ 二阶低通（Butterworth 默认 Q）。"""
    if not 0.0 < fc < sr / 2.0:
        sys.exit(f"低通频率必须在 (0, {sr / 2}) 内：{fc}")
    w0 = 2.0 * math.pi * fc / sr
    cos_w0 = math.cos(w0)
    alpha = math.sin(w0) / (2.0 * q)
    b = ((1.0 - cos_w0) / 2.0, 1.0 - cos_w0, (1.0 - cos_w0) / 2.0)
    a = (1.0 + alpha, -2.0 * cos_w0, 1.0 - alpha)
    return _normalize_coeffs(b, a)


def _impulse_response(
    b: tuple[float, float, float], a: tuple[float, float, float], n: int
) -> np.ndarray:
    ir = np.empty(n, dtype=np.float64)
    x1 = x2 = y1 = y2 = 0.0
    b0, b1, b2 = b
    _, a1, a2 = a
    for i in range(n):
        x = 1.0 if i == 0 else 0.0
        y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        ir[i] = y
        x2, x1 = x1, x
        y2, y1 = y1, y
    return ir


def apply_biquad(
    x: np.ndarray,
    b: tuple[float, float, float],
    a: tuple[float, float, float],
    f0: float,
    q: float,
    sr: int,
) -> np.ndarray:
    if x.size == 0:
        return x
    # 8 个时间常数足够让冲激响应衰减；截断成 FIR 后用 FFT 做因果卷积
    tail_s = 8.0 * q / (math.pi * max(f0, 1.0))
    n_ir = int(min(65536, max(2048, math.ceil(tail_s * sr))))
    n_ir = min(n_ir, x.size)
    ir = _impulse_response(b, a, n_ir)
    nfft = 1 << (x.size + n_ir - 2).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, nfft) * np.fft.rfft(ir, nfft), nfft)[: x.size]
    return y


# ============================================================
#  渲染
# ============================================================
def render_one(
    name: str,
    pulses: list[tuple[int, int, int]],
    args: argparse.Namespace,
    output_path: Path | None,
) -> None:
    data = synth_sequence(
        pulses,
        sr=args.sr,
        decay_ms=args.decay_ms,
        speed=args.speed,
        default_volume=args.default_volume,
        fade_ms=args.fade_ms,
        amp=args.amp,
        bandlimit_hz=args.bandlimit,
    )

    if data.size:
        data = data - data.mean()

        if args.lowpass > 0.0:
            b, a = lowpass_coeffs(args.sr, args.lowpass)
            data = apply_biquad(
                data, b, a, args.lowpass, 1.0 / math.sqrt(2.0), args.sr
            )

        if not args.no_resonance:
            b, a = bandpass_coeffs(args.sr, args.resonance, args.q)
            data = apply_biquad(
                data, b, a, args.resonance, args.q, args.sr
            )
            data = data - data.mean()

        if not args.no_normalize:
            peak = float(np.max(np.abs(data)))
            if peak > 0.0:
                data *= 0.95 / peak

    path = output_path or (args.out_dir.expanduser() / f"{name}.flac")
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(path), data, args.sr, format="FLAC", subtype="PCM_16")

    total_ms, notes, rests, freq_text = describe(pulses)
    peak = float(np.max(np.abs(data))) if data.size else 0.0
    rms = float(np.sqrt(np.mean(data * data))) if data.size else 0.0
    print(
        f"{name}: {len(pulses)} pulses, {total_ms} ms, "
        f"{len(notes)} notes, {rests} rests, {freq_text}"
    )
    print(
        f"  -> {path}  ({data.size / args.sr:.2f} s @ {args.sr} Hz, "
        f"peak {peak:.3f}, rms {rms:.3f})"
    )
    if args.dump:
        for i, (freq, duration_ms, volume_pct) in enumerate(pulses):
            print(f"  [{i:3d}] {freq:5d} Hz  {duration_ms:5d} ms  vol={volume_pct}")


# ============================================================
#  CLI
# ============================================================
def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--list", action="store_true", help="列出所有序列后退出")
    parser.add_argument(
        "--sequence",
        "-s",
        action="append",
        default=[],
        metavar="NAME",
        help="要渲染的序列名（可重复；可省略 MUSIC_ 前缀）",
    )
    parser.add_argument("--all", action="store_true", help="渲染全部序列")
    parser.add_argument(
        "--output", "-o", metavar="PATH", help="单曲输出路径（仅选中一个序列时有效）"
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help=f"输出目录（默认 {DEFAULT_OUT_DIR}）",
    )
    parser.add_argument("--sr", type=int, default=DEFAULT_SR, help="采样率（默认 48000）")
    parser.add_argument(
        "--decay-ms",
        type=float,
        default=DEFAULT_DECAY_MS,
        help="每音线性衰减时长 ms，0 = 不衰减（默认 500，与固件一致）",
    )
    parser.add_argument("--speed", type=float, default=1.0, help="倍速，>1 更快（默认 1.0）")
    parser.add_argument(
        "--default-volume",
        type=float,
        default=DEFAULT_VOLUME,
        help="volume_pct == 0 时使用的音量 0~1（默认 1.0）",
    )
    parser.add_argument(
        "--resonance",
        type=float,
        default=DEFAULT_RESONANCE_HZ,
        help="蜂鸣器谐振频率 Hz（默认 4000）",
    )
    parser.add_argument(
        "--q", type=float, default=DEFAULT_Q, help="谐振 Q 值（默认 6，真实蜂鸣器约 3~10）"
    )
    parser.add_argument(
        "--no-resonance", action="store_true", help="跳过 4 kHz 谐振带通（原始方波）"
    )
    parser.add_argument(
        "--lowpass",
        type=float,
        default=0.0,
        metavar="HZ",
        help="可选低通（0 = 关；串在谐振前，通常会削弱谐振）",
    )
    parser.add_argument(
        "--bandlimit",
        type=float,
        default=DEFAULT_BANDLIMIT_HZ,
        metavar="HZ",
        help="带限方波的谐波上限 Hz（默认 20000；0 = 原始采样方波，会混叠）",
    )
    parser.add_argument(
        "--fade-ms",
        type=float,
        default=DEFAULT_FADE_MS,
        help="每音首尾淡入淡出 ms（默认 0 = 关闭）",
    )
    parser.add_argument(
        "--amp", type=float, default=DEFAULT_AMP, help="滤波前方波幅度（默认 0.25）"
    )
    parser.add_argument(
        "--no-normalize", action="store_true", help="不做峰值归一化"
    )
    parser.add_argument("--dump", action="store_true", help="打印每个脉冲")
    parser.add_argument("--build", action="store_true", help="强制重新编译共享库")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if args.sr <= 0:
        sys.exit("--sr 必须 > 0")
    if args.speed <= 0.0:
        sys.exit("--speed 必须 > 0")
    if args.decay_ms < 0.0:
        sys.exit("--decay-ms 必须 >= 0")
    if not 0.0 <= args.default_volume <= 1.0:
        sys.exit("--default-volume 必须在 0~1")
    if args.fade_ms < 0.0:
        sys.exit("--fade-ms 必须 >= 0")
    if args.bandlimit < 0.0:
        sys.exit("--bandlimit 必须 >= 0")
    if args.amp <= 0.0:
        sys.exit("--amp 必须 > 0")

    lib = load_library(args.build)
    sequences = read_sequences(lib)
    by_name = dict(sequences)
    available = [name for name, _ in sequences]

    if args.list or (not args.sequence and not args.all):
        width = max(len(name) for name in available) if available else 0
        for name, pulses in sequences:
            total_ms, notes, rests, freq_text = describe(pulses)
            print(
                f"{name:<{width}}  {len(pulses):3d} pulses  {total_ms:7d} ms  "
                f"{len(notes):3d} notes  {rests:2d} rests  {freq_text}"
            )
        return 0

    selected = available if args.all else resolve_names(available, args.sequence)

    if args.output and len(selected) != 1:
        sys.exit("--output 只能和单个序列一起用")
    if args.lowpass > 0.0 and not args.no_resonance:
        print(
            "[warn] --lowpass 串在谐振之前，会削弱 4 kHz 谐振；通常只在 --no-resonance 时用",
            file=sys.stderr,
        )

    for name in selected:
        output_path = Path(args.output).expanduser() if args.output else None
        render_one(name, by_name[name], args, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
