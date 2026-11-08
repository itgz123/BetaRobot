#!/usr/bin/env python3
"""画激励段时间序列: pos/spd/trq/multi, 观察运动模式"""
import sys, os
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

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

# 取中间 3s
s = int(len(tuni)*0.35); e = s + 3000
te = tuni[s:e] - t0

fig, ax = plt.subplots(3, 1, figsize=(13, 9), sharex=True)
ax[0].plot(te, pos_r[s:e], 'b', lw=0.8, label='pos')
ax[0].axhline(0, color='k', lw=0.5, ls='--')
ax[0].set_ylabel("pos [rad]"); ax[0].legend(); ax[0].grid(alpha=0.3)
ax[1].plot(te, spd_r[s:e], 'g', lw=0.8, label='spd')
ax[1].axhline(0, color='k', lw=0.5, ls='--')
ax[1].set_ylabel("spd [rad/s]"); ax[1].legend(); ax[1].grid(alpha=0.3)
ax[2].plot(te, multi_r[s:e], 'r', lw=0.6, label='multi(cmd)')
ax[2].plot(te, trq_r[s:e], 'k', lw=0.6, label='trq(act)')
ax[2].plot(te, gff_r[s:e], 'm', lw=0.6, label='gff')
ax[2].set_ylabel("torque [Nm]"); ax[2].legend(); ax[2].grid(alpha=0.3)
ax[2].set_xlabel(f"t - t0 [s]  (t0={t0:.1f}s)")
plt.tight_layout()
plt.savefig(os.path.join(BASE, "timeseries.png"), dpi=110)
print("saved timeseries.png")

# 量化: 停滞段 (|v|<0.1) 的连续长度分布
still = np.abs(spd_r) < 0.1
# 连续段长度
runs = []
cnt = 0
for b in still:
    if b: cnt += 1
    else:
        if cnt: runs.append(cnt); cnt = 0
if cnt: runs.append(cnt)
runs = np.array(runs)
print(f"\n停滞段(|v|<0.1)数量: {len(runs)}, 平均长 {runs.mean():.0f}ms, "
      f"中位 {np.median(runs):.0f}ms, 最长 {runs.max():.0f}ms")
print(f"  >200ms 的停滞段: {(runs>200).sum()} 个 ({(runs>200).sum()/len(runs)*100:.0f}%)")
print(f"  >500ms 的停滞段: {(runs>500).sum()} 个")

# 正向/负向高速样本量
print(f"v>+1.5: {(spd_r>1.5).sum()},  v<-1.5: {(spd_r<-1.5).sum()}")
print(f"pos>0.3: {(pos_r>0.3).mean()*100:.1f}%,  pos<-0.05: {(pos_r<-0.05).mean()*100:.1f}%")
