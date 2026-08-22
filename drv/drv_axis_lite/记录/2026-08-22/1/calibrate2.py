"""
二维扫描 (J_real, 粘滞摩擦B) 匹配用户实测振幅: amp0.1→0.13, amp0.3→0.45
当前配置 J_cfg=0.012, g_cfg=0.28, 无摩擦前馈。
电机一阶响应 tau_m=14ms。摩擦对称: Cp=Cn=0.01(库仑小), Bp=Bn=B。
"""
import numpy as np

dt = 0.001
freq = 2.0
w = 2*np.pi*freq
TAU_M = 0.014

def sim(J_cfg, g_cfg, fr_cfg, Jr, gr, fr_real, A, n_sec=10.0):
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t)
    ref_acc = -A*w*w*np.sin(w*t)
    th = np.empty_like(t); v = np.empty_like(t); tau = np.empty_like(t)
    th[0]=0.0; v[0]=0.0; tau[0]=0.0
    def frc(vv, fr):
        if vv>0: return fr[0]+fr[1]*vv
        if vv<0: return -fr[0]+fr[1]*vv
        return 0.0
    for k in range(len(t)-1):
        setref = g_cfg*np.cos(th[k]) + J_cfg*ref_acc[k] + frc(v[k], fr_cfg)
        tau[k+1] = tau[k] + (setref - tau[k])*dt/TAU_M
        alpha = (tau[k+1] - gr*np.cos(th[k]) - frc(v[k], fr_real)) / Jr
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    s = slice(int(len(t)-3.0/dt), len(t))
    return (np.max(th[s])-np.min(th[s]))/2

J_cfg, g_cfg = 0.012, 0.28
fr_cfg = (0.0, 0.0)          # 当前无摩擦前馈
gr = 0.28                     # g_real 与 cfg 相等(振幅标定不敏感于g)

targets = {0.1: 0.13, 0.3: 0.45}
print('扫描 J_real × 粘滞B (库仑=0.01):')
print('  J_real\\B     0.02      0.05      0.10      0.15      0.20      0.25')
best = None
for Jr in [0.006, 0.008, 0.010, 0.012, 0.014]:
    row = []
    for B in [0.02, 0.05, 0.10, 0.15, 0.20, 0.25]:
        fr_real = (0.01, B)
        a1 = sim(J_cfg, g_cfg, fr_cfg, Jr, gr, fr_real, 0.1)
        a3 = sim(J_cfg, g_cfg, fr_cfg, Jr, gr, fr_real, 0.3)
        err = abs(a1-0.13)/0.13 + abs(a3-0.45)/0.45
        row.append(f'{a1:.2f}/{a3:.2f}')
        if best is None or err < best[0]:
            best = (err, Jr, B, a1, a3)
    print(f'  {Jr:.3f}      ' + '   '.join(row))
print(f'\n最优: J_real={best[1]}, B={best[2]} → amp0.1={best[3]:.3f}, amp0.3={best[4]:.3f}, err={best[0]:.3f}')
