#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_trig_lut.py — bsp_math 查表三角函数表生成器

为 bsp_math_trig_lut.h 生成正弦查表 C 文件 bsp_math_trig_lut.c。

表设计
------
- 只存储 sin 在四分之一周期 [0, π/2] 上的采样值：M 个区间，M+1 个 float32 条目，
  table[i] = float32(sin(i·(π/2)/M))。
- 同一 .c 内用 `#if BSP_MATH_TRIG_TABLE_SIZE == <M>` 切换精度档，只编译被选中的表。
- cos 通过 sin(x+π/2) 复用同表；奇象限用"从高端反向索引"实现镜像。

精度分析
--------
- float32 机器精度 ε = 2^-23 ≈ 1.19e-7（约 7.2 位有效十进制）。
- 线性插值误差上界 h²/8（|sin″|≤1）。要求 < ε ⇒ h < sqrt(8ε) ≈ 9.77e-4。
- 本脚本用 numpy 复刻 C 管线，实测每档：
  * 插值+量化(纯)：精确 float64 相位位置 + float32 表值 + double 插值 ——
    只含"查表线性插值"与表值量化的贡献，是判定满精度档的标准；
  * 整管误差：float32 输入走完整管线（归一/象限/插值/符号），
    分 [0,2π) / 负区间报告（其中含索引计算的 float32 舍入，约 ε 为下限）。
- 找出满足"插值+量化 < ε"的最小档位 M（满精度档）。

用法
----
    python gen_trig_lut.py                          # 分析 128..8192，生成 256/1024/4096
    python gen_trig_lut.py --sizes 256 1024 4096 8192 -o ../bsp_math_trig_lut.c
    python gen_trig_lut.py --target-eps 1.19e-7     # 自定义目标误差
    python gen_trig_lut.py --no-verify              # 生成后跳过自检
