#!/usr/bin/env python3
"""频域诊断: SG微分响应 + 速度延迟 + 频域复数OLS"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
DATA = np.load(os.path.join(BASE, "dedup.npz"))
t = DATA["t"]; pos = DATA["pos"]; spd = DATA["spd"]
gff = DATA["gff"]; multi = DATA["multi"]

fs = 1000.0
act = np.abs(multi) > 1e-6
t0 = t[act].min()
tuni = np.arange(t0, t[act].max(), 1.0/fs)
def rs(x):
    return np.interp(tuni, t[act], x[act])
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); gff_r = rs(gff)
n = len(tuni)
per = 2000
nper = n // per
ncut = nper * per
print(f"整段 {n} 点 = {nper} 周期, 截断 {ncut} 点")

# ---------- 1. SG 微分复响应 ----------
print("\n" + "="*60)
print("  [1] SG(41,3) 微分复响应 (对合成正弦)")
print("="*60)
win = 41
freqs = np.arange(0.5, 5.01, 0.5)
for f in freqs:
    tt = np.arange(ncut)/fs
    s = np.sin(2*np.pi*f*tt)
    a_sg = savgol_filter(s, win, 3, deriv=1, delta=1/fs)
    a_exact = 2*np.pi*f*np.cos(2*np.pi*f*tt)
    # 幅值比（避开边缘 60 点）
    sl = slice(win//2+1, ncut-win//2-1)
    ratio = np.std(a_sg[sl]) / np.std(a_exact[sl])
    print(f"  f={f:.1f}Hz: 幅值比={ratio:.4f}")

# ---------- 2. 速度延迟估计 ----------
print("\n" + "="*60)
print("  [2] 速度测量延迟: v̂(f) vs jω·poŝ(f)")
print("="*60)
def coef(x):
    return np.fft.rfft(x - x.mean()) * 2.0 / ncut
F = np.fft.rfftfreq(ncut, 1.0/fs)
def at(freq_hz, X):
    return coef(X)[int(round(freq_hz/(fs/ncut)))]

pos_c = pos_r[:ncut]; spd_c = spd_r[:ncut]
for f in freqs:
    vf = at(f, spd_c)
    jw_pos = 1j*2*np.pi*f*at(f, pos_c)
    # 相位差 vf vs jw_pos
    ang = np.angle(vf * np.conj(jw_pos))
    # 延迟 = -angle/omega (v 滞后则 ang<0)
    delay_ms = -ang/(2*np.pi*f)*1000
    mag_ratio = abs(vf)/max(abs(jw_pos), 1e-12)
    print(f"  f={f:.1f}Hz: |v̂/jωpoŝ|={mag_ratio:.3f}  相位差={ang*180/np.pi:7.2f}°  等效延迟={delay_ms:6.2f}ms")

# ---------- 3. 频域复数 OLS (jω 微分) ----------
print("\n" + "="*60)
print("  [3] 频域复数 OLS (θ̈=jω·v̂, 10 激励频率)")
print("="*60)
# 时域信号
th_dot2 = spd_r[:ncut]   # 用 jω·v̂ 微分
I_p = (spd_r[:ncut] > 0).astype(float)
I_n = (spd_r[:ncut] < 0).astype(float)
vp = spd_r[:ncut]*I_p
vn = spd_r[:ncut]*I_n
cosT = np.cos(pos_r[:ncut])
y = multi_r[:ncut]

# 构建复数回归: 每个频率一行
freq_list = freqs
A = np.zeros((len(freq_list), 6), dtype=complex)
b = np.zeros(len(freq_list), dtype=complex)
for i, f in enumerate(freq_list):
    w = 2*np.pi*f
    A[i,0] = 1j*w*at(f, th_dot2)      # J
    A[i,1] = at(f, vp)                 # cv+
    A[i,2] = at(f, vn)                 # cv-
    A[i,3] = at(f, I_p)                # Tc+
    A[i,4] = at(f, I_n)                # Tc-
    A[i,5] = at(f, cosT)               # dG
    b[i] = at(f, y)

# 复数最小二乘 (解耦成实部虚部)
Ar = np.vstack([A.real, A.imag])
br = np.concatenate([b.real, b.imag])
th, res, rank, sv = np.linalg.lstsq(Ar, br, rcond=None)
labels = ["J", "cv+", "cv-", "Tc+", "Tc-", "dG"]
for lab, v in zip(labels, th):
    print(f"  {lab:5s} = {v:+.6f}")

# 拟合残差
pred = Ar @ th
resid = br - pred
rmse = np.sqrt(np.mean(resid**2))
print(f"  频域拟合残差 RMSE = {rmse:.4f} Nm")
Jf, cvpf, cvnf, Tcpf, Tcnf, dGf = th
print(f"  g_real = {gff_r.mean() + dGf:.4f}")

# 每频率贡献与残差
print(f"\n  {'f':>5s} {'|τ̂|':>8s} {'|pred|':>8s} {'|res|':>8s}")
for i, f in enumerate(freq_list):
    p = A[i]@th
    print(f"  {f:5.1f} {abs(b[i]):8.4f} {abs(p):8.4f} {abs(b[i]-p):8.4f}")

# ---------- 4. 速度延迟下的 J 修正: 高频 J(f) ----------
print("\n" + "="*60)
print("  [4] 各频率 J(f) = |τ̂(f)| / |θ̈̂(f)| (惯性主导近似)")
print("="*60)
for f in freq_list:
    w = 2*np.pi*f
    taum = at(f, y)
    a_f = 1j*w*at(f, spd_c)
    Jf_f = abs(taum)/max(abs(a_f), 1e-12)
    print(f"  f={f:.1f}Hz: J(f)={Jf_f:.5f}  (τ̂={abs(taum):.4f}, θ̈̂={abs(a_f):.3f})")
