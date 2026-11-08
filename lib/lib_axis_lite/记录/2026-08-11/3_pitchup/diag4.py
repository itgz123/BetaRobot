#!/usr/bin/env python3
"""新数据诊断: 运动模式 / 静止段 / 频域J / 摩擦分箱"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter, csd
import scipy
print(f"scipy {scipy.__version__}")

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
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); trq_r = rs(trq); gff_r = rs(gff)
n = len(tuni)
print(f"激励段重采样: {n} 样本 @ {fs:.0f}Hz = {n/fs:.2f}s")

# ============ 1. 速度直方图 & 静止段 ============
print("\n" + "="*72)
print("  速度分布 (激励段)")
print("="*72)
bins = np.arange(-4, 3, 0.25)
h, be = np.histogram(spd_r, bins=bins)
for i in range(len(h)):
    bar = "#" * int(h[i]/max(h.max(),1)*60)
    print(f"  v[{be[i]:+.2f},{be[i+1]:+.2f}) n={h[i]:5d} {bar}")

still = np.abs(spd_r) < 0.1
print(f"\n静止段(|v|<0.1): {still.mean()*100:.1f}%")
# 静止段的分布位置
print(f"  静止段 pos: [{pos_r[still].min():+.3f},{pos_r[still].max():+.3f}]  "
      f"均值={pos_r[still].mean():+.3f}")
print(f"  静止段 trq: [{trq_r[still].min():+.3f},{trq_r[still].max():+.3f}]  "
      f"均值={trq_r[still].mean():+.3f}")
print(f"  静止段 multi: [{multi_r[still].min():+.3f},{multi_r[still].max():+.3f}]")

# ============ 2. 频域阻抗 J ============
print("\n" + "="*72)
print("  频域阻抗 J (实际力矩 trq, Welch)")
print("="*72)
npseg = 2000
fv, Pxx = csd(spd_r, spd_r, fs=fs, nperseg=npseg, noverlap=npseg//2)
fv2, Pxv = csd(spd_r, trq_r - gff_r, fs=fs, nperseg=npseg, noverlap=npseg//2)
with np.errstate(divide='ignore', invalid='ignore'):
    Z = np.divide(Pxv, Pxx, out=np.zeros_like(Pxv), where=np.abs(Pxx) > 1e-12)
Jlist = []
for f in np.arange(1.0, 10.5, 0.5):
    k = np.argmin(np.abs(fv-f))
    Jf = Z[k].imag/(2*np.pi*f)
    mark = ""
    if np.isfinite(Jf) and 0 < Jf < 0.03:
        Jlist.append(Jf); mark = "  <-- 合理"
    print(f"  f={fv[k]:5.2f}Hz  J=Im(Z)/w={Jf:+.5f}{mark}")
if Jlist:
    print(f"\n  J(1-10Hz 合理值) 中位 = {np.median(Jlist):.5f}")

# ============ 3. 摩擦辨识: 扣除 J·a + 重力 ============
print("\n" + "="*72)
print("  摩擦分箱: T_fric = trq - J·a - G·cos(θ0+pos)  vs v")
print("  先用初值 J=0.008, G=0.28, θ0=0 看形状")
print("="*72)
win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)
Jx, Gx, th0x = 0.008, 0.28, 0.0
Tf = trq_r - Jx*acc_r - Gx*np.cos(th0x + pos_r)
vb = np.linspace(-3.5, 3.0, 28)
print(f"  {'v':>10s} {'n':>6s} {'T_fric中位':>10s} {'T_fric均值':>10s}")
for i in range(len(vb)-1):
    m = (spd_r >= vb[i]) & (spd_r < vb[i+1])
    if m.sum() < 30: continue
    med = np.median(Tf[m]); mean = np.mean(Tf[m])
    bar = "#" * int(abs(med)/max(abs(Tf).max(),1e-9)*50)
    print(f"  [{vb[i]:+.2f},{vb[i+1]:+.2f}) {m.sum():5d}  {med:+9.4f}   {mean:+9.4f}  {bar}")
