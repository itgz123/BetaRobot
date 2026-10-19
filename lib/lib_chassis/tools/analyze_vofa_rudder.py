#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：VOFA+ 舵向遥测分析（换向分支 / 几何可达性 / 余弦优化）

用途
----
读 app/half_rudder_chassis 打出的 VOFA+ JustFloat CSV（27 路），回答："两轮舵向是不是
按同一套意图在动？" 具体查四件事：

  1) 几何可达性 —— 两组回转中心都在 x=0（半舵：同轴、y=±d）时，两轮的接触点速度共用
     同一个 vy 分量，所以两轮的**期望轮向**必须同在上半平面或同在下半平面：
         v_i = (vx − w·y_i, vy + w·x_i)，x_i ≡ 0 ⇒ v_iy 与 i 无关
     于是 sign(sin d_L) · sign(sin d_R) < 0 就是**任何 twist 都给不出来的组合**。
     ⚠ 纯 180° 处 sin 会取到 ±0，符号测试必须带幅值门限，否则 ±180 会假报警。
  2) 两轮参考差 —— 舵向参考 ref ≡ 期望轮向 d（mod 180°，因为 app 可折叠 ±180° 等价解），
     两轮 ref 差**不是 0 也不是 ±180** 时，说明两轮被折叠到了不同分支。
  3) 换向瞬态 —— 折叠是一次"驱动符号立即反转、舵轮再慢慢转 180° 追"的物理跳变，
     期间实际推力 τ = φ_act + 180°·rev 偏离期望；两轮若一个翻一个不翻，两个 τ 的误差
     **符号相反**，横向力在 y=±d 两个轮位上产生**同向偏航力矩** ⇒ 车体自转。
  4) 余弦优化 —— align = cos(ref − act) = cos(推力方向误差)，统计它掉到多低、
     以及指令扫速是否超过舵轮极限（超过时 align 必掉）。

输入
----
CSV：首行表头（可含 I0..I27 列名），其后每行 28 列，第 0 列是 DWT 微秒时间戳。
列映射与几何由命令行给；本脚本自检：列数、时间戳单调、通道号在范围内。

