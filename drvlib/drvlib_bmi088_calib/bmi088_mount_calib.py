#!/usr/bin/env python3
"""BMI088 安装角辨识（不含 yaw）：静态 VOFA CSV → robot_def.h 的安装矩阵

输入是 drvlib_bmi088_{kalman,mahony} 在 `vofa_enable = 1` 时录到的 CSV
（通道布局见 bmi088_calib.py；本脚本只用 CH4-6 gyro、CH7-9 acc、CH10 dt）。

原理：
    静止时加速度计测的是比力，方向就是"上"（BMI088 手册 §8.3：朝上的轴读 +1g），
    记 u = acc/|acc| 为"上"在 **IMU 系**里的单位矢量。安装矩阵 R（chassis←imu）
    必须满足 R·u = (0,0,1)ᵀ。这是一个矢量约束，只有 2 个自由度 ⇒ **绕竖直轴的
    偏航不可观测**：R→Rz(α)·R 对任何 α 都满足约束。

    所以本脚本在 ZYX 欧拉分解（R = Rz(yaw)·Ry(pitch)·Rx(roll)）下，只辨识
    roll/pitch，yaw 原样保留当前矩阵的值（可用 --yaw-deg 覆盖）。

    ⚠ 静止段包含"地面不平"：算出来的 roll/pitch 是"IMU 相对重力"的倾角，
      地面若不水平会整段混进去，本脚本无法把两者分开。

输出：可直接粘贴回 robot_def.h 的 9 个 CHASSIS_IMU_*_AXIS_* 宏
      （矩阵按列写：第 1 列 = IMU x 轴在底盘系的分量，以此类推）。

用法：
    python3 bmi088_mount_calib.py ignore/vofa+.csv
    python3 bmi088_mount_calib.py static.csv --skip 5 --tail 60
    python3 bmi088_mount_calib.py static.csv --yaw-deg 135 --current-rot ...
"""

import argparse
import sys

import numpy as np

# 通道号 = CSV 列号（第 0 列 I0 是 DWT 时间戳 us）
CH_GYRO = (4, 6)
CH_ACC = (7, 9)
CH_DT = 10

# 与 app/half_rudder_chassis/robot_def.h 同步的"当前"安装矩阵（行主序，chassis←imu）
# 物理含义：x_imu=(1,1,0)/√2、y_imu=(1,-1,0)/√2、z_imu=(0,0,-1)（z 朝下），
# 倾角取 2026-10-29 实测 roll -179.468° / pitch +1.003°，yaw=45° 由机械安装给定。
# yaw 不可观测：本脚本只会改 roll/pitch，yaw 从这里分解出来原样沿用。
DEFAULT_CURRENT_ROT = [
    [0.70699855, 0.70696135, -0.01893962],
    [0.70699855, -0.70719120, -0.00580239],
    [-0.01749600, -0.00928800, -0.99980379],
]


def load(path):
    d = np.loadtxt(path, delimiter=",", skiprows=1)
    if d.ndim == 1:
        d = d[None, :]
    if d.shape[1] <= CH_ACC[1]:
        sys.exit(f"CSV 只有 {d.shape[1]} 列，至少需要 CH9（acc z）。确认 vofa_enable 已打开。")
    return d


def roty(t):
    c, s = np.cos(t), np.sin(t)
    return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])


def rotx(t):
    c, s = np.cos(t), np.sin(t)
    return np.array([[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]])


def rotz(t):
    c, s = np.cos(t), np.sin(t)
    return np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])


def zyx(yaw, pitch, roll):
    """R = Rz(yaw)·Ry(pitch)·Rx(roll)"""
    return rotz(yaw) @ roty(pitch) @ rotx(roll)


def decompose_zyx(R):
    """ZYX 分解；返回 (yaw, pitch, roll)。pitch=±90° 附近是万向锁，本车用不到。"""
    pitch = np.arcsin(np.clip(-R[2, 0], -1.0, 1.0))
    if abs(abs(pitch) - np.pi / 2.0) < 1e-6:
        yaw = 0.0
        roll = np.arctan2(-R[0, 1], R[1, 1])
    else:
        yaw = np.arctan2(R[1, 0], R[0, 0])
        roll = np.arctan2(R[2, 1], R[2, 2])
    return yaw, pitch, roll


