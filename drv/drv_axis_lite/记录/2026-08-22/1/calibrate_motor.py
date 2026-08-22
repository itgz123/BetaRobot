"""
标定: 闭环仿真 + 电机一阶响应模型(模拟电机内部阻尼/延迟)
物理参数 J_real 扫描, 匹配用户Vofa实测稳态振幅:
  amp0.1 → 0.13, amp0.3 → 0.45
当前配置: J_cfg=0.012, g_cfg=0.28, 无摩擦
"""
import numpy as np

dt = 0.001
freq = 2.0
w = 2*np.pi*freq
TAU_M = 0.014  # 电机一阶响应时间常数 s (CH3滞后CH12约14ms)

# 摩擦(物理, amp0.1辨识)
FR = dict(Cp=0.027, Cn=0.028, Bp=0.05, Bn=0.09)

def friction(v, fr):
    if v > 0: return fr['Cp'] + fr['Bp']*v
    if v < 0: return -fr['Cn'] + fr['Bn']*v
    return 0.0

def sim_cfg(J_cfg, g_cfg, fr_cfg, J_real, A, tau_m=TAU_M, n_sec=12.0):
    """ 返回稳态振幅(最后若干周期的峰峰/2) """
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t)
    ref_acc = -A*w*w*np.sin(w*t)
    th = np.empty_like(t); v = np.empty_like(t); tau = np.empty_like(t)
    th[0]=0.0; v[0]=0.0; tau[0]=0.0
    for k in range(len(t)-1):
        setref = g_cfg*np.cos(th[k]) + J_cfg*ref_acc[k] + friction(v[k], fr_cfg)
        # 电机一阶响应
        tau[k+1] = tau[k] + (setref - tau[k])*dt/tau_m
        alpha = (tau[k+1] - g_cfg*np.cos(th[k]) - friction(v[k], FR)) / J_real
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    # 稳态: 最后 4 秒
    s = slice(int(len(t)-4.0/dt), len(t))
    return (np.max(th[s])-np.min(th[s]))/2

print('当前配置 (J_cfg=0.012, g=0.28, 无摩擦) + 电机一阶模型:')
print(f'  J_real    amp0.1振幅   amp0.3振幅   (目标: 0.13 / 0.45)')
for Jr in [0.004, 0.005, 0.0056, 0.006, 0.007, 0.008, 0.0085, 0.009, 0.010, 0.012]:
    a1 = sim_cfg(0.012, 0.28, dict(Cp=0,Cn=0,Bp=0,Bn=0), Jr, 0.1)
    a3 = sim_cfg(0.012, 0.28, dict(Cp=0,Cn=0,Bp=0,Bn=0), Jr, 0.3)
    print(f'  {Jr:.4f}     {a1:.3f}         {a3:.3f}')
