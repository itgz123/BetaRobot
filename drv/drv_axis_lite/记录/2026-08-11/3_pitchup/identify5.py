#!/usr/bin/env python3
"""修正重力基准: 频域 LS 含 sin(pos) 项 + 仿真验证"""
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

# ============ 1. 频域 LS: [J, cv+, cv-, Tc+, Tc-, Gc, Gs] ============
print("="*72)
print("  [1] 频域复数 LS (y=trq, 含 sin(pos))  — 7 参数")
print("="*72)
I_p = (spd_c > 0).astype(float); I_n = (spd_c < 0).astype(float)
vp = spd_c*I_p; vn = spd_c*I_n
cosT = np.cos(pos_c); sinT = np.sin(pos_c)

A = np.zeros((len(freqs), 7), dtype=complex)
b = np.zeros(len(freqs), dtype=complex)
for i, f in enumerate(freqs):
    w = 2*np.pi*f
    A[i,0] = 1j*w*at(f, spd_c)   # J
    A[i,1] = at(f, vp)            # cv+
    A[i,2] = at(f, vn)            # cv-
    A[i,3] = at(f, I_p)           # Tc+
    A[i,4] = at(f, I_n)           # Tc-
    A[i,5] = at(f, cosT)          # Gc
    A[i,6] = at(f, sinT)          # Gs
    b[i] = at(f, trq_c)

Ar = np.vstack([A.real, A.imag]); br = np.concatenate([b.real, b.imag])
th, res, rank, sv = np.linalg.lstsq(Ar, br, rcond=None)
J, cvp, cvn, Tcp, Tcn, Gc, Gs = th
pred = Ar@th
rmse = np.sqrt(np.mean((br-pred)**2))
print(f"  J    = {J:.5f}")
print(f"  cv+  = {cvp:.4f}   cv- = {cvn:.4f}")
print(f"  Tc+  = {Tcp:.4f}   Tc- = {Tcn:.4f}")
print(f"  Gc   = {Gc:.4f}   Gs = {Gs:.4f}")
G = np.hypot(Gc, Gs); phi0 = np.degrees(np.arctan2(-Gs, Gc))
print(f"  G = sqrt(Gc²+Gs²) = {G:.4f} Nm   φ0 = {phi0:+.1f}°")
print(f"  频域残差 RMSE = {rmse:.4f} Nm   R² = {1-np.var(br-pred)/max(np.var(br),1e-20):.4f}")

# 每频率
print("\n  每频率拟合:")
for i, f in enumerate(freqs):
    p = A[i]@th
    print(f"  f={f:.1f}: |τ̂|={abs(b[i]):.4f} |pred|={abs(p):.4f} |res|={abs(b[i]-p):.4f}")

# ============ 2. 交叉验证: 用命令力矩 y=multi (=trq-gff 用辨识重力) ============
print("\n" + "="*72)
print("  [2] 频域 LS, y = multi (=命令) 同模型")
print("="*72)
b2 = np.zeros(len(freqs), dtype=complex)
for i, f in enumerate(freqs):
    b2[i] = at(f, multi_c)
Ar2 = Ar; br2 = np.concatenate([b2.real, b2.imag])
th2, _, _, _ = np.linalg.lstsq(Ar2, br2, rcond=None)
J2, cvp2, cvn2, Tcp2, Tcn2, Gc2, Gs2 = th2
pred2 = Ar2@th2
rmse2 = np.sqrt(np.mean((br2-pred2)**2))
print(f"  J={J2:.5f}  cv+={cvp2:.4f} cv-={cvn2:.4f}  Tc+={Tcp2:.4f} Tc-={Tcn2:.4f}")
print(f"  Gc={Gc2:.4f} Gs={Gs2:.4f}  G={np.hypot(Gc2,Gs2):.4f}  φ0={np.degrees(np.arctan2(-Gs2,Gc2)):+.1f}°")
print(f"  频域残差 RMSE = {rmse2:.4f} Nm")

