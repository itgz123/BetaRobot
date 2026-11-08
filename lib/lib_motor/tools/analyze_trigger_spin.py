#!/usr/bin/env python3
"""拨盘「参考位置环」日志分析：连续转动（三发/连发）的**射频**、跟踪质量与电流。

配套 app_shoot.c 的调试通道表（**2026-10-11 位置环版**，CH1~CH12 为「核心列」，此后未再变动）：
    CH1  参考位置(rad, 多圈)   CH2  实测位置(rad, 多圈)   CH3  位置误差(rad)
    CH4  力矩指令(Nm)          CH5  参考速度(rad/s)      CH6  实测转速(rad/s)
    CH7  参考加速度(rad/s²)    CH8  实测电流(A)          CH9  本拍 dt(ms)
    CH10 状态机状态(0关/1单发/2三发/3连发/4停止)          CH11 enable&&fire(0/1)
    CH12 原始反馈转速(rad/s，电机极性，未取反)

2026-10-11 追加的「扩展列」（控制核内部量，带负载整定用；CH1~CH12 一个没动）：
    CH13 速度环 P 项(A)   CH14 速度环 I 项(A)   CH15 卡住保护状态(0/1/2)
    CH16 位置环输出=速度设定值(rad/s, app 正向)   CH17 位置环 P 项(rad/s)
    CH18 速度环误差(rad/s)   CH19 速度环前馈=摩擦前馈(A，接线前恒 0)
⚠ **极性分两套**：控制核内部量（CH13/14/16/17/18）在 app 正向域，**只有 CH4 是电机域**。
  未饱和时 CH4 = −(CH13+CH14+CH19)；|CH13+CH14+CH19| > |CH4| 即速度环输出限幅饱和。

2026-10-11 再加的「摩擦轮列」（**判弹丸有没有真的过轮**；CH1~CH19 一个没动）：
    CH20 摩擦轮实测转速(rad/s, app 正向, **未低通**)   CH21 摩擦轮实测电流(A, app 正向, **未低通**)
    CH22 摩擦轮速度环输出=实下发(A)                    CH23 速度环 I 项(A)
    CH24 速度误差(rad/s)                               CH25 摩擦轮使能且在线(0/1)
判弹原理：摩擦轮空转恒速，弹丸被两轮夹着挤过去的那 2~5 ms 会**掉速**，速度环随即**顶电流**
  补回来 —— 签名是 CH20 掉一个坑、CH21/CH22 冒一个尖、CH23 随后一段恢复。拨盘侧只能看到
  "走了多少角度"，弹丸到底出没出只能靠这一组。⚠ 采样 500 Hz 对 2~5 ms 的坑只够 1~3 个点，
  **深度会被采样相位吃掉**，故判弹优先看 CH21/CH22 电流尖峰与 CH23 积分恢复，CH20 作辅助。

⚠ 通道表随调试重点变过三次（阶跃/扫档 → 定速巡航 → 开环力矩 → 本轮位置环），
   **拿旧日志跑本脚本会得到自洽但完全错误的结论**（列自检只查表头编号，查不出语义）。
   旧日志请用对应的老脚本：analyze_vofa.py / analyze_trigger_cruise.py / analyze_trigger_torque.py。

CSV 布局（VOFA+ JustFloat 导出）：首行表头 I0,I1,...；I0 = 时间戳(DWT µs)，I1~ 依次对应 CH1~。
**扩展列与摩擦轮列都是可选的**，脚本按实际有数据的部分降级，用到缺列的分析自动跳过：
    老日志只有 I0~I12  → 「12 通道版」（只剩核心分析）
    I13~I19 有非零值   → 「19 通道版」（+ 卡住保护/前馈补位/饱和/变速积分）
    I20~I25 有非零值   → 「25 通道版」（+ 摩擦轮判弹）
判据一律是"**有没有非零值**"，不能按列数 —— VOFA 固定发满 25 通道，列**总是存在**。
未用列（若本工程以后再加通道，是 I26 起）自检会报告是否为全零，以确认"确实没接"而不是"接线掉了"。

射频的定义：拨盘"一发"= TRIGGER_STEP_RAD 的转角，故
    射频(发/s) = 参考位置对时间的导数 / TRIGGER_STEP_RAD
连续转动段取**稳态**（参考速度已爬到巡航、且未进入收速滑行）的中位数；
含加减速的整段均值会偏低，两个数都报。

用法：
    python3 analyze_trigger_spin.py ../../ignore/vofa+.csv
    python3 analyze_trigger_spin.py log.csv --step-rad 13.4045 --min-run-ms 200
    python3 analyze_trigger_spin.py log.csv --state-map 0=关,1=单发,2=三发,3=连发,4=停止
    python3 analyze_trigger_spin.py log.csv --dump-segments segs.csv
    python3 analyze_trigger_spin.py log.csv --fric-dip 0.03       # 摩擦轮判弹门限调细
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# 核心列 = app_shoot.c CH1~CH12（2026-10-11 位置环版，此后未再变动）—— **缺失即报错**
CORE_COLS = {
    "time": "I0",     # 时间戳 (DWT µs)
    "ref_pos": "I1",  # CH1 参考位置
    "pos": "I2",      # CH2 实测位置
    "err": "I3",      # CH3 位置误差
    "torque": "I4",   # CH4 力矩指令
    "ref_vel": "I5",  # CH5 参考速度
    "vel": "I6",      # CH6 实测转速
    "ref_acc": "I7",  # CH7 参考加速度
    "cur": "I8",      # CH8 实测电流
    "dt": "I9",       # CH9 本拍 dt (ms)
    "state": "I10",   # CH10 状态机状态
    "fire": "I11",    # CH11 enable&&fire
    "raw_vel": "I12", # CH12 原始反馈转速
}
# 扩展列 = app_shoot.c CH13~CH19（2026-10-11 追加，控制核内部量）—— **缺失只降级、不报错**
EXT_COLS = {
    "speed_p": "I13",    # CH13 速度环 P 项 (A, app 正向)
    "speed_i": "I14",    # CH14 速度环 I 项 (A, app 正向)
    "stuck": "I15",      # CH15 卡住保护状态 (0/1/2)
    "angle_out": "I16",  # CH16 位置环输出 = 速度设定值 (rad/s)
    "angle_p": "I17",    # CH17 位置环 P 项 (rad/s)
    "speed_err": "I18",  # CH18 速度环误差 (rad/s)
    "friction_ff": "I19",# CH19 速度环前馈 = 摩擦前馈 (A)
}
# 摩擦轮列 = app_shoot.c CH20~CH25（2026-10-11 追加，判弹丸过轮）—— **缺失只降级、不报错**
FRIC_COLS = {
    "fric_vel": "I20",  # CH20 摩擦轮实测转速 (rad/s, app 正向, 未低通)
    "fric_cur": "I21",  # CH21 摩擦轮实测电流 (A, app 正向, 未低通)
    "fric_out": "I22",  # CH22 摩擦轮速度环输出 = 实下发 (A)
    "fric_i": "I23",    # CH23 摩擦轮速度环 I 项 (A)
    "fric_err": "I24",  # CH24 摩擦轮速度误差 (rad/s)
    "fric_live": "I25", # CH25 摩擦轮使能且在线 (0/1)
}
DEFAULT_COLS = {**CORE_COLS, **EXT_COLS, **FRIC_COLS}
OPTIONAL_COLS = frozenset(EXT_COLS) | frozenset(FRIC_COLS)

STATE_NAMES = {0: "关", 1: "单发", 2: "三发", 3: "连发", 4: "停止"}
CONTINUOUS_STATES = (2, 3)  # 连续转动
SINGLE_STATE = 1

# 物理常数（app_shoot.c 当前值，供 --max-current / --max-ref-vel 的饱和判据用）
DEFAULT_GEAR_RATIO = (268.0 / 17.0) * (62.0 / 38.0)  # 268:17 × 62:38 = 25.7214（真值，不是 25.6）
DEFAULT_STEP_RAD = 2.0 * np.pi * DEFAULT_GEAR_RATIO / 12.0  # TRIGGER_STEP_RAD ≈ 13.4677 rad
DEFAULT_I_MAX = 20.0                          # TRIGGER_I_MAX (A)
DEFAULT_ANGLE_VMAX = 150.0                    # TRIGGER_ANGLE_VMAX (rad/s)


def load_csv(path: Path, cols: dict[str, str], optional: frozenset = frozenset()) -> tuple[pd.DataFrame, list[str]]:
    """读 CSV 并自检布局（表头必须是连续编号 I0,I1,...，且覆盖所有**必需**列）。

    `optional` 里的键缺失时只跳过、不报错（老日志没有扩展列）；返回 (数据, 实际缺失的扩展列名)。
    """
    if not path.is_file():
        sys.exit(f"[错误] 文件不存在: {path}")
    df = pd.read_csv(path)
    names = list(df.columns)
    if not names or names[0] != "I0":
        sys.exit(f"[错误] 表头第一列应为 I0，实际为 {names[0] if names else '(空)'}")
    expect = [f"I{k}" for k in range(len(names))]
    if names != expect:
        sys.exit(f"[错误] 表头不是连续编号 I0..I{len(names) - 1}: {names[:6]}...")
    missing = [c for c in cols.values() if c not in df.columns]
    missing_required = [c for c in missing if c not in optional]
    if missing_required:
        sys.exit(f"[错误] 缺少必需列 {missing_required}（本文件只有 {len(names)} 列: {names[0]}..{names[-1]}）")
    if len(df) < 3:
        sys.exit(f"[错误] 数据行太少: {len(df)}")

    # ---- 时间戳清洗（必须在列选取/任何按时间的切分之前）----
    # ① DWT 是 32 位 µs 计数器，约 4295 s 回绕一次 → 大幅负跳变按 +2^32 解卷绕；
    # ② VOFA 抓包**首行常是缓冲残值**（本工程实测首行 75731008 µs，其后从 35.6 s 起
    #    2 ms/拍单调递增）。不丢它的话"时长"会算成负数、按时间切分的统计全部错位。
    # 只丢**头部**破坏单调的行（实测异常都出在头部）；中段仍非单调则只告警不丢。
    t = df[cols["time"]].to_numpy(dtype=np.float64)
    d = np.diff(t)
    nwrap = int((d < -(2**31)).sum())
    if nwrap:
        df[cols["time"]] = t + np.cumsum(np.r_[0.0, np.where(d < -(2**31), 2**32, 0.0)])
        t = df[cols["time"]].to_numpy(dtype=np.float64)
        print(f"[清洗] 时间戳回绕解卷绕 {nwrap} 处 (+2^32 µs)")
    head = 0
    while head + 1 < t.size and t[head] >= t[head + 1]:
        head += 1
    if head:
        df = df.iloc[head:].reset_index(drop=True)
        t = t[head:]
        print(f"[清洗] 丢弃头部 {head} 行非单调时间戳（VOFA 缓冲残值）")
    nmid = int((np.diff(t) <= 0).sum())
    if nmid:
        print(f"[警告] 时间戳中段仍有 {nmid} 处非单调，按时间切分的统计可能失准")

    keep = {k: v for k, v in cols.items() if v in df.columns}  # 只保留本文件真有、且我们要用的列
    df = df[[*keep.values()]].rename(columns={v: k for k, v in keep.items()})
    if not np.isfinite(df.to_numpy()).all():
        bad = df.isna().sum()
        sys.exit(f"[错误] 存在 NaN: {bad[bad > 0].to_dict()}")
    return df, [k for k in cols if k not in keep]


def box_smooth(x: np.ndarray, w: int) -> np.ndarray:
    """居中滑动平均，两端用边界值补齐（避免 np.convolve('same') 的边缘缩水）。"""
    if w <= 1 or x.size < 2:
        return x
    pad = w - 1  # 总补齐数，奇偶都保证 'valid' 卷积结果长度 == len(x)
    left = pad // 2
    xp = np.r_[np.full(left, x[0]), x, np.full(pad - left, x[-1])]
    return np.convolve(xp, np.ones(w) / w, mode="valid")


def merge_runs(runs: list[tuple[int, int]], gap: int) -> list[tuple[int, int]]:
    """把间隔 < gap 拍的段合并成一段（同一次过弹被电流抖动打断时会拆成几段）。"""
    if not runs:
        return runs
    out = [runs[0]]
    for s, e in runs[1:]:
        ps, pe = out[-1]
        if s - pe <= gap:
            out[-1] = (ps, e)
        else:
            out.append((s, e))
    return out


def runs_from_mask(mask: np.ndarray) -> list[tuple[int, int]]:
    """把布尔数组切成 [start, end) 的连续 True 段。"""
    idx = np.flatnonzero(mask)
    if idx.size == 0:
        return []
    breaks = np.flatnonzero(np.diff(idx) > 1)
    starts = np.r_[idx[0], idx[breaks + 1]]
    ends = np.r_[idx[breaks] + 1, idx[-1] + 1]
    return list(zip(starts.tolist(), ends.tolist()))


def analyze_segment(df: pd.DataFrame, s: int, e: int, step_rad: float) -> dict:
    """单段统计。段内 span = [s, e)。"""
    seg = df.iloc[s:e]
    t = seg["time"].to_numpy() / 1e6  # µs → s
    dur = t[-1] - t[0]
    travel = seg["ref_pos"].to_numpy()[-1] - seg["ref_pos"].to_numpy()[0]
    shots = travel / step_rad
    # 参考位置对时间的导数 → 射频。用 diff/median(dt) 而不是 polyfit：本段可能含斜坡
    dpos = np.diff(seg["ref_pos"].to_numpy())
    dts = np.diff(t)
    v_inst = np.divide(dpos, dts, out=np.zeros_like(dpos), where=dts > 0)
    rate = v_inst / step_rad
    err = seg["err"].to_numpy()
    cur = np.abs(seg["cur"].to_numpy())
    return {
        "t0": float(t[0]),
        "t1": float(t[-1]),
        "dur_s": float(dur),
        "state": int(seg["state"].to_numpy()[0]),
        "fire": int(seg["fire"].to_numpy()[0]),
        "travel": float(travel),
        "shots": float(shots),
        "rate_mean": float(np.mean(rate)) if rate.size else 0.0,
        "rate_med": float(np.median(rate)) if rate.size else 0.0,
        "rate_max": float(np.max(np.abs(rate))) if rate.size else 0.0,
        "ref_vel_med": float(np.median(seg["ref_vel"].to_numpy())),
        "vel_med": float(np.median(seg["vel"].to_numpy())),
        "err_rms": float(np.sqrt(np.mean(err**2))),
        "err_max": float(np.max(np.abs(err))),
        "cur_mean": float(np.mean(cur)),
        "cur_max": float(np.max(cur)),
        "ref_acc_max": float(np.max(np.abs(seg["ref_acc"].to_numpy()))),
    }


def trace_window(df: pd.DataFrame, t_s: np.ndarray, t0: float, t1: float, step_rad: float) -> None:
    """回放 [t0, t1] 秒内的状态/fire 跳变点与参考位置。
    专门用于定位"卡死在某个状态不动"这类故障：能直接看到参考位置停在哪、离整数发网格差多少，
    以及每个平台的参考增量是否为整数发（终点没闭环成整数发时一眼可见）。"""
    m = (t_s >= t0) & (t_s <= t1)
    sub = df[m]
    ts = t_s[m]
    if len(sub) == 0:
        print(f"\n--- 逐拍回放 [{t0}, {t1}] s：该窗口内无数据 ---")
        return
    st = sub["state"].astype(int).to_numpy()
    fire = sub["fire"].astype(int).to_numpy()
    ref = sub["ref_pos"].to_numpy()
    pos = sub["pos"].to_numpy()
    vr = sub["ref_vel"].to_numpy()
    vf = sub["vel"].to_numpy()

    print(f"\n--- 逐拍回放 [{t0}, {t1}] s（{len(sub)} 点；只打状态/fire 跳变点）---")
    print(f"{'t(s)':>9} {'状态':>6} {'fire':>4} {'ref':>12} {'pos':>12} "
          f"{'vref':>9} {'vfb':>9} {'Δref(发)':>10}")
    base = ref[0]
    last_key = None
    for i in range(len(sub)):
        key = (int(st[i]), int(fire[i]))
        if key != last_key:
            print(f"{ts[i]:9.4f} {STATE_NAMES.get(int(st[i]), '?'):>6} {int(fire[i]):4d} "
                  f"{ref[i]:12.4f} {pos[i]:12.4f} {vr[i]:9.2f} {vf[i]:9.2f} "
                  f"{(ref[i] - base) / step_rad:10.4f}")
        last_key = key
    print(f"  窗口内 ref {ref[0]:.4f} → {ref[-1]:.4f}，增量 {(ref[-1] - ref[0]) / step_rad:.4f} 发")
    print(f"  窗口内 pos 终点 {pos[-1]:.4f}，跟踪误差 {ref[-1] - pos[-1]:.4f} rad "
          f"({(ref[-1] - pos[-1]) / step_rad * 100:+.2f}% 步长)")

    print("  平台明细（**按 fire 切分**，不代表一次完整运动；"
          "一次运动的终点是否落在整数发网格上请用 --stop-check）：")
    i = 0
    while i < len(sub):
        j = i + 1
        while j < len(sub) and (st[j], fire[j]) == (st[i], fire[i]):
            j += 1
        if j - i >= 5:  # 太短的抖动平台不打
            d = (ref[j - 1] - ref[i]) / step_rad
            print(f"    {ts[i]:8.4f}~{ts[j - 1]:8.4f} {STATE_NAMES.get(int(st[i]), '?'):>6} "
                  f"fire={fire[i]} Δref={d:8.4f} 发")
        i = j


def motion_groups(df: pd.DataFrame) -> tuple[list[tuple[int, int]], int]:
    """把「参考在动」的平台切成每次运动的 [start, end)，返回 (区间列表, settle)。

    "在动" = 参考速度 ≠ 0（CH5 = 轨迹速度前馈，空闲/停止态恒 0）。**不能用 ref 的逐拍变化**：
    停/关态的设定值锁实测位置，实测的滑行/回弹会让 ref 一直在变，把多次运动粘成一个区间。
    settle = 停机后再等多少拍才采样实测（避开停机瞬间的滑行）。
    区间**不按状态/fire 切分**，所以"连发 + 松手后的收尾轨迹"算作一次运动。"""
    dt_med = float(np.median(df["dt"].to_numpy()))  # ms
    settle = max(1, int(150.0 / dt_med))
    idx = np.flatnonzero(np.abs(df["ref_vel"].to_numpy()) > 0.5)
    groups: list[tuple[int, int]] = []
    if idx.size == 0:
        return groups, settle
    s = int(idx[0])
    p = int(idx[0])
    for k in idx[1:]:
        k = int(k)
        if k - p > 3:  # 间隔超过 3 拍才算两次运动（减速末端的取整抖动不算断开）
            groups.append((s, p + 1))
            s = k
        p = k
    groups.append((s, p + 1))
    return groups, settle


def net_check(df: pd.DataFrame, step_rad: float) -> None:
    """逐发**净推进量**（上一发起转前一拍到这一发起转前一拍）。

    与 `--stop-check` 的区别：--stop-check 在轨迹结束后固定 +150 ms 采样，那时往往还在
    "空闲爬行"的半途，量到的是**瞬时**欠走；本项在"下一次起转前一拍"采样，量到的是
    **净**推进。两者一对比就能区分两种病：
      · 净≈1.0 而瞬时<1.0  → 只是收尾慢，靠空闲爬补上了（相位不丢，但慢 + 顶牛发热）；
      · 净≠1.0             → 真的有发数累积丢失（相位漂移，会打偏）。
    """
    groups, _ = motion_groups(df)
    pos = df["pos"].to_numpy()
    if len(groups) < 2:
        print("\n--- 逐发净推进量 ---\n  （运动次数不足，跳过）")
        return
    pk = np.array([pos[max(0, s - 1)] for s, _ in groups]) / step_rad  # 每次运动起转前一拍的位置（单位：发）
    d = np.diff(pk)
    n = np.arange(len(d))
    # 与 print_phase_check 同口径：1 发 = step_rad rad = 拨盘 30°，趋势按 °/100 发
    deg_per_shot = 360.0 / (2.0 * np.pi / step_rad)  # = 30.0
    slope = float(np.polyfit(n, d, 1)[0]) * deg_per_shot * 100.0  # °/100 发
    print("\n--- 逐发净推进量（起转前一拍 → 下一次起转前一拍）---")
    print(f"  次数 {len(d)}：中位 {np.median(d):.4f} 发（{np.median(d) * deg_per_shot:.1f}°）  "
          f"均值 {d.mean():.4f} 发  标准差 {d.std():.4f} 发（{d.std() * deg_per_shot:.1f}°）")
    print(f"  min {d.min():+.4f} 发（{d.min() * deg_per_shot:+.1f}°）  "
          f"max {d.max():+.4f} 发（{d.max() * deg_per_shot:+.1f}°）")
    print(f"  偏离 1.0000 发：|Δ−1|>0.1 发 {np.mean(np.abs(d - 1.0) > 0.1) * 100:.1f}%  "
          f"|Δ−1|>0.5 发 {np.mean(np.abs(d - 1.0) > 0.5) * 100:.1f}%")
    print(f"  线性趋势 {slope:+.2f}°/100 发（≈0 = 相位不漂；与 --stop-check 的趋势同号即真漂，"
          f"只是瞬时欠走的话这里应为 0）")
    bad = np.flatnonzero(np.abs(d - 1.0) > 0.5)
    for k in bad[:20]:
        print(f"    第 {k + 1}→{k + 2} 次：净 {d[k]:+.4f} 发（{d[k] * deg_per_shot:+.1f}°）")


def stop_check(df: pd.DataFrame, t_s: np.ndarray, step_rad: float) -> None:
    """每次"运动"（参考开始动 → 参考再次停稳）的总位移是否落在整数发网格上 —— 停止行为的核心指标：
    单发应 1 发、三发应 3 发、连发应整数发（松手后收尾轨迹把终点对齐到网格）。
    运动区间见 motion_groups（不按 fire 切分）—— trace 的按 fire 切分平台会把一次发弹切成几段，
    那时的"距整"没有意义。"""
    ref = df["ref_pos"].to_numpy()
    pos = df["pos"].to_numpy()
    n = len(ref)
    st = df["state"].astype(int).to_numpy()
    groups, settle = motion_groups(df)
    if not groups:
        print("\n--- 停止检查：参考位置全程未动 ---")
        return

    print("\n--- 停止检查（每次运动的总位移 vs 整数发网格）---")
    print("  参考发数 = 轨迹走的量；实测发数 = 机构真正转过的量（停机 150 ms 后取），"
          "后者才是'停在哪'。")
    print(f"{'t0(s)':>8} {'t1(s)':>8} {'时长':>6} {'涉及状态':>14} {'位移(rad)':>10} "
          f"{'参考发数':>9} {'距整':>7} {'实测发数':>9} {'实测距整':>9}")
    bad = 0
    pmax = 0.0
    nshort = 0
    singles: list[float] = []          # 单发运动的"静息→静息"逐发有效推进量（发）
    si0: list[float] = []              # 对应的起点时刻 (s)，用于看是否相邻两发互相补偿
    for gi, (s, e) in enumerate(groups):
        names = "/".join(STATE_NAMES.get(x, f"?{x}") for x in sorted(set(st[s:e].tolist())))
        disp_rad = ref[e] - ref[s]
        disp = disp_rad / step_rad
        off = abs(disp - round(disp))
        # 实测位移取**运动前后各一段静止期**的值：起点取上一段运动结束后、本次开始前的静止值，
        # 终点取本次结束后 150 ms 的值 —— 都要卡在"下一次运动开始之前"，否则快速连点时会取到
        # 运动中途的瞬时位置（曾报出 0.49 发的假偏差）。
        prev_e = groups[gi - 1][1] if gi > 0 else 0
        next_s = groups[gi + 1][0] if gi + 1 < len(groups) else n
        k0 = max(s - settle, prev_e)
        k1 = min(e + settle, next_s - 1, n - 1)
        pdisp = (pos[k1] - pos[k0]) / step_rad
        poff = abs(pdisp - round(pdisp))
        mark = ""
        if off > 0.05:
            mark = "  ⚠ 参考终点未落在整数发网格上"
            bad += 1
        if (s - prev_e) < settle or (next_s - e) < settle:
            mark += "  (前后间隔不足 150 ms，实测列仅供参考)"
            nshort += 1
        else:
            pmax = max(pmax, poff)
        print(f"{t_s[s]:8.3f} {t_s[e - 1]:8.3f} {t_s[e - 1] - t_s[s]:6.3f} {names:>14} "
              f"{disp_rad:10.3f} {disp:9.4f} {off:7.4f} {pdisp:9.4f} {poff:9.4f}{mark}")
        if names == STATE_NAMES[SINGLE_STATE]:  # 只看纯单发（多状态的连发/三发发数不同，混在一起无意义）
            singles.append(pdisp)
            si0.append(float(t_s[s]))
    rad = pmax * step_rad
    print(f"  合计 {len(groups)} 次运动，{bad} 次参考终点偏离整数发 > 0.05 发；"
          f"实测停机位置距整最大 {pmax:.4f} 发 = {rad:.3f} rad(电机轴) = "
          f"{rad / DEFAULT_GEAR_RATIO / np.pi * 180:.2f}° (拨盘轴，孔间距 30°)"
          f"（另有 {nshort} 次前后间隔不足 150 ms，实测列未纳入统计）")

    if len(singles) < 5:
        return
    # ---- 单发逐发有效推进量分布 ----
    # ⚠ 这才是用户看到的现象的**量**：弹丸出不出的判别不在拨盘侧（拨盘只转而没多转/少转），
    #   但"一次 2 颗 / 不发"若伴随推进量异常，这一个分布就能看出来（≈2 发 / ≈0 发）。
    a = np.array(singles)
    deg = step_rad / DEFAULT_GEAR_RATIO * 180.0 / np.pi  # 1 发 → 拨盘角度 (deg) = 30°（step_rad 已含 25.6 减速比）
    print(f"\n--- 单发逐发有效推进量（静息→静息，n={len(a)}）---")
    print(f"  min {a.min():.4f}  p5 {np.percentile(a, 5):.4f}  中位 {np.median(a):.4f}  "
          f"p95 {np.percentile(a, 95):.4f}  max {a.max():.4f} 发")
    print(f"  **std {a.std(ddof=1):.4f} 发 = {a.std(ddof=1) * deg:.2f}°（拨盘轴，一发 = {deg:.1f}°）**")
    edges = [(0.0, 0.8), (0.8, 1.02), (1.02, 1.2), (1.2, 9.0)]
    for lo, hi in edges:
        m = (a >= lo) & (a < hi)
        print(f"  {lo:.2f}~{hi:.2f} 发: {int(m.sum()):4d} 发 ({m.mean() * 100:5.1f}%)")
    # 相邻两发是否成对补偿：|x_i + x_{i+1} − 2| 明显小于 |x_i − 1| + |x_{i+1} − 1| 才算补偿
    if len(a) >= 3:
        pair = np.abs(a[:-1] + a[1:] - 2.0)
        ind = np.abs(a[:-1] - 1.0) + np.abs(a[1:] - 1.0)
        ncomp = int((pair < ind - 0.05).sum())
        lag1 = float(np.corrcoef(a[:-1], a[1:])[0, 1])
        print(f"  相邻两发**成对补偿**的配对数 {ncomp}/{len(a) - 1}；lag-1 自相关 {lag1:+.3f}"
              f"（负 = 这一发多走、下一发就少走，即在还上一发的债）")
        imin, imax = int(np.argmin(a)), int(np.argmax(a))
        ms = f"{a[imin]:.4f} 发 @t={si0[imin]:.2f}s"
        Ms = f"{a[imax]:.4f} 发 @t={si0[imax]:.2f}s"
        print(f"  最小 {ms}；最大 {Ms}（两者{'相邻' if abs(imin - imax) == 1 else '相隔 ' + str(abs(imin - imax)) + ' 发'}）")


def fric_trace(df: pd.DataFrame, t_s: np.ndarray, t0: float, t1: float) -> None:
    """打印时间窗内摩擦轮逐拍样本（CH20 原始转速相对名义、CH21 电流、CH22 下发），
    用来**眼看**过弹的波形形状 —— 判弹门限的定标只能靠它，不能靠猜（500 Hz 下坑只有 1~3 点）。"""
    fv = df["fric_vel"].to_numpy()
    fc = df["fric_cur"].to_numpy()
    fo = df["fric_out"].to_numpy()
    m = (t_s >= t0) & (t_s <= t1)
    if not m.any():
        print(f"  （{t0}~{t1}s 无样本）")
        return
    nom = float(np.median(np.abs(fv)))
    print(f"\n--- 摩擦轮波形回放 {t0}~{t1}s（名义 {nom:.1f} rad/s）---")
    print(f"  {'t(s)':>9} {'CH20(rad/s)':>12} {'跌(rad/s)':>10} {'跌%':>7} {'CH21(A)':>9} {'CH22(A)':>9}")
    for i in np.flatnonzero(m):
        d = nom - abs(fv[i])
        print(f"  {t_s[i]:9.3f} {fv[i]:12.2f} {d:10.2f} {d / nom * 100:6.2f}% {fc[i]:9.3f} {fo[i]:9.3f}")


def zoom_trace(df: pd.DataFrame, t_s: np.ndarray, t0: float, t1: float, every: int) -> None:
    """打印时间窗内**拨盘控制环**的逐拍样本（每 every 拍一行）。

    与 --trace 互补：--trace 只打状态/fire 跳变点（看"参考落在哪格"），本项打连续样本
    （看"参考走到哪、机构跟到哪、谁在出力"）。定位「一发只走 0.85 发、剩下的靠空闲期
    慢慢爬」这类**跟踪缺陷**只能靠它 —— 段级统计里 ref 的行程是整数发，看不出机构差多少。"""
    m = np.flatnonzero((t_s >= t0) & (t_s <= t1))
    if m.size == 0:
        print(f"  （{t0}~{t1}s 无样本）")
        return
    ref = df["ref_pos"].to_numpy()
    pos = df["pos"].to_numpy()
    err = df["err"].to_numpy()
    rv = df["ref_vel"].to_numpy()
    v = df["vel"].to_numpy()
    cur = df["cur"].to_numpy()
    st = df["state"].to_numpy()
    fi = df["fire"].to_numpy()
    has_ext = "angle_out" in df.columns
    ao = df["angle_out"].to_numpy() if has_ext else None
    si = df["speed_i"].to_numpy() if has_ext else None
    ff = df["friction_ff"].to_numpy() if has_ext else None
    print(f"\n--- 拨盘逐拍回放 {t0}~{t1}s（每 {every} 拍一行）---")
    head = f"  {'t(s)':>9} {'状态':>4} {'F':>1} {'ref':>9} {'pos':>9} {'err':>7} {'vref':>7} {'vfb':>7} {'CH8_I':>7}"
    if has_ext:
        head += f" {'CH16_out':>9} {'CH14_i':>7} {'CH19_ff':>8}"
    print(head)
    for k, i in enumerate(m):
        if k % every:
            continue
        row = (f"  {t_s[i]:9.3f} {STATE_NAMES.get(int(st[i]), '?'):>4} {int(fi[i]):1d} "
               f"{ref[i]:9.3f} {pos[i]:9.3f} {err[i]:7.3f} {rv[i]:7.2f} {v[i]:7.2f} {cur[i]:7.2f}")
        if has_ext:
            row += f" {ao[i]:9.2f} {si[i]:7.3f} {ff[i]:8.3f}"
        print(row)


def print_phase_check(df: pd.DataFrame, t_s: np.ndarray, step_rad: float) -> None:
    """停机**相位**（相对孔位网格）随发次的漂移 —— 诊断「重锚把跟踪误差写进参考系」这一缺陷。

    定义 e_k = 第 k 次停机的实测位置 − (首次停机位置 + k×step)，即"相对于一个与孔位对齐的固定
    网格，机构实际停偏了多少"。三种实现的 e_k 特征完全不同：

      终点锚实测+重锚（当前）  ：重锚把上一次的跟踪误差**永久写进参考系** → e_k 出现**阶跃**
      终点锚固定网格（无重锚）  ：e_k ≡ 本发的跟踪误差 η_k，白噪声、有界、无趋势
      参考是纯阶梯（不锁实测）  ：e_k → 0（位置环在空闲期继续把机构拉到孔位）

    ⚠ e_k 的**量级**只有第一种能压下来；后两种靠"参考系绝对 + 环路收敛"。所以看两件事：
    e_k 有没有**阶跃**（重锚特征）、以及 e_k 的**谷值/趋势**（有没有收敛）。
    """
    pos = df["pos"].to_numpy()
    groups, settle = motion_groups(df)
    n = len(pos)
    if len(groups) < 5:
        return
    rest = []
    for gi, (s, e) in enumerate(groups):
        next_s = groups[gi + 1][0] if gi + 1 < len(groups) else n
        k1 = min(e + settle, next_s - 1, n - 1)
        rest.append(float(pos[k1]))
    r = np.array(rest)
    e = r - r[0] - np.arange(len(r), dtype=float) * step_rad
    e = e - np.median(e)
    deg = step_rad / DEFAULT_GEAR_RATIO * 180.0 / np.pi          # 1 发 → 拨盘角度 (deg)
    print(f"\n--- 停机相位（相对孔位网格，n={len(e)} 次停机）---")
    print(f"  e_k: std {e.std(ddof=1) / step_rad * deg:5.2f}°  "
          f"min {e.min() / step_rad * deg:+6.2f}°  max {e.max() / step_rad * deg:+6.2f}°"
          f"   （一发 = {deg:.1f}°，孔位容差通常按 ±¼ 孔 = ±7.5° 量级看）")
    de = np.diff(e) / step_rad * deg
    print(f"  逐发变化 Δe: std {de.std(ddof=1):5.2f}°  max |Δe| {np.abs(de).max():5.2f}°"
          f"   lag-1 自相关 {float(np.corrcoef(e[:-1], e[1:])[0, 1]):+.3f}")
    # 阶跃检测：重锚会把参考系整体挪一格，e_k 上表现为一次孤立的大跳
    big = np.flatnonzero(np.abs(de) > 3.0 * de.std(ddof=1))
    if big.size:
        print(f"  ⚠ |Δe| > 3σ 的跳变 {big.size} 处（疑似**重锚**把跟踪误差写进参考系）：")
        for k in big[:8]:
            print(f"    第 {k + 1}→{k + 2} 发 @t={t_s[groups[k + 1][0]]:.2f}s: {de[k]:+.2f}°")
    else:
        print("  OK：无 3σ 跳变（未观察到重锚特征）")
    # 趋势：与发次线性回归的斜率（° / 发）—— 持续不为 0 = 参考系在系统性漂移
    k = np.arange(len(e), dtype=float)
    slope = float(np.polyfit(k, e / step_rad * deg, 1)[0])
    print(f"  线性趋势 {slope:+.4f}° / 发（{slope * 100:+.2f}° / 100 发）；"
          f"{'⚠ 系统性漂移，参考系在与孔位错开' if abs(slope) * 100 > 3 else '无系统性漂移'}")


def drift_sim(df: pd.DataFrame, t_s: np.ndarray, step_rad: float,
              groups: list[tuple[int, int]], settle: int) -> None:
    """发数漂移推演：把本次日志实测的**收尾超调** os = 实测位移 − 参考位移 当作每发的净误差，
    对比两种规划基准的累计结果 —— 这就是 2026-10-11 那次修复（终点锚理论网格）的量化依据。

      旧基准 p1 = 实测起点 + span：超调被下一发的起点继承 → 逐发**累加**，N 发后偏 Σos；
      新基准 p1 锚理论网格 trigger_grid_pos：实测只在网格 ±一次超调内浮动 → 不累加。

    前提：超调由"轨迹收尾时机构仍在动 + 空闲态 ref 锁实测"决定，与规划基准无关，故同一份
    日志的 os 对两种基准都成立（新基准下 os 的**量级**不变，只是不再被下一发继承）。"""
    ref = df["ref_pos"].to_numpy()
    pos = df["pos"].to_numpy()
    n = len(ref)
    per: list[tuple[float, float, float, float]] = []  # (t0, 参考发数, 实测发数, os)
    for gi, (s, e) in enumerate(groups):
        prev_e = groups[gi - 1][1] if gi > 0 else 0
        next_s = groups[gi + 1][0] if gi + 1 < len(groups) else n
        k0 = max(s - settle, prev_e)
        k1 = min(e + settle, next_s - 1, n - 1)
        if k1 <= k0:
            continue
        ref_d = (ref[e] - ref[s]) / step_rad
        meas_d = (pos[k1] - pos[k0]) / step_rad
        per.append((float(t_s[s]), float(ref_d), float(meas_d), float(meas_d - ref_d)))
    if not per:
        print("\n--- 发数漂移推演：没有可用的运动段 ---")
        return

    os = np.array([p[3] for p in per])
    old = np.cumsum(os)  # 旧基准：逐发累加
    new = os             # 新基准：有界，恒 ≈ 单次超调
    print("\n--- 发数漂移推演（终点锚实测 vs 锚理论网格）---")
    print("  os = 每次运动的（实测位移 − 参考位移），单位「发」。旧基准把它逐发继承并累加；")
    print("  新基准终点锚理论网格，漂移恒 ≈ 单次 os、不累加。")
    print(f"{'#':>3} {'t0(s)':>8} {'参考发数':>9} {'实测发数':>9} {'os(发)':>8} "
          f"{'旧累计(发)':>11} {'新累计(发)':>11}")
    for i, (t0, ref_d, meas_d, o) in enumerate(per):
        print(f"{i + 1:3d} {t0:8.2f} {ref_d:9.3f} {meas_d:9.3f} {o:8.4f} {old[i]:11.4f} {new[i]:11.4f}")
    print(f"  os: 均值 {os.mean():+.4f}  中位 {np.median(os):+.4f}  |os|max {np.abs(os).max():.4f} 发"
          f"（= {np.abs(os).max() * step_rad:.2f} rad）")
    verdict = "多送" if old[-1] > 0.5 else ("少送" if old[-1] < -0.5 else "不足")
    print(f"  机构视角：{len(per)} 次运动，旧基准净漂移 {old[-1]:+.3f} 发（{verdict}整颗弹丸），"
          f"新基准 {new[-1]:+.3f} 发。")


def print_control_core_checks(df: pd.DataFrame, args) -> None:
    """控制核内部量自检（CH13~CH19，仅在日志含扩展列时调用）。

    对应带负载整定的三件事：① 卡住保护有没有误触发/该触发没触发；② 速度环顶没顶到限幅、
    I 项是不是在 0 附近（= 摩擦前馈补到位没有，判据见 app_shoot_def.h §静摩擦前馈）；
    ③ 变速积分门限把误差切在哪一档。
    """
    sp = df["speed_p"].to_numpy()      # 速度环 P 项 (A)
    si = df["speed_i"].to_numpy()      # 速度环 I 项 (A)
    stuck = df["stuck"].to_numpy()     # 卡住保护状态
    ang_out = df["angle_out"].to_numpy()
    serr = np.abs(df["speed_err"].to_numpy())
    ffn = df["friction_ff"].to_numpy()
    refv = df["ref_vel"].to_numpy()

    print("\n--- 卡住保护自检（CH15：0 正常 / 1 卡住未达阈值 / 2 卡住且已切断输出）---")
    n1, n2 = int((stuck == 1).sum()), int((stuck == 2).sum())
    ev = int(((stuck != 0) & np.r_[True, stuck[:-1] == 0]).sum())  # 0 → 非0 的上升沿数
    print(f"  样本：正常 {int((stuck == 0).sum())}  卡住未切断 {n1}  已切断 {n2}；"
          f"卡住事件 {ev} 次（占全长 {ev / max(len(stuck), 1) * 100:.2f}%）")
    if n1 or n2:
        v = np.abs(df["vel"].to_numpy()[stuck != 0])
        r = np.abs(refv[stuck != 0])
        print(f"  卡住期间 实测转速中位 {np.median(v):.3f} rad/s（判据②'没动'，应≈0）；"
              f"参考速度中位 {np.median(r):.3f} rad/s（判据①'要求它动'，应远离 0）")
    print(f"  {'⚠ 发生真卡死切断 —— 正常发弹中若出现，说明 TRIGGER_STUCK_CUT_A 偏低或确有卡弹' if n2 else '  OK：本次没有触发真卡死切断'}")

    print("\n--- 速度环（CH13 P / CH14 I / CH4 输出）---")
    moving = np.abs(refv) > 1.0  # 只在轨迹要求转动时统计，避免空闲锁位段污染
    if int(moving.sum()) > 10:
        # 恒等式 CH4 = −(P + I + 前馈)（d 项恒 0）；漏掉 CH19 会把"前馈顶限幅"误报成饱和
        out_mag = np.abs(sp + si + ffn)[moving]
        cmd_mag = np.abs(df["torque"].to_numpy()[moving])
        print(f"  输出限幅饱和占比 {np.mean(out_mag > cmd_mag + 1e-3) * 100:.2f}%"
              f"（|CH13+CH14+CH19| > |CH4| ⇒ 顶在 ±{args.i_max:.0f}A 上）")
        print(f"  I 项 CH14：中位 {np.median(si[moving]):+.3f}  均值 {si[moving].mean():+.3f} A"
              f"（**前馈补位判据**：补到位应在 0 附近正负晃，系统性偏在一侧 = 前馈偏了）")
        if np.abs(ffn).max() > 1e-6:
            print(f"  前馈 CH19：[{ffn.min():+.3f}, {ffn.max():+.3f}] A，|ff| 中位 {np.median(np.abs(ffn[moving])):.3f} A")
        else:
            print("  前馈 CH19 恒 0 —— 摩擦前馈**尚未接线**（见 app_shoot_def.h §静摩擦前馈）")
    else:
        print("  （本日志没有明显转动段，跳过）")

    print("\n--- 位置环 / 变速积分 ---")
    print(f"  位置环输出 CH16 贴限幅(≥{args.angle_vmax * 0.95:.0f} rad/s) "
          f"{np.mean(np.abs(ang_out) >= args.angle_vmax * 0.95) * 100:.1f}%")
    print(f"  速度环误差 |CH18|：中位 {np.median(serr):.2f}  p95 {np.percentile(serr, 95):.2f} rad/s")
    print(f"    变速积分档位（B=40/A=100）：全积分 {np.mean(serr <= 40) * 100:.1f}%  "
          f"衰减 {np.mean((serr > 40) & (serr <= 140)) * 100:.1f}%  冻结 {np.mean(serr > 140) * 100:.1f}%")


def print_col_scan(df: pd.DataFrame) -> None:
    """逐通道取值扫描：找出"只取极少几个值"的通道。

    用途：现场对着波形说"某一路全是 0 和 2"时，先确定是哪一路 —— 状态量（CH10/CH11/CH15/CH25）
    本来就只有几个离散值，而模拟量只取 0/2 说明它**根本没在送**（接线掉了/被常量覆盖）。
    判定：去重后 ≤6 个值 → 当离散量列出取值与占比；否则报 min/中位/max。
    ⚠ 必须用 load_csv 洗过的 df，不能用 pd.read_csv 的 raw：raw 的首行是 VOFA 通道名（字符串），
      让它进 unique/min/median 会把 I1/I3/I8 报成 nan 与 1e38 的垃圾（曾误判成"通道坏了"）。
    """
    print("\n--- 逐通道取值扫描（--col-scan）---")
    print(f"  {'列':>4} {'含义':>14} {'去重':>5}  取值 / 统计")
    iname_of = {**CORE_COLS, **EXT_COLS, **FRIC_COLS}   # 变量名 → VOFA 列名 (I0..I25)
    for c in df.columns:
        x = df[c].to_numpy()
        u = np.unique(x)
        tag = iname_of.get(c, "")
        if len(u) <= 6:
            cnt = {f"{v:g}": int((x == v).sum()) for v in u}
            body = "  ".join(f"{k}×{v}" for k, v in cnt.items())
        else:
            body = (f"min {x.min():.3f}  中位 {np.median(x):.3f}  max {x.max():.3f}  "
                    f"零值占比 {np.mean(x == 0) * 100:.1f}%")
        print(f"  {tag:>4} {c:>14} {len(u):>5}  {body}")


def print_trigger_current_budget(df: pd.DataFrame, args) -> None:
    """拨盘电流预算（CH8 实测电流 按状态拆分）。

    判据用法：**停止/关** 段里 |I| 明显非零而机构不动 = 环路在静摩擦上绕死（只发热不出活）；
    **单发** 段里 |I| 长期贴 `--trig-sat-i` = 轨迹要的力矩已经超出电机上限，
    此时跟踪滞后与欠走是"做不到"而非"没调好"，得先改轨迹而不是加增益。
    """
    st = df["state"].to_numpy()
    cur = np.abs(df["cur"].to_numpy())  # CH8 = 实测电流 (A)
    names = {0: "关", 1: "单发", 2: "三发", 3: "连发", 4: "停止"}
    sat = args.trig_sat_i
    print(f"\n--- 拨盘电流预算（CH8 实测电流，按状态拆分；饱和判据 ≥{sat:.1f} A）---")
    print(f"  {'状态':>4} {'样本':>7} {'占比%':>6} {'中位':>7} {'p95':>7} {'max':>7} {'≥饱和%':>8}")
    for code, name in names.items():
        m = st == code
        if int(m.sum()) == 0:
            continue
        c = cur[m]
        print(f"  {name:>4} {int(m.sum()):>7} {m.mean() * 100:>6.1f} {np.median(c):>7.3f} "
              f"{np.percentile(c, 95):>7.3f} {c.max():>7.3f} {np.mean(c >= sat) * 100:>8.2f}")
    # 动/不动拆分：闸门用实测转速，不用状态。停止/关 里"不动却灌流"就是**静摩擦顶牛**
    # （机构不吃这一口，环路只有积分在涨），是纯发热且会成为下一发的反向刹车。
    vel = np.abs(df["vel"].to_numpy())
    secs = df["dt"].to_numpy().sum() / 1000.0  # dt 是 ms
    print(f"  {'状态':>8} {'不动样本':>8} {'占比%':>6} {'不动时长s':>9} {'|I|中位':>8} {'p95':>7} {'max':>7}")
    for code, name in list(names.items()) + [(None, "全体")]:
        m = np.ones(len(st), dtype=bool) if code is None else (st == code)
        if int(m.sum()) == 0:
            continue
        s = m & (vel < 1.0)
        if int(s.sum()) == 0:
            print(f"  {name:>8} {0:>8} {0.0:>6.1f} {0.0:>9.2f}")
            continue
        c = cur[s]
        ts = df["dt"].to_numpy()[s].sum() / 1000.0
        print(f"  {name:>8} {int(s.sum()):>8} {s.sum() / len(st) * 100:>6.1f} {ts:>9.2f} "
              f"{np.median(c):>8.3f} {np.percentile(c, 95):>7.3f} {c.max():>7.3f}")
    print(f"    （全长时间 {secs:.2f}s；'不动'= |vfb|<1 rad/s。停止/关 里不动却持续灌流 = 静摩擦顶牛）")


def print_friction_checks(df: pd.DataFrame, args) -> tuple[np.ndarray, np.ndarray]:
    """摩擦轮自检（CH20~CH25，仅在日志含摩擦轮列时调用）：判弹丸有没有真的过轮、什么时候过轮。

    ⚠ 名义转速不写死 FRICTION_SPEED_FULL：方向/增益随 Config 而变，脚本取**实测**中位数为基准，
    并把它打印出来供人工核对（应 ≈ app_shoot_def.h 的 FRICTION_SPEED_FULL）。
    返回过弹事件的时刻数组 (s)，供与拨盘发次配对（print_feed_correlation）。
    """
    fv = df["fric_vel"].to_numpy()      # CH20 原始转速 (rad/s, app 正向)
    fc = df["fric_cur"].to_numpy()      # CH21 原始电流 (A, app 正向)
    fo = df["fric_out"].to_numpy()      # CH22 速度环输出 (A)
    fi = df["fric_i"].to_numpy()        # CH23 I 项 (A)
    fe = df["fric_err"].to_numpy()      # CH24 速度误差 (rad/s)
    live = df["fric_live"].to_numpy()   # CH25 使能且在线
    t_s = df["time"].to_numpy() / 1e6

    print("\n--- 摩擦轮自检（CH20~CH25：判弹丸过轮）---")
    on = live > 0.5
    if int(on.sum()) < 10:
        print(f"  ⚠ 摩擦轮全程未使能/无反馈（CH25 仅 {int(on.sum())} 拍为 1）→ 本次日志判不了弹")
        return np.empty(0), np.empty(0)
    # 名义转速：取**使能拍** |CH20| 的中位数（空转占绝大多数时间）。用中位数而非均值，
    # 免得过弹掉速把基准拉低（掉速正是我们想检出的东西，不能进基准）。
    nom = float(np.median(np.abs(fv[on])))
    if nom < 1.0:
        print(f"  ⚠ 使能拍名义转速仅 {nom:.3f} rad/s ≈ 0 → 摩擦轮没转起来（判据无从谈起），"
              f"先查 CH25/使能与反馈")
        return np.empty(0), np.empty(0)
    sgn = float(np.median(fv[on]))
    print(f"  名义转速（使能拍 |CH20| 中位）= {nom:.2f} rad/s；带符号中位 {sgn:+.2f} rad/s"
          f"（{'与设定值同号 ✓' if sgn > 0 else '⚠ 与设定值反号，查方向配置'}）")
    print(f"  使能拍占比 {on.mean() * 100:.1f}%；空转电流 CH21 中位 {np.median(fc[on]):+.3f} A")

    dt_med = float(np.median(np.diff(t_s)))  # 采样周期 (s)
    win = max(1, int(round(args.fric_win_ms / 1e3 / max(dt_med, 1e-6))))
    smc = box_smooth(np.abs(fc), win)  # 平滑电流 (A)
    smv = box_smooth(np.abs(fv), win)  # 平滑转速 (rad/s)

    # ---- 工作点自检：命令是否饱和 / 实测转速够不够设定值 / 电流噪声底 ----
    # 2026-10-11 第五轮实测教训（FRICTION_SPEED_FULL 从 191 抬到 956 rad/s 后）：电机**根本到不了**
    #   设定值 —— 实测中位仅 ~833 rad/s，CH22 命令恒顶 −20 A，而 CH21 实测电流只有 ~1.65 A。
    #   命令 20 A、实际只吸 1.65 A = 卡的是**反电势（转速上限）**、不是力矩。此时：
    #     ① 速度环 P 项 = kp×(设定−实测) = 0.2×123 = 24.6 A > out_max(20 A) ⇒ **整段 P 饱和**，
    #        速度根本不受控（等于开环给最大电流）；
    #     ② 电流信号失去"负载签名"：命令恒在限幅、实测电流被 PWM 纹波主导（逐拍带宽 ~2 A），
    #        原来的平滑电流判弹器直接崩掉（691 段 / 90 发）。
    #   所以这里把工作点摆出来，并据此切换判弹判据（见下）。
    fo_on = np.abs(fo[on])
    sat_frac = float(np.mean(fo_on >= 0.95 * args.fric_i_max))
    v_on, c_on = np.abs(fv[on]), np.abs(fc[on])
    v_sm = np.abs(smv[on])
    c_band = float(np.percentile(c_on, 95) - np.percentile(c_on, 5))
    v_band = float(np.percentile(v_sm, 95) - np.percentile(v_sm, 5))
    print(f"  工作点：CH22 命令 max {fo_on.max():.2f} A，≥95%×{args.fric_i_max:.0f}A 占比 "
          f"{sat_frac * 100:.1f}%" + ("   ⚠ **速度环整段饱和**：速度不受控、电流失去负载签名" if sat_frac > 0.5 else ""))
    print(f"          CH21 电流 中位 {np.median(c_on):.2f}  p5 {np.percentile(c_on, 5):.2f}  "
          f"p95 {np.percentile(c_on, 95):.2f} A（逐拍带宽 {c_band:.2f} A）")
    print(f"          CH20 速度 min {v_on.min():.1f}  p5 {np.percentile(v_on, 5):.1f}  "
          f"max {v_on.max():.1f} rad/s（平滑后带宽 {v_band:.2f} rad/s）")
    if args.fric_set > 0.0:
        ratio = nom / args.fric_set * 100.0
        print(f"  vs 设定值 {args.fric_set:.1f} rad/s（FRICTION_SPEED_FULL）：实测中位 {ratio:.1f}%"
              + ("   ✓ 达速" if ratio >= 98.0 else "   ⚠ **未达速**（配合命令饱和度看：饱和 = 转速上限到了）"))

    # ---- 过弹事件判据 ----
    # 默认 auto：命令**未**饱和时用**平滑电流**（原判据，2026-10-11 第三/四轮实测：CH20 逐拍
    #   噪声 ±5%、比弹丸凹陷还大，只能靠电流）；命令**饱和**时电流失去负载签名，改用**平滑转速
    #   凹陷**（高速下 CH20 相对噪声只有 ~0.5%，反而比电流干净）。
    # 真过弹签名（第四轮，191 rad/s 时）：持续 ~78 ms 的一小段转速凹陷 + 电流抬升。
    mode = args.fric_detect
    if mode == "auto":
        mode = "speed" if sat_frac > 0.5 else "current"
    if mode == "speed":
        thr_v = nom * (1.0 - args.fric_dip_frac)
        hit = on & (smv < thr_v)
        print(f"  过弹判据【转速】：平滑CH20 < {thr_v:.2f} rad/s（名义 {nom:.2f} − "
              f"{args.fric_dip_frac * 100:.2f}%，平滑窗 {args.fric_win_ms:.0f} ms，"
              f"合并间隔 {args.fric_merge_ms:.0f} ms）")
    else:
        base_cur = float(np.median(smc[on]))
        thr = base_cur + args.fric_cur_delta
        hit = on & (smc > thr)
        print(f"  过弹判据【电流】：平滑CH21 > {thr:.3f} A（基准 {base_cur:.3f} + "
              f"{args.fric_cur_delta:.2f} A，平滑窗 {args.fric_win_ms:.0f} ms，"
              f"合并间隔 {args.fric_merge_ms:.0f} ms）")
    runs = merge_runs(runs_from_mask(hit), max(1, int(round(args.fric_merge_ms / 1e3 / dt_med))))
    print(f"  命中 {int(hit.sum())} 拍、合并后 {len(runs)} 段（模式 {mode}）")
    if not runs:
        print("  ⚠ 没有检出任何过弹事件 —— 若本次确有发弹，说明摩擦轮压根没吃到弹"
              "（降低 --fric-cur-delta 可放宽）")
        return np.empty(0), np.empty(0)
    # 凹陷深度用**平滑转速**对**平滑基准**度量（口径一致）；负值 = 事件内转速没低于基准
    # （电流判据下的误触发特征，转速判据下不会出现）。
    nom_sm = float(np.median(smv[on]))
    print(f"  {'#':>3} {'t(s)':>9} {'长(ms)':>7} {'CH21峰(A)':>10} {'CH22峰(A)':>10} "
          f"{'凹陷(rad/s)':>12} {'凹陷%':>7}")
    deep = []
    for k, (s, e) in enumerate(runs):
        ms = (e - s) * dt_med * 1e3
        depth = nom_sm - float(np.min(smv[s:e]))  # 平滑转速在事件内的最低点
        cpk = float(np.max(smc[s:e]))
        opk = float(np.max(box_smooth(np.abs(fo), win)[s:e]))
        deep.append(depth)
        if k < args.fric_max_print:
            print(f"  {k + 1:>3} {t_s[s]:>9.3f} {ms:>7.1f} {cpk:>10.3f} {opk:>10.3f} "
                  f"{depth:>12.2f} {depth / nom_sm * 100:>6.2f}%")
    if len(runs) > args.fric_max_print:
        print(f"  ...（共 {len(runs)} 段，只列前 {args.fric_max_print} 段，--fric-max-print 可调）")
    d = np.array(deep)
    ld = np.array([e - s for s, e in runs]) * dt_med * 1e3
    print(f"  事件时长：中位 {np.median(ld):.1f} ms  p95 {np.percentile(ld, 95):.1f} ms  max {ld.max():.1f} ms")
    print(f"  凹陷深度：中位 {np.median(d):.2f} rad/s（{np.median(d) / nom_sm * 100:.2f}% 名义）  "
          f"最大 {d.max():.2f}（{d.max() / nom_sm * 100:.2f}%）  "
          f"负值(没凹)占 {np.mean(d < 0) * 100:.0f}%")
    print(f"  ⚠ 判弹的**根本局限**：只跟踪**两轮之一**；且只看得到「摩擦轮被加载」，分不清这一下"
          f"是**一颗弹通过**还是**两颗一起卡着蹭**。**必须与拨盘侧发次配对**才算数。")
    if int((np.abs(fe[on]) > 1.0).sum()) > 0:
        print(f"  速度误差 |CH24| > 1 rad/s 的拍数：{int((np.abs(fe[on]) > 1.0).sum())}；"
              f"I 项 CH23 中位 {np.median(fi[on]):+.3f} A（系统性压在一侧 = 环路有稳态偏）")
    # 返回 (事件起始时刻, 每段凹陷深度 rad/s) —— 深度是 CH21/CH20 载荷签名，供隔孔检验用
    return t_s[np.array([s for s, _ in runs])], np.array(deep)


def print_feed_correlation(df: pd.DataFrame, t_s: np.ndarray, step_rad: float,
                           dip_t: np.ndarray, args) -> None:
    """把**拨盘发次**与**摩擦轮过弹事件**配对 —— 这是"一发到底出没出弹"的唯一直接判据。

    每次运动（motion_groups）对应一次拨弹动作；统计它的时间窗内检出几段掉速：
        0 段 = 这一发**没弹**（拨盘转了但弹没被带出去 / 没进摩擦轮）
        1 段 = 正常一发
       ≥2 段 = 一次走了**两颗**
    ⚠ 窗口取 [t_start − 50 ms, t_end + 200 ms]：过弹略滞后于拨盘动作，且弹丸可能被卡在轮口
    一两拍才被拽走；窗口开太大又会把相邻两发（最小间隔 ~230 ms）串起来。
    ⚠ "0 段"有两种：真没弹 / 事件被挤出窗口。故额外报**最近事件的时延分布**与**落在所有窗
    之外的孤儿事件数** —— 若时延中位 ≈ 拨盘行程时长但尾部很长，说明是窗口问题不是没弹。
    """
    groups, _ = motion_groups(df)
    st = df["state"].astype(int).to_numpy()
    if not groups or dip_t.size == 0:
        return
    counts = []
    rows = []
    lags = []
    for gi, (s, e) in enumerate(groups):
        t0 = t_s[s] - args.corr_before
        t1 = t_s[e - 1] + args.corr_after
        c = int(((dip_t >= t0) & (dip_t <= t1)).sum())
        names = "/".join(STATE_NAMES.get(x, f"?{x}") for x in sorted(set(st[s:e].tolist())))
        counts.append(c)
        rows.append((t_s[s], names, c))
        # 最近事件的时延（相对运动起点），用来区分"真没弹"和"事件跑出窗外"
        lags.append(float(dip_t[np.argmin(np.abs(dip_t - t_s[s]))] - t_s[s]))
    c = np.array(counts)
    lag = np.array(lags)
    print("\n--- 拨盘发次 × 摩擦轮过弹 配对（判'没发弹 / 发一颗 / 一次两颗'）---")
    print(f"  共 {len(c)} 次拨盘运动；窗口 [−{args.corr_before * 1e3:.0f}, +{args.corr_after * 1e3:.0f}] ms"
          f"（相对运动起点/终点）；每次运动窗内的过弹段数分布：")
    for k in range(0, int(c.max()) + 1):
        m = (c == k)
        tag = {0: "⚠ 没弹", 1: "正常一发"}.get(k, "⚠ 一次多颗")
        print(f"    {k} 段: {int(m.sum()):4d} 次 ({m.mean() * 100:5.1f}%)   {tag if m.any() else ''}")
    bad0, bad2 = int((c == 0).sum()), int((c >= 2).sum())
    print(f"  ⇒ 没弹 {bad0} 次、一次多颗 {bad2} 次（合计 {(bad0 + bad2) / len(c) * 100:.1f}% 异常）")
    inside = int(sum(c))
    print(f"  事件归属：{len(dip_t)} 段事件中 {inside} 段落在某个窗内、{len(dip_t) - inside} 段是孤儿；"
          f"每发最近事件时延 中位 {np.median(lag) * 1e3:+.0f} ms  p95 {np.percentile(lag, 95) * 1e3:+.0f} ms "
          f"max {lag.max() * 1e3:+.0f} ms")
    if bad0 or bad2:
        print("  异常发次明细（前 12 条）：")
        for t0, names, k in [r for r in rows if r[2] != 1][:12]:
            print(f"    t={t0:7.3f}s  状态 {names:>8}  过弹 {k} 段")


def print_shot_lag(df: pd.DataFrame, t_s: np.ndarray, step_rad: float,
                   dip_t: np.ndarray, args) -> None:
    """每发的**时间漏斗**：参考停住的那一拍机构走到哪、还差多少、靠空闲期爬多久补齐、
    弹又是什么时候出的轮 —— 把「发射滞后」从相位问题里单独拎出来量。

    与 --stop-check / print_phase_check 的分工：那两项取的是**停机稳态**位置（空闲期爬完
    之后的值），所以"每发都停在整格上"能成立；本项看**动态**——参考已经停了，机构还在爬。
    判据分三层：
        走满率 = (pos[参考停] − pos[起转]) / step    参考停住那一刻机构实际走完的比例
        补齐   = 从起转到 |err| < tol 的耗时            这口差值是靠自己爬补的，还是下一发补的
        出弹   = 距离最近的过弹事件相对起转的时刻        弹到底有没有出、比拨盘动作晚多少
    """
    ref = df["ref_pos"].to_numpy()
    pos = df["pos"].to_numpy()
    cur = np.abs(df["cur"].to_numpy())
    n = len(ref)
    groups, _ = motion_groups(df)
    if not groups:
        return
    deg = step_rad / DEFAULT_GEAR_RATIO * 180.0 / np.pi   # 1 发 → 拨盘角度 (deg)
    tol = args.shot_lag_tol
    win = args.shot_lag_win
    print(f"\n--- 单发时间漏斗（参考停住瞬间走了几成 / 空闲期补多久 / 弹何时出轮）---")
    print(f"  走满 = 参考停住那一拍的实测行程 ÷ 1 发；补齐 = 从**起转**到 |误差| < {tol:.3f} 发 的耗时；"
          f"出弹 = 最近的摩擦轮事件相对起转的时刻（搜索窗 +{win * 1e3:.0f} ms）")
    print(f"{'t0(s)':>8} {'时长ms':>7} {'走满':>7} {'差(rad)':>8} {'差(°)':>6} "
          f"{'补齐ms':>7} {'爬行ms':>7} {'出弹ms':>7} {'事件':>4} "
          f"{'|I|max':>7} {'静息I':>6} {'停相':>6}")
    short_all, close_all, creep_all, ev_lag, walk_all = [], [], [], [], []
    settle_all, rest_i_all, imax_all = [], [], []
    never = 0
    for gi, (s, e) in enumerate(groups):
        next_s = groups[gi + 1][0] if gi + 1 < len(groups) else n
        k_end = min(e, n - 1)
        walk = (pos[k_end] - pos[s]) / step_rad            # 参考停住瞬间的实测行程（发）
        short = (ref[k_end] - pos[k_end])                  # 还差多少 (rad)，正 = 机构落后
        # 空闲期爬行：从参考停住那一拍往后找第一拍 |err| < tol 发
        k_close = None
        for k in range(e, next_s):
            if abs(ref[k] - pos[k]) < tol * step_rad:
                k_close = k
                break
        if k_close is None:
            t_close, t_creep = None, None
            never += 1
        else:
            t_close = (t_s[k_close] - t_s[s]) * 1e3        # 相对起转的补齐时刻 (ms)
            t_creep = (t_s[k_close] - t_s[k_end]) * 1e3    # 参考停住之后的爬行时长 (ms)
        ev = dip_t[(dip_t >= t_s[s] - args.corr_before) & (dip_t <= t_s[s] + win)]
        e_lag = ((ev - t_s[s]) * 1e3).tolist()
        # 停位相位：这一发**收尾稳定后**（补齐之后 / 参考停住之后）的实测位置。存绝对值，循环
        # 结束后统一相对第 1 发取模 —— **这是判"停位是否每发都落在同一个机械相位"的唯一口径**：
        # 相位散布小 ⇒ 位置环每发停得一样，交替只能来自机构；相位在两个值之间跳 ⇒ 锅在控制。
        k_set = k_close if k_close is not None else min(k_end + 5, next_s - 1)
        k_set = min(k_set, next_s - 1)
        seg = slice(k_set, max(k_set + 1, next_s))
        p_settle = float(np.median(pos[seg]))
        rest_i = float(np.median(cur[seg]))
        imax = float(np.max(cur[s:k_end + 1]))
        settle_all.append(p_settle)
        rest_i_all.append(rest_i)
        imax_all.append(imax)
        rows = f"{t_s[s]:8.3f} {(t_s[k_end] - t_s[s]) * 1e3:7.1f} {walk:7.3f} "
        rows += f"{short:8.3f} {short / step_rad * deg:6.2f} "
        rows += f"{'—' if t_close is None else f'{t_close:.0f}':>7} "
        rows += f"{'—' if t_creep is None else f'{t_creep:.0f}':>7} "
        rows += f"{('—' if not e_lag else ','.join(f'{x:+.0f}' for x in e_lag)):>7} {len(ev):4d} "
        rows += f"{imax:7.2f} {rest_i:6.2f} {p_settle:6.1f}"
        print(rows)
        short_all.append(short)
        walk_all.append(walk)
        if t_close is not None:
            close_all.append(t_close)
            creep_all.append(t_creep)
        if e_lag:
            ev_lag.extend(e_lag)
    sh = np.array(short_all)
    wa = np.array(walk_all)
    print(f"  走满率：min {wa.min():.3f}  p5 {np.percentile(wa, 5):.3f}  中位 {np.median(wa):.3f}  "
          f"p95 {np.percentile(wa, 95):.3f}  max {wa.max():.3f} 发（1.000 = 参考停住时正好走完）")
    print(f"  参考停住瞬间的缺口：中位 {np.median(sh):.3f} rad = {np.median(sh) / step_rad * deg:.2f}°（拨盘轴）  "
          f"p95 {np.percentile(sh, 95):.3f} rad = {np.percentile(sh, 95) / step_rad * deg:.2f}°  "
          f"max {np.max(sh):.3f} rad = {np.max(sh) / step_rad * deg:.2f}°")
    if close_all:
        c = np.array(close_all)
        k = np.array(creep_all)
        print(f"  空闲期补齐耗时（相对**起转**，n={len(c)}/{len(sh)} 发补上了）："
              f"中位 {np.median(c):.0f} ms  p95 {np.percentile(c, 95):.0f} ms  max {c.max():.0f} ms")
        print(f"    其中**参考停住之后**纯爬行的那段：中位 {np.median(k):.0f} ms  "
              f"p95 {np.percentile(k, 95):.0f} ms  max {k.max():.0f} ms")
        print(f"  ⚠ 未在下一发起转前补齐的有 {never} 发 —— 这几发是**下一发把它补上的**（发数账对得上、"
              f"但这一发的弹已经被拖后）" if never else "  OK：每发都在下一发起转前补齐了")
    if ev_lag:
        ev = np.array(ev_lag)
        print(f"  出弹时刻（相对起转，n={len(ev)} 个事件）：中位 {np.median(ev):+.0f} ms  "
              f"p5 {np.percentile(ev, 5):+.0f} ms  p95 {np.percentile(ev, 95):+.0f} ms  max {ev.max():+.0f} ms")
        print(f"    拨盘起转→参考停住只有 {np.median(np.array([t_s[e - 1] - t_s[s] for s, e in groups])) * 1e3:.0f} ms；"
              f"出弹中位若明显大于它 = 弹是在**空闲爬行**里被推出去的")
    if settle_all:
        ps = np.array(settle_all)
        ph = ((ps - ps[0]) / step_rad) % 1.0
        ph = np.where(ph > 0.5, ph - 1.0, ph)
        print(f"  **停位相位**（每发收尾稳定位置，相对第 1 发，单位=**一个步距** {step_rad:.4f} rad）："
              f"std {np.std(ph):.4f} 步距 = {np.std(ph) * deg:.2f}°(拨盘轴)  "
              f"极差 {ph.max() - ph.min():.4f} 步距")
        print(f"    逐发（负 = 比第 1 发落后）：{' '.join(f'{x:+.3f}' for x in ph)}")
        ri = np.array(rest_i_all)
        print(f"  静息电流（收尾稳定段 |CH8| 中位）：中位 {np.median(ri):.2f} A  min {ri.min():.2f}  "
              f"max {ri.max():.2f} A；>1 A 有 {(ri > 1).sum()}/{ri.size} 发、>3 A 有 {(ri > 3).sum()} 发"
              f"（'顶着弹' vs '空着' 两态）")
        # ---- 相位扫描：步距 < 1 孔时（TRIGGER_STEP_ADVANCE_SCALE < 1），停位相位会逐发遍历，
        #      按"停位相位 mod 1 孔"分 4 个 1/4 孔箱，数 过/不过，判「相位差 vs 机构」。----
        hole_rad = args.hole_rad if args.hole_rad else step_rad
        # ---- 出弹相位：每个过弹事件发生**当时**的实测位置，与停位相位同一零点、mod 1 孔。
        #      若所有事件挤在同一个相位 ⇒ 机构只有一个出弹点（那就是要找的"过弹点"）；
        #      和"停在哪一相"一比，就知道停位该往哪边挪、挪多远。----
        if dip_t.size:
            pos_ev = np.interp(dip_t, t_s, pos)
            ph_e = ((pos_ev - ps[0]) / hole_rad) % 1.0
            ang = 2.0 * np.pi * ph_e
            sn, cs = float(np.sin(ang).mean()), float(np.cos(ang).mean())
            R = float(np.hypot(sn, cs))
            mu = float((np.arctan2(sn, cs) / (2.0 * np.pi)) % 1.0)
            print(f"\n  --- 出弹相位（{dip_t.size} 个过弹事件发生时的实测位置，与停位相位同零点 mod 1 孔）---")
            print(f"    圆均值 {mu:.3f} 孔   合成矢量 R = {R:.3f}（→1 = 所有弹都在同一个机械相位出轮）"
                  f"   极差 {ph_e.max() - ph_e.min():.3f} 孔 = {(ph_e.max() - ph_e.min()) * 360 / args.paddle_holes:.1f}°(拨盘轴)")
            print(f"    逐事件相位（孔）：{' '.join(f'{x:.3f}' for x in ph_e)}")
            for q in range(4):
                m = np.abs(((ph_e - q * 0.25 + 0.5) % 1.0) - 0.5) < 0.125
                print(f"      相位 {q * 0.25:.2f}±0.125 孔：{int(m.sum())} 个事件")
        if hole_rad > step_rad * 1.5:
            ph_h = ((ps - ps[0]) / hole_rad) % 1.0
            sh = np.array(short_all)
            im = np.array(imax_all)
            # 零成本标签：机构在参考停住时到位（差小）**或**电流到过推弹档 ⇒ 判"过"。
            # （第十二轮实测：真出弹 7 发 差≤0.25 且 |I|max 12~17；没出 18 发 差≥0.6 且 |I|max 4~10；
            #   两判据在干净样本上一致，取"或"是为了不吃掉边界样本 —— 逐发三元组已打印，可自行改判。）
            ok = (sh <= args.pass_short) | (im >= args.pass_i)
            print(f"\n  --- 相位扫描（步距 {step_rad / hole_rad:.3f} 孔/发，按 停位相位 mod 1 孔 分 4 箱）---")
            print(f"  标签：过 = 差 ≤ {args.pass_short:.2f} rad（机构到位）或 |I|max ≥ {args.pass_i:.1f} A（推过弹）")
            print(f"{'相位(孔)':>9} {'发数':>5} {'过':>4} {'不过':>5} {'过率':>7}")
            tot = len(ph_h)
            for q in range(4):
                m = np.abs(((ph_h - q * 0.25 + 0.5) % 1.0) - 0.5) < 0.10
                if m.sum() == 0:
                    continue
                k = int(ok[m].sum())
                print(f"{q * 0.25:9.2f} {int(m.sum()):5d} {k:4d} {int(m.sum()) - k:5d} {k / m.sum() * 100:6.0f}%")
            print(f"{'合计':>9} {tot:5d} {int(ok.sum()):4d} {tot - int(ok.sum()):5d} {ok.mean() * 100:6.0f}%")
            print("    逐发 [相位孔/差rad/|I|max/判]：" + " ".join(
                f"{ph_h[i]:.2f}/{sh[i]:.2f}/{im[i]:.1f}/{'过' if ok[i] else '—'}" for i in range(tot)))
            print("  ⇒ 某相位箱过率 ≈100% 而其它 ≈0% ⇒ **相位差问题**（一个固定偏移就是解药）；"
                  "四箱过率接近（都好或都差）⇒ **机构问题**，TRIGGER_GRID_OFFSET_HOLES 扫不出东西。")
        print(f"  ⇒ 判据：停位相位散布 ≲0.05 齿距 ⇒ **每发停在同一个机械相位、位置环没有逐发抖动**，"
              f"交替只能来自机构（单发限位/弹链）；相位在两个值之间跳 ⇒ 锅在控制/网格。")


def print_osc_check(df: pd.DataFrame, t_s: np.ndarray, step_rad: float, args) -> None:
    """逐发**停顿期**的振荡检测（去掉缓变趋势看残差）—— 用来证实/证伪实车观察
    「某些位置卡在单发限位时，拨盘位置环会震荡」。

    对每个"参考停住 → 下一发起转"的停顿窗取实测位置，先减掉**线性趋势**（卡住时机构是缓慢
    爬行，线性最能代表那股缓变），残差才是"来回晃"的那部分：
        残差峰峰值 p-p ≥ 门限 ⇒ 这一发停顿期在振荡；`过零次数 / 2 / 窗长` = 振荡频率。
    末了按**停顿末位置 mod 1 孔**分箱交叉，看振荡是不是只集中在某些相位 —— 只有"集中"才
    说明是相位差问题，"四箱一样"说明是卡住这件事本身（机构/静摩擦）。
    ⚠ 采样 2 ms ⇒ 可测频率上限 250 Hz；拨盘惯量大，真振荡一般是几~几十 Hz，够用。
      含积分饱和的"爬-跳"循环会表现成大 p-p、**极少过零**，看 p-p 别看频率。
    """
    pos = df["pos"].to_numpy()
    cur = df["cur"].to_numpy()
    n = len(pos)
    groups, _ = motion_groups(df)
    if len(groups) < 2:
        print("\n--- 停顿期振荡检测：运动段不足，跳过 ---")
        return
    rows = []
    for gi, (s, e) in enumerate(groups[:-1]):
        a = min(e + args.osc_skip, n - 1)
        b = groups[gi + 1][0]
        if b - a < args.osc_min_n:
            continue
        t = t_s[a:b] - t_s[a]
        y = pos[a:b]
        r = y - np.polyval(np.polyfit(t, y, 1), t)      # 去线性趋势：留下"爬行残差 + 纹波"
        # 再去一次滑动平均（截止 ≈ 1/(w·dt) Hz）：只留**纹波**。停下来后"缓慢爬"（stick-slip 的
        # 爬段）是单调的、不含纹波，两个口径分开报才不会把爬行的 p-p 当成振荡。
        w = max(3, args.osc_smooth | 1)
        rac = y - box_smooth(y, w)
        dur = float(t[-1] - t[0])
        zc = int(np.sum(np.diff(np.signbit(rac - rac.mean()))))
        rows.append({"t0": float(t_s[s]), "dur": dur, "pp": float(r.max() - r.min()),
                     "ppac": float(rac.max() - rac.min()), "sd": float(rac.std()), "zc": zc,
                     "f": zc / 2.0 / dur if dur > 0 else 0.0,
                     "net": float(y[-1] - y[0]), "ri": float(np.median(cur[a:b])),
                     "pos_end": float(y[-1])})
    if not rows:
        print("\n--- 停顿期振荡检测：没有可用的停顿窗 ---")
        return
    pp = np.array([r["pp"] for r in rows])
    pa = np.array([r["ppac"] for r in rows])
    sd = np.array([r["sd"] for r in rows])
    fq = np.array([r["f"] for r in rows])
    print(f"\n--- 停顿期振荡检测（参考停住 → 下一发起转；{len(rows)} 个停顿窗）---")
    print(f"  做法：停顿窗内实测位置去**线性趋势**得 p-p（含爬行残差），再去**滑动平均**得"
          f"p-p_ac（只剩纹波/振荡，爬行被滤掉）；f = 纹波过零数/2/窗长。")
    print(f"    0.05 rad(电机轴) ≈ 0.004 孔 ≈ 0.21° 拨盘轴（肉眼可见的抖动大概在这个量级以上）")
    print(f"  p-p   （含爬行）：中位 {np.median(pp):.4f}  p95 {np.percentile(pp, 95):.4f}  max {pp.max():.4f} rad")
    print(f"  p-p_ac（只纹波）：中位 {np.median(pa):.4f}  p95 {np.percentile(pa, 95):.4f}  max {pa.max():.4f} rad"
          f"；纹波 std：中位 {np.median(sd):.4f}  max {sd.max():.4f} rad")
    for thr in (0.05, 0.10, 0.20):
        m = pa >= thr
        print(f"    p-p_ac ≥ {thr:.2f} rad：{int(m.sum())}/{len(rows)} 个停顿窗"
              f"（其中 f ≥ 10 Hz 的 {int((fq[m] >= 10.0).sum())} 个）= 真·位置环振荡")
    print(f"  {'t0(s)':>8} {'停顿ms':>7} {'p-p':>8} {'p-p_ac':>8} {'过零':>5} {'f(Hz)':>6} "
          f"{'净位移':>8} {'静息I(A)':>8}")
    for i in np.argsort(-pa)[:args.osc_max_print]:
        r = rows[i]
        print(f"  {r['t0']:8.3f} {r['dur'] * 1e3:7.0f} {r['pp']:8.4f} {r['ppac']:8.4f} {r['zc']:5d} "
              f"{r['f']:6.1f} {r['net']:8.4f} {r['ri']:8.2f}")
    hole_rad = args.hole_rad if args.hole_rad else step_rad
    pe = np.array([r["pos_end"] for r in rows])
    ph = ((pe - pe[0]) / hole_rad) % 1.0
    print(f"\n  按 停顿末位置 mod 1 孔 分 4 箱（步距 {step_rad / hole_rad:.3f} 孔）：")
    print(f"  {'相位(孔)':>9} {'窗数':>5} {'p-p_ac中位':>11} {'p-p_ac max':>11} {'≥0.05':>7} {'≥0.20':>7}")
    for q in range(4):
        m = np.abs(((ph - q * 0.25 + 0.5) % 1.0) - 0.5) < 0.10
        if m.sum() == 0:
            continue
        print(f"  {q * 0.25:9.2f} {int(m.sum()):5d} {np.median(pa[m]):11.4f} {pa[m].max():11.4f} "
              f"{int((pa[m] >= 0.05).sum()):3d}/{int(m.sum()):<3d} {int((pa[m] >= 0.20).sum()):3d}/{int(m.sum()):<3d}")
    print("  ⇒ 纹波只在某相位大（其余基本没有）⇒ 那个相位有机械干涉（相位差问题）；"
          "四箱都有、量级相当 ⇒ 是「停住」这个状态本身的极限环（间隙/静摩擦），与停在哪一相无关。")


def print_load_torque(df: pd.DataFrame, t_s: np.ndarray, step_rad: float, args,
                      dip_t: np.ndarray = np.empty(0)) -> None:
    """静息电流是「顶住**负载**」还是「顶在**硬限位**」—— 把负载力矩、静息力矩与电机能力放到
    同一条轴上比大小，免得把「被弹链/弹丸挡住」误判成「卡在单发限位」。

    拨盘停住时位置环仍灌着一份电流（实测静息 3~7 A）。它有两个来源，量级不同、对策也不同：
      · **负载**（拨齿压着弹链/弹丸）：负载力矩有确定量级 —— 把弹推过去要多大力就是多大力，
        电机只要还有裕量，机构就会**继续慢慢爬**；停位相位是被负载拖住的，不是不能动。
      · **硬限位**（真走到头，如单发限位被夹死）：机构根本过不去，电流会一路爬到**硬限幅**
        （速度环积分饱和）、位置永远差一截 ⇒ 相位被机械夹死，扫相位/调 PID 全白搭。
    本项给三把尺子，全在停住段取（α≈0、v≈0 ⇒ 电流 = **纯静负载**，不被惯性污染）：
      1) 静息 |I| 的**量级**与它随相位的**起伏**（=「拨盘变化的负载力矩」的实测形态）；
      2) 停住段位置还在不在**爬**（Δpos）：还在爬 ⇒ 电机有余量把负载推过去 ⇒ 是负载不是墙；
      3) 离**电机能力** T_max = TRIGGER_I_MAX 还有多少裕量。
    ⚠ **运动段电流不可当负载用**：0.25 孔行程够不到 vmax、全程加速-减速（没有巡航段），
      电流里混着 J·α 且**相位与轨迹时间锁死**（每发都从同一相位起、走同一条剖面）⇒ 负载与
      惯性在这份日志里**不可分离**。故本项只用停住段下结论，运动段峰值仅列作**上界**对照。
    """
    pos = df["pos"].to_numpy()
    ref = df["ref_pos"].to_numpy()
    acur = np.abs(df["cur"].to_numpy())  # CH8 实测电流 (A，无极性)
    ref_acc = df["ref_acc"].to_numpy()   # CH7 参考加速度 (rad/s²)，用于挑「轨迹顶点拍」(α≈0)
    n = len(pos)
    groups, _ = motion_groups(df)
    if not groups:
        print("\n--- 负载力矩 vs 静息力矩：本日志没有运动段，跳过 ---")
        return
    hole_rad = args.hole_rad if args.hole_rad else step_rad
    i_max = float(args.i_max)
    p0 = float(pos[0])
    pha = ((pos - p0) / hole_rad) % 1.0  # 逐拍机械相位（mod 1 孔）

    # ---- 逐停顿窗（参考已停 → 下一发起转；α≈0、v≈0）----
    stops = []
    for gi in range(len(groups) - 1):
        e, nx = groups[gi][1], groups[gi + 1][0]
        if nx - e < args.osc_min_n:
            continue
        w = slice(e, nx)
        stops.append(
            {
                "t0": float(t_s[e]),
                "dur": float(t_s[nx - 1] - t_s[e]),
                "i_med": float(np.median(acur[w])),
                "i_max": float(np.max(acur[w])),
                "i_end": float(acur[nx - 1]),  # 停住窗末拍的电流（此时最接近静力平衡）
                "err0": float(abs(ref[e] - pos[e])),  # 参考停住那一拍的残留位置误差
                # 停住段的净位移：正 = 机构在**继续爬**（把负载推过去），≈0 = 冻住不动
                "dpos": float(pos[nx - 1] - pos[e]),
                "derr": float(abs(ref[e] - pos[e]) - abs(ref[nx - 1] - pos[nx - 1])),
                "err_end": float(abs(ref[nx - 1] - pos[nx - 1])),
                "ph": float(pha[e]),
            }
        )
    if not stops:
        print("\n--- 负载力矩 vs 静息力矩：没有可用的停顿窗，跳过 ---")
        return

    im = np.array([s["i_med"] for s in stops])
    ix = np.array([s["i_max"] for s in stops])
    dp = np.array([s["dpos"] for s in stops])
    de = np.array([s["derr"] for s in stops])
    ee = np.array([s["err_end"] for s in stops])
    ph = np.array([s["ph"] for s in stops])
    crawl = np.abs(dp) > args.load_creep_rad
    mot_pk = np.array([float(np.max(acur[s:e])) for s, e in groups])
    mot_md = np.array([float(np.median(acur[s:e])) for s, e in groups])

    print(f"\n--- 负载力矩 vs 静息力矩（辨「顶住负载」还是「顶在硬限位」；{len(stops)} 个停顿窗）---")
    print(f"  取法：停住段 α≈0、v≈0 ⇒ |CH8| 就是**纯静负载**（无惯性污染）。电机能力 T_max = {i_max:.1f} A"
          f"（速度环输出限幅 TRIGGER_I_MAX）；真卡死切断阈 TRIGGER_STUCK_CUT_A=19 A。")
    print(f"  参考量（app_shoot_def.h 实测整定）：动摩擦前馈 0.24 A / 加弹突破前馈 6.0 A"
          f"（'加弹需 ≈6 A 才拨得动'）")
    print(f"\n  静息 |I|：中位 {np.median(im):.2f} A  p5 {np.percentile(im, 5):.2f}  "
          f"p95 {np.percentile(im, 95):.2f}  max {im.max():.2f} A  （= 能力的 {im.max() / i_max * 100:.0f}%）")
    nm = int(crawl.sum())
    print(f"  停住段**还在爬**（|Δpos| > {args.load_creep_rad:.3f} rad）的窗：{nm}/{len(stops)}"
          f"（占 {nm / len(stops) * 100:.0f}%）")
    if nm:
        print(f"    这些窗的 Δpos：中位 {np.median(dp[crawl]):+.3f} rad  {np.min(dp[crawl]):+.3f} ~ {np.max(dp[crawl]):+.3f}"
              f"；误差在停住段减小：中位 {np.median(de[crawl]):.3f} rad")
    nf = len(stops) - nm
    if nf:
        print(f"    冻住不动的窗 {nf} 个：静息 |I| 中位 {np.median(im[~crawl]):.2f} A  "
              f"max {im[~crawl].max():.2f} A；停住末端残余 |err| 中位 {np.median(ee[~crawl]):.3f} rad")
    # 停住窗**末端**的残余误差 = 有没有东西**持续顶着**拨盘：软限位只挡弹丸不挡拨盘 ⇒ 应当收敛到 0。
    # 若这里普遍 >0，说明拨盘被**静力**顶住了（硬限位/夹弹），才谈得上"限位保持力"。
    print(f"  停住窗**末端残余 |err|**：中位 {np.median(ee):.3f} rad  p95 {np.percentile(ee, 95):.3f}  "
          f"max {ee.max():.3f} rad（等效静载 = 5×err ⇒ 中位 {args.angle_kp * args.speed_kp * np.median(ee):.2f} A，"
          f"max {args.angle_kp * args.speed_kp * ee.max():.2f} A）")
    print(f"    ⇒ 收敛到 ≈0 ⇒ 停住后**没有任何东西继续顶住拨盘**（软限位只会挡弹丸、不会挡住拨盘）；"
          f"普遍 >0 才是「被静力顶住」的证据")
    print(f"  运动段 |I|（含惯性，仅作**上界**对照，不可当负载）：逐发峰值 中位 {np.median(mot_pk):.2f} A  "
          f"max {mot_pk.max():.2f} A；逐发中位 中位 {np.median(mot_md):.2f} A")

    # ---- 静息电流到底在量什么：它 ≈ kp_angle×kp_speed × 残留位置误差 ----
    # 停住段 drive=0、前馈=0、vfb≈0 ⇒ 位置环 P（kp=25）给速度设定值、速度环 P（kp=0.2）给电流，
    # 于是 |I| ≈ (TRIGGER_ANGLE_KP×TRIGGER_SPEED_KP)×err0 = 5×err0。**这条关系说明停住段的
    # 静息电流不是"负载力矩的直接读数"，而是"这一发被负载拖掉了多少行程"的间接指标** ——
    # 用它当负载只能横向比（同一次实验里 a 比 b），不能当绝对力矩。数据一验便知：
    e0 = np.array([s["err0"] for s in stops])
    ratio = np.divide(im, e0, out=np.zeros_like(im), where=e0 > 1e-6)
    good = e0 > 0.05
    if good.sum() >= 3:
        kfit = float(np.sum(im[good] * e0[good]) / np.sum(e0[good] * e0[good]))
        r = float(np.corrcoef(im[good], e0[good])[0, 1])
        print(f"  静息 |I| 与停顿起点残留误差 err0 的关系：|I| ≈ {kfit:.2f}×err0（相关 r = {r:.3f}，"
              f"n={int(good.sum())}）")
        print(f"    ⇒ 预测的 kp_angle×kp_speed = {args.angle_kp * args.speed_kp:.2f}"
              f"（当前 25×0.2 = 5）—— 吻合则说明**停住段的静息电流主要是位置环 P 对残留误差的响应**，"
              f"不是负载力矩的直接读数（只能横向比、不能当绝对力矩）。")

    # ---- 静力平衡读负载：停住窗**末拍**电流与残余误差同时满足 I = k·err（drive=0 ⇒ 只有 P 在顶）
    #      反演出的「等效负载电流」才是能拿去和软限位增量比大小的绝对量 ----
    ie = np.array([s["i_end"] for s in stops])
    gq = ee > 0.05
    if gq.sum() >= 3:
        kq = float(np.sum(ie[gq] * ee[gq]) / np.sum(ee[gq] * ee[gq]))
        rq = float(np.corrcoef(ie[gq], ee[gq])[0, 1])
        print(f"  平衡点标定（停住窗**末拍** 电流 vs 残余误差 err_end）：I ≈ {kq:.2f}×err（r = {rq:.3f}，"
              f"n={int(gq.sum())}）")
        L = kq * ee  # 每个停顿窗的「等效负载电流」
        print(f"    ⇒ 由 err_end 反演的**静负载量级**：中位 {np.median(L):.2f} A  "
              f"p5 {np.percentile(L, 5):.2f}  p95 {np.percentile(L, 95):.2f}  max {L.max():.2f} A")
        print(f"    ⇒ **负载自然起伏**（各窗之间的散差）：std {L.std():.2f} A、"
              f"p95−p5 {np.percentile(L, 95) - np.percentile(L, 5):.2f} A、极差 {L.max() - L.min():.2f} A")

    print(f"\n  按 停位相位 mod 1 孔 分 4 箱（步距 {step_rad / hole_rad:.3f} 孔/发）——"
          f" 静息 |I| 随相位的**起伏**就是「拨盘变化的负载力矩」：")
    print(f"  {'相位(孔)':>9} {'窗数':>5} {'静息I中位':>10} {'静息I max':>10} {'还在爬':>7} {'Δpos中位':>10}"
          f" {'末端残余err':>12} {'静载(A)=5×err':>12}")
    for q in range(4):
        m = np.abs(((ph - q * 0.25 + 0.5) % 1.0) - 0.5) < 0.10
        if m.sum() == 0:
            continue
        # 末端残余误差 × 位置环增益 = 该相位停住后**残留的等效静负载电流**（收敛到 0 ⇒ 没顶住东西）
        print(f"  {q * 0.25:9.2f} {int(m.sum()):5d} {np.median(im[m]):10.2f} {im[m].max():10.2f} "
              f"{int(crawl[m].sum()):3d}/{int(m.sum()):<3d} {np.median(dp[m]):+10.3f} "
              f"{np.median(ee[m]):12.3f} {args.angle_kp * args.speed_kp * np.median(ee[m]):10.2f}")

    # ---- 软限位增量 vs 负载自然起伏：把「停顿窗静息 |I|」按停位相位分箱后，
    #      ① 箱**内**散差 = 负载的自然起伏（同一机械相位下的重复性）；
    #      ② 箱**间**极差 = 相位相关的系统性起伏（可能含软限位那份保持力）。
    #      只有 ② 明显盖过 ① 时，才能说「电流里认得出软限位」，否则两者混在一起、分不开。----
    bins = [(q, np.abs(((ph - q * 0.25 + 0.5) % 1.0) - 0.5) < 0.10) for q in range(4)]
    bins = [(q, b) for q, b in bins if int(b.sum()) >= 3]
    if len(bins) >= 2:
        bl = [float(np.median(im[b])) for _, b in bins]
        bs = np.array([float(im[b].std()) for _, b in bins])
        bm = np.array(bl)
        within = float(np.mean(bs))          # ① 箱内散差（负载自然起伏）
        between = float(bm.max() - bm.min())  # ② 箱间极差（相位相关起伏）
        qmax = int(np.argmax(bm))
        others = np.concatenate([im[b] for q, b in bins if q != bins[qmax][0]])
        incr = float(bm[qmax] - np.median(others))  # 最重那个相位相对其余相位的增量
        print(f"  ① 箱**内**散差（同相位重复性 = **负载自然起伏**）：std 中位 {np.median(bs):.2f} A"
              f"（各箱 {bs.min():.2f}~{bs.max():.2f}）")
        print(f"  ② 箱**间**极差（相位相关起伏，**含软限位那份保持力**）：{between:.2f} A"
              f"（最重相位 {bins[qmax][0] * 0.25:.2f} 孔）")
        print(f"  ③ 最重相位相对其余相位的增量：{incr:+.2f} A  = {incr / max(within, 1e-9):.1f}×箱内散差")
        if incr > 3 * max(within, 1e-9):
            print(f"    ⇒ **增量 ≫ 自然起伏** ⇒ 软限位在电流里**认得出**，可以当「压在限位上」的标签")
        else:
            print(f"    ⇒ **增量与自然起伏同量级** ⇒ 电流里分不出「压在软限位上」和「普通负载变化」；"
                  f"用它判限位会误判")

    # ---- 去惯性的负载曲线 L(φ)：运动段里 |参考加速度| 最小的那些拍 ----
    # 0.25 孔行程是「加速→减速」三角剖面，顶点处 α≈0 ⇒ **那一拍的 CH8 就是纯负载**
    # （惯性项最小、只剩粘性摩擦）。顶点相位 = 起转相位 + 半个行程，63 发正好铺出
    # 0.125 / 0.375 / 0.625 / 0.875 四组 —— 与停住段的 0 / 0.25 / 0.5 / 0.75 四组**错开**，
    # 合起来就是一条 8 点的「负载力矩 vs 机械相位」曲线。
    mv = np.zeros(n, dtype=bool)
    for _s, _e in groups:
        mv[_s:_e] = True
    ac_abs = np.abs(ref_acc)
    thr = args.load_acc_frac * float(ac_abs.max()) if ac_abs.size else 0.0
    apex = mv & (ac_abs <= thr)
    if apex.sum() >= 8:
        pa_h = pha[apex]
        ca = acur[apex]
        print(f"\n  去惯性负载曲线 L(φ)（运动段 |ref_acc| ≤ {thr:.0f} rad/s² 的拍照，α≈0 ⇒ CH8≈纯负载）：")
        print(f"  {'相位(孔)':>9} {'样本':>6} {'中位A':>7} {'p5':>6} {'p95':>6} {'std':>6}")
        Lb = []
        for q in range(4):
            m = np.abs(((pa_h - (q * 0.25 + 0.125) + 0.5) % 1.0) - 0.5) < 0.09
            if int(m.sum()) < 3:
                continue
            c = ca[m]
            Lb.append((q * 0.25 + 0.125, float(np.median(c))))
            print(f"  {q * 0.25 + 0.125:9.3f} {int(m.sum()):6d} {np.median(c):7.2f} "
                  f"{np.percentile(c, 5):6.2f} {np.percentile(c, 95):6.2f} {c.std():6.2f}")
        if Lb:
            ls = np.array([v for _, v in Lb])
            print(f"    ⇒ 顶点负载在 {len(Lb)} 个相位点间：{ls.min():.2f} ~ {ls.max():.2f} A，"
                  f"极差 {ls.max() - ls.min():.2f} A（= 负载随相位的起伏）")

    # ---- 限位增量 vs 负载起伏：软限位只是**挡着弹丸不让滚出去**，它带来的增量要压在负载的
    #      自然起伏之上才认得出来。这里用「运动段峰值 |I|」按**有没有过弹事件**分开比：
    #      过弹 ⇒ 拨齿把弹**强推过了软限位**（那份保持力全落在电机上）；没过弹 ⇒ 没推过去。
    #      ⚠ 峰值含惯性，只看**两组之间的差**（惯性项大体相同），不看绝对值。----
    ev_cnt = np.array([int(np.sum((dip_t >= t_s[s]) & (dip_t < t_s[e] + args.corr_after)))
                       for s, e in groups])
    fired = ev_cnt > 0
    if dip_t.size and 2 <= int(fired.sum()) < len(groups):
        pf, pn = mot_pk[fired], mot_pk[~fired]
        print(f"\n  过弹 vs 未过弹（运动段峰值 |I|，含惯性；n={int(fired.sum())}/{len(groups)}）：")
        print(f"    过弹  |I|max：中位 {np.median(pf):.2f} A  p95 {np.percentile(pf, 95):.2f}  max {pf.max():.2f}")
        print(f"    未过弹|I|max：中位 {np.median(pn):.2f} A  p95 {np.percentile(pn, 95):.2f}  max {pn.max():.2f}")
        d = float(np.median(pf) - np.median(pn))
        print(f"    ⇒ 「强推过软限位」比「没推过去」多花的峰值电流 ≈ {d:+.2f} A")
        print(f"    参考：**未过弹组内部**的散差（= 负载自然起伏）std {pn.std():.2f} A、"
              f"p95−p5 {np.percentile(pn, 95) - np.percentile(pn, 5):.2f} A")
        verdict = ("增量 ≫ 起伏 ⇒ 限位在电流里**认得出**（可用电流当「压在限位上」的标签）"
                   if abs(d) > 3 * pn.std() else
                   "增量与起伏**同量级** ⇒ 电流里分不出「压在限位上」和「普通负载变化」")
        print(f"    ⇒ 判据：限位增量 {d:+.2f} A vs 负载起伏 {pn.std():.2f} A —— {verdict}")

    print(f"\n  逐窗 [t0(s)/停位孔/静息|I|/峰|I|/Δpos/Δerr]：")
    for s in stops:
        print(f"    {s['t0']:8.3f} {s['ph']:5.2f} {s['i_med']:6.2f} {s['i_max']:6.2f} "
              f"{s['dpos']:+8.3f} {s['derr']:+7.3f}")

    # ---- 判据 ----
    med, pk = float(np.median(im)), float(im.max())
    print(f"\n  ⇒ 判据：")
    print(f"    · 静息 |I| 中位 {med:.2f} A = T_max 的 {med / i_max * 100:.0f}%"
          f"、最大 {pk:.2f} A = {pk / i_max * 100:.0f}% —— "
          f"{'离硬限幅（积分饱和）还远 ⇒ 电流是被**负载**限住的，不是顶到墙' if pk < 0.7 * i_max else '⚠ 已逼近硬限幅 ⇒ 可能真顶在墙/卡死'}")
    print(f"    · 停住段 {nm}/{len(stops)} 个窗位置**仍在爬** ⇒ 电机还有裕量把负载推过去"
          f"（'冻死不动'才是硬限位/真卡死的签名）")
    print(f"    · 静息 |I| 在四箱间的**起伏** = 负载力矩随相位的变化幅度；起伏大 ⇒ 停位相位真的被"
          f"负载/机构状态调制")


def print_pitch_chain(rot: np.ndarray, depth: np.ndarray, p0: float, step_rad: float) -> None:
    """**隔孔检验（对齿距误差免疫）**：只用相邻事件的整孔步数接链，不做位置分箱。

    分箱（--per-hole）对齿距精度要求太高：齿距差 0.5% 就让箱边界在 140 个孔里漂掉大半格，
    能把"隔孔"抹平、也能凭空造出隔孔 —— 实测同一份数据换个齿距，2 孔周期分量在 0.1%~21% 之间跳。
    这里改用**事件链**：相邻两段事件之间的整孔数 n = round(Δrot/齿距) 每步独立取整，2% 的齿距
    误差也不会让 n 出 1；沿链把 (n 的奇偶) 累积 XOR，就得到每段事件所在孔的**相对奇偶**，
    与齿距误差无关。然后比较两种奇偶下的事件深度：
      - 差异显著 ⇒ 12 个孔不是等价的，机构本身带 2 孔周期；
      - 差异 ≈ 0 ⇒ 孔与孔等价，单发的隔发交替只能来自"停在哪一格"这个自由度。
    """
    if rot.size < 20:
        return
    st = np.diff(rot)
    ok = st > 1e-3
    n = np.round(st[ok] / p0).astype(np.int64)
    dep = depth[1:][ok]                     # 后一段事件的深度
    par = np.cumsum(n % 2) % 2              # 相对第一个事件的孔位奇偶
    e, o = dep[par == 0], dep[par == 1]
    odd_step = float(np.mean(n % 2 == 1))
    print(f"  隔孔检验（事件链接法，与齿距误差无关）：{n.size} 步，其中 {odd_step * 100:.0f}% 是"
          f"奇数孔步（1 格/3 格…，会翻转奇偶）、{100 - odd_step * 100:.0f}% 是偶数孔步（2 格，漏检）")
    if e.size < 5 or o.size < 5:
        print("    两种奇偶的样本不足 → 跳过")
        return
    de = float(e.mean() - o.mean())
    se = float(np.sqrt(e.var(ddof=1) / e.size + o.var(ddof=1) / o.size))
    print(f"    奇偶两组凹陷：{e.mean():.3f}±{e.std(ddof=1) / np.sqrt(e.size):.3f}（n={e.size}） vs "
          f"{o.mean():.3f}±{o.std(ddof=1) / np.sqrt(o.size):.3f}（n={o.size}）"
          f" → 差 {de:+.3f} rad/s = {abs(de) / abs(dep.mean()) * 100:.1f}% 均值，t = {de / se:+.1f}")
    print(f"    {'⚠ 显著（|t|>3）⇒ 机构自带 2 孔周期，孔的载荷不等价' if abs(de / se) > 3 else '不显著 ⇒ 两种奇偶的载荷统计上等价，看不到隔孔结构'}")


def print_bullet_pitch(df: pd.DataFrame, t_s: np.ndarray, step_rad: float,
                       dip_t: np.ndarray, args, dip_depth: np.ndarray | None = None,
                       coarse: float | None = None) -> None:
    """连发段里**每颗弹对应的电机转角** —— 反推机构真实齿距 / 每圈孔数。

    连发时拨盘不停（一条长轨迹），出弹只由"过弹窗口对齐"决定，所以**相邻两段过弹事件之间
    转过的角度恒等于机构齿距**（只有漏检才会出现 2×、3× 的倍数）。于是：
        齿距 = 相邻事件转角分布里的**最小簇**中位
        每圈孔数 = 2π × 齿轮比 / 齿距
    ⚠ 单发**量不出干净齿距**：拨盘要在规划格点上停住，齿距与规划步长不匹配的表现会被"停在
    哪一格"再调制一次（本工程实测单发每发出弹 ~0.5、连发 ~0.83，两者不同本身就是证据：
    **停在哪一格在参与决定出不出弹**）。所以本项只吃连续转动段。
    """
    st = df["state"].astype(int).to_numpy()
    pos = df["pos"].to_numpy()
    cont = np.isin(st, CONTINUOUS_STATES)
    if not cont.any():
        print("\n--- 出弹齿距（连发段）---\n  本日志没有连续转动段 → 量不出齿距（单发段停住的位置会二次调制，已放弃）")
        return
    t0, t1 = float(t_s[cont][0]), float(t_s[cont][-1])
    win = (dip_t >= t0) & (dip_t <= t1)
    dep = (dip_depth[win] if dip_depth is not None and dip_depth.size == dip_t.size
           else np.zeros(int(win.sum())))
    ev = dip_t[win]
    if ev.size < 4:
        print(f"\n--- 出弹齿距（连发段 {t0:.2f}~{t1:.2f}s）---\n  段内只有 {ev.size} 段过弹事件 → 样本太少")
        return
    k = np.searchsorted(t_s, ev)
    k = np.clip(k, 0, len(pos) - 1)
    rot = pos[k]                      # 每个过弹事件时刻的拨盘位置（电机轴 rad）
    d = np.diff(rot)
    d = d[d > 1e-3]                   # 只留正向转角（连发应恒正向）
    p = np.percentile(d, 25)
    sel = d[(d > 0.5 * p) & (d < 1.6 * p)]      # 最小簇 = 一次漏检都没发生的那些区间
    pitch = float(np.median(sel))
    holes = 2.0 * np.pi * args.gear_ratio / pitch
    print(f"\n--- 出弹齿距（连发段 {t0:.2f}~{t1:.2f}s，n={ev.size} 段事件）---")
    print(f"  段内总行程 {rot[-1] - rot[0]:.1f} rad = {(rot[-1] - rot[0]) / step_rad:.2f} 发；"
          f"平均每颗弹 {d.mean():.3f} rad = {d.mean() / step_rad:.4f} 发")
    print(f"  相邻事件转角 **最小簇**（n={sel.size}）：中位 {pitch:.3f} rad = "
          f"{pitch / step_rad:.4f} 发 = {pitch / args.gear_ratio / np.pi * 180:.2f}°（拨盘轴）")
    print(f"  ⇒ 每圈孔数 = 2π×{args.gear_ratio:g} / {pitch:.3f} = **{holes:.3f}**"
          f"（当前 robot_def.h 的 TRIGGER_PADDLE_HOLES = {args.paddle_holes}）")
    if abs(holes - args.paddle_holes) > 0.05 * args.paddle_holes:
        print(f"  ⚠ 与代码假设的 {args.paddle_holes} 孔不符：每颗弹实际要转 "
              f"{pitch / step_rad:.4f} 发，而规划器每发只给 1 发 ⇒ 拨盘位置与【该出弹的孔位】"
              f"以 {pitch / step_rad:.4f} 发为周期逐发错开")
    # 转角直方图：看最小簇是不是干净的一根柱子（有一根独立柱 = 每次出弹都转过同样角度）
    print("  相邻事件转角分布（发）：")
    bins = np.arange(0.4, 3.6, 0.1)
    h, _ = np.histogram(d / step_rad, bins=bins)
    for i, c in enumerate(h):
        if c:
            print(f"    {bins[i]:.1f}~{bins[i + 1]:.1f} 发 |{'#' * min(int(c), 60):<60}| {c}")
    print(f"  平均/齿距 = {d.mean() / pitch:.3f}（≈1.00 = 无漏检；≈1.5/2.0 = 一半事件漏检）")
    # ---- 栅格回归：把每个事件贴到"第 m 颗弹"的整齿距栅格上再最小二乘拟合斜率 ----
    # 比"相邻转角的最小簇"强得多：漏检只是跳过整号（m 不连续），不污染斜率；140 个事件的杠杆
    # 长度是 140×齿距，事件时刻抖动 0.5 rad 也只带来 ~0.01% 的斜率误差。
    # ⚠ 起点必须用**折叠给出的相干齿距**：用上面那个"最小簇中位"（13.7，被漏检带偏 2.3%）当起点，
    #   贴号会在后半段整格滑掉、回归只是把起点复述一遍（实测就是这样）。折叠峰精度 ~0.1%，
    #   140 个齿距也只漂 0.14 格，贴号才靠得住。
    cont = np.isin(df["state"].astype(int).to_numpy(), CONTINUOUS_STATES)
    p0 = coarse
    if p0 is None:
        pos_c = df["pos"].to_numpy()[cont]
        sig_c = np.abs(df["cur"].to_numpy()[cont]).astype(np.float64)
        sig_c = sig_c - float(np.median(sig_c))
        lo, hi, pstep = args.pitch_fold_range
        if hi <= 0:
            lo, hi = 0.8 * step_rad, 1.2 * step_rad
        ps = np.arange(lo, hi, pstep)
        p0 = float(ps[int(np.argmax([_fold_amp(pos_c, sig_c, p, 40)[0] for p in ps]))])
    m = np.round((rot - rot[0]) / p0).astype(np.int64)
    keep = np.r_[True, np.diff(m) > 0]          # 单调化：剔除落在同一格上的重复事件
    if keep.sum() >= 8:
        mm, rr = m[keep].astype(np.float64), rot[keep]
        empty = int(mm.max() - mm.min() + 1 - keep.sum())
        mm -= mm.mean()
        sl = float(np.sum(mm * (rr - rr.mean())) / np.sum(mm * mm))
        print(f"  栅格回归（起点用折叠齿距 {p0:.4f} rad；{keep.sum()} 个事件贴到第 m 颗弹的栅格，"
              f"跳过 {empty} 个空号）：齿距 = **{sl:.4f} rad** = {sl / step_rad:.4f} 发")
        rms = float(np.sqrt(np.mean((rr - rr.mean() - sl * mm) ** 2)))
        print(f"    拟合残差 rms {rms:.3f} rad（事件时刻抖动；明显大于 1 rad = 贴号有问题）")
        print(f"    → 每圈孔数 = 2π×{args.gear_ratio:g}/{sl:.4f} = {2 * np.pi * args.gear_ratio / sl:.3f}"
              f"；等效『每颗弹的步距』= {sl:.4f} rad")
        print(f"    → 代码步距 {step_rad:.4f} rad 相对实测齿距的偏差 {(sl / step_rad - 1) * 100:+.2f}%"
              f"（每 12 发相位滑 {12 * (sl / step_rad - 1) * 100:+.2f}% 个孔）")
        if dep.size == rot.size:
            print_pitch_chain(rot, dep, p0, step_rad)
        # ---- 停机相位：静止位置相对**实测孔栅格**的相位（0 = 正好停在孔上、0.5 = 停在两孔中间）----
        # 这是"停在哪一格"唯一能直接量出来的东西：把静止时的实测位置对实测齿距取模。也是唯一
        # 能在**连发日志**里看到的"停位"信息（连发段本身与 SCALE 无关，收尾才落在规划格点上）。
        vel = df["vel"].to_numpy()
        rest = (np.abs(vel) < 1.0) & (t_s > t1)
        if rest.sum() > 20 or (np.abs(vel) < 1.0)[:200].sum() > 20:
            segs = [("收尾后", (np.abs(vel) < 1.0) & (t_s > t1)),
                    ("起转前", (np.abs(vel) < 1.0) & (t_s < t0))]
            ph_stop = {}
            for nm, m in segs:
                if m.sum() < 20:
                    continue
                rp = float(np.median(pos[m]))
                ph = float(((rp - rot[0]) / p0) % 1.0)
                units = (rp - rot[0]) / p0
                ph_stop[nm] = ph
                print(f"    停机相位（{nm}静止 {m.sum()} 拍）：实测位置 距第一颗弹 {units:.2f} 个齿距 → "
                      f"**相位 {ph:.3f} 个齿距**（0/1 = 停在孔上，0.5 = 停在两孔正中）")
            # ---- 过弹相位 vs 停机相位 = 「规划格点 vs 机构过弹点」的直接度量 ----
            # 两者都在同一编码器系里对**实测齿距**取模，可以直接相减。零点取 rot[0]（第一颗弹），
            # 故过弹相位的定义值≈0（圆均值给出的是事件抖动）。要判"执行器跟踪不准"还是"规划器格点
            # 没对齐机构"，就看这个差：**格点偏差远大于跟踪误差时，锅在规划器/标定，不在 PID**。
            # ⚠ 连发段量的是"连续转过去"的出弹点；单发要在格点上**停住**，所以真正的判据是
            #   「格点相对过弹点差多少」—— 差 0 或 0.5（正好压在限位上）才会出现 0/2 交替。
            ang = 2.0 * np.pi * ((rot - rot[0]) / p0)
            sn, cs = float(np.sin(ang).mean()), float(np.cos(ang).mean())
            p_ej = float((np.arctan2(sn, cs) / (2.0 * np.pi)) % 1.0)
            print(f"    过弹相位（圆均值，零点=第一颗弹；合成矢量 {np.hypot(sn, cs):.3f}≈1 说明事件很集中）"
                  f" = {p_ej:.3f} 个齿距")
            for nm, ph in ph_stop.items():
                dv = (ph - p_ej) % 1.0
                dv = dv if dv <= 0.5 else dv - 1.0
                print(f"    ⇒ 【规划格点】({nm}) 相对【机构过弹点】 {dv:+.3f} 个齿距 = "
                      f"{dv * 360.0 / args.paddle_holes:+.2f}°(拨盘轴) = {dv * p0:+.2f} rad(电机轴)"
                      f"   —— 0 = 停在出弹点上、±0.5 = 停在两孔正中")


def _fold_amp(pos: np.ndarray, sig: np.ndarray, pitch: float, nbins: int) -> tuple[float, np.ndarray]:
    """把 sig 按 **(pos 对 pitch 取模)** 折叠成 nbins 个相位格，返回 (格均值的标准差, 格均值)。

    这就是周期搜寻里的 epoch folding：pitch 命中真实齿距时同相位样本叠加、脉动最深；
    pitch 偏了，几百个周期后相位漂开、脉动被抹平。**与判弹器完全无关** —— 用的是拨盘/摩擦轮
    自己的载荷脉动，所以能独立复核 --bullet-pitch 给的齿距（后者只跟踪两个摩擦轮之一、
    且受阈值与合并窗影响）。
    """
    ph = np.mod(pos - pos[0], pitch)
    b = np.minimum((ph / pitch * nbins).astype(np.int64), nbins - 1)
    cnt = np.bincount(b, minlength=nbins).astype(np.float64)
    s = np.bincount(b, weights=sig, minlength=nbins)
    ok = cnt > 0
    prof = np.zeros(nbins)
    prof[ok] = s[ok] / cnt[ok]
    return float(np.std(prof)), prof


def _fold_sources(df: pd.DataFrame, cont: np.ndarray, args) -> list[tuple[str, np.ndarray]]:
    """折叠用的两个独立信源（都只在连续转动段上取）：
      - 拨盘 |CH8|：每送一孔 = 一次推弹载荷脉冲；
      - 摩擦轮 |CH20| 凹陷：每颗弹过一次轮 = 一次转速凹陷（先做 20 ms 平滑，与判弹器同尺度）。
    """
    out = [("拨盘 |CH8|", np.abs(df["cur"].to_numpy()[cont]).astype(np.float64))]
    if "fric_vel" in df.columns:
        w = max(1, int(round(args.fric_win_ms / 1e3 / 2e-3)))
        fv = box_smooth(np.abs(df["fric_vel"].to_numpy()), w)
        base = float(np.median(fv[cont]))
        out.append(("摩擦轮 凹陷", (base - fv)[cont]))
    return out


def _half_diff(prof: np.ndarray) -> float:
    """把剖面在**同一个相位基准**下按前后两个半周期相减：>0 显著 = 隔一个孔不一样。

    这里**不能**先把脉动滚到中间再比 —— 若真周期是半个 p，滚动会把两个子周期搅在一起、
    把"隔孔不同"抹成"相同"。而两个半周期的相位基准天然一致（都是 (pos−pos0) mod 2p 的相位），
    所以直接相减才是对的。
    """
    h = prof.size // 2
    rng = float(prof.max() - prof.min())
    return float(np.max(np.abs(prof[:h] - prof[h:2 * h])) / rng) if rng > 0 else float("nan")


def print_pitch_fold(df: pd.DataFrame, t_s: np.ndarray, step_rad: float, args) -> tuple[float, np.ndarray] | None:
    """从**载荷脉动在位置域的周期**里量真实齿距（比 --bullet-pitch 高一档）。

    两个独立信源各折一次：拨盘 |CH8| 的推弹脉冲、摩擦轮 |CH20| 的过弹凹陷。两者必须以**同一个
    位置周期**重复（拨盘转一孔才推一颗弹），所以两把尺子一致 = 齿距定案。
    与 --bullet-pitch 的关键区别：**完全不经过判弹器** —— 判弹只跟踪两个摩擦轮之一、且阈值/合并窗
    都会让"相邻事件转角"的中位数系统性偏大（漏检的 2 倍区间会被"最小簇"一起收进来）。

    ⚠ 扫描**只在名义齿距附近的一个窗口内**（默认 ±20%）找相干峰，不追求全谱周期搜寻：
       对严格 p-周期信号，折叠在 2p、3p 上幅度**完全相同**（每半段都是同一段的复制），
       全谱 argmax 会到处乱跳。所以窗口是明说的假设，输出里带峰位与名义值的偏差。
    """
    st = df["state"].astype(int).to_numpy()
    cont = np.isin(st, CONTINUOUS_STATES)
    if not cont.any():
        print("\n--- 齿距折叠（位置域周期搜寻）---\n  本日志没有连续转动段 → 跳过")
        return None
    t0, t1 = float(t_s[cont][0]), float(t_s[cont][-1])
    pos = df["pos"].to_numpy()[cont]
    lo, hi, pstep = args.pitch_fold_range
    if hi <= 0:                                    # 允许 --pitch-fold-range 0 0 0 = 用名义±20%的默认窗口
        lo, hi = 0.8 * step_rad, 1.2 * step_rad
    nbins = 40
    print(f"\n--- 齿距折叠（连发段 {t0:.2f}~{t1:.2f}s，位置域周期搜寻）---")
    print(f"  扫描 {lo:.3f}~{hi:.3f} rad（步长 {pstep}）× {nbins} 相位格；名义齿距 {step_rad:.4f} rad "
          f"（{args.paddle_holes} 孔/圈 假设）= 窗口中心")
    res = []
    for name, sig in _fold_sources(df, cont, args):
        sig = sig - float(np.median(sig))          # 去直流，只留脉动
        pitches = np.arange(lo, hi, pstep)
        amps = np.array([_fold_amp(pos, sig, p, nbins)[0] for p in pitches])
        fund = float(pitches[int(np.argmax(amps))])
        pk = float(amps.max())
        prof = _fold_amp(pos, sig, fund, nbins)[1]
        holes = 2.0 * np.pi * args.gear_ratio / fund
        # 名义齿距处的相干幅度 —— 用来判"峰是不是就落在 12 孔/圈上"
        i0 = int(np.argmin(np.abs(pitches - step_rad)))
        # 隔孔调制：在 2×fund 上折叠，两个子周期直接相减（≈0 = 孔与孔等价）
        mod2 = _half_diff(_fold_amp(pos, sig, 2.0 * fund, nbins)[1])
        print(f"  [{name}] 相干峰 p = **{fund:.4f} rad**（名义的 {(fund / step_rad - 1) * 100:+.2f}%）"
              f" ⇒ 每圈 **{holes:.3f}** 孔")
        print(f"      幅度：峰 {pk:.4f}、名义齿距处 {amps[i0]:.4f}（{amps[i0] / pk * 100:.1f}% 峰）"
              f"；2×齿距折叠的半周期差 {mod2 * 100:.1f}%（**只作提示**：它同样吃齿距精度，"
              f"齿距差 0.5% 就能把它从 37% 抹到 4%；隔孔是否真实看 --bullet-pitch 的事件链检验）")
        if args.pitch_fold_curve:
            top = np.argsort(amps)[-8:][::-1]
            print("      幅度前 8 峰：" + "  ".join(f"{pitches[i]:.4f}({amps[i]:.4f})" for i in top))
        res.append((name, fund, holes, prof, mod2))
    if len(res) == 2:
        d = abs(res[0][1] - res[1][1])
        print(f"  两把尺子之差 {d:.4f} rad = {d / res[0][1] * 100:.2f}% 齿距"
              f"（<1% 即互相印证；差大了说明某一路的信源不是每孔一次）")
        dm = abs(res[0][4] - res[1][4])
        print(f"  两把尺子的隔孔调制深度之差 {dm * 100:.1f}%")
    for name, fund, holes, prof, mod2 in res:
        print(f"  [{name}] 折到 p={fund:.4f} rad 的相位剖面（每格 {fund / nbins:.3f} rad）：")
        mn, mx = float(prof.min()), float(prof.max())
        for i in range(0, nbins, 2):            # 每行两格
            row = "".join("#" if (prof[j] - mn) > 0.5 * (mx - mn) else "." for j in (i, i + 1))
            print(f"      {i * fund / nbins:7.3f} rad |{row}|  "
                  f"{prof[i]:+.4f} {prof[i + 1]:+.4f}")
    return res[0][1], pos        # (齿距, 连续段位置) —— 供 --per-hole 复用，省一次全扫描


def print_per_hole(df: pd.DataFrame, t_s: np.ndarray, step_rad: float, args,
                   pitch: float, pos: np.ndarray) -> None:
    """按真实齿距把连发段切成**逐孔**载荷序列 —— 直接看有没有『隔一个孔』的结构。

    动机（2026-10-11 用户给的机械信息）：弹链最上端靠近摩擦轮处有**单发限位**（直径小于弹丸），
    最后一段的角度随云台末端变化；若拨盘停的位置让【空格】没挡住限位，低头时最后一段弹丸会自己
    流出、超裁判系统虚拟热量。所以**正确的停位必须让空格与限位相隔整数颗弹** —— 而 3508 无绝对
    编码器，停机相位只能上电标定。若那个"正确停位集合"的周期是 2 个孔（60°），而规划器每 1 个孔
    都停一次，就会表现为 0 发/2 发交替。
    本项的判据：**拨盘在连续转动（不停）时，逐孔载荷是否等价**。
      - 逐孔载荷基本等价（偶/奇孔均值接近、lag-1 自相关 ≈0）⇒ 孔本身没差别，周期 2 只能来自
        "停在哪" ⇒ 问题在停位相位，与孔距/步距无关；
      - 逐孔载荷本身就隔孔交替 ⇒ 机构/弹链才是那个 2 周期，规划器停位只是把它暴露出来。
    """
    n = pos.size
    if args.per_hole_pitch > 0.0:
        pitch = args.per_hole_pitch          # 鲁棒性检查用：强制一个偏一点的齿距
    if n < 100 or pitch <= 0:
        print("\n--- 逐孔载荷（连发段）---\n  样本不足 → 跳过")
        return
    st = df["state"].astype(int).to_numpy()
    cont = np.isin(st, CONTINUOUS_STATES)
    k = np.floor((pos - pos[0]) / pitch).astype(np.int64)
    hmax = int(k[-1]) + 1
    steps = np.r_[np.flatnonzero(np.diff(k) > 0) + 1, n]      # 每孔第一拍的下标
    starts = np.r_[0, steps[:-1]]
    nb = len(starts) - 1
    cur = np.abs(df["cur"].to_numpy()[cont])
    w = max(1, int(round(args.fric_win_ms / 1e3 / 2e-3)))
    fvs = box_smooth(np.abs(df["fric_vel"].to_numpy()), w)[cont] if "fric_vel" in df.columns else None
    base = float(np.median(fvs)) if fvs is not None else 0.0
    per_cur = np.array([float(np.mean(cur[s:e])) for s, e in zip(starts[:nb], steps[:nb])])
    per_dip = (np.array([float(np.max(base - fvs[s:e])) for s, e in zip(starts[:nb], steps[:nb])])
               if fvs is not None else np.zeros(nb))
    # 孔号是否连续（连续转动里应当每孔都有样本；缺号 = 采样环/日志丢数据，不是机构问题）
    print(f"\n--- 逐孔载荷（连发段，按真实齿距 {pitch:.3f} rad 切孔）---")
    print(f"  切出 {nb} 个孔（孔号 0..{int(k[-1])}，期间转了 {hmax} 个齿距 = {hmax * pitch / step_rad:.1f} 发）")
    strip = "".join("░▒▓█"[min(3, int(d / max(1e-9, np.percentile(per_dip, 90)) * 4))] if d > 0 else "·"
                    for d in per_dip[:160])
    print(f"  逐孔凹陷深度（每字符 1 孔，·=没凹 ░<25% ▒<50% ▓<75% █≥75% 的 p90）：")
    print(f"    {strip}")
    ev, od = per_dip[0::2], per_dip[1::2]
    cv, co = per_cur[0::2], per_cur[1::2]
    print(f"  ⚠ 下面两个偶/奇统计**切孔相位敏感、只作诊断**（换齿距会变）："
          f"凹陷 {od.mean():.3f}/{ev.mean():.3f} rad/s，|I| {co.mean():.3f}/{cv.mean():.3f} A"
          f"（差 {abs(co.mean() - cv.mean()):.3f} A）")
    # ⚠ **不要用这里的"偶孔/奇孔"或周期谱下结论**：切孔（按齿距分箱）对齿距精度要求太高
    #   ——齿距差 0.5% 就让箱边界在 140 个孔里漂掉大半格，同一份数据换个齿距，2 孔周期分量在
    #   0.1%~21% 之间乱跳（实测）。**隔孔检验的权威口径在 --bullet-pitch 的 print_pitch_chain**：
    #   它只用相邻事件的整孔步数接链、每步独立取整，与齿距误差无关。这里保留上面的条带图只为
    #   看"哪些孔弱"的直观分布，以及下面这个**稳健**的弱孔计数（弱孔是散点、不是固定奇偶）。
    print(f"  逐孔凹陷 中位 {np.median(per_dip):.3f} rad/s  p10 {np.percentile(per_dip, 10):.3f}  "
          f"p90 {np.percentile(per_dip, 90):.3f}")
    print(f"  没凹的孔（凹陷 ≤ 0）= {(per_dip <= 0).sum()}/{nb} 个（{(per_dip <= 0).mean() * 100:.1f}%）"
          f"；凹陷 < 25% 中位的孔 = {(per_dip < 0.25 * np.median(per_dip)).sum()} 个"
          f"（这些孔基本没吃到弹）")


def main() -> None:
    ap = argparse.ArgumentParser(description="拨盘参考位置环日志分析（射频/跟踪/电流）")
    ap.add_argument("csv", type=Path, help="VOFA+ 导出的 CSV（首列时间戳 µs，次列起为 CH1..）")
    ap.add_argument("--step-rad", type=float, default=DEFAULT_STEP_RAD,
                    help=f"每发角度 TRIGGER_STEP_RAD (rad)，默认 {DEFAULT_STEP_RAD:.4f}")
    ap.add_argument("--min-run-ms", type=float, default=100.0,
                    help="段切分的最短时长 (ms)，短于此的抖动段并入相邻段，默认 100")
    ap.add_argument("--min-vel-frac", type=float, default=0.5,
                    help="稳态判据：|参考速度| ≥ 本段中位数的该比例才算'已爬到巡航'，默认 0.5")
    ap.add_argument("--i-max", type=float, default=DEFAULT_I_MAX, help="电流饱和判据 (A)")
    ap.add_argument("--angle-vmax", type=float, default=DEFAULT_ANGLE_VMAX,
                    help="参考速度限幅判据 (rad/s)")
    ap.add_argument("--dump-segments", type=Path, default=None, help="把分段表写到这个 CSV")
    ap.add_argument("--trace", type=float, nargs=2, metavar=("T0", "T1"), default=None,
                    help="回放这个时间窗（秒）：打状态/fire 跳变点与参考位置的整数发网格偏差，"
                         "用于定位'卡在某个状态不动'（如 --trace 29.0 30.5）")
    ap.add_argument("--stop-check", action="store_true",
                    help="列出每次运动（参考开始动→再次停稳）的总位移，检查终点是否落在整数发网格上")
    ap.add_argument("--drift-sim", action="store_true",
                    help="用本次日志实测的收尾超调推演「终点锚实测 vs 锚理论网格」的累计发数漂移")
    ap.add_argument("--fric-cur-delta", type=float, default=0.25,
                    help="过弹判据：平滑电流高出空转基准多少 A 算过弹，默认 0.25")
    ap.add_argument("--fric-win-ms", type=float, default=20.0,
                    help="判弹用的滑动平均窗长 (ms)，默认 20（CH20/CH21 逐拍噪声可达 ±5%%）")
    ap.add_argument("--fric-merge-ms", type=float, default=25.0,
                    help="两次命中间隔小于此值则合并为一次过弹 (ms)，默认 25")
    ap.add_argument("--fric-max-print", type=int, default=20,
                    help="摩擦轮过弹事件最多打印几段，默认 20（计数与统计始终用全量）")
    ap.add_argument("--fric-set", type=float, default=0.0,
                    help="摩擦轮设定值 (rad/s，= app_shoot_def.h 的 FRICTION_SPEED_FULL)，"
                         "给了才做'达速/未达速'判据，默认 0 = 不检查")
    ap.add_argument("--fric-i-max", type=float, default=20.0,
                    help="摩擦轮速度环电流上限 (A，= FRICTION_I_MAX)，用于判命令饱和，默认 20")
    ap.add_argument("--fric-detect", choices=("auto", "current", "speed"), default="auto",
                    help="过弹判据：current=平滑电流 / speed=平滑转速凹陷 / auto=命令饱和时自动切 speed")
    ap.add_argument("--fric-dip-frac", type=float, default=0.005,
                    help="转速判据的凹陷门限（占名义转速的比例），默认 0.005 = 0.5%%")
    ap.add_argument("--corr-before", type=float, default=0.05,
                    help="发次×过弹配对窗：运动起点前多少秒（默认 0.05）")
    ap.add_argument("--corr-after", type=float, default=0.20,
                    help="发次×过弹配对窗：运动终点后多少秒（默认 0.20）")
    ap.add_argument("--fric-trace", type=float, nargs=2, metavar=("T0", "T1"), default=None,
                    help="回放这个时间窗（秒）内摩擦轮逐拍样本，用来定标判弹门限")
    ap.add_argument("--zoom", type=float, nargs=2, metavar=("T0", "T1"), default=None,
                    help="逐拍回放拨盘控制环（ref/pos/err/vref/vfb/I/位置环输出/积分/前馈），"
                         "定位'一发没走满、靠空闲期爬'这类跟踪缺陷")
    ap.add_argument("--zoom-every", type=int, default=10,
                    help="--zoom 每几拍打一行，默认 10（2 ms 周期 → 20 ms/行）")
    ap.add_argument("--trig-sat-i", type=float, default=10.0,
                    help="拨盘电流饱和判据 (A)：C620/M3508 上限 10 A，默认 10")
    ap.add_argument("--col-scan", action="store_true",
                    help="逐通道取值扫描：找出只取极少几个值的通道（定位'某路全是 0 和 2'）")
    ap.add_argument("--net-check", action="store_true",
                    help="逐发净推进量（起转前一拍→下一次起转前一拍），区分'瞬时欠走'与'真丢发'")
    ap.add_argument("--shot-lag", action="store_true",
                    help="单发时间漏斗：参考停住瞬间走了几成 / 空闲期爬多久补齐 / 弹何时出轮"
                         "（把'发射滞后'与'相位漂移'分开量）")
    ap.add_argument("--shot-lag-tol", type=float, default=0.05,
                    help="判'已补齐'的位置误差门限（发），默认 0.05")
    ap.add_argument("--shot-lag-win", type=float, default=1.20,
                    help="找本发过弹事件的搜索窗（起转后多少秒），默认 1.20")
    ap.add_argument("--hole-rad", type=float, default=None,
                    help="一个**孔**对应的电机角度 TRIGGER_STEP_RAD (rad)，相位扫描分箱用；"
                         "默认 = --step-rad（步距=1 孔时不做相位扫描）。"
                         "做 TRIGGER_STEP_ADVANCE_SCALE<1 的相位扫描实验时：--step-rad 传步距、"
                         f"本项传 {DEFAULT_STEP_RAD:.4f}")
    ap.add_argument("--pass-short", type=float, default=0.25,
                    help="零成本标签：参考停住时缺口 ≤ 本值 (rad) 判'机构到位'（默认 0.25）")
    ap.add_argument("--pass-i", type=float, default=10.0,
                    help="零成本标签：本发 |CH8| 峰值 ≥ 本值 (A) 判'推过弹'（默认 10）")
    ap.add_argument("--osc-check", action="store_true",
                    help="逐发**停顿期**振荡检测（去线性趋势后的残差 p-p / 过零频率），"
                         "并按 停顿末位置 mod 1 孔 分箱，证实/证伪'卡在限位时位置环震荡'")
    ap.add_argument("--osc-skip", type=int, default=5,
                    help="停顿窗开头跳过的拍数（躲开轨迹刚结束的瞬态），默认 5 拍 = 10 ms")
    ap.add_argument("--osc-min-n", type=int, default=20,
                    help="停顿窗至少多少拍才纳入统计，默认 20 拍 = 40 ms")
    ap.add_argument("--osc-max-print", type=int, default=15,
                    help="按 p-p 从大到小最多打印几个停顿窗，默认 15")
    ap.add_argument("--load-torque", action="store_true",
                    help="负载力矩 vs 静息力矩 vs 电机能力：辨「停住时灌的电流」是顶住**负载**"
                         "（弹链/拨齿压弹）还是顶在**硬限位**（单发限位/真卡死）—— 用停住段"
                         "（α≈0）的 |I| 量级、位置还在不在爬、离 TRIGGER_I_MAX 的裕量三层判；"
                         "并给出「软限位增量 vs 负载自然起伏」的量级比")
    ap.add_argument("--load-acc-frac", type=float, default=0.08,
                    help="去惯性负载曲线取「轨迹顶点拍」的 |ref_acc| 门限（相对全程 max 的比例）")
    ap.add_argument("--load-creep-rad", type=float, default=0.01,
                    help="判「停住段位置还在爬」的净位移门限 (rad，电机轴)；|Δpos| 超过它算还在爬")
    ap.add_argument("--angle-kp", type=float, default=25.0, help="拨盘位置环 kp (TRIGGER_ANGLE_KP)")
    ap.add_argument("--speed-kp", type=float, default=0.2, help="拨盘速度环 kp (TRIGGER_SPEED_KP)")
    ap.add_argument("--osc-smooth", type=int, default=41,
                    help="滤掉爬行用的滑动平均窗（拍）；截止 ≈ 1/(窗·dt)，默认 41 拍 ≈ 82 ms（≈12 Hz），"
                         "即只保留 12 Hz 以上的纹波")
    ap.add_argument("--bullet-pitch", action="store_true",
                    help="连发段反推**出弹齿距 / 每圈孔数**（相邻过弹事件之间的转角 = 机构齿距；"
                         "验证 robot_def.h 的 TRIGGER_PADDLE_HOLES 是否与实机一致）")
    ap.add_argument("--gear-ratio", type=float, default=DEFAULT_GEAR_RATIO,
                    help=f"电机:拨盘 减速比（= robot_def.h 的 TRIGGER_GEAR_RATIO），默认 {DEFAULT_GEAR_RATIO:.4f}")
    ap.add_argument("--paddle-holes", type=int, default=12,
                    help="代码假设的一圈孔数（= TRIGGER_PADDLE_HOLES），只用于对照，默认 12")
    ap.add_argument("--pitch-fold", action="store_true",
                    help="连发段做**位置域周期搜寻**（载荷脉动折叠）反推真实齿距：比 --bullet-pitch "
                         "高一档，用拨盘/摩擦轮自己的脉动，不经过判弹器，可独立复核齿距与每圈孔数")
    ap.add_argument("--pitch-fold-range", type=float, nargs=3, metavar=("LO", "HI", "STEP"),
                    default=[0.0, 0.0, 0.001],
                    help="扫描区间与步长 (rad)；默认全 0 = 名义齿距 ±20%%（不追求全谱：2p/3p 上幅度与 p 全同，"
                         "全谱会乱跳），要更宽就显式给")
    ap.add_argument("--pitch-fold-curve", action="store_true", help="额外打印幅度谱前 12 峰")
    ap.add_argument("--per-hole-pitch", type=float, default=0.0,
                    help="强制 --per-hole 的切孔齿距 (rad)，默认 0 = 用折叠峰；用来做【齿距偏一点】"
                         "的鲁棒性检查（切孔漂移不该造出隔孔调制）")
    ap.add_argument("--per-hole", action="store_true",
                    help="按折叠出的真实齿距把连发段切成逐孔载荷序列：直接看有没有『隔一个孔』的结构"
                         "（空格/单发限位对齐问题在连续转动里应当表现为 2 倍齿距的周期调制）")
    args = ap.parse_args()

    df, missing = load_csv(args.csv, DEFAULT_COLS, OPTIONAL_COLS)
    ext_missing = [k for k in missing if k in EXT_COLS]
    fric_missing = [k for k in missing if k in FRIC_COLS]

    # 扩展列/摩擦轮列是否**真接了**：VOFA 固定发满 25 通道，所以列**总是存在**，判据只能是
    # "有没有非零值"。I13~I19 全零 = 2026-10-11 之前的老固件（12 通道版）→ 跳过依赖它们的分析。
    ext_present = (not ext_missing) and bool(np.abs(df[list(EXT_COLS)].to_numpy()).max() > 0)
    fric_present = (not fric_missing) and bool(np.abs(df[list(FRIC_COLS)].to_numpy()).max() > 0)

    n = len(df)
    t_all = df["time"].to_numpy() / 1e6
    dur = t_all[-1] - t_all[0]
    dt = df["dt"].to_numpy()

    print(f"=== 文件 {args.csv} ===")
    print(f"采样 {n} 点，时长 {dur:.2f} s，平均采样率 {n / dur:.1f} Hz")
    layout = "12 通道版"
    if ext_present:
        layout = "25 通道版" if fric_present else "19 通道版"
    elif fric_present:
        layout = "⚠ CH13~CH19 全零但 CH20~CH25 有数据（通道表与脚本不符，请核对）"
    print(f"布局：{layout}（有数据的最高列：CH13~CH19 {'✓' if ext_present else '✗'} / "
          f"CH20~CH25 {'✓' if fric_present else '✗'}）")
    if not ext_present:
        print("  ⚠ 扩展列无数据 —— 卡住保护/前馈补位/速度环饱和/变速积分档位 这几项分析已跳过")
    if not fric_present:
        print("  ⚠ 摩擦轮列无数据 —— 判弹丸过轮（CH20~CH25）这项分析已跳过")
    print(f"dt(ms): 均值 {dt.mean():.3f}  中位 {np.median(dt):.3f}  p95 {np.percentile(dt, 95):.3f}  "
          f"max {dt.max():.3f}  （丢帧/被拖周期看 max 与 p95 的差距）")

    # 未用列自检：VOFA 固定发 25 通道，本工程 I26 起应为恒 0（I13~I25 已各有归属）
    print("\n--- 未用列自检（I26 起应恒 0；非零说明接线与脚本不一致）---")
    unused_nonzero = []
    raw = pd.read_csv(args.csv)
    for c in list(raw.columns)[26:]:
        if raw[c].abs().max() > 0:
            unused_nonzero.append((c, float(raw[c].abs().max())))
    if unused_nonzero:
        print(f"  ⚠ 有非零未用列: {unused_nonzero[:8]}{' ...' if len(unused_nonzero) > 8 else ''}")
    else:
        print(f"  OK：I26..I{len(raw.columns) - 1} 全零" if len(raw.columns) > 26 else "  （本文件未发满 27 通道）")

    if args.col_scan:
        print_col_scan(df)

    if ext_present:
        print_control_core_checks(df, args)
    if fric_present:
        dip_t, dip_depth = print_friction_checks(df, args)
        print_feed_correlation(df, t_all, args.step_rad, dip_t, args)
        if args.shot_lag:
            print_shot_lag(df, t_all, args.step_rad, dip_t, args)
    if args.osc_check:
        print_osc_check(df, t_all, args.step_rad, args)
    if args.load_torque:
        print_load_torque(df, t_all, args.step_rad, args, dip_t if fric_present else np.empty(0))
    # 齿距折叠先跑：它给出的相干齿距是 --bullet-pitch 栅格回归的贴号起点、也是 --per-hole 的切孔尺
    # （单开 --bullet-pitch 时不打印折叠块，它会在内部自己算一次相干齿距当起点）
    fold = print_pitch_fold(df, t_all, args.step_rad, args) if (args.pitch_fold or args.per_hole) else None
    if fric_present:
        if args.bullet_pitch:
            print_bullet_pitch(df, t_all, args.step_rad, dip_t, args, dip_depth=dip_depth,
                               coarse=fold[0] if fold is not None else None)
        if args.per_hole and fold is not None:
            print_per_hole(df, t_all, args.step_rad, args, fold[0], fold[1])

    # 状态占比
    print("\n--- 状态占比 ---")
    for st, cnt in df["state"].value_counts().sort_index().items():
        name = STATE_NAMES.get(int(st), f"?{int(st)}")
        print(f"  {int(st)} {name}: {cnt:6d} 点 ({cnt / n * 100:5.1f}%)")

    # 分段：状态机状态 × fire 的连续平台
    key = df["state"].astype(int).to_numpy() * 10 + df["fire"].astype(int).to_numpy()
    segs: list[tuple[int, int]] = []
    i = 0
    while i < n:
        j = i
        while j < n and key[j] == key[i]:
            j += 1
        segs.append((i, j))
        i = j
    # 合并过短的段（抖动）
    min_pts = max(2, int(args.min_run_ms / max(dt.mean(), 1e-3)))
    merged: list[tuple[int, int]] = []
    for s, e in segs:
        if merged and (e - s) < min_pts:  # 太短 → 并入上一段（多是状态/使能的单拍抖动）
            merged[-1] = (merged[-1][0], e)
        else:
            merged.append((s, e))

    rows = [analyze_segment(df, s, e, args.step_rad) for s, e in merged]

    print("\n--- 分段表（段 = 状态机状态 × enable&&fire 的连续平台，抖动段已合并）---")
    hdr = (f"{'t0(s)':>8} {'t1(s)':>8} {'时长':>7} {'状态':>6} {'fire':>4} "
           f"{'行程(rad)':>10} {'发数':>7} {'发/s(均)':>9} {'发/s(中)':>9} "
           f"{'误差rms':>8} {'误差max':>8} {'|I|均':>7} {'|I|max':>7}")
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        print(f"{r['t0']:8.2f} {r['t1']:8.2f} {r['dur_s']:7.2f} {STATE_NAMES.get(r['state'], '?'):>6} "
              f"{r['fire']:4d} {r['travel']:10.2f} {r['shots']:7.2f} {r['rate_mean']:9.2f} "
              f"{r['rate_med']:9.2f} {r['err_rms']:8.3f} {r['err_max']:8.3f} "
              f"{r['cur_mean']:7.2f} {r['cur_max']:7.2f}")

    if args.dump_segments:
        pd.DataFrame(rows).to_csv(args.dump_segments, index=False)
        print(f"\n分段表已写入 {args.dump_segments}")

    # ---- 连续转动段：射频（整体 + 稳态） ----
    cont = [r for r in rows if r["state"] in CONTINUOUS_STATES and r["fire"] == 1 and r["dur_s"] > 0.05]
    print("\n--- 连续转动（三发/连发）：射频 ---")
    if not cont:
        print("  本日志没有连续转动段")
    else:
        print(f"  连续段 {len(cont)} 个，合计行程 "
              f"{sum(r['travel'] for r in cont):.1f} rad = {sum(r['shots'] for r in cont):.2f} 发")
        for r in cont:
            print(f"  [{r['t0']:7.2f}~{r['t1']:7.2f}s] {STATE_NAMES[r['state']]}  "
                  f"行程 {r['travel']:8.1f} rad = {r['shots']:6.2f} 发 / {r['dur_s']:6.2f} s  →  "
                  f"{r['shots'] / r['dur_s']:6.2f} 发/s = {r['shots'] / r['dur_s'] * 60:6.1f} 发/min"
                  f"（含加减速；参考速度中位 {r['ref_vel_med']:.2f} rad/s）")
        # 稳态射频：只取 |参考速度| ≥ 中位数×frac 的采样
        print("\n  稳态射频（参考速度已爬到巡航的采样，排除加速/收速滑行）：")
        for s, e in merged:
            r0 = analyze_segment(df, s, e, args.step_rad)
            if r0["state"] not in CONTINUOUS_STATES or r0["fire"] != 1:
                continue
            seg = df.iloc[s:e]
            v = np.abs(seg["ref_vel"].to_numpy())
            thr = np.median(v) * args.min_vel_frac
            m = v >= thr
            if m.sum() < 2:
                continue
            t = seg["time"].to_numpy() / 1e6
            p = seg["ref_pos"].to_numpy()
            vv = np.divide(np.diff(p), np.diff(t), out=np.zeros(len(p) - 1), where=np.diff(t) > 0)
            mm = m[:-1] & m[1:]
            if mm.sum() < 2:
                continue
            rate_ss = np.median(vv[mm]) / args.step_rad
            print(f"  [{t[0]:7.2f}~{t[-1]:7.2f}s] 稳态 {rate_ss:6.2f} 发/s = {rate_ss * 60:6.1f} 发/min "
                  f"（占段长 {mm.sum() / len(vv) * 100:.0f}%）")

    # ---- 单发段 ----
    single = [r for r in rows if r["state"] == SINGLE_STATE and r["fire"] == 1]
    print("\n--- 单发（点到点一步） ---")
    if not single:
        print("  本日志没有单发段")
    else:
        for r in single:
            print(f"  [{r['t0']:7.2f}~{r['t1']:7.2f}s] 一步耗时 {r['dur_s']:6.3f} s，"
                  f"行程 {r['travel']:6.2f} rad（{r['shots']:5.2f} 发），"
                  f"误差 rms {r['err_rms']:.3f} / max {r['err_max']:.3f} rad，"
                  f"|I| max {r['cur_max']:.2f} A，参考加速度 max {r['ref_acc_max']:.0f} rad/s²")
        print(f"  平均一步耗时 {np.mean([r['dur_s'] for r in single]):.3f} s")

    # ---- 跟踪与饱和 ----
    act = df[df["fire"] == 1]
    print("\n--- 跟踪质量（enable&&fire=1 的采样）---")
    if len(act) == 0:
        print("  本日志没有驱动段")
    else:
        err = act["err"].to_numpy()
        refv = act["ref_vel"].to_numpy()
        vel = act["vel"].to_numpy()
        cur = np.abs(act["cur"].to_numpy())
        print(f"  位置误差: rms {np.sqrt(np.mean(err**2)):.3f}  |e| 中位 {np.median(np.abs(err)):.3f}  "
              f"max {np.max(np.abs(err)):.3f} rad  （{np.max(np.abs(err)) / args.step_rad * 100:.1f}% 步长）")
        print(f"  速度: 参考中位 {np.median(refv):.2f} rad/s，实测中位 {np.median(vel):.2f} rad/s，"
              f"两者中位差 {np.median(refv - vel):.2f} rad/s（负=实测滞后）")
        print(f"  电流: |I| 均值 {cur.mean():.2f} A，中位 {np.median(cur):.2f} A，max {cur.max():.2f} A，"
              f"贴限幅(≥{args.i_max * 0.95:.1f}A) 占比 {np.mean(cur >= args.i_max * 0.95) * 100:.1f}%")
        sat_v = np.mean(np.abs(refv) >= args.angle_vmax * 0.95) * 100
        print(f"  参考速度限幅({args.angle_vmax:.0f} rad/s) 占比 {sat_v:.1f}%")

    # ---- 收速滑行段（连续段之后状态已离开 2/3，但参考仍在走）----
    tail = [r for r in rows if r["state"] not in CONTINUOUS_STATES and r["fire"] == 0
            and abs(r["travel"]) > 0.5]
    if tail:
        print("\n--- 收速滑行段（状态已离开连续态、参考仍在走）---")
        for r in tail:
            print(f"  [{r['t0']:7.2f}~{r['t1']:7.2f}s] 状态 {STATE_NAMES.get(r['state'], '?')}  "
                  f"滑行 {r['travel']:7.2f} rad = {r['shots']:.2f} 发 / {r['dur_s']:.3f} s，"
                  f"|I| max {r['cur_max']:.2f} A")
        tot = sum(r["travel"] for r in tail)
        print(f"  滑行合计 {tot:.2f} rad = {tot / args.step_rad:.2f} 发"
              f"（这是『停在哪』相对最后一份步长的额外转角，看是否可接受）")

    if args.stop_check:
        stop_check(df, t_all, args.step_rad)
        print_phase_check(df, t_all, args.step_rad)

    if args.net_check:
        net_check(df, args.step_rad)

    print_trigger_current_budget(df, args)

    if args.drift_sim:
        groups, settle = motion_groups(df)
        drift_sim(df, t_all, args.step_rad, groups, settle)

    if args.trace is not None:
        trace_window(df, t_all, args.trace[0], args.trace[1], args.step_rad)

    if args.fric_trace is not None:
        fric_trace(df, t_all, args.fric_trace[0], args.fric_trace[1])

    if args.zoom is not None:
        zoom_trace(df, t_all, args.zoom[0], args.zoom[1], max(1, args.zoom_every))


if __name__ == "__main__":
    main()
