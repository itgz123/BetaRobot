#!/usr/bin/env python3
"""收敛: 固定J/g后的摩擦拟合 + 重力残差检查 + 短窗口仿真"""
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

# ---- 频域工具 ----
def coef(x):
    return np.fft.rfft(x - x.mean()) * 2.0 / ncut
F = np.fft.rfftfreq(ncut, 1.0/fs)
def at(f, X):
    return coef(X)[int(round(f/(fs/ncut)))]
freqs = np.arange(0.5, 5.01, 0.5)

# ---- 1. J 精确估计: 1.5~4.5Hz Im(τ̂/v̂)/ω ----
print("="*64)
print("  [1] J 频域估计 (Im(τ̂/v̂)/ω, 1.5~4.5Hz)")
print("="*64)
pos_c = pos_r[:ncut]; spd_c = spd_r[:ncut]; trq_c = trq_r[:ncut]; gff_c = gff_r[:ncut]
J_list = []
for f in freqs:
    if f < 1.3 or f > 4.8: continue
    vf = at(f, spd_c)
    tt = at(f, trq_c - gff_c)
    Jf = (tt/vf).imag / (2*np.pi*f)
    J_list.append(Jf)
    print(f"  f={f:.1f}Hz: J={Jf:.5f}")
J = float(np.median(J_list))
print(f"  J 中位 = {J:.5f}  kg·m²")

# ---- 2. 固定 J, 频域求 cv, Tc, dG ----
print("\n" + "="*64)
print("  [2] 固定 J 后的频域 LS (y=trq-gff): cv⁺,cv⁻,Tc(单),dG")
print("="*64)
I_p = (spd_c > 0).astype(float); I_n = (spd_c < 0).astype(float)
sg = np.sign(spd_c); cosT = np.cos(pos_c)
A = np.zeros((len(freqs), 4), dtype=complex)
b = np.zeros(len(freqs), dtype=complex)
for i, f in enumerate(freqs):
    w = 2*np.pi*f
    A[i,0] = at(f, spd_c*I_p)   # cv+
    A[i,1] = at(f, spd_c*I_n)   # cv-
    A[i,2] = at(f, sg)          # Tc
    A[i,3] = at(f, cosT)        # dG
    b[i] = at(f, trq_c - gff_c) - J*1j*w*at(f, spd_c)
Ar = np.vstack([A.real, A.imag]); br = np.concatenate([b.real, b.imag])
th, _, _, _ = np.linalg.lstsq(Ar, br, rcond=None)
cvp, cvn, Tc, dG = th
print(f"  cv+={cvp:.4f}  cv-={cvn:.4f}  Tc={Tc:.4f}  dG={dG:.4f}  (g={gff_c.mean()+dG:.4f})")
pred = Ar@th
print(f"  频域残差 RMSE = {np.sqrt(np.mean((br-pred)**2)):.4f} Nm")

# ---- 3. T_fric 摩擦形状 (固定 J, 用 gff 命令减重力) ----
print("\n" + "="*64)
print("  [3] T_fric 分箱 (固定 J): 检查摩擦形状与重力残差")
print("="*64)
win = 21
acc_c = savgol_filter(spd_c, win, 3, deriv=1, delta=1/fs)
T_fric = trq_c - gff_c - J*acc_c
bins = np.linspace(-5, 5, 33)
bc = (bins[:-1]+bins[1:])/2
print(f"  {'|v|区间':>8s} {'v>0中位':>9s} {'v<0中位':>9s} {'n':>6s}")
for i in range(len(bins)-1):
    lo, hi = bins[i], bins[i+1]
    if abs(bc[i]) < 0.4: continue
    if bc[i] > 0:
        mm = (spd_c > lo) & (spd_c <= hi)
    else:
        mm = (spd_c < lo) & (spd_c >= hi)
    if mm.sum() < 20: continue
    print(f"  {lo:8.2f} {np.median(T_fric[mm]):9.4f} {np.median(T_fric[mm]):9.4f} {mm.sum():6d}")

