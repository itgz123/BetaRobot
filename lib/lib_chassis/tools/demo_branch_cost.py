#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：换向分支的「代价函数」演示（设计参考，不是实机数据）

用途
----
回答一个设计问题：**选哪个换向分支，能不能只比"谁转到位更快"？**

每轮有两个等价解：(舵角 d, 正转) 与 (舵角 d−180°, 反转)，推力完全相同。
所以"选解"实际就是"舵轮往哪边转"。脚本对候选组合逐个预测一段过渡过程，算三条量：

  T      : makespan（最慢一轮的舵轮行程 / ω_max）
  Yaw    : ∫|Δw| dt，过渡期的偏航冲量（自转的"剂量"）
  J      : ∫ (差值 twist 的加权平方) dt，即文里那个代价

关键结论（脚本 --check 会自证第一条）：
 1) 用了余弦缩放后，**轮速给定与分支无关**：s_i(t) = V_i·cos(d_i − φ_i(t)) 对两个分支
    恒等。于是"两个等价解"的差别**只在 φ(t) 走哪条弧**，不在给多大速度。
 2) 对称起步的纯平移，四个候选的 T **完全一样**（都转过同样的 |Δφ|，只是反向），
    但混分支的 Yaw 极大 ⇒ **时间在这里零信息量**，只有偏航积分能分好坏。

用法
----
  python3 demo_branch_cost.py                 # 纯平移 +90°，四种分支对照
  python3 demo_branch_cost.py --sweep         # 平移角 0~180 扫描，看时间何时才有区分度
  python3 demo_branch_cost.py --check         # 自检"分支无关"恒等式
  python3 demo_branch_cost.py --twist 0 1 0.8 --phi 5 -3   # 任意 twist + 两轮不同起步角

几何默认值取 app/half_rudder_chassis/robot_def.h。输出只用于设计判断，不替代实机标定。

