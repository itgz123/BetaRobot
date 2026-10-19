#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：档2（代价函数换向判据）实机日志分析

与 analyze_vofa_rudder.py 的分工
--------------------------------
后者按**表述**（ref 的支路标号）看"两轮分支是否一致"。那条推理在档1 时代成立（档1 强制两轮
同分支，故"异分支"必然等于"有一轮在磨盘"）；档2 之后反过来：**异分支恰恰是省掉 180° 白转的
正解**——两轮各留在离自己最近的等价解上、驱动符号相反，推力方向相同。所以本脚本改成按
**物理量**看，并且把"能从这 28 路里看出来"和"看不出来"分清楚。

可观测性（先说清楚，免得误读）
    · 参考与分支的关系：ref = d − π·rev（d = 内核给的期望轮向）。d ≡ ref (mod 180°) 永远成立，
      所以**单轮自己**走的是哪条支路，从 ref 里读不出来（ref 的连续代表值同时兼容 rev=0 与 rev=1）；
      能读出来的只有两轮的**异或**：几何上必须 sign(sin d_L) = sign(sin d_R)（x=0 同轴 ⇒ 两轮
      接触点共用 vy），于是
          sign(sin ref_L) · sign(sin ref_R) < 0  ⟺  rev_L ≠ rev_R
      （d 贴近 0°/180° 即 |sin d| ≈ 0 时这条不可判定，按门限剔除并单独报数）。
      两轮一起差 180°（全局翻转）在任何通道上都不可观测——本轮分析一律不依赖它。
    · 推力方向误差 e = wrap(ref − act) **与分支无关**（§2.1：τ = φ + π·rev ⇒ τ 误差 = −e），
      故 [3]/[4]/[5] 都是硬结论。

关注量
    [1] 固件指纹：align（ch26/27）是否出现负值 —— 档2 去钳负的签名（旧版钳在 0）
    [2] 两轮分支的异或：占比、驻留段长（短段 = 抖动而非驻留）
    [3] 推力方向误差 |e|：分布、稳态占比、>90°（轮子在倒推）的帧/段
    [4] 白转核算：Σ|Δact|（舵轮实际转角） vs Σ|Δ(ref mod 180)|（物理上必要的转角）
    [5] 单轮大角度命令：一轮 |e| 大、另一轮 |e| 小的段（档1 磨盘的形态学对照）
    [6] 目标角变迁：把 ref 跳变拆成"纯表述翻转"（|Δref|≈180 而物理方向没动）与"真指令变化"

输入
----
app/half_rudder_chassis 打出的 VOFA+ JustFloat CSV（28 列）：列 0 = DWT 微秒时间戳，
左舵 1..12、右舵 13..24、ch25 = 状态机、ch26/27 = 两轮 align（余弦系数，不钳负时可负）。

用法
    python3 analyze_costsel_log.py ../../../ignore/chassis_data/vofa+.csv
    python3 analyze_costsel_log.py <csv> --dump 6.6 8.4      # 细看某段逐拍
