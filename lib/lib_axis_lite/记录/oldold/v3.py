"""
MIT Lite 轴参数辨识脚本
- 从扫频实验数据辨识惯量、摩擦、重力参数
- 计算数据信噪比

用法:
    python identify_axis_mit_lite.py <csv_path>

@author TRW
@date 2026-06-16
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
    """MIT Lite 轴参数辨识器"""

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
            "friction_viscous": None,
            "friction_coulomb": None,
            "gravity": None,
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
        valid_time = valid_time_us[self.valid_start_idx:self.valid_end_idx + 1]
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
        return self.data[self.valid_start_idx:self.valid_end_idx + 1, :]

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

    def identify_inertia_and_friction(self) -> dict[str, Optional[float]]:
        """辨识惯量和粘性摩擦"""
        freq, magnitude, _ = self.compute_frequency_response()

        def model_magnitude(w: np.ndarray, J: float, c_v: float) -> np.ndarray:
            return 1.0 / np.sqrt((J * w) ** 2 + c_v**2)

        w = 2 * np.pi * freq

        try:
            popt, _ = curve_fit(
                model_magnitude, w, magnitude, p0=[0.001, 0.01], bounds=(0, np.inf)
            )
            self.results["inertia"] = float(popt[0])
            self.results["friction_viscous"] = float(popt[1])
        except Exception:
            low_freq_mask = freq < 5
            if np.any(low_freq_mask):
                self.results["friction_viscous"] = float(
                    1.0 / np.median(magnitude[low_freq_mask])
                )
            high_freq_mask = freq > 50
            if np.any(high_freq_mask):
                self.results["inertia"] = float(
                    np.median(
                        1.0
                        / (magnitude[high_freq_mask] * 2 * np.pi * freq[high_freq_mask])
                    )
                )

        print(f"\n惯量和粘性摩擦辨识:")
        print(f"  - 惯量: {self.results['inertia']:.6f} kg*m^2")
        print(f"  - 粘性摩擦: {self.results['friction_viscous']:.6f} Nm*s/rad")

        return self.results

    def identify_coulomb_friction(self) -> dict[str, Optional[float]]:
        """辨识库仑摩擦"""
        valid_data = self.get_valid_data()
        speed = valid_data[:, self.COL_SPEED]
        total_ff = valid_data[:, self.COL_TOTAL_FF]

        zero_crossings = np.where(np.diff(np.sign(speed)))[0]
        if len(zero_crossings) > 0:
            torque_diffs = []
            window = 10
            for idx in zero_crossings:
                if idx > window and idx < len(speed) - window:
                    torque_before = float(np.mean(total_ff[idx - window : idx]))
                    torque_after = float(np.mean(total_ff[idx : idx + window]))
                    torque_diffs.append(np.abs(torque_after - torque_before))
            if torque_diffs:
                self.results["friction_coulomb"] = float(np.median(torque_diffs) / 2)

        if self.results["friction_coulomb"] is None:
            positive_speed_mask = speed > 0.1
            negative_speed_mask = speed < -0.1
            if np.any(positive_speed_mask) and np.any(negative_speed_mask):
                avg_torque_pos = float(np.mean(total_ff[positive_speed_mask]))
                avg_torque_neg = float(np.mean(total_ff[negative_speed_mask]))
                self.results["friction_coulomb"] = (
                    np.abs(avg_torque_pos - avg_torque_neg) / 2
                )

        print(f"\n库仑摩擦辨识:")
        print(f"  - 库仑摩擦: {self.results['friction_coulomb']:.6f} Nm")

        return self.results

    def identify_gravity(self) -> dict[str, Optional[float]]:
        """辨识重力参数"""
        valid_data = self.get_valid_data()
        pos = valid_data[:, self.COL_POS]
        speed = valid_data[:, self.COL_SPEED]
        gravity_ff = valid_data[:, self.COL_GRAVITY_FF]

        if np.any(np.abs(gravity_ff) > 1e-6):
            cos_pos = np.cos(pos)
            valid_mask = np.abs(cos_pos) > 0.1
            if np.any(valid_mask):
                self.results["gravity"] = float(
                    np.median(np.abs(gravity_ff[valid_mask] / cos_pos[valid_mask]))
                )

        if self.results["gravity"] is None:
            low_speed_mask = np.abs(speed) < 0.1
            if np.any(low_speed_mask):
                avg_torque = float(
                    np.mean(valid_data[low_speed_mask, self.COL_TOTAL_FF])
                )
                avg_pos = float(np.mean(pos[low_speed_mask]))
                cos_avg_pos = np.cos(avg_pos)
                if np.abs(cos_avg_pos) > 0.1:
                    self.results["gravity"] = float(np.abs(avg_torque / cos_avg_pos))
                else:
                    self.results["gravity"] = 0.0

        print(f"\n重力参数辨识:")
        print(f"  - 重力参数: {self.results['gravity']:.6f} Nm")

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
        self, csv_path: Optional[str] = None
    ) -> dict[str, Optional[float]]:
        """执行完整辨识流程"""
        self.load_data(csv_path)
        self.estimate_chirp_params()
        self.identify_inertia_and_friction()
        self.identify_coulomb_friction()
        self.identify_gravity()
        self.calculate_snr()
        return self.results

    def print_results_summary(self) -> None:
        """打印辨识结果汇总"""
        print("\n" + "=" * 50)
        print("辨识结果汇总")
        print("=" * 50)
        print(f"惯量 (J):            {self.results['inertia']:.6f} kg*m^2")
        print(f"粘性摩擦 (c_v):      {self.results['friction_viscous']:.6f} Nm*s/rad")
        print(f"库仑摩擦 (c_c):      {self.results['friction_coulomb']:.6f} Nm")
        print(f"重力参数 (gravity):  {self.results['gravity']:.6f} Nm")
        print("-" * 50)
        print(f"扫频起始频率:        {self.results['chirp_start_freq']:.2f} Hz")
        print(f"扫频结束频率:        {self.results['chirp_end_freq']:.2f} Hz")
        print(f"扫频振幅:            {self.results['chirp_amplitude']:.4f} Nm")
        print(f"扫频时长:            {self.results['chirp_duration']:.2f} s")
        print("-" * 50)
        print(f"信噪比 (SNR):        {self.results['snr']:.2f} dB")
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
