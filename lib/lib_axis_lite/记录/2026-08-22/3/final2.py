# -*- coding: utf-8 -*-
"""最终确认: 高分辨率 Welch 频响 + 惯性斜率检验 + 置信区间"""
import os, sys
import numpy as np
import pandas as pd
from scipy import signal
sys.stdout.reconfigure(encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))
df = pd.read_csv(os.path.join(HERE, 'plot_data.csv'), encoding='utf-8-sig')
v = df[df['CH0'] != 0].reset_index(drop=True)
t_raw = v['CH0'].values / 1e6
fs = 1000.0; dt = 1.0/fs
t0, t1 = t_raw[0], t_raw[-1]
n = int(round((t1-t0)/dt)) + 1
tg = t0 + np.arange(n)*dt
w = np.interp(tg, t_raw, v['CH2'].values)
tau = np.interp(tg, t_raw, v['CH3'].values)
chirp = np.interp(tg, t_raw, v['CH9'].values)
mask = np.abs(chirp) > 1e-6
w_e, tau_e = w[mask], tau[mask]

# ---- 高分辨率 Welch (nperseg=4096, 50% 重叠) ----
nperseg = 4096
f, Pwt = signal.csd(tau_e, w_e, fs=fs, nperseg=nperseg, noverlap=nperseg//2, detrend='linear')
f, Ptt = signal.welch(tau_e, fs=fs, nperseg=nperseg, noverlap=nperseg//2, detrend='linear')
f, Pww = signal.welch(w_e, fs=fs, nperseg=nperseg, noverlap=nperseg//2, detrend='linear')
H = Pwt / Ptt
mag = np.abs(H)
coh = np.abs(Pwt)**2/(Ptt*Pww + 1e-30)

sel = (f>=1.0) & (f<=8.0) & (coh>0.3)
g = mag[sel]*2*np.pi*f[sel]
print(f'频点 {sel.sum()} 个 (nperseg={nperseg}, 分辨率 {fs/nperseg:.2f}Hz)')
print('='*60)
print('惯性段 |H|·2πf (1-8Hz):')
print(f'  median = {np.median(g):.3f}  -> J = {1/np.median(g):.5f}')
print(f'  mean   = {np.mean(g):.3f}   -> J = {1/np.mean(g):.5f}')
print(f'  std    = {np.std(g):.3f}  (离散度 {100*np.std(g)/np.mean(g):.1f}%)')
Jp, Jm = 1/np.percentile(g, 84), 1/np.percentile(g, 16)
print(f'  1σ 区间 J ∈ [{Jp:.5f}, {Jm:.5f}]')

# 分频段(密)
print()
print('分频段 J:')
for lo, hi in [(1,2),(2,3),(3,4),(4,5),(5,6),(6,7),(7,8)]:
    s = (f>=lo)&(f<hi)&(coh>0.3)
    if s.sum()>2:
        gs = mag[s]*2*np.pi*f[s]
        print(f'  {lo}-{hi}Hz ({s.sum():2d}点): median J={1/np.median(gs):.5f}')

# ---- 惯性斜率检验: log|H| vs log f, 斜率应≈-1 ----
print()
s2 = (f>=1.0)&(f<=8.0)&(coh>0.3)
lf, lh = np.log(f[s2]), np.log(mag[s2])
A = np.hstack([np.ones_like(lf).reshape(-1,1), lf.reshape(-1,1)])
beta, *_ = np.linalg.lstsq(A, lh, rcond=None)
slope = beta[1]
print(f'log-log 斜率 = {slope:.3f}  (纯惯性应为 -1.0)')
print(f'  截距 {beta[0]:.3f} -> J = {np.exp(-beta[0]):.5f}')

# ---- 最终值 ----
J_final = 1/np.median(g)
print()
print('='*60)
print(f'FINAL: J_yaw = {J_final:.4f} kg·m²  (范围 {Jp:.4f}~{Jm:.4f})')