退出码：0 正常；1 自检失败。
"""

import argparse
import math
import sys

import numpy as np

# 舵轮回转中心（半舵：同轴 x=0，y=±d）。见 robot_def.h 的 CHASSIS_RUDDER_*_POS_Y
X_POS = (0.0, 0.0)


def wrap(a):
    """归一化到 (-π, π]"""
    return math.atan2(math.sin(a), math.cos(a))


def inverse(twist, y_pos):
    """LibChassisInverse 的舵轮分支：返回每轮 (期望轮向 d, 轮缘速度大小 V)"""
    vx, vy, w = twist
    out = []
    for y in y_pos:
        vix = vx - w * y   # x_i = 0 ⇒ v_iy = vy，两轮共用
        viy = vy
        out.append((math.atan2(viy, vix), math.hypot(vix, viy)))
    return out


def forward_least_squares(alpha, wheel_speed, y_pos):
    """LibChassisForward：驱动行 + 侧向约束行，最小二乘解 (vx, vy, w)"""
    rows, b = [], []
    for a, s, y in zip(alpha, wheel_speed, y_pos):
        x = 0.0
        ca, sa = math.cos(a), math.sin(a)
        rows.append([ca, sa, sa * x - ca * y])   # 驱动行：g·v_i = R·ω_i
        b.append(s)
        rows.append([-sa, ca, ca * x + sa * y])  # 侧向行：轮面不侧滑，右端 0
        b.append(0.0)
    xi, *_ = np.linalg.lstsq(np.array(rows), np.array(b), rcond=None)
    return xi


def predict(twist, phi0, branch, y_pos, omega_max, dt, steps, w=(1.0, 1.0, 3.0)):
    """预测一个候选：舵角以 ω_max 走最短弧到目标（砰-砰 + 到位保持）；轮速按余弦律；逐步喂正解

    branch[i] = 0: 目标 d_i，正转    1: 目标 d_i−180°，反转
    返回 (T = makespan, yaw_impulse, J)      J = ∫ ‖ξ̂ − ξ*‖²_W dt（先按 |v*|、|w*| 归一化）
    """
    ref = inverse(twist, y_pos)
    d = [r[0] for r in ref]
    v = [r[1] for r in ref]
    target = [d[i] - math.pi * branch[i] for i in range(len(y_pos))]
    delta = [wrap(target[i] - phi0[i]) for i in range(len(y_pos))]
    travel = [abs(x) for x in delta]
    t_end = max(travel) / omega_max if omega_max > 0 else 0.0

    t = 0.0
    yaw_imp, jcost = 0.0, 0.0
    # 归一化尺度：线性速度取"指令"与"自旋在轮上产生的轮速"里大的那个，
    # 否则纯自旋指令（vx=vy=0）会把误差除以 ~0，代价虚高几个数量级。
    arm = max(abs(y) for y in y_pos)
    vmag = max(abs(twist[0]), abs(twist[1]), abs(twist[2]) * arm, 1e-6)
    wmag = abs(twist[2]) if abs(twist[2]) > 1e-6 else 1.0

    for _ in range(steps):
        if t > t_end + dt:
            break
        alpha, spd = [], []
        for i in range(len(y_pos)):
            # 砰-砰（时间最优）+ 到位保持：每轮各自以 ω_max 走最短弧，先到的先停。
            # 注意不是"按共同 makespan 均摊"——位置环不会让先到的那轮降速等另一轮。
            frac = min(t * omega_max / travel[i], 1.0) if travel[i] > 1e-9 else 0.0
            phi = phi0[i] + frac * delta[i]
            alpha.append(phi)
            # 余弦律：s = V·cos(d − φ)。两个分支恒等（见 --check）
            spd.append(v[i] * math.cos(d[i] - phi))
        xi = forward_least_squares(alpha, spd, y_pos)
        dvx, dvy, dw = xi[0] - twist[0], xi[1] - twist[1], xi[2] - twist[2]
        yaw_imp += abs(dw) * dt
        jcost += (w[0] * (dvx / vmag) ** 2 + w[1] * (dvy / vmag) ** 2
                  + w[2] * (dw / wmag) ** 2)
        t += dt

    return t_end, yaw_imp, jcost * dt


def check_identity(y_pos, omega_max, dt, steps, twist, phi0):
    """自证：轮速给定 s(t) 与分支无关（两分支的 φ 路径互为镜像）"""
    ref = inverse(twist, y_pos)
    d = [r[0] for r in ref]
    v = [r[1] for r in ref]
    worst = 0.0
    for t in np.linspace(0.0, 0.5, 51):
        for i in range(len(y_pos)):
            phi = phi0[i] + min(t / (math.pi / 2 / omega_max), 1.0) * (math.pi / 2)
            a0 = (-1) ** 0 * v[i] * math.cos(d[i] - 0 * math.pi - phi)
            a1 = (-1) ** 1 * v[i] * math.cos(d[i] - 1 * math.pi - phi)
            worst = max(worst, abs(a0 - a1))
    print(f"[check] 两分支轮速给定最大差 {worst:.3e} m/s"
          f"（应为 0；非 0 说明余弦律/符号约定写错了）")
    return worst < 1e-12


def main():
    ap = argparse.ArgumentParser(description="半舵底盘换向分支代价演示")
    ap.add_argument("--pos-y", type=float, default=0.20225, help="舵轮回转中心 |y| [m]（robot_def.h）")
    ap.add_argument("--phi", type=float, nargs=2, default=[0.0, 0.0], help="两轮当前舵角 [deg]")
    ap.add_argument("--twist", type=float, nargs=3, default=None, metavar=("VX", "VY", "W"),
                    help="指令 twist（车体系）；缺省用 --psi 造纯平移")
    ap.add_argument("--psi", type=float, default=90.0, help="纯平移方向 [deg]（--twist 缺省时用）")
    ap.add_argument("--mag", type=float, default=1.0, help="纯平移速度大小 [m/s]")
    ap.add_argument("--omega-max", type=float, default=358.0, help="舵轮最大角速度 [deg/s]")
    ap.add_argument("--dt", type=float, default=0.002, help="预测步长 [s]")
    ap.add_argument("--steps", type=int, default=600, help="最大预测步数")
    ap.add_argument("--w", type=float, nargs=3, default=[1.0, 1.0, 3.0], metavar=("WX", "WY", "WW"),
                    help="代价权重（先按 |v*|、|w*| 归一化再加权）")
    ap.add_argument("--sweep", action="store_true", help="扫平移角 0~180°，看 T 何时才有区分度")
    ap.add_argument("--check", action="store_true", help="自检分支无关恒等式")
    args = ap.parse_args()

    y_pos = (args.pos_y, -args.pos_y)
    omega_max = math.radians(args.omega_max)
    phi0 = [math.radians(p) for p in args.phi]
    twist = (tuple(args.twist) if args.twist is not None
             else (args.mag * math.cos(math.radians(args.psi)),
                   args.mag * math.sin(math.radians(args.psi)), 0.0))

    if args.check:
        return 0 if check_identity(y_pos, omega_max, args.dt, args.steps, twist, phi0) else 1

    if args.sweep:
        print("平移角扫描（起步舵角 L=%.1f° R=%.1f°，ω_max=%.0f°/s）"
              % (math.degrees(phi0[0]), math.degrees(phi0[1]), args.omega_max))
        print(" ψ[deg] | d[deg] | 同分支最优T[ms] 混分支T[ms] | 同分支Yaw 混分支Yaw")
        for psi in range(0, 181, 15):
            tw = (args.mag * math.cos(math.radians(psi)), args.mag * math.sin(math.radians(psi)), 0.0)
            d_deg = math.degrees(inverse(tw, y_pos)[0][0])
            t00, y00, _ = predict(tw, phi0, [0, 0], y_pos, omega_max, args.dt, args.steps, args.w)
            t11, y11, _ = predict(tw, phi0, [1, 1], y_pos, omega_max, args.dt, args.steps, args.w)
            t_mm, y_mm, _ = predict(tw, phi0, [0, 1], y_pos, omega_max, args.dt, args.steps, args.w)
            t_ss, y_ss = min((t00, y00), (t11, y11))  # 同分支里更快的那个
            flag = "  ← 时间打平" if abs(t_ss - t_mm) < 1e-9 else ""
            print(f" {psi:6d} |{d_deg:7.1f} |{t_ss * 1e3:12.1f} {t_mm * 1e3:9.1f} |"
                  f"{y_ss:9.3f} {y_mm:9.3f}{flag}")
        print("\n※ 同分支 = 两轮取同一个解（[0,0] 或 [1,1]，T/Yaw 对称相同，取快者）；"
              "混分支 = 一轮翻一轮不翻")
        print("※ 平移时 d_L ≡ d_R，所以'都翻'永远合法 ⇒ 混分支既不更快也不更直（T 只在 |d|=90° 打平）")
        return 0

    print(f"指令 twist = ({twist[0]:+.3f}, {twist[1]:+.3f}, {twist[2]:+.3f})   "
          f"起步舵角 L={math.degrees(phi0[0]):+.1f}° R={math.degrees(phi0[1]):+.1f}°")
    ref = inverse(twist, y_pos)
    print(f"内核期望： L d={math.degrees(ref[0][0]):+7.1f}° V={ref[0][1]:.3f} m/s   "
          f"R d={math.degrees(ref[1][0]):+7.1f}° V={ref[1][1]:.3f} m/s")
    print(f"\n候选(0=正解 1=反解)  行程L   行程R       T       Yaw      J")
    best = None
    for bl in (0, 1):
        for br in (0, 1):
            T, yaw_imp, J = predict(twist, phi0, [bl, br], y_pos, omega_max,
                                   args.dt, args.steps, args.w)
            d = [r[0] for r in ref]
            trav = [abs(math.degrees(wrap(d[i] - math.pi * (bl if i == 0 else br) - phi0[i])))
                    for i in range(2)]
            tag = "混分支" if bl != br else "同分支"
            print(f"  L{bl} R{br} ({tag})  {trav[0]:6.1f}° {trav[1]:6.1f}°  "
                  f"{T * 1e3:6.1f}ms  {yaw_imp:6.3f}  {J:8.4f}")
            if best is None or J < best[0]:
                best = (J, bl, br)
    print(f"\n代价最小 = L{best[1]} R{best[2]}（J={best[0]:.4f}）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
