#!/usr/bin/env python3
"""最终: 固定J=0.008(频域), 优化摩擦+重力; 实际力矩仿真验证"""
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
win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)
p = pos_r[free]; v = spd_r[free]; a = acc_r[free]; y = trq_r[free]

print("="*70)
print("  [1] 固定 J=0.008, 优化 [cv+, cv-, Tc, G, θ0]")
print("="*70)
Jfix = 0.008
def resid(params):
    cvp, cvn, Tc, G, th0 = params
    fric = np.where(v > 0, cvp*v, cvn*v) + Tc*np.where(v > 0, 1.0, np.where(v < 0, -1.0, 0.0))
    grav = G*np.cos(th0 + p)
    return y - (Jfix*a + fric + grav)
def obj(params):
    return float(np.mean(resid(params)**2))

bounds = [(0, 0.3), (0, 0.3), (0, 0.5), (0, 1.0), (-np.pi, np.pi)]
res = minimize(obj, [0.03, 0.03, 0.10, 0.30, 0.0], method='L-BFGS-B', bounds=bounds,
               options={'maxiter': 3000})
cvp, cvn, Tc, G, th0 = res.x
print(f"  cv+={cvp:.4f}  cv-={cvn:.4f}  Tc={Tc:.4f}  G={G:.4f}  θ0={th0:+.3f}rad ({np.degrees(th0):+.1f}°)")
r = resid(res.x)
rmse = np.sqrt(np.mean(r**2))
print(f"  力矩残差 RMSE = {rmse:.4f} Nm  (配置参数=0.1465)")

# 尝试 J 也自由 (对比)
print("\n  [1b] J 自由优化 (对照)")
def residJ(params):
    Jx, cvp, cvn, Tc, G, th0 = params
    fric = np.where(v > 0, cvp*v, cvn*v) + Tc*np.where(v > 0, 1.0, np.where(v < 0, -1.0, 0.0))
    grav = G*np.cos(th0 + p)
    return y - (Jx*a + fric + grav)
def objJ(params):
    return float(np.mean(residJ(params)**2))
bJ = [(0.001, 0.03), (0, 0.3), (0, 0.3), (0, 0.5), (0, 1.0), (-np.pi, np.pi)]
resJ = minimize(objJ, [0.008, 0.03, 0.03, 0.10, 0.30, 0.0], method='L-BFGS-B', bounds=bJ,
                options={'maxiter': 3000})
print(f"  J={resJ.x[0]:.5f} cv+={resJ.x[1]:.4f} cv-={resJ.x[2]:.4f} Tc={resJ.x[3]:.4f} "
      f"G={resJ.x[4]:.4f} θ0={resJ.x[5]:+.2f}")
print(f"  力矩残差 RMSE = {np.sqrt(np.mean(residJ(resJ.x)**2)):.4f} Nm")

# 残差结构
print("\n  固定J 残差 vs 速度:")
vb = np.linspace(-7.5, 4.5, 20)
for i in range(len(vb)-1):
    m = (v >= vb[i]) & (v < vb[i+1])
    if m.sum() < 40: continue
    print(f"  v[{vb[i]:+.1f},{vb[i+1]:+.1f}]  残差中位={np.median(r[m]):+.4f}  (n={m.sum()})")

# ============ 2. 仿真验证 (实际力矩输入) ============
print("\n" + "="*70)
print("  [2] 开环仿真 (实际力矩输入, 自由段)")
print("="*70)
def simulate(tau_in, pos0, spd0, J, cvp, cvn, Tc, G, th0, n_steps):
    pp = np.zeros(n_steps); vv = np.zeros(n_steps)
    pp[0], vv[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vcur = vv[i-1]
        if vcur > 0: fric = cvp*vcur + Tc
        elif vcur < 0: fric = cvn*vcur - Tc
        else: fric = 0.0
        grav = G*np.cos(th0 + pp[i-1])
        a_ = (tau_in[i] - grav - fric)*invJ
        vv[i] = vcur + a_/fs
        pp[i] = pp[i-1] + vcur/fs
    return pp, vv

seg_starts = []
i = 0
while i < n - 1200:
    if free[i] and free[i+1200]:
        seg_starts.append(i); i += 1500
    else:
        i += 1
print(f"  自由连续段: {len(seg_starts)} 个")
for name, pars in [("固定J辨识", (Jfix, cvp, cvn, Tc, G, th0)),
                   ("J自由辨识", tuple(resJ.x)),
                   ("配置参数", (0.008, 0.05, 0.06, 0.02, 0.28, 0.0))]:
    Jx, cvpx, cvnx, Tcx, Gx, th0x = pars
    v_list = []; p_list = []
    for s0 in seg_starts[:12]:
        s1 = s0 + 800
        if s1 > n: break
        ps, vs = simulate(trq_r[s0:s1], pos_r[s0], spd_r[s0],
                          Jx, cvpx, cvnx, Tcx, Gx, th0x, 800)
        v_list.append(np.sqrt(np.mean((vs[40:-40]-spd_r[s0+40:s1-40])**2)))
        p_list.append(np.sqrt(np.mean((ps[40:-40]-pos_r[s0+40:s1-40])**2)))
    if v_list:
        print(f"  {name:10s}  速度RMSE均={np.mean(v_list):.3f}  位置RMSE均={np.mean(p_list):.4f}")
