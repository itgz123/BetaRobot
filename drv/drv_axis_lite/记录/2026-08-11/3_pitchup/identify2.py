#!/usr/bin/env python3
"""用实际力矩重辨识 + 摩擦形状提取 (固定 J)"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter

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
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); gff_r = rs(gff); trq_r = rs(trq)
n = len(tuni); per = 2000
ncut = (n // per) * per
pos_c = pos_r[:ncut]; spd_c = spd_r[:ncut]; trq_c = trq_r[:ncut]; gff_c = gff_r[:ncut]
multi_c = multi_r[:ncut]

def coef(x):
    return np.fft.rfft(x - x.mean()) * 2.0 / ncut
F = np.fft.rfftfreq(ncut, 1.0/fs)
def at(f, X):
    return coef(X)[int(round(f/(fs/ncut)))]

freqs = np.arange(0.5, 5.01, 0.5)
w0 = 2*np.pi*freqs

# ============ 1. 摩擦形状提取 (固定 J = 0.008) ============
print("="*70)
print("  [1] 摩擦余量 T_fric = trq - gff - J·θ̈  vs  速度 (固定 J=0.008)")
print("="*70)
Jfix = 0.008
win = 21
acc_c = savgol_filter(spd_c, win, 3, deriv=1, delta=1/fs)
T_fric = trq_c - gff_c - Jfix*acc_c
# 分箱: 按速度分箱取中位数 (去掉 |v|<0.05 过零带)
bins = np.linspace(-5, 5, 41)
bin_c = (bins[:-1]+bins[1:])/2
med_fric = []
for i in range(len(bins)-1):
    m = (spd_c > bins[i]) & (spd_c <= bins[i+1]) & (np.abs(spd_c) > 0.05)
    if m.sum() > 20:
        med_fric.append(np.median(T_fric[m]))
    else:
        med_fric.append(np.nan)
med_fric = np.array(med_fric)
print(f"  {'v(rad/s)':>9s} {'T_fric中位':>10s}")
for v, tf in zip(bin_c, med_fric):
    if np.isfinite(tf):
        print(f"  {v:9.2f} {tf:10.4f}")

# 线性段拟合 (|v|>0.5, 分正负)
print("\n  线性拟合 (|v|>0.5 rad/s):")
for sign, nm in [(1, "v>0"), (-1, "v<0")]:
    m = (spd_c*sign > 0.5)
    if m.sum() > 100:
        Xf = np.column_stack([spd_c[m], np.ones(m.sum())])
        coefs, _, _, _ = np.linalg.lstsq(Xf, T_fric[m], rcond=None)
        print(f"  {nm}:  cv={coefs[0]:.4f}  Tc={coefs[1]:.4f}   (n={m.sum()})")

# 库仑平台: 低速 (0.05<|v|<0.3) 的 T_fric 中位
m_slow = (np.abs(spd_c) > 0.05) & (np.abs(spd_c) < 0.3)
if m_slow.sum() > 20:
    print(f"  低速带(0.05<|v|<0.3) T_fric 中位 = {np.median(T_fric[m_slow]):.4f}  (n={m_slow.sum()})")

# ============ 2. 频域 LS (实际力矩) + 速度延迟扫描 ============
print("\n" + "="*70)
print("  [2] 频域复数 LS (y=trq-gff, 单Tc), 扫描速度延迟")
print("="*70)

def freq_ls(tau_ms):
    """给定延迟 tau_ms, 构造复数 LS 求 [J,cv+,cv-,Tc,dG]"""
    tau = tau_ms*1e-3
    # 速度信号移位: v_shift(t) = v(t+tau)  (使 v̂_shift = v̂·e^{+jωτ})
    t_sh = tuni[:ncut] + tau
    spd_sh = np.interp(t_sh, tuni[:ncut], spd_c)
    # 注意：移位后末尾越界，用周期延伸
    I_p = (spd_sh > 0).astype(float); I_n = (spd_sh < 0).astype(float)
    vp = spd_sh*I_p; vn = spd_sh*I_n
    sg = np.sign(spd_sh)
    A = np.zeros((len(freqs), 5), dtype=complex)
    b = np.zeros(len(freqs), dtype=complex)
    cosT = np.cos(pos_c)
    for i, f in enumerate(freqs):
        w = 2*np.pi*f
        A[i,0] = 1j*w*at(f, spd_sh)   # J
        A[i,1] = at(f, vp)             # cv+
        A[i,2] = at(f, vn)             # cv-
        A[i,3] = at(f, sg)             # Tc (单一库仑)
        A[i,4] = at(f, cosT)           # dG
        b[i] = at(f, trq_c - gff_c)
    Ar = np.vstack([A.real, A.imag]); br = np.concatenate([b.real, b.imag])
    th, _, _, _ = np.linalg.lstsq(Ar, br, rcond=None)
    pred = Ar@th
    rmse = np.sqrt(np.mean((br-pred)**2))
    return th, rmse

taus = np.arange(0, 31, 2)  # 0~30ms
best = None
print(f"  {'τ(ms)':>6s} {'J':>9s} {'cv+':>8s} {'cv-':>8s} {'Tc':>8s} {'dG':>8s} {'g_real':>8s} {'RMSE':>7s}")
for tau in taus:
    th, rmse = freq_ls(tau)
    J, cvp, cvn, Tc, dG = th
    g_real = gff_c.mean() + dG
    print(f"  {tau:6d} {J:9.5f} {cvp:8.4f} {cvn:8.4f} {Tc:8.4f} {dG:8.3f} {g_real:8.3f} {rmse:7.4f}")
    if best is None or rmse < best[1]:
        best = (tau, rmse, th)

tau_b, rmse_b, th_b = best
print(f"\n  最优: τ={tau_b}ms  RMSE={rmse_b:.4f}")
Jb, cvpb, cvnb, Tcb, dGb = th_b
print(f"  J={Jb:.5f}  cv+={cvpb:.4f}  cv-={cvnb:.4f}  Tc={Tcb:.4f}  dG={dGb:.3f}  g_real={gff_c.mean()+dGb:.3f}")

# 每频率残差
print("\n  最优参数下每频率拟合:")
tau = tau_b*1e-3
t_sh = tuni[:ncut] + tau
spd_sh = np.interp(t_sh, tuni[:ncut], spd_c)
I_p = (spd_sh>0).astype(float); I_n=(spd_sh<0).astype(float)
sg = np.sign(spd_sh); cosT = np.cos(pos_c)
for f in freqs:
    w = 2*np.pi*f
    pred = (Jb*1j*w*at(f,spd_sh) + cvpb*at(f,spd_sh*I_p) + cvnb*at(f,spd_sh*I_n)
            + Tcb*at(f,sg) + dGb*at(f,cosT))
    b_ = at(f, trq_c-gff_c)
    print(f"  f={f:.1f}: |τ̂|={abs(b_):.4f} |pred|={abs(pred):.4f} |res|={abs(b_-pred):.4f}")
