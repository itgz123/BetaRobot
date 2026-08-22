"""
频率响应稳定性分析: 含电机延迟8.6ms + 速度低通RC。
环路增益 (力矩/误差 → 位置):
  L(jω) = [kp + jω·kd·H(jω)] · e^(-jω·τm) / (J·(jω)²)
  H(jω) = 1/(1+jω·RC) 速度低通
稳定判据: 在 |L|=1 频率处 相位裕度 PM > 0
"""
import numpy as np

J = 0.008
TAU_M = 0.0086

def gain_phase(kp, kd, RC, w):
    H = 1/(1+1j*w*RC)
    num = kp + 1j*w*kd*H
    mech = J*(1j*w)**2  # = -J w²
    L = num * np.exp(-1j*w*TAU_M) / mech
    return abs(L), np.angle(L)

def phase_margin(kp, kd, RC):
    """找 |L|=1 处的相位裕度。若|L|从未到1(增益裕度大), 找最大|L|处相位。"""
    w = np.logspace(0.5, 3, 4000)  # 0.3~1000 rad/s
    g = np.zeros_like(w); ph = np.zeros_like(w)
    for i, wi in enumerate(w):
        g[i], ph[i] = gain_phase(kp, kd, RC, wi)
    # |L|=1 穿越
    cross = np.where(np.diff(np.sign(g-1)) != 0)[0]
    if len(cross) == 0:
        return None, g.max(), w[np.argmax(g)], 1.0/g.max()
    # 最低频穿越
    i = cross[0]
    wc = w[i]
    g1, p1 = gain_phase(kp, kd, RC, wc)
    g2, p2 = gain_phase(kp, kd, RC, w[i+1])
    pm = 180 + np.degrees((p1+p2)/2)
    return wc/2/np.pi, g.max(), wc/2/np.pi, pm

print(f'模型: J={J}, 电机延迟τm={TAU_M*1000:.1f}ms')
print(f'{"RC":>6} {"kp":>4} {"kd":>4} {"|L|max":>8} {"穿越Hz":>8} {"PM°":>7} {"稳定?":>5}')
for RC in [0.02, 0.008, 0.004, 0.002]:
    for kp in [20, 40, 60, 80]:
        kd = 1.2
        res = phase_margin(kp, kd, RC)
        if res is None:
            wc, gmax, wcross, pm = None, res[1] if isinstance(res, tuple) else 0, 0, 0
            stable = 'OK' if gmax < 1 else '发散'
            print(f'{RC:6.3f} {kp:4d} {kd:4.1f} {gmax:8.3f} {"--":>8} {"--":>7} {stable:>5}')
        else:
            wc, gmax, wcross, pm = res
            stable = 'OK' if pm > 30 else ('临界' if pm > 0 else '发散')
            print(f'{RC:6.3f} {kp:4d} {kd:4.1f} {gmax:8.3f} {wc:8.2f} {pm:7.1f} {stable:>5}')
    print()