退出码：0 正常；1 输入布局不符。
"""

import argparse
import csv
import math
import sys

import numpy as np

# 每个舵轮 12 路的相对偏移（见 app_chassis.c 的 ChassisVofaRudder）
OFF_REF = 0  # 舵角给定（舵轮侧, rad）
OFF_ACT = 1  # 舵角实际
OFF_ERR = 2  # 舵角误差
OFF_VSPD = 5  # 速度给定（角度环输出, 电机侧 rad/s）
OFF_ISPD = 6  # 速度实际
OFF_ICUR = 11  # 实际电流


def load(path, ncol_hint):
    with open(path, newline="") as f:
        r = csv.reader(f)
        header = next(r)
        rows = [list(map(float, x)) for x in r if x and x[0].strip()]
    a = np.array(rows, dtype=np.float64)
    ncol = a.shape[1]
    if ncol < ncol_hint:
        print(f"✗ 列数 {ncol} < 期望 {ncol_hint}（首行表头 {len(header)} 项）")
        sys.exit(1)
    ts = a[:, 0]
    dts = np.diff(ts)
    if np.any(dts <= 0):
        print(f"✗ 时间戳非严格递增（{(dts <= 0).sum()} 处）")
        sys.exit(1)
    return a, ncol, header


def wrap(a):
    """归一化到 (-π, π]"""
    return np.angle(np.exp(1j * a))


def report_segments(t, mask, gap, top, tag, fmt):
    idx = np.where(mask)[0]
    if len(idx) == 0:
        print(f"  {tag}: 无")
        return []
    segs, s, p = [], idx[0], idx[0]
    for i in idx[1:]:
        if i - p > gap:
            segs.append((s, p))
            s = i
        p = i
    segs.append((s, p))
    print(f"  {tag}: {len(idx)} 帧 / {len(segs)} 段")
    for (s, e) in segs[:top]:
        print(f"    t={t[s]:7.3f}~{t[e]:7.3f} ({(e - s + 1):5d} 拍) " + fmt(s, e))
    if len(segs) > top:
        print(f"    … 其余 {len(segs) - top} 段省略")
    return segs


def main():
    ap = argparse.ArgumentParser(description="半舵底盘 VOFA 舵向遥测分析")
    ap.add_argument("csv", help="VOFA+ JustFloat CSV 路径")
    ap.add_argument("--st", type=int, default=25, help="底盘状态机通道（默认 25）")
    ap.add_argument("--run-state", type=int, default=3, help="RUN 态取值（默认 3）")
    ap.add_argument("--l-base", type=int, default=1, help="左舵 12 路起始通道（默认 1）")
    ap.add_argument("--r-base", type=int, default=13, help="右舵 12 路起始通道（默认 13）")
    ap.add_argument("--cos-l", type=int, default=26, help="左舵余弦系数通道（默认 26）")
    ap.add_argument("--cos-r", type=int, default=27, help="右舵余弦系数通道（默认 27）")
    ap.add_argument("--pos-y", type=float, default=0.20225, help="舵轮回转中心 |y| [m]，仅用于提示")
    ap.add_argument("--ang-tol-deg", type=float, default=5.0, help="符号测试的幅值门限 [deg]")
    ap.add_argument("--diff-deg", type=float, default=15.0, help="两轮参考差报警门限 [deg]")
    ap.add_argument("--gap", type=int, default=20, help="事件分段：相邻帧间隔超过该拍数即断段")
    ap.add_argument("--slew-deg-s", type=float, default=358.0, help="舵轮稳态极限转速 [deg/s]（默认 M3508 空载）")
    ap.add_argument("--top", type=int, default=12, help="每类事件最多打印段数")
    ap.add_argument("--cos-low", type=float, default=0.2,
                    help="余弦系数低于此值即视为该轮推力被钳死（默认 0.2 ≈ 78° 失配）")
    ap.add_argument("--trace-deg", type=float, default=15.0,
                    help="[6] 目标舵角变迁的打印门限 [deg]")
    ap.add_argument("--dump", type=float, nargs=2, default=None, metavar=("T0", "T1"),
                    help="打印 [T0,T1] 秒窗口内的逐拍通道，用于细看某段事件")
    args = ap.parse_args()

    ncol_hint = max(args.st, args.l_base + 11, args.r_base + 11, args.cos_l, args.cos_r) + 1
    a, ncol, header = load(args.csv, ncol_hint)
    t = (a[:, 0] - a[0, 0]) / 1e6  # s
    dt = float(np.median(np.diff(t)))
    run = a[:, args.st] == args.run_state
    print(f"文件 {args.csv}")
    print(f"  {ncol} 列 / {len(a)} 帧 / {t[-1]:.3f}s / 采样 {dt * 1000:.2f}ms")
    vals = {int(v): int((a[:, args.st] == v).sum()) for v in np.unique(a[:, args.st])}
    print(f"  状态机分布 {vals}（RUN={args.run_state}）")
    # 状态分段：SAFE(2)/FAULT(1) 帧里舵向失能、给定被清零，且 RUN 重入会重新播种换向状态，
    # 故任何"参考跳变/误差爆表"都必须先对照状态切换时刻读，否则会把失能一拍当成决策 bug。
    stv = a[:, args.st].astype(int)
    chg = np.where(np.diff(stv) != 0)[0]
    print(f"  状态切换 {len(chg)} 次")
    for k in chg[:args.top]:
        print(f"    t={t[k + 1]:7.3f}  {stv[k]} → {stv[k + 1]}"
              f"（前一状态持续 {t[k + 1] - t[k]:.3f}s）")
    if len(chg) > args.top:
        print(f"    … 其余 {len(chg) - args.top} 次省略")
    if run.sum() == 0:
        print("✗ 无 RUN 帧，无可分析内容")
        return 1

    # 各通道（数组下标 = 通道号，因第 0 列是时间戳时 a[:, k] 即 ch k）
    Lref, Lact = a[:, args.l_base + OFF_REF], a[:, args.l_base + OFF_ACT]
    Rref, Ract = a[:, args.r_base + OFF_REF], a[:, args.r_base + OFF_ACT]
    cosL, cosR = a[:, args.cos_l], a[:, args.cos_r]
    eL, eR = Lref - Lact, Rref - Ract  # 舵轮侧角差 = 推力方向误差

    tr = math.radians(args.ang_tol_deg)
    eps = math.sin(tr)
    print(f"\n[1] 换向分支是否分裂（x=0 同轴 ⇒ 两轮 vy 必须同号；门限 ±{args.ang_tol_deg:g}°）")
    # ref ≡ d (mod 180°)：app 折叠 ±180° 等价解时 ref = d ∓ 180。
    # 故 ref 的 sin 符号本身定不了论——要穷举 (fold_L, fold_R) 四种分支组合，
    # 反推 d = ref − 180°·fold，再看哪几种组合满足"两轮 vy 同号"这一几何必要条件。
    ok_same = np.zeros(len(a), bool)   # 存在"两轮同分支"的可达解
    ok_mix = np.zeros(len(a), bool)    # 存在"两轮异分支"的可达解
    for fl in (0, 1):
        for fr in (0, 1):
            dL = Lref - math.pi * fl
            dR = Rref - math.pi * fr
            sL, sR = np.sin(dL), np.sin(dR)
            good = (np.abs(sL) > eps) & (np.abs(sR) > eps) & (sL * sR > 0)
            if fl == fr:
                ok_same |= good
            else:
                ok_mix |= good
    split = run & ok_mix & ~ok_same

    def f_geo(s, e):
        return (f"Lref={math.degrees(Lref[s]):7.1f} Rref={math.degrees(Rref[s]):7.1f} "
                f"Lact={math.degrees(Lact[s]):7.1f} Ract={math.degrees(Ract[s]):7.1f} "
                f"cosL={cosL[s]:.2f} cosR={cosR[s]:.2f}")

    print(f"  RUN 帧 {run.sum()}，其中**只能靠两轮异分支解释** {split.sum()} 帧"
          f"（{split.sum() / max(run.sum(), 1) * 100:.2f}%）← 两轮驱动必然一正一反")
    report_segments(t, split, args.gap, args.top, "分支分裂段", f_geo)

    print(f"\n[2] 两轮参考差（非 0 非 ±180 ⇒ 两轮落在不同换向分支）")
    d = np.degrees(wrap(Lref - Rref))
    off = np.abs(np.abs(d) - 180.0)
    seg_mask = run & (np.abs(d) > args.diff_deg) & (off > args.diff_deg)

    def f_diff(s, e):
        return (f"diff={d[s]:7.1f}（mod180 后偏离 {off[s]:5.1f}）"
                f" Lref={math.degrees(Lref[s]):7.1f} Rref={math.degrees(Rref[s]):7.1f}")

    report_segments(t, seg_mask, args.gap, args.top, "参考差异常段", f_diff)

    print(f"\n[3] 换向瞬态：推力方向误差 = 舵角跟踪误差（τ=φ+180°·rev ⇒ 误差与 rev 无关）")
    for nm, e in (("左", eL), ("右", eR)):
        m = np.abs(e[run])
        print(f"  {nm}舵 |err|: 中位 {math.degrees(np.median(m)):5.1f}° "
              f"p95 {math.degrees(np.percentile(m, 95)):5.1f}° 最大 {math.degrees(m.max()):6.1f}°")
    both = np.degrees(np.abs(eL - eR))[run]
    print(f"  两轮误差之差 |eL−eR|: 中位 {np.median(both):5.1f}° p95 {np.percentile(both, 95):5.1f}°")
    # 误差符号相反且都够大 → 两轮横向力同向、偏航力矩叠加
    opp = run & (np.sign(np.degrees(eL)) * np.sign(np.degrees(eR)) < 0) & \
        (np.abs(np.degrees(eL)) > 20) & (np.abs(np.degrees(eR)) > 20)

    def f_opp(s, e):
        return (f"eL={math.degrees(eL[s]):7.1f} eR={math.degrees(eR[s]):7.1f} "
                f"（横向力同向 ⇒ 叠加偏航）")

    report_segments(t, opp, args.gap, args.top, "两轮误差反向段", f_opp)

    print(f"\n[4] 余弦优化 align = cos(ref−act)（= 推力方向投影，负值表示已取反向解）")
    for nm, c in (("左", cosL), ("右", cosR)):
        m = c[run]
        print(f"  {nm}舵: 中位 {np.median(m):5.2f}  p5 {np.percentile(m, 5):5.2f}  "
              f"最小 {m.min():5.2f}  取负比例 {100.0 * (m < 0).mean():4.1f}%  "
              f"<0.5 占比 {100.0 * (m < 0.5).mean():4.1f}%")
    # ⚠ 扫速必须算在**归一化后的角差**上：参考是连续角，从 +180° 跳到 −180°（物理上 0°）
    #   时 np.diff 会给出 360°，不 wrap 的话峰值会假报成 180000°/s（= 360°/2ms）。
    ref_rate = np.degrees(np.abs(wrap(np.diff(Lref)))) / dt
    over = run[1:] & (ref_rate > args.slew_deg_s)
    print(f"  指令扫速超舵轮极限({args.slew_deg_s:g}°/s)的帧: {over.sum()}"
          f"（{100.0 * over.sum() / max(run[1:].sum(), 1):.1f}%）"
          f"，峰值 {ref_rate[run[1:]].max():.0f}°/s（单拍最大转过 "
          f"{np.degrees(np.abs(wrap(np.diff(Lref))))[run[1:]].max():.1f}°）")

    # 参考跳变事件（换向判决翻转）：单拍舵角给定转过 > 90°。判据换代后**两轮应在同一拍翻**，
    # 故这里把两轮并排打出来，一眼看是否同拍、是否只是一轮在跳。
    jl = np.degrees(np.abs(wrap(np.diff(Lref)))) > 90.0
    jr = np.degrees(np.abs(wrap(np.diff(Rref)))) > 90.0
    both = np.where(run[1:] & jl & jr)[0]
    only = np.where(run[1:] & (jl ^ jr))[0]
    print(f"  参考跳变(单拍 > 90°)：两轮同拍 {len(both)} 次，仅单轮 {len(only)} 次"
          f"（换向决策是两轮联合的 ⇒ 单轮跳变属异常）")
    for k in np.concatenate([both[:args.top], only[:args.top]]):
        print(f"    t={t[k]:7.3f}  左 {math.degrees(Lref[k]):7.1f}→{math.degrees(Lref[k + 1]):7.1f}"
              f"  右 {math.degrees(Rref[k]):7.1f}→{math.degrees(Rref[k + 1]):7.1f}"
              f"  {'两轮' if (jl[k] and jr[k]) else '单轮'}")

    print(f"\n[5] 余弦系数掉到 {args.cos_low:g} 以下的段（轮面与目标方向对不上 ⇒ 该轮推力被钳到 ~0）")
    for nm, c, rf, ac in (("左", cosL, Lref, Lact), ("右", cosR, Rref, Ract)):
        m = run & (c < args.cos_low)

        def f_cos(s, e, _c=c, _rf=rf, _ac=ac):
            return (f"cos 最小 {_c[s:e + 1].min():.2f} 末端 {_c[e]:.2f}"
                    f" ref={math.degrees(_rf[e]):7.1f}"
                    f" act={math.degrees(_ac[e]):7.1f}"
                    f" err={math.degrees(wrap(_rf[e] - _ac[e])):7.1f}")

        report_segments(t, m, args.gap, args.top, f"{nm}舵对齐失效段", f_cos)

    print(f"\n[6] 目标舵角变迁：只有在 ref 变化 >{args.trace_deg:g}° 时才打一行（换向决策 + 指令切换的完整轨迹）")
    dl = wrap(np.diff(Lref))
    dr = wrap(np.diff(Rref))
    chg_idx = np.where(run[1:] & ((np.abs(np.degrees(dl)) > args.trace_deg)
                                  | (np.abs(np.degrees(dr)) > args.trace_deg)))[0]
    print(f"  共 {len(chg_idx)} 次（RUN 帧 {run.sum()}）")
    for k in chg_idx[:args.top]:
        print(f"    t={t[k] * 1e3:8.1f}ms  L {math.degrees(Lref[k]):7.1f}→{math.degrees(Lref[k + 1]):7.1f}"
              f"   R {math.degrees(Rref[k]):7.1f}→{math.degrees(Rref[k + 1]):7.1f}"
              f"   actL {math.degrees(Lact[k + 1]):7.1f} actR {math.degrees(Ract[k + 1]):7.1f}"
              f"  ΔL {math.degrees(dl[k]):7.1f}")
    if len(chg_idx) > args.top:
        print(f"    … 其余 {len(chg_idx) - args.top} 次省略")

    hit = np.where(np.abs(a[:, args.l_base + OFF_VSPD]) >= 49.9)[0]
    print(f"  左舵速度给定触限幅(±50 电机侧)的帧: {len(hit)}"
          f"（{100.0 * len(hit) / run.sum():.1f}% 的 RUN 帧）")

    if args.dump is not None:
        t0, t1 = args.dump
        sel = np.where((t >= t0) & (t <= t1))[0]
        print(f"\n[dump] t∈[{t0:g},{t1:g}]s 共 {len(sel)} 拍（角度 °，速度给定电机侧 rad/s）")
        print("      t[ms]  st | Lref  Lact  eL   Lrev | Rref  Ract  eR   Rrev | Lvspd Rvspd  cosL cosR")
        lv = a[:, args.l_base + OFF_VSPD]
        rv = a[:, args.r_base + OFF_VSPD]
        for k in sel:
            print(f"  {t[k] * 1e3:9.1f} {int(a[k, args.st]):3d} |"
                  f"{math.degrees(Lref[k]):7.1f}{math.degrees(Lact[k]):7.1f}"
                  f"{math.degrees(eL[k]):7.1f} {'R' if cosL[k] < 0 else ' '}   |"
                  f"{math.degrees(Rref[k]):7.1f}{math.degrees(Ract[k]):7.1f}"
                  f"{math.degrees(eR[k]):7.1f} {'R' if cosR[k] < 0 else ' '}   |"
                  f"{lv[k]:7.1f}{rv[k]:7.1f}{cosL[k]:6.2f}{cosR[k]:6.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
