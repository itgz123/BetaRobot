#!/usr/bin/env python3
"""机械阻抗诊断: 各激励频率的 τ̂/v̂ 与 τ̂/θ̈̂ 复数响应"""
import sys, os
import numpy as np

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
DATA = np.load(os.path.join(BASE, "dedup.npz"))
t = DATA["t"]; pos = DATA["pos"]; spd = DATA["spd"]
trq = DATA["trq"]; gff = DATA["gff"]; multi = DATA["multi"]

fs = 1000.0
act = np.abs(multi) > 1e-6
t0 = t[act].min()
tuni = np.arange(t0, t[act].max(), 1.0/fs)
def rs(x):
    return np.interp(tuni, t[act], x[act])
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); gff_r = rs(gff); trq_r = rs(trq)
n = len(tuni); per = 2000
ncut = (n // per) * per
pos_c = pos_r[:ncut]; spd_c = spd_r[:ncut]; multi_c = multi_r[:ncut]
trq_c = trq_r[:ncut]; gff_c = gff_r[:ncut]

def coef(x):
    return np.fft.rfft(x - x.mean()) * 2.0 / ncut
F = np.fft.rfftfreq(ncut, 1.0/fs)
def at(f, X):
    return coef(X)[int(round(f/(fs/ncut)))]

freqs = np.arange(0.5, 5.01, 0.5)
w0 = 2*np.pi*freqs

print("="*72)
print("  机械阻抗 Zv = τ̂/v̂   (Re=有效粘滞, Im/ω=有效惯量)")
print("="*72)
print(f"  {'f':>5s} {'|τ̂cmd|':>8s} {'|τ̂trq|':>8s} {'Re(Zcmd)':>9s} {'Im(Zcmd)/ω':>10s} {'Re(Ztrq)':>9s} {'Im(Ztrq)/ω':>10s}")
for i, f in enumerate(freqs):
    vf = at(f, spd_c)
    tcmd = at(f, multi_c)
    ttrq = at(f, trq_c - gff_c)  # 实际净激励
    zc = tcmd/vf if abs(vf) > 1e-9 else 0j
    zt = ttrq/vf if abs(vf) > 1e-9 else 0j
    print(f"  {f:5.1f} {abs(tcmd):8.4f} {abs(ttrq):8.4f} "
          f"{zc.real:9.5f} {zc.imag/w0[i]:10.5f} "
          f"{zt.real:9.5f} {zt.imag/w0[i]:10.5f}")

print("\n" + "="*72)
print("  相位: τ̂cmd vs v̂   (延迟/动态诊断)")
print("="*72)
print(f"  {'f':>5s} {'ang(τ/v)°':>10s} {'等效延迟ms':>10s}  {'|τ/v|':>8s}")
for i, f in enumerate(freqs):
    vf = at(f, spd_c); tcmd = at(f, multi_c)
    ang = np.angle(tcmd * np.conj(vf))
    print(f"  {f:5.1f} {ang*180/np.pi:10.2f} {-ang/w0[i]*1000:10.2f}  {abs(tcmd)/max(abs(vf),1e-9):8.4f}")

print("\n" + "="*72)
print("  命令 vs 实际力矩 (电机响应)")
print("="*72)
print(f"  {'f':>5s} {'|trq̂|':>8s} {'|multî|':>8s} {'ratio':>7s} {'ang(trq/multi)°':>14s}")
for f in freqs:
    tm = at(f, multi_c); tt = at(f, trq_c - gff_c)
    ang = np.angle(tt * np.conj(tm))
    print(f"  {f:5.1f} {abs(tt):8.4f} {abs(tm):8.4f} {abs(tt)/max(abs(tm),1e-9):7.4f} {ang*180/np.pi:14.2f}")

print("\n" + "="*72)
print("  位置低频分量的重力耦合: 检查 cos(pos) 频谱")
print("="*72)
cosT = np.cos(pos_c)
# 各激励频率 cos 系数
for f in freqs:
    print(f"  f={f:.1f}Hz: |coŝ|={abs(at(f, cosT)):.4f}")
# 直流分量
print(f"  直流: mean(cos)={cosT.mean():.4f}")
print(f"  pos 直流: {pos_c.mean():.4f}")
