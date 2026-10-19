#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵力控 app（app_chassis_force）：100Hz 舵向/轮向 VOFA 遥测分析

用途
----
回答"舵轮为什么会在某些时刻乱转"。脚本只读 CSV，不假设任何控制参数：

  1) 目标角/实际角/目标力矩/实测力矩的时序统计；
  2) 目标角自己转得快的时段（> --fast-thr deg/s）——"乱转"的直接证据；
  3) 四种换向组合代价几乎持平的时段（max-min < --flat-thr）——决策病态点；
  4) combo（换向方案）与 locked（小力矩锁定）的切换次数/抖动；
  5) 目标角的高频抖动（与实际角无关的那部分）——小力矩下 atan2 方向噪声；
  6) 轮向出力比例高时的跟踪误差——判断舵向 PID 本身是否正常；
  7) 轮向力矩 / 出力比例统计；
  8) 指令-反馈符号一致性、期望力大小与方向自转；
  9) yaw 自激（指令不转、车在转）→ 舵角被要求以多快频率/多大幅度摆动
     → 舵实际跟得上吗 → 力分配要求的轮向倾斜角。"乱转"的机理链。

第 8、9 节需要 28 通道日志（≥29 列）。

输入布局（app_cfg.h 的 VOFA_CHANNELS；CSV 列 k = CHk）
------------------------------------------------------
列 0       DWT 微秒时间戳
列 1~4     左/右舵 目标角、实际角 (deg)
列 5~8     左/右轮 目标力矩、实测力矩 (N·m，带符号)
列 9       换向方案 combo（bit0=左反、bit1=右反）
列 10~13   四种组合的代价 (s)
列 14      locked（1=小力矩锁定）
列 15      状态机状态（2=SAFE，3=RUN）
列 16~17   左/右驱动轮出力比例 cos(目标−实际)，带符号；旧固件是 0/1 对齐门控
列 18~20   云台下发速度指令 vx/vy/w (m/s, m/s, rad/s)      ← 17 通道日志没有
列 21~23   实测速度反馈 vx/vy/w（vx/vy=轮速正解，w=IMU）  ← 17 通道日志没有
列 24~26   期望广义力 fx/fy/mw (N, N, N·m)               ← 17 通道日志没有
列 27~28   左/右驱动轮轮速 (rad/s，轮侧，app 正方向)      ← 17 通道日志没有

≥29 列的日志会额外跑第 8、9 节。旧 18 列日志仍可分析（第 0~7 节）。

用法
----
    python3 analyze_force_steer.py ../../../../ignore/chassis_data/vofa+.csv
    python3 analyze_force_steer.py log.csv --fast-thr 100 --flat-thr 0.05

