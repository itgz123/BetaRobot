#!/usr/bin/env python3
"""高速样本时域 OLS + 仿真验证 (收敛辨识)"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter

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
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); gff_r = rs(gff); trq_r = rs(trq)
n = len(tuni)

win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)

# ============ 1. 高速样本时域 OLS, 扫描速度延迟 ============
print("="*70)
print("  [1] 时域 OLS (y=trq-gff, |v|>1), 扫描速度延迟 τ")
print("="*70)
def ols_tau(tau_ms, vthr=1.0):
    tau = tau_ms*1e-3
    t_sh = tuni + tau
    spd_sh = np.interp(t_sh, tuni, spd_r)   # 速度前移(消除滞后)
    I_p = (spd_sh > 0).astype(float); I_n = (spd_sh < 0).astype(float)
    mask = (np.abs(spd_sh) > vthr) & (tuni > tuni[0]+0.5) & (tuni < tuni[-1]-0.5)
    m = mask
    X = np.column_stack([acc_r[m], spd_sh[m]*I_p[m], spd_sh[m]*I_n[m],
                         I_p[m], I_n[m], np.cos(pos_r[m])])
    y = (trq_r[m] - gff_r[m])
    th, res, rank, sv = np.linalg.lstsq(X, y, rcond=None)
    r2 = 1 - np.sum(res**2)/max(np.sum((y-y.mean())**2), 1e-20)
    return th, np.sqrt(np.mean(res**2)), r2, len(m)

labels = ["J", "cv+", "cv-", "Tc+", "Tc-", "dG"]
print(f"  {'τ':>4s} {'J':>9s} {'cv+':>8s} {'cv-':>8s} {'Tc+':>8s} {'Tc-':>8s} {'dG':>8s} {'g':>7s} {'RMSE':>7s} {'R²':>6s}")
for tau in range(0, 11):
    th, rmse, r2, npts = ols_tau(tau)
    J, cvp, cvn, Tcp, Tcn, dG = th
    g_real = gff_r.mean() + dG
    print(f"  {tau:4d} {J:9.5f} {cvp:8.4f} {cvn:8.4f} {Tcp:8.4f} {Tcn:8.4f} {dG:8.3f} {g_real:7.3f} {rmse:7.4f} {r2:6.3f} ({npts})")

# ============ 2. 高速摩擦形状 (固定 J 从[1]最优) ============
print("\n" + "="*70)
print("  [2] 高速摩擦形状: T_fric = trq - gff - J·θ̈  (|v|>0.8)")
print("="*70)
th_b, _, _, _ = ols_tau(2)
J_est = th_b[0]
print(f"  采用 J = {J_est:.5f} (τ=2ms 时 OLS)")
T_fric = trq_r - gff_r - J_est*acc_r
bins = np.linspace(0.8, 5.0, 12)
print(f"  {'|v|':>7s} {'T_fric中位(v>0)':>16s} {'T_fric中位(v<0)':>16s} {'n':>5s}")
for i in range(len(bins)-1):
    lo, hi = bins[i], bins[i+1]
    mp = (spd_r > lo) & (spd_r <= hi)
    mn = (spd_r < -lo) & (spd_r >= -hi)
    tp = np.median(T_fric[mp]) if mp.sum()>20 else np.nan
    tn = np.median(T_fric[mn]) if mn.sum()>20 else np.nan
    print(f"  {lo:7.2f} {tp:16.4f} {tn:16.4f} {mp.sum()+mn.sum():5d}")

# ============ 3. 仿真验证 ============
print("\n" + "="*70)
print("  [3] 开环仿真 vs 实测速度 (用 τ=2ms 参数)")
print("="*70)
th, _, _, _ = ols_tau(2)
J, cvp, cvn, Tcp, Tcn, dG = th
g_real = gff_r.mean() + dG
print(f"  参数: J={J:.5f} cv+={cvp:.4f} cv-={cvn:.4f} Tc+={Tcp:.4f} Tc-={Tcn:.4f} g={g_real:.4f}")

def simulate(y_cmd, gff_in, pos0, spd0, J, cvp, cvn, Tcp, Tcn, g_real, n_steps):
    p = np.zeros(n_steps); v = np.zeros(n_steps)
    p[0], v[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vv = v[i-1]
        if vv > 0: fric = cvp*vv + Tcp
        elif vv < 0: fric = cvn*vv - Tcn
        else: fric = 0.0
        a = (y_cmd[i] + gff_in[i] - g_real*np.cos(p[i-1]) - fric) * invJ
        v[i] = vv + a/fs
        p[i] = p[i-1] + vv/fs
    return p, v

# 3a. 用命令力矩 + gff 命令 仿真 (真实执行模型)
n_sim = 10000  # 10s
y_cmd = multi_r[:n_sim]
gff_in = gff_r[:n_sim]
p_sim, v_sim = simulate(y_cmd, gff_in, pos_r[0], spd_r[0], J, cvp, cvn, Tcp, Tcn, g_real, n_sim)
e_v = np.sqrt(np.mean((v_sim - spd_r[:n_sim])**2))
e_p = np.sqrt(np.mean((p_sim - pos_r[:n_sim])**2))
rng_v = spd_r[:n_sim].max() - spd_r[:n_sim].min()
rng_p = pos_r[:n_sim].max() - pos_r[:n_sim].min()
print(f"  [命令力矩] 速度RMSE={e_v:.3f} rad/s (NRMSE={e_v/rng_v*100:.1f}%)  "
      f"位置RMSE={e_p:.4f} rad (NRMSE={e_p/rng_p*100:.1f}%)")

# 3b. 用实际力矩 仿真
y_cmd2 = trq_r[:n_sim]
p_sim2, v_sim2 = simulate(y_cmd2, gff_in, pos_r[0], spd_r[0], J, cvp, cvn, Tcp, Tcn, g_real, n_sim)
e_v2 = np.sqrt(np.mean((v_sim2 - spd_r[:n_sim])**2))
e_p2 = np.sqrt(np.mean((p_sim2 - pos_r[:n_sim])**2))
print(f"  [实际力矩] 速度RMSE={e_v2:.3f} rad/s (NRMSE={e_v2/rng_v*100:.1f}%)  "
      f"位置RMSE={e_p2:.4f} rad (NRMSE={e_p2/rng_p*100:.1f}%)")

# 3c. 与配置参数对比仿真 (J=0.008, cv=0.05, Tc=0.02, g=0.28)
p_sim3, v_sim3 = simulate(y_cmd, gff_in, pos_r[0], spd_r[0],
                          0.008, 0.05, 0.06, 0.02, 0.0, 0.28, n_sim)
e_v3 = np.sqrt(np.mean((v_sim3 - spd_r[:n_sim])**2))
e_p3 = np.sqrt(np.mean((p_sim3 - pos_r[:n_sim])**2))
print(f"  [配置参数] 速度RMSE={e_v3:.3f} rad/s (NRMSE={e_v3/rng_v*100:.1f}%)  "
      f"位置RMSE={e_p3:.4f} rad (NRMSE={e_p3/rng_p*100:.1f}%)")
