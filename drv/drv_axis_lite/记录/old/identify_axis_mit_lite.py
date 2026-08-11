#!/usr/bin/env python3
"""
MIT Lite 轴参数辨识脚本 (V3 综合版)
====================================

核心方法（实验验证最优组合）：
  1. 频域惯量辨识     — V1 curve_fit 法 (0.5~20Hz, 已验证最可靠)
  2. SG 粗摩擦提取     — V2 分位数分箱法
  3. 多窗口摩擦精炼     — 固定 J, 仅优化 4 摩擦参数 (避免 5 参数模糊)
  4. 全长仿真验证      — NRMSE + SNR + 窗口一致性评估

核心洞见（来自大量实验）：
  - J 必须来自 V1 curve_fit (3组数据给出 J≈3.3e-6，一致性稳定)
  - 5 参数同时优化 (J+cv+Tc) 导致 J 被摩擦污染，高估 4 倍
  - 固定 J 后 4 参数优化收敛性大幅提升
  - 5s 短窗口 (50%重叠) 避免 60s 积分漂移
  - 速度 RMSE 为目标（位置仅参考），避免积分漂移污染

用法:
  python identify_axis_mit_lite.py <csv_path> [--save] [--method lstsq]
  python identify_axis_mit_lite.py                           # 默认 1.csv

@author TRW
@date 2026-06-19
"""

import os
import sys
import argparse
import json
import warnings
import numpy as np
import pandas as pd
from scipy.optimize import minimize, curve_fit
from scipy.signal import savgol_filter, coherence, hilbert
from typing import Optional

if sys.platform == "win32":
    import io
    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding="utf-8")