# 线性拟合 cv/Tc (|v|>0.6, 分方向, 只拟合粘滞部分——用大速度段)
print("\n  摩擦线性拟合 (|v|>0.8):")
for sgn, nm in [(1, "v>0"), (-1, "v<0")]:
    m = spd_c*sgn > 0.8
    if m.sum() > 200:
        Xf = np.column_stack([spd_c[m], np.ones(m.sum())])
        c, _, _, _ = np.linalg.lstsq(Xf, T_fric[m], rcond=None)
        print(f"  {nm}:  cv={c[0]:.4f}  Tc={c[1]:.4f}  (n={m.sum()})")

# 重力残差: T_fric 与 cos(pos) 的关系 (在高速, v>1.5)
print("\n  高速(v>1.5) T_fric vs pos:")
m = spd_c > 1.5
if m.sum() > 100:
    pbins = np.linspace(-0.6, 0.8, 8)
    for i in range(len(pbins)-1):
        mm2 = m & (pos_c > pbins[i]) & (pos_c <= pbins[i+1])
        if mm2.sum() > 20:
            print(f"    pos[{pbins[i]:+.2f},{pbins[i+1]:+.2f}]  T_fric中位={np.median(T_fric[mm2]):+.4f}  (n={mm2.sum()})")

# ---- 4. 短窗口仿真验证 ----
print("\n" + "="*64)
print("  [4] 短窗口(2s)开环仿真 vs 实测 (命令力矩)")
print("="*64)
def simulate(y_cmd, gff_in, pos0, spd0, J, cvp, cvn, Tcp, Tcn, g_real, n_steps):
    p = np.zeros(n_steps); v = np.zeros(n_steps)
    p[0], v[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vv = v[i-1]
        if vv > 0: fric = cvp*vv + Tcp
        elif vv < 0: fric = cvn*vv - Tcn
        else: fric = 0.0
        a = (y_cmd[i] + gff_in[i] - g_real*np.cos(p[i-1]) - fric) * invJ
        v[i] = vv + a/fs
        p[i] = p[i-1] + vv/fs
    return p, v

# 候选参数组
cands = [
    ("辨识:J={J:.4f},cv+={cvp:.3f},cv-={cvn:.3f},Tc={Tc:.3f},g={g:.3f}".format(
        J=J, cvp=cvp, cvn=cvn, Tc=Tc, g=gff_c.mean()+dG),
     J, cvp, cvn, Tc, Tc, gff_c.mean()+dG),
    ("辨识2:J={J:.4f},cv+=0.04,cv-=0.04,Tc=0.15,g=0.28".format(J=J),
     J, 0.04, 0.04, 0.15, 0.15, 0.28),
    ("配置:J=0.008,cv+=0.05,cv-=0.06,Tc+=0.02,Tc-=0,g=0.28",
     0.008, 0.05, 0.06, 0.02, 0.0, 0.28),
]
n_win = 6
win_len = 2*fs
rng_v = spd_r.max() - spd_r.min()
rng_p = pos_r.max() - pos_r.min()
for name, Jx, cvpx, cvnx, Tcpp, Tcnn, gx in cands:
    v_rmses = []
    for w in range(n_win):
        s0 = w*(win_len//2)
        s1 = s0 + win_len
        if s1 > n: break
        p_s, v_s = simulate(multi_r[s0:s1], gff_r[s0:s1],
                            pos_r[s0], spd_r[s0], Jx, cvpx, cvnx, Tcpp, Tcnn, gx, win_len)
        e_v = np.sqrt(np.mean((v_s[100:-100] - spd_r[s0+100:s1-100])**2))
        v_rmses.append(e_v)
    mv = np.mean(v_rmses)
    print(f"  {name:44s}  速度RMSE(6窗口均)={mv:.3f} rad/s  ({mv/rng_v*100:.1f}% NRMSE)")
