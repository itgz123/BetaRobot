#!/usr/bin/env python3
"""
MIT Lite 轴参数辨识脚本 V2
===========================
核心思路: 频域惯量 + 多窗口时域精炼 + 一致性评估

V2 不是替代 V1, 而是 V1 的验证和精度增强工具.
- 用 V1 曲线拟合法求 J (频域, 抗噪, 已验证可靠)
- 用 SG 滤波 + 分箱提取粗摩擦
- 用多窗口短时仿真精炼摩擦参数 (5s 窗口避免积分漂移)
- 用窗口间标准差衡量参数可靠性
- 对比 V1 结果, 输出一致性报告

用法:
  python identify_axis_mit_lite_v2.py <csv_path> [--plot] [--save]
  python identify_axis_mit_lite_v2.py                           # 默认 measure_data/1.csv

@author TRW
@date 2026-06-19
"""

import os
import sys
import argparse
import json
import numpy as np
from scipy.optimize import minimize, curve_fit
from scipy.signal import savgol_filter, coherence
from typing import Optional

if sys.platform == "win32":
    import io
    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding="utf-8")


class AxisMitLiteIdentifierV2:
    """轴参数辨识器 V2

    流水线:
      1. load_data()             — CSV 加载, 去重, 提取有效扫频段
      2. identify_inertia()       — 频域 FRF 曲线拟合 → J (V1 式)
      3. extract_friction_sg()    — SG 滤波微分 → 粗摩擦参数
      4. refine_friction()        — 多窗口短时仿真 → 精炼摩擦参数 + 一致性
      5. verify()                 — 全量仿真 + 验证报告

    数据格式:
      VOFA 13通道 CSV: Time, CH0(ts), CH1(pos), CH2(speed), ..., CH7(torque_cmd), ...
      (列索引: ts=1, pos=2, speed=3, torque=8)
    """

    # 列索引 (13通道 VOFA CSV)
    COL_TS      = 1    # CH0: DWT μs 时间戳
    COL_POS     = 2    # CH1: 位置
    COL_SPEED   = 3    # CH2: 速度
    COL_TORQUE  = 8    # CH7: 摩擦力前馈 = 扫频力矩 (Nm)

    # 8通道格式 (预留)
    COL8_TS     = 1
    COL8_POS    = 2
    COL8_SPEED  = 3
    COL8_TORQUE = 4

    def __init__(self, csv_path: Optional[str] = None):
        self.csv_path = csv_path
        self.fmt: str = None
        self.fs: float = 1000.0
        self.dt: float = 0.001
        self.n_total: int = 0

        # 有效段数据
        self.valid: np.ndarray = None   # [n_valid, 4] = [torque, speed, pos, time]
        self.n: int = 0

        # 辨识结果
        self.params = {
            'inertia': None,            # J (kg·m²)
            'friction_viscous_pos': 0.0,   # cv+ (Nm·s/rad)
            'friction_viscous_neg': 0.0,   # cv-
            'friction_coulomb_pos': 0.0,   # Tc+ (Nm)
            'friction_coulomb_neg': 0.0,   # Tc-
        }

        # FRF 中间结果
        self.frq: np.ndarray = None
        self.mag: np.ndarray = None
        self.coh: np.ndarray = None
        self.J_curve: float = None       # 曲线拟合 J
        self.J_v1style: float = None     # V1 式高频中值 J

        # 多窗口精炼
        self.window_results: list = None   # 每窗口参数
        self.window_params_std: np.ndarray = None
        self.n_windows: int = 0

        # 最终仿真
        self.sim_pos: np.ndarray = None
        self.sim_speed: np.ndarray = None

        # 验证
        self.verification: dict = {}

    # ═════════════════════════════════════════════════════════
    #  1. 数据加载
    # ═════════════════════════════════════════════════════════

    def load_data(self, csv_path: Optional[str] = None) -> None:
        """加载 CSV, 去重, 提取有效扫频段"""
        if csv_path is None:
            csv_path = self.csv_path
        if csv_path is None:
            raise ValueError("请提供 CSV 文件路径")

        raw = np.genfromtxt(csv_path, delimiter=',', skip_header=1).astype(np.float64)
        n_cols = raw.shape[1]

        # 格式检测
        if n_cols >= 13:
            self.fmt = '13ch'
            ci_ts, ci_pos, ci_spd, ci_trq = (self.COL_TS, self.COL_POS,
                                              self.COL_SPEED, self.COL_TORQUE)
        elif n_cols >= 8:
            self.fmt = '8ch'
            ci_ts, ci_pos, ci_spd, ci_trq = (self.COL8_TS, self.COL8_POS,
                                              self.COL8_SPEED, self.COL8_TORQUE)
        else:
            raise ValueError(f"无法识别的列数: {n_cols}")

        # 去重
        ts = raw[:, ci_ts]
        uniq = np.diff(ts, prepend=-1) > 0
        data = raw[uniq]
        self.n_total = len(data)

        # 时间基
        dt_us = np.median(np.diff(data[:, ci_ts]))
        self.dt = dt_us * 1e-6 if dt_us > 0 else 0.002
        self.fs = 1.0 / self.dt

        # 有效段检测
        trq = data[:, ci_trq]
        active = np.abs(trq) > 0.001
        if not np.any(active):
            active = np.abs(data[:, ci_spd]) > 0.01
        if not np.any(active):
            active[:] = True

        idx = np.where(active)[0]
        s, e = int(idx[0]), int(idx[-1])
        self.valid = np.column_stack([
            data[s:e+1, ci_trq],       # 0: torque
            data[s:e+1, ci_spd],       # 1: speed
            data[s:e+1, ci_pos],       # 2: position
            data[s:e+1, ci_ts],        # 3: timestamp
        ])
        self.n = len(self.valid)

        print(f"\n{'='*55}")
        print(f"  数据: {os.path.basename(csv_path)}")
        print(f"{'='*55}")
        print(f"  格式:     {self.fmt}  ({n_cols} 列)")
        print(f"  采样率:   {self.fs:.0f} Hz  (dt={self.dt*1000:.2f}ms)")
        print(f"  总样本:   {self.n_total:,}")
        print(f"  有效段:   {self.n:,} 点  ({self.n/self.fs:.1f}s)  "
              f"[{s}, {e}]")

    def get_valid(self):
        return self.valid

    # ═════════════════════════════════════════════════════════
    #  2. 频域惯量辨识 (V1 式)
    # ═════════════════════════════════════════════════════════

    def identify_inertia(self) -> float:
        """频域惯量辨识

        方法 (与 V1 一致):
          1. 直接 FFT (无窗), 计算 FRF H(f) = FFT(speed) / FFT(torque)
          2. 在 0.5~20Hz 频段做加权曲线拟合 |H|=1/sqrt((Jω)²+cv²)
          3. 备选: 18~30Hz 高频中值法

        Returns:
            J_est: 惯量 (kg·m²)
        """
        torque = self.valid[:, 0].copy()
        speed = self.valid[:, 1].copy()
        n = len(torque)

        # SG 预滤波
        win = min(15, n // 20 * 2 + 1)
        if win >= 5:
            torque = savgol_filter(torque, win, 3)
            speed = savgol_filter(speed, win, 3)

        # FFT
        X = np.fft.rfft(torque, n=n)
        Y = np.fft.rfft(speed, n=n)
        freq = np.fft.rfftfreq(n, d=self.dt)
        H = np.divide(Y, X, out=np.ones_like(Y), where=np.abs(X) > 1e-10)
        mag = np.abs(H)

        # 相干 (Welch, 仅参考)
        nperseg = min(2048, max(128, n // 4))
        _, coh = coherence(torque, speed, fs=self.fs,
                           nperseg=nperseg, noverlap=nperseg // 2)
        coh_f = np.interp(freq, np.linspace(0, self.fs / 2, len(coh)), coh)

        self.frq = freq
        self.mag = mag
        self.coh = coh_f

        # ── 曲线拟合 (V1 式) ──
        mask = (freq >= 0.5) & (freq <= 20.0) & (coh_f > 0.1)
        if np.sum(mask) < 10:
            mask = (freq >= 0.5) & (freq <= 20.0)

        def model(w, J, cv):
            return 1.0 / np.sqrt(np.maximum((J * w)**2 + cv**2, 1e-20))

        J_cf = None
        try:
            popt, _ = curve_fit(
                model, 2.0 * np.pi * freq[mask], mag[mask],
                p0=[1e-3, 5e-3], bounds=(0, np.inf), maxfev=5000
            )
            J_cf = float(popt[0])
        except Exception:
            pass

        # ── 高频中值法 (V1 后备) ──
        hf = (freq > 18.0) & (freq <= 30.0)
        Jv = 1.0 / (mag[hf] * 2.0 * np.pi * freq[hf])
        Jv = Jv[(Jv > 1e-10) & np.isfinite(Jv)]
        J_hf = float(np.median(Jv)) if len(Jv) > 5 else None

        # 取最佳
        if J_cf is not None and J_hf is not None:
            J_est = J_cf  # 曲线拟合法优先
        elif J_hf is not None:
            J_est = J_hf
        elif J_cf is not None:
            J_est = J_cf
        else:
            J_est = 1e-4

        self.J_curve = J_cf
        self.J_v1style = J_hf
        self.params['inertia'] = J_est

        print(f"\n  惯量辨识 (频域 FRF):")
        print(f"  - 频段:           0.5~20.0 Hz  ({np.sum(mask)} 点)")
        print(f"  - 曲线拟合 J:     {J_cf if J_cf else '—':.3e}")
        print(f"  - 高频中值 J:     {J_hf if J_hf else '—':.3e}")
        print(f"  - 采用 J:         {J_est:.3e}")

        return J_est

    # ═════════════════════════════════════════════════════════
    #  3. SG 滤波粗摩擦提取
    # ═════════════════════════════════════════════════════════

    def extract_friction_sg(self, J: float) -> None:
        """SG 滤波微分提取粗摩擦

        T_friction = T_cmd - J * SG_derivative(speed)
        分位数分箱拟合双向摩擦参数.
        """
        speed = self.valid[:, 1].astype(np.float64)
        torque = self.valid[:, 0].astype(np.float64)
        n = len(speed)

        win = min(15, n // 20 * 2 + 1)
        accel = savgol_filter(speed, win, 3, deriv=1, delta=self.dt) if win >= 5 else np.gradient(speed, self.dt)
        T_fric = torque - J * accel

        def bin_fit(abs_spd, abs_frc):
            edges = np.percentile(abs_spd, np.linspace(0, 100, 20))
            edges = np.unique(edges)
            s_m, f_m = [], []
            for i in range(len(edges) - 1):
                m = (abs_spd >= edges[i]) & (abs_spd < edges[i+1])
                if np.sum(m) >= 5:
                    s_m.append(np.median(abs_spd[m]))
                    f_m.append(np.median(abs_frc[m]))
            if len(s_m) < 3:
                return 0.0, float(np.median(abs_frc))
            c = np.polyfit(s_m, f_m, 1)
            return max(c[0], 0), max(c[1], 0)

        pos = speed > 0.1
        neg = speed < -0.1
        cv_p, Tc_p = bin_fit(np.abs(speed[pos]), np.abs(T_fric[pos])) if np.any(pos) else (0, 0)
        cv_n, Tc_n = bin_fit(np.abs(speed[neg]), np.abs(T_fric[neg])) if np.any(neg) else (0, 0)

        self.params['friction_viscous_pos'] = cv_p
        self.params['friction_viscous_neg'] = cv_n
        self.params['friction_coulomb_pos'] = Tc_p
        self.params['friction_coulomb_neg'] = Tc_n

        print(f"\n  粗摩擦提取 (SG 滤波微分):")
        print(f"  - cv+: {cv_p:.6f}  cv-: {cv_n:.6f}")
        print(f"  - Tc+: {Tc_p:.6f}  Tc-: {Tc_n:.6f}")

    # ═════════════════════════════════════════════════════════
    #  4. 多窗口摩擦精炼
    # ═════════════════════════════════════════════════════════

    def _simulate(self, th0, om0, T, dt, J, cp, cn, Tcp, Tcn, N):
        """动力学前向仿真 (二阶积分)"""
        th = np.zeros(N, dtype=np.float64)
        om = np.zeros(N, dtype=np.float64)
        th[0], om[0] = float(th0), float(om0)
        inv_J = 1.0 / max(J, 1e-10)
        for i in range(1, N):
            if om[i-1] > 0:
                fric = cp * om[i-1] + Tcp
            elif om[i-1] < 0:
                fric = cn * om[i-1] - Tcn
            else:
                fric = 0.0
            alpha = (T[min(i-1, len(T)-1)] - fric) * inv_J
            om[i] = om[i-1] + alpha * dt
            th[i] = th[i-1] + om[i-1]*dt + 0.5*alpha*dt**2
            if not np.isfinite(om[i]): om[i] = om[i-1]
            if not np.isfinite(th[i]): th[i] = th[i-1]
        return th, om

    def refine_friction(self, J: float, window_s: float = 5.0,
                        maxiter: int = 60, verbose: bool = True) -> None:
        """多窗口短时仿真精炼摩擦参数

        将全长数据切为 5s 窗口 (50% 重叠), 固定 J, 仅优化 4 个摩擦参数.
        短窗口 = 避免积分漂移, 聚焦局部动力学.

        Args:
            J: 频域辨识的惯量
            window_s: 窗口时长 (秒)
            maxiter: 每窗口最大迭代次数
        """
        torque = self.valid[:, 0].astype(np.float64)
        speed = self.valid[:, 1].astype(np.float64)
        pos = self.valid[:, 2].astype(np.float64)
        n = len(torque)

        win_len = int(window_s / self.dt)
        step = win_len // 2
        starts = list(range(0, n - win_len, step))
        if not starts:
            starts = [0]

        # 初值 (来自 SG 粗提取)
        Tc_rough = float(np.median(np.abs(torque))) * 0.8
        x0_list = [
            [self.params['friction_viscous_pos'],
             self.params['friction_viscous_neg'],
             self.params['friction_coulomb_pos'],
             self.params['friction_coulomb_neg']],
            [0.0, 0.0, Tc_rough, Tc_rough],
            [0.003, 0.003, Tc_rough * 0.6, Tc_rough * 0.6],
        ]

        all_params = []
        n_win = len(starts)

        for wi, ws in enumerate(starts):
            we = min(ws + win_len, n)
            tm = torque[ws:we]
            sm = speed[ws:we]
            pm = pos[ws:we]

            def objective(x):
                cp = np.clip(np.abs(x[0]), 0, 1)
                cn = np.clip(np.abs(x[1]), 0, 1)
                Tp = np.clip(np.abs(x[2]), 0, 1)
                Tn = np.clip(np.abs(x[3]), 0, 1)
                _, om_p = self._simulate(pm[0], sm[0], tm, self.dt,
                                         J, cp, cn, Tp, Tn, len(tm))
                N = min(len(om_p), len(sm))
                return float(np.sqrt(np.mean((om_p[:N] - sm[:N]) ** 2)))

            best_p, best_e = None, np.inf
            for x0 in x0_list:
                try:
                    res = minimize(objective, x0, method='Nelder-Mead',
                                   options={'maxiter': maxiter,
                                            'xatol': 1e-7, 'fatol': 1e-7,
                                            'adaptive': True})
                    if res.fun < best_e:
                        best_e = float(res.fun)
                        best_p = np.abs(res.x)
                except Exception:
                    continue

            if best_p is not None:
                all_params.append(best_p)

            if verbose and (wi + 1) % max(1, n_win // 8) == 0:
                t_s = ws * self.dt
                print(f"    窗口 {wi+1}/{n_win} @ {t_s:.0f}s: "
                      f"RMSE={best_e:.1f} Tc+={best_p[2]:.4f}")

        self.n_windows = len(all_params)

        if not all_params:
            print("  ⚠ 所有窗口优化失败, 保持粗摩擦参数")
            return

        arr = np.array(all_params)
        med = np.median(arr, axis=0)
        std = np.std(arr, axis=0)
        self.window_results = arr
        self.window_params_std = std

        self.params['friction_viscous_pos'] = float(med[0])
        self.params['friction_viscous_neg'] = float(med[1])
        self.params['friction_coulomb_pos'] = float(med[2])
        self.params['friction_coulomb_neg'] = float(med[3])

        # 全量仿真 (仅报告)
        th_f, om_f = self._simulate(pos[0], speed[0], torque, self.dt,
                                     J, med[0], med[1], med[2], med[3], n)
        self.sim_pos = th_f
        self.sim_speed = om_f

        e_om = np.sqrt(np.mean((om_f - speed) ** 2))
        sr_om = np.max(speed) - np.min(speed)

        if verbose:
            print(f"\n  多窗口精炼 ({self.n_windows} × {window_s:.0f}s):")
            print(f"  - 速度 RMSE:       {e_om:.1f}  (NRMSE={e_om/max(sr_om,1e-6)*100:.1f}%)")
            print(f"  - 窗口一致性 (标准差):")
            print(f"    cv±{std[0]:.4f}/{std[1]:.4f}  "
                  f"(CV={std[0]/max(med[0],1e-10)*100:.0f}%/{std[1]/max(med[1],1e-10)*100:.0f}%)")
            print(f"    Tc±{std[2]:.4f}/{std[3]:.4f}  "
                  f"(CV={std[2]/max(med[2],1e-10)*100:.0f}%/{std[3]/max(med[3],1e-10)*100:.0f}%)")
            print(f"  - 中位数参数:")
            print(f"    cv+: {med[0]:.6f}  cv-: {med[1]:.6f}")
            print(f"    Tc+: {med[2]:.6f}  Tc-: {med[3]:.6f}")

            if any(s > m * 0.5 for m, s in zip(med[:2], std[:2]) if m > 1e-6):
                print(f"  ⚠ cv 窗口间变异大 (>50%), 建议保持 cv=0")

    # ═════════════════════════════════════════════════════════
    #  5. 验证与报告
  # ═════════════════════════════════════════════════════════

    def verify(self) -> dict:
        """运行验证并生成报告"""
        torque = self.valid[:, 0]
        speed_m = self.valid[:, 1]
        pos_m = self.valid[:, 2]

        if self.sim_speed is None:
            th_f, om_f = self._simulate(
                pos_m[0], speed_m[0], torque, self.dt,
                self.params['inertia'],
                self.params['friction_viscous_pos'],
                self.params['friction_viscous_neg'],
                self.params['friction_coulomb_pos'],
                self.params['friction_coulomb_neg'],
                len(torque)
            )
            self.sim_pos = th_f
            self.sim_speed = om_f

        N = min(len(self.sim_speed), len(speed_m))
        e_om = float(np.sqrt(np.mean((self.sim_speed[:N] - speed_m[:N]) ** 2)))
        e_th = float(np.sqrt(np.mean((self.sim_pos[:N] - pos_m[:N]) ** 2)))
        sr_om = float(np.max(speed_m) - np.min(speed_m))
        sr_th = float(np.max(pos_m) - np.min(pos_m))

        # SNR
        fft_s = np.fft.rfft(speed_m)
        psd = np.abs(fft_s) ** 2 / len(speed_m)
        cutoff = max(10, len(psd) // 10)
        signal_power = float(np.sum(psd[:cutoff]))
        noise_power = float(np.sum(psd[cutoff:])) if cutoff < len(psd) else 1e-10
        snr = 10 * np.log10(max(signal_power / max(noise_power, 1e-10), 1e-10))

        # 力矩拟合 (SG 加速度, 仅参考)
        win = min(15, N // 20 * 2 + 1)
        accel_e = savgol_filter(speed_m[:N], win, 3, deriv=1, delta=self.dt) if win >= 5 else np.gradient(speed_m[:N], self.dt)
        I_p = (speed_m[:N] > 0).astype(float)
        I_n = (speed_m[:N] < 0).astype(float)
        Tp = self.params
        T_pred = (Tp['inertia'] * accel_e
                  + Tp['friction_viscous_pos'] * speed_m[:N] * I_p
                  + Tp['friction_viscous_neg'] * speed_m[:N] * I_n
                  + Tp['friction_coulomb_pos'] * I_p
                  + Tp['friction_coulomb_neg'] * I_n)
        e_T = float(np.sqrt(np.mean((torque[:N] - T_pred) ** 2)))

        self.verification = {
            'omega_rmse': e_om,
            'omega_nrmse_pct': e_om / max(sr_om, 1e-6) * 100,
            'theta_rmse': e_th,
            'theta_nrmse_pct': e_th / max(sr_th, 1e-6) * 100,
            'torque_rmse': e_T,
            'snr_db': snr,
            'n_samples': N,
            'n_windows': self.n_windows,
        }

        return self.verification

    def print_report(self) -> None:
        """打印完整辨识报告"""
        p = self.params
        v = self.verification

        print(f"\n{'='*55}")
        print(f"  轴参数辨识报告")
        print(f"{'='*55}")

        print(f"\n  惯量 J:           {p['inertia']:.6f}  kg·m²")
        print(f"    V1 曲线拟合:     {self.J_curve:.3e}")
        print(f"    V1 高频中值:     {self.J_v1style:.3e}")
        if self.J_curve and self.J_v1style:
            ratio = max(self.J_curve, self.J_v1style) / min(self.J_curve, self.J_v1style)
            print(f"    一致性:          {'✓' if ratio < 2 else '✗'}  (差异 {ratio:.1f}x)")

        print(f"\n  摩擦参数:")
        print(f"    cv+: {p['friction_viscous_pos']:.6f}  "
              f"cv-: {p['friction_viscous_neg']:.6f}")
        print(f"    Tc+: {p['friction_coulomb_pos']:.6f}  "
              f"Tc-: {p['friction_coulomb_neg']:.6f}")

        if self.window_params_std is not None:
            std = self.window_params_std
            print(f"    (窗口 ±{std[2]:.4f}/{std[3]:.4f})")

        print(f"\n  验证指标:")
        print(f"    速度 NRMSE:      {v.get('omega_nrmse_pct', 0):.1f}%")
        print(f"    位置 NRMSE:      {v.get('theta_nrmse_pct', 0):.1f}%  (60s 参考)")
        print(f"    力矩 RMSE:       {v.get('torque_rmse', 0):.4f} Nm")
        print(f"    信噪比:          {v.get('snr_db', 0):.1f} dB")
        print(f"    数据点数:        {self.n}")
        print(f"    采样率:          {self.fs:.0f} Hz")
        print(f"    窗口数:          {v.get('n_windows', 0)}")
        print(f"{'='*55}")

    def to_dict(self) -> dict:
        """导出结果为 dict"""
        return {
            'parameters': {
                'inertia': self.params['inertia'],
                'inertia_curve_fit': self.J_curve,
                'inertia_high_freq': self.J_v1style,
                'friction_viscous_pos': self.params['friction_viscous_pos'],
                'friction_viscous_neg': self.params['friction_viscous_neg'],
                'friction_coulomb_pos': self.params['friction_coulomb_pos'],
                'friction_coulomb_neg': self.params['friction_coulomb_neg'],
            },
            'verification': self.verification,
            'data_info': {
                'sampling_rate_hz': self.fs,
                'n_samples': self.n,
            },
            'n_windows': self.n_windows,
        }

    def save_report(self, path: str) -> None:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
        print(f"\n  报告已保存: {path}")

    # ═════════════════════════════════════════════════════════
    #  6. 流水线
    # ═════════════════════════════════════════════════════════

    def identify_all(self, csv_path: Optional[str] = None,
                     window_s: float = 5.0, maxiter: int = 60,
                     verbose: bool = True) -> dict:
        """执行完整辨识流水线"""
        self.load_data(csv_path)

        J = self.identify_inertia()

        self.extract_friction_sg(J)

        self.refine_friction(J, window_s=window_s, maxiter=maxiter,
                             verbose=verbose)

        self.verify()

        if verbose:
            self.print_report()

        return self.params

    # ═════════════════════════════════════════════════════════
    #  7. 绘图 (可选)
    # ═════════════════════════════════════════════════════════

    def plot_results(self, save_path: Optional[str] = None) -> None:
        """绘制验证结果图"""
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            print("  ⚠ 需要 matplotlib 才能绘图")
            return

        time = (self.valid[:, 3] - self.valid[0, 3]) * 1e-6
        pos_m = self.valid[:, 2]
        speed_m = self.valid[:, 1]
        torque = self.valid[:, 0]

        N = min(len(self.sim_speed or []), len(speed_m))
        if self.sim_speed is None:
            print("  ⚠ 尚未仿真, 请先运行 identify_all")
            return

        fig, axes = plt.subplots(4, 1, figsize=(12, 10), sharex=True)

        ax = axes[0]
        ax.plot(time[:N], pos_m[:N], 'b-', lw=0.8, label='实测')
        ax.plot(time[:N], self.sim_pos[:N], 'r--', lw=0.8, label='仿真')
        ax.set_ylabel('位置')
        ax.legend(); ax.grid(True, alpha=0.3)

        ax = axes[1]
        ax.plot(time[:N], speed_m[:N], 'b-', lw=0.8, label='实测')
        ax.plot(time[:N], self.sim_speed[:N], 'r--', lw=0.8, label='仿真')
        ax.set_ylabel('速度')
        ax.legend(); ax.grid(True, alpha=0.3)

        ax = axes[2]
        ax.plot(time[:N], (self.sim_pos[:N] - pos_m[:N]) * 1000, 'g-', lw=0.8)
        ax.axhline(0, color='k', lw=0.5)
        ax.set_ylabel('位置误差 (mrad)')
        ax.grid(True, alpha=0.3)

        ax = axes[3]
        ax.plot(time[:N], torque[:N], 'b-', lw=0.8, label='指令')
        win = min(15, N // 20 * 2 + 1)
        if win >= 5:
            accel = savgol_filter(speed_m[:N], win, 3, deriv=1, delta=self.dt)
            I_p = (speed_m[:N] > 0).astype(float)
            I_n = (speed_m[:N] < 0).astype(float)
            T_pred = (self.params['inertia'] * accel
                      + self.params['friction_viscous_pos'] * speed_m[:N] * I_p
                      + self.params['friction_viscous_neg'] * speed_m[:N] * I_n
                      + self.params['friction_coulomb_pos'] * I_p
                      + self.params['friction_coulomb_neg'] * I_n)
            ax.plot(time[:N], T_pred, 'r--', lw=0.8, alpha=0.7, label='模型')
        ax.set_xlabel('时间 (s)')
        ax.set_ylabel('力矩 (Nm)')
        ax.legend(); ax.grid(True, alpha=0.3)

        f_pct = self.verification.get('omega_nrmse_pct', 0)
        fig.suptitle(f'轴参数辨识验证  速度 NRMSE={f_pct:.1f}%')
        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=150, bbox_inches='tight')
            print(f"  图已保存: {save_path}")
        plt.show()


# ═══════════════════════════════════════════════════════════════
#  主函数
# ═══════════════════════════════════════════════════════════════

def main() -> None:
    parser = argparse.ArgumentParser(description="MIT Lite 轴参数辨识脚本 V2")
    parser.add_argument("csv_path", nargs="?", default=None,
                        help="CSV 数据文件路径")
    parser.add_argument("--plot", action="store_true",
                        help="显示辨识结果图")
    parser.add_argument("--save", type=str, default=None,
                        help="保存报告到 JSON 文件")
    parser.add_argument("--maxiter", type=int, default=60,
                        help="每窗口最大迭代 (默认 60)")
    parser.add_argument("--window", type=float, default=5.0,
                        help="窗口时长 (秒, 默认 5.0)")
    parser.add_argument("--no-verbose", action="store_true",
                        help="静默模式")
    args = parser.parse_args()

    csv_path = args.csv_path
    if csv_path is None:
        csv_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
            "measure_data", "1.csv"
        )

    identifier = AxisMitLiteIdentifierV2(csv_path)
    try:
        identifier.identify_all(
            window_s=args.window,
            maxiter=args.maxiter,
            verbose=not args.no_verbose,
        )

        if args.save:
            identifier.save_report(args.save)

        if args.plot:
            identifier.plot_results()

    except Exception as e:
        print(f"\n  ❌ 辨识过程出错: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
