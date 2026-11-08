"""
2026-08-22 pitchup 摩擦辨识
用 CH3 实际力矩驱动运动方程仿真，扫参匹配实测位置 CH1，
反推真实 J(惯量)、gravity(重力)、摩擦参数。
数据：plot_data2.csv (amplitude=0.3, freq=2Hz)
"""
import pandas as pd
import numpy as np
from scipy.optimize import minimize

# ---------- 读数据 ----------
df = pd.read_csv('drv/drv_axis_lite/记录/2026-08-22/plot_data2.csv')
mask = (df['CH4'] != 0) & (df['CH1'] != 0)
seg = df[mask].reset_index(drop=True)

pos_meas = seg.CH1.values.astype(float)
torq = seg.CH3.values.astype(float)      # 实际力矩 (轴侧 Nm)
vel_meas = seg.CH2.values.astype(float)

# 真实采样率由过零检测得出 ≈1kHz (ref 2Hz 正弦, ~503 点/周期)
n = len(seg)
dt = 0.001  # s
print(f'n={n}, dt={dt*1000:.3f}ms, duration={n*dt:.2f}s')

# 只用后半段做拟合（初始瞬态有未知初值，取稳态段）
start = int(n*0.2)
pos_fit = pos_meas[start:]
torq_fit = torq[start:]
vel0 = vel_meas[start]  # 用实测速度做初值

# ---------- 运动方程仿真 ----------
def simulate(J, grav, C_pos, C_neg, B_pos, B_neg, pos0, v0, tau, dt=dt):
    """ J*alpha = tau - grav*cos(pos) - friction(v) """
    theta = np.empty(len(tau))
    v = np.empty(len(tau))
    theta[0] = pos0
    v[0] = v0
    for k in range(len(tau)-1):
        fr = 0.0
        if v[k] > 0:
            fr = C_pos + B_pos * v[k]
        elif v[k] < 0:
            fr = -C_neg + B_neg * v[k]
        alpha = (tau[k] - grav*np.cos(theta[k]) - fr) / J
        v[k+1] = v[k] + alpha * dt
        theta[k+1] = theta[k] + v[k+1] * dt
    return theta

# ---------- 代价函数 ----------
def cost(p):
    J, grav, C_pos, C_neg, B_pos, B_neg = p
    if J <= 0 or grav <= 0 or C_pos < 0 or C_neg < 0 or B_pos < 0 or B_neg < 0:
        return 1e6
    theta_sim = simulate(J, grav, C_pos, C_neg, B_pos, B_neg, pos_fit[0], vel0, torq_fit)
    err = theta_sim - pos_fit
    return float(np.sqrt(np.mean(err**2)))

# ---------- 分步辨识：先 J/grav（摩擦=0），再全参 ----------
print('Step1: 仅扫描 J, gravity (摩擦=0)...')
res1 = minimize(cost, [0.012, 0.28, 0.0, 0.0, 0.0, 0.0],
                method='L-BFGS-B',
                bounds=[(0.003, 0.05), (0.10, 0.60), (0,0),(0,0),(0,0),(0,0)],
                options={'maxiter': 200})
J0, g0 = res1.x[0], res1.x[1]
print(f'  J={J0:.4f}, gravity={g0:.4f}, rmse={res1.fun:.4f}')

print('Step2: 全参数优化 (J, gravity, 4 friction)...')
res2 = minimize(cost, [J0, g0, 0.01, 0.01, 0.02, 0.02],
                method='L-BFGS-B',
                bounds=[(0.003, 0.05), (0.10, 0.60), (0, 0.5), (0, 0.5), (0, 0.5), (0, 0.5)],
                options={'maxiter': 500})
J, grav, Cp, Cn, Bp, Bn = res2.x
print(f'  J={J:.4f}, gravity={grav:.4f}')
print(f'  Coulomb_pos={Cp:.4f}, Coulomb_neg={Cn:.4f}')
print(f'  Viscous_pos={Bp:.4f}, Viscous_neg={Bn:.4f}')
print(f'  rmse={res2.fun:.4f}')

# ---------- 对照：无摩擦 vs 有摩擦 ----------
th0 = simulate(J0, g0, 0,0,0,0, pos_fit[0], vel0, torq_fit)
th1 = simulate(J, grav, Cp, Cn, Bp, Bn, pos_fit[0], vel0, torq_fit)
e0 = np.sqrt(np.mean((th0-pos_fit)**2))
e1 = np.sqrt(np.mean((th1-pos_fit)**2))
print(f'  [对照] 无摩擦 rmse={e0:.4f}, 加辨识摩擦 rmse={e1:.4f}')

# ---------- 输出可直接写入配置的参数 ----------
print()
print('===== 建议写入 app_gimbal.c =====')
print(f'    .gravity = {grav:.3f},')
print(f'    .inertia = {J:.4f},')
print(f'    .friction_coulomb_pos = {Cp:.4f},')
print(f'    .friction_coulomb_neg = {Cn:.4f},')
print(f'    .friction_viscous_pos = {Bp:.4f},')
print(f'    .friction_viscous_neg = {Bn:.4f},')
