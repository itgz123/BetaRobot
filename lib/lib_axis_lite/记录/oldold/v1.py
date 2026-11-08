"""
MIT Lite 轴参数辨识脚本
- 从扫频实验数据辨识惯量、双向库仑摩擦、双向粘性摩擦
- 默认重力已补偿
- 计算数据信噪比

用法:
    python identify_axis_mit_lite.py <csv_path>

@author TRW
@date 2026-06-18
"""

import os
import sys
import argparse
import numpy as np
import pandas as pd
from scipy import signal
from scipy.optimize import curve_fit
from typing import Optional

# 设置控制台编码
if sys.platform == "win32":
    import io

    if isinstance(sys.stdout, io.TextIOWrapper):
        sys.stdout.reconfigure(encoding="utf-8")


class AxisMitLiteIdentifier:
    """MIT Lite 轴参数辨识器

    从扫频实验数据辨识惯量、双向库仑摩擦、双向粘性摩擦。
    默认重力已补偿，T_chirp = J*dω/dt + T_friction(ω)。

    摩擦模型（绝对值法）：
    - 正向 ω>0: |T_friction| = c_v_pos * |ω| + T_c_pos
    - 负向 ω<0: |T_friction| = c_v_neg * |ω| + T_c_neg
    """

    # CSV列索引映射
    COL_CH0 = 1  # 单片机us时间戳
    COL_POS = 2  # CH1: 反馈位置
    COL_SPEED = 3  # CH2: 反馈速度
    COL_REF_POS = 4  # CH3: 位置设定值
    COL_REF_SPEED = 5  # CH4: 速度设定值
    COL_GRAVITY_FF = 6  # CH5: 重力前馈
    COL_INERTIA_FF = 7  # CH6: 惯量前馈
    COL_FRICTION_FF = 8  # CH7: 摩擦前馈 - 扫频力矩
    COL_TOTAL_FF = 9  # CH8: 总前馈

    def __init__(self, csv_path: Optional[str] = None):
        self.csv_path = csv_path
        self.data: Optional[np.ndarray] = None
        self.fs: float = 1000.0
        self.valid_start_idx: int = 0
        self.valid_end_idx: int = 0  # 扫频结束索引

        # 辨识结果
        self.results: dict[str, Optional[float]] = {
            "inertia": None,
            "friction_viscous_pos": None,
            "friction_viscous_neg": None,
            "friction_coulomb_pos": None,
            "friction_coulomb_neg": None,
            "chirp_start_freq": None,
            "chirp_end_freq": None,
            "chirp_amplitude": None,
            "chirp_duration": None,
            "snr": None,
        }

    def load_data(self, csv_path: Optional[str] = None) -> np.ndarray:
        """加载CSV数据"""
        if csv_path is None:
            csv_path = self.csv_path
        if csv_path is None:
            raise ValueError("请提供CSV文件路径")

        df = pd.read_csv(csv_path)
        raw_data = df.values

        # 使用单片机us时间戳 (CH0)，去除重复帧
        time_us = raw_data[:, self.COL_CH0].astype(np.float64)
        unique_mask = np.diff(time_us, prepend=-1) > 0
        self.data = raw_data[unique_mask]

        # 计算采样频率
        valid_time_us = self.data[:, self.COL_CH0].astype(np.float64)
        dt_us = np.median(np.diff(valid_time_us))
        if dt_us > 0:
            self.fs = 1.0 / (dt_us * 1e-6)
        else:
            self.fs = 5000.0

        # 检测有效数据起始点（扫频力矩非零）
        friction_ff = self.data[:, self.COL_FRICTION_FF]
        non_zero_mask = np.abs(friction_ff) > 1e-6
        if np.any(non_zero_mask):
            non_zero_indices = np.where(non_zero_mask)[0]
            self.valid_start_idx = int(non_zero_indices[0])
            self.valid_end_idx = int(non_zero_indices[-1])
        else:
            self.valid_start_idx = 0
            self.valid_end_idx = len(self.data) - 1

        # 计算有效数据时长（扫频期间）
        valid_time = valid_time_us[self.valid_start_idx : self.valid_end_idx + 1]
        duration_s = (valid_time[-1] - valid_time[0]) * 1e-6

        print(f"数据加载完成:")
        print(f"  - 采样频率: {self.fs:.1f} Hz")
        print(f"  - 原始样本数: {len(raw_data)}, 去重后: {len(self.data)}")
        print(f"  - 扫频起始索引: {self.valid_start_idx}")
        print(f"  - 扫频结束索引: {self.valid_end_idx}")
        print(f"  - 扫频时长: {duration_s:.2f} s")

        return self.data

    def get_valid_data(self) -> np.ndarray:
        """获取扫频有效数据段"""
        if self.data is None:
            raise RuntimeError("请先 load_data()")
        return self.data[self.valid_start_idx : self.valid_end_idx + 1, :]

    def get_time_array(self) -> np.ndarray:
        """获取单片机时间戳数组 (秒)"""
        valid_data = self.get_valid_data()
        time_us = valid_data[:, self.COL_CH0].astype(np.float64)
        return (time_us - time_us[0]) * 1e-6

    def estimate_chirp_params(self) -> dict[str, Optional[float]]:
        """从数据估计扫频参数"""
        valid_data = self.get_valid_data()
        time = self.get_time_array()
        friction_ff = valid_data[:, self.COL_FRICTION_FF]

        self.results["chirp_duration"] = float(time[-1] - time[0])
        self.results["chirp_amplitude"] = float(np.percentile(np.abs(friction_ff), 95))

        # 瞬时频率估计
        analytic_signal = np.asarray(signal.hilbert(friction_ff))
        instantaneous_phase = np.unwrap(np.angle(analytic_signal))
        instantaneous_freq = np.diff(instantaneous_phase) / (2 * np.pi * np.diff(time))

        valid_freq_mask = (instantaneous_freq > 0) & (instantaneous_freq < 1000)
        if np.any(valid_freq_mask):
            valid_freqs = instantaneous_freq[valid_freq_mask]
            n_start = int(len(valid_freqs) * 0.05)
            self.results["chirp_start_freq"] = float(
                np.median(valid_freqs[: max(n_start, 1)])
            )
            self.results["chirp_end_freq"] = float(
                np.median(valid_freqs[-max(n_start, 1) :])
            )

        print(f"\n扫频参数估计:")
        print(f"  - 起始频率: {self.results['chirp_start_freq']:.2f} Hz")
        print(f"  - 结束频率: {self.results['chirp_end_freq']:.2f} Hz")
        print(f"  - 振幅: {self.results['chirp_amplitude']:.4f} Nm")
        print(f"  - 时长: {self.results['chirp_duration']:.2f} s")

        return self.results

    def compute_frequency_response(self) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        """计算频率响应"""
        valid_data = self.get_valid_data()
        n_samples = len(valid_data)
        torque = valid_data[:, self.COL_FRICTION_FF]
        speed = valid_data[:, self.COL_SPEED]

        n_fft = n_samples
        freq = np.fft.rfftfreq(n_fft, 1.0 / self.fs)
        torque_fft = np.fft.rfft(torque, n=n_fft)
        speed_fft = np.fft.rfft(speed, n=n_fft)

        eps = 1e-10
        H = speed_fft / (torque_fft + eps)
        magnitude = np.abs(H)
        phase = np.angle(H)

        freq_min = (
            self.results["chirp_start_freq"] * 0.5
            if self.results["chirp_start_freq"]
            else 0.1
        )
        freq_max = (
            self.results["chirp_end_freq"] * 1.5
            if self.results["chirp_end_freq"]
            else 100
        )
        valid_freq_mask = (freq >= freq_min) & (freq <= freq_max)

        return freq[valid_freq_mask], magnitude[valid_freq_mask], phase[valid_freq_mask]

    def identify_inertia(self) -> float:
        """频域辨识惯量 J

        通过幅频响应曲线拟合模型 |H(ω)| = 1/sqrt((J·ω)² + c_v²)，
        高频段惯性主导，取 J 作为最终结果。

        Returns:
            J: 惯量 (kg·m²)
        """
        freq, magnitude, _ = self.compute_frequency_response()

        def model_magnitude(w: np.ndarray, J: float, c_v: float) -> np.ndarray:
            return 1.0 / np.sqrt((J * w) ** 2 + c_v**2)

        w = 2 * np.pi * freq
        J_est = self.results["inertia"]

        try:
            popt, _ = curve_fit(
                model_magnitude, w, magnitude, p0=[0.001, 0.01], bounds=(0, np.inf)
            )
            J_est = float(popt[0])
        except Exception:
            high_freq_mask = freq > 50
            if np.any(high_freq_mask):
                J_est = float(
                    np.median(
                        1.0
                        / (magnitude[high_freq_mask] * 2 * np.pi * freq[high_freq_mask])
                    )
                )

        self.results["inertia"] = J_est

        print(f"\n惯量辨识（频域）:")
        print(f"  - 惯量 J: {J_est:.6f} kg·m²")

        return J_est

    def identify_friction_model(self, J: float) -> dict[str, Optional[float]]:
        """时域辨识双向库仑摩擦和双向粘性摩擦

        原理: T_friction = T_chirp - J * dω/dt
        - 正向 ω>0: |T_friction| = c_v_pos * |ω| + T_c_pos
        - 负向 ω<0: |T_friction| = c_v_neg * |ω| + T_c_neg

        使用绝对值法避免符号约定歧义，分位数分箱提高抗噪性。

        Args:
            J: 惯量 (kg·m²), 来自频域辨识

        Returns:
            self.results (更新摩擦相关字段)
        """
        valid_data = self.get_valid_data()
        time = self.get_time_array()
        speed = valid_data[:, self.COL_SPEED].astype(np.float64)
        torque = valid_data[:, self.COL_FRICTION_FF].astype(np.float64)

        # ---------- 1. 数值微分求加速度 ----------
        dt = np.median(np.diff(time))
        accel = np.gradient(speed, dt)

        # ---------- 2. 估计摩擦转矩 ----------
        T_friction = torque - J * accel
        T_abs = np.abs(T_friction)
        abs_speed = np.abs(speed)

        # ---------- 3. 参数 ----------
        SPEED_THR = 0.2  # rad/s，高速区阈值
        MIN_BIN_PTS = 5
        N_BINS = 20

        # ---------- 4. 分位数分箱线性拟合 ----------
        def quantile_bin_fit(
            s_data: np.ndarray, t_data: np.ndarray
        ) -> tuple[float, float]:
            """分位数分箱 + 线性拟合 |T_friction| vs |ω|
            Returns: (c_v, T_c)
            """
            n_pts = len(s_data)
            if n_pts < MIN_BIN_PTS * 3:
                return 0.0, float(np.median(t_data))

            bin_edges = np.percentile(s_data, np.linspace(0, 100, N_BINS + 1))
            bin_edges = np.unique(bin_edges)

            s_means, t_means = [], []
            for i in range(len(bin_edges) - 1):
                lo, hi = bin_edges[i], bin_edges[i + 1]
                if hi - lo < 1e-8:
                    continue
                in_bin = (s_data >= lo) & (s_data < hi)
                if np.sum(in_bin) >= MIN_BIN_PTS:
                    s_means.append(float(np.median(s_data[in_bin])))
                    t_means.append(float(np.median(t_data[in_bin])))

            s_means = np.array(s_means)
            t_means = np.array(t_means)

            if len(s_means) < 3:
                return 0.0, float(np.median(t_data))

            coeffs = np.polyfit(s_means, t_means, 1)
            cv_fit = max(float(coeffs[0]), 0.0)
            tc_fit = max(float(coeffs[1]), 0.0)

            if cv_fit < 1e-8:
                return 0.0, float(np.median(t_data))

            return cv_fit, tc_fit

        # ---------- 5. 正负方向独立拟合 ----------
        pos_mask = speed > SPEED_THR
        neg_mask = speed < -SPEED_THR

        c_v_pos = 0.0
        c_v_neg = 0.0
        T_c_pos = 0.0
        T_c_neg = 0.0

        if np.any(pos_mask):
            c_v_pos, T_c_pos = quantile_bin_fit(abs_speed[pos_mask], T_abs[pos_mask])

        if np.any(neg_mask):
            c_v_neg, T_c_neg = quantile_bin_fit(abs_speed[neg_mask], T_abs[neg_mask])

        self.results["friction_viscous_pos"] = float(f"{c_v_pos:.10f}")
        self.results["friction_viscous_neg"] = float(f"{c_v_neg:.10f}")
        self.results["friction_coulomb_pos"] = float(f"{T_c_pos:.10f}")
        self.results["friction_coulomb_neg"] = float(f"{T_c_neg:.10f}")

        # ---------- 6. 打印 ----------
        print(f"\n摩擦模型辨识（时域）:")
        print(f"  - 粘性摩擦 (正向) c_v_pos: {c_v_pos:.6f} Nm·s/rad")
        print(f"  - 粘性摩擦 (负向) c_v_neg: {c_v_neg:.6f} Nm·s/rad")
        print(f"  - 库仑摩擦 (正向) T_c_pos: {T_c_pos:.6f} Nm")
        print(f"  - 库仑摩擦 (负向) T_c_neg: {T_c_neg:.6f} Nm")

        return self.results

    def identify_by_least_squares(self) -> dict[str, Optional[float]]:
        """时域批量最小二乘辨识

        一次性解算全部参数：惯量 J、双向粘性、双向库仑、重力残差。
        模型: T(t) = J·α + c_v_pos·ω·I⁺ + c_v_neg·ω·I⁻ + T_c_pos·I⁺ + T_c_neg·I⁻ + G·cos(pos)

        注意: 实测 R² ≈ 0.18，残差 (0.016 Nm) >> 摩擦信号 (0.0005 Nm)，
        说明电机电流环动态等未建模效应主导了残差。从 chirp 数据可靠识别摩擦的
        信噪比不足。建议用频域法辨识 J，摩擦参数通过 TUNE 阶段经验调试确定。
        """
        valid_data = self.get_valid_data()
        time = self.get_time_array()
        speed = valid_data[:, self.COL_SPEED].astype(np.float64)
        torque = valid_data[:, self.COL_FRICTION_FF].astype(np.float64)
        position = valid_data[:, self.COL_POS].astype(np.float64)

        dt = np.median(np.diff(time))

        # ---------- 1. 加速度 ----------
        alpha = np.gradient(speed, dt)

        # ---------- 2. 回归矩阵 ----------
        I_pos = (speed > 0.0).astype(np.float64)
        I_neg = (speed < 0.0).astype(np.float64)

        Phi = np.column_stack([
            alpha, np.cos(position),
            speed * I_pos, speed * I_neg, I_pos, I_neg,
        ])

        # ---------- 3. 最小二乘 ----------
        theta, _, _, _ = np.linalg.lstsq(Phi, torque, rcond=None)

        J_est = float(theta[0])
        G_est = float(theta[1])
        cv_pos = max(float(theta[2]), 0.0)
        cv_neg = max(float(theta[3]), 0.0)
        Tc_pos = max(float(theta[4]), 0.0)
        Tc_neg = max(float(theta[5]), 0.0)

        self.results["inertia"] = float(f"{J_est:.10f}")
        self.results["friction_viscous_pos"] = float(f"{cv_pos:.10f}")
        self.results["friction_viscous_neg"] = float(f"{cv_neg:.10f}")
        self.results["friction_coulomb_pos"] = float(f"{Tc_pos:.10f}")
        self.results["friction_coulomb_neg"] = float(f"{Tc_neg:.10f}")

        pred = Phi @ theta
        residual = torque - pred
        r2 = 1.0 - np.var(residual) / np.var(torque)

        # 频域法 J 作为对比
        J_freq = self.results["inertia"]
        if J_freq is None:
            J_freq = self.identify_inertia()

        print(f"\n时域批量最小二乘辨识:")
        print(f"  - 惯量 J: {J_est:.6f} kg·m²  (频域 J={J_freq:.6f})")
        print(f"  - 粘性摩擦 (正向) c_v_pos: {cv_pos:.6f} Nm·s/rad")
        print(f"  - 粘性摩擦 (负向) c_v_neg: {cv_neg:.6f} Nm·s/rad")
        print(f"  - 库仑摩擦 (正向) T_c_pos: {Tc_pos:.6f} Nm")
        print(f"  - 库仑摩擦 (负向) T_c_neg: {Tc_neg:.6f} Nm")
        print(f"  - 重力残差 G: {G_est:.6f} Nm")
        print(f"  - 拟合优度 R²: {r2:.4f} (残差 std={np.std(residual):.6f} Nm)")
        print(f"  ⚠ R² 过低: 残差 >> 摩擦信号，摩擦值不可靠，建议经验调试")

        return self.results

    def calculate_snr(self) -> dict[str, Optional[float]]:
        """计算信噪比"""
        valid_data = self.get_valid_data()
        n_samples = len(valid_data)
        speed = valid_data[:, self.COL_SPEED]
        torque = valid_data[:, self.COL_FRICTION_FF]

        signal_power = float(np.var(speed))
        n_fft = n_samples
        speed_fft = np.fft.rfft(speed, n=n_fft)
        freq = np.fft.rfftfreq(n_fft, 1.0 / self.fs)

        high_freq = (
            self.results["chirp_end_freq"] * 2
            if self.results["chirp_end_freq"]
            else 100
        )
        high_freq_mask = freq > high_freq
        if np.any(high_freq_mask):
            noise_power = float(np.sum(np.abs(speed_fft[high_freq_mask]) ** 2) / n_fft)
        else:
            noise_power = float(
                np.var(speed - np.convolve(torque, np.ones(50) / 50, mode="same"))
            )

        if noise_power > 0:
            self.results["snr"] = float(10 * np.log10(signal_power / noise_power))
        else:
            self.results["snr"] = float("inf")

        print(f"\n信噪比: {self.results['snr']:.2f} dB")

        return self.results

    def identify_all(
        self, csv_path: Optional[str] = None, method: str = "lstsq"
    ) -> dict[str, Optional[float]]:
        """执行完整辨识流程

        Args:
            csv_path: CSV 文件路径
            method: "lstsq"（时域最小二乘，推荐）或 "freq"（频域两步法，参考）

        流程:
          1. 加载 CSV 数据
          2. 估计扫频参数
          3. 参数辨识
          4. 信噪比计算
        """
        self.load_data(csv_path)
        self.estimate_chirp_params()

        if method == "lstsq":
            self.identify_by_least_squares()
        else:
            J = self.identify_inertia()
            self.identify_friction_model(J)

        self.calculate_snr()
        return self.results

    def print_results_summary(self) -> None:
        """打印辨识结果汇总"""
        valid_data = self.get_valid_data()
        n_samples = len(valid_data)

        print("\n" + "=" * 50)
        print("         MIT Lite 轴参数辨识结果")
        print("=" * 50)
        print(f"惯量 (J):                   {self.results['inertia']:.6f} kg·m²")
        print("")
        print("粘性摩擦（双向）:")
        print(
            f"  正向 (c_v_pos):           {self.results['friction_viscous_pos']:.6f} Nm·s/rad"
        )
        print(
            f"  负向 (c_v_neg):           {self.results['friction_viscous_neg']:.6f} Nm·s/rad"
        )
        print("")
        print("库仑摩擦（双向）:")
        print(
            f"  正向 (T_c_pos):           {self.results['friction_coulomb_pos']:.6f} Nm"
        )
        print(
            f"  负向 (T_c_neg):           {self.results['friction_coulomb_neg']:.6f} Nm"
        )
        print("-" * 50)
        print(f"采样频率:                   {self.fs:.1f} Hz")
        print(f"采样点数:                   {n_samples}")
        print(f"信噪比 (SNR):               {self.results['snr']:.2f} dB")
        print("=" * 50)


def main() -> None:
    """主函数"""
    parser = argparse.ArgumentParser(description="MIT Lite 轴参数辨识脚本")
    parser.add_argument("csv_path", nargs="?", default=None, help="CSV数据文件路径")
    args = parser.parse_args()

    # 确定CSV文件路径
    if args.csv_path:
        csv_path = args.csv_path
    else:
        csv_path = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
            "measure_data",
            "1.csv",
        )

    identifier = AxisMitLiteIdentifier(csv_path)
    identifier.identify_all()
    identifier.print_results_summary()


if __name__ == "__main__":
    main()
