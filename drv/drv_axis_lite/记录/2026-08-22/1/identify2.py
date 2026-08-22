"""
改进版辨识: 分段重置的短段仿真。
用 CH12(setref 命令力矩) 或 CH3 驱动运动方程, 分段(每 seg_len 点)重置初值为实测,
避免开环误差累积。目标: 最小化仿真位置与实测位置误差 → 反推 J,g,摩擦(物理参数)。
"""
import pandas as pd
import numpy as np
from scipy.optimize import minimize

dt = 0.001

def load(fn):
    df = pd.read_csv(f'drv/drv_axis_lite/记录/2026-08-22/{fn}')
    mask = (df['CH4'] != 0) & (df['CH1'] != 0)
    return df[mask].reset_index(drop=True)

def seg_sim_cost(p, pos, vel, tau, seg_len=2000, n_seg=8, dt=dt):
    """ 分段短段仿真, 返回平均 rmse """
    J, g, Cp, Cn, Bp, Bn = p
    if min(p) < 0 or J <= 0 or g <= 0:
        return 1e6
    n = len(pos)
    # 取从 15% 起的段
    starts = np.linspace(int(n*0.15), n - seg_len, n_seg).astype(int)
    errs = []
    for s0 in starts:
        sl = slice(s0, s0+seg_len)
        theta = np.empty(seg_len); v = np.empty(seg_len)
        theta[0] = pos[sl][0]; v[0] = vel[sl][0]
        tq = tau[sl]
        for k in range(seg_len-1):
            if v[k] > 0: fr = Cp + Bp*v[k]
            elif v[k] < 0: fr = -Cn + Bn*v[k]
            else: fr = 0.0
            alpha = (tq[k] - g*np.cos(theta[k]) - fr) / J
            v[k+1] = v[k] + alpha*dt
            theta[k+1] = theta[k] + v[k+1]*dt
        errs.append(np.sqrt(np.mean((theta - pos[sl])**2)))
    return float(np.mean(errs))

def identify(seg, drive, tag):
    pos = seg.CH1.values; vel = seg.CH2.values
    tau = seg.CH12.values if drive=='CH12' else seg.CH3.values
    best = None
    for init in [[0.008,0.28,0.01,0.01,0.02,0.02],
                 [0.015,0.30,0.02,0.02,0.03,0.03],
                 [0.005,0.25,0.00,0.00,0.00,0.00]]:
        r = minimize(lambda p: seg_sim_cost(p, pos, vel, tau),
                     init, method='L-BFGS-B',
                     bounds=[(0.003,0.03),(0.15,0.5),(0,0.2),(0,0.2),(0,0.3),(0,0.3)],
                     options={'maxiter':400})
        if best is None or r.fun < best.fun:
            best = r
    J,g,Cp,Cn,Bp,Bn = best.x
    print(f'  [{tag}] J={J:.4f} g={g:.3f} Cp={Cp:.4f} Cn={Cn:.4f} Bp={Bp:.4f} Bn={Bn:.4f} rmse={best.fun:.4f}')

for drive in ['CH12', 'CH3']:
    print(f'===== 驱动源: {drive} =====')
    identify(load('plot_data2.csv'), drive, 'amp0.3 plot_data2')
    identify(load('plot_data.csv'), drive, 'amp0.1 plot_data ')
    print()
