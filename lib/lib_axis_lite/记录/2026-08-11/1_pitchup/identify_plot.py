# -*- coding: utf-8 -*-
"""
v7 最终辨识:
  J 取 CH3-FRF 高频值 (8.3e-3), 固定 J, 优化 [G, cv+, cv-, Tc+, Tc-, τ]
  同时给 binned 残差摩擦作为交叉验证; 输出仿真验证
"""
import sys, io
if sys.platform == "win32" and isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")
import numpy as np
import pandas as pd
from scipy.signal import savgol_filter
from scipy.optimize import minimize

CSV = "ig/data/plot_data.csv"
NZ_OFFSET = 982550
FS = 1000.0

df = pd.read_csv(CSV)
d = df.iloc[NZ_OFFSET:].reset_index(drop=True).astype(float)
ts = d["CH0"].values
t = (ts - ts[0]) / 1e6
pos, spd = d["CH1"].values, d["CH2"].values
trq, chirp = d["CH3"].values, d["CH9"].values

ci = np.where(np.abs(chirp) > 1e-6)[0]
i0, i1 = ci[0], ci[-1]
t_s, t_e = t[i0], t[i1]
n = int((t_e - t_s) * FS) + 1
tg = np.linspace(t_s, t_e, n)
dt = 1 / FS
pos_g = np.interp(tg, t, pos)
spd_g = np.interp(tg, t, spd)
trq_g = np.interp(tg, t, trq)

J = 8.3e-3

def simulate(G, cp, cn, Tcp, Tcn, tau_s):
    om = np.zeros(n); om[0] = spd_g[0]
    invJ = 1.0 / J
    net = trq_g - G * np.cos(pos_g)
    for i in range(1, n):
        o = om[i-1]
        fric = (cp*o + Tcp) if o > 0 else ((cn*o - Tcn) if o < 0 else 0.0)
        om[i] = o + (net[i-1] - fric) * invJ * dt
    sh = int(round(tau_s * FS))
    if sh > 0:
        om = np.concatenate([[om[0]]*sh, om[:-sh]])
    return om

def obj(x):
    G, cp, cn, Tcp, Tcn, tau = x
    G = max(G, 0.05); cp = max(cp, 0); cn = max(cn, 0)
    Tcp = max(Tcp, 0); Tcn = max(Tcn, 0); tau = max(tau, 0)
    om = simulate(G, cp, cn, Tcp, Tcn, tau)
    return float(np.sqrt(np.mean((om - spd_g) ** 2)))

best = None
for g0, cp0, tcp0, tau0 in [(0.28, 0.06, 0.0, 5e-3), (0.25, 0.05, 0.05, 0.0),
                            (0.30, 0.08, 0.0, 8e-3), (0.28, 0.06, 0.05, 2e-3)]:
    r = minimize(obj, [g0, cp0, cp0, tcp0, tcp0, tau0], method="Nelder-Mead",
                 options={"maxiter": 600, "xatol": 1e-9, "fatol": 1e-7, "adaptive": True})
    if best is None or r.fun < best.fun:
        best = r
G, cp, cn, Tcp, Tcn, tau = best.x
G = max(G, 0.05); cp = max(cp, 0); cn = max(cn, 0)
Tcp = max(Tcp, 0); Tcn = max(Tcn, 0); tau = max(tau, 0)
om = simulate(G, cp, cn, Tcp, Tcn, tau)
sr = spd_g.max() - spd_g.min()
rmse = np.sqrt(np.mean((om - spd_g) ** 2))
print("===== 最终辨识 (固定 J=%.2e) =====" % J)
print(f"G   (重力)   = {G:.4f} Nm")
print(f"cv+ = {cp:.5f}   cv- = {cn:.5f}  Nm·s/rad")
print(f"Tc+ = {Tcp:.5f}   Tc- = {Tcn:.5f}  Nm")
print(f"τ   = {tau*1000:.1f} ms")
print(f"速度RMSE={rmse:.4f}  NRMSE={rmse/sr*100:.1f}%")

# 位置验证
pos_sim = pos_g[0] + np.cumsum(om) * dt
print(f"位置RMSE={np.sqrt(np.mean((pos_sim-pos_g)**2)):.4f} rad")

# 重力敏感度: G 取值 0.26/0.28/0.30 时的最优摩擦与 RMSE
print("\n重力敏感度 (固定J, 扫G):")
for Gt in [0.26, 0.28, 0.30]:
    def objG(x):
        cp, cn, Tcp, Tcn, tau = x
        cp = max(cp, 0); cn = max(cn, 0); Tcp = max(Tcp, 0); Tcn = max(Tcn, 0); tau = max(tau, 0)
        om = simulate(Gt, cp, cn, Tcp, Tcn, tau)
        return float(np.sqrt(np.mean((om - spd_g) ** 2)))
    r = minimize(objG, [cp, cn, 0.01, 0.01, tau], method="Nelder-Mead",
                 options={"maxiter": 400, "xatol": 1e-9, "fatol": 1e-7, "adaptive": True})
    print(f"  G={Gt}: cv+={max(r.x[0],0):.5f} cv-={max(r.x[1],0):.5f} "
          f"Tc+={max(r.x[2],0):.5f} Tc-={max(r.x[3],0):.5f} RMSE={r.fun:.4f}")
