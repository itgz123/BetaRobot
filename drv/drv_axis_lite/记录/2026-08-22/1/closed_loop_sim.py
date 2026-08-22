"""
闭环仿真: 前馈控制(J_cfg,g_cfg,friction_cfg) + 物理模型(J_real,g_real,friction_real)
预测不同控制器配置下的正弦跟踪误差。场景: 当前 / 仅加摩擦 / 修惯量+加摩擦。
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

dt = 0.001
A, freq = 0.3, 2.0
w = 2*np.pi*freq

# 物理参数 (真实系统, 来自辨识 amp0.1 段)
J_real, g_real = 0.0056, 0.30
fr_real = dict(Cp=0.027, Cn=0.028, Bp=0.05, Bn=0.09)

def friction(v, fr):
    if v > 0: return fr['Cp'] + fr['Bp']*v
    if v < 0: return -fr['Cn'] + fr['Bn']*v
    return 0.0

def sim(J_cfg, g_cfg, fr_cfg, n_cyc=20):
    t = np.arange(0, n_cyc/freq, dt)
    ref = A*np.sin(w*t)
    ref_acc = -A*w*w*np.sin(w*t)
    th = np.empty_like(t); v = np.empty_like(t)
    th[0] = 0.0; v[0] = A*w
    for k in range(len(t)-1):
        setref = g_cfg*np.cos(th[k]) + J_cfg*ref_acc[k] + friction(v[k], fr_cfg)
        alpha = (setref - g_real*np.cos(th[k]) - friction(v[k], fr_real)) / J_real
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    # 稳态: 最后 10 周期
    cut = int(len(t)*0.5)
    err = th[cut:] - ref[cut:]
    rmse = np.sqrt(np.mean(err**2))
    amp_sim = (np.max(th[cut:]) - np.min(th[cut:]))/2
    return t, ref, th, rmse, amp_sim

# 场景
scenes = [
    ('A 当前配置 (J=0.012,g=0.28,无摩擦)', dict(J=0.012, g=0.28, fr=dict(Cp=0,Cn=0,Bp=0,Bn=0))),
    ('B 仅加摩擦 (J=0.012,g=0.28)',       dict(J=0.012, g=0.28, fr=fr_real)),
    ('C 修惯量+重力+摩擦 (J=0.0056,g=0.30)', dict(J=0.0056, g=0.30, fr=fr_real)),
]
print(f'设定: A={A} rad, f={freq} Hz   (实测 amp0.3: 反馈幅值0.97, 跟踪rmse≈0.17)')
print(f'物理: J_real={J_real}, g_real={g_real}, 摩擦={fr_real}')
print()
res = {}
for name, c in scenes:
    t, ref, th, rmse, amp = sim(c['J'], c['g'], c['fr'])
    res[name] = (t, ref, th)
    print(f'{name}: 反馈幅值={amp:.3f} rad, 跟踪rmse={rmse:.4f} rad  (改善{(rmse<0.17)}  {rmse/0.17*100:.0f}% of 当前)')
print()
print('注: 当前实测跟踪rmse≈0.167 (std of CH1-CH4)')

# 画图
fig, ax = plt.subplots(len(scenes), 1, figsize=(14, 9), sharex=True)
for i, (name, c) in enumerate(scenes):
    t, ref, th = res[name]
    t_s = t[:2000]
    ax[i].plot(t_s, ref[:2000], 'k--', label='ref', lw=1)
    ax[i].plot(t_s, th[:2000], label=name.split('(')[0], lw=1)
    ax[i].legend(loc='upper right'); ax[i].grid(alpha=0.3)
    ax[i].set_ylabel('pos (rad)')
ax[-1].set_xlabel('time (s)')
plt.suptitle('Closed-loop tracking with different controller configs')
plt.tight_layout()
plt.savefig('drv/drv_axis_lite/记录/2026-08-22/closed_loop_sim.png', dpi=110)
print('图: closed_loop_sim.png')
