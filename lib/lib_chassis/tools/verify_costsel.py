#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：换向判据（档2 代价函数）离线验证 —— 复刻 app_chassis.c 的 ChassisRudderCost/Decide

用途
----
档2 把判据从"|角差| 与 90°+裕量比大小"（档1）换成"短时域预测的代价 J 谁小"。
本脚本干三件事：

 1) **复刻 C 侧算法**（`cost_c`/`decide_cost`，与 app_chassis.c 一一对应：砰-砰 + 16 步中点
    积分 + ZERO_RAD 快路径 + "上一拍分支排第一、严格 <"平手保持），拿它和
    `demo_branch_cost.py` 的**细网格参考**（2ms 固定步长、600 步）对比：
      regret   = J(档2 选的) / J(细网格事后最优)   —— 选得多差
      argmin 一致 = 粗格与细格选的是不是同一个候选 —— 实时近似（16 步）够不够
 2) **对照档1**（`verify_flip_rule.decide_new`，已上车的规则）跑同一批工况，看档2 到底修了
    什么、有没有修坏；
 3) **复现三个命名工况**：§2.2 表1（对称起步平移，四候选 T 完全相同）/ 表2（13°/−14° 起步，
    "谁快选谁"会主动挑中自转的混分支）/ 日志里那次单轮 183.5° 零推力磨盘（`--cases`）。

为什么不做"整段日志回放"
    回放需要知道当时两轮的期望轮向 d_i（= 指令 twist）。日志里只有**舵角给定/实测/电流**
    那 12 路 ×2：给定列给的是折算过分支的目标角 φ_target = d_i − 180°·rev_i，只能反推出
    d_i mod 180°，分辨不出"平移还是自旋"（两种假设对应不同的 ξ*，代价不同）；驱动轮的速度
    给定也没进 VOFA。⇒ 只能定点复现，不能整段回放。真要回放得再占 2~3 路 VOFA 通道
    （帧已占 921600 的 ~76%，见 app_cfg.h 的 VOFA_CHANNELS）。

用法
    python3 verify_costsel.py              # 扫描：档2 vs 档1 vs 事后最优
    python3 verify_costsel.py --cases      # 打印三个命名工况的候选表
    python3 verify_costsel.py --tracking   # 稳态跟踪工况（快路径命中率）
    python3 verify_costsel.py --detail     # 逐工况打印
    python3 verify_costsel.py --steps 8    # 换 C 侧采样步数看敏感度

