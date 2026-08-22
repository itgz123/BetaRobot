"""plot_data4 深入分析: 完整setref频谱 + 时间线ref范围"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data4.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
ang=c[1]; spd=c[2]; tor=c[3]; ref=c[4]; refv=c[5]; setref=c[12]
i0,i1=989933,999999
ts=(c[0][i0:i1+1]-c[0][i0])/1e6

# 时间线: 每段 ref 范围判断正弦状态
print('=== 时间线 (ref/angle/setref) ===')
for i in range(12):
    s=slice(len(ts)*i//12,len(ts)*(i+1)//12)
    rf=ref[i0+s.start:i0+s.stop]
    print(f'  t={ts[s.start]:4.1f}-{ts[s.stop-1]:4.1f}: ref[{np.min(rf):+.3f},{np.max(rf):+.3f}] setref_std={np.std(setref[i0+s.start:i0+s.stop]):.3f}')

# 正弦活跃段
act = np.abs(ref[i0:i1+1])>0.15
idx = np.nonzero(act)[0]
t_act0, t_act1 = ts[idx[0]], ts[idx[-1]]
print(f'\nref活跃段: t={t_act0:.1f}-{t_act1:.1f}s ({t_act1-t_act0:.1f}s)')

# 用活跃段的中间60%做频谱
si0 = idx[0] + int(len(idx)*0.2); si1 = idx[0] + int(len(idx)*0.8)
s = slice(i0+si0, i0+si1)
print(f'频谱段: {si0}-{si1}, {si1-si0}点, {(c[0][i0+si1]-c[0][i0+si0])/1e6:.1f}s')

def amp_ph(x, f, fs=1000):
    X=np.fft.rfft(x); fr=np.fft.rfftfreq(len(x),1/fs)
    j=np.argmin(np.abs(fr-f)); return 2*np.abs(X[j])/len(x), np.angle(X[j])

print('\n=== 2Hz/18Hz/36Hz 分量 ===')
for name, ch in [('angle',ang),('speed',spd),('torque',tor),('setref',setref)]:
    row=[]
    for f in [2,18,36]:
        a,_=amp_ph(ch[s],f); row.append(f'{a:.4f}')
    print(f'  {name:>7}: 2Hz={row[0]} 18Hz={row[1]} 36Hz={row[2]}')

# setref 全频谱找主导
X=np.fft.rfft(setref[s]-np.mean(setref[s])); fr=np.fft.rfftfreq(len(setref[s]),0.001)
amp=2*np.abs(X)/len(setref[s])
print('\n=== setref 前10个频率峰 ===')
top=np.argsort(amp)[-10:][::-1]
for j in top: print(f'  {fr[j]:6.2f}Hz: {amp[j]:.4f}')

# err 频谱
e=ref[s]-ang[s]
ea,_=amp_ph(e,2); em=np.max(np.abs(e)); es=np.std(e)
print(f'\n正弦段误差: max={em*1000:.1f}mrad std={es*1000:.1f}mrad 2Hz成分={ea*1000:.1f}mrad (目标10.5)')

# 速度噪声
X=np.fft.rfft(spd[s]-np.mean(spd[s])); amp=2*np.abs(X)/len(spd[s])
m=(fr>100)&(fr<400)
print(f'速度噪声(100-400Hz): max={np.max(amp[m]):.4f} rad/s')

# 波形抽样: 看setref/err形态
print('\n=== 波形 (正弦段中间, 每5点) ===')
s0=si0+len(idx)//2; s1=s0+300
for k in range(s0, s1, 5):
    print(f'  {ts[k]:6.3f} ref={ref[i0+k]:+.4f} ang={ang[i0+k]:+.4f} spd={spd[i0+k]:+.3f} setref={setref[i0+k]:+.3f}')
