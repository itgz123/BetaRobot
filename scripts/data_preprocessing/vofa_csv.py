#!/usr/bin/env python3
"""VOFA+ JustFloat CSV 的通用预处理。

各分析脚本（拨盘阶跃 / 定速巡航 / 开环力矩 / 位置环）里反复出现、且与具体通道语义无关的部分：

    CSV 布局：首行表头 I0,I1,...；I0 = DWT 时间戳，I1~ 依次对应 CH1~。
    时间戳：32 位 µs 计数器约 4295 s 回绕一次；导出首行常是上一段会话的缓冲残值；
            MCU 复位或两次导出拼接会让时间戳回退 / 大跳，必须按硬边界切段，不能连算。

本模块只做「读进来、自检、把时间轴洗干净」。通道含义、可选列降级、具体统计仍留在各分析脚本。
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

DWT_MOD = float(2**32)  # DWT 32 位 µs 计数器的模
_TIME_SCALE = {"us": 1e-6, "ms": 1e-3, "s": 1.0}


def load_csv(path: Path | str, *, min_rows: int = 3) -> pd.DataFrame:
    """读 CSV 并自检布局（表头必须是连续编号 I0,I1,...）。"""
    path = Path(path)
    if not path.is_file():
        sys.exit(f"[错误] 文件不存在: {path}")
    df = pd.read_csv(path)
    names = list(df.columns)
    if not names or names[0] != "I0":
        sys.exit(f"[错误] 表头第一列应为 I0，实际为 {names[0] if names else '(空)'}")
    expect = [f"I{k}" for k in range(len(names))]
    if names != expect:
        sys.exit(f"[错误] 表头不是连续编号 I0..I{len(names) - 1}: {names[:6]}...")
    if len(df) < min_rows:
        sys.exit(f"[错误] 数据行太少: {len(df)}")
    return df


def infer_time_unit(ts: np.ndarray, unit: str = "auto") -> str:
    """按正间隔的中位数猜时间单位。DWT_GetTimeUs 在 500 Hz 下中位间隔约 2000。"""
    if unit != "auto":
        if unit not in _TIME_SCALE:
            sys.exit(f"[错误] 未知时间单位: {unit}")
        return unit
    d = np.diff(np.asarray(ts, dtype=np.float64))
    d = d[d > 0]
    med = float(np.median(d)) if d.size else 0.0
    if med >= 1000:
        return "us"
    if med >= 1:
        return "ms"
    return "s"


def time_scale(unit: str) -> float:
    """原始时间戳单位 → 秒的比例。unit 为 auto 时先调用 infer_time_unit。"""
    if unit not in _TIME_SCALE:
        sys.exit(f"[错误] 未知时间单位: {unit}（auto 请先 infer_time_unit）")
    return _TIME_SCALE[unit]


def unwrap_dwt(ts: np.ndarray) -> tuple[np.ndarray, int]:
    """32 位 µs 计数器回绕：负跳变且补上 2^32 后仍是正常拍间隔的，按 +2^32 解卷绕。

    判据是 d + 2^32 落在 (0, 2^31)：真回绕补回来就是一拍（µs 级到千秒级都在这半个模内），
    复位重新计时 / 两次导出拼接的小幅回退补上 2^32 会变成近乎一整圈，不会被当成回绕。
    """
    t = np.asarray(ts, dtype=np.float64).copy()
    if t.size < 2:
        return t, 0
    d = np.diff(t)
    healed = d + DWT_MOD
    wraps = (d < 0) & (healed > 0) & (healed < 2**31)
    nwrap = int(wraps.sum())
    if nwrap:
        t += np.cumsum(np.r_[0.0, np.where(wraps, DWT_MOD, 0.0)])
    return t, nwrap


def drop_head_glitch(ts: np.ndarray) -> int:
    """VOFA 导出首部常夹着上一段会话的缓冲残值。返回应丢弃的头部行数。

    两种形态都见过，且都只出在头部：
      1. 破坏单调：首行 >= 下一行，其后才开始正常递增；
      2. 另一量级：首行之后严格递增，但首行与第二行的间隔远大于其后的中位周期
         （残留缓冲的时间戳跳到了别的会话，并不一定更大）。
    中段的回退 / 大跳不在这里丢，留给 split_segments 切段。
    """
    t = np.asarray(ts, dtype=np.float64)
    head = 0
    while head + 1 < t.size and t[head] >= t[head + 1]:
        head += 1
    rest = t[head:]
    if rest.size >= 3:
        d = np.diff(rest)
        tail = d[1:]
        pos = tail[tail > 0]
        med = float(np.median(pos)) if pos.size else 0.0
        # 只看丢完非单调行之后的**第一拍**：它自己就跨了两个量级，后面是正常周期。
        if med > 0 and (d[0] <= 0 or d[0] > 1e3 * med):
            head += 1
    return head


def monotonic_breaks(ts: np.ndarray) -> np.ndarray:
    """中段非单调处的行下标（diff<=0 的右端）。空数组表示时间戳严格递增。"""
    t = np.asarray(ts, dtype=np.float64)
    if t.size < 2:
        return np.array([], dtype=int)
    return np.flatnonzero(np.diff(t) <= 0)


def split_segments(ts: np.ndarray, gap_factor: float = 10.0) -> list[tuple[int, int]]:
    """按时间戳回退 (d<=0) 或大跳切段，返回左闭右开的 [start, end)。

    段内时间戳严格递增，积分 / 时长统计才可信。gap_factor<=0 时只按回退切。
    短于 2 个样本的碎片段丢弃。
    """
    t = np.asarray(ts, dtype=np.float64)
    n = t.size
    if n < 2:
        return [(0, n)] if n else []
    d = np.diff(t)
    pos = d[d > 0]
    med = float(np.median(pos)) if pos.size else 0.0
    cuts = np.flatnonzero(d <= 0)
    if gap_factor > 0 and med > 0:
        cuts = np.union1d(cuts, np.flatnonzero(d > gap_factor * med))
    bounds = np.concatenate(([0], cuts + 1, [n])).astype(int)
    return [(int(a), int(b)) for a, b in zip(bounds[:-1], bounds[1:]) if b - a >= 2]


def gap_rows(ts: np.ndarray, gap_factor: float = 10.0) -> np.ndarray:
    """大跳（间隔 > gap_factor × 中位正间隔）右侧的行下标。不含回退。

    给「不改写时间轴、只把断点当硬边界下传」的分析用。
    """
    t = np.asarray(ts, dtype=np.float64)
    if gap_factor <= 0 or t.size < 2:
        return np.array([], dtype=int)
    d = np.diff(t)
    pos = d[d > 0]
    med = float(np.median(pos)) if pos.size else 0.0
    if med <= 0:
        return np.array([], dtype=int)
    return np.flatnonzero(d > gap_factor * med) + 1


def build_time(ts: np.ndarray, segs: list[tuple[int, int]], scale: float) -> tuple[np.ndarray, float]:
    """拼出单调时间轴（秒）：各段首尾相接，段间停表的墙钟时间不计入。

    返回 (t_s, skipped_s)。段外的样本保持 0，调用方应按段取。
    """
    t_raw = np.asarray(ts, dtype=np.float64)
    t = np.zeros(t_raw.size, dtype=np.float64)
    off = 0.0
    for a, b in segs:
        s = (t_raw[a:b] - t_raw[a]) * scale
        t[a:b] = off + s
        off += float(s[-1]) if s.size else 0.0
    skipped = 0.0
    for (_, b), (a2, _) in zip(segs[:-1], segs[1:]):
        skipped += float(t_raw[a2] - t_raw[b - 1]) * scale
    return t, skipped


def to_seconds(ts: np.ndarray, unit: str = "auto") -> tuple[np.ndarray, str]:
    """整段时间戳 → 相对首样本的秒。不做切段；回绕 / 断点请先 unwrap + split。"""
    t = np.asarray(ts, dtype=np.float64)
    unit = infer_time_unit(t, unit)
    if t.size == 0:
        return t, unit
    return (t - t[0]) * time_scale(unit), unit


def median_period(ts: np.ndarray, segs: list[tuple[int, int]], scale: float) -> float:
    """段内中位周期（秒）。跨段的两拍是停表时间，不计入。"""
    t = np.asarray(ts, dtype=np.float64)
    diffs = np.concatenate([np.diff(t[a:b]) for a, b in segs if b - a >= 2]) if segs else np.array([])
    pos = diffs[diffs > 0]
    if pos.size == 0:
        return 0.0
    return float(np.median(pos)) * scale


def prepare_time(
    ts: np.ndarray,
    *,
    unit: str = "auto",
    gap_factor: float = 10.0,
    unwrap: bool = True,
    drop_head: bool = True,
) -> dict:
    """一次做完通用时间轴清洗，返回可直接交给分析的量。

    键：
        ts           清洗后的原始时间戳（已解卷绕、已丢头部残值）
        dropped      丢弃的头部行数
        nwrap        解卷绕次数
        unit / scale 时间单位及其到秒的比例
        segs         切段 [start, end)
        t_s          段内拼接的单调秒轴
        skipped_s    段间停表秒数
        period_s     段内中位周期（秒）
        breaks       清洗后仍非单调的行下标（正常应为空；非空说明中段还有乱序）
    """
    t = np.asarray(ts, dtype=np.float64).copy()
    nwrap = 0
    if unwrap:
        t, nwrap = unwrap_dwt(t)
    dropped = drop_head_glitch(t) if drop_head else 0
    if dropped:
        t = t[dropped:]
    unit = infer_time_unit(t, unit)
    scale = time_scale(unit)
    segs = split_segments(t, gap_factor)
    t_s, skipped = build_time(t, segs, scale)
    return {
        "ts": t,
        "dropped": dropped,
        "nwrap": nwrap,
        "unit": unit,
        "scale": scale,
        "segs": segs,
        "t_s": t_s,
        "skipped_s": skipped,
        "period_s": median_period(t, segs, scale),
        "breaks": monotonic_breaks(t),
    }


def runs_of(mask: np.ndarray) -> list[tuple[int, int]]:
    """布尔掩码里的连续 True 段（左闭右开）。"""
    m = np.asarray(mask, dtype=bool)
    if m.size == 0:
        return []
    edges = np.flatnonzero(np.diff(np.concatenate(([False], m, [False]))))
    return list(zip(edges[::2].tolist(), edges[1::2].tolist()))
