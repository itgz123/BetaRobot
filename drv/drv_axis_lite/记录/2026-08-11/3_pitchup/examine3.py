#!/usr/bin/env python3
"""探查新数据 (duration=1s, 基频1Hz, 1~10Hz) + 重新生成 dedup.npz"""
import sys, os
import numpy as np
import pandas as pd

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(BASE, "plot_data.csv")

df = pd.read_csv(CSV)
print("列:", list(df.columns))
print("形状:", df.shape)

# CH0 = DWT us
ts = df["CH0"].to_numpy(np.float64)
t = (ts - ts[0]) * 1e-6
print(f"总时长: {t[-1]:.2f} s  首行CH0={ts[0]:.0f}  末行CH0={ts[-1]:.0f}")

# 去重 (唯一时间戳)
uniq, first = np.unique(ts, return_index=True)
dup = len(ts) - len(uniq)
print(f"重复/乱序样本: {dup}  ({dup/len(ts)*100:.1f}%)")
ts_u = uniq
t_u = (ts_u - ts_u[0]) * 1e-6

dt = np.diff(ts_u)
dt_us = np.median(dt)
print(f"中位 dt={dt_us:.3f} us  ->  fs={1e6/dt_us:.1f} Hz")
print(f"dt 5%/95%分位: {np.percentile(dt,5):.1f} / {np.percentile(dt,95):.1f} us")

ch = df["CH9"].to_numpy(np.float64)      # friction_ff = multi_sine
ch1 = df["CH1"].to_numpy(np.float64)     # pos 差动
ch2 = df["CH2"].to_numpy(np.float64)     # spd raw pitchup
ch3 = df["CH3"].to_numpy(np.float64)     # trq 实际力矩
ch7 = df["CH7"].to_numpy(np.float64)     # gff
ch12 = df["CH12"].to_numpy(np.float64)   # setref

# 去重后的数组
pos = ch1[first]; spd = ch2[first]; trq = ch3[first]
gff = ch7[first]; multi = ch[first]; setref = ch12[first]

active = np.abs(multi) > 1e-6
print(f"\n激励(CH9 multi)非零样本: {active.sum()} ({active.sum()/len(pos)*100:.1f}%)")
if active.sum():
    idx = np.where(active)[0]
    t_on = t_u[active].min(); t_off = t_u[active].max()
    print(f"  激励段: t=[{t_on:.3f}, {t_off:.3f}]s  时长={t_off-t_on:.2f}s")
    # 周期数估计: duration=1s
    T = 1.0
    n_cycles = (t_off - t_on) / T
    print(f"  周期(1s)数 ≈ {n_cycles:.1f}")

# 有效段统计
m = active
print(f"\n有效段(激励中)统计:")
for name, v in [("pos", pos), ("spd", spd), ("trq", trq), ("gff", gff),
                ("multi", multi), ("setref", setref)]:
    vv = v[m]
    print(f"  {name:6s}  min={vv.min():+.4f}  max={vv.max():+.4f}  "
          f"mean={vv.mean():+.4f}  std={vv.std():.4f}")

# 限位污染检查
print(f"\n位置范围: [{pos[m].min():+.4f}, {pos[m].max():+.4f}]")
print(f"|pos|>0.55 样本: {(np.abs(pos[m])>0.55).sum()} ({(np.abs(pos[m])>0.55).mean()*100:.1f}%)")
print(f"|pos|>0.67 样本: {(np.abs(pos[m])>0.67).sum()} ({(np.abs(pos[m])>0.67).mean()*100:.1f}%)")

# 静止卡住检测: |spd| 很小但 pos 顶住
still = (np.abs(spd[m]) < 0.1)
print(f"静止样本(|spd|<0.1): {still.mean()*100:.1f}%  (贴限位通常静止)")

# setref vs gff+multi
diff = setref[m] - (gff[m] + multi[m])
print(f"\n setref-(gff+multi): max|.|={np.abs(diff).max():.4f}  (应≈0)")

# 分段位置漂移
seg = 100000
print(f"\n位置分段 (每{seg/1000:.0f}s):")
for i in range(0, len(pos), seg):
    mm = active[i:i+seg]
    if mm.sum() < 100: continue
    v = pos[i:i+seg][mm]; s = spd[i:i+seg][mm]
    print(f"  t={t_u[i]:6.1f}s  pos[{v.min():+.3f},{v.max():+.3f}]  spd[{s.min():+.2f},{s.max():+.2f}]")

# 保存 dedup.npz (字段名与旧版一致)
np.savez(os.path.join(BASE, "dedup.npz"),
         t=t_u, pos=pos, spd=spd, trq=trq, gff=gff, multi=multi, setref=setref)
print(f"\n已保存 dedup.npz ({len(pos)} 样本)")
