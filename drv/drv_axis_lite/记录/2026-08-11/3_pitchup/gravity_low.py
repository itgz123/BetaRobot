#!/usr/bin/env python3
"""低速段(|v|<0.6) pos分箱分离重力与库仑摩擦 (避免加速度微分污染)"""
import sys, os
import numpy as np
from scipy.optimize import curve_fit

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
pos_r = rs(pos); spd_r = rs(spd); trq_r = rs(trq)

# 低速段
low = np.abs(spd_r) < 0.6
print(f"低速段(|v|<0.6): {low.sum()} 样本 ({low.sum()/len(spd_r)*100:.1f}%)")
print(f"  低速段 pos: [{pos_r[low].min():+.3f}, {pos_r[low].max():+.3f}]")

pb = np.linspace(-0.15, 0.50, 14)
print("\n" + "="*88)
print("  低速段 trq 按 pos 分箱, 正/负速度分开")
print("="*88)
print(f"  {'pos箱':>14s} {'trq+中位':>9s} {'trq-中位':>9s} {'(±)/2 重力':>11s} {'(±)差/2':>9s} {'n+':>5s} {'n-':>5s}")
mids = []; grav = []; coul = []
for i in range(len(pb)-1):
    m = (pos_r >= pb[i]) & (pos_r < pb[i+1]) & low
    mp = m & (spd_r > 0.15)
    mn = m & (spd_r < -0.15)
    if mp.sum() < 20 or mn.sum() < 20: continue
    tp = np.median(trq_r[mp]); tn = np.median(trq_r[mn])
    pc = (pb[i]+pb[i+1])/2
    mids.append(pc); grav.append((tp+tn)/2); coul.append((tp-tn)/2)
    print(f"  [{pb[i]:+.2f},{pb[i+1]:+.2f}]  {tp:+8.4f}  {tn:+8.4f}  "
          f"{(tp+tn)/2:+10.4f}  {(tp-tn)/2:+8.4f}   {mp.sum():4d}  {mn.sum():4d}")

mids = np.array(mids); grav = np.array(grav); coul = np.array(coul)

# 拟合重力: G·cos(th0+pos)
def fgrav(x, G, th0):
    return G*np.cos(th0 + x)
try:
    popt, pcov = curve_fit(fgrav, mids, grav, p0=[0.3, -0.5], bounds=([0, -3], [1, 1]))
    G, th0 = popt
    perr = np.sqrt(np.diag(pcov))
    print(f"\n重力拟合: G={G:.4f}±{perr[0]:.4f}  θ0={th0:+.3f}±{perr[1]:.3f} rad ({np.degrees(th0):+.1f}°)")
    # 残差
    r = grav - fgrav(mids, G, th0)
    print(f"  拟合残差 RMSE={np.sqrt(np.mean(r**2)):.4f}  (重力量级 {np.abs(grav).mean():.3f})")
except Exception as e:
    print("重力拟合失败:", e)

# 库仑
print(f"\n库仑 (trq+ - trq-)/2 中位 = {np.median(coul):.4f} Nm  (低速=库仑Tc)")
print(f"  随pos变化: std={coul.std():.4f} (应近似常数)")

# 与配置/旧辨识对比
print("\n对比:")
print(f"  配置: G=0.28, θ0=0, Tc=0.02")
print(f"  旧辨识(旧数据): G=0.284, θ0=-0.80rad(-46°), Tc=0.055")
