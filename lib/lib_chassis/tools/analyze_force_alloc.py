#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘力分配 / 舵向振荡分析

输入 app/half_rudder_chassis 打出的 VOFA+ JustFloat CSV（29 列：第 0 列时间戳，
CH1~CH28 见 app_cfg.h 的通道表）。回答三个问题：

  1) 舵向目标到底跳多快、舵机跟不跟得上？
     - 目标/实际的角速度分布、跟踪误差、目标超舵机上限的比例；
     - 纯 vx+w / 纯 vy / 斜行三种工况的对比。

  2) yaw 振荡是 IMU 噪声还是车体真的在转？
     - 用两轮轮速 + 舵角做运动学最小二乘解算 yaw 角速度，和 IMU 的 w 比；
     - 相关系数高 ⇒ 振荡是真的，滤波 IMU 治不了根。

  3) 当前最小范数力分配把 mz 分给了谁？
     - 复现 lib_chassis 的 4 行最小范数解；
     - 把 mz 拆成"驱动行贡献 mz_drive"和"侧向行贡献 mz_lat"；
     - 给出目标舵角的闭式：
           φ_L* = atan2(fy, fx − mz/a)
           φ_R* = atan2(fy, fx + mz/a)
       （x=0 的半舵几何；与当前舵角无关，所以 W 一变目标立刻变）
     - 扫描侧向力惩罚权重 r_lat，看"少用侧向力"要以多少 mz 残差为代价。

用法
----
    python3 analyze_force_alloc.py a.csv b.csv ...
    python3 analyze_force_alloc.py --verbose ignore/斜着走.csv

