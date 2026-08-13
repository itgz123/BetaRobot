#!/usr/bin/env python3
"""探查 3_pitchup 多正弦辨识数据 (IDENTIFY_OLS 阶段)"""
import sys, os
import numpy as np
import pandas as pd

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

CSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plot_data.csv")

# 读取（首列 Time 是电脑时间戳，忽略；CH0 是 DWT us）
df = pd.read_csv(CSV)
print("列:", list(df.columns))
print("形状:", df.shape)

# CH0 = DWT us
ts = df["CH0"].to_numpy(np.float64)
t = ts - ts[0]
print(f"总时长: {t[-1]*1e-6:.2f} s  首行CH0={ts[0]:.0f}  末行CH0={ts[-1]:.0f}")

# 去重
dup = np.sum(np.diff(ts) <= 0)
print(f"重复/乱序样本: {dup}")

# 采样间隔统计
dt = np.diff(ts)
dt_us = np.median(dt)
print(f"中位 dt={dt_us:.3f} us  ->  fs={1e6/dt_us:.1f} Hz")
print(f"dt 5%/95%分位: {np.percentile(dt,5):.1f} / {np.percentile(dt,95):.1f} us")

# 通道
ch = df["CH9"].to_numpy(np.float64)  # friction_ff = multi_sine 激励
ch12 = df["CH12"].to_numpy(np.float64)  # setref
ch1 = df["CH1"].to_numpy(np.float64)   # 位置（差动）
ch2 = df["CH2"].to_numpy(np.float64)   # 速度（raw pitchup）
ch3 = df["CH3"].to_numpy(np.float64)   # 实际力矩
ch7 = df["CH7"].to_numpy(np.float64)   # gravity_ff

active = np.abs(ch) > 1e-6
print(f"\n激励(CH9 multi_sine)非零样本: {active.sum()}/{len(ch)}")
if active.sum():
    idx = np.where(active)[0]
    print(f"  首段: {idx[0]} (t={t[idx[0]]*1e-6:.3f}s)  末段: {idx[-1]} (t={t[idx[-1]]*1e-6:.3f}s)")

# 各通道统计（非零激励段）
if active.sum():
    m = active
    print(f"\n 有效段数据统计 (激励中):")
    for name, col in [("CH1 pos", ch1), ("CH2 spd", ch2), ("CH3 trq", ch3),
                      ("CH7 gff", ch7), ("CH9 multi", ch), ("CH12 setref", ch12)]:
        v = col[m]
        print(f"  {name:10s}  min={v.min():+.4f}  max={v.max():+.4f}  "
              f"mean={v.mean():+.4f}  std={v.std():.4f}")

# 有效段激励 CH9 与 setref CH12 的差异
if active.sum():
    m = active
    diff = ch12[m] - ch7[m] - ch[m]
    print(f"\n setref-(gff+multi): max|.|={np.abs(diff).max():.4f}  (应≈0)")

# 位置摆动（验证 multi_sine 是否驱动起来）
if active.sum():
    m = active
    print(f"\n 位置摆动范围: [{ch1[m].min():+.4f}, {ch1[m].max():+.4f}]  "
          f"span={ch1[m].max()-ch1[m].min():.4f} rad")
    print(f" 速度范围: [{ch2[m].min():+.4f}, {ch2[m].max():+.4f}]")

# 分段统计：每 50s 一段看位置是否漂移
seg = 50000
print(f"\n 位置分段统计 (每{seg/1000:.0f}s):")
for i in range(0, len(ch1), seg):
    m = active[i:i+seg]
    if m.sum() < 100:
        continue
    v = ch1[i:i+seg][m]
    s = ch2[i:i+seg][m]
    print(f"  t={t[i]*1e-6:6.1f}s  pos[{v.min():+.3f},{v.max():+.3f}]  "
          f"spd[{s.min():+.2f},{s.max():+.2f}]")
