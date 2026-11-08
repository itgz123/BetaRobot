"""
验证辨识参数: 视觉对比 + 交叉验证(amp0.1) + 稳健性(分段重辨识)
"""
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

dt = 0.001

def simulate(J, grav, Cp, Cn, Bp, Bn, pos0, v0, tau, dt=dt):
    n = len(tau)
    theta = np.empty(n); v = np.empty(n)
    theta[0] = pos0; v[0] = v0
    for k in range(n-1):
        if v[k] > 0: fr = Cp + Bp*v[k]
        elif v[k] < 0: fr = -Cn + Bn*v[k]
        else: fr = 0.0
        alpha = (tau[k] - grav*np.cos(theta[k]) - fr)/J
        v[k+1] = v[k] + alpha*dt
        theta[k+1] = theta[k] + v[k+1]*dt
    return theta

P = dict(J=0.0061, grav=0.302, Cp=0.0159, Cn=0.0317, Bp=0.0293, Bn=0.0388)
P0 = dict(J=0.012, grav=0.28, Cp=0, Cn=0, Bp=0, Bn=0)

def load(fn):
    df = pd.read_csv(f'drv/drv_axis_lite/记录/2026-08-22/{fn}')
    mask = (df['CH4'] != 0) & (df['CH1'] != 0)
    seg = df[mask].reset_index(drop=True)
    return seg

# ---------- 1. 视觉对比: plot_data2 (amp0.3) ----------
seg = load('plot_data2.csv')
pos = seg.CH1.values; torq = seg.CH3.values
start = int(len(seg)*0.3)
th_new = simulate(**P, pos0=pos[start], v0=seg.CH2.values[start], tau=torq[start:])
th_old = simulate(**P0, pos0=pos[start], v0=seg.CH2.values[start], tau=torq[start:])
# 只显示其中 3 个周期 (~1.5s)
w = 1500
t = np.arange(w)*dt
th_new_w = th_new[:w]; th_old_w = th_old[:w]
pos_w = pos[start:start+w]; ref_w = seg.CH4.values[start:start+w]
print('=== amp0.3 (plot_data2) 视觉对比 前1.5s ===')
e_new = th_new_w - pos_w; e_old = th_old_w - pos_w
print(f'  新参数(加摩擦): rmse={np.sqrt(np.mean(e_new**2)):.4f}')
print(f'  旧参数(无摩擦): rmse={np.sqrt(np.mean(e_old**2)):.4f}')
plt.figure(figsize=(14,8))
plt.subplot(3,1,1)
plt.plot(t, pos_w, label='measured pos', lw=1)
plt.plot(t, th_new_w, label='sim w/ friction', ls='--', lw=1)
plt.plot(t, th_old_w, label='sim no friction', ls=':', lw=1)
plt.plot(t, ref_w, label='ref pos', alpha=0.5)
plt.legend(); plt.title('plot_data2 amp0.3')
plt.subplot(3,1,2)
plt.plot(t, e_new, label='err w/ friction', lw=1)
plt.plot(t, e_old, label='err no friction', lw=1)
plt.legend()
plt.subplot(3,1,3)
plt.plot(t, torq[start:start+w], label='CH3 torque', lw=1)
plt.plot(t, seg.CH2.values[start:start+w], label='CH2 vel', lw=1)
plt.legend()
plt.tight_layout(); plt.savefig('drv/drv_axis_lite/记录/2026-08-22/verify_amp03.png', dpi=110)
plt.close()
print('  图: verify_amp03.png')

# ---------- 2. 交叉验证: amp0.1 (plot_data.csv) ----------
seg1 = load('plot_data.csv')
pos1 = seg1.CH1.values; torq1 = seg1.CH3.values
start1 = int(len(seg1)*0.3)
th1_new = simulate(**P, pos0=pos1[start1], v0=seg1.CH2.values[start1], tau=torq1[start1:])
th1_old = simulate(**P0, pos0=pos1[start1], v0=seg1.CH2.values[start1], tau=torq1[start1:])
e1n = th1_new - pos1[start1:]; e1o = th1_old - pos1[start1:]
print('=== amp0.1 (plot_data.csv) 交叉验证 ===')
print(f'  新参数(加摩擦): rmse={np.sqrt(np.mean(e1n**2)):.4f}')
print(f'  旧参数(无摩擦): rmse={np.sqrt(np.mean(e1o**2)):.4f}')

# ---------- 3. 稳健性: 用前半/后半重新辨识 ----------
def cost_given(p, pos_fit, torq_fit, v0):
    J, grav, Cp, Cn, Bp, Bn = p
    if min(p) < 0 or J <= 0 or grav <= 0: return 1e6
    th = simulate(J, grav, Cp, Cn, Bp, Bn, pos_fit[0], v0, torq_fit)
    return float(np.sqrt(np.mean((th-pos_fit)**2)))
from scipy.optimize import minimize
for tag, a0, b0 in [('前半', 0.2, 0.6), ('后半', 0.6, 1.0)]:
    a = int(len(seg)*a0); b = int(len(seg)*b0)
    pf = pos[a:b]; tf = torq[a:b]
    r = minimize(lambda p: cost_given(p, pf, tf, seg.CH2.values[a]),
                 [0.006, 0.30, 0.01, 0.02, 0.02, 0.03], method='L-BFGS-B',
                 bounds=[(0.003,0.03),(0.2,0.5),(0,0.2),(0,0.2),(0,0.2),(0,0.2)],
                 options={'maxiter':300})
    J,g,Cp,Cn,Bp,Bn = r.x
    print(f'=== {tag}段重辨识 ===')
    print(f'  J={J:.4f} grav={g:.3f} Cp={Cp:.4f} Cn={Cn:.4f} Bp={Bp:.4f} Bn={Bn:.4f} rmse={r.fun:.4f}')