退出码：0 正常；1 出现"档2 粗格与细格最优不一致"或命名工况选错。
"""

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # 允许从任意目录运行
import demo_branch_cost as bc  # noqa: E402  同目录；正解/余弦律只此一份
import verify_flip_rule as vf  # noqa: E402  档1 判据的复刻（已上线那一版）

DEG = math.degrees

# ── C 侧常量：与 app_chassis_def.h 一一对应 ──────────────────────────────────
OMEGA_MAX = 50.0 / 8.0  # CHASSIS_STEER_OMEGA_MAX_RAD = VMAX_MOTOR / STEER_GEAR_RATIO
STEPS_C = 16            # CHASSIS_STEER_COST_STEPS
W_C = (1.0, 1.0, 3.0)   # W_V / W_W
SCALE_MIN = 1e-3        # CHASSIS_STEER_COST_SCALE_MIN（vscale 下限）
W_SCALE_MIN = 1.0       # CHASSIS_STEER_COST_W_SCALE_MIN（w*=0 时偏航误差的尺子）
ZERO_RAD = 0.0174533    # CHASSIS_STEER_COST_ZERO_RAD（两轮行程 < 1° ⇒ J≡0）


def scales(twist, y_pos):
    """复刻 ChassisRudderDecide 里的 vscale/wscale"""
    arm = max(abs(y) for y in y_pos)
    vscale = max(abs(twist[0]), abs(twist[1]), abs(twist[2]) * arm, SCALE_MIN)
    wscale = abs(twist[2]) if abs(twist[2]) > SCALE_MIN else W_SCALE_MIN
    return vscale, wscale


def cost_c(twist, phi0, branch, y_pos, omega_max=OMEGA_MAX, steps=STEPS_C, w=W_C):
    """复刻 ChassisRudderCost：砰-砰到位 + 中点法积分（自适应步长 = 行程/ω_max/steps）

    与 demo_branch_cost.predict 的区别只有两处（都是有意为之的对齐）：
      · 采样栅格：这里用 C 侧那套（固定步数、步长随行程自适应），predict 用固定 2ms 细格；
      · ZERO_RAD 快路径：两轮行程都 < 1° 时 J≡0（J 非负 ⇒ 必为全局最优）。predict 无此短路。
    """
    d = [r[0] for r in bc.inverse(twist, y_pos)]
    v = [r[1] for r in bc.inverse(twist, y_pos)]
    moving = [vi > 0.0 for vi in v]
    delta = [bc.wrap(d[i] - math.pi * branch[i] - phi0[i]) if moving[i] else 0.0
             for i in range(len(y_pos))]
    travel = max(abs(x) for x in delta)
    if travel < ZERO_RAD:
        return 0.0

    t_end = travel / omega_max
    dt = t_end / steps
    vscale, wscale = scales(twist, y_pos)
    j = 0.0
    for k in range(steps):
        t = (k + 0.5) * dt                       # 中点法：采样点全落在过渡期内
        alpha, spd = [], []
        for i in range(len(y_pos)):
            phi = phi0[i]
            if abs(delta[i]) > 1e-6:
                phi += delta[i] * min(t * omega_max / abs(delta[i]), 1.0)  # 到位后保持
            alpha.append(phi)
            spd.append(v[i] * math.cos(d[i] - phi))   # 余弦律（不钳负，§2.1）
        xi = bc.forward_least_squares(alpha, spd, y_pos)
        j += (w[0] * ((xi[0] - twist[0]) / vscale) ** 2
              + w[1] * ((xi[1] - twist[1]) / vscale) ** 2
              + w[2] * ((xi[2] - twist[2]) / wscale) ** 2)
    return j * dt


def decide_cost(twist, phi0, rev_prev, y_pos, omega_max=OMEGA_MAX, steps=STEPS_C, w=W_C):
    """复刻 ChassisRudderDecide：4 候选，按 (k&1)^cur_l / (k>>1)^cur_r 枚举，严格 < 取最小

    rev_prev = 上一拍两轮各自的分支（平手时保持它；C 侧对应 s_steer_reverse）。
    """
    best_j, best = None, list(rev_prev)
    for k in range(4):
        rl = (k & 1) ^ rev_prev[0]
        rr = (k >> 1) ^ rev_prev[1]
        j = cost_c(twist, phi0, (rl, rr), y_pos, omega_max, steps, w)
        if k == 0 or j < best_j:
            best_j, best = j, [rl, rr]
    return best, best_j


def cost_ref(twist, phi0, branch, y_pos, omega_max, dt, steps, w):
    """细网格参考（demo_branch_cost.predict）：J 与 yaw 冲量"""
    _, yaw, j = bc.predict(twist, phi0, branch, y_pos, omega_max, dt, steps, w)
    return j, yaw


def candidates(twist, phi0, y_pos, omega_max, dt, steps, w, c_steps):
    """四个候选的 (J细, J粗, T, 行程L, 行程R)"""
    d = [r[0] for r in bc.inverse(twist, y_pos)]
    out = {}
    for bl in (0, 1):
        for br in (0, 1):
            jf, yaw = cost_ref(twist, phi0, (bl, br), y_pos, omega_max, dt, steps, w)
            jc = cost_c(twist, phi0, (bl, br), y_pos, omega_max, c_steps, w)
            trav = [abs(DEG(bc.wrap(d[i] - math.pi * (bl if i == 0 else br) - phi0[i])))
                    for i in range(2)]
            out[(bl, br)] = {"Jf": jf, "Jc": jc, "yaw": yaw, "trav": trav}
    return out


def fmt_cand(cand, key):
    c = cand[key]
    return (f"{key[0]}{key[1]} ({'混' if key[0] != key[1] else '同'}) "
            f"{c['trav'][0]:6.1f}° {c['trav'][1]:6.1f}° {c['yaw']:7.3f} "
            f"{c['Jf']:8.4f} {c['Jc']:8.4f}")


def named_cases(y_pos, omega_max, dt, steps, w, c_steps):
    """§2.2 表1/表2 + 日志里那次单轮磨盘"""
    cases = [
        ("§2.2 表1 对称起步 +90° 平移", (0.0, 1.0, 0.0), (0.0, 0.0)),
        ("§2.2 表2 13°/−14° 起步 +90° 平移", (0.0, 1.0, 0.0), (13.0, -14.0)),
        # 日志 t=26.874：自旋切回平动(d=180°)，两轮其实都已对位 ⇒ 零行程解是**混分支**
        ("日志复现 L=−179.6° R=−0.2° d=180°", (-1.0, 0.0, 0.0), (-179.6, -0.2)),
        # 同一个物理工况、把 d 改标成 0°（两解等价 ⇒ 物理上选的目标角必须一样）
        ("同上但 d=0°（分支标号应整体翻转）", (1.0, 0.0, 0.0), (179.6, -0.2)),
    ]
    ok = True
    for name, twist, phi_deg in cases:
        phi0 = [math.radians(p) for p in phi_deg]
        cand = candidates(twist, phi0, y_pos, omega_max, dt, steps, w, c_steps)
        best_fine = min(cand, key=lambda k: cand[k]["Jf"])
        b1 = tuple(vf.decide_new([r[0] for r in bc.inverse(twist, y_pos)], phi0, 15.0, 90.0))
        b2, j2 = decide_cost(twist, phi0, [0, 0], y_pos, omega_max, c_steps, w)
        print(f"\n【{name}】  指令 twist=({twist[0]:+.3f},{twist[1]:+.3f},{twist[2]:+.3f})  "
              f"起步角 L={phi_deg[0]:+.1f}° R={phi_deg[1]:+.1f}°")
        print("  候选      行程L   行程R    Yaw      J细      J粗")
        for k in sorted(cand):
            print("   " + fmt_cand(cand, k))
        print(f"  细格事后最优 {best_fine}  |  档1(FLIP/SYNC) 选 {b1} "
              f"regret={cand[b1]['Jf'] / cand[best_fine]['Jf']:.3f}  |  "
              f"档2(代价) 选 {tuple(b2)} J粗={j2:.4f} "
              f"regret={cand[tuple(b2)]['Jf'] / cand[best_fine]['Jf']:.3f}")
        if tuple(b2) != best_fine:
            print(f"  ✗ 档2 与细格最优不一致")
            ok = False
    return ok


def sweep(y_pos, omega_max, dt, steps, w, c_steps, detail):
    """平移/弯行/自旋扫描：档2 vs 档1 vs 事后最优；粗格与细格 argmin 一致性"""
    rows = []
    mix2 = 0
    agree = 0
    fast = 0
    for spin in (False, True):
        for a in range(0, 181, 15):                       # 平移角（自旋与平移角无关，只跑 a=0）
            if spin and a != 0:
                continue
            ang = math.radians(a)
            tw = ((0.0, 0.0, 0.6) if spin else (math.cos(ang), math.sin(ang), 0.0))
            d = [r[0] for r in bc.inverse(tw, y_pos)]
            for dphi in (0, 12, -12, 20, -20):
                for asym in ((1, 1), (1, -1), (0.5, -1.5)):   # 两轮起步角差的不对称分配
                    phi = [math.radians(dphi * asym[0]), math.radians(-dphi * asym[1])]
                    cand = candidates(tw, phi, y_pos, omega_max, dt, steps, w, c_steps)
                    best_fine = min(cand, key=lambda k: cand[k]["Jf"])
                    b1 = tuple(vf.decide_new(d, phi, 15.0, 90.0))
                    b2, _ = decide_cost(tw, phi, [0, 0], y_pos, omega_max, c_steps, w)
                    c2 = tuple(b2)
                    # 粗格选出来的，其**细格代价**是否仍等于细格最优（判据只要求排序对；
                    # 镜像对称工况下四候选 J 精确相等，用比值容差而不是候选下标比较）
                    if cand[c2]["Jf"] <= cand[best_fine]["Jf"] * (1.0 + 1e-6):
                        agree += 1
                    if c2[0] != c2[1]:
                        mix2 += 1
                    if cand[c2]["Jc"] == 0.0:
                        fast += 1
                    r1 = cand[b1]["Jf"] / cand[best_fine]["Jf"]
                    r2 = cand[c2]["Jf"] / cand[best_fine]["Jf"]
                    rows.append((r1, r2, (a, spin, dphi, tuple(asym)), b1, c2, best_fine))
                    if detail:
                        print(f"  ψ={a:3d}° spin={int(spin)} Δφ={dphi:+3d} asym={asym} | "
                              f"最优 {best_fine} | 档1 {b1} ×{r1:.3f} | 档2 {c2} ×{r2:.3f}")
    return rows, agree, mix2, fast


def tracking(y_pos, omega_max, dt, steps, w, c_steps):
    """稳态跟踪：起步角 = 目标角 ± ε（ε 远小于 ZERO_RAD），检验快路径
       快路径命中时选的候选必须与细格最优一致（J 非负 ⇒ 0 必为最小）。"""
    bad = 0
    hit = 0
    n = 0
    for a in range(0, 181, 15):
        ang = math.radians(a)
        for spin in (False, True):
            tw = ((0.0, 0.0, 0.6) if spin else (math.cos(ang), math.sin(ang), 0.0))
            d = [r[0] for r in bc.inverse(tw, y_pos)]
            for eps in (0.0, 0.2, 0.5, -0.5, 0.9, -0.9):     # °，都在 1° 快路径阈值内
                for rev_prev in ((0, 0), (1, 1), (0, 1)):
                    # 上一拍选定的目标角（= d − 180°·rev_prev），本轮实测就在它附近
                    phi = [d[i] - math.pi * rev_prev[i] + math.radians(eps) for i in range(2)]
                    cand = candidates(tw, phi, y_pos, omega_max, dt, steps, w, c_steps)
                    best_fine = min(cand, key=lambda k: cand[k]["Jf"])
                    b2, j2 = decide_cost(tw, phi, list(rev_prev), y_pos, omega_max, c_steps, w)
                    n += 1
                    if j2 == 0.0:
                        hit += 1
                        if tuple(b2) != best_fine:
                            bad += 1
                            print(f"  ✗ 快路径选错：ψ={a}° spin={int(spin)} ε={eps}° "
                                  f"rev_prev={rev_prev} → 选 {tuple(b2)}，细格最优 {best_fine}")
    return n, hit, bad


def main():
    ap = argparse.ArgumentParser(description="半舵换向判据（档2 代价函数）离线验证")
    ap.add_argument("--pos-y", type=float, default=0.20225, help="舵轮回转中心 |y| [m]")
    ap.add_argument("--omega-max", type=float, default=DEG(OMEGA_MAX),
                    help="舵轮最大角速度 [deg/s]（缺省 = VMAX/GEAR = 358）")
    ap.add_argument("--dt", type=float, default=0.002, help="细格参考步长 [s]")
    ap.add_argument("--steps", type=int, default=600, help="细格参考最大步数")
    ap.add_argument("--c-steps", type=int, default=STEPS_C, help="C 侧每候选采样步数")
    ap.add_argument("--w", type=float, nargs=3, default=list(W_C), metavar=("WX", "WY", "WW"))
    ap.add_argument("--cases", action="store_true", help="只跑命名工况（表1/表2/日志复现）")
    ap.add_argument("--tracking", action="store_true", help="只跑稳态跟踪（快路径检验）")
    ap.add_argument("--detail", action="store_true", help="逐工况打印")
    args = ap.parse_args()

    y_pos = (args.pos_y, -args.pos_y)
    omega_max = math.radians(args.omega_max)
    w = tuple(args.w)

    print(f"半舵 {args.pos_y:g}m，ω_max={args.omega_max:g}°/s，C 侧 {args.c_steps} 步/候选，"
          f"细格 {args.dt * 1e3:g}ms×{args.steps} 步，W={w}")

    if args.cases:
        return 0 if named_cases(y_pos, omega_max, args.dt, args.steps, w, args.c_steps) else 1

    if args.tracking:
        n, hit, bad = tracking(y_pos, omega_max, args.dt, args.steps, w, args.c_steps)
        print(f"稳态跟踪 {n} 例：快路径命中 {hit}（{hit / n * 100:.1f}%），选错 {bad}")
        return 1 if bad else 0

    rows, agree, mix2, fast = sweep(y_pos, omega_max, args.dt, args.steps, w, args.c_steps,
                                    args.detail)
    r1 = sorted(r[0] for r in rows)
    r2 = sorted(r[1] for r in rows)
    print(f"\n工况 {len(rows)}（平移/弯行/自旋 × 起步角差，复刻 C 侧 {args.c_steps} 步栅格）")
    print(f"  档1（FLIP/SYNC）  : regret 中位 {r1[len(r1) // 2]:.3f}  p95 {r1[int(len(r1) * .95)]:.3f}"
          f"  最大 {max(r1):.3f}")
    print(f"  档2（代价函数）  : regret 中位 {r2[len(r2) // 2]:.3f}  p95 {r2[int(len(r2) * .95)]:.3f}"
          f"  最大 {max(r2):.3f}")
    print(f"  档2 粗格=细格 argmin 一致 {agree}/{len(rows)}；档2 选混分支 {mix2}/{len(rows)}；"
          f"快路径命中 {fast}/{len(rows)}")
    worst = max(rows, key=lambda r: r[1])
    print(f"  档2 最差工况 ×{worst[1]:.3f} @ ψ={worst[2][0]}° spin={int(worst[2][1])} "
          f"Δφ={worst[2][2]:+d} asym={worst[2][3]} → 选 {worst[4]}，最优 {worst[5]}")

    if args.detail:
        print("\n最差 5 例：")
        for r in sorted(rows, key=lambda r: -r[1])[:5]:
            print(f"  ψ={r[2][0]:3d}° spin={int(r[2][1])} Δφ={r[2][2]:+3d} asym={r[2][3]} | "
                  f"档1 {r[3]} ×{r[0]:.3f} | 档2 {r[4]} ×{r[1]:.3f} | 最优 {r[5]}")

    ok = True
    if agree != len(rows):
        print(f"\n✗ 档2 粗格与细格事后最优不一致 {len(rows) - agree} 例 —— 16 步栅格不够，调 --c-steps")
        ok = False
    else:
        print(f"\n✓ 档2 与细格事后最优在每个工况都选了同一个候选（粗格只影响 J 的绝对值，不影响排序）")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
