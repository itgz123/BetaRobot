"""
分析 plot_data2.csv (kp=80, kd=1.8) 振荡特征。
通道映射: CH0=us时间戳(可靠), Time列不可靠。
CH1=angle CH2=speed CH3=torque CH4=ref_pos CH5=ref_vel CH6=ref_acc
CH7=gravity_ff CH8=inertia_ff CH9=friction_ff CH10=MIT_pos CH11=MIT_speed CH12=setref
"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data2.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
t_us = c[0]
ang = c[1]; spd = c[2]; tor = c[3]; ref = c[4]; refv = c[5]
setref = c[12]

# 定位有效段
nz = np.nonzero(np.abs(ang) > 1e-9)[0]
i0, i1 = nz[0], nz[-1]
print(f'有效段行: {i0}-{i1}, 点数 {i1-i0+1}')

# 有效段内时间戳检查
ts = t_us[i0:i1+1]
dt = np.diff(ts)
dtv = dt[dt > 0]
print(f'有效段dt: 中位={np.median(dtv):.1f}us 均值={np.mean(dtv):.1f}us 重复数={np.sum(dt<=0)}')
t0 = ts[0]
t_rel = (ts - t0)/1e6   # 相对秒
span = t_rel[-1]
print(f'有效段跨度: {span:.2f}s, 估算采样率≈{len(ts)/span:.0f}Hz')

# 按时间切 8 段看振荡幅度变化
print('\n=== 误差 (ref_pos - angle) 分段 (时间从左到右) ===')
e = ref[i0:i1+1] - ang[i0:i1+1]
for i in range(8):
    s = slice(len(ts)*i//8, len(ts)*(i+1)//8)
    print(f'  t={t_rel[s.start]:5.1f}s-{t_rel[s.stop-1]:5.1f}s: max|err|={np.max(np.abs(e[s]))*1000:7.1f}mrad  std={np.std(e[s])*1000:6.1f}mrad  max|setref|={np.max(np.abs(setref[i0+s.start:i0+s.stop])):.3f}')

# 频谱 (用中位数dt近似均匀采样)
dtm = np.median(dtv)/1e6
fs = 1.0/dtm
print(f'\n采样率={fs:.0f}Hz')
fft = np.abs(np.fft.rfft(e - np.mean(e)))
freqs = np.fft.rfftfreq(len(e), dtm)
top = np.argsort(fft)[-8:][::-1]
print('=== 误差主导频率成分 ===')
for j in top:
    print(f'  {freqs[j]:7.2f} Hz: 幅度≈{2*fft[j]/len(e)*1000:7.1f} mrad')

# 速度/setref 特征
print(f'\nsetref: min={np.min(setref[i0:i1+1]):.3f} max={np.max(setref[i0:i1+1]):.3f} std={np.std(setref[i0:i1+1]):.3f}')
print(f'speed: min={np.min(spd[i0:i1+1]):.3f} max={np.max(spd[i0:i1+1]):.3f} std={np.std(spd[i0:i1+1]):.3f}')
print(f'torque: min={np.min(tor[i0:i1+1]):.3f} max={np.max(tor[i0:i1+1]):.3f} std={np.std(tor[i0:i1+1]):.3f}')