# ============ 3. 时域 OLS (中心化, 含 sin) — 交叉检查 ============
print("\n" + "="*72)
print("  [3] 时域 OLS (中心化, 含 sin(pos))")
print("="*72)
from scipy.signal import savgol_filter
win = 21
acc_c = savgol_filter(spd_c, win, 3, deriv=1, delta=1/fs)
X = np.column_stack([acc_c, vp, vn, I_p, I_n, cosT, sinT])
# 中心化
Xc = X - X.mean(axis=0)
yc = trq_c - trq_c.mean()
th3, res3, _, _ = np.linalg.lstsq(Xc, yc, rcond=None)
J3, cvp3, cvn3, Tcp3, Tcn3, Gc3, Gs3 = th3
pred3 = Xc@th3
rmse3 = np.sqrt(np.mean((yc-pred3)**2))
r2_3 = 1 - np.sum((yc-pred3)**2)/max(np.sum(yc**2), 1e-20)
print(f"  J={J3:.5f}  cv+={cvp3:.4f} cv-={cvn3:.4f}  Tc+={Tcp3:.4f} Tc-={Tcn3:.4f}")
print(f"  Gc={Gc3:.4f} Gs={Gs3:.4f}  G={np.hypot(Gc3,Gs3):.4f}  φ0={np.degrees(np.arctan2(-Gs3,Gc3)):+.1f}°")
print(f"  RMSE={rmse3:.4f} Nm  R²={r2_3:.4f}")

# ============ 4. 重力残差检查: trq - (friction+inertia) vs sin(pos) ============
print("\n" + "="*72)
print("  [4] 重力验证: 残差力矩 vs sin(pos) (用频域参数)")
print("="*72)
T_res = trq_c - (J*acc_c + cvp*vp + cvn*vn + Tcp*I_p + Tcn*I_n)
pbins = np.linspace(-0.7, 0.8, 10)
print(f"  {'pos':>10s} {'残差中位':>10s} {'Gc·cos+Gs·sin':>14s} {'n':>6s}")
for i in range(len(pbins)-1):
    m = (pos_c >= pbins[i]) & (pos_c < pbins[i+1])
    if m.sum() < 30: continue
    pc = np.median(pos_c[m])
    grav_model = Gc*np.cos(pc) + Gs*np.sin(pc)
    print(f"  [{pbins[i]:+.2f},{pbins[i+1]:+.2f}] {np.median(T_res[m]):+10.4f} {grav_model:+14.4f} {m.sum():6d}")

# ============ 5. 仿真验证 (重力用 Gc/Gs) ============
print("\n" + "="*72)
print("  [5] 短窗口开环仿真 vs 实测")
print("="*72)
def simulate(y_cmd, pos0, spd0, J, cvp, cvn, Tcp, Tcn, Gc, Gs, n_steps):
    p = np.zeros(n_steps); v = np.zeros(n_steps)
    p[0], v[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vv = v[i-1]
        if vv > 0: fric = cvp*vv + Tcp
        elif vv < 0: fric = cvn*vv - Tcn
        else: fric = 0.0
        g = Gc*np.cos(p[i-1]) + Gs*np.sin(p[i-1])
        a = (y_cmd[i] - g - fric) * invJ
        v[i] = vv + a/fs
        p[i] = p[i-1] + vv/fs
    return p, v

rng_v = spd_r.max() - spd_r.min(); rng_p = pos_r.max() - pos_r.min()
# 用命令力矩 multi（系统实际施加: multi + Gc_cos+Gs_sin，但仿真已含重力）
cands = [
    ("频域[1]参数", J, cvp, cvn, Tcp, Tcn, Gc, Gs),
    ("配置参数",    0.008, 0.05, 0.06, 0.02, 0.0, 0.28, 0.0),
    ("频域J+库仑0.15", J, 0.03, 0.03, 0.15, 0.15, Gc, Gs),
]
for name, Jx, cvpx, cvnx, Tcpp, Tcnn, Gcx, Gsx in cands:
    v_rmses = []; p_rmses = []
    for w in range(8):
        s0 = int(w*(per//2)); s1 = s0 + per
        if s1 > n: break
        p_s, v_s = simulate(multi_r[s0:s1], pos_r[s0], spd_r[s0],
                            Jx, cvpx, cvnx, Tcpp, Tcnn, Gcx, Gsx, per)
        e_v = np.sqrt(np.mean((v_s[100:-100]-spd_r[s0+100:s1-100])**2))
        e_p = np.sqrt(np.mean((p_s[100:-100]-pos_r[s0+100:s1-100])**2))
        v_rmses.append(e_v); p_rmses.append(e_p)
    mv = np.mean(v_rmses); mp = np.mean(p_rmses)
    print(f"  {name:20s}  速度RMSE={mv:.3f} ({mv/rng_v*100:.1f}%)  位置RMSE={mp:.4f} ({mp/rng_p*100:.1f}%)")
