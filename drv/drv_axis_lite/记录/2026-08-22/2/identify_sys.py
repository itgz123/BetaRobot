"""
从 plot_data3.csv 识别系统真实参数:
1. 2Hz 误差幅度和相位 → 反推 2Hz 扰动力矩 D
2. setref→angle 互谱 → 闭环/开环频响, 找相位滞后根源
3. 判断 47Hz 模态来源
"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data3.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
ang=c[1]; spd=c[2]; tor=c[3]; ref=c[4]; refv=c[5]; refa=c[6]
gff=c[7]; iff=c[8]; frff=c[9]; posout=c[10]; spdout=c[11]; setref=c[12]
i0,i1=989631,999999
ts=(c[0][i0:i1+1]-c[0][i0])/1e6

# 正弦段
s = slice(5200,9300)
n = len(range(5200,9300))
fs = 1000.0

def amp_phase(x, f=2.0):
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1/fs)
    j = np.argmin(np.abs(fr-f))
    return 2*np.abs(X[j])/len(x), np.angle(X[j])

def amp(x, f=2.0):
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1/fs)
    j = np.argmin(np.abs(fr-f))
    return 2*np.abs(X[j])/len(x)

print('=== 2Hz 分量幅度/相位 (设定 amplitude=0.2) ===')
for name, ch in [('ref', ref), ('angle', ang), ('speed', spd), ('refv', refv), ('refa', refa),
                 ('setref', setref), ('gravity_ff', gff), ('inertia_ff', iff), ('friction_ff', frff),
                 ('pos_out', posout), ('spd_out', spdout), ('torque', tor)]:
    a, p = amp_phase(ch[i0+s.start:i0+s.stop])
    print(f'  {name:>10}: A={a:7.4f}  相位={p:+.3f}rad')

# 误差反推扰动 D (2Hz): e = D / (kp + kd*jw - J*w^2)
kp, kd, J = 80, 1.2, 0.008
w = 2*np.pi*2
e = ref[i0+s.start:i0+s.stop]-ang[i0+s.start:i0+s.stop]
ea, ep = amp_phase(e)
den = (kp - J*w**2) + 1j*(kd*w)
D = ea*np.exp(1j*ep) * den
print(f'\n=== 2Hz 误差和反推扰动 ===')
print(f'  误差: A={ea*1000:.1f}mrad')
print(f'  扰动 D: A={abs(D):.3f}Nm  相位={np.angle(D):+.3f}rad')

# 分解扰动来源: 惯量残差 J_res*refa + 摩擦 + 重力残差
# refa A=0.2*w^2=31.58, D_inertia = J_res*31.58 → J_res = |D|/31.58
# 假设扰动全来自惯量残差:
J_res = abs(D)/31.58
print(f'  若全为惯量残差: J_res = {J_res:.4f} (J_cfg=0.008, J_real={0.008+J_res:.4f})')

# setref→angle 相位 (评估环路总滞后)
sa, sp_ = amp_phase(setref[i0+s.start:i0+s.stop])
aa, ap_ = amp_phase(ang[i0+s.start:i0+s.stop])
print(f'\n=== 2Hz setref→angle ===')
print(f'  setref A={sa:.3f}, angle A={aa:.3f}')
print(f'  angle 落后 setref: {sp_-ap_:+.3f}rad = {(sp_-ap_)*180/np.pi:.1f}°')

# 高频模态: 看 setref 和 spd 的 47Hz 成分是否是整数倍谐波
print('\n=== setref 高频模态检查 ===')
X = np.fft.rfft(setref[i0+s.start:i0+s.stop]-np.mean(setref[i0+s.start:i0+s.stop]))
fr = np.fft.rfftfreq(len(X), 1/fs)
for f in [16, 18, 32, 36, 47, 48]:
    j = np.argmin(np.abs(fr-f))
    print(f'  {fr[j]:.1f}Hz: {2*np.abs(X[j])/len(X):.4f}')
# 检查 47 是否 = 3*15.7
print(f'  关系: 47.6/18.0 = {47.6/18.0:.2f}, 18*2.66')

# 速度反馈有效性: 实际速度2Hz vs 参考速度2Hz
print(f'\n=== 速度反馈 ===')
print(f'  CH2速度2Hz A={amp(spd[i0+s.start:i0+s.stop]):.3f}, 理想 A*w={0.2*w:.3f}')
print(f'  低通衰减比: {amp(spd[i0+s.start:i0+s.stop])/(0.2*w):.3f}')
