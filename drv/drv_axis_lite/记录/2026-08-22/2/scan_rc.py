"""
精细扫描: 固定真实电机延迟 τm=3ms, 扫 RC 和 kp/kd。
验证: RC=0.01, kp=80 稳定且误差<10.5mrad。检查振荡频率。
"""
import numpy as np

J, g, B = 0.008, 0.30, 0.035
dt, TAU_M = 0.001, 0.003
A, FREQ = 0.2, 2.0
w = 2*np.pi*FREQ
LIMIT = 0.6*np.pi/180

def sim(kp, kd, RC, n_sec=15.0):
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
    # 振荡检测: 最后段速度幅度
    vend = v[s]
    return np.max(np.abs(err)), err.std(), np.std(vend), th, v, t

print(f'目标误差: {LIMIT*1000:.1f} mrad')
print(f'{"RC":>5} {"kp":>5} {"kd":>4} {"maxErr":>7} {"std":>7} {"v_std":>7} {"状态":>6}')
best = []
for RC in [0.02, 0.015, 0.01, 0.008, 0.005]:
    for kp in [20, 40, 60, 80, 100]:
        for kd in [1.2, 1.8, 2.2, 3.0]:
            em, es, vs, th, v, t = sim(kp, kd, RC)
            if em < 0.5:  # 不发散
                ok = 'OK ' if em < LIMIT else '达标'
                st = f'{ok}'
                if em < LIMIT: best.append((em, RC, kp, kd))
            else:
                st = '发散'
            print(f'{RC:5.2f} {kp:5d} {kd:4.1f} {em*1000:7.1f} {es*1000:7.1f} {vs:7.3f} {st:>6}')
    print()

if best:
    best.sort()
    print('\n=== 达标且最稳的组合 ===')
    for em, RC, kp, kd in best[:5]:
        print(f'  RC={RC} kp={kp} kd={kd}: maxErr={em*1000:.1f}mrad')

# 验证当前代码 RC=0.02 的振荡频率
em, es, vs, th, v, t = sim(80, 1.8, 0.02, n_sec=8)
print('\n当前RC=0.02, kp=80 振荡频率:')
fft = np.abs(np.fft.rfft(v - np.mean(v))); fr = np.fft.rfftfreq(len(v), dt)
top = np.argsort(fft)[-5:][::-1]
for j in top: print(f'  {fr[j]:.1f}Hz 幅度{fft[j]/len(v)*2:.3f}')
# 验证 RC=0.01, kp=80 频谱
em, es, vs, th, v, t = sim(80, 1.8, 0.01, n_sec=8)
print('\nRC=0.01, kp=80 频谱:')
fft = np.abs(np.fft.rfft(v - np.mean(v))); fr = np.fft.rfftfreq(len(v), dt)
top = np.argsort(fft)[-5:][::-1]
for j in top: print(f'  {fr[j]:.1f}Hz 幅度{fft[j]/len(v)*2:.3f}')