退出码：0 正常；1 输入布局不符。
"""

import argparse
import csv
import math
import sys

import numpy as np

# 每轮 12 路的相对偏移（见 app_chassis.c 的 ChassisVofaRudder）
OFF_REF = 0    # 舵角给定（舵轮侧, rad）
OFF_ACT = 1    # 舵角实际
OFF_ERR = 2    # 角度环误差（舵轮侧 rad）
OFF_DOUT = 4   # 角度环 d_out（电机侧 rad/s）
OFF_VSPD = 5   # 速度给定（角度环输出, 电机侧 rad/s）
OFF_ISPD = 6   # 速度实际（电机侧 rad/s）
OFF_ICUR = 11  # 实际电流

PI = math.pi


def wrap_pi(a):
    """归一化到 (-π, π]（保留 180° 的区别）"""
    return np.angle(np.exp(1j * np.asarray(a, dtype=np.float64)))


def wrap_half(a):
    """归一化到 (-π/2, π/2]（把 180° 等价解折叠掉 = 物理上必要的最小转角）"""
    return (np.asarray(a, dtype=np.float64) + PI / 2) % PI - PI / 2


def load(path, ncol_hint):
    with open(path, newline="") as f:
        r = csv.reader(f)
        header = next(r)
        rows = [list(map(float, x)) for x in r if x and x[0].strip()]
    a = np.array(rows, dtype=np.float64)
    if a.shape[1] < ncol_hint:
        print(f"✗ 列数 {a.shape[1]} < 期望 {ncol_hint}（首行表头 {len(header)} 项）")
        sys.exit(1)
    if np.any(np.diff(a[:, 0]) <= 0):
        print(f"✗ 时间戳非严格递增（{(np.diff(a[:, 0]) <= 0).sum()} 处）")
        sys.exit(1)
    return a, len(header)


def segments(mask, gap):
    """布尔掩码切段，返回 [(s, e), ...]；相邻命中帧间隔 > gap 即断段"""
    idx = np.where(mask)[0]
    if len(idx) == 0:
        return []
    out, s, p = [], idx[0], idx[0]
    for i in idx[1:]:
        if i - p > gap:
            out.append((s, p))
            s = i
        p = i
    out.append((s, p))
    return out


def report(t, mask, gap, top, tag, fmt):
    segs = segments(mask, gap)
    if not segs:
        print(f"  {tag}: 无")
        return segs
    print(f"  {tag}: {int(mask.sum())} 帧 / {len(segs)} 段")
    for (s, e) in segs[:top]:
        print(f"    t={t[s]:7.3f}~{t[e]:7.3f} ({e - s + 1:5d} 拍) " + fmt(s, e))
    if len(segs) > top:
        print(f"    … 其余 {len(segs) - top} 段省略")
    return segs


def main():
    ap = argparse.ArgumentParser(description="半舵底盘 档2 切换判据实机日志分析")
    ap.add_argument("csv", help="VOFA+ JustFloat CSV 路径")
    ap.add_argument("--st", type=int, default=25, help="状态机通道（默认 25）")
    ap.add_argument("--run-state", type=int, default=3, help="RUN 态取值（默认 3）")
    ap.add_argument("--l-base", type=int, default=1, help="左舵 12 路起始通道（默认 1）")
    ap.add_argument("--r-base", type=int, default=13, help="右舵 12 路起始通道（默认 13）")
    ap.add_argument("--align-l", type=int, default=26, help="左舵余弦系数通道（默认 26）")
    ap.add_argument("--align-r", type=int, default=27, help="右舵余弦系数通道（默认 27）")
    ap.add_argument("--gap", type=int, default=20, help="事件分段：间隔超过该拍数即断段（默认 20）")
    ap.add_argument("--top", type=int, default=10, help="每类事件最多打印段数（默认 10）")
    ap.add_argument("--ang-tol-deg", type=float, default=5.0,
                    help="[2] 判定异或的 |sin d| 幅值门限 [deg]（默认 5）")
    ap.add_argument("--steady-deg", type=float, default=1.0, help="稳态门限 [deg]（默认 1）")
    ap.add_argument("--drift-deg", type=float, default=10.0, help="过渡门限 [deg]（默认 10）")
    ap.add_argument("--solo-deg", type=float, default=45.0, help="[5] '单轮在大转' 门限 [deg]（默认 45）")
    ap.add_argument("--solo-other-deg", type=float, default=15.0,
                    help="[5] '另一轮基本不动' 门限 [deg]（默认 15）")
    ap.add_argument("--flip-deg", type=float, default=30.0,
                    help="[7] 判'物理方向没动'的门限 [deg]（默认 30）")
    ap.add_argument("--vmax-motor", type=float, default=50.0,
                    help="[6] 舵向位置环输出限幅（电机侧 rad/s，默认 50 = CHASSIS_STEER_VMAX_MOTOR）")
    ap.add_argument("--gear", type=float, default=8.0,
                    help="[6] 舵向减速比（默认 8 = STEER_GEAR_RATIO）")
    ap.add_argument("--dump", type=float, nargs=2, default=None, metavar=("T0", "T1"),
                    help="打印 [T0,T1] 秒窗口的逐拍通道")
    args = ap.parse_args()

    ncol_hint = max(args.st, args.l_base + 11, args.r_base + 11, args.align_l, args.align_r) + 1
    a, nhead = load(args.csv, ncol_hint)
    t = (a[:, 0] - a[0, 0]) / 1e6
    dt = float(np.median(np.diff(t)))
    run = a[:, args.st] == args.run_state
    if run.sum() == 0:
        print("✗ 无 RUN 帧")
        return 1

    print(f"文件 {args.csv}（表头 {nhead} 项）")
    print(f"  {a.shape[1]} 列 / {len(a)} 帧 / {t[-1]:.3f}s / 采样 {dt * 1000:.2f}ms；"
          f"RUN 帧 {int(run.sum())}（{dt * run.sum():.2f}s）")
    vals = {int(v): int((a[:, args.st] == v).sum()) for v in np.unique(a[:, args.st])}
    print(f"  状态机分布 {vals}")

    Lref, Lact = a[:, args.l_base + OFF_REF], a[:, args.l_base + OFF_ACT]
    Rref, Ract = a[:, args.r_base + OFF_REF], a[:, args.r_base + OFF_ACT]
    aL, aR = a[:, args.align_l], a[:, args.align_r]

    # ---------------------------------------------------------------- [1] 固件指纹
    print(f"\n[1] 固件指纹（align = 真正乘进轮速的余弦系数；档2 去钳负后可为负）")
    for nm, c in (("左", aL), ("右", aR)):
        m = c[run]
        print(f"  {nm}舵: 最小 {m.min():+7.4f}  最大 {m.max():7.4f}  负值帧 "
              f"{int((m < 0).sum()):5d}（{100.0 * (m < 0).mean():5.3f}%）  "
              f"|align|<1e-6 的帧 {int((np.abs(m) < 1e-6).sum()):5d}")
    if (aL[run] < 0).any() or (aR[run] < 0).any():
        print("  → 出现负值 ⇒ 档2（不钳负）固件")
    else:
        print("  → 全程非负 ⇒ 旧版（钳 0）固件，[2]~[6] 的结论仅供对照")

    # ---------------------------------------------------------------- [2] 两轮分支异或
    print(f"\n[2] 两轮分支的异或（唯一可观测的分支自由度；两轮一起翻 180° 不可观测）")
    tr = math.radians(args.ang_tol_deg)
    eps = math.sin(tr)
    sL, sR = np.sin(Lref), np.sin(Rref)
    obs = run & (np.abs(sL) > eps) & (np.abs(sR) > eps)
    mixed = obs & (sL * sR < 0.0)
    print(f"  可判定帧 {int(obs.sum())} / RUN {int(run.sum())}"
          f"（其余 {int(run.sum() - obs.sum())} 帧里至少一轮的 d 贴近 0°/180°，|sin d|≤{eps:.3f}，判不了）")
    if obs.sum():
        print(f"  **异分支** {int(mixed.sum())} 帧 = 可判定帧的 {100.0 * mixed.sum() / obs.sum():.1f}%，"
              f"占全部 RUN 帧 {100.0 * mixed.sum() / run.sum():.2f}%")
        print("  （异分支 = 两轮本拍落在相差 180° 的两条等价支路上、驱动符号相反 ⇒ 都不用转）")
        segs = report(t, mixed, args.gap, args.top, "异分支段",
                      lambda s, e: f"d_Lmod180={math.degrees(wrap_half(Lref[s])):6.1f} "
                                   f"actL={math.degrees(Lact[e]):6.1f} actR={math.degrees(Ract[e]):6.1f} "
                                   f"ΔactL/R={math.degrees(Lact[e] - Lact[s]):6.1f}/"
                                   f"{math.degrees(Ract[e] - Ract[s]):6.1f}")
        lens = np.array([e - s + 1 for s, e in segs])
        print(f"    段长: 中位 {int(np.median(lens))} 拍 / 最长 {int(lens.max())} 拍（{lens.max() * dt:.2f}s）"
              f"；≤5 拍的短段 {int((lens <= 5).sum())} 段（那才是抖动）")
        # 物理姿态：异分支若真是"两轮各留最近的等价解"，两轮实测角应相差 ≈180°
        dact = np.degrees(np.abs(wrap_pi(Lact - Ract)))
        if mixed.sum():
            print(f"    异分支帧的两轮实测角差 |actL−actR|: 中位 {np.median(dact[mixed]):6.1f}°")
        print(f"    非异分支帧的两轮实测角差: 中位 {np.median(np.degrees(np.abs(wrap_pi(Lact - Ract))[obs & ~mixed])):6.1f}°"
              f"（帧数 {int((obs & ~mixed).sum())}）")
    allpost = np.degrees(np.abs(wrap_pi(Lact - Ract)))[run]
    print(f"  全体 RUN 帧的两轮实测角差 |actL−actR|: 中位 {np.median(allpost):6.1f}°  "
          f"贴着 180°（>150°）的帧 {100.0 * np.mean(allpost > 150.0):5.2f}%  "
          f"贴着 0°（<30°）的帧 {100.0 * np.mean(allpost < 30.0):5.2f}%")

    # ---------------------------------------------------------------- [3] 推力方向误差
    err = {"L": wrap_pi(Lref - Lact), "R": wrap_pi(Rref - Ract)}
    print(f"\n[3] 推力方向误差 |e| = |wrap(ref − act)|（与分支无关，就是轮面偏了目标多少）")
    for nm in ("L", "R"):
        d = np.degrees(np.abs(err[nm][run]))
        print(f"  {nm}: 中位 {np.median(d):6.2f}°  p95 {np.percentile(d, 95):6.2f}°  max {d.max():7.2f}°"
              f"  |  <{args.steady_deg:g}° {100.0 * (d < args.steady_deg).mean():5.2f}%"
              f"  >{args.drift_deg:g}° {100.0 * (d > args.drift_deg).mean():5.2f}%"
              f"  >90° {int((d > 90).sum()):4d} 帧"
              f"  >120° {int((d > 120).sum()):4d} 帧   ← 长弧（档1 磨盘的签名）应恒为 0")
    print("  注：|e| 上界 ≈90° 意味着**决策永远让舵轮走短弧**（rev 只改代表值 ±180°，不产生额外行程）；")
    print("      若某拍把目标摆到 180°−|e| 之外，这里会看到 >120° 的帧。")
    ceff = np.mean([np.abs(np.cos(err[nm][run])) for nm in ("L", "R")])
    print(f"  过渡期推力损失：平均 |cos e| = {ceff:.3f} ⇒ 全程平均只交出 "
          f"{100 * (1 - ceff):.1f}% 的推力；|cos e|<0.9（≈偏 25° 以上）的帧 "
          f"{100.0 * np.mean([(np.abs(np.cos(err[nm][run])) < 0.9) for nm in ('L', 'R')]):5.2f}%")
    for nm in ("L", "R"):
        bad = run & (np.degrees(np.abs(err[nm])) > 90.0)
        report(t, bad, args.gap, args.top, f"{nm} 倒推段（|e|>90°，轮子在反着出头）",
               lambda s, e, _n=nm: f"|e| 最大 {np.degrees(np.abs(err[_n][s:e + 1])).max():6.2f}°")

    # ---------------------------------------------------------------- [4] 白转核算
    print(f"\n[4] 白转核算：舵轮实际转过的角 vs 物理上必要的角（180° 等价解已折叠）")
    for nm, ref, act in (("L", Lref, Lact), ("R", Rref, Ract)):
        dphi = np.abs(np.diff(np.unwrap(act)))[run[1:]].sum()
        need = np.abs(wrap_half(np.diff(ref)))[run[1:]].sum()
        print(f"  {nm}: 实际 Σ|Δact| {math.degrees(dphi):8.1f}°   必要 Σ|Δ(ref mod 180)| "
              f"{math.degrees(need):8.1f}°   比值 {dphi / max(need, 1e-9):.3f}")

    # ---------------------------------------------------------------- [5] 单轮大角度
    print(f"\n[5] 单轮大角度命令（一轮 |e|>{args.solo_deg:g}°、另一轮 |e|<{args.solo_other_deg:g}°；"
          f"档1 磨盘的形态是这里出现 |e|≈180°）")
    eLd, eRd = np.degrees(np.abs(err["L"])), np.degrees(np.abs(err["R"]))
    solo = run & (((eLd > args.solo_deg) & (eRd < args.solo_other_deg)) |
                  ((eRd > args.solo_deg) & (eLd < args.solo_other_deg)))
    segs = segments(solo, args.gap)
    if not segs:
        print("  无")
    else:
        print(f"  {int(solo.sum())} 帧 / {len(segs)} 段")
        for (s, e) in segs[:args.top]:
            who = "L" if eLd[s] > args.solo_deg else "R"
            other = eRd if who == "L" else eLd
            self_ = eLd if who == "L" else eRd
            act = Lact if who == "L" else Ract
            print(f"    t={t[s]:7.3f}~{t[e]:7.3f} ({e - s + 1:4d} 拍) {who}转 "
                  f"|e| {self_[s:e + 1].min():5.1f}→{self_[s:e + 1].max():6.1f}° "
                  f"（另一轮 {other[s:e + 1].max():4.1f}°）  Δact{who}={math.degrees(act[e] - act[s]):7.1f}°")

    # ---------------------------------------------------------------- [6] 舵向执行器
    print(f"\n[6] 舵向执行器（档2 预测里 ω_max = VMAX/GEAR，这里看位置环是不是真的压在它上面）")
    print(f"  VMAX_MOTOR={args.vmax_motor:g} rad/s（电机侧） ⇒ 舵轮侧 {args.vmax_motor / args.gear:.2f} rad/s"
          f" = {math.degrees(args.vmax_motor / args.gear):.0f}°/s")
    for nm, base in (("左", args.l_base), ("右", args.r_base)):
        vspd = np.abs(a[run, base + OFF_VSPD])
        ispd = np.abs(a[run, base + OFF_ISPD])
        icur = np.abs(a[run, base + OFF_ICUR])
        sat = vspd >= 0.999 * args.vmax_motor
        # 贴限幅但实测速度仍远低于限幅 ⇒ 速度环喂不饱（不是位置环没要，是要了给不出）
        starve = sat & (ispd < 0.6 * args.vmax_motor)
        print(f"  {nm}: |速度给定| 中位 {np.median(vspd):5.1f} 贴限幅 "
              f"{100.0 * sat.mean():5.2f}%  |  |实际速度| 中位 {np.median(ispd):5.1f} "
              f"p95 {np.percentile(ispd, 95):5.1f} max {ispd.max():5.1f}（舵轮侧 "
              f"{math.degrees(np.median(ispd) / args.gear):.0f}°/s 中位 / "
              f"{math.degrees(ispd.max() / args.gear):.0f}°/s 峰）  |  |电流| 中位 "
              f"{np.median(icur):4.2f} p95 {np.percentile(icur, 95):4.2f} max {icur.max():4.2f} A")
        print(f"     贴限幅却只跑到 <60% 限幅的帧: {int(starve.sum())}（占 RUN "
              f"{100.0 * starve.sum() / run.sum():5.2f}%）")
        # 角度环 D 项：ANGLE_D_LPF_RC=0 时是对位置读数的差分数值求导，会出单拍尖峰。
        # 判据：误差本身够大（>5°）时，角度环输出却把轮子往反方向推 ⇒ D 项盖过了 P 项。
        ea = a[run, base + OFF_ERR]                      # 舵轮侧 rad
        out = a[run, base + OFF_VSPD]                    # 电机侧 rad/s = 角度环输出
        dout = a[run, base + OFF_DOUT]
        bad = (np.abs(ea) > math.radians(5.0)) & (np.sign(out) * np.sign(ea) < 0)
        print(f"     角度环 d_out: |d| 中位 {np.median(np.abs(dout)):5.2f} p95 "
              f"{np.percentile(np.abs(dout), 95):5.2f} max {np.abs(dout).max():5.2f}（限幅 "
              f"{args.vmax_motor:g}）；误差>5° 却把轮子往反方向推的帧 {int(bad.sum())}"
              f"（占 RUN {100.0 * bad.sum() / run.sum():5.2f}%）")

    # ---------------------------------------------------------------- [7] 目标角变迁
    print(f"\n[7] 目标角变迁：ref 单拍跳 >90° 的成分分解")
    dref = np.degrees(wrap_pi(np.diff(Lref)))
    dref_r = np.degrees(wrap_pi(np.diff(Rref)))
    for nm, dr, er, act in (("L", dref, err["L"], Lact), ("R", dref_r, err["R"], Ract)):
        big = run[1:] & (np.abs(dr) > 90.0)
        # 纯翻转：ref 跳了 ~180°，而物理方向（mod 180）没动
        pure = big & (np.degrees(np.abs(wrap_half(np.radians(dr)))) <= args.flip_deg)
        real = big & ~pure
        print(f"  {nm}: ref 单拍跳 >90° 共 {int(big.sum())} 次 —— 纯表述翻转（物理方向没动）"
              f"{int(pure.sum())} 次，真指令变化 {int(real.sum())} 次")
        # 翻转点一定落在 |e|=90° 附近（rev 只改代表值 ±180°）：这里看翻转前后舵轮是继续同向还是反向
        for k in np.where(pure)[0]:
            spd = np.diff(np.unwrap(act))[max(k - 10, 0):k]
            v_in = np.median(spd) / dt if len(spd) else 0.0   # 翻转前 20ms 的舵轮角速度
            print(f"      t={t[k]:7.3f}  Δref={dr[k]:+7.1f}°  |e| {abs(math.degrees(er[k])):5.1f}"
                  f"→{abs(math.degrees(er[k + 1])):5.1f}°  翻转前舵轮速度 {math.degrees(v_in):+7.0f}°/s")
    chg = np.where(run[1:] & ((np.abs(dref) > 15.0) | (np.abs(dref_r) > 15.0)))[0]
    print(f"  目标角变化 >15° 的行共 {len(chg)} 次")

    if args.dump is not None:
        t0, t1 = args.dump
        sel = np.where((t >= t0) & (t <= t1))[0]
        lv, rv = a[:, args.l_base + OFF_VSPD], a[:, args.r_base + OFF_VSPD]
        li, ri = a[:, args.l_base + OFF_ICUR], a[:, args.r_base + OFF_ICUR]
        print(f"\n[dump] t∈[{t0:g},{t1:g}]s 共 {len(sel)} 拍（角度 °，速度给定电机侧 rad/s，电流 A）")
        print("      t[ms] st |  Lref  Lact    eL |  Rref  Ract    eR | Lvspd Rvspd |  Lic  Ric | cosL cosR")
        for k in sel:
            print(f"  {t[k] * 1e3:9.1f} {int(a[k, args.st]):2d} |"
                  f"{math.degrees(Lref[k]):7.1f}{math.degrees(Lact[k]):7.1f}{math.degrees(err['L'][k]):7.1f} |"
                  f"{math.degrees(Rref[k]):7.1f}{math.degrees(Ract[k]):7.1f}{math.degrees(err['R'][k]):7.1f} |"
                  f"{lv[k]:6.1f}{rv[k]:6.1f} |{li[k]:5.1f}{ri[k]:5.1f} |{aL[k]:5.2f}{aR[k]:5.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