def rot_angle(A, B):
    """两个旋转矩阵之间的夹角（rad）"""
    c = (np.trace(A @ B.T) - 1.0) / 2.0
    return float(np.arccos(np.clip(c, -1.0, 1.0)))


def wrap_deg(x):
    """折到 (−180°,180°]，用于显示角度差（roll≈180° 时裸差会跳成 −359°）"""
    return float((x + 180.0) % 360.0 - 180.0)


def main():
    ap = argparse.ArgumentParser(description="BMI088 安装角辨识（重力约束，yaw 不可观测）")
    ap.add_argument("csv", help="VOFA 录的 CSV（vofa_enable=1 的 IMU 标定布局）")
    ap.add_argument("--skip", type=float, default=2.0, help="丢掉开头多少秒（上电自热），默认 2")
    ap.add_argument("--tail", type=float, default=None, help="只用最后多少秒，默认全段")
    ap.add_argument("--gyro-thresh", type=float, default=0.05, help="判静止的角速度阈值 (rad/s)，默认 0.05")
    ap.add_argument("--acc-min", type=float, default=5.0, help="判静止的 |acc| 下限 (m/s²)，默认 5")
    ap.add_argument("--yaw-deg", type=float, default=None, help="覆盖 yaw（度）；默认沿用当前安装矩阵分解值")
    ap.add_argument("--current-rot", nargs=9, type=float, metavar="R",
                    help="当前安装矩阵（行主序 9 个数），默认 robot_def.h 现值")
    a = ap.parse_args()

    d = load(a.csv)
    t = (d[:, 0] - d[0, 0]) * 1e-6
    dt = np.diff(t)
    gy = d[:, CH_GYRO[0]:CH_GYRO[1] + 1]
    ac = d[:, CH_ACC[0]:CH_ACC[1] + 1]

    m = t >= a.skip
    if a.tail is not None:
        m &= t >= (t[-1] - a.tail)
    if m.sum() < 100:
        sys.exit(f"筛选后只剩 {int(m.sum())} 帧，检查 --skip/--tail")

    t, gy, ac = t[m], gy[m], ac[m]
    dtm = np.diff(t) * 1000.0

    # ---- 输入布局自检 ----
    print(f"文件: {a.csv}   帧数 {d.shape[0]}   时长 {t[-1]:.1f}s (筛选后)")
    print(f"帧间隔 dt: 中位 {np.median(dtm):.2f}ms  范围 {dtm.min():.2f}~{dtm.max():.2f}ms")
    if d.shape[1] > CH_DT:
        mdt = d[m, CH_DT]
        print(f"模块 dt(CH10): 中位 {np.median(mdt):.3f}ms  0 占比 {np.mean(mdt == 0) * 100:.0f}%（0=该帧无新陀螺样本）")
    nrm = np.linalg.norm(ac, axis=1)
    if not (8.0 < np.median(nrm) < 11.0):
        print(f"⚠ |acc| 中位 {np.median(nrm):.2f} m/s² 不像 1g → CH7-9 可能不是加速度，先核对布局")

    # ---- 静止段筛选：陀螺小 + |acc| 正常 ----
    still = (np.linalg.norm(gy, axis=1) < a.gyro_thresh) & (nrm > a.acc_min)
    frac = still.mean() * 100.0
    print(f"静止帧占比: {frac:.1f}%   (|gyro| < {a.gyro_thresh} rad/s, |acc| > {a.acc_min})")
    if frac < 50.0:
        print("⚠ 静止帧不足一半：录制期间可能有人碰/车在动，结果按剩余帧算")

    ac_s = ac[still]
    gy_s = gy[still]
    u = ac_s.mean(axis=0)
    acc_n = float(np.linalg.norm(u))
    u = u / acc_n
    # 逐帧单位矢量散布 → 角度噪声（1σ），用于判断倾角是否显著
    us = ac_s / np.linalg.norm(ac_s, axis=1)[:, None]
    spread = float(np.degrees(np.arccos(np.clip(np.mean(np.sum(us * u, axis=1)), -1.0, 1.0))))

    print(f"\n静止段 acc 均值: [{u[0]:+.6f} {u[1]:+.6f} {u[2]:+.6f}] m/s² (|acc|={acc_n:.4f})")
    print(f"  单位化(=IMU 系里的“上”): [{u[0]:+.6f} {u[1]:+.6f} {u[2]:+.6f}]   散布 {spread:.3f}°")
    print(f"静止段 gyro 均值(残差): [{gy_s[:, 0].mean():+.6f} {gy_s[:, 1].mean():+.6f} {gy_s[:, 2].mean():+.6f}] rad/s")

    # ---- 重力只给 roll/pitch ----
    # R = Rz·Ry·Rx 的第三行 = [−sinθ, cosθ·sinφ, cosθ·cosφ] = R^T·e_z = u
    pitch = np.arctan2(-u[0], np.hypot(u[1], u[2]))
    roll = np.arctan2(u[1], u[2])
    tilt = np.degrees(np.arccos(np.clip(-u[2], -1.0, 1.0)))  # 与“z 朝正上方”的夹角

    R_old = np.array(a.current_rot, dtype=float).reshape(3, 3) if a.current_rot else np.array(DEFAULT_CURRENT_ROT)
    yaw_old, pitch_old, roll_old = decompose_zyx(R_old)
    yaw = np.radians(a.yaw_deg) if a.yaw_deg is not None else yaw_old

    print("\n── ZYX 角度（R = Rz(yaw)·Ry(pitch)·Rx(roll)）──")
    print(f"  roll  = {np.degrees(roll):+9.3f}°   (当前 {np.degrees(roll_old):+9.3f}°, "
          f"Δ={wrap_deg(np.degrees(roll - roll_old)):+7.3f}°)")
    print(f"  pitch = {np.degrees(pitch):+9.3f}°   (当前 {np.degrees(pitch_old):+9.3f}°, "
          f"Δ={wrap_deg(np.degrees(pitch - pitch_old)):+7.3f}°)")
    print(f"  yaw   = {np.degrees(yaw):+9.3f}°   ← 重力观测不到，沿用当前矩阵分解值"
          f"{'（--yaw-deg 覆盖）' if a.yaw_deg is not None else ''}")
    print(f"  合倾角（IMU 的 -z 与竖直方向夹角）= {tilt:.3f}°   (逐帧散布 {spread:.3f}°)")
    # 机械解释：IMU 的 +z（名义朝下）相对真实竖直偏了多少，用 IMU 自己的轴表示
    print(f"  IMU 的 +z（名义朝下）相对竖直: 向 +x_imu 偏 {np.degrees(np.arctan2(-u[0], -u[2])):+.3f}°，"
          f"向 +y_imu 偏 {np.degrees(np.arctan2(-u[1], -u[2])):+.3f}°")

    # ---- 重建矩阵 ----
    R_new = zyx(yaw, pitch, roll)
    print("\n── 自检 ──")
    ru = R_new @ u
    print(f"  R_new·u = [{ru[0]:+.6f} {ru[1]:+.6f} {ru[2]:+.6f}]  (应为 0,0,1)")
    print(f"  det(R_new) = {np.linalg.det(R_new):+.9f}   |R·Rᵀ-I|max = {np.abs(R_new @ R_new.T - np.eye(3)).max():.2e}")
    print(f"  相对当前矩阵的修正角 = {np.degrees(rot_angle(R_new, R_old)):.3f}°")
    if np.degrees(rot_angle(R_new, R_old)) < 3 * spread:
        print("  ⚠ 修正角与噪声同量级：这次数据不足以说明安装角与现有值有差别")

    # 宏按列写：第 j 列 = IMU 第 j 轴在底盘系的分量
    names = [("X", 0), ("Y", 1), ("Z", 2)]
    print("\n── 粘贴到 robot_def.h（只改旋转，CHASSIS_IMU_POS_* 位置项不动）──")
    for imu_ax, j in names:
        for ch_ax, i in names:
            print(f"#define CHASSIS_IMU_{imu_ax}_AXIS_{ch_ax} ({R_new[i, j]:+.8f}f)")

    print("\n备注：1) yaw 不可观测，本次沿用当前值；2) 地面不水平会整段计入倾角；"
          "\n      3) 修正后请把 R_new 的列读回 robot_def.h，重烧后再看 CH1~3 的姿态是否合理。")


if __name__ == "__main__":
    main()
