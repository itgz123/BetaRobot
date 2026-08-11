#!/usr/bin/env python3
"""检查高速段组成: 负向 vs 正向, 加速度尖峰, T_fric 成分"""
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
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); trq_r = rs(trq); gff_r = rs(gff)

win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)
# 更宽平滑的加速度 (对比)
acc_w = savgol_filter(spd_r, 51, 3, deriv=1, delta=1/fs)

print("="*80)
print("  负向高速段 (v < -2) 组成")
print("="*80)
for name, mask in [("v<-2", spd_r < -2), ("-2<v<-1", (spd_r<-1)&(spd_r>=-2)),
                   ("v>+1.5", spd_r > 1.5), ("1<v<1.5", (spd_r>1)&(spd_r<=1.5))]:
    m = mask
    if m.sum() < 10: continue
    print(f"\n  [{name}] n={m.sum()}")
    print(f"    pos   : [{pos_r[m].min():+.3f}, {pos_r[m].max():+.3f}] 均值={pos_r[m].mean():+.3f}")
    print(f"    spd   : [{spd_r[m].min():+.3f}, {spd_r[m].max():+.3f}]")
    print(f"    trq   : [{trq_r[m].min():+.3f}, {trq_r[m].max():+.3f}] 均值={trq_r[m].mean():+.3f}")
    print(f"    multi : [{multi_r[m].min():+.3f}, {multi_r[m].max():+.3f}] 均值={multi_r[m].mean():+.3f}")
    print(f"    gff   : [{gff_r[m].min():+.3f}, {gff_r[m].max():+.3f}]")
    print(f"    J*a(21): [{acc_r[m].min():+.3f}, {acc_r[m].max():+.3f}]  (J=0.008)")
    print(f"    J*a(51): [{acc_w[m].min():+.3f}, {acc_w[m].max():+.3f}]")

# 加速度尖峰检查: |a| 分布
print("\n" + "="*80)
print("  加速度分布 (SG-21)")
print("="*80)
for lo, hi in [(-60,-20),(-20,-5),(-5,5),(5,20),(20,60)]:
    m = (acc_r >= lo) & (acc_r < hi)
    print(f"  a[{lo:+4d},{hi:+3d}]: {m.sum():5d}  ({m.sum()/len(acc_r)*100:.1f}%)")

# 停滞-滑动: 检查|a|大但|v|小的样本 (滑动起始)
slip = (np.abs(acc_r) > 15) & (np.abs(spd_r) < 0.5)
print(f"\n  |a|>15 且 |v|<0.5 (滑动起始): {slip.sum()} 个")
if slip.sum():
    print(f"    pos: [{pos_r[slip].min():+.3f}, {pos_r[slip].max():+.3f}]")
    print(f"    trq: [{trq_r[slip].min():+.3f}, {trq_r[slip].max():+.3f}] 均值={trq_r[slip].mean():+.3f}")

# 速度-加速度相图: 停滞区域的 a 分布
print("\n" + "="*80)
print("  T_fric = trq - J*a - G*cos(th0+pos), 用辨识 θ0=-0.485 检查")
print("="*80)
Gx, th0x = 0.3085, -0.485
Tf_a = trq_r - 0.008*acc_r - Gx*np.cos(th0x + pos_r)
Tf_w = trq_r - 0.008*acc_w - Gx*np.cos(th0x + pos_r)
vb = np.linspace(-3.5, 2.6, 27)
print(f"  {'v':>10s} {'n':>5s} {'Tf(a21)中位':>11s} {'Tf(a51)中位':>11s}")
for i in range(len(vb)-1):
    m = (spd_r >= vb[i]) & (spd_r < vb[i+1])
    if m.sum() < 30: continue
    print(f"  [{vb[i]:+.2f},{vb[i+1]:+.2f}) {m.sum():4d}  {np.median(Tf_a[m]):+10.4f}   {np.median(Tf_w[m]):+10.4f}")
