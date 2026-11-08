"""
用标定物理参数(J_real=0.008,摩擦B=0.03,Coulomb=0.02,电机14ms)验证配置:
  A 当前(0.012,无摩擦)  B 仅摩擦  C 新配置(0.008+摩擦)
输出: 稳态振幅(应≈设定) + 跟踪rmse
"""
import numpy as np

dt = 0.001
freq = 2.0
w = 2*np.pi*freq
TAU_M = 0.014
Jr, gr = 0.008, 0.30
fr_real = (0.02, 0.03)   # (库仑, 粘滞)

def frc(vv, fr):
    if vv>0: return fr[0]+fr[1]*vv
    if vv<0: return -fr[0]+fr[1]*vv
    return 0.0

def sim(J_cfg, g_cfg, fr_cfg, A, n_sec=10.0):
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t)
    ref_acc = -A*w*w*np.sin(w*t)
    th = np.empty_like(t); v = np.empty_like(t); tau = np.empty_like(t)
    th[0]=0.0; v[0]=0.0; tau[0]=0.0
    for k in range(len(t)-1):
        setref = g_cfg*np.cos(th[k]) + J_cfg*ref_acc[k] + frc(v[k], fr_cfg)
        tau[k+1] = tau[k] + (setref - tau[k])*dt/TAU_M
        alpha = (tau[k+1] - gr*np.cos(th[k]) - frc(v[k], fr_real)) / Jr
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    s = slice(int(len(t)-3.0/dt), len(t))
    amp = (np.max(th[s])-np.min(th[s]))/2
    rmse = np.sqrt(np.mean((th[s]-ref[s])**2))
    return amp, rmse

fr_cfg_phys = (0.02, 0.03)  # 物理摩擦(标定)
scenes = [
    ('A 当前配置 J=0.012 无摩擦',       dict(J=0.012, g=0.28, fr=(0,0))),
    ('B 仅加摩擦前馈(满)',              dict(J=0.012, g=0.28, fr=fr_cfg_phys)),
    ('C J=0.008+满摩擦前馈',            dict(J=0.008, g=0.30, fr=fr_cfg_phys)),
    ('D J=0.008 无摩擦前馈',            dict(J=0.008, g=0.30, fr=(0,0))),
    ('E J=0.008+半摩擦前馈(0.01,0.015)', dict(J=0.008, g=0.30, fr=(0.01,0.015))),
    ('F J=0.008+1/4摩擦前馈(0.005,0.008)',dict(J=0.008, g=0.30, fr=(0.005,0.008))),
]
print(f'物理: J_real={Jr}, g_real={gr}, 摩擦={fr_real}, 电机tau={TAU_M}s')
print('目标振幅: amp0.1→0.10, amp0.3→0.30')
print()
for name, c in scenes:
    a1, e1 = sim(c['J'], c['g'], c['fr'], 0.1)
    a3, e3 = sim(c['J'], c['g'], c['fr'], 0.3)
    print(f'{name}:')
    print(f'   amp0.1: 振幅={a1:.3f} (设定0.1)  rmse={e1:.4f}')
    print(f'   amp0.3: 振幅={a3:.3f} (设定0.3)  rmse={e3:.4f}')
    print()
