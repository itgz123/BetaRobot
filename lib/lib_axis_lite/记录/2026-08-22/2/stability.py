"""
闭环稳定性分析 v2: 加入粘性摩擦阻尼 B。
机械: J s² + B s
电机延迟: 1/(1+s·τm)
速度低通: 1/(1+s·RC)
控制: u = kp·(θr-θ) + kd·(θ̇r - L(s)·θ̇) + ff
特征方程 (摩擦+延迟+低通):
  s⁴(J·τm·RC) + s³(J(τm+RC) + B·τm·RC) + s²(J + B(τm+RC)) + s(B + kp·RC + kd) + kp = 0
"""
import numpy as np

J = 0.008
TAU_M = 0.014
RC = 0.02
B = 0.035  # 粘性摩擦 (标定区间 0.02-0.05)

def poles(kp, kd, B=B, RC=RC, TAU_M=TAU_M):
    a = J*TAU_M*RC
    b = J*(TAU_M+RC) + B*TAU_M*RC
    c = J + B*(TAU_M+RC)
    d = B + kp*RC + kd
    e = kp
    return np.roots([a, b, c, d, e])

def maxre(kp, kd, B=B, RC=RC, TAU_M=TAU_M):
    return np.max(poles(kp, kd, B, RC, TAU_M).real)

print(f'模型: J={J}, B={B}, τm={TAU_M}, RC={RC}')
print('验证实测: kp=20,kd=1.2 应稳定, kp=80,kd=1.8 应振荡')
for kp,kd in [(20,1.2),(80,1.8)]:
    p = poles(kp,kd)
    print(f'  kp={kp} kd={kd}: maxRe={maxre(kp,kd):+.4f} 频率=',
          f'{np.abs(p[np.argmax(p.real)].imag)/2/np.pi:.1f}Hz' if np.max(p.imag)>0 else '无复根')
    if maxre(kp,kd)>0:
        p2 = p[p.real==np.max(p.real)][0]
        print(f'     主导不稳定极点: {p2.real:+.3f}{p2.imag:+.3f}j  freq={abs(p2.imag)/2/np.pi:.1f}Hz')

print('\n=== 扫描稳定边界 (固定kd) ===')
print('kd=1.2:')
for kp in [15,20,25,30,35,40,45,50,60,70,80]:
    r = maxre(kp,1.2)
    print(f'  kp={kp:3d}: maxRe={r:+.4f} {"OK" if r<0 else "★不稳"}')
print('kd=1.8:')
for kp in [20,30,40,50,60,70,80]:
    r = maxre(kp,1.8)
    print(f'  kp={kp:3d}: maxRe={r:+.4f} {"OK" if r<0 else "★不稳"}')
