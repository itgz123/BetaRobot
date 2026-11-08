#!/usr/bin/env python3
"""独立重力验证: mean(trq) vs pos 分箱 (摩擦在平均中抵消)"""
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
pos_r = rs(pos); spd_r = rs(spd); trq_r = rs(trq); gff_r = rs(gff)
n = len(tuni)

# 全段分箱 mean(trq), mean(gff), 摩擦在 v 平均后抵消
pbins = np.linspace(-0.7, 0.8, 16)
print("="*78)
print("  mean(trq) vs pos  (摩擦±抵消 → 应≈真实重力力矩)")
print("="*78)
print(f"  {'pos':>8s} {'mean(trq)':>10s} {'mean(gff)':>10s} {'残差(trq-gff)':>12s} {'n':>7s}")
med_p = []; med_trq = []
for i in range(len(pbins)-1):
    m = (pos_r >= pbins[i]) & (pos_r < pbins[i+1])
    if m.sum() < 50: continue
    pc = np.median(pos_r[m])
    mt = np.mean(trq_r[m])
    mg = np.mean(gff_r[m])
    med_p.append(pc); med_trq.append(mt)
    print(f"  [{pbins[i]:+.2f},{pbins[i+1]:+.2f}] {mt:+10.4f} {mg:+10.4f} {mt-mg:+12.4f} {m.sum():7d}")

# 拟合 G·cos(θ0+p): 最小二乘
med_p = np.array(med_p); med_trq = np.array(med_trq)
# 模型: Gc·cos(p) + Gs·sin(p)
X = np.column_stack([np.cos(med_p), np.sin(med_p)])
th, _, _, _ = np.linalg.lstsq(X, med_trq, rcond=None)
Gc, Gs = th
G = np.hypot(Gc, Gs); phi0 = np.degrees(np.arctan2(-Gs, Gc))
pred = X@th
rmse = np.sqrt(np.mean((med_trq-pred)**2))
print(f"\n  mean(trq) 拟合: Gc={Gc:.4f} Gs={Gs:.4f}")
print(f"  G = {G:.4f} Nm,  θ0 = {phi0:+.1f}°   (分箱拟合 RMSE={rmse:.4f})")
print(f"  → 真实重力力矩 = {G:.3f}·cos({phi0:+.1f}° + pos)")

# 配置模型对比
print("\n  配置 gff = 0.28·cos(pos):")
cfg_pred = 0.28*np.cos(med_p)
print(f"  与 mean(trq) 的残差 RMSE = {np.sqrt(np.mean((med_trq-cfg_pred)**2)):.4f} Nm")

# 也拟合 mean(trq-gff)
mg2 = med_trq - 0.28*np.cos(med_p)
th2, _, _, _ = np.linalg.lstsq(X, mg2, rcond=None)
Gc2, Gs2 = th2
print(f"\n  mean(trq-0.28cos): 残差拟合 Gc'={Gc2:.4f} Gs'={Gs2:.4f}")
print(f"  → 等效 G'={np.hypot(Gc2,Gs2):.4f}, θ0'={np.degrees(np.arctan2(-Gs2,Gc2)):+.1f}°")

# 用 G, θ0 的模型预测并对比
print("\n  G·cos(θ0+p) 预测 vs mean(trq):")
for p, mtrq in zip(med_p, med_trq):
    pm = G*np.cos(np.radians(phi0)+p)
    print(f"  pos={p:+.2f}  mean(trq)={mtrq:+.4f}  模型={pm:+.4f}  差={mtrq-pm:+.4f}")
