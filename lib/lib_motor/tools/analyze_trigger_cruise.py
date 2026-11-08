#!/usr/bin/env python3
"""拨盘「定速巡航」段统计：发弹态下实际转速/电流能跟到几成、卡住多久、走完几发。

配套 app_shoot.c 的"定速巡航"版调试通道表（2026-10-10，**拨盘速度环**）：
    CH1  本发起累计行程(rad)，满 TRIGGER_STEP_RAD 即走完一发
    CH2  实际转速(rad/s，过低通)   CH3 误差   CH4 总输出/电流指令(A)
    CH5  P项(A)  CH6 I项(A)  CH7 D项(A)  CH8 实测电流(A)  CH9 本拍 dt(ms)
    CH10 摩擦前馈(A，未接线恒 0)   CH11 状态机状态(0关/1单发/2三发/3连发/4停止)
    CH12 原始反馈转速(rad/s，未过低通)

⚠ 2026-10-10 第三轮起 app_shoot.c 的拨盘改成**开环定力矩试验**，通道含义又变了，本脚本
   **不再适用**（CH1 变成单圈位置而不是累计行程 → "走完整发"统计是假数；CH2/CH4~CH8 的
   环内量变成 drv 原始反馈；CH11 变成 enable&&fire）。那一轮要看的是"多大扭矩才拨得动"，
   该另写一个按"驱动条件(CH11)分段 → 段内转速/转角/电流"的脚本，别硬套这份。

⚠ 本版（定速巡航）**日志里没有目标转速通道**（拨盘目标恒为 TRIGGER_DRIVE_SPEED），故目标用
  --ref 给定值；活跃/发弹段用 CH11 的状态判，不是用"设定值非零"判 —— 后者所需的 setpoint
  通道这一版固件里已经没有了（旧版 CH1 是目标转速、CH11 是旋钮，别拿旧脚本的默认列名套新日志）。

CSV 布局（VOFA+ JustFloat 导出）：首行表头 I0,I1,...；I0 = 时间戳(DWT µs)，
I1~ 依次对应 CH1~。文件里若含 MCU 复位后重新计时（时间戳回退）或两次导出拼接，会按硬
边界切段：段内不跨边界连算，总时长按各段之和计（段间停表的墙钟时间不计入）。

用法：
    python3 analyze_trigger_cruise.py ../../ignore/vofa+.csv
    python3 analyze_trigger_cruise.py log.csv --ref 20 --stick-band 0.4
    python3 analyze_trigger_cruise.py log.csv --state-col I11 --firing-states 1,2,3,4
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# 默认列名 = app_shoot.c 当前调试块（见文首表）
DEFAULT_COLS = {
    "time": "I0",
    "travel": "I1",
    "measure": "I2",
    "output": "I4",
    "current": "I8",
    "dt": "I9",
    "state": "I11",
    "raw_speed": "I12",
}


def load_csv(path: Path) -> pd.DataFrame:
    """读 CSV 并自检布局（表头必须是 I0,I1,... 顺序编号）。"""
    if not path.is_file():
        sys.exit(f"[错误] 文件不存在: {path}")
    df = pd.read_csv(path)
    cols = list(df.columns)
    if not cols or cols[0] != "I0":
        sys.exit(f"[错误] 表头第一列应为 I0，实际为 {cols[0] if cols else '(空)'}")
    if cols != [f"I{i}" for i in range(len(cols))]:
        sys.exit(f"[错误] 表头不是连续编号 I0..I{len(cols) - 1}: {cols[:6]}...")
    if len(df) < 100:
        sys.exit(f"[错误] 数据行太少: {len(df)}")
    return df


def split_segments(ts_raw: np.ndarray, gap_factor: float) -> list[tuple[int, int]]:
    """按时间戳回退(d<=0，MCU 复位重新计时/两次导出拼接)或大跳切段（左闭右开）。
    段内时间戳严格递增 —— 段内做积分/时长统计才是可信的。"""
    if ts_raw.size < 2:
        return [(0, len(ts_raw))]
    d = np.diff(ts_raw)
    pos = d[d > 0]
    med = float(np.median(pos)) if pos.size else 0.0
    cuts = np.flatnonzero(d <= 0)
    if gap_factor > 0 and med > 0:
        cuts = np.union1d(cuts, np.flatnonzero(d > gap_factor * med))
    bounds = np.concatenate(([0], cuts + 1, [len(ts_raw)])).astype(int)
    return [(a, int(b)) for a, b in zip(bounds[:-1], bounds[1:]) if b - a >= 2]


def build_time(ts_raw: np.ndarray, segs: list[tuple[int, int]], unit: str) -> tuple[np.ndarray, str]:
    """拼出单调时间轴（秒）：各段首尾相接，段间停表的墙钟时间不计入。"""
    d = np.diff(ts_raw)
    med = float(np.median(d[d > 0])) if np.any(d > 0) else 0.0
    if unit == "auto":
        unit = "us" if med >= 1000 else ("ms" if med >= 1 else "s")
    scale = {"us": 1e-6, "ms": 1e-3, "s": 1.0}[unit]
    t = np.zeros(len(ts_raw))
    off = 0.0
    for a, b in segs:
        s = (ts_raw[a:b] - ts_raw[a]) * scale
        t[a:b] = off + s
        off += float(s[-1])
    return t, unit


def runs_of(mask: np.ndarray) -> list[tuple[int, int]]:
    """布尔掩码里的连续 True 段（左闭右开），自动跨段断开（调用方按段传入）。"""
    idx = np.flatnonzero(np.diff(np.concatenate(([0], mask.view(np.int8), [0]))))
    return list(zip(idx[::2], idx[1::2]))


def pct(v: np.ndarray, q: float) -> float:
    return float(np.percentile(v, q)) if v.size else float("nan")


def cruise_report(travel, meas, out, cur, dtms, state, raw, args, segs, period_s) -> None:
    firing = np.isin(state.astype(np.int64), args.firing_states)
    firing &= dtms > 0  # dt=0 的样本控制核没跑，值可能是残值
    # 只保留时间戳连续的那些发弹样本：按段分别做 runs，段边界不连算
    n_fire = 0
    stalls: list[int] = []   # 各段"卡住"持续的样本数
    shots = 0
    rot = 0.0                # 发弹段累计转角 (rad)，用原始转速积分
    dur = 0.0
    for a, b in segs:
        m = firing[a:b]
        n_fire += int(m.sum())
        dur += float(m.sum()) * period_s
        rot += float(np.sum(np.abs(raw[a:b][m])) * period_s)
        # 卡住：原始转速（判据必须用原始量，见 app_shoot.c 的静摩擦前馈说明）低于带宽
        for p, q in runs_of(np.abs(raw[a:b]) < args.stick_band):
            if m[p:q].all():
                if (q - p) * period_s * 1e3 >= args.stick_min_ms:
                    stalls.append(q - p)
        # 走完一发：行程累计被清零（掉幅 > 半个步长）
        tr = travel[a:b]
        drops = np.flatnonzero(tr[:-1] - tr[1:] > 0.5 * args.step_rad)
        shots += int(sum(1 for i in drops if m[i]))
    fm, fo, fc, fr = meas[firing], out[firing], cur[firing], raw[firing]

    print(f"\n== 发弹段（state ∈ {sorted(args.firing_states)}）==")
    print(f"  时长 {dur:8.2f} s（{n_fire} 样本，中位周期 {period_s * 1e3:.3f} ms）")
    if not n_fire:
        print("  （没有发弹样本：核对 --state-col / --firing-states）")
        return

    print(f"\n  目标转速 {args.ref:.2f} rad/s（定值，日志无此通道）")
    print(f"  实际转速  中位 {pct(fm, 50):7.2f}  p05 {pct(fm, 5):7.2f}  p95 {pct(fm, 95):7.2f}  "
          f"max {fm.max():7.2f}  min {fm.min():7.2f} rad/s")
    print(f"  → 跟随率（中位/目标）{pct(fm, 50) / args.ref * 100:6.1f}%   "
          f"均值/目标 {fm.mean() / args.ref * 100:6.1f}%")
    stuck = np.abs(fr) < args.stick_band
    print(f"  卡住(|原始转速|<{args.stick_band:.2f}) 占比 {stuck.mean() * 100:5.1f}%   "
          f"事件 {len(stalls)} 次   最长 {max(stalls) * period_s * 1e3 if stalls else 0:.0f} ms   "
          f"中位 {pct(np.array(stalls, dtype=float), 50):.0f} ms")

    print(f"\n  输出电流指令  中位 {pct(fo, 50):6.2f}  p95 {pct(fo, 95):6.2f}  max {fo.max():6.2f} A   "
          f"min {fo.min():6.2f}   （上限 {args.i_max:.0f} A）")
    print(f"  实测电流      中位 {pct(fc, 50):6.2f}  p95 {pct(fc, 95):6.2f}  max {fc.max():6.2f} A")
    print(f"  → 峰值/上限 {abs(fo).max() / args.i_max * 100:5.1f}%   "
          f"顶到限幅(|out|>={args.i_max - 0.1:.1f}A)占比 "
          f"{float(np.mean(np.abs(fo) >= args.i_max - 0.1)) * 100:.2f}%")
    if abs(fo).max() < 0.5 * args.i_max:
        print("  ⚠ 电流远没顶到限幅 → 环路没在使劲：查变速积分门限（|e| 是否落在冻结区）")
        print("     与 kp（满误差时 P 项 = kp×|e|）、以及是否有前馈没接上，别先怀疑驱动能力。")

    print(f"\n  走完整发 {shots} 发（判据：行程累计被清零 ≥{args.step_rad:.3f} rad）")
    if shots:
        print(f"  → {shots / dur:.2f} 发/s；按目标转速应到 {args.ref / args.step_rad:.2f} 发/s"
              f"（达成率 {shots / dur / (args.ref / args.step_rad) * 100:.1f}%）")
    print(f"  发弹段累计转角 {rot:.0f} rad = {rot / (2 * np.pi):.1f} 圈(电机) "
          f"= {rot / args.step_rad:.1f} 发的量")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", type=Path, help="VOFA+ 导出的 CSV")
    ap.add_argument("--time-unit", choices=["auto", "us", "ms", "s"], default="auto")
    ap.add_argument("--ref", type=float, default=20.0, help="拨盘目标转速 (rad/s)，定值（默认 20）")
    ap.add_argument("--i-max", type=float, default=20.0, help="速度环输出限幅 (A)，对齐 TRIGGER_I_MAX（默认 20）")
    ap.add_argument("--step-rad", type=float, default=13.404, help="每发对应电机转角 (rad)，= TRIGGER_STEP_RAD")
    ap.add_argument("--firing-states", default="1,2,3",
                    help="算「发弹段」的状态机取值（默认 1,2,3 = 单发/三发/连发；4=停止不含）")
    ap.add_argument("--stick-band", type=float, default=0.4, help="判「卡住」的原始转速带宽 (rad/s)，默认 0.4")
    ap.add_argument("--stick-min-ms", type=float, default=20.0, help="「卡住」最短时长 (ms)，默认 20")
    ap.add_argument("--gap-factor", type=float, default=10.0,
                    help="时间戳相邻间隔 > 该倍数 × 中位周期 判为段间中断（默认 10；设 0 关闭）")
    for key, col in DEFAULT_COLS.items():
        ap.add_argument(f"--{key}-col", default=col, help=f"{key} 用的列名（默认 {col}）")
    args = ap.parse_args()
    args.firing_states = [int(s) for s in args.firing_states.split(",") if s.strip()]

    df = load_csv(args.csv)
    need = {k: getattr(args, f"{k}_col") for k in DEFAULT_COLS}
    missing = [c for c in need.values() if c not in df.columns]
    if missing:
        sys.exit(f"[错误] CSV 里没有这些列: {missing}；可用列: {list(df.columns)[:10]}...")

    # 布局自检：状态通道必须只取状态机那几个值，否则多半是列名对错了版次
    state = df[need["state"]].to_numpy(dtype=np.float64)
    bad = set(np.unique(state).astype(np.int64)) - set(range(5))
    if bad:
        sys.exit(f"[错误] {need['state']} 不是状态机取值（出现 {sorted(bad)}，应为 0~4）——"
                 f"列名可能对错了固件版次（旧版 CH11 是旋钮）")

    ts_raw = df[need["time"]].to_numpy(dtype=np.float64)
    segs = split_segments(ts_raw, args.gap_factor)
    t_s, unit = build_time(ts_raw, segs, args.time_unit)
    # 中位周期只在段内取（跨段的两拍之间是停表时间，不是控制周期）
    diffs = np.concatenate([np.diff(ts_raw[a:b]) for a, b in segs]) if len(segs) else np.array([])
    med_raw = float(np.median(diffs)) if diffs.size else 0.0
    period_s = med_raw * {"us": 1e-6, "ms": 1e-3, "s": 1.0}[unit]
    print(f"文件      : {args.csv}")
    print(f"样本      : {len(df)} 行，{len(segs)} 段，轴上总时长 {t_s[-1]:.2f} s，中位周期 "
          f"{period_s * 1e3:.3f} ms（时间按 {unit} 解释）")
    if len(segs) > 1:
        seg_len = [float((ts_raw[b - 1] - ts_raw[a]) * {"us": 1e-6, "ms": 1e-3, "s": 1.0}[unit])
                   for a, b in segs]
        print(f"          段长(s): " + " ".join(f"{x:.1f}" for x in seg_len)
              + f"   （时间戳回退/大跳处切段，段间停表不计入）")

    cruise_report(df[need["travel"]].to_numpy(dtype=np.float64),
                  df[need["measure"]].to_numpy(dtype=np.float64),
                  df[need["output"]].to_numpy(dtype=np.float64),
                  df[need["current"]].to_numpy(dtype=np.float64),
                  df[need["dt"]].to_numpy(dtype=np.float64),
                  state, df[need["raw_speed"]].to_numpy(dtype=np.float64),
                  args, segs, period_s)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
