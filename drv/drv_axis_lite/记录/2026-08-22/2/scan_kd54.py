"""
验证: RC=0.002 下降低 kd 能否消除 54.5Hz 振荡。
模型含电机延迟8.6ms + 速度低通 + MIT。跑长时长(60s)看高频模态。
"""
import numpy as np

J, g, B = 0.008, 0.30, 0.035
dt = 0.001
A, FREQ = 0.2, 2.0
w = 2*np.pi*FREQ
TAU_M = 0.0086
LIMIT = 0.6*np.pi/180

def sim(kp, kd, RC, n_sec=60.0):
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
    # 最后10s
    s = slice(int(len(t)*0.8), len(t))
    err = ref[s]-th[s]
    em = np.max(np.abs(err))
    vs = np.std(v[s])
    # 高频分量: 速度FFT 20-100Hz
    X = np.fft.rfft(v[s]-np.mean(v[s])); fr = np.fft.rfftfreq(len(v[s]), dt)
    amp = 2*np.abs(X)/len(v[s])
    m = (fr>20)&(fr<100)
    v_hi = np.max(amp[m]) if np.any(m) else 0
    return em, err.std(), vs, v_hi

print(f'目标: {LIMIT*1000:.1f}mrad')
print(f'{"kp":>5} {"kd":>4} {"RC":>6} {"maxErr":>9} {"std":>7} {"v_std":>7} {"v_hi":>7}')
for kp in [80]:
    for kd in [1.2, 1.0, 0.8, 0.6, 0.4]:
        for RC in [0.002]:
            em, es, vs, vh = sim(kp, kd, RC)
            st = 'OK' if em<LIMIT else '超标'
            if em > 0.5: st='发散'
            print(f'{kp:5d} {kd:4.1f} {RC:6.3f} {em*1000:9.1f} {es*1000:7.1f} {vs:7.3f} {vh:7.4f} {st}')

# 也试 kp 降低 + RC 折中
print('\n=== kp降低 + RC折中 ===')
for kp in [40, 50, 60]:
    for kd in [0.6, 0.8, 1.0]:
        for RC in [0.004, 0.003]:
            em, es, vs, vh = sim(kp, kd, RC)
            st = 'OK' if em<LIMIT else ('~' if em<LIMIT*1.5 else '超标')
            if em>0.5: st='发散'
            print(f'{kp:5d} {kd:4.1f} {RC:6.3f} {em*1000:9.1f} {es*1000:7.1f} {vs:7.3f} {vh:7.4f} {st}')
