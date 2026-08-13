#!/usr/bin/env python3
"""交叉验证: 前8周期辨识, 后8周期动力学仿真验证"""
import sys, os
import numpy as np
from scipy.signal import savgol_filter
from scipy.optimize import minimize

if sys.platform == "win32":
    sys.stdout.reconfigure(encoding="utf-8")

BASE = os.path.dirname(os.path.abspath(__file__))
DATA = np.load(os.path.join(BASE, "dedup.npz"))
t = DATA["t"]; pos = DATA["pos"]; spd = DATA["spd"]
trq = DATA["trq"]; multi = DATA["multi"]

fs = 1000.0
act = np.abs(multi) > 1e-6
t0 = t[act].min()
tuni = np.arange(t0, t[act].max(), 1.0/fs)
def rs(x):
    return np.interp(tuni, t[act], x[act])
pos_r = rs(pos); spd_r = rs(spd); multi_r = rs(multi); trq_r = rs(trq)
n = len(tuni); per = 2000

pos_lim = 0.55
free = (np.abs(pos_r) < pos_lim) & (tuni > tuni[0]+0.3) & (tuni < tuni[-1]-0.3)
win = 21
acc_r = savgol_filter(spd_r, win, 3, deriv=1, delta=1/fs)

# 前8周期 / 后8周期 (激励从 t0 开始, 16 周期 = 32s)
t_train_end = t0 + 16.0   # 8 周期
t_eval_start = t0 + 16.0
train = free & (tuni < t_train_end)
eval_ = free & (tuni >= t_eval_start)
print(f"训练段: {train.sum()} 样本 ({train.sum()/fs:.1f}s)")
print(f"验证段: {eval_.sum()} 样本 ({eval_.sum()/fs:.1f}s)")

def optimize(Jfix, mm):
    p = pos_r[mm]; v = spd_r[mm]; a = acc_r[mm]; y = trq_r[mm]
    def resid(params):
        cvp, cvn, Tc, G, th0 = params
        fric = np.where(v > 0, cvp*v, cvn*v) + Tc*np.where(v > 0, 1.0, np.where(v < 0, -1.0, 0.0))
        return y - (Jfix*a + fric + G*np.cos(th0 + p))
    def obj(params):
        return float(np.mean(resid(params)**2))
    bounds = [(0, 0.3), (0, 0.3), (0, 0.5), (0, 1.0), (-np.pi, np.pi)]
    res = minimize(obj, [0.05, 0.05, 0.05, 0.28, 0.0], method='L-BFGS-B',
                   bounds=bounds, options={'maxiter': 3000})
    return res.x, np.sqrt(res.fun)

Jfix = 0.008
pars_tr, rmse_tr = optimize(Jfix, train)
print(f"\n训练段辨识 (J=0.008): cv+={pars_tr[0]:.4f} cv-={pars_tr[1]:.4f} "
      f"Tc={pars_tr[2]:.4f} G={pars_tr[3]:.4f} θ0={pars_tr[4]:+.3f}rad ({np.degrees(pars_tr[4]):+.1f}°)")
print(f"训练段力矩RMSE = {rmse_tr:.4f} Nm")

def simulate(tau_in, pos0, spd0, J, cvp, cvn, Tc, G, th0, n_steps):
    pp = np.zeros(n_steps); vv = np.zeros(n_steps)
    pp[0], vv[0] = pos0, spd0
    invJ = 1.0/J
    for i in range(1, n_steps):
        vcur = vv[i-1]
        if vcur > 0: fric = cvp*vcur + Tc
        elif vcur < 0: fric = cvn*vcur - Tc
        else: fric = 0.0
        grav = G*np.cos(th0 + pp[i-1])
        a_ = (tau_in[i] - grav - fric)*invJ
        vv[i] = vcur + a_/fs
        pp[i] = pp[i-1] + vcur/fs
    return pp, vv

# 验证: 后8周期内取自由连续段
seg_starts = []
i = int((t_eval_start - tuni[0]) * fs)
while i < n - 1200:
    if eval_[i] and eval_[i+1200]:
        seg_starts.append(i); i += 1500
    else:
        i += 1
print(f"\n验证段自由连续段: {len(seg_starts)} 个")

def eval_params(name, pars):
    Jx, cvpx, cvnx, Tcx, Gx, th0x = pars
    v_list = []; p_list = []
    for s0 in seg_starts[:15]:
        s1 = s0 + 800
        if s1 > n: break
        ps, vs = simulate(trq_r[s0:s1], pos_r[s0], spd_r[s0],
                          Jx, cvpx, cvnx, Tcx, Gx, th0x, 800)
        v_list.append(np.sqrt(np.mean((vs[40:-40]-spd_r[s0+40:s1-40])**2)))
        p_list.append(np.sqrt(np.mean((ps[40:-40]-pos_r[s0+40:s1-40])**2)))
    mv = np.mean(v_list); mp = np.mean(p_list)
    print(f"  {name:28s}  速度RMSE={mv:.3f} ({mv/11.5*100:.1f}%)  位置RMSE={mp:.4f}")
    return mv

# 候选
cvp, cvn, Tc, G, th0 = pars_tr
print("\n验证段动力学仿真 (实际力矩输入):")
eval_params("训练辨识(全参数)", (Jfix, cvp, cvn, Tc, G, th0))
eval_params("训练辨识+配置重力(θ0=0)", (Jfix, cvp, cvn, Tc, 0.28, 0.0))
eval_params("训练辨识摩擦+配置重力G", (Jfix, cvp, cvn, Tc, 0.28, 0.0))
eval_params("配置参数", (0.008, 0.05, 0.06, 0.02, 0.28, 0.0))
eval_params("配置+辨识重力", (0.008, 0.05, 0.06, 0.028, 0.284, th0))
