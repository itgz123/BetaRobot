#!/usr/bin/env python3
"""非线性整体优化辨识: 自由段力矩残差最小化"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter
from scipy.optimize import minimize

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
DATA = np.load(os.path.join(BASE, "dedup.npz"))
t = DATA["t"]; pos = DATA["pos"]; spd = DATA["spd"]
trq = DATA["trq"]; gff = DATA["gff"]; multi = DATA["multi"]

fs = 1000.0
act = np.abs(multi) > 1e-6
t0 = t[act].min()
tuni = np.arange(t0, t[act].max(), 1.0/fs)
def rs(x):
    return np.interp(tuni, t[act], x[act])
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); trq_r = rs(trq)
n = len(tuni)

# 自由段
pos_lim = 0.55
free = (np.abs(pos_r) < pos_lim) & (tuni > tuni[0]+0.3) & (tuni < tuni[-1]-0.3)
print(f"自由段: {free.sum()} 样本 ({free.sum()/fs:.1f}s)")

win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)

p = pos_r[free]; v = spd_r[free]; a = acc_r[free]; y = trq_r[free]
N = p.shape[0]
print(f"自由段位置范围 [{p.min():.2f},{p.max():.2f}], 速度范围 [{v.min():.2f},{v.max():.2f}]")

# 残差函数
def resid(params):
    J, cvp, cvn, Tc, G, th0 = params
    sgn = np.where(v > 0, 1.0, np.where(v < 0, -1.0, 0.0))
    fric = np.where(v > 0, cvp*v, cvn*v) + Tc*sgn
    grav = G*np.cos(th0 + p)
    return y - (J*a + fric + grav)

def obj(params):
    r = resid(params)
    return float(np.mean(r**2))

# 初值 + 边界
x0 = [0.008, 0.03, 0.03, 0.10, 0.28, 0.0]
bounds = [(0.001, 0.03), (0, 0.3), (0, 0.3), (0, 0.5), (0, 1.0), (-np.pi, np.pi)]

# 多初值
print("\n 多初值优化:")
results = []
for th0_guess in [-1.5, -1.0, -0.5, 0.0, 0.5]:
    for J_guess in [0.006, 0.008, 0.010]:
        x0i = [J_guess, 0.03, 0.03, 0.10, 0.30, th0_guess]
        res = minimize(obj, x0i, method='L-BFGS-B', bounds=bounds,
                       options={'maxiter': 3000})
        results.append((res.fun, res.x))
        print(f"  init[J={J_guess},th0={th0_guess}]: cost={res.fun:.6f}  "
              f"J={res.x[0]:.5f} cv+={res.x[1]:.4f} cv-={res.x[2]:.4f} "
              f"Tc={res.x[3]:.4f} G={res.x[4]:.4f} th0={res.x[5]:+.2f}")

results.sort(key=lambda t: t[0])
best_cost, best = results[0]
print(f"\n最优: cost={best_cost:.6f} (力矩RMSE={np.sqrt(best_cost):.4f} Nm)")
J, cvp, cvn, Tc, G, th0 = best
print(f"  J   = {J:.5f}")
print(f"  cv+ = {cvp:.4f}   cv- = {cvn:.4f}")
print(f"  Tc  = {Tc:.4f}")
print(f"  G   = {G:.4f} Nm   θ0 = {th0:+.3f} rad ({np.degrees(th0):+.1f}°)")

# 残差检查
r = resid(best)
print(f"  残差 RMSE = {np.sqrt(np.mean(r**2)):.4f} Nm")
# 残差 vs pos (检查是否还有重力残余)
print("\n  残差 vs pos 分箱:")
pb = np.linspace(-0.55, 0.55, 12)
for i in range(len(pb)-1):
    m = (p >= pb[i]) & (p < pb[i+1])
    if m.sum() < 30: continue
    print(f"  pos[{pb[i]:+.2f},{pb[i+1]:+.2f}]  残差中位={np.median(r[m]):+.4f}  (n={m.sum()})")

# 残差 vs v (检查摩擦残余)
print("\n  残差 vs 速度 分箱:")
vb = np.linspace(-7, 4, 16)
for i in range(len(vb)-1):
    m = (v >= vb[i]) & (v < vb[i+1])
    if m.sum() < 30: continue
    print(f"  v[{vb[i]:+.1f},{vb[i+1]:+.1f}]  残差中位={np.median(r[m]):+.4f}  (n={m.sum()})")

# 与配置参数对比
r_cfg = y - (0.008*acc_r[free] + np.where(v>0, 0.05*v, 0.06*v) + np.where(v>0, 0.02, np.where(v<0, 0.0, 0.0)) + 0.28*np.cos(p))
print(f"\n  配置参数残差 RMSE = {np.sqrt(np.mean(r_cfg**2)):.4f} Nm")

# 仿真验证 (自由段 1s 窗口)
print("\n  自由段仿真验证:")
def simulate(y_cmd, pos0, spd0, J, cvp, cvn, Tc, G, th0, n_steps):
    pp = np.zeros(n_steps); vv = np.zeros(n_steps)
    pp[0], vv[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vcur = vv[i-1]
        if vcur > 0: fric = cvp*vcur + Tc
        elif vcur < 0: fric = cvn*vcur - Tc
        else: fric = 0.0
        grav = G*np.cos(th0 + pp[i-1])
        a_ = (y_cmd[i] - grav - fric)*invJ
        vv[i] = vcur + a_/fs
        pp[i] = pp[i-1] + vcur/fs
    return pp, vv

# 找自由连续段
seg_starts = []
i = 0
while i < n - 1200:
    if free[i] and free[i+1200]:
        seg_starts.append(i); i += 1500
    else:
        i += 1
print(f"  自由连续段: {len(seg_starts)} 个")
for name, pars in [("辨识", (J, cvp, cvn, Tc, G, th0)),
                   ("配置", (0.008, 0.05, 0.06, 0.02, 0.28, 0.0))]:
    Jx, cvpx, cvnx, Tcx, Gx, th0x = pars
    v_rmses = []
    for s0 in seg_starts[:10]:
        s1 = s0 + 1000
        if s1 > n: break
        if np.abs(pos_r[s0:s1]).max() > pos_lim: continue
        ps, vs = simulate(multi_r[s0:s1], pos_r[s0], spd_r[s0],
                          Jx, cvpx, cvnx, Tcx, Gx, th0x, 1000)
        e = np.sqrt(np.mean((vs[50:-50]-spd_r[s0+50:s1-50])**2))
        v_rmses.append(e)
    if v_rmses:
        print(f"  {name}: 速度RMSE均 = {np.mean(v_rmses):.3f} rad/s")
