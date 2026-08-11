#!/usr/bin/env python3
"""探查2：去重后分析 3_pitchup 多正弦数据"""
import sys, os
import numpy as np
import pandas as pd

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plot_data.csv")
df = pd.read_csv(CSV)

ts = df["CH0"].to_numpy(np.float64)
# 去重（保留每个唯一时间戳的最后一个/第一个？用第一个，保序）
uniq_idx = np.concatenate(([True], np.diff(ts) > 0))
print(f"去重后唯一样本: {uniq_idx.sum()}  (总 {len(ts)})")

d = df.to_numpy(np.float64)[uniq_idx]  # 17列: Time, CH0..CH15
t = (d[:, 1] - d[0, 1]) * 1e-6
print(f"去重后时长: {t[-1]:.3f} s")

dt = np.diff(t)
print(f"dt: 中位={np.median(dt)*1000:.3f}ms  p5={np.percentile(dt,5)*1000:.3f} p95={np.percentile(dt,95)*1000:.3f}")

pos = d[:, 2]   # CH1
spd = d[:, 3]   # CH2
trq = d[:, 4]   # CH3
gff = d[:, 8]   # CH7
multi = d[:, 10]  # CH9
setref = d[:, 13] # CH12
spd_dn = d[:, 14] # CH13 pitchdown speed
pos_dn = d[:, 15] # CH14 pitchdown position
spd_up = d[:, 16] # CH15 pitchup speed

active = np.abs(multi) > 1e-6
print(f"\n激励段: {active.sum()} 样本, t=[{t[active].min():.3f}, {t[active].max():.3f}] s")
print(f"激励段时长: {t[active].max()-t[active].min():.3f} s  (周期数={ (t[active].max()-t[active].min())/2:.1f})")

# 激励段位置/速度
m = active
print(f"\n激励段 位置: [{pos[m].min():+.3f},{pos[m].max():+.3f}]")
print(f"激励段 速度: [{spd[m].min():+.3f},{spd[m].max():+.3f}]")
print(f"激励段 setref: [{setref[m].min():+.3f},{setref[m].max():+.3f}]")
print(f"激励段 multi:  min={multi[m].min():+.3f} max={multi[m].max():+.3f} std={multi[m].std():.4f}")

# pitchdown 是否静止
print(f"\npitchdown speed (CH13) 激励段: min={spd_dn[m].min():+.4f} max={spd_dn[m].max():+.4f}")
print(f"pitchdown pos  (CH14) 激励段: min={pos_dn[m].min():+.4f} max={pos_dn[m].max():+.4f}")

# 激励段相对参考时刻（delay后）
t0 = t[m].min()
print(f"\n多正弦起始: t={t0:.3f}s  结束: t={t[m].max():.3f}s")

# FFT 检查激励频谱（确认 0.5~5Hz 10个峰）
mm = multi[m].copy()
# 重采样到均匀网格（用激励段）
n_resamp = int((t[m].max() - t0) * 1000) + 1
tuni = t0 + np.arange(n_resamp) / 1000.0
multi_r = np.interp(tuni, t[m], mm)
# 去均值后 FFT
multi_r = multi_r - multi_r.mean()
w = np.hanning(len(multi_r))
spec = np.abs(np.fft.rfft(multi_r * w)) * 2 / np.sum(w)
freq = np.fft.rfftfreq(len(multi_r), 0.001)
mask = (freq >= 0.25) & (freq <= 6)
pk_idx = np.argsort(spec[mask])[-12:]
pk_f = np.sort(freq[mask][pk_idx])
pk_v = spec[mask][pk_idx]
order = np.argsort(freq[mask][pk_idx])
print(f"\n激励 FFT 最强峰频率: {np.round(freq[mask][pk_idx][order],3)}")
print(f"  理论: 0.5~5.0 Hz 步长0.5 (共10个)")

# 位置 FFT 峰值
mm2 = pos[m].copy()
pos_r = np.interp(tuni, t[m], mm2)
pos_r = pos_r - pos_r.mean()
spec2 = np.abs(np.fft.rfft(pos_r * w)) * 2 / np.sum(w)
pk_idx2 = np.argsort(spec2[mask])[-12:]
print(f"位置 FFT 最强峰频率: {np.round(np.sort(freq[mask][pk_idx2]),3)}")

# 保存去重数据备用
np.savez_compressed(os.path.join(os.path.dirname(CSV), "dedup.npz"),
                    t=t, pos=pos, spd=spd, trq=trq, gff=gff,
                    multi=multi, setref=setref)
print("\n已保存去重数据 dedup.npz")
