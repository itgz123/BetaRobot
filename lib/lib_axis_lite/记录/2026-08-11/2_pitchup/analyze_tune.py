# -*- coding: utf-8 -*-
"""
分析 TUNE 阶段(2Hz正弦, kp=kd=0) 开环跟踪问题:
1. 速度噪声评估
2. 从本组数据直接辨识 [J, G, cv+, cv-, Tc+, Tc-] 对比当前设定
3. 仿真验证: 当前参数 vs 辨识参数 的跟踪误差
"""
import sys, io
if sys.platform == "win32" and isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8")
import numpy as np
import pandas as pd
from scipy.signal import savgol_filter, butter, filtfilt
from scipy.optimize import minimize

CSV = "drv/drv_axis_lite/记录/2026-08-11/2_pitchup/plot_data.csv"
FS = 1000.0

df = pd.read_csv(CSV)
ts = df["CH0"].values
nz = np.where(ts > 0)[0][0]
d = df.iloc[nz:].reset_index(drop=True).astype(float)
t = (d["CH0"].values - d["CH0"].values[0]) / 1e6
pos = d["CH1"].values; spd = d["CH2"].values
trq = d["CH3"].values; ref_pos = d["CH4"].values; ref_vel = d["CH5"].values; ref_acc = d["CH6"].values
gff = d["CH7"].values; iff = d["CH8"].values; fff = d["CH9"].values; setref = d["CH12"].values

# 稳定正弦段
m = (t > 8) & (t < 66)
t_g = t[m]; pos_g = pos[m]; spd_g = spd[m]; trq_g = trq[m]
setref_g = setref[m]; gff_g = gff[m]; iff_g = iff[m]; fff_g = fff[m]
ref_pos_g = ref_pos[m]; ref_vel_g = ref_vel[m]; ref_acc_g = ref_acc[m]
t_g = t_g - t_g[0]
dt = 1 / FS

# ---------- 1. 速度噪声评估 ----------
raw_noise = np.diff(spd_g) * FS
print("===== 速度信号质量 =====")
print(f"速度: 2Hz分量幅值~{np.sqrt(np.mean(spd_g**2))*np.sqrt(2):.4f}, max|{spd_g.max():.3f},{spd_g.min():.3f}|")
print(f"原始差分加速度: std={raw_noise.std():.3f} rad/s²  max={np.abs(raw_noise).max():.1f}")
# 高通残留(>30Hz视为噪声)
from numpy.fft import rfft, rfftfreq
fr = rfftfreq(len(spd_g), dt)
S = np.abs(rfft(spd_g - spd_g.mean()))
hf = (fr > 30)
print(f"速度频谱 30Hz以上能量占比: {S[hf].sum()/S.sum()*100:.1f}%")

# ---------- 2. 时域辨识 (滤波后) ----------
# 不同滤波窗
def ident(win):
    spd_f = savgol_filter(spd_g, win, 3)
    acc_f = savgol_filter(spd_g, win, 3, deriv=1, delta=dt)
    Ip = (spd_f > 0).astype(float); In = (spd_f < 0).astype(float)
    Phi = np.column_stack([acc_f, np.cos(pos_g), spd_f*Ip, spd_f*In, Ip, In])
    th, *_ = np.linalg.lstsq(Phi, trq_g, rcond=None)
    pred = Phi @ th
    r2 = 1 - np.var(trq_g - pred) / np.var(trq_g)
    return th, np.sqrt(np.mean((trq_g - pred)**2)), r2

print("\n===== 时域辨识 (输入=CH3实际力矩) =====")
for win in [21, 31, 51]:
    th, rmse, r2 = ident(win)
    print(f"  SG窗{win}: J={th[0]:.5e} G={th[1]:+.4f} cv+={th[2]:+.4f} cv-={th[3]:+.4f} "
          f"Tc+={th[4]:+.4f} Tc-={th[5]:+.4f}  R²={r2:.4f} RMSE={rmse:.4f}")

# ---------- 3. 频率法: 从2Hz分量直接估 J 和 阻尼 ----------
w = 2 * 2 * np.pi
def amp_phase(y):
    S = np.sin(w*t_g); C = np.cos(w*t_g)
    A, B, D = np.linalg.lstsq(np.column_stack([S, C, np.ones_like(S)]), y, rcond=None)[0]
    return np.sqrt(A*A+B*B), np.degrees(np.arctan2(A, B))
a_acc, p_acc = amp_phase(acc := savgol_filter(spd_g, 31, 3, deriv=1, delta=dt))
a_trq, p_trq = amp_phase(trq_g)
a_vel, p_vel = amp_phase(spd_g)
print("\n===== 2Hz 分量 (从辨识数据) =====")
print(f"  CH3 力矩: 幅值={a_trq:.4f} Nm 相位={p_trq:+.1f}°")
print(f"  加速度:   幅值={a_acc:.3f} 相位={p_acc:+.1f}°")
print(f"  速度:     幅值={a_vel:.4f} 相位={p_vel:+.1f}°")
# J = 力矩幅值/加速度幅值 (如果力矩主要驱动acc)
print(f"  J=τ/a = {a_trq/a_acc:.5f}  (含阻尼偏置)")
# 相位差 力矩 vs 加速度
print(f"  acc滞后torque: {p_acc-p_trq:+.1f}°   (=90°表示纯惯量)")

# ---------- 4. 仿真验证当前参数 ----------
def simulate(J, cp, cn, Tcp, Tcn, refacc, gravity_model="cos"):
    """开环前馈仿真: 用代码的 fff=function(v), iff=J*refacc, gff=G*cos(theta)"""
    n = len(refacc)
    om = np.zeros(n); th = np.zeros(n)
    th[0] = pos_g[0]; om[0] = spd_g[0]
    G = 0.28
    invJ = 1.0 / J
    for i in range(1, n):
        o = om[i-1]
        gff_i = G * np.cos(th[i-1])
        fff_i = (cp*o + Tcp) if o > 0 else ((cn*o - Tcn) if o < 0 else 0.0)
        cmd = gff_i + J*refacc[i-1] + fff_i
        # 实际重力+摩擦(假设真实参数=给定)
        g_t = G * np.cos(th[i-1])
        f_t = (cp*o + Tcp) if o > 0 else ((cn*o - Tcn) if o < 0 else 0.0)
        alpha = (cmd - g_t - f_t) * invJ
        om[i] = o + alpha * dt
        th[i] = th[i-1] + om[i] * dt
    return om, th

print("\n===== 开环仿真 (假设辨识参数=真实参数, 应完美跟踪) =====")
for lbl, Js, cp, cn, Tcp, Tcn in [("当前参数", 0.008, 0.05, 0.06, 0.02, 0.0),
                                  ("增大cv", 0.008, 0.09, 0.10, 0.02, 0.0),
                                  ("增J", 0.0087, 0.05, 0.06, 0.02, 0.0)]:
    om_sim, th_sim = simulate(Js, cp, cn, Tcp, Tcn, ref_acc_g)
    a_s, p_s = amp_phase(om_sim)
    a_m, p_m = amp_phase(spd_g)
    rmse = np.sqrt(np.mean((om_sim - spd_g)**2))
    print(f"  {lbl}: 仿真2Hz幅值={a_s:.4f} 相位={p_s:+.1f}° | 实测幅值={a_m:.4f} 相位={p_m:+.1f}° | 速度RMSE={rmse:.4f}")