列映射默认就是 app_cfg.h 的当前布局，换布局时用参数覆盖。
退出码：0 正常；1 输入布局不符。
"""

import argparse
import sys

import numpy as np
import pandas as pd

# ---------------------------------------------------------------- 通道默认值
# app_cfg.h：CSV 第 k 列 = CHk，第 0 列是时间戳
CH_TGT_L, CH_ACT_L = 1, 2
CH_TGT_R, CH_ACT_R = 3, 4
CH_TAU_L, CH_TAU_L_FB = 5, 6
CH_TAU_R, CH_TAU_R_FB = 7, 8
CH_COMBO = 9
CH_LOCKED = 14
CH_STATE = 15
CH_ALIGN_L, CH_ALIGN_R = 16, 17
CH_VX_CMD, CH_VY_CMD, CH_W_CMD = 18, 19, 20
CH_VX_FB, CH_VY_FB, CH_W_FB = 21, 22, 23
CH_FX, CH_FY, CH_MZ = 24, 25, 26
CH_WS_L, CH_WS_R = 27, 28

POS_Y_DEFAULT = 0.20225   # 舵轮回转中心 |y| (m)
RADIUS_DEFAULT = 0.05025  # 驱动轮半径 (m)
SERVO_VMAX_DEFAULT = 358.0  # 舵轮侧 50 rad/s 电机 / 8 = 358 deg/s
TAU_LOCK_DEFAULT = 0.20   # CHASSIS_COST_TORQUE_LOCK (N·m)


def die(msg):
    print(f"✗ {msg}")
    sys.exit(1)


def load_csv(path, ncol):
    df = pd.read_csv(path)
    a = df.to_numpy(dtype=np.float64)
    if a.ndim != 2 or a.shape[1] < ncol:
        die(f"{path}: 列数 {a.shape[1] if a.ndim == 2 else '?'} < 期望 {ncol}")
    drop = 0
    # 录数时偶发的"首行多插一个时间戳"：CH16(align) 不可能 >1。
    # 必须先丢再算相对时间：首行时间戳可能是 ~1e36 的垃圾值，
    # 用 a[0,0] 做基准会把后面所有时间戳压成同一个数（float64 精度丢失）。
    if abs(a[0, CH_ALIGN_L]) > 1.01:
        a = a[1:]
        drop = 1
    t = (a[:, 0] - a[0, 0]) / 1e6
    dt = np.diff(t)
    if np.any(dt <= 0):
        die(f"{path}: 时间戳非严格递增（{(dt <= 0).sum()} 处）")
    return a, t, drop


def wrap(x):
    return (x + np.pi) % (2.0 * np.pi) - np.pi


def fold90(x):
    """按 180° 等价折叠到 (-90°, 90°]。"""
    return (x + np.pi / 2.0) % np.pi - np.pi / 2.0


def unwrap_deg(x):
    return np.degrees(np.unwrap(np.radians(x)))


def spec_peak(x, fs):
    """去均值 + Hann 窗后的主频/幅值。"""
    x = np.asarray(x, float)
    x = x - np.mean(x)
    n = len(x)
    if n < 16:
        return 0.0, 0.0
    w = np.hanning(n)
    X = np.abs(np.fft.rfft(x * w))
    fr = np.fft.rfftfreq(n, 1.0 / fs)
    k = int(np.argmax(X[1:]) + 1)
    return fr[k], 2.0 * X[k] / np.sum(w)


def h1_amp_phase(x, fs, f0):
    """在 f0 处最小二乘拟合 A·cos(2πf0t+φ)，返回 (A, φ[rad])。"""
    n = np.arange(len(x))
    M = np.column_stack([
        np.cos(2.0 * np.pi * f0 * n / fs),
        np.sin(2.0 * np.pi * f0 * n / fs),
        np.ones(len(x)),
    ])
    c = np.linalg.lstsq(M, np.asarray(x, float), rcond=None)[0]
    return float(np.hypot(c[0], c[1])), float(np.arctan2(-c[1], c[0]))


def jac_rows(alpha, a_y, x=0.0):
    """4×3：每轮 [驱动行; 侧向行]。alpha = [α_L, α_R] (rad)。"""
    c, s = np.cos(alpha), np.sin(alpha)
    y = np.array([a_y, -a_y], float)
    J = np.zeros((4, 3))
    for i in range(2):
        J[2 * i] = [c[i], s[i], -y[i] * c[i]]          # 驱动行
        J[2 * i + 1] = [-s[i], c[i], y[i] * s[i]]      # 侧向约束行
    return J, c, s, y


def kinematic_yaw(alpha, ws, radius, a_y):
    """两轮轮速 + 舵角 → (vx, vy, w) 最小二乘（侧向不滑约束）。"""
    out = np.zeros((len(alpha), 3))
    for k in range(len(alpha)):
        cl, sl = np.cos(alpha[k, 0]), np.sin(alpha[k, 0])
        cr, sr = np.cos(alpha[k, 1]), np.sin(alpha[k, 1])
        M = np.array([
            [cl, sl, -a_y * cl],
            [-sl, cl, a_y * sl],
            [cr, sr, a_y * cr],
            [-sr, cr, -a_y * sr],
        ])
        b = np.array([ws[k, 0] * radius, 0.0, ws[k, 1] * radius, 0.0])
        out[k] = np.linalg.lstsq(M, b, rcond=None)[0]
    return out


def minnorm_alloc(alpha, W, a_y):
    """复现 lib_chassis：q=(JᵀJ)⁻¹W，f=Jq。返回 fd, fl, phi, mz_drive, mz_lat。"""
    n = len(alpha)
    fd = np.zeros((n, 2))
    fl = np.zeros((n, 2))
    phi = np.zeros((n, 2))
    mzd = np.zeros(n)
    mzl = np.zeros(n)
    for k in range(n):
        J, c, s, y = jac_rows(alpha[k], a_y)
        q = np.linalg.solve(J.T @ J, W[k])
        f = J @ q
        fd[k], fl[k] = f[0::2], f[1::2]
        phi[k] = alpha[k] + np.arctan2(fl[k], fd[k])
        mzd[k] = np.sum(-y * c * fd[k])
        mzl[k] = np.sum(y * s * fl[k])
    return fd, fl, phi, mzd, mzl


def weighted_alloc(alpha, W, a_y, r_lat, r_drive=1e-9):
    """软约束加权分配：min ||Jᵀf−W||² + fᵀRf。"""
    J, c, s, y = jac_rows(alpha, a_y)
    Q = np.eye(3)
    R = np.diag([r_drive, r_lat, r_drive, r_lat])
    f = np.linalg.solve(J @ Q @ J.T + R, J @ Q @ W)
    return f[0::2], f[1::2], J.T @ f


def analyze(path, args):
    ncol = max(CH_WS_R, CH_MZ, CH_STATE, CH_FX) + 1
    a, t, dropped = load_csv(path, ncol)
    run = a[:, CH_STATE] == args.run_state
    if run.sum() < 20:
        print(f"\n{path}: RUN 帧只有 {run.sum()}，跳过")
        return

    fs = 100.0
    aL = np.radians(a[:, CH_ACT_L])
    aR = np.radians(a[:, CH_ACT_R])
    alpha = np.column_stack([aL, aR])
    tgtL = a[:, CH_TGT_L]
    tgtR = a[:, CH_TGT_R]
    vxc, vyc, wc = a[:, CH_VX_CMD], a[:, CH_VY_CMD], a[:, CH_W_CMD]
    wf = a[:, CH_W_FB]
    fx, fy, mz = a[:, CH_FX], a[:, CH_FY], a[:, CH_MZ]
    ws = np.column_stack([a[:, CH_WS_L], a[:, CH_WS_R]])
    W = np.column_stack([fx, fy, mz])

    print("\n" + "=" * 88)
    print(f"{path}  ({len(a)} 帧，RUN {run.sum()}，{t[-1]:.2f}s，{fs:.0f}Hz"
          + ("，已丢弃首行异常" if dropped else "") + ")")
    vmag = np.hypot(vxc, vyc)
    print(f"  指令 vx [{vxc.min():.2f},{vxc.max():.2f}]  "
          f"vy [{vyc.min():.2f},{vyc.max():.2f}]  "
          f"w [{wc.min():.2f},{wc.max():.2f}]")

    # ---------------------------------------------------------------- 1) 舵向
    print("\n[1] 舵向目标/实际（RUN）")
    for ch, nm in [(CH_TGT_L, "目标L"), (CH_TGT_R, "目标R"),
                   (CH_ACT_L, "实际L"), (CH_ACT_R, "实际R")]:
        x = unwrap_deg(a[:, ch])
        sp = np.abs(np.diff(x) / np.diff(t))[run[1:] & run[:-1]]
        print(f"  {nm}: 角速度 med {np.median(sp):6.0f}  p95 {np.percentile(sp, 95):7.0f}  "
              f"p99 {np.percentile(sp, 99):7.0f}  max {np.max(sp):8.0f} deg/s;  "
              f">按 {args.servo_vmax:.0f} 占 {np.mean(sp > args.servo_vmax) * 100:4.0f}%")
    for tch, ach, nm in [(CH_TGT_L, CH_ACT_L, "L"), (CH_TGT_R, CH_ACT_R, "R")]:
        e = (a[:, tch] - a[:, ach] + 180.0) % 360.0 - 180.0
        print(f"  跟踪误差{nm}: med|{np.median(np.abs(e[run])):.1f}|  "
              f"p95|{np.percentile(np.abs(e[run]), 95):.1f}|  "
              f"max|{np.max(np.abs(e[run])):.1f}| deg;  >30° 占 {np.mean(np.abs(e[run]) > 30) * 100:4.0f}%")

    # ---------------------------------------------------------------- 2) yaw
    kin = kinematic_yaw(alpha, ws, args.radius, args.pos_y)
    corr = np.corrcoef(wf[run], kin[run, 2])[0, 1]
    print("\n[2] yaw：IMU vs 轮速运动学")
    print(f"  IMU w std {np.std(wf[run]):.3f} rad/s，运动学 w std {np.std(kin[run, 2]):.3f}，"
          f"相关 {corr:.3f}，差 std {np.std(wf[run] - kin[run, 2]):.3f} rad/s")

    # 主频：整段 RUN 或指定窗口
    m = run.copy()
    if args.window:
        m &= (t >= args.window[0]) & (t <= args.window[1])
    if m.sum() >= 64:
        print(f"  振荡主频（窗 {t[m][0]:.2f}~{t[m][-1]:.2f}s）:")
        psi = np.arctan2(vyc, vxc)
        relL = np.degrees(fold90(aL - psi))
        relR = np.degrees(fold90(aR - psi))
        for sig, nm in [(relL - relR, "toe(实际)"),
                        (wf, "w(IMU)"), (mz, "mw(力控)"),
                        (np.abs(fx) + np.abs(fy), "|W|平动")]:
            f0, amp = spec_peak(sig[m], fs)
            print(f"    {nm:8s} 峰值 {f0:5.2f} Hz，幅值 {amp:7.3f}，std {np.std(sig[m]):7.3f}")

    # ---------------------------------------------------------------- 3) 分配
    fd, fl, phi, mzd, mzl = minnorm_alloc(alpha, W, args.pos_y)
    tauL = a[:, CH_TAU_L]
    tauR = a[:, CH_TAU_R]
    print("\n[3] 当前最小范数分配")
    J0, c0, s0, _ = jac_rows(alpha[run][0], args.pos_y)
    A = J0.T @ J0
    print(f"  JᵀJ = diag/常数：cond {np.linalg.cond(A):.2f}（x=0 半舵与舵角无关，"
          f"不存在奇异）")
    frac = np.abs(mzl[run]) / (np.abs(mzd[run]) + np.abs(mzl[run]) + 1e-9)
    print(f"  mz 中侧向力承担比例：med {np.median(frac):.2f}  p95 {np.percentile(frac, 95):.2f}")
    sa = np.abs(np.sin(alpha[run, 0]))
    for lo, hi in [(0.0, 0.2), (0.2, 0.5), (0.5, 0.8), (0.8, 1.01)]:
        mm = (sa >= lo) & (sa < hi)
        if mm.sum() > 10:
            print(f"    |sinα| {lo:.1f}~{hi:.1f}: n={mm.sum():4d}  "
                  f"侧向占比 med {np.median(frac[mm]):.2f}  p95 {np.percentile(frac[mm], 95):.2f}")

    # 闭式目标角 vs 录到的目标
    pL = np.arctan2(fy, fx - mz / args.pos_y)
    pR = np.arctan2(fy, fx + mz / args.pos_y)
    eL = np.degrees(fold90(np.radians(tgtL) - pL))
    eR = np.degrees(fold90(np.radians(tgtR) - pR))
    print(f"  闭式目标 φ*=atan2(fy, fx∓mz/a) 与录到的目标："
          f"L med|{np.median(np.abs(eL[run])):.2f}| p95|{np.percentile(np.abs(eL[run]), 95):.2f}|，"
          f"R med|{np.median(np.abs(eR[run])):.2f}| p95|{np.percentile(np.abs(eR[run]), 95):.2f}| deg")

    # 侧向力惩罚扫描（只跑 RUN 的抽样帧，够看趋势）
    idx = np.where(run)[0][::max(1, int(run.sum() / 400))]
    print("  侧向力惩罚扫描（r_lat↑ = 少用侧向力；代价是 mz 残差）")
    for rl in args.rlat_list:
        dev, lf, res = [], [], []
        for k in idx:
            fdd, fll, Wa = weighted_alloc(alpha[k], W[k], args.pos_y, rl)
            dev.extend(np.degrees(fold90((alpha[k] + np.arctan2(fll, fdd)) - np.arctan2(W[k, 1], W[k, 0]))))
            lf.extend(np.abs(fll) / (np.abs(fdd) + 1e-3))
            res.append(Wa[2] - W[k, 2])
        dev, lf, res = np.array(dev), np.array(lf), np.array(res)
        print(f"    r_lat={rl:<7g} |fl/fd| med {np.median(lf):5.2f} p95 {np.percentile(lf, 95):6.2f}  "
              f"舵角偏离力方向 med|{np.median(np.abs(dev)):5.1f}| p95|{np.percentile(np.abs(dev), 95):5.1f}|°  "
              f"mz 残差 std {np.std(res):5.2f} p95|{np.percentile(np.abs(res), 95):5.2f}|")

    # 驱动行单独能不能扛住
    res = []
    for k in idx:
        J, c, s, y = jac_rows(alpha[k], args.pos_y)
        Jd = J[0::2]
        f = np.linalg.lstsq(Jd.T, W[k], rcond=None)[0]
        res.append(W[k] - Jd.T @ f)
    res = np.array(res)
    print(f"  仅驱动行最小二乘：残差 fx p95|{np.percentile(np.abs(res[:, 0]), 95):5.2f}|  "
          f"fy p95|{np.percentile(np.abs(res[:, 1]), 95):5.2f}|  "
          f"mz p95|{np.percentile(np.abs(res[:, 2]), 95):5.2f}| N/Nm")

    # ---------------------------------------------------------------- 5) yaw 权限
    # 只看"指令 w≈0 的平动段"：此时任何 IMU w 都是 yaw 误差。
    seg = run & (np.abs(wc) < args.w_cmd_tol) & (np.hypot(vxc, vyc) > args.trans_speed_min)
    if seg.sum() >= 20:
        eL = np.degrees(wrap(np.radians(tgtL) - aL))
        eR = np.degrees(wrap(np.radians(tgtR) - aR))
        ca = np.abs(np.cos(aL))
        # 按"目标轮缘力矩 + 实际舵角"估算真正落地的驱动行 yaw 力矩：
        #   M = d·(f_R·cosφ_R − f_L·cosφ_L)，f_i = τ_i/R
        M_real = args.pos_y * (np.cos(aR) * tauR / args.radius
                               - np.cos(aL) * tauL / args.radius)
        # 老代码的余弦优化等价：τ_i *= max(cos(目标−实际), 0)
        cL = np.maximum(np.cos(np.radians(eL)), 0.0)
        cR = np.maximum(np.cos(np.radians(eR)), 0.0)
        M_cos = args.pos_y * (np.cos(aR) * tauR * cR / args.radius
                              - np.cos(aL) * tauL * cL / args.radius)
        ratio = np.abs(M_real[seg]) / (np.abs(mz[seg]) + 1e-9)
        print("\n[5] yaw 权限（仅 |w_cmd|≈0 且 |v|>%.2f m/s 的平动段）" % args.trans_speed_min)
        print(f"  段内 {seg.sum()} 帧：|w_imu| med {np.median(np.abs(wf[seg])):.2f} "
              f"p95 {np.percentile(np.abs(wf[seg]), 95):.2f} rad/s；"
              f"|mw| med {np.median(np.abs(mz[seg])):.2f} p95 {np.percentile(np.abs(mz[seg]), 95):.2f} N·m；"
              f"|M_real| med {np.median(np.abs(M_real[seg])):.2f} p95 {np.percentile(np.abs(M_real[seg]), 95):.2f} N·m")
        print(f"  驱动行实现率 |M_real|/|mw|：med {np.median(ratio):.2f} p95 {np.percentile(ratio, 95):.2f}；"
              f"corr(w_imu, mw) {np.corrcoef(wf[seg], mz[seg])[0, 1]:.2f}")

        def _corr(x, y):
            return np.corrcoef(x, y)[0, 1] if np.std(x) > 1e-9 and np.std(y) > 1e-9 else 0.0

        dtgt = np.abs(np.degrees(np.gradient(np.unwrap(np.radians(tgtL)), t)))
        dpsi = np.abs(np.degrees(np.gradient(np.unwrap(np.arctan2(vyc, vxc)), t)))
        print(f"  corr(|w|, 目标舵速) {_corr(np.abs(wf[seg]), dtgt[seg]):+.2f}，"
              f"corr(|w|, 舵向误差) {_corr(np.abs(wf[seg]), np.abs(eL[seg])):+.2f}，"
              f"corr(|w|, 1−|cosφ|) {_corr(np.abs(wf[seg]), 1.0 - ca[seg]):+.2f}")

        print("  |cosφ| 分箱（cosφ→0 = 轮向接近横移，驱动行 yaw 权限消失）")
        for lo, hi in [(0.0, 0.2), (0.2, 0.5), (0.5, 0.8), (0.8, 1.01)]:
            mm = seg & (ca >= lo) & (ca < hi)
            if mm.sum() < 10:
                continue
            r = np.abs(M_real[mm]) / (np.abs(mz[mm]) + 1e-9)
            print(f"    {lo:.1f}~{hi:.1f}: n={mm.sum():4d}  "
                  f"|w| med {np.median(np.abs(wf[mm])):5.2f} p95 {np.percentile(np.abs(wf[mm]), 95):5.2f}  "
                  f"|mw| med {np.median(np.abs(mz[mm])):5.2f}  "
                  f"|M| med {np.median(np.abs(M_real[mm])):5.2f}  "
                  f"实现率 med {np.median(r):4.2f}")

        bad = seg & ((np.abs(eL) > 30.0) | (np.abs(eR) > 30.0))
        print(f"  余弦优化（τ·max(cos e,0)）：段内 |e|>30° 占 {np.mean(bad[seg]) * 100:4.1f}%；"
              f"M_cos/M_real med {np.median(M_cos[seg] / (M_real[seg] + 1e-12)):.4f}，"
              f"RMS {np.sqrt(np.mean(M_real[seg] ** 2)):.2f}→{np.sqrt(np.mean(M_cos[seg] ** 2)):.2f} N·m")
        if bad.sum() > 0:
            print(f"    仅 |e|>30° 帧：M_cos/M_real med "
                  f"{np.median(M_cos[bad] / (M_real[bad] + 1e-12)):.3f}")

    # 工况分类
    if args.verbose:
        print("\n[4] 0.5s 分箱（RUN）")
        print("   t | vx vy w | 舵角相对指令 L/R | toe | w_imu | mw")
        for s in range(0, len(a), int(fs / 2)):
            e = min(s + int(fs / 2), len(a))
            if run[s:e].sum() < int(fs / 4):
                continue
            psi = np.arctan2(vyc[s:e].mean(), vxc[s:e].mean())
            rl = np.degrees(fold90(aL[s:e] - psi))
            rr = np.degrees(fold90(aR[s:e] - psi))
            print(f"  {t[s]:5.2f} | {vxc[s:e].mean():5.2f} {vyc[s:e].mean():5.2f} {wc[s:e].mean():5.2f} | "
                  f"{np.mean(rl):6.1f} {np.mean(rr):6.1f} | {np.mean(rl - rr):6.1f} | "
                  f"{np.mean(wf[s:e]):6.2f} | {np.mean(mz[s:e]):6.2f}")


def main():
    ap = argparse.ArgumentParser(description="半舵底盘力分配 / 舵向振荡分析")
    ap.add_argument("csv", nargs="+", help="VOFA+ JustFloat CSV")
    ap.add_argument("--run-state", type=int, default=3)
    ap.add_argument("--pos-y", type=float, default=POS_Y_DEFAULT, help="舵轮回转中心 |y| (m)")
    ap.add_argument("--radius", type=float, default=RADIUS_DEFAULT, help="驱动轮半径 (m)")
    ap.add_argument("--servo-vmax", type=float, default=SERVO_VMAX_DEFAULT,
                    help="舵轮侧最大角速度 (deg/s，默认 50rad/s 电机 / 8)")
    ap.add_argument("--rlat-list", type=float, nargs="+", default=[1e-9, 0.01, 0.1, 1.0, 10.0],
                    help="侧向力惩罚权重扫描")
    ap.add_argument("--window", type=float, nargs=2, default=None, metavar=("T0", "T1"),
                    help="主频分析窗口 (s)")
    ap.add_argument("--w-cmd-tol", type=float, default=1e-3,
                    help="[5] 判为平动段的 |w_cmd| 上限 (rad/s)")
    ap.add_argument("--trans-speed-min", type=float, default=0.5,
                    help="[5] 判为平动段的最小 |v_cmd| (m/s)")
    ap.add_argument("--verbose", action="store_true", help="打印 0.5s 分箱")
    args = ap.parse_args()
    for p in args.csv:
        analyze(p, args)


if __name__ == "__main__":
    main()
