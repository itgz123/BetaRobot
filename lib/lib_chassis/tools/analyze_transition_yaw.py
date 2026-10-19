#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：平动时"两轮差模 → 可见偏航"的剂量分析（把日志能说和不能说的分开）

问题
----
实机现象：指令纯前后左右（ξ*.w = 0）时车身仍有可见偏航 w。本脚本回答两件事：
  ① 这份日志里**哪些量能指证偏航来源**、哪些看不出来；
  ② 那个来源有多大、占多少时间（剂量）。

加装 IMU 后（34/36 列版日志）
-----------------------------
[8] 段把"车到底转没转"从推断变成直接测量。ch28 是陀螺实测的世界系偏航角速度 ⇒
[4] 里那些"过渡期两轮推力失配"到底有没有变成真实偏航、有多大，一次读完。
⚠ ch28/29 恒为 0 = IMU 没就绪（不是"车没转"）；零偏未标定的日志里 ch28 的**均值**不可当
   真值用 —— [8] 用「静止帧中位」估零偏并给出积分不确定度，同时用 ch29 的航向变化做交叉
   校验（两个零偏估计一致才敢用积分值）。
⚠ ch32/33（两驱动轮实际轮速）顺带解决了 [5] 里"纯平动/纯自旋不可分（mod 180°）"的老问题：
   两轮同号 = 平动型，反号 = 自旋型 —— 判据用轮速符号，不看舵角分支。
⚠ ch34/35（两驱动轮速度给定，36 列版日志才有）让 [10] 能把"命令本身就有差模"与"速度环
   没跟住/打滑"分开；34 列的老日志会在 [10] 里提示跳过。

机理（几何，与分支无关）
    两舵轮回转中心在 (0, ±a) ⇒ 绕 z 力矩 M_z = Σ(x_i·F_iy − y_i·F_ix) = a(F_Rx − F_Lx)。
    **两轮共模**（推力方向同、大小同）⇒ M_z ≡ 0；**只有差模**才产生偏航。故"平动时 w 转"
    只可能来自两轮推力不一致，两类各对应一把尺子（都 branch-free）：
      · 大小差 |A_L − A_R|，A_i = |align_i| = |cos(角度环误差)|（推力余弦因子）——纵向（d≈0/180°）主项
      · 方向差 |wrap_half(act_L − act_R)|——横向（d≈±90°）主项（进 M_z 时被 sin d 加权）
    两把尺子在**过渡期**（舵角没跟到位）最大；稳态两轮都在 ±1° 内、量级解释不了"明显"。

可观测性（先划清楚，别超范围解读）
    · **看不到 w**：本布局没有陀螺，正解 ξ̂ 在 HalfRudderSolve 里算了却没进 VOFA。
    · **分不出纯平动/纯自旋**：舵角给定只给到 d mod 180°，而纯平动（d_L ≡ d_R）与纯自旋
      （d_L − d_R ≡ 180°）在 mod 180° 下同构。能判的只有一边：
        |wrap_half(ref_L − ref_R)| > 门限 ⟹ **必定是平动+自旋混合**（rev 项 mod 180 相消，不需判分支）；
        ≈ 0 ⟹ 纯平动或纯自旋，不可分（[4] 把混合帧单独剔出）。
    · 分支 rev 只有**异或**可观测（sign(sin ref_L)·sign(sin ref_R) < 0 ⟺ rev_L ≠ rev_R，且要求
      |sin d| 大 = 两轮 y 向分量显著）；全局 +180° 标号不可观测 ⇒ 所有"力偶类"结论只报幅值、不报符号。
      本脚本因此**绕开 rev**：只用"两轮平面差 mod 180°"和"余弦因子"，两者都不需要 rev。

指标
    [1] 固件指纹（align 出现负值 = 档2 去钳负）
    [2] 稳态平行度：|wrap_half(act_L − act_R)|（恒定差模 = 零位/装配不对称的证据）
    [3] 过渡窗口 + 双轮/单轮离靶分解（单轮离靶 = 纯差模，直接是偏航源；段长）
    [4] 命令方向分箱（纵向/横向/斜向/混合）下的两把尺子：窗口 vs 稳态
    [5] 命令分类占比（必定混合 / 纯平动或纯自旋不可分）
    [6] 舵向执行速度：实测角速度、贴限幅/饿死、窗口时长（= 偏航剂量的时间尺度）
    [12] IMU 原始量：静止 acc 定安装方向（BMI088 手册 §8.3：朝上的轴静止读 +1g）、
         dot(gyro, 世界向上) 给出不依赖 KF 姿态的物理 yaw_rate，再判"谁与谁反号"
    [13] 平动+无自旋指令：稳态/过渡剂量拆分、|cosδ| 分档（横移无偏航权限）
    [14] 偏航闭环（ch44~46）：修正量饱和率、权限门控覆盖率、cmd.w≈0 时的
         零穿次数（判极限环）、大偏航时正解/物理同号率（判打滑失控）

输入
----
半舵 app 打出的 VOFA+ JustFloat CSV：列 0 = DWT 微秒（**会回绕**），
左舵 1..12、右舵 13..24、ch25 = 状态机、ch26/27 = 两轮 align、ch28~35 = 偏航观测组
（28=陀螺 yaw_rate、29=KF euler.yaw、30=指令 w、31=正解 w、32/33=左右驱动轮实际轮速、
34/35=左右驱动轮速度给定）、ch36~43 = IMU 原始量（36~38=gyro xyz、39~41=acc xyz、
42/43=KF roll/pitch）、ch44~46 = 偏航闭环（44=实测 yaw rate、45=速率误差、46=修正量）。
后加的通道按列号取：28/34/36/44 列的老日志只会跳过对应的段，其余照常。
每轮相对偏移（见 app_chassis.c 的 ChassisVofaRudder）：0=舵角给定(舵轮侧,可超 ±180°)
1=实际(归一化在 ±180°) 2=角度环误差 3=p_out 4=d_out 5=速度给定 6=速度实际 7..10=速度环内部 11=实际电流。

用法
    python3 analyze_transition_yaw.py ../../../ignore/chassis_data/vofa+.csv
    python3 analyze_transition_yaw.py <csv> --off-deg 10 --wheel-y 0.20225
