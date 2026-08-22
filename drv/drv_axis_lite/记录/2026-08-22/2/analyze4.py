"""分析 plot_data4.csv (RC=0.002, kp=80, kd=1.2): 确认振荡消除 + 误差达标"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data4.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
ang=c[1]; spd=c[2]; tor=c[3]; ref=c[4]; setref=c[12]
N=len(ang)
nz = np.nonzero(np.abs(ang)>1e-9)[0]
i0,i1=nz[0],nz[-1]
ts=(c[0][i0:i1+1]-c[0][i0])/1e6
print(f'有效段: {i0}-{i1}, {i1-i0+1}点, {ts[-1]:.1f}s, dt={np.median(np.diff(c[0][i0:i1+1])):.0f}us')

e = ref[i0:i1+1]-ang[i0:i1+1]
print('\n=== 分段 ===')
for i in range(12):
    s=slice(len(ts)*i//12,len(ts)*(i+1)//12)
    print(f'  t={ts[s.start]:5.1f}-{ts[s.stop-1]:5.1f}: err_max={np.max(np.abs(e[s]))*1000:7.1f}mrad err_std={np.std(e[s])*1000:6.1f} setref_std={np.std(setref[i0+s.start:i0+s.stop]):.3f}')

# 找正弦段
act = np.abs(ref[i0:i1+1])>0.15
idx = np.nonzero(act)[0]
print(f'\nref活跃点: {len(idx)}')
if len(idx) > 2000:
    s0 = i0+idx[0]+len(idx)//4; s1 = i0+idx[0]+3*len(idx)//4
    s = slice(s0, s1)
    n = s1-s0
    print(f'正弦段: {s0}-{s1} ({n}点, {(c[0][s1]-c[0][s0])/1e6:.1f}s)')
    def amp_ph(x, f, fs=1000):
        X=np.fft.rfft(x); fr=np.fft.rfftfreq(len(x),1/fs)
        j=np.argmin(np.abs(fr-f)); return 2*np.abs(X[j])/len(x), np.angle(X[j])
    print('\n=== 频谱 (2Hz / 18Hz / 36Hz / 47Hz) ===')
    for name, ch in [('angle',ang),('speed',spd),('torque',tor),('setref',setref)]:
        row=[]
        for f in [2,18,36,47]:
            a,_=amp_ph(ch[s],f); row.append(f'{a:.4f}')
        print(f'  {name:>7}: 2Hz={row[0]} 18Hz={row[1]} 36Hz={row[2]} 47Hz={row[3]}')
    ea,_ = amp_ph(e[s],2)
    print(f'\n  2Hz误差幅度: {ea*1000:.1f}mrad (目标<10.5mrad)')
    em = np.max(np.abs(e[s])); es = np.std(e[s])
    print(f'  正弦段max|err|={em*1000:.1f}mrad std={es*1000:.1f}mrad')
    # 速度噪声
    X=np.fft.rfft(spd[s]-np.mean(spd[s])); fr=np.fft.rfftfreq(len(spd[s]),0.001)
    amp=2*np.abs(X)/len(spd[s])
    m=(fr>100)&(fr<400)
    print(f'  速度高频噪声(100-400Hz): max={np.max(amp[m]):.4f} rad/s')
else:
    print('正弦段不足，可能未正常跟踪')
    print('ref范围:', np.min(ref[i0:i1+1]), np.max(ref[i0:i1+1]))
