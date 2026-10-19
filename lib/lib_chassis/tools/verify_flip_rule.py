#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
半舵底盘：换向判据（档1）离线验证 —— 复刻 app_chassis.c 的 ChassisRudderDecide，与代价最优对比

⚠ 档1 已被**档2（短时域代价预测）取代**：`app_chassis.c` 现在选的是 min J，档1 的
  FLIP_MARGIN/FLIP/SYNC 三个常量已从 `app_chassis_def.h` 删除。本脚本保留为档1 的
  **历史记录与对照基线**（`--margin/--sync` 仍可复现那套规则），当前判据请用
  `verify_costsel.py`（后者也 import 本文件的 `decide_new` 作对照）。

用途
----
换向判据没有实机可测（本机无 CAN 硬件），只能在 PC 上回放：
  · 用 demo_branch_cost.py 的同一个预测器/代价 J(c) 算出四个候选里**事后最优**的那个；
  · 用它复刻 C 侧的新判据（无迟滞 + 两轮联合）与旧判据（逐轮迟滞），看谁选得对。

判据（与 app_chassis.c 一一对应）
    e_i = wrap(d_i − φ_i)
    新：|Δd| = |wrap(d_L − d_R)| < SYNC 时两轮同分支，rev = (max|e_i| > FLIP)；
        否则（自旋）各轮独立 rev_i = (|e_i| > FLIP)。
        FLIP = 90° + MARGIN，无跨帧状态。
    旧：两轮各自 rev_i = (|e_i| > 90° ∓ HYST)（按 rev_i 上一拍的值分档）——有状态，
        故本脚本显式喂一个"上一拍"的历史状态，用来复现"一轮翻一轮不翻"。

指标
    regret = J(判据选的) / J(事后最优)      —— 选得多差
    yaw    = 该选择下的偏航冲量∫|Δw|dt      —— 自转剂量
    混分支 = 是否两轮选了不同分支           —— 旧版出问题的形态

用法
    python3 verify_flip_rule.py                 # 平移角 × 起步角差 扫描，新/旧对比
    python3 verify_flip_rule.py --margin 8      # 换裕量看敏感度
    python3 verify_flip_rule.py --detail        # 逐工况打印

