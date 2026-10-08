#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lib_chassis 运动学内核数值自检

用途
----
在 PC 上独立复算 lib_chassis（统一雅可比内核）与 6 个类型封装的几何，验证：
  1) 增广雅可比满秩（rank=3）
  2) 逆解→正解 往返一致
  3) 力分配满足 Jᵀf = W（含舵轮侧向行）
  4) 冗余底盘的最小力范数性
  5) 与历史 drv_chassis_lite 的闭式解逐项等价（全向/麦轮参考阵 + 舵轮正解内核）
把"约定"钉死在脚本里：改内核/封装后重跑即可发现约定漂移。

模型（与 lib_chassis_def.h 一致）
--------------------------------
  每轮贡献约束行，堆成 m×3 的 J（每行一个 vector3_t）：
    驱动行（全向/麦轮/舵轮都有）：J_i = [ g_x, g_y, g_y·x_i − g_x·y_i ]
    侧向行（仅舵轮，轮面不侧滑）：[-g_y, g_x, g_x·x_i + g_y·y_i]
  g = e_r − cot(β)·e_t，e_r=(cosα,sinα)，e_t=(−sinα,cosα)
    全向/舵轮 β=π/2 → g=e_r；麦轮 β=±π/4 → g=e_r∓e_t（模长 √2）
  轮缘线速度 R·ω = J·ξ（驱动行），侧向行右端为 0；力分配 τ = R·J·(JᵀJ)⁻¹·W。

