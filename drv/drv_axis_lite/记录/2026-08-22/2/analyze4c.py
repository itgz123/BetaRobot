"""plot_data4: 定位位置误差主要成分 + 54.5Hz控制环振荡来源"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data4.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
ang=c[1]; spd=c[2]; tor=c[3]; ref=c[4]; refv=c[5]; setref=c[12]
posout=c[10]; spdout=c[11]; iff=c[8]; gff=c[7]
i0,i1=989933,999999
ts=(c[0][i0:i1+1]-c[0][i0])/1e6

# 纯正弦段 (t≈5.8-7.9s, 索引5771-7880)
s = slice(i0+5771, i0+7880)
n = 7880-5771
e = ref[s]-ang[s]

def amp_ph(x, f, fs=1000):
    X=np.fft.rfft(x); fr=np.fft.rfftfreq(len(x),1/fs)
    j=np.argmin(np.abs(fr-f)); return 2*np.abs(X[j])/len(x), np.angle(X[j])

# err 全频谱
X=np.fft.rfft(e-np.mean(e)); fr=np.fft.rfftfreq(len(e),0.001)
amp=2*np.abs(X)/len(e)
print('=== 位置误差 全频谱 前8峰 ===')
top=np.argsort(amp)[-8:][::-1]
for j in top: print(f'  {fr[j]:6.2f}Hz: {amp[j]*1000:7.1f}mrad')

print('\n=== setref 各环节 54.5Hz 分量 ===')
for name, ch in [('setref',setref),('pos_out',posout),('spd_out',spdout),('inertia_ff',iff),('gravity_ff',gff),('speed',spd),('angle',ang)]:
    a,p = amp_ph(ch[s],54.5)
    print(f'  {name:>10}: A={a:7.4f} ph={p:+.2f}')
    if name=='speed':
        a2,p2 = amp_ph(ch[s],54.5)
        print(f'    → 若spd_out=kd*speed: 1.2*{a2:.4f}={1.2*a2:.4f}')

# 检查 54.5Hz 是否是速度反馈固有 (电机内部速度估计振荡?)
print('\n=== 54.5Hz 速度vs位置关系 ===')
# 如果54.5Hz是真实机械振荡: angle_A * 2π*54.5 ≈ speed_A
a_ang,_ = amp_ph(ang[s],54.5); a_spd,_ = amp_ph(spd[s],54.5)
print(f'  angle54.5={a_ang*1000:.2f}mrad, 期望速度=角度微商={a_ang*2*np.pi*54.5:.4f}, 实测速度={a_spd:.4f}')
print(f'  比值 speed/(w*angle)={a_spd/(a_ang*2*np.pi*54.5):.1f} (≈1表示机械真实振荡, >>1表示速度反馈自带噪声)')

# 看 speed 原始波形是否锯齿/量化
print('\n=== speed 原始数据 (纯正弦段抽样) ===')
sv = spd[s]
for k in range(0, len(sv), 20):
    if k>=len(sv): break
    print(f'  spd[{k}]={sv[k]:+.4f} ang[{k}]={ang[i0+5771+k]:+.4f}')