退出码：0 正常；1 新判据出现混分支（平移工况下不应发生）。
"""

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # 允许从任意目录运行
import demo_branch_cost as bc  # noqa: E402  同目录；预测器与代价只此一份

DEG = math.degrees


def decide_new(d, phi, margin_deg, sync_deg):
    """复刻 ChassisRudderDecide（无迟滞 + 两轮联合）"""
    flip = math.radians(90.0 + margin_deg)
    e = [bc.wrap(d[i] - phi[i]) for i in range(2)]
    span = abs(bc.wrap(d[0] - d[1]))
    if span < math.radians(sync_deg):
        rev = 1 if max(abs(e[0]), abs(e[1])) > flip else 0
        return [rev, rev]
    return [1 if abs(e[i]) > flip else 0 for i in range(2)]


def decide_old(d, phi, rev_prev, hyst_deg):
    """复刻旧版 ChassisRudderSteer（逐轮、有状态的迟滞）"""
    h = math.radians(hyst_deg)
    out = []
    for i in range(2):
        e = abs(bc.wrap(d[i] - phi[i]))
        th = (math.pi / 2 - h) if rev_prev[i] else (math.pi / 2 + h)
        out.append(1 if e > th else 0)
    return out


def cost_of(twist, phi, branch, y_pos, omega_max, dt, steps, w):
    t, yaw, j = bc.predict(twist, phi, branch, y_pos, omega_max, dt, steps, w)
    return j, yaw


def main():
    ap = argparse.ArgumentParser(description="半舵换向判据离线验证")
    ap.add_argument("--pos-y", type=float, default=0.20225, help="舵轮回转中心 |y| [m]")
    ap.add_argument("--omega-max", type=float, default=358.0, help="舵轮最大角速度 [deg/s]")
    ap.add_argument("--dt", type=float, default=0.002, help="预测步长 [s]")
    ap.add_argument("--steps", type=int, default=600, help="最大预测步数")
    ap.add_argument("--margin", type=float, default=15.0, help="新判据翻转裕量 [deg]（= 头文件 FLIP_MARGIN）")
    ap.add_argument("--sync", type=float, default=90.0, help="两轮同分支的 |Δd| 上限 [deg]（= SYNC_RAD）")
    ap.add_argument("--hyst", type=float, default=15.0, help="旧判据迟滞带宽 [deg]（已删除的 HYST_RAD）")
    ap.add_argument("--w", type=float, nargs=3, default=[1.0, 1.0, 3.0], metavar=("WX", "WY", "WW"))
    ap.add_argument("--detail", action="store_true", help="逐工况打印")
    args = ap.parse_args()

    y_pos = (args.pos_y, -args.pos_y)
    omega_max = math.radians(args.omega_max)
    stats = {"new": [], "old": []}
    worst = {"new": (0.0, None), "old": (0.0, None)}
    mix_new = mix_old = 0
    total = 0

    for psi in range(0, 181, 15):
        tw = (math.cos(math.radians(psi)), math.sin(math.radians(psi)), 0.0)
        d = [r[0] for r in bc.inverse(tw, y_pos)]
        for dphi in (-20, -12, 0, 12, 20):  # 进场两轮起步角差
            phi = [math.radians(dphi), math.radians(-dphi)]
            total += 1
            cand = {}
            for bl in (0, 1):
                for br in (0, 1):
                    cand[(bl, br)] = cost_of(tw, phi, (bl, br), y_pos, omega_max,
                                             args.dt, args.steps, args.w)
            best = min(cand.values(), key=lambda v: v[0])

            # 旧判据：喂两种历史状态（两轮都未反向 / 只有左轮反向 = 日志里那种不对称进场）
            for rev_prev in ([0, 0], [1, 0]):
                ob = decide_old(d, phi, rev_prev, args.hyst)
                oj, oy = cand[tuple(ob)]
                stats["old"].append(oj / best[0])
                if ob[0] != ob[1]:
                    mix_old += 1
                if oj / best[0] > worst["old"][0]:
                    worst["old"] = (oj / best[0], (psi, dphi, rev_prev, ob))

            nb = decide_new(d, phi, args.margin, args.sync)
            nj, ny = cand[tuple(nb)]
            stats["new"].append(nj / best[0])
            if nb[0] != nb[1]:
                mix_new += 1
            if nj / best[0] > worst["new"][0]:
                worst["new"] = (nj / best[0], (psi, dphi, nb))

            if args.detail:
                bo = min(cand, key=lambda k: cand[k][0])
                print(f"ψ={psi:3d}° Δφ={dphi:+3d}° | 最优 {bo} J={best[0]:.3f} | "
                      f"新 {tuple(nb)} J={nj:.3f} (×{nj / best[0]:.2f}) | "
                      f"旧[0,0] {tuple(decide_old(d, phi, [0, 0], args.hyst))} | "
                      f"旧[1,0] {tuple(decide_old(d, phi, [1, 0], args.hyst))}")

    print(f"\n工况数 {total}（平移角 0~180° × 起步角差 ±20°），预测 ω_max={args.omega_max:g}°/s，"
          f"新判据 margin={args.margin:g}° sync={args.sync:g}°，旧判据 hyst={args.hyst:g}°")
    for k, n in (("new", "新判据（无迟滞+两轮联合）"), ("old", "旧判据（逐轮迟滞）")):
        s = stats[k]
        mix = mix_new if k == "new" else mix_old
        print(f"  {n}: regret 中位 {sorted(s)[len(s) // 2]:.3f}  p95 {sorted(s)[int(len(s) * .95)]:.3f}  "
              f"最大 {max(s):.3f} | 混分支 {mix}/{len(s)}")
        print(f"      最差工况 J/J*={worst[k][0]:.3f} @ ψ={worst[k][1][0]}° Δφ={worst[k][1][1]}° "
              f"→ 分支 {worst[k][1][-1]}")

    if mix_new > 0:
        print("\n✗ 新判据在平移/弯行工况下出现了混分支——与 SYNC 逻辑不符")
        return 1
    print(f"\n✓ 新判据在全部 {total} 个平移/弯行工况下两轮分支一致"
          f"（自旋不在本次扫描内：那里 |Δd| ≥ sync，按设计走独立分支）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
