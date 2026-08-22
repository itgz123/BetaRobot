"""
用实测反推参数验证 RC=0.002 方案:
- 电机延迟 τm=8.6ms (从18Hz相位滞后98°反推)
- RC=0.002 (截止80Hz), kp=80, kd=1.2
对比: RC=0.008 应振荡, RC=0.002 应稳定
"""
import numpy as np

J, g, B = 0.008, 0.30, 0.035
dt = 0.001
A, FREQ = 0.2, 2.0
w = 2*np.pi*FREQ
LIMIT = 0.6*np.pi/180

def sim(kp, kd, RC, TAU_M=0.0086, n_sec=15.0):
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t); refv = A*w*np.cos(w*t); refa = -A*w*w*np.sin(w*t)
    th = np.zeros_like(t); v = np.zeros_like(t); tau = np.zeros_like(t); vfb = np.zeros_like(t)
    for k in range(len(t)-1):
        vfb[k+1] = vfb[k] + (v[k]-vfb[k])*dt/RC
        ff = g*np.cos(th[k]) + J*refa[k]
        fb = kp*(ref[k]-th[k]) + kd*(refv[k]-vfb[k+1])
        tau[k+1] = tau[k] + (fb+ff - tau[k])*dt/TAU_M
        v[k+1] = v[k] + (tau[k+1] - g*np.cos(th[k]) - B*v[k])/J*dt
        th[k+1] = th[k] + v[k+1]*dt
    s = slice(int(len(t)*0.5), len(t))
    err = ref[s]-th[s]
    return np.max(np.abs(err)), err.std()

print(f'目标: {LIMIT*1000:.1f}mrad  模型: τm=8.6ms')
print(f'{"RC":>6} {"kp":>5} {"kd":>4} {"maxErr":>9} {"std":>7} {"状态":>6}')
for RC in [0.008, 0.002]:
    for kp,kd in [(20,1.2),(40,1.2),(80,1.2),(80,1.8),(100,1.5),(120,1.8)]:
        em, es = sim(kp, kd, RC)
        st = 'OK' if em < LIMIT else ('~' if em < LIMIT*1.5 else '发散' if em>0.5 else '超标')
        print(f'{RC:6.3f} {kp:5d} {kd:4.1f} {em*1000:9.1f} {es*1000:7.1f} {st:>6}')
    print()
