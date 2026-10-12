#!/usr/bin/env python3
"""拨盘「开环定力矩」试验分析：同一恒定力矩下，什么时候在动、什么时候卡死、走了几发。

配套 app_shoot.c 的**开环定力矩试验**版调试通道表（2026-10-10 第三轮）：
    CH1  实测位置 (rad，单圈，app 正向) —— 是单圈值，本脚本按段 unwrap 成多圈行程
    CH2  实测转速 (rad/s，app 正向，未低通) —— 判"动了没"的主判据
    CH3  0                       CH4  实下发力矩指令 (Nm/A，= -TRIGGER_TEST_TORQUE 或 0)
    CH5-7 0（无 P/I/D）           CH8  实测电流 (A，app 正向)      CH9  本拍 dt (ms)
    CH10 0                       CH11 驱动条件 enable&&fire (0/1)  CH12 原始反馈转速 (rad/s，电机极性)

核心读法：力矩在这份日志里是**恒定**的（CH4 只有一个非零值），于是同一力矩会出现两副面孔 --
  - 卡住段：|CH2| 长时间≈0 而力矩一直给着  →  静摩擦 > 该力矩；
  - 动起来后转速很快冲到很大（远超任何稳态工作点）→  动摩擦 ≪ 该力矩。
  同一力矩既"推不动"又"推飞了" = "静摩擦远大于动摩擦"的直接证据，也是"该上摩擦前馈、
  不该靠加大积分硬顶"的量化依据（动/静两项分开给，前馈各管一段）。

⚠ 反过来说：开环恒定力矩**没有限速**，这正是这类试验最容易飞车的地方 —— 脚本会把
  "超速(fly)" 单独标出来，并把"超速前/后"分开统计。飞车段的机构往往已经脱离正常啮合，
  那一部分的"摩擦"数字不能当健康机构用。

CSV 布局（VOFA+ JustFloat 导出）：首行表头 I0,I1,...；I0 = 时间戳(DWT µs)，I1~ 依次对应 CH1~。
时间戳回退（MCU 复位/两次导出拼接）或大跳处按硬边界切段；总时长按各段之和计（段间停表不计）。

用法：
    python3 analyze_trigger_torque.py ../../ignore/xxx.csv
    python3 analyze_trigger_torque.py log.csv --torque 6 --band 0.4 --fly 200
    python3 analyze_trigger_torque.py log.csv --runs-max 40 --gap-factor 10
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
    "pos": "I1",
    "speed": "I2",
    "torque": "I4",
    "current": "I8",
    "dt": "I9",
    "gate": "I11",
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
    """按时间戳回退(d<=0)或大跳切段（左闭右开）。段内时间戳严格递增。"""
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


def build_time(ts_raw: np.ndarray, segs: list[tuple[int, int]], scale: float) -> tuple[np.ndarray, float]:
    """拼出单调时间轴（秒）：各段首尾相接，段间停表不计；返回 (t, 被跳过的段间秒数)。"""
    t = np.zeros(len(ts_raw))
    off = 0.0
    for a, b in segs:
        s = (ts_raw[a:b] - ts_raw[a]) * scale
        t[a:b] = off + s
        off += float(s[-1])
    skipped = 0.0
    for (_, b), (a2, _) in zip(segs[:-1], segs[1:]):
        skipped += float(ts_raw[a2] - ts_raw[b - 1]) * scale
    return t, skipped


def unwrap_travel(pos: np.ndarray, segs: list[tuple[int, int]], period: float) -> np.ndarray:
    """单圈位置 → 多圈行程（逐段 unwrap，绝不跨段连算）。"""
    out = np.zeros_like(pos, dtype=np.float64)
    for a, b in segs:
        out[a:b] = np.unwrap(pos[a:b], period=period)
    return out


def runs_of(mask: np.ndarray) -> list[tuple[int, int]]:
    """布尔掩码里的连续 True 段（左闭右开）。"""
    idx = np.flatnonzero(np.diff(np.concatenate(([0], mask.view(np.int8), [0]))))
    return list(zip(idx[::2], idx[1::2]))


def pct(v: np.ndarray, q: float) -> float:
    return float(np.percentile(v, q)) if v.size else float("nan")


def stuck_runs_in(mask: np.ndarray, segs: list[tuple[int, int]], min_samples: float) -> list[tuple[int, int]]:
    """mask 里持续 ≥min_samples 个样本的连续 True 段（按段切，绝不跨段连算）。"""
    out: list[tuple[int, int]] = []
    for a, b in segs:
        for i, j in runs_of(mask[a:b]):
            if j - i >= min_samples:
                out.append((a + i, a + j))
    return out


def wrap_disp(pos_slice: np.ndarray) -> float:
    """一段内的总位移 (rad)：单圈位置差分做半圈判向后累加幅值。"""
    if pos_slice.size < 2:
        return 0.0
    d = (np.diff(pos_slice) + np.pi) % (2 * np.pi) - np.pi
    return float(np.abs(d).sum())


def report(df, args, need, t_s, travel, segs, period_s, skipped):
    gate = df[need["gate"]].to_numpy(np.float64) != 0
    spd = df[need["speed"]].to_numpy(np.float64)
    cur = df[need["current"]].to_numpy(np.float64)
    trq = df[need["torque"]].to_numpy(np.float64)
    pos = df[need["pos"]].to_numpy(np.float64)
    dtms = df[need["dt"]].to_numpy(np.float64)
    live = dtms > 0  # dt=0 的样本控制任务没跑，值是残值

    print(f"\n文件      : {args.csv}")
    print(f"样本      : {len(df)} 行，{len(segs)} 段，轴上总时长 {t_s[-1]:.2f} s"
          f"（另有段间停表 {skipped:.1f} s 不计），中位周期 {period_s * 1e3:.3f} ms")
    if len(segs) > 1:
        seg_len = [float(t_s[b - 1] - t_s[a]) for a, b in segs]
        print("          段长(s): " + " ".join(f"{x:.1f}" for x in seg_len) + "   （回退/大跳处切段）")

    # ---- 指令力矩：恒定力矩试验的前提 ----
    nz = np.unique(np.round(trq[trq != 0], 4))
    tq = abs(float(nz[0])) if nz.size == 1 else float("nan")
    print("\n== 指令力矩 (CH4) ==")
    if nz.size == 0:
        print("  整份日志没有非零力矩（没使能/没按 ch7）")
    elif nz.size == 1:
        print(f"  非零值唯一：{nz[0]:+.2f}（Nm/A）→ 恒定力矩试验的前提成立，|力矩| = {tq:.2f}")
    else:
        print(f"  ⚠ 出现多个非零值 {nz} —— 不是恒定力矩日志，下面'同一力矩两副面孔'的判读不成立")

    # ---- 发弹段（gate=1 且任务在跑），按段内连续 run 切 ----
    g = gate & live
    on_runs: list[tuple[int, int]] = []
    for a, b in segs:
        for p, q in runs_of(g[a:b]):
            on_runs.append((a + p, a + q))
    dur = sum(q - p for p, q in on_runs) * period_s

    print("\n== 发弹段（CH11=1）汇总 ==")
    print(f"  总时长 {dur:6.2f} s（{int(g.sum())} 样本，{len(on_runs)} 次按下 ch7）")
    if not g.any():
        return
    sg, cg = spd[g], cur[g]
    split = float(np.mean(sg > 0)) * 100
    print(f"  实测转速  中位 {pct(sg, 50):8.2f}  p05 {pct(sg, 5):8.2f}  p95 {pct(sg, 95):8.2f}  "
          f"max {sg.max():8.2f}  min {sg.min():8.2f} rad/s")
    print(f"  → 转速为正(发弹向) {split:5.1f}%，为负 {float(np.mean(sg < 0)) * 100:5.1f}%，"
          f"≈0(|转速|<{args.band:.2f}) {float(np.mean(np.abs(sg) < args.band)) * 100:5.1f}%")
    print(f"  实测电流  中位 {pct(cg, 50):8.2f}  |中位| {pct(np.abs(cg), 50):8.2f}  "
          f"max {np.abs(cg).max():8.2f} A（指令 {tq:.2f}）")

    # ---- 逐次按下 ch7 的明细：这是这份日志真正的粒度 ----
    print(f"\n== 逐次发弹段明细（每次按下 ch7 一段，最多列 {args.runs_max} 段）==")
    print("     #   t起~t止(s)      时长    |转速|中位    |转速|max   电流|中位|   位移(发)   卡住%   判定")
    nbig = int(np.sum(np.abs(spd) > args.fly))
    for k, (a, b) in enumerate(on_runs[: args.runs_max]):
        s, c = spd[a:b], cur[a:b]
        disp = wrap_disp(pos[a:b])
        stuckp = float(np.mean(np.abs(s) < args.band)) * 100
        peak = float(np.abs(s).max())
        verdict = "飞车" if peak > args.fly else ("卡死" if stuckp > 50 else "正常/其他")
        print(f"  {k:4d}  {t_s[a]:7.2f}~{t_s[b - 1]:7.2f}  {t_s[b - 1] - t_s[a]:6.2f}  "
              f"{pct(np.abs(s), 50):9.2f}  {peak:9.1f}  {pct(np.abs(c), 50):9.2f}  "
              f"{disp / args.step_rad:9.2f}  {stuckp:6.1f}  {verdict}")
    if len(on_runs) > args.runs_max:
        print(f"  ...（还有 {len(on_runs) - args.runs_max} 段未列）")

    # ---- 行程 / 发数 ----
    dstep = np.diff(travel)
    rot_gate = sum(wrap_disp(pos[a:b]) for a, b in on_runs)
    rot_total = float(np.abs(dstep).sum())
    movers = np.flatnonzero(np.abs(dstep) >= args.pos_lsb)
    print("\n== 行程 / 发数 ==")
    print(f"  发弹段内位移 {rot_gate:8.1f} rad = {rot_gate / args.step_rad:5.2f} 发"
          f"（每发 {args.step_rad:.3f} 电机 rad = 2.133 圈电机），时长 {dur:.2f} s"
          f" → 平均 {rot_gate / args.step_rad / dur:.2f} 发/s")
    print(f"  全程累计转角 {rot_total:8.1f} rad = {rot_total / (2 * np.pi):6.1f} 圈(电机)"
          f"（含滑行/静止段的量化抖动，别当真实行程）")
    print(f"  位置发生变化的拍 {movers.size} 个 = {movers.size * period_s:.2f} s（占全程 {movers.size * period_s / t_s[-1] * 100:5.1f}%）"
          f" —— 大于发弹段的 {dur:.2f} s，说明 ch7 松开后机构还在滑行/被反向带动")

    # ---- 动/静对比（⚠ 超速前后**分开算**：飞车段机构往往已脱离正常啮合/已被撞坏） ----
    print("\n== 同一力矩下的两副面孔 ==")
    over = np.abs(spd) > args.fly
    fly_i = int(np.flatnonzero(over)[0]) if over.any() else len(spd)
    idx = np.arange(len(spd))
    gate_mask = g  # = gate & live
    stuck_mask = (np.abs(spd) < args.band) & gate_mask
    min_samp = args.stuck_min_ms / (period_s * 1e3)

    # 统计 [lo,hi) 区间内的「推不动」
    def stuck_of(lo: int, hi: int) -> tuple[float, int, float, float]:
        sub = stuck_mask.copy()
        sub[:lo] = False
        sub[hi:] = False
        runs = stuck_runs_in(sub, segs, min_samp)
        gsec = float((gate_mask & (idx >= lo) & (idx < hi)).sum()) * period_s
        d = sum(j - i for i, j in runs) * period_s
        worst = max((j - i for i, j in runs), default=0) * period_s * 1e3
        return d, len(runs), worst, gsec

    # 推飞了（先印，好让下面「撞坏前/后」的切点有出处）
    print(f"  推飞了：转速 >{args.fly:.0f} rad/s 的样本 {int(over.sum())} 个 = {int(over.sum()) * period_s:.2f} s"
          f"（占发弹段 {float(over.mean()) * 100:.1f}%），峰值 {np.abs(spd).max():.1f} rad/s"
          f"（转速瞬时等效 {np.abs(spd).max() / args.step_rad:.1f} 发/s，不等于真实射频）")
    if over.any():
        print(f"  ⚠ 首次超速在 t={t_s[fly_i]:.2f} s —— 下面以**首次超速**为界切"
              f"（超速往往意味着机构已脱离正常啮合）；真正被撞坏的时刻看下一条")
    # 撞击时刻 = 最猛的一拍减速（负加速度峰值），它是「撞坏」的真实时刻
    acc = np.diff(spd) / np.where(np.diff(t_s) > 0, np.diff(t_s), np.nan)
    acc = np.where(np.isfinite(acc), acc, 0.0)
    j = int(np.argmin(acc))
    print(f"  撞击：最猛一拍 t={t_s[j + 1]:.2f} s，转速 {spd[j]:.0f}→{spd[j + 1]:.0f} rad/s"
          f"（等效加速度 {acc[j]:.0f} rad/s²，电流 {cur[j + 1]:.2f} A）"
          f" —— 这之后才是**撞坏了的机构**")

    if over.any() and fly_i > 0 and fly_i < len(spd):
        spans = [("超速前", 0, fly_i), ("超速后", fly_i, len(spd))]
    else:
        spans = [("全程", 0, len(spd))]
    for label, lo, hi in spans:
        d, n, worst, gsec = stuck_of(lo, hi)
        p = d / gsec * 100 if gsec else 0.0
        print(f"  推不动[{label}]：{d:.2f} s / 该区间发弹 {gsec:.2f} s（{p:5.1f}%）力矩 {tq:.2f} 一直给着，"
              f"转速仍 <{args.band:.2f}（{n} 次，最长 {worst:.0f} ms）  →  静摩擦 ≳ {tq:.2f}")
    print("  ⚠ 「超速后」那行里的静摩擦 ≳ 混了飞车段(机构尚好)与撞击后的**坏机构**，"
          "只有撞击前的数字能当健康机构的调参依据。")

    mv = np.abs(spd[~over & (np.abs(spd) >= args.band)])
    if mv.size:
        print(f"  正常运动(带宽~{args.fly:.0f}) 的那 {mv.size * period_s:.2f} s 里，|转速| "
              f"p50 {pct(mv, 50):.1f}  p90 {pct(mv, 90):.1f}  p99 {pct(mv, 99):.1f} rad/s")
    print("  同一力矩既推不动又推飞 → 静摩擦 ≫ 动摩擦：该用**前馈**（动/静两项分开给），"
          "\n  不该靠加大积分硬顶（积分把这份差值储起来，脱困后就是一脚超速）。"
          "\n  ⚠ 但前馈解决不了'飞车'本身 —— 开环无限速，飞车只有速度环的目标限幅能管住。")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", type=Path, help="VOFA+ 导出的 CSV")
    ap.add_argument("--torque", type=float, default=6.0, help="本轮试验的恒定力矩 (Nm/A)，仅用于提示")
    ap.add_argument("--step-rad", type=float, default=13.404, help="每发对应电机转角 (rad)，= TRIGGER_STEP_RAD")
    ap.add_argument("--band", type=float, default=0.4, help="判「卡住」的转速带宽 (rad/s)，默认 0.4")
    ap.add_argument("--stuck-min-ms", type=float, default=20.0, help="「卡住」最短时长 (ms)，默认 20")
    ap.add_argument("--fly", type=float, default=200.0, help="判「超速/飞车」的转速 (rad/s)，默认 200")
    ap.add_argument("--pos-lsb", type=float, default=2 * np.pi / 8192, help="位置量化步长 (rad)，默认 M3508 的 2π/8192")
    ap.add_argument("--runs-max", type=int, default=30, help="逐次发弹段明细最多列几段，默认 30")
    ap.add_argument("--gap-factor", type=float, default=10.0, help="时间戳相邻间隔 > 该倍数×中位周期 判为段间中断")
    ap.add_argument("--time-unit", choices=["auto", "us", "ms", "s"], default="auto")
    ap.add_argument("--pos-period", type=float, default=2 * np.pi, help="位置通道的回绕周期 (rad)")
    for key, col in DEFAULT_COLS.items():
        ap.add_argument(f"--{key}-col", default=col, help=f"{key} 用的列名（默认 {col}）")
    args = ap.parse_args()

    df = load_csv(args.csv)
    need = {k: getattr(args, f"{k}_col") for k in DEFAULT_COLS}
    missing = [c for c in need.values() if c not in df.columns]
    if missing:
        sys.exit(f"[错误] CSV 里没有这些列: {missing}；可用列: {list(df.columns)[:10]}...")

    # 布局自检：闸门通道必须是 0/1，否则多半列名对错了版次
    gate_v = df[need["gate"]].to_numpy(np.float64)
    bad = set(np.unique(gate_v).tolist()) - {0.0, 1.0}
    if bad:
        sys.exit(f"[错误] {need['gate']} 不是 0/1 闸门（出现 {sorted(bad)}）—— 列名可能对错了固件版次")

    ts_raw = df[need["time"]].to_numpy(np.float64)
    d = np.diff(ts_raw)
    med_raw = float(np.median(d[d > 0])) if np.any(d > 0) else 0.0
    unit = args.time_unit
    if unit == "auto":
        unit = "us" if med_raw >= 1000 else ("ms" if med_raw >= 1 else "s")
    scale = {"us": 1e-6, "ms": 1e-3, "s": 1.0}[unit]

    segs = split_segments(ts_raw, args.gap_factor)
    t_s, skipped = build_time(ts_raw, segs, scale)
    travel = unwrap_travel(df[need["pos"]].to_numpy(np.float64), segs, args.pos_period)
    period_s = med_raw * scale

    report(df, args, need, t_s, travel, segs, period_s, skipped)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