退出码：0 正常；1 输入布局不符。
"""

import argparse
import csv
import sys

import numpy as np

# 列号（0 起）
C_T = 0
C_LT, C_LA, C_RT, C_RA = 1, 2, 3, 4
C_LTT, C_LTA, C_RTT, C_RTA = 5, 6, 7, 8
C_COMBO = 9
C_COST = slice(10, 14)
C_LOCK = 14
C_STATE = 15
C_GL, C_GR = 16, 17        # 出力比例（旧固件为 0/1 门控；统一按 |值| 解释）
# 17 通道之后新增（旧日志没有这些列）
C_CMD = slice(18, 21)  # vx, vy, w
C_FB = slice(21, 24)   # vx, vy, w
C_WR = slice(24, 27)   # fx, fy, mw
C_WS = slice(27, 29)   # 左/右驱动轮轮速
N_COLS_NEW = 29        # 时间戳 + 28 通道

STATE_NAMES = {0: "CAL", 1: "FAULT", 2: "SAFE", 3: "RUN"}


def load(path):
    with open(path, newline="") as f:
        r = csv.reader(f)
        header = next(r)
        rows = [list(map(float, x)) for x in r if x and x[0].strip()]
    a = np.array(rows, dtype=np.float64)
    if a.ndim != 2 or a.shape[1] < 18:
        print(f"✗ 列数 {a.shape[1] if a.ndim == 2 else '?'} < 18（表头 {len(header)} 项），布局不符")
        sys.exit(1)
    ts = a[:, C_T]
    dts = np.diff(ts)
    if np.any(dts <= 0):
        print("✗ 时间戳非单调递增")
        sys.exit(1)
    dt = np.median(dts) * 1e-6
    return a, (ts - ts[0]) * 1e-6, dt


def unwrap_deg(x):
    return np.degrees(np.unwrap(np.radians(x)))


def wrap_err_deg(a, b):
    return ((a - b + 180.0) % 360.0) - 180.0


def clusters(idx, gap):
    """把下标数组按间隔 > gap 切成若干段，返回 [(start, end)]（end 含）"""
    if len(idx) == 0:
        return []
    out = []
    s = p = idx[0]
    for i in idx[1:]:
        if i - p > gap:
            out.append((s, p))
            s = i
        p = i
    out.append((s, p))
    return out


def section(title):
    print("\n" + "=" * 78)
    print(title)
    print("=" * 78)


def main():
    ap = argparse.ArgumentParser(description="半舵力控 app VOFA 遥测分析")
    ap.add_argument("csv", help="VOFA+ 导出的 CSV（首行表头）")
    ap.add_argument("--fast-thr", type=float, default=100.0,
                    help="目标角角速度阈值 (deg/s)，默认 100")
    ap.add_argument("--flat-thr", type=float, default=0.05,
                    help="四组合代价极差阈值 (s)，默认 0.05")
    ap.add_argument("--gap", type=float, default=0.15,
                    help="时段聚类最小间隔 (s)，默认 0.15")
    ap.add_argument("--top", type=int, default=20,
                    help="每类事件最多打印条数，默认 20")
    ap.add_argument("--big-force", type=float, default=50.0,
                    help="期望力模值告警阈值 (N)，默认 50（需要 CH18~28）")
    ap.add_argument("--osc-thr", type=float, default=0.15,
                    help="yaw 自激判定阈值 (rad/s)，指令 w≈0 时 |w_fb| 超过它算一段，默认 0.15")
    ap.add_argument("--wheel-radius", type=float, default=0.05025,
                    help="驱动轮半径 (m)，默认 0.05025（robot_def.h）")
    ap.add_argument("--wheel-y", type=float, default=0.20225,
                    help="舵轮横向半距 |y| (m)，默认 0.20225（robot_def.h）")
    args = ap.parse_args()

    a, t, dt = load(args.csv)
    n = len(t)

    section("0. 文件")
    print(f"文件      {args.csv}")
    print(f"样本      {n}，时长 {t[-1]:.2f} s，dt 中位 {dt*1e3:.1f} ms（≈{1/dt:.0f} Hz）")

    # 时间戳回绕保护：若 dt 中位异常大，提示
    if dt > 0.2:
        print(f"⚠ dt 中位 {dt*1e3:.1f} ms 偏大，确认列 0 是否为微秒时间戳")

    # ---------------- 状态 ----------------
    section("1. 状态机")
    st = a[:, C_STATE].astype(int)
    edges = np.where(np.diff(st) != 0)[0] + 1
    seg = [0, *edges.tolist(), n]
    for s, e in zip(seg[:-1], seg[1:]):
        v = st[s]
        print(f"  {STATE_NAMES.get(v, v):5s}  {t[s]:7.2f} ~ {t[e-1]:7.2f} s  ({t[e-1]-t[s]:6.2f} s)")

    # ---------------- 目标/实际角 ----------------
    Lt, La = unwrap_deg(a[:, C_LT]), unwrap_deg(a[:, C_LA])
    Rt, Ra = unwrap_deg(a[:, C_RT]), unwrap_deg(a[:, C_RA])
    rL = np.diff(Lt) / np.diff(t)
    rR = np.diff(Rt) / np.diff(t)

    section("2. 目标角：快速转动时段（“乱转”的直接证据）")
    fast = (np.abs(rL) > args.fast_thr) | (np.abs(rR) > args.fast_thr)
    eps = clusters(np.where(fast)[0], int(args.gap / dt))
    print(f"阈值 |d(target)/dt| > {args.fast_thr:.0f} deg/s，共 {len(eps)} 段")
    print("  起止(s)          时长   L目标(起→止)      R目标(起→止)     combo    lock率  低出力率")
    for s, e in eps[: args.top]:
        e1 = min(e + 1, n - 1)
        lock_rate = a[s:e + 1, C_LOCK].mean()
        weak = 0.5 * (np.mean(np.abs(a[s:e + 1, C_GL]) < 0.5) + np.mean(np.abs(a[s:e + 1, C_GR]) < 0.5))
        net = "" if abs(Lt[e1] - Lt[s]) > 20 or abs(Rt[e1] - Rt[s]) > 20 else "  ← 净位移小，是来回抖"
        print(f"  {t[s]:6.2f}-{t[e1]:6.2f}  {t[e1]-t[s]:5.2f}s  "
              f"{Lt[s]:7.1f}→{Lt[e1]:7.1f}  {Rt[s]:7.1f}→{Rt[e1]:7.1f}  "
              f"{a[s, C_COMBO]:.0f}→{a[e1, C_COMBO]:.0f}  {lock_rate:5.2f}  {weak:5.2f}{net}")

    # ---------------- 跟踪质量 ----------------
    section("3. 舵向跟踪质量（判断 PID 是否正常）")
    errL = np.abs(wrap_err_deg(a[:, C_LT], a[:, C_LA]))
    errR = np.abs(wrap_err_deg(a[:, C_RT], a[:, C_RA]))
    on = (np.abs(a[:, C_GL]) > 0.5) & (np.abs(a[:, C_GR]) > 0.5)
    if on.sum() > 10:
        print(f"两轮出力比例都高 |align|>0.5（{on.sum()} 样本，{100*on.mean():.1f}%）：")
        for nm, e in (("左", errL), ("右", errR)):
            print(f"  {nm}舵 |目标−实际|：p50 {np.percentile(e[on],50):5.2f}  "
                  f"p95 {np.percentile(e[on],95):5.2f}  p99 {np.percentile(e[on],99):5.2f}  "
                  f"max {e[on].max():6.2f} deg")
    else:
        print("两轮出力比例同时高的样本太少，跳过")
    rLa, rRa = np.diff(La) / np.diff(t), np.diff(Ra) / np.diff(t)
    # 稳态：目标角 100ms 内几乎不动
    k = max(3, int(round(0.1 / dt)))
    stable = np.ones(n, dtype=bool)
    for i in range(n - k):
        stable[i] = (Lt[i + k] - Lt[i]) ** 2 + (Rt[i + k] - Rt[i]) ** 2 < 25.0  # 两轮合计 <5deg
    stable[-k:] = False
    ss = stable[:-1]
    print(f"目标角稳定的样本（{ss.sum()}）：实际角速度 |rate| "
          f"左 p95 {np.percentile(np.abs(rLa[ss]),95):6.1f} 右 p95 {np.percentile(np.abs(rRa[ss]),95):6.1f} deg/s"
          if ss.sum() > 10 else "稳态样本太少")

    # ---------------- 代价平坦 ----------------
    section("4. 换向代价几乎持平（决策病态点）")
    costs = a[:, C_COST]
    spread = costs.max(axis=1) - costs.min(axis=1)
    flat = spread < args.flat_thr
    # 只统计有意义的样本：非锁定、状态 RUN
    flat &= (a[:, C_LOCK] < 0.5) & (st == 3)
    feps = clusters(np.where(flat)[0], int(args.gap / dt))
    print(f"代价极差 < {args.flat_thr:.3f} s 且未锁定，共 {len(feps)} 段")
    for s, e in feps[: args.top]:
        print(f"  {t[s]:6.2f}-{t[e]:6.2f}  {t[e]-t[s]:5.2f}s  "
              f"极差 p50 {np.percentile(spread[s:e+1],50):.3f}  "
              f"L实际 {La[s]:7.1f}  R实际 {Ra[s]:7.1f}  "
              f"|tau| {np.mean(np.abs(a[s:e+1, C_LTA])):.2f}/"
              f"{np.mean(np.abs(a[s:e+1, C_RTA])):.2f} N·m")

    # ---------------- combo / lock 抖动 ----------------
    section("5. 换向方案与锁定的抖动")
    dc = np.diff(a[:, C_COMBO])
    ci = np.where(dc != 0)[0]
    print(f"combo 切换 {len(ci)} 次")
    rapid = [i for i in ci if i + 1 < n and abs(t[i + 1] - t[i]) < 0.05]
    print(f"  其中间隔 <50ms 的连续切换 {len(rapid)} 处")
    for i in ci[: args.top]:
        jumpL = Lt[i + 1] - Lt[i]
        jumpR = Rt[i + 1] - Rt[i]
        print(f"  t={t[i+1]:6.3f}  {a[i,C_COMBO]:.0f}→{a[i+1,C_COMBO]:.0f}  "
              f"目标跳变 L {jumpL:+7.1f}  R {jumpR:+7.1f} deg  "
              f"costs " + "/".join(f"{c:.2f}" for c in a[i + 1, C_COST]))

    dl = np.diff(a[:, C_LOCK])
    li = np.where(dl != 0)[0]
    print(f"\nlocked 切换 {len(li)} 次；locked 占 {100*a[:,C_LOCK].mean():.1f}%")
    ch = clusters(np.where(np.diff(np.r_[a[0, C_LOCK], a[:, C_LOCK]]) != 0)[0],
                  max(1, int(round(0.05 / dt))))
    print(f"  50ms 内的成组抖动 {len(ch)} 组")

    # ---------------- 目标高频抖动 ----------------
    section("6. 目标角高频抖动（小力矩下方向噪声）")
    # 目标角相对 5 点滑动中值的残差；只统计未锁定、目标无大趋势的样本
    def hf_jitter(x):
        kk = 5
        med = x.copy()
        for i in range(kk, n - kk):
            med[i] = np.median(x[i - kk:i + kk + 1])
        d = x[kk:n - kk] - med[kk:n - kk]
        mask = (a[kk:n - kk, C_LOCK] < 0.5) & (st[kk:n - kk] == 3)
        return np.abs(d[mask]), mask

    for nm, x in (("左", Lt), ("右", Rt)):
        d, mask = hf_jitter(x)
        if len(d):
            print(f"  {nm}目标高频残差：p95 {np.percentile(d,95):5.2f}  "
                  f"p99 {np.percentile(d,99):5.2f}  max {d.max():5.2f} deg")

    # 小力矩样本里的方向噪声
    small = (np.abs(a[:, C_LTT]) + np.abs(a[:, C_RTT])) / 2.0 < 0.5
    small &= (a[:, C_LOCK] < 0.5) & (st == 3)
    if small.sum() > 10:
        dL, _ = hf_jitter(Lt)
        print(f"  目标力矩均值 <0.5 N·m 的样本占 {100*small.mean():.1f}%"
              f"（这些时刻 atan2(f_lat,f_drive) 最病态）")

    # ---------------- 轮向力矩 ----------------
    section("7. 轮向力矩")
    for nm, tt, ta, g in (("左", a[:, C_LTT], a[:, C_LTA], a[:, C_GL]),
                          ("右", a[:, C_RTT], a[:, C_RTA], a[:, C_GR])):
        print(f"  {nm}轮 目标 |tau| p50 {np.percentile(np.abs(tt[t>1.9]),50):5.2f} "
              f"p95 {np.percentile(np.abs(tt[t>1.9]),95):5.2f} max {np.abs(tt).max():5.2f} N·m；"
              f"实测 |tau| p95 {np.percentile(np.abs(ta[t>1.9]),95):5.2f}；"
              f"出力比例 均值 {g.mean():+.2f} |比例| p95 {np.percentile(np.abs(g),95):.2f}")

    # ---------------- 指令 / 反馈 / 期望力（28 通道日志） ----------------
    if a.shape[1] >= N_COLS_NEW:
        section("8. 指令 / 反馈 / 期望力（新通道）")
        cmd = a[:, C_CMD]
        fb = a[:, C_FB]
        wr = a[:, C_WR]
        ws = a[:, C_WS]
        run = st == 3
        if run.sum() < 10:
            print("  RUN 样本太少，跳过统计")
            return 0

        # 8.1 符号一致性：区分"指令在转"和"反馈在转"前，先确认两边同号
        for k, nm, thr in ((0, "vx", 0.05), (1, "vy", 0.05), (2, "w", 0.1)):
            m = run & (np.abs(cmd[:, k]) > thr) & (np.abs(fb[:, k]) > thr)
            if m.sum() > 10:
                agree = 100.0 * np.mean(np.sign(cmd[m, k]) == np.sign(fb[m, k]))
                print(f"  {nm:2s}: 指令/反馈都 >{thr:g} 的 {m.sum():4d} 点，同号 {agree:5.1f}%"
                      f"{'  ← 疑似符号链反了' if agree < 20 else ''}")
        print(f"  指令 |vx| p95 {np.percentile(np.abs(cmd[run,0]),95):5.2f} m/s，"
              f"|vy| p95 {np.percentile(np.abs(cmd[run,1]),95):5.2f} m/s，"
              f"|w| p95 {np.percentile(np.abs(cmd[run,2]),95):5.2f} rad/s")
        print(f"  反馈峰值 |vx| {np.abs(fb[:,0]).max():5.2f} m/s，|vy| {np.abs(fb[:,1]).max():5.2f}，"
              f"|w| {np.abs(fb[:,2]).max():5.2f} rad/s")

        # 8.2 期望力大小与方向自转
        mag = np.hypot(wr[:, 0], wr[:, 1])
        ang = np.degrees(np.arctan2(wr[:, 1], wr[:, 0]))
        rate = np.abs(np.diff(np.unwrap(np.radians(ang)))) / np.diff(t)
        print(f"  期望力 |F|：p50 {np.percentile(mag[run],50):5.1f}  "
              f"p95 {np.percentile(mag[run],95):5.1f}  max {mag[run].max():6.1f} N；"
              f"|mw| p95 {np.percentile(np.abs(wr[run,2]),95):5.1f}  max {np.abs(wr[:,2]).max():6.1f} N·m")
        print(f"  期望力方向转速：p95 {np.percentile(rate[run[:-1]],95):5.0f}  "
              f"max {rate[run[:-1]].max():6.0f} deg/s")
        beps = clusters(np.where(run & (mag > args.big_force))[0], int(args.gap / dt))
        print(f"  |F| > {args.big_force:.0f} N 的时段 {len(beps)} 段（前 {args.top}）：")
        for s, e in beps[: args.top]:
            print(f"    {t[s]:6.2f}-{t[e]:6.2f}  {t[e]-t[s]:5.2f}s  "
                  f"|F| max {mag[s:e+1].max():6.1f} N  "
                  f"方向 {ang[s]:6.1f}→{ang[e]:6.1f}°  "
                  f"mw {wr[s:e+1,2].min():+6.1f}~{wr[s:e+1,2].max():+6.1f}")

        # 8.3 轮速与力方向是否对得上（轮子真的在转，而不是只改了舵角）
        print(f"  驱动轮轮速：L p95 {np.percentile(np.abs(ws[:,0]),95):5.1f}，"
              f"R p95 {np.percentile(np.abs(ws[:,1]),95):5.1f} rad/s")
    else:
        section("8. 指令 / 反馈 / 期望力（新通道）")
        print(f"  本日志只有 {a.shape[1]} 列（<{N_COLS_NEW}），没有 CH18~28；"
              "重新烧录 VOFA_CHANNELS=28 的固件后再采一次。")

    # ---------------- 9. yaw 自激 / 舵角跟随 / 力方向（28 通道日志） ----------------
    section("9. yaw 振荡与舵角跟随（乱转机理）")
    if a.shape[1] < N_COLS_NEW:
        print(f"  本日志只有 {a.shape[1]} 列（<{N_COLS_NEW}），跳过")
        return 0

    cmd = a[:, C_CMD]
    fb = a[:, C_FB]
    wr = a[:, C_WR]
    run = a[:, C_STATE] == 3
    if run.sum() < 10:
        print("  RUN 样本太少，跳过")
        return 0

    # 9.1 分工况看 yaw：区分"指令本来就在转"和"指令不转但车在转"
    print("  各工况反馈 yaw |w_fb| (rad/s)：")
    regimes = (
        ("前进 vx>0.3,vy≈0", run & (np.abs(cmd[:, 1]) < 0.1) & (np.abs(cmd[:, 0]) > 0.3)),
        ("横移 |vy|>0.3", run & (np.abs(cmd[:, 1]) > 0.3)),
        ("自转 |w_cmd|>0.2", run & (np.abs(cmd[:, 2]) > 0.2)),
        ("静止 三轴≈0", run & (np.max(np.abs(cmd), axis=1) < 0.05)),
    )
    for nm, m in regimes:
        if m.sum() < 10:
            print(f"    {nm:16s} 样本不足")
            continue
        aw = np.abs(fb[m, 2])
        print(f"    {nm:16s} n={m.sum():4d} ({100*m.mean():4.1f}%)  "
              f"p50 {np.percentile(aw,50):4.2f}  p95 {np.percentile(aw,95):4.2f}  "
              f"max {aw.max():4.2f}")

    # 9.2 指令 w≈0 但 yaw 在振荡的时段：这是"自激"而不是"跟随指令"
    osc = run & (np.abs(fb[:, 2]) > args.osc_thr) & (np.abs(cmd[:, 2]) < 0.1)
    eps = clusters(np.where(osc)[0], int(0.05 / dt))
    print(f"\n  自激判据：|w_fb|>{args.osc_thr:g} rad/s 且 |w_cmd|<0.1，"
          f"占 RUN {100*osc.sum()/run.sum():5.1f}%，共 {len(eps)} 段")
    longest = sorted(eps, key=lambda p: p[1] - p[0], reverse=True)[:3]
    for s, e in longest:
        seg = fb[s:e + 1, 2]
        if len(seg) < int(0.15 / dt):
            continue
        x = seg - seg.mean()
        f = np.fft.rfftfreq(len(x), dt)
        spec = np.abs(np.fft.rfft(x))
        k = int(np.argmax(spec[1:])) + 1
        print(f"    {t[s]:6.2f}-{t[e]:6.2f}  {t[e]-t[s]:5.2f}s  峰峰 "
              f"{seg.max()-seg.min():4.2f} rad/s  主频 {f[k]:4.1f} Hz  "
              f"|mw| p95 {np.percentile(np.abs(wr[s:e+1,2]),95):4.1f} N·m")

    # 9.3 自激段里，舵目标/实际跟得上吗？（前 3 长时段各算一次）
    print("\n  自激段的舵角跟随（目标→实际滞后取一阶差分互相关峰）：")
    for s, e in longest:
        if e - s < int(0.15 / dt):
            continue
        tau = a[s:e + 1]
        al = np.abs(tau[:, C_GL])
        ar = np.abs(tau[:, C_GR])
        weak = 0.5 * (np.mean(al < 0.5) + np.mean(ar < 0.5))
        print(f"    {t[s]:6.2f}-{t[e]:6.2f} 出力比例 |align| 均值 {0.5*(al.mean()+ar.mean()):.2f}，低出力(<0.5) {100*weak:3.0f}%")
        for nm, ct, ca in (("左", C_LT, C_LA), ("右", C_RT, C_RA)):
            tg = unwrap_deg(tau[:, ct])
            ac = unwrap_deg(tau[:, ca])
            err = wrap_err_deg(tau[:, ct], tau[:, ca])
            # 目标→实际滞后：对去均值后的一阶差分做互相关（两段都连续）
            dg = np.diff(tg)
            da = np.diff(ac)
            dg = dg - dg.mean()
            da = da - da.mean()
            lags = range(0, min(20, len(dg) // 2)) if len(dg) > 2 else range(0)
            cc = [np.corrcoef(dg[:len(dg) - k] if k else dg,
                              da[k:] if k else da)[0, 1] for k in lags]
            kbest = int(np.nanargmax(cc)) if cc else 0
            print(f"      {nm}舵 目标 {tg.min():6.0f}~{tg.max():6.0f}°  "
                  f"实际 {ac.min():6.0f}~{ac.max():6.0f}°  "
                  f"误差 p95 {np.percentile(np.abs(err),95):5.1f}° max {np.abs(err).max():5.1f}°  "
                  f"滞后 {kbest*dt*1e3:4.0f} ms (r={cc[kbest]:.2f})")

    # 9.4 力分配要求的轮向倾斜：mw 要靠"两轮纵向力反向"产生，
    #     横移时轮面要从 ±90° 偏开 ==> 舵轮被要求来回摆。
    print("\n  力分配要求的轮向倾斜（用实测舵角 + 期望广义力重建）：")
    R = args.wheel_radius
    Y = args.wheel_y
    idx = np.where(run)[0]
    tilt = np.zeros((len(idx), 2))
    tau_f = np.zeros((len(idx), 2))
    for j, k in enumerate(idx):
        al = np.radians(a[k, C_LA])
        ar = np.radians(a[k, C_RA])
        rows = []
        for a_i, y in ((al, Y), (ar, -Y)):
            g = np.array([np.cos(a_i), np.sin(a_i)])
            rows.append([g[0], g[1], g[1] * 0.0 - g[0] * y])
            rows.append([-g[1], g[0], g[0] * 0.0 + g[1] * y])
        J = np.array(rows)
        try:
            q = np.linalg.solve(J.T @ J, wr[k])
        except np.linalg.LinAlgError:
            continue
        f = J @ q
        for w, a_i in enumerate((al, ar)):
            fd, fl = f[2 * w], f[2 * w + 1]
            phi = a_i + np.arctan2(fl, fd)
            # 与换向决策一致：φ 与 φ+180° 配 ±τ 等价，取离当前角更近的那个解
            e = wrap_err_deg(np.degrees(phi), np.degrees(a_i))
            if abs(e) > 90.0:
                e -= 180.0 if e > 0.0 else -180.0
            tilt[j, w] = e
            tau_f[j, w] = np.hypot(fd, fl) * R
    at = np.abs(tilt)
    print(f"    |倾斜角| p50 {np.percentile(at,50):4.0f}°  p95 {np.percentile(at,95):4.0f}°  "
          f"max {at.max():4.0f}°；>30° 占 {100*np.mean(at>30):4.1f}%")
    print(f"    期望轮缘力矩 |tau_F| p95 {np.percentile(np.abs(tau_f),95):4.1f} N·m")
    # 横移时 fy 与 mw 的几何关系：倾斜角 ≈ atan((mw/(2Y)) / (fy/2))
    m = run & (np.abs(wr[:, 1]) > 20.0)
    if m.sum() > 10:
        need = np.degrees(np.arctan2(np.abs(wr[m, 2]) / (2 * Y), np.abs(wr[m, 1]) / 2))
        print(f"    |fy|>20 N 的 {m.sum()} 点：仅由 mw 需要的倾斜角 "
              f"p50 {np.percentile(need,50):4.0f}° p95 {np.percentile(need,95):4.0f}° "
              f"max {need.max():4.0f}°")

    return 0


if __name__ == "__main__":
    sys.exit(main())