class AxisMitLiteIdentifier:
    """MIT Lite 轴参数辨识器 (V3 综合版)

    流水线:
      1. load_data()              — pandas 加载 + 去重 + 有效段
      2. estimate_chirp_params()  — Hilbert 瞬时频率 (V1)
      3. identify_inertia()       — FRF curve_fit → J (V1, 最可靠)
      4. extract_friction_sg()    — SG 微分 → 粗摩擦 (V2)
      5. refine_friction()        — 多窗口仿真精炼 (固定 J, 4 参数)
      6. verify()                 — 全长仿真验证 + SNR
      7. print_report()           — 完整报告
    """

    # ── 列索引 (VOFA CSV) ────────────────────────────
    # 新版通道映射 (2026-06-19):
    #   CH0(1)=ts, CH1(2)=pos, CH2(3)=speed, CH3(4)=current,
    #   CH4(5)=ref_pos, CH5(6)=ref_speed, CH6(7)=ref_acc,
    #   CH7(8)=gravity_ff, CH8(9)=inertia_ff, CH9(10)=friction_ff(chirp)
    # 旧版通道映射 (之前):
    #   CH0(1)=ts, CH1(2)=pos, CH2(3)=speed, CH7(8)=friction_ff(chirp)
    # 自动检测: 新版 CH9(col=10) 有非零力矩 → 新版; 否则旧版 CH7(col=8)
    COL_TS      = 1   # CH0: DWT μs 时间戳
    COL_POS     = 2   # CH1: 反馈位置 (rad)
    COL_SPEED   = 3   # CH2: 反馈速度 (rad/s)
    COL_TORQUE_NEW = 10  # CH9: 摩擦前馈 (新版)
    COL_TORQUE_OLD = 8   # CH7: 摩擦前馈 (旧版)

    # 8 通道格式 (后备)
    COL8_TS     = 1
    COL8_POS    = 2
    COL8_SPEED  = 3
    COL8_TORQUE = 4

    def __init__(self, csv_path: Optional[str] = None):
        self.csv_path = csv_path
        self.fmt: Optional[str] = None       # '13ch' | '8ch'
        self.data: Optional[np.ndarray] = None
        self.fs: float = 1000.0
        self.dt: float = 0.001
        self.n_total: int = 0

        # ── 有效扫频段 ──
        self.valid: Optional[np.ndarray] = None   # [n, 4] = [torque, speed, pos, ts]
        self.n: int = 0

        # ── 辨识结果 ──
        self.params: dict = {
            'inertia':                None,
            'friction_viscous_pos':   0.0,
            'friction_viscous_neg':   0.0,
            'friction_coulomb_pos':   0.0,
            'friction_coulomb_neg':   0.0,
        }

        # ── 频域中间结果 ──
        self.frq: Optional[np.ndarray] = None
        self.mag: Optional[np.ndarray] = None
        self.coh: Optional[np.ndarray] = None
        self.chirp_params: dict = {
            'start_freq': None, 'end_freq': None,
            'amplitude': None, 'duration': None,
        }
        self.J_curve: Optional[float] = None    # V1 曲线拟合 J
        self.J_v1style: Optional[float] = None  # V1 高频中值 J (参考)

        # ── 多窗口结果 ──
        self.window_results: Optional[np.ndarray] = None
        self.window_params_std: Optional[np.ndarray] = None
        self.n_windows: int = 0

        # ── 仿真验证 ──
        self.sim_pos: Optional[np.ndarray] = None
        self.sim_speed: Optional[np.ndarray] = None
        self.verification: dict = {}

    # ══════════════════════════════════════════════════════════
    #  1. 数据加载
    # ══════════════════════════════════════════════════════════

    def load_data(self, csv_path: Optional[str] = None) -> None:
        """加载 CSV, 去重, 提取有效扫频段 (pandas)

        自动检测力矩列:
          - 新版 (CH9=col10): friction_ff 在 CH9, 有非零值
          - 旧版 (CH7=col8):  friction_ff 在 CH7, 回退
        """
        if csv_path is None:
            csv_path = self.csv_path
        if csv_path is None:
            raise ValueError("请提供 CSV 文件路径")

        raw = pd.read_csv(csv_path, header=0, comment=None).values.astype(np.float64)
        n_cols = raw.shape[1]

        # ── 格式检测 + 力矩列自动选择 ──
        if n_cols >= 13:
            ci_ts  = self.COL_TS
            ci_pos = self.COL_POS
            ci_spd = self.COL_SPEED

            # 检查新版 CH9 有无非零力矩
            test_new = raw[:, self.COL_TORQUE_NEW]
            nz_new = int(np.sum(np.abs(test_new) > 1e-6))
            # 检查旧版 CH7 有无非零力矩
            test_old = raw[:, self.COL_TORQUE_OLD]
            nz_old = int(np.sum(np.abs(test_old) > 1e-6))

            if nz_new > nz_old and nz_new > 100:
                ci_trq = self.COL_TORQUE_NEW
                self.fmt = '13ch (CH9=torque 新版)'
            else:
                ci_trq = self.COL_TORQUE_OLD
                self.fmt = '13ch (CH7=torque 旧版)'
        elif n_cols >= 8:
            self.fmt = '8ch'
            ci_ts, ci_pos, ci_spd, ci_trq = (
                self.COL8_TS, self.COL8_POS, self.COL8_SPEED, self.COL8_TORQUE)
        else:
            raise ValueError(f"无法识别的列数: {n_cols}")

        # ── 去重 (DWT 时间戳) ──
        ts = raw[:, ci_ts]
        uniq = np.concatenate(([True], np.diff(ts) > 0))
        data = raw[uniq]
        self.n_total = len(data)
        self.data = data

        # ── 时间基 ──
        dt_us = float(np.median(np.diff(data[:, ci_ts])))
        if dt_us > 0:
            self.dt = dt_us * 1e-6    # μs → s
            self.fs = 1.0 / self.dt
        else:
            self.dt = 0.002
            self.fs = 500.0

        # ── 有效段检测 (力矩非零) ──
        trq = data[:, ci_trq]
        active = np.abs(trq) > 1e-6
        if not np.any(active):
            active = np.abs(data[:, ci_spd]) > 0.01
        if not np.any(active):
            active[:] = True

        idx = np.where(active)[0]
        start, end = int(idx[0]), int(idx[-1])

        self.valid = np.column_stack([
            data[start:end + 1, ci_trq],    # 0: torque
            data[start:end + 1, ci_spd],    # 1: speed
            data[start:end + 1, ci_pos],    # 2: position
            data[start:end + 1, ci_ts],     # 3: timestamp
        ])
        self.n = len(self.valid)

        duration_s = (self.valid[-1, 3] - self.valid[0, 3]) * 1e-6

        print(f"\n{'=' * 55}")
        print(f"  数据: {os.path.basename(csv_path)}")
        print(f"{'=' * 55}")
        print(f"  格式:     {self.fmt}  ({n_cols} 列)")
        print(f"  采样率:   {self.fs:.0f} Hz  (dt={self.dt * 1000:.2f} ms)")
        print(f"  总样本:   {self.n_total:,}")
        print(f"  去重后:   {len(data):,}")
        print(f"  有效段:   [{start}, {end}]  {self.n:,} 点 ({duration_s:.1f} s)")

    # ══════════════════════════════════════════════════════════
    #  2. 扫频参数估计 (Hilbert 瞬时频率)
    # ══════════════════════════════════════════════════════════

    def estimate_chirp_params(self) -> dict:
        """Hilbert 变换估计扫频参数"""
        if self.valid is None:
            raise RuntimeError("请先 load_data()")

        torque = self.valid[:, 0]
        ts_us = self.valid[:, 3]
        time = (ts_us - ts_us[0]) * 1e-6

        self.chirp_params['duration'] = float(time[-1])

        # 振幅: 力矩 P95
        self.chirp_params['amplitude'] = float(np.percentile(np.abs(torque), 95))

        # 瞬时频率: Hilbert 相位差分
        try:
            analytic = hilbert(torque)
            inst_phase = np.unwrap(np.angle(analytic))
            inst_freq = np.diff(inst_phase) / (2.0 * np.pi * np.diff(time + 1e-10))

            valid_f = (inst_freq > 0) & (inst_freq < 1000)
            if np.any(valid_f):
                freqs = inst_freq[valid_f]
                n_pct = max(int(len(freqs) * 0.05), 1)
                self.chirp_params['start_freq'] = float(np.median(freqs[:n_pct]))
                self.chirp_params['end_freq'] = float(np.median(freqs[-n_pct:]))
        except Exception:
            freq = np.fft.rfftfreq(len(torque), d=self.dt)
            spec = np.abs(np.fft.rfft(torque))
            mask = (freq > 0.1) & (freq < 100)
            if np.any(mask):
                peak_idx = np.argmax(spec[mask])
                pk = freq[mask][peak_idx]
                self.chirp_params['start_freq'] = pk * 0.3
                self.chirp_params['end_freq'] = pk * 1.5

        print(f"\n  扫频参数估计 (Hilbert):")
        print(f"  - 起始频率: {self.chirp_params['start_freq']:.2f} Hz")
        print(f"  - 结束频率: {self.chirp_params['end_freq']:.2f} Hz")
        print(f"  - 振幅:     {self.chirp_params['amplitude']:.4f} Nm")
        print(f"  - 时长:     {self.chirp_params['duration']:.1f} s")

        return self.chirp_params

    # ══════════════════════════════════════════════════════════
    #  3. 频域惯量辨识 (V1 curve_fit — 最可靠方法)
    # ══════════════════════════════════════════════════════════

    def identify_inertia(self) -> float:
        """频域惯量辨识

        方法 (V1 式, 实验验证最可靠):
          1. 直接 FFT (无窗), FRF H(f) = FFT(speed) / FFT(torque)
          2. 0.5~20Hz 加权 curve_fit |H|=1/sqrt((Jω)²+cv²)
          3. 备选: 18~30Hz 高频中值法 (参考)

        Returns:
            J_est: 惯量 (kg·m²)
        """
        torque = self.valid[:, 0].copy()
        speed  = self.valid[:, 1].copy()
        n = len(torque)

        # SG 预滤波
        win = min(15, n // 20 * 2 + 1)
        if win >= 5:
            torque = savgol_filter(torque, win, 3)
            speed  = savgol_filter(speed,  win, 3)

        # FFT
        X = np.fft.rfft(torque, n=n)
        Y = np.fft.rfft(speed,  n=n)
        freq = np.fft.rfftfreq(n, d=self.dt)
        H = np.divide(Y, X, out=np.ones_like(Y), where=np.abs(X) > 1e-10)
        mag = np.abs(H)

        # 相干 (Welch, 参考权重)
        nperseg = min(2048, max(256, n // 4))
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            _, coh = coherence(torque, speed, fs=self.fs,
                               nperseg=nperseg, noverlap=nperseg // 2)
        coh_f = np.interp(freq, np.linspace(0, self.fs / 2, len(coh)), coh)

        self.frq = freq
        self.mag = mag
        self.coh = coh_f

        # ── 自适应频率段 (基于扫频参数估计) ──
        cp_start = self.chirp_params.get('start_freq', 0.5) or 0.5
        cp_end   = self.chirp_params.get('end_freq', 20.0) or 20.0
        fit_low  = max(cp_start * 0.5, 0.3)
        fit_high = min(cp_end * 0.8, self.fs / 4.0)
        hf_low   = cp_end * 0.5
        hf_high  = min(cp_end * 1.2, self.fs / 4.0)

        # ── 曲线拟合 |H(ω)| = 1 / sqrt((Jω)² + cv²) (V1 式, 相干加权) ──
        mask = (freq >= fit_low) & (freq <= fit_high) & (coh_f > 0.1) & np.isfinite(mag)
        if np.sum(mask) < 10:
            mask = (freq >= fit_low) & (freq <= fit_high) & np.isfinite(mag)

        def model(w: np.ndarray, J: float, cv: float) -> np.ndarray:
            return 1.0 / np.sqrt(np.maximum((J * w) ** 2 + cv ** 2, 1e-20))

        J_curve: Optional[float] = None
        try:
            # 相干加权: sigma = 1/coh → 高相干频点权重高
            sigma_w = 1.0 / np.maximum(coh_f[mask], 0.05)
            popt, _ = curve_fit(
                model, 2.0 * np.pi * freq[mask], mag[mask],
                p0=[1e-3, 5e-3], bounds=(0, np.inf),
                sigma=sigma_w, absolute_sigma=False,
                max_nfev=10000,
            )
            J_curve = float(popt[0])
        except Exception:
            pass

        # ── 自适应高频中值法 (参考, 不用于最终结果) ──
        hf = (freq > hf_low) & (freq <= hf_high) & np.isfinite(mag)
        Jv = 1.0 / (mag[hf] * 2.0 * np.pi * freq[hf])
        Jv = Jv[(Jv > 1e-10) & np.isfinite(Jv)]
        J_hf = float(np.median(Jv)) if len(Jv) > 5 else None

        # 取 curve_fit (已验证最可靠)
        J_est = J_curve if (J_curve is not None and J_curve > 1e-10) else (J_hf or 1e-4)

        self.J_curve = J_curve
        self.J_v1style = J_hf
        self.params['inertia'] = J_est

        print(f"\n  惯量辨识 (频域 FRF, 相干加权):")
        print(f"  - 模型:     |H| = 1/√((Jω)² + cv²)")
        print(f"  - 频段:     {fit_low:.1f}~{fit_high:.1f} Hz  ({np.sum(mask)} 点, "
              f"自适应)")
        print(f"  - 高频段:   {hf_low:.1f}~{hf_high:.1f} Hz  "
              f"({np.sum(hf)} 点)")
        print(f"  - J curve_fit:  {J_curve if J_curve else '—':.3e}")
        print(f"  - J 高频中值:  {J_hf if J_hf else '—':.3e}")
        print(f"  - J 采用:      {J_est:.3e}  kg·m²")

        return J_est

    # ══════════════════════════════════════════════════════════
    #  4. SG 滤波粗摩擦提取 (V2)
    # ══════════════════════════════════════════════════════════

    def extract_friction_sg(self, J: float) -> None:
        """SG 滤波微分 + 迭代鲁棒拟合 (IRLS) 提取粗摩擦

        T_friction = T_cmd - J * SG_derivative(speed)
        分位数分箱 → 迭代加权最小二乘 → 粗摩擦参数.
        第 1 轮: 普通 polyfit  → 第 2 轮: 剔除 >3σ 异常值 → 第 3 轮: 残差加权拟合
        """
        speed  = self.valid[:, 1].astype(np.float64)
        torque = self.valid[:, 0].astype(np.float64)
        n = len(speed)

        # SG 微分
        win = min(15, n // 20 * 2 + 1)
        if win >= 5:
            accel = savgol_filter(speed, win, 3, deriv=1, delta=self.dt)
        else:
            accel = np.gradient(speed, self.dt)
        T_fric = torque - J * accel

        def bin_fit_robust(abs_spd, abs_frc):
            """分位数分箱 + 迭代鲁棒线性拟合 (IRLS)

            分箱取中位数 → 第 1 轮普通拟合 → 剔除 >3σ 残留 → 加权重拟合
            """
            edges = np.percentile(abs_spd, np.linspace(0, 100, 20))
            edges = np.unique(edges)
            s_m, f_m = [], []
            for i in range(len(edges) - 1):
                m = (abs_spd >= edges[i]) & (abs_spd < edges[i + 1])
                if np.sum(m) >= 5:
                    s_m.append(float(np.median(abs_spd[m])))
                    f_m.append(float(np.median(abs_frc[m])))
            if len(s_m) < 3:
                return 0.0, float(np.median(abs_frc))
            s_arr = np.array(s_m)
            f_arr = np.array(f_m)

            # 第 1 轮: 普通线性拟合
            c1 = np.polyfit(s_arr, f_arr, 1)
            residuals = np.abs(f_arr - np.polyval(c1, s_arr))
            sigma = np.std(residuals) + 1e-10

            # 第 2 轮: 剔除 >3σ 异常值, 重新拟合
            keep = residuals < 3.0 * sigma
            if np.sum(keep) >= 3:
                s2, f2 = s_arr[keep], f_arr[keep]
                c2 = np.polyfit(s2, f2, 1)
                residuals2 = np.abs(f2 - np.polyval(c2, s2))
                sigma2 = np.std(residuals2) + 1e-10
            else:
                c2, keep, sigma2 = c1, slice(None), sigma

            # 第 3 轮: 残差倒数加权拟合
            sk = s_arr[keep] if isinstance(keep, np.ndarray) else s_arr
            fk = f_arr[keep] if isinstance(keep, np.ndarray) else f_arr
            weights = 1.0 / (np.abs(fk - np.polyval(c2, sk)) + sigma2 * 0.5)
            weights = np.clip(weights, 0, 10.0 / max(sigma2, 1e-10))
            try:
                c3 = np.polyfit(sk, fk, 1, w=weights)
            except Exception:
                c3 = c2

            cv_est = max(float(c3[0]), 0.0)
            Tc_est = max(float(c3[1]), 0.0)
            return cv_est, Tc_est

        pos = speed > 0.1
        neg = speed < -0.1
        cv_p, Tc_p = bin_fit_robust(np.abs(speed[pos]), np.abs(T_fric[pos])) if np.any(pos) else (0, 0)
        cv_n, Tc_n = bin_fit_robust(np.abs(speed[neg]), np.abs(T_fric[neg])) if np.any(neg) else (0, 0)

        self.params['friction_viscous_pos'] = cv_p
        self.params['friction_viscous_neg'] = cv_n
        self.params['friction_coulomb_pos'] = Tc_p
        self.params['friction_coulomb_neg'] = Tc_n

        print(f"\n  粗摩擦提取 (SG 滤波微分):")
        print(f"  - cv+: {cv_p:.6f}  cv-: {cv_n:.6f}")
        print(f"  - Tc+: {Tc_p:.6f}  Tc-: {Tc_n:.6f}")

    # ══════════════════════════════════════════════════════════
    #  5. 动力学仿真 (防发散)
    # ══════════════════════════════════════════════════════════

    @staticmethod
    def _simulate(th0: float, om0: float, T: np.ndarray, dt: float,
                  J: float, cp: float, cn: float,
                  Tcp: float, Tcn: float, N: int, n_substeps: int = 4):
        """动力学前向仿真 (二阶积分, 子步细分, 防发散裁剪)

        子步细分: 每 dt 内细分为 n_substeps 步, 减小欧拉积分累积误差.
        """
        th = np.zeros(N, dtype=np.float64)
        om = np.zeros(N, dtype=np.float64)
        th[0], om[0] = float(th0), float(om0)
        inv_J = 1.0 / max(J, 1e-10)
        alpha_max = 1e5  # rad/s², 防输出溢出
        dts = dt / n_substeps  # 子步时长
        for i in range(1, N):
            o = om[i - 1]
            t_cur = th[i - 1]
            T_ext = T[min(i - 1, len(T) - 1)]  # 外部力矩
            for _ in range(n_substeps):
                if o > 0:
                    fric = cp * o + Tcp
                elif o < 0:
                    fric = cn * o - Tcn
                else:
                    fric = 0.0
                alpha = (T_ext - fric) * inv_J
                alpha = np.clip(alpha, -alpha_max, alpha_max)
                o_new = o + alpha * dts
                t_new = t_cur + o * dts + 0.5 * alpha * dts ** 2
                if np.isfinite(o_new):
                    o = o_new
                if np.isfinite(t_new):
                    t_cur = t_new
            om[i] = o
            th[i] = t_cur
        return th, om

    # ══════════════════════════════════════════════════════════
    #  6. 多窗口摩擦精炼 (固定 J, 4 参数)
    # ══════════════════════════════════════════════════════════

    def refine_friction(self, J: float, window_s: float = 10.0,
                        n_windows: int = 3, maxiter: int = 30,
                        verbose: bool = True) -> None:
        """多窗口短时仿真精炼摩擦参数

        关键改进: 固定 J (来自频域辨识), 仅优化 4 个摩擦参数.

        Args:
            J: 固定惯量 (来自频域 curve_fit)
            window_s: 窗口时长 (秒)
            n_windows: 窗口数量 (均匀分布于全长)
            maxiter: 每窗口最大迭代
            verbose: 进度打印
        """
        torque = self.valid[:, 0].astype(np.float64)
        speed  = self.valid[:, 1].astype(np.float64)
        pos    = self.valid[:, 2].astype(np.float64)
        n = len(torque)

        win_len = int(window_s / self.dt)
        # 均匀选取 n_windows 个窗口
        if n_windows <= 0:
            n_windows = 3
        total_span = n - win_len
        if total_span <= 0:
            starts = [0]
        else:
            starts = [int(i * total_span / max(n_windows - 1, 1))
                      for i in range(n_windows)]

        Tc_rough = float(np.median(np.abs(torque))) * 0.8

        # 初值列表
        x0_list = [
            [0.0, 0.0, Tc_rough, Tc_rough],
            [0.002, 0.002, Tc_rough * 0.7, Tc_rough * 0.7],
        ]

        all_params: list = []

        for wi, ws in enumerate(starts):
            we = min(ws + win_len, n)
            tm = torque[ws:we]
            sm = speed[ws:we]
            pm = pos[ws:we]
            th0, om0 = float(pm[0]), float(sm[0])

            def objective(x):
                cp  = max(abs(x[0]), 0.0)
                cn  = max(abs(x[1]), 0.0)
                Tcp = max(abs(x[2]), 0.0)
                Tcn = max(abs(x[3]), 0.0)
                _, om_p = self._simulate(th0, om0, tm, self.dt,
                                          J, cp, cn, Tcp, Tcn, len(tm),
                                          n_substeps=1)
                n_sim = min(len(om_p), len(sm))
                diff = om_p[:n_sim] - sm[:n_sim]
                return float(np.sqrt(np.mean(diff ** 2)))

            best_p = None
            best_e = np.inf
            for x0 in x0_list:
                try:
                    res = minimize(objective, x0, method='Nelder-Mead',
                                   options={'maxiter': maxiter,
                                            'xatol': 1e-6, 'fatol': 1e-6,
                                            'adaptive': True})
                    if res.fun < best_e:
                        best_e = float(res.fun)
                        best_p = np.abs(res.x)
                except Exception:
                    continue

            if best_p is not None:
                all_params.append(best_p)
                x0_list[0] = best_p.tolist()  # 更新初值

            if verbose:
                t_s = ws * self.dt
                status = f"RMSE={best_e:.4f} Tc+={best_p[2]:.5f}" if best_p is not None else "失败"
                print(f"    窗口 {wi + 1}/{len(starts)} @ {t_s:.0f}s: {status}")

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

        # 全长仿真 (仅用于验证报告)
        th_f, om_f = self._simulate(pos[0], speed[0], torque, self.dt,
                                      J, med[0], med[1], med[2], med[3], n)
        self.sim_pos   = th_f
        self.sim_speed = om_f

        e_om = float(np.sqrt(np.mean((om_f - speed) ** 2)))
        sr_om = float(np.max(speed) - np.min(speed))

        if verbose:
            print(f"\n  多窗口精炼 ({self.n_windows} × {window_s:.0f}s):")
            print(f"  - 速度 RMSE:       {e_om:.2f}  "
                  f"(NRMSE={e_om / max(sr_om, 1e-6) * 100:.1f}%)")
            print(f"  - 窗口一致性 (标准差 / 变异系数):")
            def fmt_cv(m, s):
                if m > 1e-8:
                    return f"{s:.4f} / {s / m * 100:.0f}%"
                return f"{s:.4f} / ∞"
            print(f"    cv+  {fmt_cv(med[0], std[0])}")
            print(f"    cv-  {fmt_cv(med[1], std[1])}")
            print(f"    Tc+  {fmt_cv(med[2], std[2])}")
            print(f"    Tc-  {fmt_cv(med[3], std[3])}")
            print(f"  - 中位数参数:")
            print(f"    cv+: {med[0]:.6f}  cv-: {med[1]:.6f}")
            print(f"    Tc+: {med[2]:.6f}  Tc-: {med[3]:.6f}")

            for i, m in enumerate(med[:2]):
                if m > 1e-6 and std[i] > m * 0.5:
                    print(f"  ⚠ {'cv+' if i == 0 else 'cv-'} "
                          f"窗口间变异大 (>50%), 建议固定为 0")

    # ══════════════════════════════════════════════════════════
    #  7. 验证与报告
    # ══════════════════════════════════════════════════════════

    def verify(self) -> dict:
        """全长仿真验证 + SNR + 分段 NRMSE + 参数不确定性"""
        torque  = self.valid[:, 0]
        speed_m = self.valid[:, 1]
        pos_m   = self.valid[:, 2]
        n = len(torque)

        if self.sim_speed is None:
            th_f, om_f = self._simulate(
                pos_m[0], speed_m[0], torque, self.dt,
                self.params['inertia'],
                self.params['friction_viscous_pos'],
                self.params['friction_viscous_neg'],
                self.params['friction_coulomb_pos'],
                self.params['friction_coulomb_neg'],
                n,
            )
            self.sim_pos   = th_f
            self.sim_speed = om_f

        N = min(len(self.sim_speed), len(speed_m))

        # ── 全长仿真误差 ──
        e_om = float(np.sqrt(np.mean((self.sim_speed[:N] - speed_m[:N]) ** 2)))
        e_th = float(np.sqrt(np.mean((self.sim_pos[:N] - pos_m[:N]) ** 2)))
        sr_om = float(np.max(speed_m) - np.min(speed_m))
        sr_th = float(np.max(pos_m) - np.min(pos_m))

        # ── 分段 NRMSE (前/中/后 各 1/3) ──
        seg_len = N // 3
        seg_om_rmse = []
        seg_th_rmse = []
        for seg_i in range(3):
            s0 = seg_i * seg_len
            s1 = min(s0 + seg_len, N)
            seg_om_rmse.append(float(np.sqrt(
                np.mean((self.sim_speed[s0:s1] - speed_m[s0:s1]) ** 2))))
            seg_th_rmse.append(float(np.sqrt(
                np.mean((self.sim_pos[s0:s1] - pos_m[s0:s1]) ** 2))))

        # ── SNR (PSD 法, 自适应截止频率) ──
        n_fft = N
        speed_fft = np.fft.rfft(speed_m[:N], n=n_fft)
        freq_snr  = np.fft.rfftfreq(n_fft, d=self.dt)
        psd = np.abs(speed_fft) ** 2 / n_fft
        f_end = (self.chirp_params['end_freq']
                 if self.chirp_params['end_freq'] else 100.0)
        # 信号带: 0 ~ f_end*2
        sig_mask = freq_snr < min(f_end * 2.0, self.fs * 0.4)
        signal_power = float(np.sum(psd[sig_mask]))
        noise_power  = float(np.sum(psd[~sig_mask])) if np.any(~sig_mask) else 1e-10
        snr = 10.0 * np.log10(max(signal_power / max(noise_power, 1e-10), 1e-10))

        # ── 力矩拟合 (SG 加速度, 仅参考) ──
        win = min(15, N // 20 * 2 + 1)
        if win >= 5:
            accel_e = savgol_filter(speed_m[:N], win, 3, deriv=1, delta=self.dt)
        else:
            accel_e = np.gradient(speed_m[:N], self.dt)
        I_p = (speed_m[:N] > 0).astype(float)
        I_n = (speed_m[:N] < 0).astype(float)
        Tp = self.params
        T_pred = (Tp['inertia'] * accel_e
                  + Tp['friction_viscous_pos'] * speed_m[:N] * I_p
                  + Tp['friction_viscous_neg'] * speed_m[:N] * I_n
                  + Tp['friction_coulomb_pos'] * I_p
                  + Tp['friction_coulomb_neg'] * I_n)
        e_T = float(np.sqrt(np.mean((torque[:N] - T_pred) ** 2)))

        # ── 力矩 SNR ──
        torque_fft = np.fft.rfft(torque[:N], n=n_fft)
        torque_psd = np.abs(torque_fft) ** 2 / n_fft
        T_signal = float(np.sum(torque_psd[sig_mask]))
        T_noise  = float(np.sum(torque_psd[~sig_mask])) if np.any(~sig_mask) else 1e-10
        torque_snr = 10.0 * np.log10(max(T_signal / max(T_noise, 1e-10), 1e-10))

        # ── 参数不确定性 (窗口间变异系数中位数) ──
        param_cv = None
        if self.window_results is not None and len(self.window_results) > 1:
            wr = np.array(self.window_results)
            med = np.median(wr, axis=0)
            std = np.std(wr, axis=0)
            # 变异系数 = std/|median|, 只对非零中位数计算
            cv_arr = np.array([s / max(abs(m), 1e-12) for s, m in zip(std, med)])
            param_cv = float(np.median(cv_arr))
        elif self.window_params_std is not None:
            med = np.array([self.params['friction_viscous_pos'],
                           self.params['friction_viscous_neg'],
                           self.params['friction_coulomb_pos'],
                           self.params['friction_coulomb_neg']])
            std = self.window_params_std
            cv_arr = np.array([s / max(abs(m), 1e-12) for s, m in zip(std, med)])
            param_cv = float(np.median(cv_arr))

        self.verification = {
            'omega_rmse':    e_om,
            'omega_nrmse':   e_om / max(sr_om, 1e-6),
            'omega_nrmse_pct': e_om / max(sr_om, 1e-6) * 100,
            'theta_rmse':    e_th,
            'theta_nrmse':   e_th / max(sr_th, 1e-6),
            'theta_nrmse_pct': e_th / max(sr_th, 1e-6) * 100,
            'torque_rmse':   e_T,
            'snr_db':        snr,
            'torque_snr_db': torque_snr,
            'n_samples':     N,
            'n_windows':     self.n_windows,
            'seg_omega_rmse': seg_om_rmse,
            'seg_theta_rmse': seg_th_rmse,
            'param_cv':      param_cv,
        }

        return self.verification

    def print_report(self) -> None:
        """打印完整辨识报告"""
        p = self.params
        v = self.verification

        print(f"\n{'=' * 55}")
        print(f"  轴参数辨识报告  |  {os.path.basename(self.csv_path or '')}")
        print(f"{'=' * 55}")

        print(f"\n  ── 惯量 ──")
        print(f"    J = {p['inertia']:.3e}  kg·m²")
        if self.J_curve and self.J_v1style:
            ratio = max(self.J_curve, self.J_v1style) / max(min(self.J_curve, self.J_v1style), 1e-20)
            print(f"    (curve_fit: {self.J_curve:.3e}, 高频: {self.J_v1style:.3e}, 比={ratio:.1f}x)")

        print(f"\n  ── 摩擦参数 ──")
        print(f"    cv+:  {p['friction_viscous_pos']:.6f}     "
              f"cv-:  {p['friction_viscous_neg']:.6f}   Nm·s/rad")
        print(f"    Tc+:  {p['friction_coulomb_pos']:.6f}     "
              f"Tc-:  {p['friction_coulomb_neg']:.6f}   Nm")

        if self.window_params_std is not None:
            std = self.window_params_std
            print(f"    (窗口标准差)  cv±{std[0]:.4f}/{std[1]:.4f}  "
                  f"Tc±{std[2]:.4f}/{std[3]:.4f}")

        print(f"\n  ── 扫频参数 ──")
        cp = self.chirp_params
        print(f"    {cp['start_freq']:.1f}→{cp['end_freq']:.1f} Hz  "
              f"A={cp['amplitude']:.4f} Nm  T={cp['duration']:.1f}s")

        print(f"\n  ── 验证指标 ──")
        print(f"    速度 NRMSE:   {v.get('omega_nrmse_pct', 0):.1f}%")
        print(f"    位置 NRMSE:   {v.get('theta_nrmse_pct', 0):.1f}%")
        print(f"    力矩 RMSE:    {v.get('torque_rmse', 0):.4f} Nm")
        print(f"    速度 SNR:     {v.get('snr_db', 0):.1f} dB")
        print(f"    力矩 SNR:     {v.get('torque_snr_db', 0):.1f} dB")
        print(f"    数据:         {self.n:,} pts × {self.fs:.0f} Hz")
        print(f"    窗口:         {v.get('n_windows', 0)}")
        pcv = v.get('param_cv')
        if pcv is not None:
            icv = "✓" if pcv < 0.3 else ("⚠" if pcv < 0.5 else "✗")
            print(f"    参数 CV 中位数: {pcv:.2f}  {icv}")
        seg = v.get('seg_omega_rmse')
        if seg and len(seg) == 3 and max(v.get('omega_rmse', 1), 1e-6) > 0:
            seg_pct = [s / max(v['omega_rmse'], 1e-6) * 100 for s in seg]
            print(f"    分段速度 NRMSE (前/中/后): "
                  f"{seg_pct[0]:.0f}% / {seg_pct[1]:.0f}% / {seg_pct[2]:.0f}%")

        print(f"{'=' * 55}")

    # ══════════════════════════════════════════════════════════
    #  8. 时域最小二乘辨识 (V1 替代方法)
    # ══════════════════════════════════════════════════════════

    def identify_by_least_squares(self) -> dict:
        """时域批量最小二乘辨识 (V1 替代方法, 仅参考)"""
        if self.valid is None:
            raise RuntimeError("请先 load_data()")

        speed  = self.valid[:, 1].astype(np.float64)
        torque = self.valid[:, 0].astype(np.float64)
        pos    = self.valid[:, 2].astype(np.float64)
        n = len(speed)

        win = min(15, n // 20 * 2 + 1)
        if win >= 5:
            alpha = savgol_filter(speed, win, 3, deriv=1, delta=self.dt)
        else:
            alpha = np.gradient(speed, self.dt)

        I_p = (speed > 0.0).astype(np.float64)
        I_n = (speed < 0.0).astype(np.float64)
        Phi = np.column_stack([alpha, np.cos(pos),
                                speed * I_p, speed * I_n, I_p, I_n])
        theta, residuals, _, _ = np.linalg.lstsq(Phi, torque, rcond=None)

        J_est  = float(theta[0])
        G_est  = float(theta[1])
        cv_pos = max(float(theta[2]), 0.0)
        cv_neg = max(float(theta[3]), 0.0)
        Tc_pos = max(float(theta[4]), 0.0)
        Tc_neg = max(float(theta[5]), 0.0)

        self.params['inertia']              = J_est
        self.params['friction_viscous_pos'] = cv_pos
        self.params['friction_viscous_neg'] = cv_neg
        self.params['friction_coulomb_pos'] = Tc_pos
        self.params['friction_coulomb_neg'] = Tc_neg

        pred     = Phi @ theta
        residual = torque - pred
        r2 = 1.0 - float(np.var(residual) / max(np.var(torque), 1e-20))
        e_T = float(np.sqrt(np.mean(residual ** 2)))

        print(f"\n  时域批量最小二乘:")
        print(f"  - J:        {J_est:.3e}  kg·m²")
        print(f"  - cv+/cv-:  {cv_pos:.6f} / {cv_neg:.6f}")
        print(f"  - Tc+/Tc-:  {Tc_pos:.6f} / {Tc_neg:.6f}")
        print(f"  - 重力残差: {G_est:.6f}  Nm")
        print(f"  - R²:       {r2:.4f}   力矩 RMSE={e_T:.4f} Nm")

        return self.params

    # ══════════════════════════════════════════════════════════
    #  9. 序列化
    # ══════════════════════════════════════════════════════════

    def to_dict(self) -> dict:
        """导出结果为 dict"""
        result = {
            'parameters': {
                'inertia':                self.params['inertia'],
                'inertia_curve_fit':      self.J_curve,
                'inertia_high_freq':      self.J_v1style,
                'friction_viscous_pos':   self.params['friction_viscous_pos'],
                'friction_viscous_neg':   self.params['friction_viscous_neg'],
                'friction_coulomb_pos':   self.params['friction_coulomb_pos'],
                'friction_coulomb_neg':   self.params['friction_coulomb_neg'],
            },
            'chirp_params': {
                k: float(v) if v is not None else None
                for k, v in self.chirp_params.items()
            },
            'verification': self.verification,
            'data_info': {
                'sampling_rate_hz': self.fs,
                'n_samples':        self.n,
                'format':           self.fmt,
            },
            'n_windows': self.n_windows,
        }
        # 添加窗口一致性细节
        if self.window_params_std is not None:
            med = np.array([self.params['friction_viscous_pos'],
                           self.params['friction_viscous_neg'],
                           self.params['friction_coulomb_pos'],
                           self.params['friction_coulomb_neg']])
            std = self.window_params_std
            result['window_consistency'] = {
                'cv_pos_std': float(std[0]),
                'cv_neg_std': float(std[1]),
                'Tc_pos_std': float(std[2]),
                'Tc_neg_std': float(std[3]),
                'cv_pos_cv': float(std[0] / max(abs(med[0]), 1e-12)),
                'cv_neg_cv': float(std[1] / max(abs(med[1]), 1e-12)),
                'Tc_pos_cv': float(std[2] / max(abs(med[2]), 1e-12)),
                'Tc_neg_cv': float(std[3] / max(abs(med[3]), 1e-12)),
            }
        return result

    def save_report(self, path: str) -> None:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
        print(f"\n  报告已保存: {path}")

    # ══════════════════════════════════════════════════════════
    #  10. 完整流水线
    # ══════════════════════════════════════════════════════════

    def identify_all(self, csv_path: Optional[str] = None,
                     window_s: float = 10.0, n_windows: int = 3,
                     maxiter: int = 30, verbose: bool = True) -> dict:
        """完整辨识流水线 (默认 V3 综合法)"""
        self.load_data(csv_path)
        self.estimate_chirp_params()

        # 1. 频域惯量 (V1, 最可靠)
        J = self.identify_inertia()

        # 2. SG 粗摩擦 (V2, 初值)
        self.extract_friction_sg(J)

        # 3. 多窗口精炼 (固定 J, 仅优化摩擦)
        self.refine_friction(J, window_s=window_s, n_windows=n_windows,
                             maxiter=maxiter, verbose=verbose)

        # 4. 验证
        self.verify()

        if verbose:
            self.print_report()
        return self.params

    def identify_all_lstsq(self, csv_path: Optional[str] = None,
                           verbose: bool = True) -> dict:
        """替代流水线: 时域最小二乘 (V1)"""
        self.load_data(csv_path)
        self.estimate_chirp_params()
        self.identify_by_least_squares()
        self.verify()
        if verbose:
            self.print_report()
        return self.params


    # ══════════════════════════════════════════════════════════
    #  11. 多文件批量辨识
    # ══════════════════════════════════════════════════════════

    @staticmethod
    def identify_batch(csv_files: list, window_s: float = 5.0,
                       maxiter: int = 80, verbose: bool = True,
                       save_dir: Optional[str] = None) -> dict:
        """批量辨识多个 CSV 文件, 输出一致性汇总

        Args:
            csv_files: CSV 文件路径列表
            window_s: 窗口时长
            maxiter: 每窗口最大迭代
            verbose: 进度打印
            save_dir: 保存单个报告的目录 (可选)

        Returns:
            包含各文件结果列表和汇总统计的 dict
        """
        all_params = []
        all_verification = []
        all_file_results = []

        print(f"\n{'=' * 55}")
        print(f"  批量辨识模式: {len(csv_files)} 个文件")
        print(f"{'=' * 55}")

        for i, f in enumerate(csv_files):
            fname = os.path.basename(f)
            print(f"\n  [{i + 1}/{len(csv_files)}] {fname}")

            identifier = AxisMitLiteIdentifier(f)
            try:
                result = identifier.identify_all(
                    window_s=window_s, maxiter=maxiter,
                    verbose=verbose,
                )
                all_params.append(identifier.params.copy())
                all_verification.append(identifier.verification.copy())
                all_file_results.append({
                    'file': f,
                    'filename': fname,
                    'params': identifier.params.copy(),
                    'verification': identifier.verification.copy(),
                    'chirp_params': identifier.chirp_params.copy(),
                })

                if save_dir:
                    os.makedirs(save_dir, exist_ok=True)
                    base = os.path.splitext(fname)[0]
                    identifier.save_report(
                        os.path.join(save_dir, f"{base}_report.json"))

            except Exception as e:
                print(f"    ❌ 失败: {e}")
                continue

        if not all_params:
            print("\n  ⚠ 无成功辨识结果")
            return {'files': [], 'summary': {}}

        # ── 汇总统计 ──
        param_keys = ['inertia', 'friction_viscous_pos', 'friction_viscous_neg',
                      'friction_coulomb_pos', 'friction_coulomb_neg']
        summary = {}
        for key in param_keys:
            vals = [p[key] for p in all_params if p.get(key) is not None]
            if vals:
                summary[key] = {
                    'median': float(np.median(vals)),
                    'mean':   float(np.mean(vals)),
                    'std':    float(np.std(vals)),
                    'cv':     float(np.std(vals) / max(abs(np.median(vals)), 1e-12)),
                    'n':      len(vals),
                }

        # 验证指标汇总
        v_keys = ['omega_nrmse_pct', 'theta_nrmse_pct',
                  'torque_rmse', 'snr_db', 'torque_snr_db']
        v_summary = {}
        for key in v_keys:
            vals = [v[key] for v in all_verification if key in v and v[key] is not None]
            if vals:
                v_summary[key] = {
                    'median': float(np.median(vals)),
                    'mean':   float(np.mean(vals)),
                    'std':    float(np.std(vals)),
                    'n':      len(vals),
                }

        print(f"\n{'=' * 55}")
        print(f"  批量辨识汇总 ({len(all_params)}/{len(csv_files)} 成功)")
        print(f"{'=' * 55}")
        print(f"\n  ── 参数一致性 ──")
        for key in param_keys:
            s = summary.get(key, {})
            if s:
                print(f"    {key:24s}  {s['median']:.3e}  "
                      f"± {s['std']:.3e}  (CV={s.get('cv', 0)*100:.0f}%, n={s['n']})")

        print(f"\n  ── 验证一致性 ──")
        for key in v_keys:
            s = v_summary.get(key, {})
            if s:
                print(f"    {key:24s}  {s['median']:.3f}  "
                      f"± {s['std']:.3f}")

        print(f"{'=' * 55}")

        return {
            'files':   all_file_results,
            'summary': {'params': summary, 'verification': v_summary},
        }


# ═══════════════════════════════════════════════════════════════
#  主函数
# ═══════════════════════════════════════════════════════════════

def main() -> None:
    parser = argparse.ArgumentParser(
        description="MIT Lite 轴参数辨识脚本 (V3 综合版, 优化)")
    parser.add_argument("csv_path", nargs="?", default=None,
                        help="CSV 数据文件路径 (批量模式时忽略)")
    parser.add_argument("--save", type=str, default=None,
                        help="保存报告到 JSON 文件")
    parser.add_argument("--method", choices=['default', 'lstsq'],
                        default='default',
                        help="辨识方法: default(频域惯量+多窗口) / lstsq(最小二乘)")
    parser.add_argument("--maxiter", type=int, default=30,
                        help="每窗口最大迭代 (默认 30)")
    parser.add_argument("--window", type=float, default=10.0,
                        help="窗口时长 (秒, 默认 10.0)")
    parser.add_argument("--n-windows", type=int, default=3,
                        help="窗口数量 (默认 3)")
    parser.add_argument("--no-verbose", action="store_true",
                        help="静默模式")
    parser.add_argument("--batch", nargs="+", default=None,
                        help="批量辨识多个 CSV 文件")
    parser.add_argument("--batch-dir", type=str, default=None,
                        help="批量辨识目录下所有 CSV 文件")
    parser.add_argument("--save-dir", type=str, default=None,
                        help="批量模式: 保存单个报告到此目录")
    args = parser.parse_args()

    try:
        verbose = not args.no_verbose

        # ── 批量模式 ──
        if args.batch or args.batch_dir:
            files = args.batch or []
            if args.batch_dir:
                import glob
                bd = args.batch_dir
                pattern = os.path.join(bd, "*.csv")
                dir_files = sorted(glob.glob(pattern))
                files.extend(dir_files)
            # 去重
            files = list(dict.fromkeys(files))
            if not files:
                print("  ⚠ 未找到 CSV 文件")
                sys.exit(1)
            AxisMitLiteIdentifier.identify_batch(
                files, window_s=args.window, maxiter=args.maxiter,
                verbose=verbose, save_dir=args.save_dir,
            )
            return

        # ── 单文件模式 ──
        csv_path = args.csv_path
        if csv_path is None:
            base = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
            csv_path = os.path.join(base, "measure_data", "1.csv")

        identifier = AxisMitLiteIdentifier(csv_path)

        if args.method == 'lstsq':
            identifier.identify_all_lstsq(
                verbose=verbose,
            )
        else:
            identifier.identify_all(
                window_s=args.window,
                n_windows=args.n_windows,
                maxiter=args.maxiter,
                verbose=verbose,
            )

        if args.save:
            identifier.save_report(args.save)

    except Exception as e:
        print(f"\n  ❌ 辨识过程出错: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
