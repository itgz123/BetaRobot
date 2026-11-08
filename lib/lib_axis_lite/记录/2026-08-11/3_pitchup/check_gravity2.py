#!/usr/bin/env python3
"""最终验证: 修正重力角度(θ0)后, mean(trq-重力) 应平坦"""
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
pos_r = rs(pos); trq_r = rs(trq)
n = len(tuni)

print("="*78)
print("  mean(trq) - 重力模型  随 pos 分箱  (平坦 → 模型正确)")
print("="*78)
pbins = np.linspace(-0.7, 0.8, 16)

def model_resid(G, th0):
    """mean(trq - G·cos(th0+pos)) 按 pos 分箱的中位数组"""
    vals = []
    for i in range(len(pbins)-1):
        m = (pos_r >= pbins[i]) & (pos_r < pbins[i+1])
        if m.sum() < 50: continue
        r = trq_r[m] - G*np.cos(th0 + pos_r[m])
        vals.append(np.median(r))
    return vals

# 配置重力: 0.28·cos(pos)
cfg = model_resid(0.28, 0.0)
# 辨识重力: 0.284·cos(pos-0.79)
idn = model_resid(0.284, -0.79)
# 贴限位静止点拟合 (v≈0, 顶限位): trq=G·cos(th0+pos)
# 简化: 只统计平坦度
print(f"  {'pos':>8s} {'配置残差':>10s} {'辨识残差':>10s}")
vals_cfg = []; vals_idn = []
for i in range(len(pbins)-1):
    m = (pos_r >= pbins[i]) & (pos_r < pbins[i+1])
    if m.sum() < 50: continue
    pc = (pbins[i]+pbins[i+1])/2
    print(f"  [{pbins[i]:+.2f},{pbins[i+1]:+.2f}] {cfg[i]:+10.4f} {idn[i]:+10.4f}  ({m.sum()})")
    vals_cfg.append(cfg[i]); vals_idn.append(idn[i])
vals_cfg = np.array(vals_cfg); vals_idn = np.array(vals_idn)
print(f"\n  残差散布 (中位数的std, 越小越平坦):")
print(f"    配置重力: std={vals_cfg.std():.4f}  (波动 {vals_cfg.min():+.3f}~{vals_cfg.max():+.3f})")
print(f"    辨识重力: std={vals_idn.std():.4f}  (波动 {vals_idn.min():+.3f}~{vals_idn.max():+.3f})")

# 网格搜索最佳 (G, θ0) 使残差最平坦
print("\n  网格搜索最佳 (G, θ0) 使分箱残差 std 最小:")
best = None
for G in np.arange(0.24, 0.36, 0.01):
    for th0 in np.arange(-1.5, 0.1, 0.02):
        vv = np.array(model_resid(G, th0))
        s = vv.std()
        if best is None or s < best[0]:
            best = (s, G, th0)
print(f"  最佳: G={best[1]:.3f}, θ0={best[2]:+.3f} rad ({np.degrees(best[2]):+.1f}°), std={best[0]:.4f}")
print(f"  辨识给出: G=0.284, θ0=-0.79 ({np.degrees(-0.79):+.1f}°)")
