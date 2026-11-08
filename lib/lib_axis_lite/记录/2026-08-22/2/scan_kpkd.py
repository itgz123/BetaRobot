"""
扫描 kp/kd 满足位置误差 < 0.6° (0.01047 rad)。
闭环仿真: MIT反馈 + 电机一阶延迟(14ms) + 物理模型。
物理参数(已标定): J=0.008, g=0.30, 摩擦(0.02,0.03), 电机tau=14ms。
设定: amplitude=0.2, freq=2Hz。
"""
import numpy as np

dt = 0.001
freq = 2.0
w = 2*np.pi*freq
TAU_M = 0.014
A = 0.2
J_real, g_real = 0.008, 0.30
fr_real = (0.02, 0.03)

def frc(v, fr):
    if v > 0: return fr[0]+fr[1]*v
    if v < 0: return -fr[0]+fr[1]*v
    return 0.0

RC_V = 0.02   # 速度低通 (speed_lpf_rc)

def sim(kp, kd, n_sec=10.0):
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t)
    refv = A*w*np.cos(w*t)
    refacc = -A*w*w*np.sin(w*t)
    th = np.empty_like(t); v = np.empty_like(t); tau = np.empty_like(t)
    v_fb = np.empty_like(t)  # 低通后的速度反馈
    th[0]=0.0; v[0]=0.0; tau[0]=0.0; v_fb[0]=0.0
    for k in range(len(t)-1):
        # 速度反馈低通 (一阶)
        v_fb[k+1] = v_fb[k] + (v[k] - v_fb[k])*dt/RC_V
        ff = g_real*np.cos(th[k]) + J_real*refacc[k]          # 前馈(重力+惯量)
        fb = kp*(ref[k]-th[k]) + kd*(refv[k]-v_fb[k+1])       # MIT 反馈(用低通速度)
        setref = fb + ff
        tau[k+1] = tau[k] + (setref - tau[k])*dt/TAU_M
        alpha = (tau[k+1] - g_real*np.cos(th[k]) - frc(v[k], fr_real)) / J_real
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    s = slice(int(len(t)-3/dt), len(t))
    err = ref[s] - th[s]
    return np.max(np.abs(err)), err.std(), np.median(th[s])

LIMIT = 0.6*np.pi/180  # 0.6° in rad
print(f'目标: max|err| < {LIMIT*1000:.1f} mrad ({0.6}°)')
print(f'当前 kp=20,kd=1.2 实测: max={0.0357*1000:.1f} mrad, std={0.0236*1000:.1f} mrad')
print(f'{"kp":>6} {"kd":>6} {"maxErr(mrad)":>14} {"std(mrad)":>10} {"中心偏移":>8}  状态')
print('-'*70)
for kp in [15, 20, 30, 40, 50, 70, 100, 150]:
    for zeta in [0.7, 1.0, 1.3]:
        kd = 2*zeta*np.sqrt(J_real*kp)
        emax, es, cen = sim(kp, kd)
        ok = 'OK' if emax < LIMIT else ('~' if emax < LIMIT*1.5 else 'x')
        print(f'{kp:6.0f} {kd:6.2f} {emax*1000:14.1f} {es*1000:10.1f} {cen*1000:8.1f}  {ok}')