"""

import argparse
import sys

import numpy as np

# ---------------- 常数（float64 真值 + float32 逐位镜像） ----------------
PI_F64 = np.float64(np.pi)
TWO_PI_F64 = np.float64(2.0) * PI_F64
QUARTER_F64 = np.float64(0.5) * PI_F64

INV_2PI_F32 = np.float32(1.0 / (2.0 * np.pi))   # 0.159154943f  (1/2π)
INV_QUARTER_F32 = np.float32(2.0 / np.pi)       # 0.636619773f  (2/π)
TWO_PI_F32 = np.float32(2.0 * np.pi)
QUARTER_F32 = np.float32(0.5 * np.pi)


def make_table(M: int) -> np.ndarray:
    """table[i] = float32(sin(i·(π/2)/M))，i ∈ [0, M]。"""
    idx = np.arange(M + 1, dtype=np.float64)
    vals = np.sin(idx * (QUARTER_F64 / np.float64(M)))
    return vals.astype(np.float32)


def lerp(table: np.ndarray, p: np.ndarray) -> np.ndarray:
    """float32 线性插值，与 C 的 BSP_Math_TrigTableLerp 逐位一致。

    p: float32 数组，取值 [0, M]。p == M 时钳制到 [M-1, 1.0]。
    """
    M = table.size - 1
    i = p.astype(np.uint32)
    frac = p - i.astype(np.float32)
    over = i >= np.uint32(M)
    i = np.where(over, np.uint32(M - 1), i)
    frac = np.where(over, np.float32(1.0), frac)
    v0 = table[i]
    v1 = table[i + 1]
    return v0 + frac * (v1 - v0)


def _err_stats(err: np.ndarray):
    """返回 (max 绝对误差, rms 绝对误差)。"""
    e = err.astype(np.float64)
    return float(np.max(e)), float(np.sqrt(np.mean(e * e)))


def measure_interp_error(M: int, table: np.ndarray):
    """纯插值 + 表量化误差（用户判定"满精度档"的标准）。

    用精确 float64 相位位置 u = φ·(2M/π)（覆盖全部表索引点与每个区间内 32 个偏移），
    取 float32 表值，在 double 下做线性插值，对照 float64 sin(φ)。
    该误差只含"查表线性插值"本身与表值量化的贡献，与索引计算的 float32 舍入无关。
    """
    offsets = np.linspace(0.0, 1.0, 33)[:-1]
    grid = (np.arange(M, dtype=np.float64)[:, None] + offsets[None, :]).ravel()
    grid = np.append(grid, np.float64(M))          # 右端点 φ=π/2
    u = grid                                        # 位置 ∈ [0, M]
    i0 = np.floor(u).astype(np.int64)
    frac = u - i0
    over = i0 >= np.int64(M)                        # u==M 时钳制：i=M-1、frac=1（与 C 一致）
    i = np.where(over, np.int64(M - 1), i0)
    frac = np.where(over, 1.0, frac)
    v0 = table[i].astype(np.float64)
    v1 = table[np.minimum(i + 1, np.int64(M))].astype(np.float64)
    got = v0 + frac * (v1 - v0)
    phi = u / np.float64(M) * QUARTER_F64
    ref = np.sin(phi)
    return _err_stats(np.abs(got - ref))


def measure_pipeline(theta64: np.ndarray, M: int, table: np.ndarray):
    """ModeB：整管误差。float32 θ 走完整管线，对照 sin((double)θ_f32)。"""
    scale = np.float32(M) * INV_QUARTER_F32
    m_f = np.float32(M)
    theta = theta64.astype(np.float32)
    # 1) 归一化到 [0, 2π)（int 截断 + 负数补偿，与 C 一致）
    t = theta - TWO_PI_F32 * np.trunc(theta * INV_2PI_F32).astype(np.int32).astype(np.float32)
    t = np.where(t < np.float32(0.0), t + TWO_PI_F32, t)
    # 2) 象限与相位
    qf = t * INV_QUARTER_F32
    q = qf.astype(np.uint32)
    q = np.where(q > np.uint32(3), np.uint32(3), q)
    phi = t - q.astype(np.float32) * QUARTER_F32
    u = phi * scale
    u = np.minimum(u, m_f)
    # 3) 插值 + 符号
    ps = np.where((q & np.uint32(1)) != np.uint32(0), m_f - u, u)
    sv = lerp(table, ps)
    sv = np.where((q & np.uint32(2)) != np.uint32(0), -sv, sv)
    ref = np.sin(theta.astype(np.float64)).astype(np.float32)
    return _err_stats(np.abs(sv - ref))


def fmt_f32(v: np.float32) -> str:
    """float32 → 精确往返的十进制字面量，带 f 后缀，贴合模块 0.0f/1.0f 风格。"""
    f = float(np.float32(v))
    s = format(f, ".9g")
    if "." not in s and "e" not in s and "E" not in s and "n" not in s:
        s += ".0"
    return s + "f"


def gen_c_file(sizes, out_path) -> None:
    lines = [
        "/**",
        " * @file bsp_math_trig_lut.c",
        " * @brief 自研查表三角函数表（四分之一周期 [0,π/2] 正弦表）",
        " *",
        " * @note 本文件由 tools/gen_trig_lut.py 自动生成，请勿手动修改",
        " * @note 用 BSP_MATH_TRIG_TABLE_SIZE 宏（四分之一周期区间数 M）切换精度档，",
        " *       只编译被选中的表；表存放于 flash（.rodata）",
        " */",
        "",
        '#include "bsp_math_trig_lut.h"',
        "",
    ]
    for i, M in enumerate(sizes):
        kw = "#if" if i == 0 else "#elif"
        lines.append("%s BSP_MATH_TRIG_TABLE_SIZE == %d" % (kw, M))
        lines.append("const float BSP_Math_SinTable[%d] = {" % (M + 1))
        vals = [fmt_f32(v) for v in make_table(M)]
        for j in range(0, len(vals), 12):
            lines.append("    " + ", ".join(vals[j:j + 12]) + ",")
        lines.append("};")
        lines.append("")
    lines.append("#else")
    lines.append(
        '#error "BSP_MATH_TRIG_TABLE_SIZE 必须为 %s 之一"'
        % (" / ".join(str(s) for s in sizes))
    )
    lines.append("#endif /* BSP_MATH_TRIG_TABLE_SIZE */")
    lines.append("")
    with open(out_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))
    print("[gen] 已生成 %s，包含档位：%s" % (out_path, " / ".join(map(str, sizes))))


def main() -> int:
    ap = argparse.ArgumentParser(description="bsp_math 查表三角函数表生成器")
    ap.add_argument("--sizes", type=int, nargs="*", default=[256, 1024, 2048, 4096],
                    help="写入 .c 的精度档（四分之一表区间数 M），默认 256 1024 2048 4096")
    ap.add_argument("--target-eps", type=float, default=float(2.0 ** -23),
                    help="满精度目标误差，默认 2^-23 ≈ 1.19e-7")
    ap.add_argument("-o", "--out", default="../bsp_math_trig_lut.c",
                    help="输出 C 文件路径（相对脚本目录）")
    ap.add_argument("--no-verify", action="store_true", help="生成后跳过自检")
    args = ap.parse_args()

    eps = float(args.target_eps)
    print("=== bsp_math 查表三角函数精度分析 ===")
    print("目标：线性插值误差 < float32 机器精度 ε = 2^-23 ≈ %.3e" % eps)
    print("")

    # 分析候选档位：128..8192（含生成档）
    candidates = [s for s in (128, 256, 512, 1024, 2048, 4096, 8192)]

    header = "  M    | 理论插值 | 插值+量化(纯) | 整管[0,2π) | 整管负区间 | 满精度档"
    print(header)
    print("-" * len(header))

    full_prec = None
    results = {}
    for M in candidates:
        table = make_table(M)
        h = float(QUARTER_F64) / M
        theory = h * h / 8.0
        a_max, a_rms = measure_interp_error(M, table)
        num = max(4 * M * 8, 1 << 16)
        theta_full = np.linspace(0.0, TWO_PI_F64, num, endpoint=False)
        special = np.array([0.0, QUARTER_F64, np.pi, 3.0 * QUARTER_F64,
                            2.0 * np.pi - 1e-3, np.pi / 2.0 - 1e-4,
                            np.pi / 2.0 + 1e-4, -1e-4, 1e-4], dtype=np.float64)
        b_full, _ = measure_pipeline(np.concatenate([theta_full, special]), M, table)
        theta_neg = np.linspace(-4.0 * TWO_PI_F64, 0.0, num, endpoint=False)
        b_neg, _ = measure_pipeline(theta_neg, M, table)

        star = ""
        if a_max < eps and full_prec is None:
            full_prec = M
            star = "  ★ 满精度"
        elif a_max < eps:
            star = "  (满足)"
        print("  %-4d | %9.2e | %13.2e | %11.2e | %12.2e |%s"
              % (M, theory, a_max, b_full, b_neg, star))
        results[M] = (a_max, b_full, b_neg)

    if full_prec is None:
        print("\n[FAIL] 候选档位内均未达到目标误差 %.3e，请增大 M。" % eps)
        return 1
    print("\n结论：满精度档为 M = %d（表 %d 项 ≈ %.1f KB flash）"
          % (full_prec, full_prec + 1, (full_prec + 1) * 4 / 1024.0))
    if full_prec not in args.sizes:
        print("警告：满精度档 %d 不在生成列表 %s 中，建议 --sizes 包含它。"
              % (full_prec, args.sizes))

    gen_c_file(args.sizes, args.out)

    # 生成后自检：生成档中必须包含满精度档（低档位是速度优先，不要求达 ε）
    if not args.no_verify:
        print("\n=== 生成后自检 ===")
        ok = True
        for M in args.sizes:
            table = make_table(M)
            a_max, a_rms = measure_interp_error(M, table)
            if a_max < eps:
                status = "满精度(PASS)"
            else:
                status = "低精度档(速度优先)"
                if M == full_prec:
                    status = "满精度(PASS)"
            print("  M=%-4d 插值+量化 max=%.3e  [%s]" % (M, a_max, status))
        if full_prec not in args.sizes:
            print("[WARN] 生成档位 %s 不包含满精度档 %d（若刻意只用低档位可忽略）"
                  % (args.sizes, full_prec))
            ok = False
        if not ok:
            return 1
        print("[OK] 生成档位包含满精度档 M=%d" % full_prec)

    return 0


if __name__ == "__main__":
    sys.exit(main())
