# -*- coding: utf-8 -*-
"""
验证 TUNE 开环跟踪偏差机制: 欠补偿阻尼 cv.
真实动力学固定 (J=0.008, cv_true, Tc_true), 前馈参数变化.
预测: cv_ff=0.05 且 cv_true=0.09 时 滞后~22° 幅值~0.92, 与实测(20°/92.5%)对比.
"""
import sys, io
import numpy as np
if sys.platform == "win32" and isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")

FS = 1000.0
dt = 1 / FS
A, F = 0.1, 2.0
w = 2 * np.pi * F
T_total = 12.0
n = int(T_total * FS)
t = np.arange(n) * dt
ref_pos = A * np.sin(w * t)
ref_vel = A * w * np.cos(w * t)
ref_acc = -A * w * w * np.sin(w * t)

J = 0.008
G = 0.28
Tc_true = 0.02


def sign_s(x, dead=1e-3):
    """带死区的符号函数, 模拟静摩擦/避免抖振"""
    return np.where(x > dead, 1.0, np.where(x < -dead, -1.0, 0.0))


def simulate(J_ff, cv_ff_pos, cv_ff_neg, Tc_ff, cv_true_pos, cv_true_neg, Tc_t):
    """开环前馈仿真: cmd=J_ff*ref_acc+cv_ff*v+Tc_ff*sign(v)+G*cos(th); 真实: J*a+cv_true*v+Tc_t*sign(v)+G*cos(th)=cmd"""
    v = np.zeros(n)
    th = np.zeros(n)
    th[0] = 0.108  # 实测偏置
    v[0] = 0.0
    for i in range(1, n):
        o = v[i - 1]
        sgn = sign_s(o)
        ff = G * np.cos(th[i - 1]) + J_ff * ref_acc[i - 1] + cv_ff_pos * o * (o > 0) + cv_ff_neg * o * (o < 0) + Tc_ff * sgn
        f_real = G * np.cos(th[i - 1]) + cv_true_pos * o * (o > 0) + cv_true_neg * o * (o < 0) + Tc_t * sgn
        a = (ff - f_real) / J
        v[i] = o + a * dt
        th[i] = th[i - 1] + v[i] * dt
    return v, th


def fit2(y, tt):
    S, C = np.sin(w * tt), np.cos(w * tt)
    sol = np.linalg.lstsq(np.column_stack([S, C, np.ones_like(tt)]), y, rcond=None)[0]
    return np.hypot(sol[0], sol[1]), np.degrees(np.arctan2(sol[0], sol[1]))


tt = t[t > 2.0]
a_rv, p_rv = fit2(ref_vel[t > 2.0], tt)
print("=" * 60)
print(f"实测: 速度滞后 19.9° (27.7ms), 幅值比 0.925")
print("=" * 60)

cases = [
    # 标签, 前馈cv, 真实cv
    ("当前: 前馈0.05/0.06  vs 真实0.09", 0.05, 0.06, 0.02, 0.09, 0.09, 0.02),
    ("当前: 前馈0.05/0.06  vs 真实0.09/0.10", 0.05, 0.06, 0.02, 0.09, 0.10, 0.02),
    ("改前馈cv=0.09/0.10 vs 真实0.09/0.10 (理想)", 0.09, 0.10, 0.02, 0.09, 0.10, 0.02),
    ("改前馈cv=0.085 vs 真实0.09 (接近)", 0.085, 0.095, 0.02, 0.09, 0.10, 0.02),
]
for lbl, cvpf, cvnf, tcf, cvpt, cvnt, tct in cases:
    v, th = simulate(J, cvpf, cvnf, tcf, cvpt, cvnt, tct)
    a_s, p_s = fit2(v[t > 2.0], tt)
    lag = p_rv - p_s
    print(f"\n{lbl}")
    print(f"  仿真速度2Hz: 幅值={a_s:.4f} (ref {a_rv:.4f}, 比{a_s/a_rv:.3f})  相位={p_s:+.1f}°")
    print(f"  滞后ref_vel: {lag:+.1f}° = {lag/360*0.5*1000:.1f} ms")
