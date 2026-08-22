"""
扫描电机真实延迟 τm 和速度低通 RC，用数值仿真匹配实测:
- kp=20, kd=1.2 稳定 (实测 plot_data.csv)
- kp=80, kd=1.8 振荡 (实测 plot_data2.csv, 16.6Hz + 44Hz)
闭环仿真含: 机械 J+B粘性摩擦 + 重力前馈 + 电机一阶延迟 + 速度低通 + MIT反馈。
"""
import numpy as np

J = 0.008
g = 0.30
B = 0.035
dt = 0.001
A, FREQ = 0.2, 2.0
w = 2*np.pi*FREQ

def sim(kp, kd, TAU_M, RC, n_sec=12.0):
    t = np.arange(0, n_sec, dt)
    ref = A*np.sin(w*t)
    refv = A*w*np.cos(w*t)
    refa = -A*w*w*np.sin(w*t)
    th = np.zeros_like(t); v = np.zeros_like(t)
    tau = np.zeros_like(t); vfb = np.zeros_like(t)
    for k in range(len(t)-1):
        vfb[k+1] = vfb[k] + (v[k]-vfb[k])*dt/RC
        ff = g*np.cos(th[k]) + J*refa[k]
        fb = kp*(ref[k]-th[k]) + kd*(refv[k]-vfb[k+1])
        setref = fb + ff
        tau[k+1] = tau[k] + (setref - tau[k])*dt/TAU_M
        alpha = (tau[k+1] - g*np.cos(th[k]) - B*v[k]) / J
        v[k+1] = v[k] + alpha*dt
        th[k+1] = th[k] + v[k+1]*dt
    # 稳态段
    s = slice(int(len(t)*0.5), len(t))
    err = ref[s] - th[s]
    return np.max(np.abs(err)), err.std(), th, v, t

print(f'{"τm(ms)":>7} {"RC":>5} {"kp":>5} {"kd":>4} {"maxErr":>8} {"std":>7} {"发散?":>5}')
for TAU_M in [0.003, 0.005, 0.008, 0.010, 0.014]:
    for RC in [0.02, 0.01]:
        for kp, kd in [(20, 1.2), (80, 1.8)]:
            em, es, th, v, t = sim(kp, kd, TAU_M, RC)
            div = '发散' if em > 0.5 else '稳定'
            print(f'{TAU_M*1000:7.1f} {RC:5.2f} {kp:5d} {kd:4.1f} {em*1000:8.1f} {es*1000:7.1f} {div:>5}')
    print()
