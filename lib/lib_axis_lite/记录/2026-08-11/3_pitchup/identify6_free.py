#!/usr/bin/env python3
"""自由运动段(远离限位)时域辨识 + 仿真验证"""
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

# ============ 1. 自由段定义与统计 ============
pos_lim = 0.55
free = (np.abs(pos_r) < pos_lim) & (tuni > tuni[0]+0.3) & (tuni < tuni[-1]-0.3)
print("="*72)
print(f"  自由运动段: |pos|<{pos_lim}  →  {free.sum()} 样本 ({free.sum()/fs:.1f}s, "
      f"{free.sum()/n*100:.0f}%)")
print("="*72)
print(f"  自由段位置范围: [{pos_r[free].min():.2f}, {pos_r[free].max():.2f}]")
print(f"  自由段速度范围: [{spd_r[free].min():.2f}, {spd_r[free].max():.2f}]")
print(f"  自由段 |trq|:   均值 {np.mean(np.abs(trq_r[free])):.3f}  峰值 {np.abs(trq_r[free]).max():.2f}")
print(f"  自由段 |multi|: 均值 {np.mean(np.abs(multi_r[free])):.3f}")

# ============ 2. 自由段时域 OLS (中心化) ============
win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)
I_p = (spd_r > 0).astype(float); I_n = (spd_r < 0).astype(float)
vp = spd_r*I_p; vn = spd_r*I_n
cosT = np.cos(pos_r); sinT = np.sin(pos_r)

m = free
X = np.column_stack([acc_r[m], vp[m], vn[m], I_p[m], I_n[m], cosT[m], sinT[m]])
yc = trq_r[m]
# 中心化
Xc = X - X.mean(axis=0); y0 = yc - yc.mean()
th, res, rank, sv = np.linalg.lstsq(Xc, y0, rcond=None)
J, cvp, cvn, Tcp, Tcn, Gc, Gs = th
pred = Xc@th
r2 = 1 - np.sum(res**2)/max(np.sum(y0**2), 1e-20)
rmse = np.sqrt(np.mean(res**2))
print("\n  自由段时域 OLS:")
print(f"  J    = {J:.5f}")
print(f"  cv+  = {cvp:.4f}   cv- = {cvn:.4f}")
print(f"  Tc+  = {Tcp:.4f}   Tc- = {Tcn:.4f}")
print(f"  Gc   = {Gc:.4f}   Gs = {Gs:.4f}")
G = np.hypot(Gc, Gs); phi0 = np.degrees(np.arctan2(-Gs, Gc))
print(f"  G = {G:.4f} Nm   θ0 = {phi0:+.1f}°")
print(f"  R² = {r2:.4f}   RMSE = {rmse:.4f} Nm")

# 条件数
print(f"  回归矩阵条件数 = {np.linalg.cond(Xc):.1f}")

# ============ 3. 频域检查 (自由段拼接, 窗口FFT) ============
print("\n" + "="*72)
print("  自由段频域 (Welch平均) J 检查")
print("="*72)
from scipy.signal import welch
f_m = free
# 用 Welch 估计互谱/自谱
npseg = 2000
fv, Pxx = welch(trq_r[f_m]-gff_r[f_m], fs=fs, nperseg=npseg, noverlap=npseg//2)
_, Pxv = welch(trq_r[f_m]-gff_r[f_m], spd_r[f_m], fs=fs, nperseg=npseg, noverlap=npseg//2, return_onesided=True, axis=-1)
# 阻抗 H = Pxv/Pxx
with np.errstate(divide='ignore', invalid='ignore'):
    H = np.divide(Pxv, Pxx, out=np.zeros_like(Pxv), where=np.abs(Pxx) > 1e-12)
J_w = []
for f in np.arange(1.5, 5.5, 0.5):
    k = np.argmin(np.abs(fv-f))
    Jf = H[k].imag/(2*np.pi*f)
    if np.isfinite(Jf) and 0 < Jf < 0.02:
        J_w.append(Jf)
        print(f"  f={fv[k]:.2f}Hz: J={Jf:.5f}")
if J_w:
    print(f"  J(1.5-5Hz) 中位 = {np.median(J_w):.5f}")

# ============ 4. 自由段仿真验证 ============
print("\n" + "="*72)
print("  自由段内 1s 滑动仿真 vs 实测 (用辨识参数)")
print("="*72)
def simulate(y_cmd, pos0, spd0, J, cvp, cvn, Tcp, Tcn, Gc, Gs, n_steps):
    p = np.zeros(n_steps); v = np.zeros(n_steps)
    p[0], v[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vv = v[i-1]
        if vv > 0: fric = cvp*vv + Tcp
        elif vv < 0: fric = cvn*vv - Tcn
        else: fric = 0.0
        g = Gc*np.cos(p[i-1]) + Gs*np.sin(p[i-1])
        a = (y_cmd[i] - g - fric) * invJ
        v[i] = vv + a/fs
        p[i] = p[i-1] + vv/fs
    return p, v

# 找自由段内的连续子段 (位置远离限位)
# 简单: 每 2s 周期, 取自由运动窗口
cands_list = [
    ("辨识参数", J, cvp, cvn, Tcp, Tcn, Gc, Gs),
    ("辨识(全段)+修正", 0.008, 0.03, 0.03, 0.13, 0.13, Gc, Gs),
    ("配置参数", 0.008, 0.05, 0.06, 0.02, 0.0, 0.28, 0.0),
]
rng_v = spd_r[free].max() - spd_r[free].min()
# 取前几个自由连续段 (span>0.3)
seg_starts = []
i = 0
while i < n - 1000:
    if free[i] and free[i+1000]:
        # 这段连续自由
        s0 = i
        seg_starts.append(s0)
        i += 2000
    else:
        i += 1
print(f"  找到 {len(seg_starts)} 个自由连续段候选")
for name, Jx, cvpx, cvnx, Tcpp, Tcnn, Gcx, Gsx in cands_list:
    v_rmse_list = []
    for s0 in seg_starts[:8]:
        s1 = s0 + 1000
        if s1 > n: break
        # 只仿真自由段内部
        if np.abs(pos_r[s0:s1]).max() > pos_lim: continue
        p_s, v_s = simulate(multi_r[s0:s1], pos_r[s0], spd_r[s0],
                            Jx, cvpx, cvnx, Tcpp, Tcnn, Gcx, Gsx, 1000)
        e_v = np.sqrt(np.mean((v_s[50:-50] - spd_r[s0+50:s1-50])**2))
        v_rmse_list.append(e_v)
    if v_rmse_list:
        mv = np.mean(v_rmse_list)
        print(f"  {name:18s}  自由段速度RMSE = {mv:.3f} rad/s  ({mv/rng_v*100:.1f}%)")
