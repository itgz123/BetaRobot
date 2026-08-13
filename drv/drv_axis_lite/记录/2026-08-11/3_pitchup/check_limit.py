#!/usr/bin/env python3
"""限位验证: pos 直方图 + 时序 + 边界处力矩"""
import sys, os
import numpy as np

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
pos_r = rs(pos); spd_r = rs(spd); trq_r = rs(trq); multi_r = rs(multi)
n = len(tuni)

print("="*72)
print("  [1] 位置分布直方图 (激励段)")
print("="*72)
hist, edges = np.histogram(pos_r, bins=24, range=(-0.85, 0.85))
print(f"  {'pos区间':>12s} {'计数':>7s} {'占比%':>7s}")
for i in range(len(hist)):
    frac = hist[i]/n*100
    bar = '#'*int(frac/2)
    print(f"  [{edges[i]:+.2f},{edges[i+1]:+.2f}] {hist[i]:7d} {frac:6.1f}  {bar}")

# 边界占比
n_pos_low = (pos_r < -0.55).sum()
n_pos_high = (pos_r > 0.6).sum()
print(f"\n  pos<-0.55: {n_pos_low} ({n_pos_low/n*100:.0f}%)   pos>+0.6: {n_pos_high} ({n_pos_high/n*100:.0f}%)")

print("\n" + "="*72)
print("  [2] 位置时序 (找卡住段)")
print("="*72)
# 每0.5s 统计位置范围
seg = 500
for i in range(0, n-seg, seg):
    segpos = pos_r[i:i+seg]
    segtrq = trq_r[i:i+seg]
    print(f"  t={tuni[i]:7.2f}s  pos[{segpos.min():+.2f},{segpos.max():+.2f}]  "
          f"span={segpos.max()-segpos.min():.2f}  |trq|max={np.abs(segtrq).max():.2f}")

print("\n" + "="*72)
print("  [3] 边界 vs 中间段 数据特征")
print("="*72)
for name, m in [("pos<-0.55", pos_r < -0.55),
                ("-0.3<pos<0.3", (pos_r > -0.3) & (pos_r < 0.3)),
                ("pos>+0.6", pos_r > 0.6)]:
    print(f"  {name:14s}: n={m.sum():5d}  |spd|max={np.abs(spd_r[m]).max():.2f}  "
          f"mean|trq|={np.mean(np.abs(trq_r[m])):.3f}  "
          f"|multi|mean={np.mean(np.abs(multi_r[m])):.3f}")

# 中间段的加速度/速度特征 (自由运动)
m_mid = (pos_r > -0.3) & (pos_r < 0.3)
print(f"\n  中间段(-0.3~0.3) 速度范围: [{spd_r[m_mid].min():.2f}, {spd_r[m_mid].max():.2f}]")

# 检查 0.5Hz 分量的位置幅值理论
A = 0.1; J = 0.008
for f in [0.5, 1.0, 1.5, 2.0]:
    pos_amp = A/(J*(2*np.pi*f)**2)
    print(f"  理论位置幅值 @{f}Hz: {pos_amp:.3f} rad")