退出码：0 正常；1 输入布局不符。
"""

import argparse
import csv
import math
import sys

import numpy as np

# 每轮 12 路的相对偏移
OFF_REF = 0    # 舵角给定（舵轮侧 rad，= 实测 + 折过分支后的角差；可超 ±π）
OFF_ACT = 1    # 舵角实际（舵轮侧 rad，归一化在 ±π）
OFF_ERR = 2    # 角度环误差（舵轮侧 rad）
OFF_VSPD = 5   # 速度给定（角度环输出，电机侧 rad/s）
OFF_ISPD = 6   # 速度实际（电机侧 rad/s）

PI = math.pi


def wrap_half(x):
    """按 180° 为周期归一（轮面是条线，反向解与原解等价）：(−90°, +90°]"""
    return (np.asarray(x, dtype=float) + PI / 2.0) % PI - PI / 2.0


def wrap_pi(x):
    """按 360° 为周期归一：(−180°, +180°]"""
    return (np.asarray(x, dtype=float) + PI) % (2.0 * PI) - PI


def load(path, l_base, r_base):
    rows = []
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if not row or row[0].lstrip().startswith(("I", "#", "t")):
                continue
            rows.append([float(v) for v in row])
    a = np.asarray(rows, dtype=float)
    need = max(l_base + 12, r_base + 12) - 1
    if a.ndim != 2 or a.shape[1] <= need:
        print(f"✗ 布局不符：需要 ≥{need + 1} 列，实际 {a.shape[1]} 列", file=sys.stderr)
        sys.exit(1)
    return a


def segments(mask):
    """返回 [(起点索引, 长度)]"""
    m = np.asarray(mask, dtype=bool)
    if not m.any():
        return []
    d = np.diff(m.astype(np.int8))
    starts = list(np.flatnonzero(d == 1) + 1)
    ends = list(np.flatnonzero(d == -1) + 1)
    if m[0]:
        starts.insert(0, 0)
    if m[-1]:
        ends.append(len(m))
    return list(zip(starts, [e - s for s, e in zip(starts, ends)]))


def pctl(v, q):
    v = np.asarray(v, dtype=float)
    return float(np.percentile(v, q)) if v.size else float("nan")


def main():
    ap = argparse.ArgumentParser(description="半舵平动偏航（两轮差模）剂量分析")
    ap.add_argument("csv", help="VOFA+ JustFloat CSV")
    ap.add_argument("--l-base", type=int, default=1, help="左舵通道基址（1 = 第 1 列）")
    ap.add_argument("--r-base", type=int, default=13, help="右舵通道基址")
    ap.add_argument("--state", type=int, default=25, help="状态机列")
    ap.add_argument("--run-state", type=int, default=3, help="RUN 态的状态值")
    ap.add_argument("--align-l", type=int, default=26, help="左舵 align 列")
    ap.add_argument("--align-r", type=int, default=27, help="右舵 align 列")
    ap.add_argument("--off-deg", type=float, default=10.0, help="离靶门限 [deg]")
    ap.add_argument("--steady-deg", type=float, default=2.0, help="稳态门限 [deg]")
    ap.add_argument("--mixed-deg", type=float, default=5.0, help="判『必定混合』的 |wrap_half(Δref)| 门限 [deg]")
    ap.add_argument("--axis-deg", type=float, default=30.0, help="纵向/横向分箱半宽 [deg]")
    ap.add_argument("--vmax-motor", type=float, default=50.0, help="舵向速度限幅（电机侧 rad/s）")
    ap.add_argument("--gear", type=float, default=8.0, help="舵向减速比（模型 ω_max = vmax/gear）")
    # 偏航观测组（ch28~33）
    ap.add_argument("--yaw-rate", type=int, default=28, help="IMU yaw_rate 列（0 关掉 [8]）")
    ap.add_argument("--yaw-pos", type=int, default=29, help="IMU euler.yaw 列")
    ap.add_argument("--cmd-w", type=int, default=30, help="指令 w 列")
    ap.add_argument("--rep-w", type=int, default=31, help="正解 w 列")
    ap.add_argument("--wv-l", type=int, default=32, help="左驱动轮实际轮速列")
    ap.add_argument("--wv-r", type=int, default=33, help="右驱动轮实际轮速列")
    ap.add_argument("--wv-l-ref", type=int, default=34, help="左驱动轮速度给定列（0 = 关掉 [10]）")
    ap.add_argument("--wv-r-ref", type=int, default=35, help="右驱动轮速度给定列")
    ap.add_argument("--rest-v", type=float, default=0.5, help="判『车没动』的轮速门限 [rad/s]（取零偏用）")
    ap.add_argument("--class-v", type=float, default=0.5, help="判『在动』的轮速门限 [rad/s]（分平动/自旋用）")
    ap.add_argument("--cmd-w-quiet", type=float, default=0.02, help="判『没下自旋指令』的 |cmd.w| 门限 [rad/s]")
    ap.add_argument("--gap-x", type=float, default=4.0, help="间隔 > N×中位间隔 = 丢帧空洞，积分时跳过")
    ap.add_argument("--wheel-r", type=float, default=0.05025, help="驱动轮半径 [m]（CHASSIS_WHEEL_RADIUS）")
    ap.add_argument("--wheel-y", type=float, default=0.20225, help="舵轮回转中心到车中心距离 |y| [m]")
    ap.add_argument("--spin-dump", action="store_true", help="[11] 逐帧打印纯自旋段（排查符号翻转的瞬态）")
    # IMU 原始量（ch36~43，2026-10-17 加；老日志自动跳过 [12]）
    ap.add_argument("--gyro-x", type=int, default=36, help="机体系陀螺 x 列（0 = 关掉 [12]）")
    ap.add_argument("--gyro-y", type=int, default=37, help="机体系陀螺 y 列")
    ap.add_argument("--gyro-z", type=int, default=38, help="机体系陀螺 z 列")
    ap.add_argument("--acc-x", type=int, default=39, help="机体系加速度 x 列")
    ap.add_argument("--acc-y", type=int, default=40, help="机体系加速度 y 列")
    ap.add_argument("--acc-z", type=int, default=41, help="机体系加速度 z 列")
    ap.add_argument("--roll", type=int, default=42, help="KF roll 列")
    ap.add_argument("--pitch", type=int, default=43, help="KF pitch 列")
    # 偏航闭环（ch44~46，2026-10-17 加；老日志自动跳过 [14]）
    ap.add_argument("--loop-rate", type=int, default=44, help="闭环实测 yaw rate 列（0 = 关掉 [14]）")
    ap.add_argument("--loop-err", type=int, default=45, help="闭环速率误差列")
    ap.add_argument("--loop-corr", type=int, default=46, help="闭环修正量列")
    ap.add_argument("--yaw-authority", type=float, default=0.5,
                    help="权限门控：两轮 |cos(舵角)| 都 > 它才算有权限（配合固件 CHASSIS_YAW_MIN_AUTHORITY）")
    args = ap.parse_args()

    a = load(args.csv, args.l_base, args.r_base)
    n_raw = len(a)

    # ---------------- 行级清洗（入口统一做，别让垃圾行污染后面每个统计） ----------------
    # VOFA 的 JustFloat 是纯 4 字节浮点流：串口上丢/多一个字节，之后按 4 字节切出来的
    # 就是垃圾浮点 —— 表现三种：NaN、1e36 级溢出、时间戳突然归零或回退。
    # 不清掉它们，一个 1e36 就能把"中位/p95"污染成 nan、把积分算成天文数字。
    fin = np.isfinite(a).all(axis=1)
    sane = (np.abs(a[:, 1:]).max(axis=1) < 1e4) & (a[:, 0] >= 0.0) & (a[:, 0] < 1e9)
    # 时间戳单调：只收"比上一个被接受的时间戳前进且前进量 < 0.5s"的行（回退 = 失步行）。
    # 本机时间戳只有 1e6~1e8 µs，远不到 32 位 µs 回绕（4295s），故不需要 unwrap。
    keep = fin & sane
    mono = np.zeros(n_raw, dtype=bool)
    prev = None
    for i in range(n_raw):
        if not keep[i]:
            continue
        if prev is None or (a[i, 0] > prev and a[i, 0] - prev < 5e5):
            mono[i] = True
            prev = a[i, 0]
    a = a[mono]
    clean_stats = (n_raw, int((~fin).sum()), int((fin & ~sane).sum()), int((fin & sane).sum() - mono.sum()))
    if len(a) < 10:
        print(f"✗ 清洗后只剩 {len(a)} 行，无法分析", file=sys.stderr)
        return 1
    t = (a[:, 0] - a[0, 0]) * 1e-6
    dt = float(np.median(np.diff(t)))
    fs = 1.0 / dt

    L = a[:, args.l_base:args.l_base + 12]
    R = a[:, args.r_base:args.r_base + 12]
    actL, actR = L[:, OFF_ACT], R[:, OFF_ACT]
    errL, errR = L[:, OFF_ERR], R[:, OFF_ERR]
    refL, refR = L[:, OFF_REF], R[:, OFF_REF]
    run = a[:, args.state] == args.run_state
    if not run.any():
        print(f"✗ 没有状态值为 {args.run_state} 的帧（--run-state 设错？）", file=sys.stderr)
        return 1

    print(f"文件 {args.csv}：{len(t)} 帧 / {t[-1]:.2f} s（{fs:.1f} Hz），"
          f"RUN 态 {run.sum()} 帧（{100.0 * run.mean():.1f}%）")
    print(f"门限：离靶 >{args.off_deg:g}°、稳态 <{args.steady_deg:g}°、"
          f"混合 >{args.mixed_deg:g}°；模型 ω_max = {args.vmax_motor:g}/{args.gear:g} = "
          f"{math.degrees(args.vmax_motor / args.gear):.0f}°/s（舵轮侧）")

    # ---------------- [0] 数据体检（清洗报告 / 丢帧空洞） ----------------
    # 帧是 2ms 一发的，PC 端 VOFA+ 会丢掉大部分帧 ⇒ 采样点是**不规则**的：
    # 相邻对的 dt 是真实经过时间，必须用它积分；跨空洞的积分只能跳过（那段轨迹未知）。
    dts = np.diff(t)
    gap = dts > args.gap_x * dt
    print(f"\n[0] 数据体检（入口清洗 + 丢帧空洞）")
    print(f"    行级清洗：{clean_stats[0]} → {len(a)} 行（丢 {100.0 * (1 - len(a) / clean_stats[0]):.1f}%）："
          f"NaN/±Inf {clean_stats[1]} 行、量纲溢出(>1e4 或时间戳离谱) {clean_stats[2]} 行、"
          f"时间戳回退/跳变 {clean_stats[3]} 行")
    print(f"    ⚠ 上面三类都是 VOFA JustFloat **字节失步**（丢/多一个字节后按 4 字节切出的垃圾浮点）"
          "，不是固件算出来的值")
    odd = dts[dts < 1e-3]                                   # <1ms 的相邻对：时间戳带小数/重复
    small = dts[(dts >= 1e-3) & (dts < args.gap_x * dt)]
    u, c = np.unique(np.round(small, 3), return_counts=True)   # 1ms 粒度找众数（small 单位是秒）
    tp = float(u[int(np.argmax(c))]) if len(u) else dt      # 最小间隔的众数 = 固件发送周期
    if odd.size:
        print(f"    ⚠ 相邻 <1ms 的帧 {odd.size} 对（时间戳带小数或重复帧）⇒ 发送周期取众数时已排除这些")
    print(f"    固件发送周期（最小间隔的众数）：{tp * 1e3:.2f}ms = {1.0 / tp:.0f}Hz"
          f"（{100.0 * np.mean(np.abs(small - tp) < 100):.0f}% 的相邻对就是这个周期）")
    print(f"    帧间隔：中位 {dt * 1e3:.2f}ms（{fs:.0f}Hz 是**日志侧**的有效率，不是发送率）"
          f" p95 {pctl(dts, 95) * 1e3:.2f}ms max {dts.max() * 1e3:.1f}ms；"
          f"> {args.gap_x:g}× 中位的空洞 {int(gap.sum())} 处、合计 {dts[gap].sum():.2f}s"
          f"（占全程 {100.0 * dts[gap].sum() / max(t[-1], 1e-9):.1f}%）")
    print(f"    采样点 {len(t)} 个 / 全程 {t[-1]:.2f}s ⇒ PC 端只记下约 "
          f"{100.0 * len(t) * tp / max(t[-1], 1e-9):.0f}% 的帧，且成串（缓冲满→丢一批→再收一串）")
    print(f"    ⚠ 采样**不规则**：过渡窗口（几十 ms）常常只落到 1~3 个点上，别在窗口内做逐拍微分；"
          f"积分必须用真实 dt 并跳过空洞（本脚本已这样做）")

    # ---------------- [1] 固件指纹 ----------------
    alnL, alnR = a[:, args.align_l], a[:, args.align_r]
    neg = (alnL < 0) | (alnR < 0)
    dmis = np.maximum(np.abs(np.cos(errL) - alnL), np.abs(np.cos(errR) - alnR))
    print(f"\n[1] 固件指纹：align 出现负值 {neg.sum()} 帧（{100.0 * neg.mean():.2f}%）"
          f"→ {'档2（已去钳负）✓' if neg.any() else '旧版（钳在 0）✗ 这份日志不是档2'}")
    print(f"    align 与 cos(角度环误差) 的偏差：超 0.01 的帧 {int((dmis > 0.01).sum())}"
          f"（{100.0 * float((dmis > 0.01).mean()):.1f}%）、max {dmis.max():.2e}"
          f"（应为 0：align = cos(给定−实测)，而给定 = 实测 + 该误差；"
          f"非 0 = ch26/27 与角度环误差不同拍，过渡期会错一拍）")

    # ---------------- [2] 稳态平行度 ----------------
    # 轮面是条线，两轮"平行"的判据是 actL ≡ actR (mod 180°)，与分支无关
    para = np.degrees(np.abs(wrap_half(actL - actR)))
    offL = np.abs(errL) > math.radians(args.off_deg)
    offR = np.abs(errR) > math.radians(args.off_deg)
    dspan0 = np.degrees(np.abs(wrap_half(refL - refR)))  # > 门限 ⇒ 平动+自旋混合（非平行是设计如此）
    pure = run & (dspan0 <= args.mixed_deg)
    steady = pure & ~offL & ~offR                      # 只看纯平动/纯自旋：稳态应严格平行
    print(f"\n[2] 两轮平行度 |wrap_half(actL−actR)|（稳态帧 = 纯平动/纯自旋 且两轮离靶都在 "
          f"{args.steady_deg:g}° 内；混合指令剔除，它的非平行是设计如此）")
    print(f"    稳态帧 {steady.sum()} 帧（{100.0 * steady.sum() / run.sum():.1f}% of RUN）："
          f"中位 {pctl(para[steady], 50):.2f}°  p95 {pctl(para[steady], 95):.2f}°  "
          f"max {para[steady].max():.2f}°")
    for th in (1.0, 2.0, 5.0):
        print(f"    > {th:g}° 的稳态帧占比：{100.0 * np.mean(para[steady] > th):.1f}%"
              f"（恒定差模在 {th:g}° 以上的时间；这部分是零位/装配/负载不对称，不是过渡）")

    # ---------------- [3] 过渡窗口分解 ----------------
    n = int(run.sum())
    both = int((run & offL & offR).sum())
    onlyL = int((run & offL & ~offR).sum())
    onlyR = int((run & offR & ~offL).sum())
    print(f"\n[3] 过渡窗口（RUN 内，某轮离靶 > {args.off_deg:g}°，合计 {100.0 * (both + onlyL + onlyR) / n:.1f}%）")
    print(f"    两轮同时离靶（共模，本身不产生 M_z）      ：{both} 帧（{100.0 * both / n:.1f}%）")
    print(f"    只有左轮离靶（纯差模，直接是偏航源）      ：{onlyL} 帧（{100.0 * onlyL / n:.1f}%）")
    print(f"    只有右轮离靶（纯差模）                    ：{onlyR} 帧（{100.0 * onlyR / n:.1f}%）")
    for nm, m in (("两轮同时离靶", run & offL & offR), ("仅左轮离靶", run & offL & ~offR),
                  ("仅右轮离靶", run & offR & ~offL)):
        segs = segments(m)
        if not segs:
            continue
        dur = np.array([s[1] for s in segs]) * dt
        print(f"    {nm}段：{len(segs)} 段，中位 {np.median(dur) * 1e3:.0f}ms  "
              f"p95 {np.percentile(dur, 95) * 1e3:.0f}ms  max {dur.max() * 1e3:.0f}ms  "
              f"总 {dur.sum():.2f}s")
    print("    ⚠ 只有一轮离靶的段中位只有几毫秒（= 另一轮刚跨过门限），说明档2 的"
          "『两轮都不磨盘』保住了；下面 [4] 才是偏航的主项：两轮都离靶但**幅度不等**。")

    # ---------------- [4] 命令方向分箱下的两把尺子 ----------------
    dmod = np.degrees(np.abs(wrap_half(refL)))           # 指令方向 mod 180°（rev 项 mod 180 相消）
    mixed = run & (dspan0 > args.mixed_deg)              # [2] 里已算
    AL, AR = np.abs(alnL), np.abs(alnR)                  # 推力余弦因子（= |cos(离靶角)|，branch-free）
    dmag = np.abs(AL - AR)                              # 尺子① 推力大小失配（纵向主项）
    ddir = para                                         # 尺子② 两轮平面差（横向主项，M_z 里被 sin d 加权）
    bins = [("纵向 |d|<30°", pure & ((dmod < args.axis_deg) | (dmod > 180.0 - args.axis_deg))),
            ("横向 |d−90°|<30°", pure & (np.abs(dmod - 90.0) < args.axis_deg)),
            ("斜向（其余纯平动/纯自旋）", pure & (dmod >= args.axis_deg) & (dmod <= 180.0 - args.axis_deg)
             & (np.abs(dmod - 90.0) >= args.axis_deg)),
            ("必定混合（平动+自旋）", mixed)]
    print(f"\n[4] 两把差模尺子，按指令方向分箱（纵向看大小差、横向看方向差）")
    for nm, m in bins:
        wm = m & (offL | offR)
        sm = m & ~offL & ~offR
        if m.sum() == 0:
            continue
        print(f"  · {nm}：{m.sum()} 帧（占 RUN {100.0 * m.sum() / n:.1f}%）"
              f"，其中过渡窗口 {100.0 * wm.sum() / max(m.sum(), 1):.1f}%")
        if wm.any():
            print(f"      窗口内 大小差 |A_L−A_R|：中位 {pctl(dmag[wm], 50):.3f} "
                  f"p95 {pctl(dmag[wm], 95):.3f}（1.0 = 一轮满推另一轮不推）")
            print(f"      窗口内 方向差 |Δact|    ：中位 {pctl(ddir[wm], 50):.2f}° "
                  f"p95 {pctl(ddir[wm], 95):.2f}°  max {ddir[wm].max():.2f}°"
                  f"（sin 后进 M_z 的系数：中位 {math.sin(math.radians(pctl(ddir[wm], 50))):.3f} "
                  f"p95 {math.sin(math.radians(pctl(ddir[wm], 95))):.3f}）")
        if sm.any():
            print(f"      稳态对照 大小差 中位 {pctl(dmag[sm], 50):.4f}、"
                  f"方向差 中位 {pctl(ddir[sm], 50):.2f}°（p95 {pctl(ddir[sm], 95):.2f}°）")

    # ---------------- [5] 命令分类 ----------------
    print(f"\n[5] 命令分类（RUN {n} 帧；rev 项 mod 180° 相消，故本判据不需判分支）")
    print(f"    必定是平动+自旋混合（|wrap_half(Δref)| > {args.mixed_deg:g}°）："
          f"{mixed.sum()} 帧（{100.0 * mixed.sum() / n:.1f}%）")
    print(f"    纯平动或纯自旋（不可分）：{pure.sum()} 帧（{100.0 * pure.sum() / n:.1f}%）"
          f"；其中指令方向贴 x 轴（d≈0/180°，横移与横滚同构）{100.0 * np.mean((dmod < 15) | (dmod > 165)):.1f}%")
    print(f"    ⚠ 要真正分开纯平动与纯自旋、并看到 w 本身，只能加通道（见 [6] 尾注）")

    # ---------------- [6] 舵向执行速度 ----------------
    wmax = args.vmax_motor / args.gear
    vspd, ispd = L[:, OFF_VSPD], L[:, OFF_ISPD]
    rvspd, rispd = R[:, OFF_VSPD], R[:, OFF_ISPD]
    sat = run & (np.abs(vspd) > 0.98 * args.vmax_motor)
    starve = sat & (np.abs(ispd) < 0.6 * np.abs(vspd))
    satr = run & (np.abs(rvspd) > 0.98 * args.vmax_motor)
    starver = satr & (np.abs(rispd) < 0.6 * np.abs(rvspd))
    print(f"\n[6] 舵向执行速度（偏航剂量的时间尺度：过渡窗口有多长就积多久）")
    for nm, act, spd in (("左", actL, ispd), ("右", actR, rispd)):
        w = np.degrees(np.abs(wrap_pi(np.diff(act)))) / dt
        w = w[run[1:]]
        v = np.abs(spd[run])
        print(f"    {nm}舵：实测 |速度| 中位 {np.median(v):.2f} rad/s（{math.degrees(np.median(v)):.0f}°/s 电机侧）"
              f"  p95 {pctl(v, 95):.2f}（{math.degrees(pctl(v, 95)):.0f}°/s）")
        print(f"          实测角速度（舵轮侧）：中位 {np.median(w):.0f}°/s  p95 {pctl(w, 95):.0f}°/s  "
              f"p99.9 {pctl(w, 99.9):.0f}°/s（模型 {math.degrees(wmax):.0f}°/s）")
    print(f"    速度给定贴限幅：左 {100.0 * sat.sum() / n:.1f}%  右 {100.0 * satr.sum() / n:.1f}%"
          f"；其中『贴限幅却跑不到 60%』（饿死）：左 {100.0 * starve.sum() / n:.1f}%  "
          f"右 {100.0 * starver.sum() / n:.1f}%")
    print("    尾注：要闭环回答『平动时 w 有多少、谁贡献的』，建议加 2~4 路通道——"
          "cmd.w（指令偏航）、odom.w（正解 ξ̂ 的偏航，HalfRudderSolve 里现成）、"
          "两驱动轮实际速度（横向推力失配的直接证据）。")

    # ---------------- [7] 舵轮转不动？给定量已饱和段里的实测速率 ----------------
    # 角度环 kp=50、输出限幅 ±50 电机侧 ⇒ |离靶| > ~8°(舵轮侧) 给定量必饱和；
    # 此时若实测速率远低于 358°/s，瓶颈在速度环（给不出力），不在决策、也不在限幅本身。
    print(f"\n[7] 给定量已饱和（角度环输出贴 ±{args.vmax_motor:g}）时，舵轮实际转多快？")
    print(f"    分档看 |离靶角| 与 驱动余弦因子 |align|（align 大 = 驱动轮正在出力，"
          f"轮胎侧刮擦给舵轴的反载也大）")
    wl = np.degrees(np.abs(wrap_pi(np.diff(actL)))) / dt
    wr = np.degrees(np.abs(wrap_pi(np.diff(actR)))) / dt
    jumpOK = np.ones(len(t), dtype=bool)
    jumpOK[1:] = (np.abs(np.diff(actL)) < 0.05) & (np.abs(np.diff(actR)) < 0.05)  # 剔反馈跳变
    for nm, err, spd, rate, aln in (("左", errL, vspd, wl, alnL), ("右", errR, rvspd, wr, alnR)):
        sat = run & jumpOK & (np.abs(spd) > 0.98 * args.vmax_motor)
        e = np.degrees(np.abs(err))[:len(rate)]
        m = sat[1:]
        print(f"  · {nm}舵（饱和帧 {m.sum()}，占 RUN {100.0 * m.sum() / n:.1f}%）：")
        hdr = "      按 |离靶角| 分档："
        row = "        实测速率中位 [°/s]："
        for lo, hi in ((8, 20), (20, 45), (45, 90), (90, 180)):
            sel = m & (e >= lo) & (e < hi)
            row += f"{pctl(rate[sel], 50):7.0f} ({sel.sum():5d})" if sel.any() else f"{'—':>7s} ({0:5d})"
        print(f"{hdr} 8~20° 20~45° 45~90° 90~180°（模型 {math.degrees(wmax):.0f}°/s）")
        print(row)
        row2 = "      按 |align| 分档："
        al = np.abs(aln)
        for lo, hi in ((0.0, 0.3), (0.3, 0.7), (0.7, 0.99), (0.99, 1.01)):
            sel = m & (al[:len(rate)] >= lo) & (al[:len(rate)] < hi)
            row2 += f"{pctl(rate[sel], 50):7.0f} ({sel.sum():5d})" if sel.any() else f"{'—':>7s} ({0:5d})"
        print("      |align| 0~0.3 0.3~0.7 0.7~1.0 ≈1.0 => " + row2)
    print("    读法：饱和段实测速率若远低于模型值，且 |align| 越大越慢，则瓶颈是"
          "『舵向速度环给不出力（+驱动轮侧刮擦反载）』，不是决策、也不是速度限幅本身。")

    # ---------------- [8] 偏航观测组（ch28~33）：直接测"转了没、转多少" ----------------
    need = max(args.yaw_rate, args.yaw_pos, args.cmd_w, args.rep_w, args.wv_l, args.wv_r)
    if args.yaw_rate <= 0:
        print("\n[8] 偏航观测组：已用 --yaw-rate 0 关闭")
        return 0
    if a.shape[1] <= need:
        print(f"\n[8] 偏航观测组：本日志只有 {a.shape[1]} 列（需要 ≥{need + 1}）"
              "——是加通道之前的老日志，跳过")
        return 0

    gyro = a[:, args.yaw_rate]
    yawraw = a[:, args.yaw_pos]
    cmdw = a[:, args.cmd_w]
    repw = a[:, args.rep_w]
    vL = a[:, args.wv_l]
    vR = a[:, args.wv_r]
    ok = ~np.isnan(gyro) & ~np.isnan(yawraw) & ~np.isnan(vL) & ~np.isnan(vR) & ~np.isnan(cmdw)

    print(f"\n[8] 偏航观测组：车真的转了没、转了多少（偏航 = ch28 陀螺实测）")
    zero = ok & (gyro == 0.0)
    print(f"    IMU 就绪检查：ch28 恒 0 的帧 {int(zero.sum())}（{100.0 * zero.sum() / max(ok.sum(), 1):.1f}%）"
          f"→ {'⚠ 基本没数据/没就绪，下面全是噪声' if zero.mean() > 0.95 else '有数据 ✓'}"
          f"；ch28 中位 |值| {np.nanmedian(np.abs(gyro[ok])):.4f} rad/s")

    # ---- 零偏：静止帧中位（未标定的日志只能这样估） ----
    rest = ok & (np.abs(vL) < args.rest_v) & (np.abs(vR) < args.rest_v)
    if rest.sum() >= 50:
        bias_rest = float(np.nanmedian(gyro[rest]))
        noise = float(np.nanstd(gyro[rest]))
        src = f"静止帧（轮速 <{args.rest_v:g} rad/s）中位，n={int(rest.sum())}"
    else:
        bias_rest = float(np.nanmedian(gyro[ok]))
        noise = float(np.nanstd(gyro[ok]))
        src = f"⚠ 静止帧只有 {int(rest.sum())} 帧，退回全程中位（可能把真实转动当零偏）"
    g0 = gyro - bias_rest

    # ---- 交叉校验的**正确**做法：ch29 是固件每拍积出来的（权威），日志里只能拿着抽到的
    #      点积分。抽样只覆盖约 1/4 时间 ⇒ 直接比 ∫ 会系统性偏小，不能当零偏估计用。 ----
    yawu = np.unwrap(wrap_pi(yawraw))
    span = t[-1] - t[0]
    dt_all = np.diff(t)
    dt_all = np.where(dt_all > args.gap_x * dt, np.nan, dt_all)  # 空洞不积分
    valid = ok[1:] & ok[:-1]
    cover = float(np.nansum(np.where(valid, dt_all, np.nan))) / max(span, 1e-9)
    dyaw = float(yawu[-1] - yawu[0])
    gint = float(np.nansum(np.where(valid, gyro[1:] * np.where(np.isnan(dt_all), 0.0, dt_all), 0.0)))

    print(f"    零偏 ch28 均值：{bias_rest:+.4f} rad/s（{math.degrees(bias_rest):+.2f}°/s，{src}）")
    print(f"    静止段噪声：std {math.degrees(noise):.2f}°/s（逐拍，未滤波）"
          f" → 逐帧用它判『转没转』时，小于 {math.degrees(2 * noise):.1f}°/s 的差别不要当信号")
    print(f"    ch29（固件每拍积分，权威）全程航向变化 {math.degrees(dyaw):+.1f}°；"
          f"日志抽样只覆盖 {100.0 * cover:.0f}% 的时间（∫陀螺 = {math.degrees(gint):+.1f}°）"
          f"⇒ 两者不可直接比，别从它们反推零偏")

    # ---- 平动/自旋分类（用驱动轮轮速符号，branch-free，补上 [5] 的分不开） ----
    moving = ok & (np.abs(vL) > args.class_v) & (np.abs(vR) > args.class_v)
    co = moving & (vL * vR > 0)     # 两轮同向 = 平动型（含转向）
    anti = moving & (vL * vR < 0)   # 两轮反向 = 自旋型
    quiet_cmd = ok & (np.abs(cmdw) < args.cmd_w_quiet)
    print(f"    平动/自旋分类（ch32/33 轮速符号，不受舵角 mod 180° 影响）："
          f"平动型 {int(co.sum())} 帧（{100.0 * co.sum() / max(moving.sum(), 1):.1f}% of 运动帧）、"
          f"自旋型 {int(anti.sum())} 帧、轮太慢不可判 {int((ok & ~moving).sum())} 帧")

    # ---- 核心：没下自旋指令（|cmd.w|≈0）时，车实际转过多少 ----
    trans = co & quiet_cmd
    print(f"    ★ 平动型 且 |指令 w|<{args.cmd_w_quiet:g} rad/s：{int(trans.sum())} 帧"
          f"（{100.0 * trans.sum() / max(co.sum(), 1):.1f}% of 平动型）")
    if trans.sum() >= 20:
        print(f"      ch28 去零偏后：中位 |w| {pctl(np.abs(g0[trans]), 50) * 1000:.1f} mrad/s "
              f"（{math.degrees(pctl(np.abs(g0[trans]), 50)):.2f}°/s）"
              f"  p95 {math.degrees(pctl(np.abs(g0[trans]), 95)):.2f}°/s"
              f"  max {math.degrees(np.abs(g0[trans]).max()):.2f}°/s")
        # 只统计与运动段连着的窗口：这里的"诱导偏航剂量"= ∫|w|dt（走直线时本该是 0）
        trans_i = np.flatnonzero(trans)
        # 把"平动型且无自旋指令"的帧各自累加 |w|dt（跨空洞不积）
        inc = np.zeros(len(t))
        m = trans[1:] & trans[:-1]
        inc[1:] = np.where(m, np.abs(g0[1:]) * np.where(np.isnan(dt_all), 0.0, dt_all), 0.0)
        cum = np.cumsum(inc)
        segs = segments(trans)
        dur = np.array([s[1] for s in segs]) * dt
        print(f"      平动段（{len(segs)} 段，中位 {np.median(dur) * 1e3:.0f}ms 总 {dur.sum():.2f}s）："
              f"段内 |w| 积分合计 {math.degrees(cum[-1]):.2f}° "
              f"= 全程平均 {math.degrees(cum[-1]) / max(dur.sum(), 1e-9):.1f}°/s 的持续偏航")
        print(f"      判据：静止段噪声 std {math.degrees(noise):.2f}°/s ⇒ 平动段中位 |w| 若高出它"
              f"一个量级，就是**实测坐实的持续偏航**，不是零偏/噪声（本档 "
              f"{pctl(np.abs(g0[trans]), 50) / max(noise, 1e-9):.0f}×）")
        # 按 [4] 的两把尺子分档：实测偏航跟不跟差模走
        print(f"      实测 |w| 跟不跟差模走（同帧对照，平动段内）：")
        AL_, AR_ = np.abs(alnL), np.abs(alnR)
        dmag_ = np.abs(AL_ - AR_)
        ddir_ = para
        for lo, hi in ((0.0, 0.02), (0.02, 0.1), (0.1, 0.3), (0.3, 1.01)):
            sel = trans & (dmag_ >= lo) & (dmag_ < hi)
            if sel.sum() >= 5:
                print(f"        |A_L−A_R| ∈ [{lo:g},{hi:g})：{int(sel.sum()):5d} 帧，"
                      f"实测 |w| 中位 {math.degrees(pctl(np.abs(g0[sel]), 50)):6.2f}°/s")
        for lo, hi in ((0.0, 2.0), (2.0, 5.0), (5.0, 15.0), (15.0, 181.0)):
            sel = trans & (ddir_ >= lo) & (ddir_ < hi)
            if sel.sum() >= 5:
                print(f"        方向差 |Δact| ∈ [{lo:g},{hi:g})°：{int(sel.sum()):5d} 帧，"
                      f"实测 |w| 中位 {math.degrees(pctl(np.abs(g0[sel]), 50)):6.2f}°/s")
    else:
        print("      帧数不足，跳过（这段日志里没有『两轮同向出力且没下自旋指令』的片段）")

    # ---- 三种 w 互相对照（含 ch28 符号方向是否装反） ----
    spin_cmd = ok & (np.abs(cmdw) > 0.1)
    if spin_cmd.sum() >= 20:
        same = float(np.mean(np.sign(g0[spin_cmd]) == np.sign(cmdw[spin_cmd])))
        print(f"    有自旋指令帧（|cmd.w|>0.1，n={int(spin_cmd.sum())}）："
              f"ch28 与 ch30 同号率 {100.0 * same:.1f}% → "
              f"{'符号一致 ✓（IMU z 与车体系同向）' if same > 0.8 else ('⚠ 大多反号 ⇒ IMU 装反/z 轴反向，ch28 要取负' if same < 0.2 else '⚠ 同号率居中：指令幅度太小或零偏没扣干净，别下符号结论')}")
        print(f"      该段 |ch28| 中位 {math.degrees(pctl(np.abs(g0[spin_cmd]), 50)):.2f}°/s、"
              f"|ch30 指令| 中位 {math.degrees(pctl(np.abs(cmdw[spin_cmd]), 50)):.2f}°/s"
              f"、|ch31 正解| 中位 {math.degrees(pctl(np.abs(repw[spin_cmd]), 50)):.2f}°/s"
              "（三者同量级 = 指令/正解/实测自洽）")
    repw_quiet = ok & (np.abs(repw) < args.cmd_w_quiet) & (np.abs(cmdw) > 0.1)
    if repw_quiet.sum() >= 10:
        print(f"    ⚠ ch31 正解 w 与 ch30 指令 w 长期不符：{int(repw_quiet.sum())} 帧"
              "（指令有自旋、正解报 0）⇒ 要么 HalfRudderSolve 的 w 没接线，要么指令被上层清零")
    print("    读法：① 平动段平均 |w| 明显大于零偏不确定度 ⇒ 实测坐实『平动在转』，再看 [4] 两把尺子哪把"
          "跟它相关；② ch28 与 ch31 不符 ⇒ 轮面不侧滑前提不成立（侧滑），ch32/33 给出是哪一轮；"
          "③ 零偏未标定 ⇒ 只信『同时段相对比较』，别信 ch29 的绝对航向。")

    # ---------------- [9] 陀螺 vs ch31 正解：两条独立链路的对撞 ----------------
    # ch31 是**底盘自己**按"轮面不侧滑"从实测轮速+舵角算出的 w（正解，含全部分支决策），
    # ch28 是 MEMS 陀螺。两者硬件、算法、误差源完全不重叠 ⇒ 它们一致 = 观测可信；
    # 它们不一致的**方式**本身带信息（低端陀螺有噪声底、高端是轮胎跟不上＝侧滑）。
    # 这比 [9b] 里我自己按几何重推的轮速差干净：那条要判 cos δ、而 act 在日志里归一化在
    # ±180°，符号天然有歧义（见下），只能比幅值。
    both = ok & (np.abs(repw) > 1e-3) & (np.abs(g0) > 1e-3)
    print(f"\n[9] 陀螺(ch28) vs 底盘正解(ch31)：两条独立链路对撞（两者都非零帧 n={int(both.sum())}）")
    if both.sum() >= 30:
        print(f"    相关 r = {float(np.corrcoef(g0[both], repw[both])[0, 1]):+.3f}；"
              f"同号率 {100.0 * float(np.mean(np.sign(g0[both]) == np.sign(repw[both]))):.0f}%"
              "（符号不一致先怀疑 IMU z 装反/取反，再看下面分档）")
        print(f"    整体：|ch28| 中位 {math.degrees(pctl(np.abs(g0[both]), 50)):.1f}°/s vs "
              f"|ch31| {math.degrees(pctl(np.abs(repw[both]), 50)):.1f}°/s"
              f"（比值 {pctl(np.abs(g0[both]), 50) / pctl(np.abs(repw[both]), 50):.2f}，≈1 = 刻度可信）")
        print("    按 |ch31| 分档（低端看陀螺噪声底、高端看是否跟不上）：")
        for lo, hi in ((0.0, 5.0), (5.0, 20.0), (20.0, 60.0), (60.0, 1e9)):
            sel = both & (np.degrees(np.abs(repw)) >= lo) & (np.degrees(np.abs(repw)) < hi)
            if sel.sum() >= 8:
                same = 100.0 * float(np.mean(np.sign(g0[sel]) == np.sign(repw[sel])))
                print(f"      |ch31| ∈ [{lo:g},{hi:g})°：n={int(sel.sum()):4d}  ch31 中位 "
                      f"{math.degrees(pctl(np.abs(repw[sel]), 50)):6.1f}°  ch28 中位 "
                      f"{math.degrees(pctl(np.abs(g0[sel]), 50)):6.1f}°  比值 "
                      f"{pctl(np.abs(g0[sel]), 50) / max(pctl(np.abs(repw[sel]), 50), 1e-9):.2f}"
                      f"  同号 {same:.0f}%")
        # 符号专项：只在**大幅值**档下结论。低幅值档被陀螺噪声/残余零偏主导（|ch31| 5°/s
        # 以下噪声就能翻符号），拿它定符号会把 61% 这种数当"装反"，其实只是没信号。
        big = both & (np.degrees(np.abs(repw)) >= 20.0)
        if big.sum() >= 20:
            same_big = float(np.mean(np.sign(g0[big]) == np.sign(repw[big])))
            print(f"    符号专项（|ch31| ≥ 20°/s，n={int(big.sum())}）：同号率 "
                  f"{100.0 * same_big:.0f}% → "
                  f"{'ch28 与车体系同向（符号 +1）' if same_big > 0.9 else ('⚠ ch28 与车体系反向（符号 −1）' if same_big < 0.1 else '⚠ 大幅值档仍不一致：先查侧滑/舵角分支，别下符号结论')}")
        q = both & (np.abs(cmdw) < args.cmd_w_quiet)
        if q.sum() >= 20:
            print(f"    ★ 指令 w≡0 却两者都非零的帧 {int(q.sum())}：ch28 中位 "
                  f"{math.degrees(pctl(np.abs(g0[q]), 50)):.1f}°/s、ch31 中位 "
                  f"{math.degrees(pctl(np.abs(repw[q]), 50)):.1f}°/s"
                  "（**两条独立链路都报『没下自旋指令却在转』＝偏航是真的**）")

    # ---------------- [9b] 陀螺 vs 我自己的轮速差几何（副证，只比幅值） ----------------
    # 两个舵轮回转中心在 (0, ±a)、两轮共面 δ ⇒ 刚体运动学 v_i = (v_x ∓ a·w)cos δ + v_y sin δ，
    # 相减得 **w = (v_R − v_L)·R / (2a·cos δ)**（δ=0/180° 时才退化成不带 cos 的版本；
    # δ≈90°（横移）时轮速里根本没有偏航信息，公式发散 —— 分档里的 >100°/s 多半是这种情况）。
    # ⚠ 日志里 act 归一化在 ±180°，无法区分 u 与 −u ⇒ **符号有歧义**，只比幅值。
    cosd = np.cos((actL + actR) / 2.0)
    w_wheel = (vR - vL) * args.wheel_r / (2.0 * args.wheel_y * cosd)
    # 只在几何成立处比：两轮共面（|Δact| 小）且轮面贴 x（|cos δ| 大）；横移/斜行的帧整段丢掉
    valid = ok & (np.abs(wrap_half(actL - actR)) < math.radians(10.0)) & (np.abs(cosd) > 0.9)
    moving_all = ok & (np.abs(vL) > args.class_v / 2) & (np.abs(vR) > args.class_v / 2)
    print(f"\n[9b] 陀螺 vs 我按几何重推的轮速差（只比幅值，符号不可判）："
          f"几何有效的帧 {int(valid.sum())} / 运动帧 {int(moving_all.sum())}"
          f"（{100.0 * valid.sum() / max(moving_all.sum(), 1):.0f}%）")
    cmp_m = valid & trans & np.isfinite(w_wheel)
    if cmp_m.sum() >= 20:
        print(f"    平动型+无自旋指令且几何有效（n={int(cmp_m.sum())}）：陀螺中位 "
              f"{math.degrees(pctl(np.abs(g0[cmp_m]), 50)):.1f}°/s vs 几何 "
              f"{math.degrees(pctl(np.abs(w_wheel[cmp_m]), 50)):.1f}°/s"
              f"（比值 {pctl(np.abs(g0[cmp_m]), 50) / max(pctl(np.abs(w_wheel[cmp_m]), 50), 1e-9):.2f}）")
        print("    按 |几何| 分档（幅值比≈1 的档 = 无侧滑成立；<<1 的档 = 轮子空转/打滑）：")
        for lo, hi in ((0.0, 3.0), (3.0, 10.0), (10.0, 30.0), (30.0, 1e9)):
            sel = cmp_m & (np.degrees(np.abs(w_wheel)) >= lo) & (np.degrees(np.abs(w_wheel)) < hi)
            if sel.sum() >= 8:
                print(f"      |几何| ∈ [{lo:g},{hi:g})°：n={int(sel.sum()):4d}  几何中位 "
                      f"{math.degrees(pctl(np.abs(w_wheel[sel]), 50)):6.1f}°  陀螺中位 "
                      f"{math.degrees(pctl(np.abs(g0[sel]), 50)):6.1f}°  比值 "
                      f"{pctl(np.abs(g0[sel]), 50) / max(pctl(np.abs(w_wheel[sel]), 50), 1e-9):.2f}")
    else:
        print(f"    几何有效且平动型的帧只有 {int(cmp_m.sum())}，不足以对照")
    print("    读法：[9] 是主证（两条独立链路同量级 ⇒ 偏航是真的；|ch31| 越大比值越小 ⇒ 大偏航时"
          "轮胎跟不上=侧滑）；[9b] 只作旁证，且只在轮面贴 x 时才成立。")

    # ---------------- [10] 驱动轮给定 vs 实际：差模是"下发的"还是"跟丢的" ----------------
    # ch31 只能说明"实际轮速里有多少差模"，分不开**给定本来就有**还是**速度环/打滑造出来的**。
    # ch34/35 是 HalfRudderSolve 下发的轮缘速度给定（已含换向符号与 cos 余弦优化），
    # 与 ch32/33 同量纲，相减才是跟随误差；两者的差模才是"偏航源在命令链路还是执行链路"。
    print("\n[10] 驱动轮给定(ch34/35) vs 实际(ch32/33)：差模从哪来")
    if args.wv_l_ref <= 0 or args.wv_r_ref <= 0:
        print("    已用 --wv-*-ref 0 关闭")
    elif a.shape[1] <= max(args.wv_l_ref, args.wv_r_ref):
        print(f"    本日志只有 {a.shape[1]} 列（需要 ≥{max(args.wv_l_ref, args.wv_r_ref) + 1}）"
              "——加给定通道之前的老日志，跳过")
    else:
        sL = a[:, args.wv_l_ref]
        sR = a[:, args.wv_r_ref]
        refok = ok & np.isfinite(sL) & np.isfinite(sR)
        mov = refok & moving  # 停车/校准帧给定量恒 0，混进来会稀释跟随误差
        if mov.sum() < 20:
            print(f"    运动帧只有 {int(mov.sum())}，不足以量跟随误差")
        else:
            eL = vL[mov] - sL[mov]
            eR = vR[mov] - sR[mov]
            print(f"    跟随误差（实际−给定，运动帧 n={int(mov.sum())}）："
                  f"左中位 {np.median(eL):+.2f} rad/s（|e| p95 {pctl(np.abs(eL), 95):.2f}）、"
                  f"右中位 {np.median(eR):+.2f} rad/s（|e| p95 {pctl(np.abs(eR), 95):.2f}）")
            print(f"    量级：给定中位 左 {pctl(np.abs(sL[mov]), 50):.2f} / 右 {pctl(np.abs(sR[mov]), 50):.2f} rad/s；"
                  f"实际中位 左 {pctl(np.abs(vL[mov]), 50):.2f} / 右 {pctl(np.abs(vR[mov]), 50):.2f} rad/s")
            # 差模换算成线速度 (m/s)：平动型且没下自旋指令时，无侧滑的刚体本该差模 ≡ 0
            dset = (sR - sL) * args.wheel_r
            dact = (vR - vL) * args.wheel_r
            q = refok & trans
            if q.sum() < 20:
                print(f"    平动型+无自旋指令帧只有 {int(q.sum())}，不足以分差模")
            else:
                thr = 0.02  # m/s；2a·w ⇒ 约 2.8°/s 的偏航，低于陀螺噪声底的量级
                set_only = q & (np.abs(dset) < thr) & (np.abs(dact) >= thr)
                act_only = q & (np.abs(dset) >= thr) & (np.abs(dact) < thr)
                both_q = q & (np.abs(dset) >= thr) & (np.abs(dact) >= thr)
                print(f"    ★ 平动型+无自旋指令（n={int(q.sum())}）的轮速差模（换算线速度）："
                      f"给定中位 |{pctl(np.abs(dset[q]), 50):.3f}| m/s、"
                      f"实际中位 |{pctl(np.abs(dact[q]), 50):.3f}| m/s")
                print(f"      给定≈0 而实际≠0（速度环没跟住/打滑）：{int(set_only.sum())} 帧"
                      f"（{100.0 * set_only.sum() / q.sum():.0f}%）；"
                      f"给定≠0 而实际≈0：{int(act_only.sum())} 帧（{100.0 * act_only.sum() / q.sum():.0f}%）；"
                      f"两边都有：{int(both_q.sum())} 帧（{100.0 * both_q.sum() / q.sum():.0f}%）")
                print(f"      差模同号率 {100.0 * float(np.mean(np.sign(dset[q]) == np.sign(dact[q]))):.0f}%"
                      "（给定自己就有差模 ⇒ 偏航源在逆解/换向；给定没有 ⇒ 在执行链路）")

    # ---------------- [11] 纯自旋段：定符号 + 定刻度（指令 w、无平动的那一段） ----------------
    # 纯自旋的判据用**驱动轮给定**：逆解里轮速共模 = 平移分量、差模 = 自旋分量。
    #   common = |sL+sR|·R/2、diff = |sR−sL|·R/2（都换算成线速度 m/s）；
    #   纯自旋 ⇒ common ≈ 0 且 diff 大。比"轮速反号"更准（横移/斜行时轮速符号也可能反）。
    # 这一段里 ch30(指令) / ch31(正解) / ch28(陀螺) 应同号同幅：
    #   同号率 ≈ 100% ⇒ IMU z 与车体系同向 ⇒ CHASSIS_IMU_YAW_SIGN = +1
    #   同号率 ≈   0% ⇒ IMU z 装反        ⇒ CHASSIS_IMU_YAW_SIGN = -1
    # ⚠ 刚体上任意点的角速度相同 ⇒ IMU 离底盘中心 13cm **不需要**为 yaw 做杆臂补偿。
    #   杆臂只影响加速度计（向心 a=ω²r、切向 a=αr），会轻微扰动倾角估计，不改变 yaw_rate。
    print("\n[11] 纯自旋段专项：IMU 符号 / 刻度 / 实际偏航")
    if args.wv_l_ref <= 0 or args.wv_r_ref <= 0 or a.shape[1] <= max(args.wv_l_ref, args.wv_r_ref):
        print("    缺少 ch34/35（驱动轮给定），无法判纯自旋，跳过")
    else:
        sl = a[:, args.wv_l_ref]
        sr = a[:, args.wv_r_ref]
        spin_ok = ok & np.isfinite(sl) & np.isfinite(sr) & (np.abs(cmdw) >= 0.1)
        common = np.abs(sl + sr) * 0.5 * args.wheel_r  # 共模线速度 = 平移分量
        diff = np.abs(sr - sl) * 0.5 * args.wheel_r    # 差模线速度 = 自旋分量
        pure = spin_ok & (diff > 0.05) & (common < 0.25 * diff)
        if pure.sum() < 20:
            print(f"    纯自旋帧只有 {int(pure.sum())}，不足以定符号")
        else:
            cmd_p = cmdw[pure]
            g_p = g0[pure]
            rep_p = repw[pure]
            same_g = float(np.mean(np.sign(g_p) == np.sign(cmd_p)))
            same_r = float(np.mean(np.sign(rep_p) == np.sign(cmd_p)))
            print(f"    纯自旋帧 {int(pure.sum())}（{pure.sum() * dt:.2f}s）："
                  f"共模中位 {pctl(common[pure], 50):.3f} m/s（≈0 = 没平动）、"
                  f"差模中位 {pctl(diff[pure], 50):.3f} m/s")
            print(f"    符号：ch28 vs 指令同号率 {100.0 * same_g:.1f}%；"
                  f"ch31(正解) vs 指令 {100.0 * same_r:.1f}%"
                  "（后者应接近 100%，否则先查正解分支）")
            # 段内 ch28 相对指令的符号翻转次数：刚体绕固定轴匀速旋转时，真实 yaw_rate 不可能翻转。
            # 翻转 > 2 ⇒ 不是"装反"（装反只会整体同号率≈0、绝不在段内翻转），而是姿态解算坏了
            # （典型：IMU 没装平、roll/pitch 接近 ±90° 的欧拉角奇异区，不可观的 roll 在转）。
            sg = np.sign(g_p)
            flips = int(np.sum(sg[1:] * sg[:-1] < 0))
            r_abs = float(np.corrcoef(np.abs(g_p), np.abs(rep_p))[0, 1])
            if same_g < 0.1:
                imu_sign = -1.0
            elif same_g > 0.9:
                imu_sign = 1.0
            else:
                imu_sign = None
            if flips > 2:
                print(f"    ⚠ ch28 在纯自旋段内符号翻转 {flips} 次（刚体绕定轴转不该翻转）——"
                      "**这不是装反，不能靠 CHASSIS_IMU_YAW_SIGN 修**；"
                      f"|ch28| 与 |ch31| 的相关 {r_abs:+.2f}（幅值对、方向乱 = 姿态投影奇异）")
            elif imu_sign is not None:
                print(f"    ★ 结论：CHASSIS_IMU_YAW_SIGN = {imu_sign:+.0f}"
                      f"（IMU z 与车体 z {'同向' if imu_sign > 0 else '反向'}）")
            else:
                print("    ⚠ 同号率居中：该段信号太小或混有噪声，别据此定符号")

            print(f"    刻度（中位 |值|）：ch28 {math.degrees(pctl(np.abs(g_p), 50)):.1f}°/s、"
                  f"ch31 {math.degrees(pctl(np.abs(rep_p), 50)):.1f}°/s、"
                  f"指令 {math.degrees(pctl(np.abs(cmd_p), 50)):.1f}°/s")
            if pctl(np.abs(cmd_p), 50) > 1e-6:
                print(f"    比值：ch28/指令 {pctl(np.abs(g_p), 50) / pctl(np.abs(cmd_p), 50):.3f}、"
                      f"ch31/指令 {pctl(np.abs(rep_p), 50) / pctl(np.abs(cmd_p), 50):.3f}"
                      "（≈1 = 刻度可信；≈0.5/2.0 = 差 2 倍）")

            # 段明细：找出用户说的"那一段"，同时给出实转 vs 指令
            segs_p = segments(pure)
            inc_g = np.zeros(len(t))
            inc_c = np.zeros(len(t))
            inc_r = np.zeros(len(t))
            pair = pure[1:] & pure[:-1]
            dd = np.where(np.isnan(dt_all), 0.0, dt_all)
            inc_g[1:] = np.where(pair, g0[1:] * dd, 0.0)
            inc_c[1:] = np.where(pair, cmdw[1:] * dd, 0.0)
            inc_r[1:] = np.where(pair, repw[1:] * dd, 0.0)
            # ch29 是固件每拍对 ch28(yaw_rate) 积分得到的航向：它若也跟着符号翻，
            # 说明固件内部累积的 yaw 同样是错的（不只是观测通道显示问题）。
            print(f"    分段 {len(segs_p)} 段，合计 {pure.sum() * dt:.2f}s；"
                  f"∫指令 = {math.degrees(inc_c.sum()):+.1f}°、"
                  f"∫ch31(正解) = {math.degrees(inc_r.sum()):+.1f}°、"
                  f"∫ch28(陀螺) = {math.degrees(inc_g.sum()):+.1f}°"
                  f"（前者是实际转动的最可信估计）")
            dyaw_total = 0.0
            for st_i, ln in segs_p:
                idx = np.arange(st_i, st_i + ln)
                seg_yaw = yawu[idx]
                if np.all(np.isfinite(seg_yaw)):
                    dyaw_total += float(seg_yaw[-1] - seg_yaw[0])
            print(f"    ch29(KF 航向) 在纯自旋段内净变化 {math.degrees(dyaw_total):+.1f}°"
                  f"（对照 ∫ch31 {math.degrees(inc_r.sum()):+.1f}°）"
                  "——若两者反号，固件里累积的航向也是错的")
            print("    段明细（≥0.2s）：t0[s]  时长[s]  指令中位  ch28中位  ch31中位  "
                  "ch28同号%  ch31同号%  Δch29[°]  [rad/s]")
            for st_i, ln in segs_p:
                if ln * dt < 0.2:
                    continue
                idx = np.arange(st_i, st_i + ln)
                sg = 100.0 * float(np.mean(np.sign(g0[idx]) == np.sign(cmdw[idx])))
                sr_ = 100.0 * float(np.mean(np.sign(repw[idx]) == np.sign(cmdw[idx])))
                seg_yaw = yawu[idx]
                dyaw = math.degrees(float(seg_yaw[-1] - seg_yaw[0])) \
                    if np.all(np.isfinite(seg_yaw)) else float("nan")
                print(f"      {t[st_i]:8.2f}  {ln * dt:6.3f}  {np.median(cmdw[idx]):+8.3f}  "
                      f"{np.median(g0[idx]):+9.3f}  {np.median(repw[idx]):+9.3f}  "
                      f"{sg:8.1f}  {sr_:8.1f}  {dyaw:+8.1f}")
                if args.spin_dump:
                    act_l = a[idx, args.l_base + OFF_ACT]
                    act_r = a[idx, args.r_base + OFF_ACT]
                    for j, i in enumerate(idx):
                        print(f"        t={t[i]:8.3f} cmd={cmdw[i]:+8.3f} ch28={g0[i]:+8.3f} "
                              f"ch31={repw[i]:+8.3f} ch32={vL[i]:+8.3f} ch33={vR[i]:+8.3f} "
                              f"setL={sl[i]:+8.3f} setR={sr[i]:+8.3f} "
                              f"actL={math.degrees(act_l[j]):+7.1f}° actR={math.degrees(act_r[j]):+7.1f}°")

            # 杆臂量级：只是说明"为什么不需要补偿"，不是修正项
            w95 = pctl(np.abs(cmd_p), 95)
            ac = w95 * w95 * 0.13
            print(f"    杆臂 13cm 的影响（仅加速度计）：|w| p95 {w95:.2f} rad/s ⇒ 向心加速度 "
                  f"{ac:.2f} m/s² = {ac / 9.81:.2f}g；角速度与安装位置无关，yaw 不做杆臂补偿。")

    # ---------------- [12] IMU 原始量：安装方向 + 与姿态解算无关的竖直轴投影 ----------------
    # ch28 的 yaw_rate 是"世界系欧拉角速率"，要用 KF 的 roll/pitch 把机体系陀螺投影出来。
    # 若 IMU 没装平（俯仰/横滚接近 ±90° = 欧拉角奇异），roll 从加速度计几乎不可观，
    # 投影会在两个分支间跳符号（幅值仍然对）——这不是装反，CHASSIS_IMU_YAW_SIGN 修不了。
    # 原始量给出一条不依赖姿态解算的独立答案：
    #   ① 静止段 acc 方向 = 车体"上"在机体系的方向 u（一次标定，安装方向一目了然）
    #   ② 绕竖直轴的角速度 = dot(gyro, u)（刚体角速度投影，不需要 KF）
    print("\n[12] IMU 原始量（ch36~43）：安装方向 / 竖直轴投影")
    need_raw = max(args.gyro_x, args.gyro_y, args.gyro_z, args.acc_x, args.acc_y, args.acc_z,
                   args.roll, args.pitch)
    if args.gyro_x <= 0 or a.shape[1] <= need_raw:
        print(f"    本日志只有 {a.shape[1]} 列（需要 ≥{need_raw + 1}）"
              "——原始量通道是 2026-10-17 才加的，跳过")
    else:
        gx = a[:, args.gyro_x]
        gy = a[:, args.gyro_y]
        gz = a[:, args.gyro_z]
        ax = a[:, args.acc_x]
        ay = a[:, args.acc_y]
        az = a[:, args.acc_z]
        roll = a[:, args.roll]
        pitch = a[:, args.pitch]
        rawok = ok & np.isfinite(gx) & np.isfinite(ax) & np.isfinite(roll) & np.isfinite(pitch)
        rest_raw = rawok & (np.abs(vL) < args.rest_v) & (np.abs(vR) < args.rest_v)
        if rest_raw.sum() < 20:
            print(f"    静止段只有 {int(rest_raw.sum())} 帧，无法确定安装方向")
        else:
            acc_rest = np.array([np.median(ax[rest_raw]), np.median(ay[rest_raw]),
                                 np.median(az[rest_raw])])
            nacc = float(np.linalg.norm(acc_rest))
            if nacc < 1e-3:
                print("    静止段 acc ≈ 0（IMU 没数据？），跳过")
            else:
                # BMI088 数据手册 §8.3（本地 official_docs/BMI088 第 56 页）：
                #   静止时，**朝上的轴输出 +1g**（Table 15：flat/upright ⇒ Z=+1g，
                #   upside-down ⇒ Z=−1g；"gravity acting contrary to the indicated axis
                #   ⇒ 输出为正"）。所以静止 acc 向量指向**上**，
                #   世界"上"在机体系的分量就是 u = acc_rest/|acc_rest|，不是它的反向。
                # ⇒ acc_rest 里哪根轴 ≈ +9.8 就是该轴朝上；≈ −9.8 就是该轴朝下（装反）。
                up = acc_rest / nacc
                ang = [math.degrees(math.acos(min(1.0, abs(float(up[i]))))) for i in range(3)]
                axis_name = "xyz"[int(np.argmax(np.abs(up)))]
                print(f"    静止段 acc 均值 = ({acc_rest[0]:+.3f}, {acc_rest[1]:+.3f}, "
                      f"{acc_rest[2]:+.3f})，模长 {nacc:.2f} m/s²（≈g ✓）")
                print(f"    世界\"上\"在机体系 = ({up[0]:+.3f}, {up[1]:+.3f}, {up[2]:+.3f})；"
                      f"与 ±x/±y/±z 的最小夹角 {ang[0]:.1f}° / {ang[1]:.1f}° / {ang[2]:.1f}°"
                      f" ⇒ 最接近 {'+' if up[int(np.argmax(np.abs(up)))] > 0 else '−'}{axis_name} 轴"
                      "（按手册：静止时该轴读数应为 +g）")
                if ang[int(np.argmax(np.abs(up)))] > 20.0:
                    print("    ⚠ 没有任何一根机体系轴接近竖直 ⇒ IMU 装得很斜（欧拉角奇异区）")
                # 静止段原始陀螺中位 = 写进 s_bmi088_calib.gyro.bias 的候选值。
                # yaw 是纯积分：不标零偏，航向环会按残余零偏慢性漂（见 [13] 的说明）。
                gb = np.array([np.median(gx[rest_raw]), np.median(gy[rest_raw]),
                               np.median(gz[rest_raw])])
                print(f"    静止段陀螺零偏候选（rad/s）：({gb[0]:+.6f}, {gb[1]:+.6f}, "
                      f"{gb[2]:+.6f}) = ({math.degrees(gb[0]):+.3f}, "
                      f"{math.degrees(gb[1]):+.3f}, {math.degrees(gb[2]):+.3f})°/s"
                      " ⇒ 写进 s_bmi088_calib.gyro.bias")

                # 自旋帧：优先用 ch34/35 判纯自旋；没有就退化为"有自旋指令"帧
                if args.wv_l_ref > 0 and a.shape[1] > max(args.wv_l_ref, args.wv_r_ref):
                    sl2 = a[:, args.wv_l_ref]
                    sr2 = a[:, args.wv_r_ref]
                    c2 = np.abs(sl2 + sr2) * 0.5 * args.wheel_r
                    d2 = np.abs(sr2 - sl2) * 0.5 * args.wheel_r
                    spin = rawok & (np.abs(cmdw) >= 0.1) & (d2 > 0.05) & (c2 < 0.25 * d2)
                else:
                    spin = rawok & (np.abs(cmdw) >= 0.1)
                if spin.sum() < 20:
                    print(f"    自旋帧只有 {int(spin.sum())}，不足以验证投影")
                else:
                    # 物理世界系 yaw_rate：刚体角速度在"世界向上"方向上的投影。
                    # 这条不依赖 KF 的 roll/pitch，也不依赖底盘的轮速模型。
                    w_phys = gx * up[0] + gy * up[1] + gz * up[2]
                    same_up = 100.0 * float(np.mean(np.sign(w_phys[spin]) == np.sign(cmdw[spin])))
                    same_ch28 = 100.0 * float(np.mean(np.sign(g0[spin]) == np.sign(cmdw[spin])))
                    same_c28p = 100.0 * float(np.mean(np.sign(g0[spin]) == np.sign(w_phys[spin])))
                    same_c31p = 100.0 * float(np.mean(np.sign(repw[spin]) == np.sign(w_phys[spin])))
                    print(f"    自旋帧 n={int(spin.sum())}：物理 yaw_rate=dot(gyro,u) 中位 "
                          f"{math.degrees(np.median(w_phys[spin])):+7.1f}°/s、|中位| "
                          f"{math.degrees(pctl(np.abs(w_phys[spin]), 50)):5.1f}°/s；"
                          f"指令 {math.degrees(np.median(cmdw[spin])):+7.1f}°/s")
                    print(f"    同号率：物理 vs 指令 {same_up:5.1f}%；ch28 vs 指令 {same_ch28:5.1f}%；"
                          f"ch28 vs 物理 {same_c28p:5.1f}%；ch31(模型) vs 物理 {same_c31p:5.1f}%")
                    # 纯自旋段积分转角（只累加相邻两帧都在自旋段内的区间，与 [11] 同法）
                    pair2 = spin[1:] & spin[:-1]
                    dd2 = np.where(np.isnan(dt_all), 0.0, dt_all)
                    inc_phys = np.where(pair2, w_phys[1:] * dd2, 0.0)
                    inc_cmd2 = np.where(pair2, cmdw[1:] * dd2, 0.0)
                    inc_c312 = np.where(pair2, repw[1:] * dd2, 0.0)
                    sgn_phys = float(np.sign(inc_phys.sum()))
                    sgn_cmd = float(np.sign(inc_cmd2.sum()))
                    sgn_c31 = float(np.sign(inc_c312.sum()))
                    print(f"    ∫：物理 {math.degrees(inc_phys.sum()):+.1f}°、"
                          f"指令 {math.degrees(inc_cmd2.sum()):+.1f}°、"
                          f"ch31(模型) {math.degrees(inc_c312.sum()):+.1f}° ⇒ "
                          f"{'物理与指令同向 ✓、模型也同向 ✓' if sgn_phys == sgn_cmd == sgn_c31 else ('⚠ 物理与指令反号 ⇒ 实转与指令相反' if sgn_phys != sgn_cmd else '⚠ 模型(ch31)与物理反号 ⇒ 正解链路方向不对')}")
                    # 逐轴：哪根轴承载 yaw、每根轴的符号
                    print("    机体系陀螺逐轴（自旋段中位，°/s）：", end="")
                    for nm, arr in (("gx", gx), ("gy", gy), ("gz", gz)):
                        print(f" {nm} {math.degrees(np.median(arr[spin])):+7.2f}", end="")
                    print()
                    if np.std(g0[spin]) > 1e-6 and np.std(gz[spin]) > 1e-6:
                        print(f"    ch28 与 gz 的相关 {float(np.corrcoef(g0[spin], gz[spin])[0, 1]):+.3f}"
                              f"（-1 = ch28 就是 −gz；+1 = ch28 就是 +gz）")
                    rr = roll[spin]
                    pp = pitch[spin]
                    jumps = int(np.sum(np.abs(np.diff(rr)) > math.pi / 2.0))
                    print(f"    KF roll 中位 {math.degrees(np.median(rr)):+.1f}°、范围 "
                          f"[{math.degrees(rr.min()):+.1f}°, {math.degrees(rr.max()):+.1f}°]，"
                          f"|roll|>90° 占比 {100.0 * float(np.mean(np.abs(rr) > math.pi / 2)):.1f}%；"
                          f"pitch 中位 {math.degrees(np.median(pp)):+.1f}°、"
                          f"范围 [{math.degrees(pp.min()):+.1f}°, {math.degrees(pp.max()):+.1f}°]；"
                          f"roll 单拍跳变 >90° 的次数 {jumps}")
                    if args.wv_l_ref > 0 and a.shape[1] > max(args.wv_l_ref, args.wv_r_ref):
                        print(f"    轮速符号（自旋段中位，轮缘 rad/s）：给定 L {np.median(sl2[spin]):+6.2f}"
                              f" / R {np.median(sr2[spin]):+6.2f}；"
                              f"实际 L {np.median(vL[spin]):+6.2f} / R {np.median(vR[spin]):+6.2f}")
                        # 轮速的物理方向 = 轮速 × 轮面方向 g(φ)：舵轮有 ±180° 等价解，
                        # 光看轮速符号证不了转向，必须同时看舵角落在哪个分支。
                        ang_l = a[spin, args.l_base + OFF_ACT]
                        ang_r = a[spin, args.r_base + OFF_ACT]
                        print(f"    舵角（自旋段中位，度，±180 等价）：L "
                              f"{math.degrees(np.median(ang_l)):+7.1f}° / R "
                              f"{math.degrees(np.median(ang_r)):+7.1f}° ⇒ 轮面方向 g = "
                              f"(cos φ, sin φ) 决定轮速符号的物理含义")
                        # 杆臂的向心加速度：a_c = −ω²·r（从 IMU 指向回转中心），
                        # 在机体系里恒定；与静止水平分量之差 ≈ a_c，可反推"中心在 IMU 的哪一侧"。
                        # 已知杆臂 13cm，|a_c| ≈ ω²·0.13，可当校验。
                        ax_sp = float(np.median(ax[spin])) - float(np.median(ax[rest_raw]))
                        ay_sp = float(np.median(ay[spin])) - float(np.median(ay[rest_raw]))
                        wm = float(np.median(np.abs(w_phys[spin])))
                        print(f"    水平 acc 自旋−静止 = ({ax_sp:+.3f}, {ay_sp:+.3f}) m/s²，"
                              f"模长 {math.hypot(ax_sp, ay_sp):.3f}；"
                              f"理论 |ω²r| = {wm * wm * 0.13:.3f} m/s²（r=13cm）"
                              "——方向指向回转中心（在机体系的表达）")
                    # 判读：三方（物理/指令/模型）两两一致性能唯一确定谁反了
                    if same_up > 90.0:
                        if same_c28p < 10.0:
                            print("    ★ 物理跟指令一致，而 ch28 与物理反号 ⇒ KF 的姿态投影把 yaw_rate"
                                  "反了（roll/pitch 分支问题），command/模型/执行链路都正常")
                        else:
                            print("    ★ 物理、指令、ch28 三者同号 ⇒ 方向链路完全自洽，无需改符号")
                    elif same_up < 10.0:
                        if same_c28p > 90.0:
                            print("    ★ 物理与指令**反号**，而 ch28 与物理同号 ⇒ IMU 这一路是对的；"
                                  "**反的是底盘的 yaw 方向**（指令/运动学模型 → 实际轮子/车体）")
                            print("      且 ch31(模型正解) 与物理也反号 ⇒ 模型与执行一致、两者一起反："
                                  "优先查左右驱动轮的物理映射（motor_id ↔ L/R）与 wheel 符号")
                        else:
                            print("    ★ 物理、ch28、指令三者中 ch28 与另两者都反 ⇒ KF 投影反号；"
                                  "同时底盘 yaw 方向也可能反，需再做一次受控转动复核")
                    else:
                        print("    ⚠ 物理与指令同号率居中 ⇒ 纯自旋段里还有别的运动/轴混叠，别据此定符号")

    # ---------------- [13] 平动无自旋指令：稳态残余 vs 舵向过渡 ----------------
    # [8] 只说"平动段整体在转"，[10] 只说"给定差模≈0、实际差模≠0"。这里把平动段按
    # **舵向状态**再切一刀，回答最后一个问题：这点偏航是
    #   ① 稳态残余（两轮都已对齐，仍有轮速差 ⇒ 速度环/摩擦/打滑不对称，与控制无关），还是
    #   ② 舵向过渡（某轮还在换向/回正，轮面斜着出力 ⇒ 几何上就会推着车转）。
    # 判据用角度环误差 errL/errR（rad）：稳态 = 两轮都 ≤ --steady-deg；过渡 = 任一轮 > --off-deg。
    print("\n[13] 平动型+无自旋指令：稳态残余 vs 舵向过渡（偏航剂量拆分）")
    if trans.sum() < 20:
        print(f"    平动型+无自旋指令帧只有 {int(trans.sum())}，跳过")
    else:
        base = trans & np.isfinite(vL) & np.isfinite(vR)
        in_st = (np.abs(errL) <= math.radians(args.steady_deg)) & \
                (np.abs(errR) <= math.radians(args.steady_deg))
        is_tr = (np.abs(errL) > math.radians(args.off_deg)) | \
                (np.abs(errR) > math.radians(args.off_deg))
        groups = (
            ("稳态（两轮 ≤%g°）" % args.steady_deg, base & in_st),
            ("过渡（任一轮 >%g°）" % args.off_deg, base & is_tr),
            ("中间带", base & ~in_st & ~is_tr),
        )
        dd = np.where(np.isnan(dt_all), 0.0, dt_all)   # 跨空洞不积分
        a_half = 2.0 * args.wheel_y                     # yaw = Δv / (2a·cosδ)
        dact = (vR - vL) * args.wheel_r                 # 实际轮速差模 [m/s]
        # 轮面方向 δ：差模只在 cosδ 不为 0 时才转化为偏航（横移 δ≈90° 时没有偏航信息）
        cosd13 = np.cos((actL + actR) / 2.0)
        dir_ok = np.abs(cosd13) > 0.9
        w_imp = np.full(len(t), np.nan)
        w_imp[dir_ok] = dact[dir_ok] / (a_half * cosd13[dir_ok])
        # 测量量改用**物理投影** dot(gyro, u)：[12] 已证明 ch28 的符号会被 KF roll
        # （本日志 |roll|>90° 占 44%）翻掉，幅值也会被姿态跳变污染。没有原始量才退回 ch28。
        w_meas = g0
        w_lbl = "ch28"
        raw13 = (args.gyro_x > 0 and a.shape[1] > max(args.gyro_x, args.gyro_y,
                                                      args.gyro_z, args.acc_x,
                                                      args.acc_y, args.acc_z))
        if raw13:
            rest13 = ok & (np.abs(vL) < args.rest_v) & (np.abs(vR) < args.rest_v)
            acc13 = np.array([np.median(a[rest13, args.acc_x]),
                              np.median(a[rest13, args.acc_y]),
                              np.median(a[rest13, args.acc_z])]) if rest13.sum() >= 20 else None
            if acc13 is not None and np.linalg.norm(acc13) > 1e-3:
                u13 = acc13 / np.linalg.norm(acc13)
                w_meas = (a[:, args.gyro_x] * u13[0] + a[:, args.gyro_y] * u13[1]
                          + a[:, args.gyro_z] * u13[2])
                w_lbl = "物理投影 dot(gyro,u)"
        # 给定差模（若有 ch34/35）：稳态里它应为 0，非 0 就是逆解/换向还在下差模
        have_set = (args.wv_l_ref > 0 and args.wv_r_ref > 0
                    and a.shape[1] > max(args.wv_l_ref, args.wv_r_ref))
        if have_set:
            sL2 = a[:, args.wv_l_ref]
            sR2 = a[:, args.wv_r_ref]
            dset = (sR2 - sL2) * args.wheel_r
        print(f"    分组：稳态 {int(groups[0][1].sum())} 帧、过渡 {int(groups[1][1].sum())} 帧、"
              f"中间带 {int(groups[2][1].sum())} 帧（|cmd.w|<{args.cmd_w_quiet:g} rad/s）；"
              f"偏航测量用 {w_lbl}")
        for nm, m in groups:
            if m.sum() < 10:
                print(f"    {nm}：{int(m.sum())} 帧，不足 10 帧，跳过")
                continue
            pair = m[1:] & m[:-1]
            dur = float(np.sum(np.where(pair, dd, 0.0)))       # 组内占用时间 [s]
            dose = float(np.sum(np.where(pair, np.abs(w_meas[1:]) * dd, 0.0)))  # ∫|w|dt
            net = float(np.sum(np.where(pair, w_meas[1:] * dd, 0.0)))           # ∫w dt
            mdir = m & dir_ok                                  # 差模能换算成偏航的帧
            print(f"    {nm}：n={int(m.sum()):5d}  组内时间 {dur:6.2f}s  "
                  f"剂量 ∫|w|dt = {math.degrees(dose):7.1f}°（{math.degrees(dose) / max(dur, 1e-9):5.1f}°/s）"
                  f"  净 ∫w = {math.degrees(net):+7.1f}°")
            print(f"        实测 |w| 中位 {math.degrees(pctl(np.abs(w_meas[m]), 50)):5.2f}°/s"
                  f"（p95 {math.degrees(pctl(np.abs(w_meas[m]), 95)):6.2f}）"
                  f"；轮面可用（|cosδ|>0.9）{int(mdir.sum())} 帧（{100.0 * mdir.sum() / m.sum():.0f}%）"
                  f"、|cosδ| 中位 {pctl(np.abs(cosd13[m]), 50):.2f}")
            if mdir.sum() >= 10:
                ai = np.abs(w_imp[mdir])
                print(f"        其中实际差模中位 |Δv| {pctl(np.abs(dact[mdir]), 50):.4f} m/s"
                      f" ⇒ 几何蕴含 |w| 中位 {math.degrees(pctl(ai, 50)):5.2f}°/s"
                      f"（实测/几何 {pctl(np.abs(w_meas[mdir]), 50) / max(pctl(ai, 50), 1e-9):.2f}）")
            if have_set:
                aset = np.abs(dset[m])
                print(f"        给定差模中位 |Δv_set| {pctl(aset, 50):.4f} m/s"
                      f"（<0.02 m/s 的帧占 {100.0 * float(np.mean(aset < 0.02)):.0f}%）"
                      + (f"；轮面可用帧里实测与几何蕴含同号率 "
                         f"{100.0 * float(np.mean(np.sign(w_meas[mdir][np.abs(w_imp[mdir]) > 1e-3]) == np.sign(w_imp[mdir][np.abs(w_imp[mdir]) > 1e-3]))):.0f}%"
                         "（差模是被执行出来的、还是另有侧滑源）" if mdir.sum() >= 10 else ""))
            # 有符号统计：区分"恒定偏置"（均值/中位稳定同号、远离 0）与"来回晃"（均值≈0）
            md = mdir if mdir.sum() >= 10 else m
            print(f"        有符号：实际差模均值 {np.mean(dact[md]):+.4f} m/s、中位 "
                  f"{np.median(dact[md]):+.4f}、>0 占 {100.0 * float(np.mean(dact[md] > 0)):.0f}%；"
                  f"实测 w 均值 {math.degrees(np.mean(w_meas[md])):+.2f}°/s、中位 "
                  f"{math.degrees(np.median(w_meas[md])):+.2f}°/s"
                  "（均值与中位同号且远离 0 = 恒定偏置；否则是逐段来回晃）")
            if have_set:
                eL_ = vL[m] - sL2[m]
                eR_ = vR[m] - sR2[m]
                print(f"        跟随误差（实际−给定）有符号中位：左 {np.median(eL_):+.2f}、"
                      f"右 {np.median(eR_):+.2f} rad/s ⇒ 右比左多跟丢 "
                      f"{np.median(eR_ - eL_):+.2f} rad/s"
                      f"（{np.median(eR_ - eL_) * args.wheel_r:+.4f} m/s 的差模）")
                # 按**物理**行驶方向拆（±180° 分支下"给定为正"可能实际是后退，
                # 必须乘 cosδ 才是沿 x 的物理方向）：速度环增益不对称 ⇒ 偏航随行驶
                # 方向**翻号**；固定外部力矩（舵向反扭、装配不对称）⇒ 偏航符号不变。
                along = np.sign((sL2 + sR2) * cosd13)
                along[~dir_ok] = np.nan
                for nmd, seld in (("前进", m & (along > 0)), ("后退", m & (along < 0))):
                    if seld.sum() < 10:
                        continue
                    print(f"        {nmd}帧 n={int(seld.sum()):4d}：实测 w 均值 "
                          f"{math.degrees(np.mean(w_meas[seld])):+6.2f}°/s、差模均值 "
                          f"{np.mean(dact[seld]):+.4f} m/s、w·方向均值 "
                          f"{math.degrees(np.mean(w_meas[seld] * along[seld])):+6.2f}°/s")
        # 按轮面方向分档：|cosδ|≈0 = 两轮横着（横移），此时差模**没有偏航权限**
        #（v_i = v·g ∓ a·w·cosδ，cosδ=0 ⇒ 轮速与 w 无关）；这段里的偏航全部是
        # 侧向轮胎刮擦/外部扰动，速度环无法用差模纠正。
        print("    按轮面方向 |cosδ| 分档（平动型+无自旋指令，全部过渡态混合）：")
        for lo, hi in ((0.9, 1.01), (0.5, 0.9), (0.0, 0.5)):
            sel = base & (np.abs(cosd13) >= lo) & (np.abs(cosd13) < hi)
            if sel.sum() < 10:
                continue
            pair = sel[1:] & sel[:-1]
            dur_ = float(np.sum(np.where(pair, dd, 0.0)))
            dose_ = float(np.sum(np.where(pair, np.abs(w_meas[1:]) * dd, 0.0)))
            print(f"      |cosδ| ∈ [{lo:g},{hi:g})：n={int(sel.sum()):5d}  时间 {dur_:5.2f}s  "
                  f"|w| 中位 {math.degrees(pctl(np.abs(w_meas[sel]), 50)):5.2f}°/s  "
                  f"w 均值 {math.degrees(np.mean(w_meas[sel])):+6.2f}°/s  "
                  f"剂量 {math.degrees(dose_):6.1f}°（{math.degrees(dose_) / max(dur_, 1e-9):5.1f}°/s）")
        # 汇总：偏航剂量主要来自哪一组（按组内时间归一后的绝对贡献）
        tot = 0.0
        for nm, m in groups:
            if m.sum() < 2:
                continue
            pair = m[1:] & m[:-1]
            tot += float(np.sum(np.where(pair, np.abs(w_meas[1:]) * dd, 0.0)))
        print("    ★ 剂量占比：", end="")
        for k, (nm, m) in enumerate(groups):
            if m.sum() < 2:
                continue
            pair = m[1:] & m[:-1]
            d_ = float(np.sum(np.where(pair, np.abs(w_meas[1:]) * dd, 0.0)))
            print(f"{nm} {100.0 * d_ / max(tot, 1e-9):.0f}%", end="  ")
        print("（若稳态占比高 ⇒ 查速度环/左右摩擦/打滑；"
              "若过渡占比高 ⇒ 查换向代价/对齐门控）")
        print("    ⚠ 日志抽样只覆盖约 1/4 时间，过渡窗口（几十 ms）常整段落在空洞里 ⇒ "
              "过渡组只统计到被采到的部分，剂量占比对过渡是**下限**。")

    # ---------------- [14] 偏航闭环：饱和 / 权限门控 / 是否在制造极限环 ----------------
    # ch44 = 闭环用的实测 yaw rate（物理投影+低通），ch45 = cmd.w − ch44，ch46 = 施加的修正。
    # 这里回答三个问题：
    #   ① 修正量是不是长期顶在限幅上（顶住 = 闭环在硬碰硬，不是在微调）；
    #   ② 权限门控（两轮 |cos| 都 > 阈值）会关掉多少帧、这些帧里饱和占多少；
    #   ③ cmd.w≈0 时实测 yaw 有没有来回穿零——零穿率高 = 极限环/自激，不是"漂"。
    print("\n[14] 偏航闭环（ch44~46）：修正量饱和 / 权限门控 / 极限环")
    if args.loop_rate <= 0:
        print("    已用 --loop-rate 0 关闭")
    else:
        need14 = max(args.loop_rate, args.loop_err, args.loop_corr)
        if a.shape[1] <= need14:
            print(f"    本日志只有 {a.shape[1]} 列（需要 ≥{need14 + 1}）——是加闭环通道之前的老日志，跳过")
        else:
            rate14 = a[:, args.loop_rate]
            corr14 = a[:, args.loop_corr]
            m14 = run & np.isfinite(rate14) & np.isfinite(corr14)
            if m14.sum() < 20:
                print(f"    RUN 帧只有 {int(m14.sum())}，跳过")
            else:
                acorr = np.abs(corr14[m14])
                cmax = float(np.max(acorr))
                # 顶到实测最大修正量的 95% 视为饱和（固件限幅改过，不写死 1.0）
                sat14 = m14 & (np.abs(corr14) >= 0.95 * cmax) if cmax > 1e-6 else np.zeros_like(m14)
                print(f"    RUN 帧 {int(m14.sum())}；|修正| 中位 {pctl(np.abs(corr14[m14]), 50):.3f} rad/s、"
                      f"p95 {pctl(np.abs(corr14[m14]), 95):.3f}、最大 {cmax:.3f}"
                      f"（≈{math.degrees(cmax):.1f}°/s）")
                print(f"    饱和（|修正| ≥ 95% 实测最大）{int(sat14.sum())} 帧 = "
                      f"{100.0 * sat14.sum() / m14.sum():.1f}%（>20% 说明闭环长期硬碰硬）")

                # 权限门控：与固件同一条判据（两轮 |cos(实测舵角)| 都够大）
                auth14 = np.minimum(np.abs(np.cos(actL)), np.abs(np.cos(actR)))
                closed14 = m14 & (auth14 < args.yaw_authority)
                dd14 = np.where(np.isnan(dt_all), 0.0, dt_all)
                pair14 = m14[1:] & m14[:-1]
                tot14 = float(np.sum(np.where(pair14, np.abs(rate14[1:]) * dd14, 0.0)))
                close_dose = float(np.sum(np.where(pair14 & closed14[1:], np.abs(rate14[1:]) * dd14, 0.0)))
                print(f"    权限门控（min |cos(舵角)| < {args.yaw_authority:g}）：会关掉 "
                      f"{100.0 * closed14.sum() / m14.sum():.1f}% 的 RUN 帧、"
                      f"占偏航剂量 {100.0 * close_dose / max(tot14, 1e-9):.1f}%"
                      f"；其中修正饱和占 {100.0 * float(np.mean(sat14[closed14])) if closed14.sum() else 0.0:.0f}%"
                      "（门控只保护横移段，主因还得看增益/限幅）")

                # cmd.w≈0 时：零穿率高 = 自激/极限环
                quiet14 = m14 & (np.abs(cmdw) < args.cmd_w_quiet)
                if quiet14.sum() >= 20:
                    rq = rate14[quiet14]
                    durq = float(np.sum(dd14[quiet14[1:]]))
                    zc = int(np.sum(np.diff(np.signbit(rq)) != 0))
                    print(f"    cmd.w≈0：n={int(quiet14.sum())}  {durq:.2f}s  "
                          f"|w| 中位 {math.degrees(pctl(np.abs(rq), 50)):5.2f}°/s  "
                          f"std {math.degrees(np.std(rq)):5.2f}°/s  "
                          f"零穿 {zc} 次（{zc / max(durq, 1e-9):.2f}/s）"
                          f"  ⇒ {'⚠ 来回晃/极限环' if zc / max(durq, 1e-9) > 1.0 else '基本是单向漂/静止'}")
                    opp = float(np.mean(corr14[quiet14] * rq < 0.0))
                    print(f"    修正与实测反向占 {100.0 * opp:.1f}%（接近 100% = 一直在对抗；"
                          f"但对抗≠稳，要看 [14] 的零穿和饱和）")
                # 大偏航时正解与物理是否同号：反号 = 打滑/失控，模型权限不可信
                big14 = m14 & (np.abs(rate14) > 0.5) & np.isfinite(repw)
                if big14.sum() >= 20:
                    same14 = float(np.mean(np.sign(rate14[big14]) == np.sign(repw[big14])))
                    print(f"    |w|>0.5 rad/s 的帧 n={int(big14.sum())}：正解 vs 物理同号率 "
                          f"{100.0 * same14:.1f}%（远低于 90% ⇒ 轮胎在大幅打滑/车在甩，"
                          "此时闭环的几何权限不可信，应先限幅/退让）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
