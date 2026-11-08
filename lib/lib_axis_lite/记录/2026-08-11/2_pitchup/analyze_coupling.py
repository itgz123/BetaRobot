# -*- coding: utf-8 -*-
"""
分析 plot_data2.csv (CH13=pitchdown速度, CH14=pitchup速度) 上下pitch耦合问题。
核心: CH1 是差动位置 (pitchup - pitchdown), CH2 是 pitchup 原始速度(非差动)。
方法: 均匀重采样到1000Hz后再做2Hz正弦拟合(避免非均匀时间戳累积相位误差)。
"""
import sys, io
if sys.platform == "win32" and isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")
import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt

CSV = "drv/drv_axis_lite/记录/2026-08-11/2_pitchup/plot_data2.csv"
FS_TARGET = 1000.0

df = pd.read_csv(CSV)
ts = df["CH0"].values
nz = np.where(ts > 0)[0][0]
d = df.iloc[nz:].reset_index(drop=True).astype(float)
t_raw = (d["CH0"].values - d["CH0"].values[0]) / 1e6
print(f"原始: {len(t_raw)} 点, {t_raw[-1]:.1f}s, 实际FS={1/np.median(np.diff(t_raw)):.1f}Hz")

# 均匀重采样
channels = ["CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "CH8", "CH9", "CH12", "CH13", "CH14"]
t_uniform = np.arange(0, t_raw[-1], 1 / FS_TARGET)
cols = {}
for ch in channels:
    cols[ch] = np.interp(t_uniform, t_raw, d[ch].values)
n = len(t_uniform)
print(f"重采样: {n} 点 @ {FS_TARGET:.0f}Hz")

# 去掉前5s延时
m = (t_uniform > 5.0) & (t_uniform < t_raw[-1] - 0.5)
t = t_uniform[m]
sel = {ch: cols[ch][m] for ch in channels}
pos, spd = sel["CH1"], sel["CH2"]
ref_vel, ref_pos = sel["CH5"], sel["CH4"]
trq, setref = sel["CH3"], sel["CH12"]
ch13, ch14 = sel["CH13"], sel["CH14"]

w = 2 * 2 * np.pi
S, C = np.sin(w * t), np.cos(w * t)


def fit2(y, trend=True):
    """2Hz正弦拟合, 可选线性漂移项。返回 (幅值, 相位deg, 漂移, 偏置)"""
    cols = [S, C]
    if trend:
        cols += [t, np.ones_like(t)]
    sol = np.linalg.lstsq(np.column_stack(cols), y, rcond=None)[0]
    A, B = sol[0], sol[1]
    D = sol[3] if trend else sol[2]
    K = sol[2] if trend else 0.0
    return np.hypot(A, B), np.degrees(np.arctan2(A, B)), K, D


print("\n===== 2Hz 分量 (均匀重采样后) =====")
for ch, nm in [("CH1", "位置"), ("CH2", "速度"), ("CH3", "力矩"), ("CH4", "ref_pos"),
               ("CH5", "ref_vel"), ("CH12", "setref")]:
    a, p, K, D = fit2(sel[ch])
    print(f"  {ch} {nm:6s}: 幅值={a:.4f} 相位={p:+7.1f}°  漂移={K:+.5f} 偏置={D:+.4f}")

a_v, p_v, *_ = fit2(spd)
a_rv, p_rv, *_ = fit2(ref_vel)
a_p, p_p, *_ = fit2(pos)
a_rp, p_rp, *_ = fit2(ref_pos)
print(f"\n  速度滞后: ref_vel{p_rv:+.1f}° vs CH2{p_v:+.1f}° → 滞后{p_rv-p_v:+.1f}° = {(p_rv-p_v)/360*0.5*1000:.1f} ms")
print(f"  幅值比 CH2/ref_vel = {a_v/a_rv:.3f}   位置比 CH1/ref_pos = {a_p/a_rp:.3f}")

# 位置-速度一致性: d(CH1)/dt 相位应 = 位置相位+90°
print(f"\n  位置CH1相位{p_p:+.1f}° + 90° = {p_p+90:+.1f}° vs 速度CH2相位{p_v:+.1f}° (差{(p_v)-(p_p+90):+.1f}°)")

# 差动一致性: dpos vs CH2
print("\n===== 差动一致性: 若 pitchdown 静止, d(CH1)/dt ≈ CH2 =====")
b_lp, a_lp = butter(3, 40 / (FS_TARGET / 2), "low")
spd_f = filtfilt(b_lp, a_lp, spd)
dpos = np.gradient(pos, t)
dpos_f = filtfilt(b_lp, a_lp, dpos)
res = dpos_f - spd_f
a_r, p_r, K_r, D_r = fit2(res)
print(f"  残差 dpos-CH2: 2Hz幅值={a_r:.5f} (信号{a_v:.3f}的{a_r/a_v*100:.1f}%) 相位={p_r:+.1f}° 常数D={D_r:+.5f}")
# 若残差= -v_pitchdown (CH1=pitchup-pitchdown), 则 pitchdown 2Hz速度
print(f"  → pitchdown 2Hz速度≈ -残差: 幅值={a_r:.5f} rad/s 相位={((p_r+180)%360)-180:+.1f}° 位置幅值≈{a_r/(2*np.pi*2):.6f} rad")

# pitchdown 位置反演: pitchdown_pos = ∫CH14 - CH1 (相对值)
print("\n===== pitchdown 位置反演 =====")
ch14_m = sel["CH14"]
pup_int = np.cumsum(ch14_m) / FS_TARGET
pdn_recon = pup_int - (pos - pos[0])  # 相对起始
pdn_f = filtfilt(b_lp, a_lp, pdn_recon)
a_pdn, p_pdn, K_pdn, D_pdn = fit2(pdn_recon)
print(f"  反演 pitchdown 位置: 2Hz幅值={a_pdn:.6f} rad (={a_pdn*180/np.pi:.3f}°) 相位={p_pdn:+.1f}° 漂移={K_pdn:+.6f} rad/s")
print(f"  pitchdown 慢变漂移: 前2s={pdn_f[:2000].mean():+.5f} 后2s={pdn_f[-2000:].mean():+.5f} 变化={pdn_f[-1]-pdn_f[0]:+.5f}")

# 幅频: 残差和反演位置是否真的有2Hz
for nm, y in [("残差dpos-CH2", res), ("反演pdn位置", pdn_recon)]:
    fr = np.fft.rfftfreq(len(y), 1 / FS_TARGET)
    R = np.abs(np.fft.rfft(y - y.mean()))
    i2 = np.argmin(np.abs(fr - 2))
    amp2 = 2 * R[i2] / len(y)
    print(f"  {nm}: f=2.00Hz 幅值={amp2:.6f}  频谱峰值@{fr[np.argmax(R)]:.2f}Hz")
