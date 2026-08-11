#!/usr/bin/env python3
"""多正弦时域 OLS + 频域验证辨识 (IDENTIFY_OLS 阶段数据)

模型（输出侧，gear=1）:
    multi(t) = J·θ̈ + cv⁺·v·I⁺ + cv⁻·v·I⁻ + Tc⁺·I⁺ + Tc⁻·I⁻ + ΔG·cos(θ)
    g_real = g_ff + ΔG

方法:
  1. 均匀重采样激励段 -> 1000 Hz
  2. SG 微分求 θ̈
  3. 全段时域 OLS
  4. 每周期(2s)窗口 OLS -> 参数一致性
  5. 频域 FRF 验证 (各激励频率复数对比)
"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
DATA = np.load(os.path.join(BASE, "dedup.npz"))
t    = DATA["t"]; pos = DATA["pos"]; spd = DATA["spd"]
trq  = DATA["trq"]; gff = DATA["gff"]; multi = DATA["multi"]; setref = DATA["setref"]

# ---- 截取激励段，去掉首个周期瞬态 ----
act = np.abs(multi) > 1e-6
t0 = t[act].min()
# 去掉前 2s（1 个周期）和后 0.5s 尾端
t_start = t0 + 2.0
t_end = t[act].max() - 0.3
m = act & (t >= t_start) & (t <= t_end)
print(f"分析段: t=[{t_start:.3f}, {t_end:.3f}] s  ({m.sum()} 样本, {(t_end-t_start):.1f}s)")

# ---- 均匀重采样到 1000Hz ----
fs = 1000.0
tuni = np.arange(t_start, t_end, 1.0/fs)
def rs(x):
    return np.interp(tuni, t[m], x[m])
pos_r = rs(pos); spd_r = rs(spd); gff_r = rs(gff)
multi_r = rs(multi); setref_r = rs(setref); trq_r = rs(trq)
n = len(tuni)

# ---- 加速度: SG 微分 ----
win = 41  # 41ms @1kHz
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1.0/fs)

# ---- 回归矩阵 ----
I_p = (spd_r > 0).astype(float)
I_n = (spd_r < 0).astype(float)
cos_theta = np.cos(pos_r)
Phi = np.column_stack([acc_r, spd_r*I_p, spd_r*I_n, I_p, I_n, cos_theta])
labels = ["J", "cv+", "cv-", "Tc+", "Tc-", "dG"]

def ols(y, X):
    theta, res, rank, sv = np.linalg.lstsq(X, y, rcond=None)
    # 残差方差 -> 参数标准差 (白噪声假设)
    dof = len(y) - X.shape[1]
    sigma2 = np.sum((y - X@theta)**2) / max(dof, 1)
    cov = sigma2 * np.linalg.inv(X.T@X)
    se = np.sqrt(np.abs(np.diag(cov)))
    r2 = 1 - np.sum((y - X@theta)**2)/max(np.sum((y-y.mean())**2), 1e-20)
    return theta, se, r2, (y - X@theta)

# ============ 1. 全段 OLS (命令净激励 multi) ============
print("\n" + "="*60)
print("  [方法1] 全段时域 OLS,  y = multi(t) (命令净激励)")
print("="*60)
th1, se1, r2_1, res1 = ols(multi_r, Phi)
for lab, v, s in zip(labels, th1, se1):
    print(f"  {lab:5s} = {v:+.6f}  ± {s:.6f}")
print(f"  R² = {r2_1:.5f}   力矩残差RMSE = {np.sqrt(np.mean(res1**2)):.4f} Nm")
print(f"  g_real = g_ff + dG = {gff_r.mean() + th1[5]:.4f}  (g_ff平均={gff_r.mean():.4f})")

# ============ 2. 全段 OLS (实际净激励 trq - gff) ============
print("\n" + "="*60)
print("  [方法2] 全段时域 OLS,  y = trq - gff (实际净激励)")
print("="*60)
th2, se2, r2_2, res2 = ols(trq_r - gff_r, Phi)
for lab, v, s in zip(labels, th2, se2):
    print(f"  {lab:5s} = {v:+.6f}  ± {s:.6f}")
print(f"  R² = {r2_2:.5f}   力矩残差RMSE = {np.sqrt(np.mean(res2**2)):.4f} Nm")
print(f"  g_real = {gff_r.mean() + th2[5]:.4f}")

# ============ 3. 每周期(2s)窗口 OLS ============
print("\n" + "="*60)
print("  [方法3] 每周期(2s)窗口 OLS 参数一致性  (y = multi)")
print("="*60)
per = 2000  # 2s = 2000 点 @1kHz
starts = range(0, n - per, per)
w_th = []
for i, s0 in enumerate(starts):
    sl = slice(s0, s0+per)
    th, _, _, _ = ols(multi_r[sl], Phi[sl])
    w_th.append(th)
w_th = np.array(w_th)
print(f"  窗口数: {len(w_th)}")
med = np.median(w_th, axis=0)
std = np.std(w_th, axis=0)
for i, lab in enumerate(labels):
    if abs(med[i]) > 1e-8:
        print(f"  {lab:5s}: 中位={med[i]:+.6f}  标准差={std[i]:.5f}  CV={std[i]/abs(med[i])*100:.0f}%")
    else:
        print(f"  {lab:5s}: 中位={med[i]:+.6f}  标准差={std[i]:.5f}")

# ============ 4. 频域 FRF 验证 ============
print("\n" + "="*60)
print("  [方法4] 频域验证 (每个激励频率的复数平衡)")
print("="*60)
# 用整段 16 周期, 频率分辨率 = 0.5/16
multi_r2 = rs(multi)  # 用整个激励段
tuni2 = np.arange(t[act].min(), t[act].max(), 1.0/fs)
pos_r2 = np.interp(tuni2, t[m], pos[m]); spd_r2 = np.interp(tuni2, t[m], spd[m])
acc_r2 = savgol_filter(spd_r2, win, 3, deriv=1, delta=1.0/fs)
multi_r2 = np.interp(tuni2, t[m], multi[m])
n2 = len(tuni2)
nper = n2 // per
ncut = nper * per  # 截断到整数周期
print(f"  整段: {n2} 点 = {nper} 周期")

spd_c = spd_r2[:ncut] - spd_r2[:ncut].mean()
acc_c = acc_r2[:ncut] - acc_r2[:ncut].mean()
multi_c = multi_r2[:ncut] - multi_r2[:ncut].mean()
cos_c = np.cos(pos_r2[:ncut]) - np.cos(pos_r2[:ncut]).mean()
I_p_c = (spd_r2[:ncut] > 0).astype(float); I_p_c = I_p_c - I_p_c.mean()
I_n_c = (spd_r2[:ncut] < 0).astype(float); I_n_c = I_n_c - I_n_c.mean()
spd_Ip_c = spd_r2[:ncut]*I_p_c; spd_Ip_c = spd_Ip_c - spd_Ip_c.mean()
spd_In_c = spd_r2[:ncut]*I_n_c; spd_In_c = spd_In_c - spd_In_c.mean()

def coef(x):
    # 复数傅里叶系数 (单边, 幅值) 序列
    return np.fft.rfft(x) * 2.0 / ncut

F = np.fft.rfftfreq(ncut, 1.0/fs)
def at(freq_hz, X):
    k = int(round(freq_hz / (fs/ncut)))
    return coef(X)[k]

freqs = np.arange(0.5, 5.01, 0.5)
print(f"  {'f(Hz)':6s} {'tau_cmd':>10s} {'tau_model':>10s} {'|d|':>9s}  构成")
for f in freqs:
    # 用方法1参数模型预测该频率的复数力矩
    J, cvp, cvn, Tcp, Tcn, dG = th1
    T_model = (J*at(f, acc_c) + cvp*at(f, spd_Ip_c) + cvn*at(f, spd_In_c)
               + Tcp*at(f, I_p_c) + Tcn*at(f, I_n_c) + dG*at(f, cos_c))
    T_cmd = at(f, multi_c)
    diff = abs(T_model - T_cmd)
    comp = []
    if abs(at(f,acc_c)) > 1e-4: comp.append(f"J·a={abs(J*at(f,acc_c)):.3f}")
    if abs(cvp*at(f,spd_Ip_c)) > 1e-4: comp.append(f"cvp={abs(cvp*at(f,spd_Ip_c)):.3f}")
    if abs(cvn*at(f,spd_In_c)) > 1e-4: comp.append(f"cvn={abs(cvn*at(f,spd_In_c)):.3f}")
    if abs(Tcp*at(f,I_p_c)) > 1e-4: comp.append(f"Tcp={abs(Tcp*at(f,I_p_c)):.3f}")
    if abs(Tcn*at(f,I_n_c)) > 1e-4: comp.append(f"Tcn={abs(Tcn*at(f,I_n_c)):.3f}")
    if abs(dG*at(f,cos_c)) > 1e-4: comp.append(f"dG={abs(dG*at(f,cos_c)):.3f}")
    print(f"  {f:6.1f} {abs(T_cmd):10.4f} {abs(T_model):10.4f} {diff:9.4f}  {','.join(comp)}")

# ============ 5. 后验检查：模型仿真 vs 实测速度 ============
print("\n" + "="*60)
print("  [验证] 开环仿真 (命令力矩) vs 实测速度")
print("="*60)
def simulate(multi_in, g_ff_in, pos0, spd0, J, cvp, cvn, Tcp, Tcn, g_real):
    """前向仿真: J*a = multi + g_ff*cos - J... 实际: J*a + cv*v + Tc + g_real*cos = g_ff*cos + multi
    => a = (multi + (g_ff - g_real)*cos - cv*v - Tc*sign)/J
    """
    N = len(multi_in)
    p = np.zeros(N); v = np.zeros(N)
    p[0], v[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, N):
        vv = v[i-1]
        if vv > 0: fric = cvp*vv + Tcp
        elif vv < 0: fric = cvn*vv - Tcn
        else: fric = 0.0
        a = (multi_in[i] + (g_ff_in[i] - g_real)*np.cos(p[i-1]) - fric) * invJ
        v[i] = vv + a/fs
        p[i] = p[i-1] + vv/fs
    return p, v

J, cvp, cvn, Tcp, Tcn, dG = th1
g_real = gff_r.mean() + dG
# 用一小段（3 周期）仿真
n_sim = 3*per
p_sim, v_sim = simulate(multi_r[:n_sim], gff_r[:n_sim], pos_r[0], spd_r[0],
                        J, cvp, cvn, Tcp, Tcn, g_real)
v_rmse = np.sqrt(np.mean((v_sim - spd_r[:n_sim])**2))
p_rmse = np.sqrt(np.mean((p_sim - pos_r[:n_sim])**2))
print(f"  速度 RMSE = {v_rmse:.4f} rad/s  (速度范围 {spd_r[:n_sim].min():.2f}~{spd_r[:n_sim].max():.2f})")
print(f"  位置 RMSE = {p_rmse:.4f} rad  (位置范围 {pos_r[:n_sim].min():.2f}~{pos_r[:n_sim].max():.2f})")