输入
----
纯几何参数由命令行给；本脚本不读任何文件，输入布局自检 = 参数合法性。
退出码：0 全部通过；1 有失败项。
"""

import argparse
import math
import sys

import numpy as np

# 轮序（与 LibChassisWheel4_e 一致）
LF, LB, RB, RF = 0, 1, 2, 3
PI = math.pi

FAIL = []


def check(name, ok, detail=""):
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail else ""))
    if not ok:
        FAIL.append(name)


# ----------------------------------------------------------------------------
# 内核复算
# ----------------------------------------------------------------------------
def g_of(alpha, beta):
    er = np.array([math.cos(alpha), math.sin(alpha)])
    et = np.array([-math.sin(alpha), math.cos(alpha)])
    cotb = math.cos(beta) / math.sin(beta)
    return er - cotb * et


def build_J(wheels, steer=None):
    """堆约束行。返回 (J[m,3], rows)，rows[i] = (wheel_idx, is_lateral)"""
    J, rows = [], []
    for i, w in enumerate(wheels):
        a = steer[i] if w["kind"] == "rudder" else w["mount_angle"]
        g = g_of(a, w["roller_angle"])
        J.append([g[0], g[1], g[1] * w["x"] - g[0] * w["y"]])
        rows.append((i, 0))
        if w["kind"] == "rudder":  # 轮面不侧滑：g⊥·v = 0
            J.append([-g[1], g[0], g[0] * w["x"] + g[1] * w["y"]])
            rows.append((i, 1))
    return np.array(J), rows


def inverse(wheels, twist, steer=None):
    """与 LibChassisInverse 一致；返回 (steer_target[], wheel_speed[])"""
    st, ws = [], []
    for i, w in enumerate(wheels):
        vix = twist[0] - twist[2] * w["y"]
        viy = twist[1] + twist[2] * w["x"]
        if w["kind"] == "rudder":
            sp = math.hypot(vix, viy)
            cur = steer[i] if steer is not None else 0.0
            if sp < 1e-4:
                st.append(cur)
                ws.append(0.0)
                continue
            phi = math.atan2(viy, vix)
            e = (phi - cur + PI) % (2 * PI) - PI  # (-π, π]
            rev = 0
            if abs(e) > PI / 2:
                e -= PI if e > 0 else -PI
                rev = 1
            st.append(cur + e)
            ws.append((-sp if rev else sp) / w["radius"])
        else:
            g = g_of(w["mount_angle"], w["roller_angle"])
            st.append(w["mount_angle"])
            ws.append((g[0] * vix + g[1] * viy) / w["radius"])
    return np.array(st), np.array(ws)


def forward(wheels, steer, wheel_speed):
    """与 LibChassisForward 一致；返回 (twist, ok)"""
    J, rows = build_J(wheels, steer)
    A = J.T @ J
    if abs(np.linalg.det(A)) < 1e-12:
        return np.zeros(3), False
    s = np.array([0.0 if lat else wheel_speed[wi] * wheels[wi]["radius"] for wi, lat in rows])
    return np.linalg.solve(A, J.T @ s), True


def allocate(wheels, steer, wrench):
    """与 LibChassisAllocateTorque 一致；返回 (torque[n], lateral[n], ok)"""
    n = len(wheels)
    J, rows = build_J(wheels, steer)
    A = J.T @ J
    if abs(np.linalg.det(A)) < 1e-12:
        return np.zeros(n), np.zeros(n), False
    q = np.linalg.solve(A, np.array(wrench))
    f = J @ q
    tau, lat = np.zeros(n), np.zeros(n)
    for k, (wi, is_lat) in enumerate(rows):
        if is_lat:
            lat[wi] = f[k]
        else:
            tau[wi] = f[k] * wheels[wi]["radius"]
    return tau, lat, True


def old_rudder_forward(xs, ys, R, drive_speed, steer):
    """历史 drv_chassis_lite 舵轮正解内核（reduction=1），逐项照抄作交叉验证"""
    n = len(xs)
    Sx = Sy = Srr = Sc = Sd = Sb = 0.0
    for i in range(n):
        s = drive_speed[i] * R
        c, d = s * math.cos(steer[i]), s * math.sin(steer[i])
        Sx += xs[i]
        Sy += ys[i]
        Srr += xs[i] ** 2 + ys[i] ** 2
        Sc += c
        Sd += d
        Sb += d * xs[i] - c * ys[i]
    N = float(n)
    det = N * (N * Srr - Sx * Sx - Sy * Sy)
    if abs(det) < 1e-9:
        return None
    vx = (Sc * (N * Srr - Sx * Sx) - Sy * Sd * Sx + N * Sy * Sb) / det
    vy = (N * (Sd * Srr - Sx * Sb) - Sc * Sx * Sy - Sd * Sy * Sy) / det
    w = (N * Sb - Sd * Sx + Sc * Sy) / (N * Srr - Sx * Sx - Sy * Sy)
    return np.array([vx, vy, w])


# ----------------------------------------------------------------------------
# 类型封装（与 6 个 lib_chassis_<type>.c 一一对应）
# ----------------------------------------------------------------------------
def w(x, y, alpha, beta, R, kind):
    return {"x": x, "y": y, "mount_angle": alpha, "roller_angle": beta, "radius": R, "kind": kind}


CORNER = lambda hl, hw: [(hl, hw), (-hl, hw), (-hl, -hw), (hl, -hw)]


def preset_half_rudder(hl, hw, R):
    return [w(0.0, hw, 0.0, PI / 2, R, "rudder"), w(0.0, -hw, 0.0, PI / 2, R, "rudder")]


def preset_all_rudder(hl, hw, R):
    return [w(x, y, 0.0, PI / 2, R, "rudder") for x, y in CORNER(hl, hw)]


def preset_omni_x(hl, hw, R):
    return [w(x, y, math.atan2(-x, y), PI / 2, R, "omni") for x, y in CORNER(hl, hw)]


def preset_omni_t(hl, hw, R):
    pos = [(hl, 0.0), (0.0, hw), (-hl, 0.0), (0.0, -hw)]
    ang = [-PI / 2, 0.0, PI / 2, PI]
    return [w(x, y, a, PI / 2, R, "omni") for (x, y), a in zip(pos, ang)]


def preset_mecanum_x(hl, hw, R):
    beta = [PI / 4, -PI / 4, PI / 4, -PI / 4]
    return [w(x, y, 0.0, b, R, "mecanum") for (x, y), b in zip(CORNER(hl, hw), beta)]


def preset_mecanum_o(hl, hw, R):
    alpha = [0.0, 0.0, PI, PI]
    beta = [PI / 4, -PI / 4, PI / 4, -PI / 4]
    return [w(x, y, a, b, R, "mecanum") for (x, y), a, b in zip(CORNER(hl, hw), alpha, beta)]


PRESETS = {
    "half_rudder": preset_half_rudder,
    "all_rudder": preset_all_rudder,
    "omni_x": preset_omni_x,
    "omni_t": preset_omni_t,
    "mecanum_x": preset_mecanum_x,
    "mecanum_o": preset_mecanum_o,
}


# ----------------------------------------------------------------------------
# 显式参考阵（钉死约定；只列固定轮——舵轮 J 随舵角时变，改用 JᵀJ 结构验证）
# ----------------------------------------------------------------------------
def ref_rows(name, hl, hw):
    z = hl + hw
    if name == "mecanum_x":
        return np.array([[1, -1, -z], [1, 1, -z], [1, -1, z], [1, 1, z]])
    if name == "mecanum_o":
        return np.array([[1, -1, -z], [1, 1, -z], [-1, 1, -z], [-1, -1, -z]])
    if name == "omni_t":
        return np.array([[0, -1, -hl], [1, 0, -hw], [0, 1, -hl], [-1, 0, -hw]])
    if name == "omni_x":
        r = math.hypot(hl, hw)
        b, a = hw / r, hl / r
        return np.array([[b, -a, -r], [b, a, -r], [-b, a, -r], [-b, -a, -r]])
    return None


# ----------------------------------------------------------------------------
# 逐类型测试
# ----------------------------------------------------------------------------
def test_preset(name, hl, hw, R, rng, tol):
    wheels = PRESETS[name](hl, hw, R)
    n = len(wheels)
    is_rudder = any(x["kind"] == "rudder" for x in wheels)
    steer = rng.uniform(-PI, PI, size=n) if is_rudder else None

    J, _ = build_J(wheels, steer)

    # 1) 满秩
    rank = np.linalg.matrix_rank(J, tol=1e-9)
    check(f"{name}: 雅可比满秩 (m={len(J)}, n={n})", rank == 3, f"rank={rank}")

    # 2) 参考阵比对（仅固定轮）
    ref = ref_rows(name, hl, hw)
    if ref is not None:
        check(f"{name}: 雅可比与参考阵一致", np.allclose(J, ref, atol=1e-9) if J.shape == ref.shape else False)

    # 2b) 舵轮：JᵀJ 应与位置量有关、与舵角无关（= 历史闭式结构）
    if is_rudder:
        Sx, Sy = sum(x["x"] for x in wheels), sum(x["y"] for x in wheels)
        Srr = sum(x["x"] ** 2 + x["y"] ** 2 for x in wheels)
        A_ref = np.array([[n, 0.0, -Sy], [0.0, n, Sx], [-Sy, Sx, Srr]])
        check(f"{name}: JᵀJ 与历史闭式结构一致", np.allclose(J.T @ J, A_ref, atol=1e-9))

    # 3) 逆解 → 正解 往返
    md = []
    for _ in range(50):
        twist = rng.uniform(-1.0, 1.0, size=3)
        st, ws = inverse(wheels, twist, steer)
        tw2, ok = forward(wheels, st if is_rudder else steer, ws)
        md.append(np.max(np.abs(tw2 - twist)) if ok else 1e9)
    check(f"{name}: 逆解→正解往返一致", max(md) < tol, f"max_err={max(md):.3e}")

    # 3b) 舵轮：与历史正解闭式逐项等价
    if is_rudder:
        me = 0.0
        for _ in range(20):
            ws = rng.uniform(-30.0, 30.0, size=n)
            sa = rng.uniform(-PI, PI, size=n)
            tw_new, ok = forward(wheels, sa, ws)
            tw_old = old_rudder_forward([x["x"] for x in wheels], [x["y"] for x in wheels], R, ws, sa)
            me = max(me, np.max(np.abs(tw_new - tw_old)) if (ok and tw_old is not None) else 1e9)
        check(f"{name}: 正解与历史闭式等价", me < 1e-6, f"max_err={me:.3e}")

    # 4) 力分配 Jᵀf = W（含侧向行）
    mf = []
    for _ in range(50):
        W = rng.uniform(-3.0, 3.0, size=3)
        tau, lat, ok = allocate(wheels, steer, W)
        if not ok:
            mf.append(1e9)
            continue
        Jc, rows = build_J(wheels, steer)
        f = np.array([lat[wi] if is_lat else tau[wi] / wheels[wi]["radius"] for wi, is_lat in rows])
        mf.append(np.max(np.abs(Jc.T @ f - W)))
    check(f"{name}: 力分配满足 Jᵀf=W", max(mf) < tol, f"max_err={max(mf):.3e}")

    # 5) 最小力范数（冗余）
    if len(J) > 3:
        W = np.array([1.0, 0.0, 0.0])
        tau, lat, _ = allocate(wheels, steer, W)
        _, rows = build_J(wheels, steer)
        f = np.array([lat[wi] if is_lat else tau[wi] / wheels[wi]["radius"] for wi, is_lat in rows])
        check(f"{name}: 力分配为最小范数解", np.allclose(f, np.linalg.pinv(J.T) @ W, atol=tol))


def test_degenerate(hl, hw, R):
    # 半舵两轮回转中心重合 → 奇异
    wheels = preset_half_rudder(hl, hw, R)
    wheels[1]["y"] = wheels[0]["y"]
    wheels[1]["x"] = wheels[0]["x"]
    _, ok = forward(wheels, np.zeros(2), np.array([1.0, 0.0]))
    check("half_rudder: 两轮回转中心重合时正解失败", not ok)

    # 全舵四轮回转中心全重合 → 奇异
    wheels = preset_all_rudder(hl, hw, R)
    for x in wheels[1:]:
        x["x"], x["y"] = wheels[0]["x"], wheels[0]["y"]
    _, ok = forward(wheels, np.zeros(4), np.ones(4))
    check("all_rudder: 四轮回转中心全重合时正解失败", not ok)

    # 固定轮 2 轮欠驱动 → 奇异
    wheels = [w(hl, hw, 0.0, PI / 2, R, "omni"), w(-hl, -hw, 0.0, PI / 2, R, "omni")]
    _, ok = forward(wheels, None, np.array([1.0, 1.0]))
    check("omni(2 轮): 欠驱动时正解失败", not ok)

    # 固定轮 3 轮共线 → 奇异
    wheels = [w(hl, 0.0, 0.0, PI / 2, R, "omni"), w(0.0, 0.0, 0.0, PI / 2, R, "omni"), w(-hl, 0.0, 0.0, PI / 2, R, "omni")]
    _, ok = forward(wheels, None, np.ones(3))
    check("omni(3 轮共线): 正解失败", not ok)


def main():
    ap = argparse.ArgumentParser(description="lib_chassis 运动学内核数值自检")
    ap.add_argument("--half-len", type=float, default=0.30, help="半轴距 (m)")
    ap.add_argument("--half-width", type=float, default=0.25, help="半轮距 (m)")
    ap.add_argument("--radius", type=float, default=0.05, help="轮半径 (m)")
    ap.add_argument("--seed", type=int, default=20261008)
    ap.add_argument("--tol", type=float, default=1e-9)
    args = ap.parse_args()

    # 输入自检
    if args.half_len <= 0 or args.half_width <= 0 or args.radius <= 0:
        print("参数必须为正数：--half-len/--half-width/--radius")
        return 1
    if args.half_len == args.half_width:
        print("提示：half-len == half-width（正方形），仍继续")

    rng = np.random.default_rng(args.seed)
    print(f"几何: half_len={args.half_len} half_width={args.half_width} radius={args.radius}\n")

    for name in PRESETS:
        test_preset(name, args.half_len, args.half_width, args.radius, rng, args.tol)
        print()

    test_degenerate(args.half_len, args.half_width, args.radius)
    print()

    if FAIL:
        print(f"== 失败 {len(FAIL)} 项: {FAIL}")
        return 1
    print("== 全部通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
