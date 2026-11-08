"""追踪 plot_data3 36Hz 振荡来源: 逐环节检查2Hz/18Hz/36Hz分量"""
import numpy as np, pandas as pd

df = pd.read_csv('plot_data3.csv')
c = {i: df.iloc[:, i+1].astype(float).values for i in range(16)}
names = {1:'angle',2:'speed',3:'torque',4:'ref',5:'refv',6:'refa',
         7:'gravity_ff',8:'inertia_ff',9:'friction_ff',10:'pos_out',11:'spd_out',12:'setref'}
i0, s0, s1 = 989631, 5200, 9300
s = slice(i0+s0, i0+s1)

def amp_ph(x, f, fs=1000):
    X = np.fft.rfft(x)
    fr = np.fft.rfftfreq(len(x), 1/fs)
    j = np.argmin(np.abs(fr-f))
    return 2*np.abs(X[j])/len(x), np.angle(X[j])

print(f'{"通道":>12} | {"2Hz":>10} | {"18Hz":>10} | {"36Hz":>10} | {"47Hz":>10}')
print('-'*75)
for i in range(1, 16):
    if i not in names: continue
    ch = c[i][s]
    vals = []
    for f in [2, 18, 36, 47]:
        a, p = amp_ph(ch, f)
        vals.append(f'{a:6.4f}')
    print(f'{names[i]:>12} | ' + ' | '.join(f'{v:>10}' for v in vals))

# 检查 setref 36Hz 是否与 angle/speed 36Hz 相关(相干性)
print('\n=== setref vs speed 18Hz 相位 ===')
a1,p1 = amp_ph(setref:=c[12][s], 18); a2,p2 = amp_ph(c[2][s], 18)
print(f'setref 18Hz A={a1:.4f} ph={p1:+.3f}, speed 18Hz A={a2:.4f} ph={p2:+.3f}, 差={p1-p2:+.3f}rad')
a1,p1 = amp_ph(setref, 36); a2,p2 = amp_ph(c[2][s], 36)
print(f'setref 36Hz A={a1:.4f} ph={p1:+.3f}, speed 36Hz A={a2:.4f} ph={p2:+.3f}, 差={p1-p2:+.3f}rad')
a1,p1 = amp_ph(setref, 47); a2,p2 = amp_ph(c[2][s], 47)
print(f'setref 47Hz A={a1:.4f} ph={p1:+.3f}, speed 47Hz A={a2:.4f} ph={p2:+.3f}, 差={p1-p2:+.3f}rad')

# setref 36Hz 原始波形: 是否接近电机可执行? 看 torque(实际执行)36Hz
a3,p3 = amp_ph(c[3][s], 36)
print(f'\ntorque(CH3实际) 36Hz A={a3:.4f} — setref命令36Hz vs torque实际36Hz: {a3/a1:.2f}倍')
a3,p3 = amp_ph(c[3][s], 18)
print(f'torque 18Hz A={a3:.4f}')
