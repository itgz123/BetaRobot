"""
线性最小二乘辨识 pitchup 动力学:
  CH3(实际力矩) = J*alpha + g*cos(theta) + friction(v)
  v>0: friction = Cp + Bp*v
  v<0: friction = -Cn + Bn*v
加速度 alpha 用位置 CH1 的 SG 二阶导(避免 CH2 低通滞后)。
"""
import pandas as pd
import numpy as np
from scipy.signal import savgol_filter

dt = 0.001  # 1kHz

def load(fn):
    df = pd.read_csv(f'drv/drv_axis_lite/记录/2026-08-22/{fn}')
    mask = (df['CH4'] != 0) & (df['CH1'] != 0)
    return df[mask].reset_index(drop=True)

def solve(seg, w_sg=81, p_sg=5, lag_comp=0):
    pos = seg.CH1.values
    vel = seg.CH2.values
    torq = seg.CH3.values
    n = len(seg)
    # 位置一阶、二阶导 (SG)
    v_from_pos = savgol_filter(pos, w_sg, p_sg, deriv=1, delta=dt)
    a_from_pos = savgol_filter(pos, w_sg, p_sg, deriv=2, delta=dt)
    # 力矩滞后补偿: 前移 lag_comp 点 (对齐力矩与加速度)
    a = a_from_pos
    tq = torq
    if lag_comp > 0:
        a = a[:-lag_comp]; tq = tq[lag_comp:]
        vel = vel[:-lag_comp]; pos = pos[:-lag_comp]
    # 取稳态中段, 过滤低速样本(避免 sign 抖动)
    start, end = int(n*0.15), int(n*0.95)
    sl = slice(start, end)
    a_s = a[sl]; tq_s = tq[sl]; v_s = vel[sl]; p_s = pos[sl]
    good = np.abs(v_s) > 0.02
    a_s = a_s[good]; tq_s = tq_s[good]; v_s = v_s[good]; p_s = p_s[good]

    m_pos = v_s > 0
    # X = [alpha, cos(theta), f1, f2] 其中 f 是摩擦项
    # 合并两段: 用两套摩擦列
    X = np.column_stack([
        a_s,
        np.cos(p_s),
        np.where(m_pos, 1.0, 0.0),
        np.where(~m_pos, -1.0, 0.0),
        np.where(m_pos, v_s, 0.0),
        np.where(~m_pos, v_s, 0.0),
    ])
    coef, res, *_ = np.linalg.lstsq(X, tq_s, rcond=None)
    J, g, Cp, Cn, Bp, Bn = coef
    fit = X @ coef
    rmse = float(np.sqrt(np.mean((tq_s - fit)**2)))
    # 解释度
    ss = float(1 - np.sum((tq_s-fit)**2)/np.sum((tq_s-tq_s.mean())**2))
    return dict(J=J, g=g, Cp=Cp, Cn=Cn, Bp=Bp, Bn=Bn, rmse=rmse, r2=ss, n=len(a_s))

for fn in ['plot_data2.csv', 'plot_data.csv']:
    seg = load(fn)
    print(f'===== {fn} =====')
    for lag in [0, 5, 14, 20]:
        r = solve(seg, lag_comp=lag)
        print(f'  滞后补偿{lag}点({lag}ms): J={r["J"]:.4f} g={r["g"]:.3f} '
              f'Cp={r["Cp"]:.4f} Cn={r["Cn"]:.4f} Bp={r["Bp"]:.4f} Bn={r["Bn"]:.4f} '
              f'rmse={r["rmse"]:.4f} r2={r["r2"]:.3f}')
    print()
