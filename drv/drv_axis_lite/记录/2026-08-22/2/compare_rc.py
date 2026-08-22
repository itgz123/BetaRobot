"""对比 plot_data2(rc=0.02,kd=1.8) 和 plot_data3(rc=0.008,kd=1.2) 的速度反馈"""
import numpy as np, pandas as pd

def amp_ph(x, f, fs=1000):
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1/fs)
    j = np.argmin(np.abs(fr-f))
    return 2*np.abs(X[j])/len(x), np.angle(X[j])

cases = [
    ('plot_data2.csv', 987603, 4600, 7800),
    ('plot_data3.csv', 989631, 5200, 9300),
]
w = 2*np.pi*2
for fn, i0, s0, s1 in cases:
    df = pd.read_csv(fn)
    c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
    ref=c[4]; spd=c[2]; ang=c[1]; refv=c[5]
    s = slice(i0+s0, i0+s1)
    A_ref,_ = amp_ph(ref[s],2); A_ang,_=amp_ph(ang[s],2)
    A_spd,_=amp_ph(spd[s],2); A_refv,_=amp_ph(refv[s],2)
    ideal_v = A_ref*w
    print(f'{fn}:')
    print(f'  ref={A_ref:.4f} angle={A_ang:.4f} 位置比={A_ang/A_ref:.3f}')
    print(f'  速度实测={A_spd:.3f} 理想={ideal_v:.3f} 速度总衰减={A_spd/ideal_v:.3f}')
    print(f'  参考速度={A_refv:.3f} 参考速度衰减={A_refv/ideal_v:.3f}')
    r = A_spd/ideal_v
    if r < 1:
        RC = np.sqrt(1/r**2-1)/w
        print(f'  速度衰减等效RC={RC:.4f}s 截止{1/(2*np.pi*RC):.1f}Hz')
    print()
