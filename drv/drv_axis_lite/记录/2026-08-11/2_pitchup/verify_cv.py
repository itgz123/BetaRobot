# -*- coding: utf-8 -*-
"""
从 plot_data3 (cv_ff=0.09/0.10 失控数据) 反解真实粘滞系数 cv_real.
动力学: setref - gff = J*a + cv_real*v + Tc*sign(v)   (假设 gff=真实重力, Tc_ff=真实Tc)
cv_real = (setref - gff - J*a - Tc*sign(v)) / v
"""
import sys, io
import numpy as np
import pandas as pd
from scipy.signal import savgol_filter

if sys.platform == "win32" and isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")

CSV = "drv/drv_axis_lite/记录/2026-08-11/2_pitchup/plot_data3.csv"
df = pd.read_csv(CSV)
ts = df["CH0"].values
nz = np.where(ts > 0)[0][0]
d = df.iloc[nz:].reset_index(drop=True).astype(float)
t = (d["CH0"].values - d["CH0"].values[0]) / 1e6
pos, spd = d["CH1"].values, d["CH2"].values
gff, setref = d["CH7"].values, d["CH12"].values
J = 0.008
Tc_pos, Tc_neg = 0.02, 0.0
dt = np.median(np.diff(t))

# 速度量化步长确认
ds = np.diff(spd)
ds = ds[ds != 0]
print(f"速度最小非零变化: {np.abs(ds).min():.6f} rad/s  (对比 60/8192={60/8192:.6f})")

acc = savgol_filter(spd, 31, 3, deriv=1, delta=dt)


def cv_est(mask, label):
    v = spd[mask]
    a = acc[mask]
    s = setref[mask] - gff[mask] - J * a
    Tc = np.where(v > 0, Tc_pos, np.where(v < 0, -Tc_neg, 0.0))
    est = (s - Tc) / v
    est = est[np.abs(v) > 0.05]
    est = est[np.isfinite(est)]
    print(f"  {label}: n={len(est):5d} 中位数={np.median(est):+.4f} "
          f"均值={np.mean(est):+.4f} std={np.std(est):.4f} "
          f"[P25={np.percentile(est, 25):+.4f} P75={np.percentile(est, 75):+.4f}]")
    return est


print("\n反解 cv_real (假设 gff=重力, J=0.008):")
e1 = cv_est((t > 9.6) & (t < 10.3) & (np.abs(spd) > 0.05), "正常段(9.6-10.3s)")
e2 = cv_est((t > 10.6) & (t < 10.9) & (np.abs(spd) > 0.05), "初漂移段(10.6-10.9s)")
e3 = cv_est((t > 11.4) & (t < 11.7) & (np.abs(spd) > 0.05), "跑偏段(11.4-11.7s)")

# 正反馈项验证: 前馈多余驱动 = (cv_ff - cv_real)*v
cv_real = np.median(np.concatenate([e1, e2, e3])) if len(e1) + len(e2) + len(e3) else 0.05
print(f"\n综合 cv_real ≈ {cv_real:.3f}")
print(f"当前前馈 cv_ff = 0.09/0.10 → 过补偿 Δcv = {0.09 - cv_real:.3f}")
print(f"正反馈系数 Δcv/J = {(0.09 - cv_real) / J:.2f} /s (时间常数 {J/(0.09-cv_real):.2f}s)")

# 显示跑偏段的加速度来源分解
print("\n跑偏段(11.4-11.7s)加速度来源分解 (取速度>2的时刻):")
m = (t > 11.45) & (t < 11.7) & (np.abs(spd) > 2.0)
for i in np.where(m)[0][::40]:
    v = spd[i]
    a_meas = acc[i]
    a_ref = iff_tmp if False else 0  # 实际用 iff
    print(f"  t={t[i]:6.3f} v={v:+.2f} a_实测={a_meas:+7.2f}  iff/J=ref_acc={d['CH6'].values[i]:+7.2f}  "
          f"Δcv*v/J={(0.09-cv_real)*v/J:+7.2f}  setref={setref[i]:+.3f}")
