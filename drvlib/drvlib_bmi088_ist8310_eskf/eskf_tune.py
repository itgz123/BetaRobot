#!/usr/bin/env python3
"""
eskf_tune.py — drvlib_bmi088_ist8310_eskf 的实机日志分析与调参

输入：VOFA+ 存的 CSV（JustFloat 25 通道）。列布局与本模块的 VOFA 通道表一一对应
      （见 drvlib_bmi088_ist8310_eskf.h「VOFA 通道表」）：
        I0        = VOFA+ 自己的时间列（µs，可能回绕）
        I1..I3    = CH1-3   roll/pitch/yaw (rad)
        I4..I6    = CH4-6   陀螺 (rad/s，已标定)
        I7..I9    = CH7-9   加速度 (m/s²，已标定)
        I10       = CH10    dt (ms)
        I11       = CH11    yaw_rate (rad/s)
        I12       = CH12    温度 (℃，可能 nan)
        I13..I15  = CH13-15 bias = 标定 + 温漂 + ESKF 残余
        I16..I19  = CH16-19 名义四元数 w/x/y/z（机体系→世界系）
        I20..I22  = CH20-22 标定后磁矢量 (µT)
        I23       = CH23    mag_used
        I24       = CH24    acc 残差模长
        I25       = CH25    mag 残差模长

脚本自检输入布局（四元数模长、dt 量级），布局不符直接报错退出，避免"读串列还出结论"。

用法：
    python3 eskf_tune.py <log.csv>              # 全部分析 + 调参建议
    python3 eskf_tune.py <log.csv> --section drift
    python3 eskf_tune.py <log.csv> --quiet 5    # 静态段判定阈值 (rad/s)

依赖 numpy。
"""

import argparse
import sys

import numpy as np

# ---------------------------------------------------------------- 列布局

COL_T = 0
COL_ROLL, COL_PITCH, COL_YAW = 1, 2, 3
COL_GYRO = slice(4, 7)
COL_ACC = slice(7, 10)
COL_DT = 10
COL_YAW_RATE = 11
COL_TEMP = 12
COL_BIAS = slice(13, 16)
COL_QUAT = slice(16, 20)
COL_MAG = slice(20, 23)
COL_MAG_USED = 23
COL_ACC_RESID = 24
COL_MAG_RESID = 25
N_COL = 26

EARTH_G = 9.80665

# ---------------------------------------------------------------- 固件当前参数
# 与 app/half_rudder_gimbal/app_gimbal/app_gimbal.c 里的取值同步（改了那边就改这里，
# 否则本脚本"配置里写的是 XX"的判据就在跟一个不存在的固件比）。
# IGRF-14 真值由 ppigrf 实算：长沙理工大学 28.16N/112.98E @2026-09-28
#   → I = +44.07°、D = -4.13°、总场 F = 48.96 µT；实测 |m| = 56.4 µT（差值 = 未标硬铁）
CFG_MAG_INC_DEG = 44.07
CFG_MAG_DEC_DEG = -4.13
CFG_MAG_REF_UT = 49.0
# 磁计→IMU 安装旋转（行主序），与 s_ist8310_calib.mount_rot 同值：绕 IMU 的 Y 轴转 180°
CFG_MOUNT_ROT = np.array([[-1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, -1.0]])
# mag 更新的三道门限（app 里 .mag_reject = 0.0 → 0.3·49 与 15 取大 = 15）
CFG_MAG_REJECT_UT = max(0.3 * CFG_MAG_REF_UT, 15.0)
CFG_MAG_DIP_MIN = 0.15
# 日志里 CH20-22 是 IST8310MagCorrect **之后**的量：若记录了 mount_rot，它已在 IMU 系。
# 由此可反推记录时固件用的 mount_rot —— 判据见 sec_interf。
CFG_LOG_MAG_IS_IMU_FRAME = None  # None = 未知，由数据反推


# ---------------------------------------------------------------- 小工具


def wrap_pi(a):
    """折到 (-pi, pi]"""
    return (a + np.pi) % (2.0 * np.pi) - np.pi


def quat_rotate(q, v):
    """q ⊗ v ⊗ q*（与 Lib_Math_QuatRotateVector 同约定：q 为 机体系→世界系）。

    q: (N,4) [w,x,y,z]；v: (N,3)。返回 (N,3) 世界系矢量。

    ⚠ 全程用 (N,) 视图做逐元素运算：若拿 (N,1) 去乘 (N,)，numpy 会广播成 (N,N)，
      本文件 5.9 万行就是 26 GiB —— 之前两次把机器 OOM 掉的就是这里。
    """
    w0, x0, y0, z0 = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
    vx, vy, vz = v[:, 0], v[:, 1], v[:, 2]
    # t = 2 * (qv × v)
    tx = 2.0 * (y0 * vz - z0 * vy)
    ty = 2.0 * (z0 * vx - x0 * vz)
    tz = 2.0 * (x0 * vy - y0 * vx)
    # v' = v + w*t + qv × t
    out = np.empty_like(v)
    out[:, 0] = vx + w0 * tx + (y0 * tz - z0 * ty)
    out[:, 1] = vy + w0 * ty + (z0 * tx - x0 * tz)
    out[:, 2] = vz + w0 * tz + (x0 * ty - y0 * tx)
    return out


def unit(v, eps=1e-9):
    n = np.linalg.norm(v, axis=1, keepdims=True)
    return v / np.maximum(n, eps), n[:, 0]


def stats(name, a, unit_str="", fmt="%.4g"):
    a = np.asarray(a, dtype=float)
    a = a[np.isfinite(a)]
    if a.size == 0:
        print(f"    {name:<28} 无有效样本")
        return
    print(
        f"    {name:<28} mean={fmt % a.mean():>10} std={fmt % a.std():>10}"
        f" min={fmt % a.min():>10} max={fmt % a.max():>10} {unit_str}"
    )


def pct(name, mask):
    print(f"    {name:<28} {100.0 * np.count_nonzero(mask) / mask.size:6.2f}%  ({np.count_nonzero(mask)}/{mask.size})")


# ---------------------------------------------------------------- 载入


def load(path):
    # loadtxt 而非 genfromtxt：同结果、更快更省（本文件 43 MB 峰值 / 0.2 s）
    raw = np.loadtxt(path, delimiter=",", skiprows=1, dtype=float)
    if raw.ndim == 1:
        raw = raw[None, :]
    if raw.shape[1] != N_COL:
        raise SystemExit(f"列数不符：期望 {N_COL} 列（I0 + CH1..CH25），实得 {raw.shape[1]} 列")

    # ---- 布局自检：四元数模长必须≈1，dt 必须在 0~几十 ms 量级 ----
    q = raw[:, COL_QUAT]
    qnorm = np.linalg.norm(q, axis=1)
    finite_q = np.count_nonzero(np.isfinite(qnorm))
    if finite_q < 0.5 * raw.shape[0]:
        raise SystemExit("布局自检失败：CH16-19 大多不是有限值，列偏移可能不对")
    ok_q = np.abs(qnorm - 1.0) < 0.05
    if np.count_nonzero(ok_q) < 0.5 * finite_q:
        raise SystemExit(
            f"布局自检失败：CH16-19 模长只有 {100.0 * np.count_nonzero(ok_q) / finite_q:.1f}% 接近 1，"
            "不像四元数；确认 CSV 是否含 I0 时间列"
        )
    dt = raw[:, COL_DT]
    dt_ok = dt[np.isfinite(dt)]
    if dt_ok.size and (np.median(dt_ok) < 0.0 or np.median(dt_ok) > 1000.0):
        raise SystemExit(f"布局自检失败：CH10(dt) 中位数 {np.median(dt_ok)} ms 不合理")

    # ---- 丢掉非有限行 + 荒谬行 ----
    # CH12 温度上电初期就是 nan，不算作废；首帧常是"半帧"，会出现 1e23 这种量级
    # （本文件第 1 行 CH23/CH24 就是），必须按量级剔掉，否则均值全被它带飞
    cols = [i for i in range(1, N_COL) if i != COL_TEMP]
    finite = np.all(np.isfinite(raw[:, cols]), axis=1)
    sane = np.max(np.abs(raw[:, cols]), axis=1) < 1.0e4
    use = finite & sane
    n_raw = raw.shape[0]
    n_bad = n_raw - np.count_nonzero(use)
    n_garbage = np.count_nonzero(finite & ~sane)
    return raw[use], (n_bad, n_garbage), n_raw


# ---------------------------------------------------------------- 各节


def sec_overview(d, t, drop_info, n_raw):
    n_drop, n_garbage = drop_info
    print("── ① 帧与时间 ──")
    print(f"    有效帧 {d.shape[0]} / 原始 {n_raw}（剔除 {n_drop} 行，其中量级荒谬 {n_garbage} 行）")
    mono = np.count_nonzero(np.diff(t) > 0)
    print(f"    I0 单调递增占比 {100.0 * mono / max(1, t.size - 1):6.2f}%（不足 100% = 回绕或被丢帧）")
    span = t[-1] - t[0]
    print(f"    I0 跨度 {span:.0f} µs = {span * 1e-6:.2f} s")

    dt = d[:, COL_DT]
    stats("dt", dt, "ms", "%.3f")
    temp = d[:, COL_TEMP]
    n_nan = np.count_nonzero(~np.isfinite(temp))
    print(f"    温度 CH12：nan {n_nan} 帧（{100.0 * n_nan / temp.size:.2f}%）"
          + (f"，有效值 mean={np.nanmean(temp):.2f}℃ 范围 {np.nanmin(temp):.2f}~{np.nanmax(temp):.2f}℃"
             if n_nan < temp.size else ""))
    over = dt > 10.0
    print(f"    dt > 10ms（会被 dt_max 钳位）占比 {100.0 * np.count_nonzero(over) / dt.size:6.3f}%")
    # 以 dt 累计的真实时间轴（比 I0 可靠）
    tsec = np.cumsum(dt) * 1e-3
    print(f"    按 dt 累计的时长 {tsec[-1]:.2f} s")
    return tsec


def sec_attitude(d):
    print("\n── ② 姿态 ──")
    for i, n in enumerate(("roll", "pitch", "yaw")):
        stats(n, np.degrees(d[:, COL_ROLL + i]), "°", "%.3f")
    qn = np.linalg.norm(d[:, COL_QUAT], axis=1)
    stats("|四元数|", qn, "", "%.6f")
    # 姿态角速度：差分
    tsec = np.cumsum(d[:, COL_DT]) * 1e-3
    if d.shape[0] > 10:
        rate = np.degrees(np.abs(np.diff(d[:, COL_YAW], prepend=d[0, COL_YAW]))) / np.maximum(np.diff(tsec, prepend=tsec[0]), 1e-6)
        stats("|dyaw/dt|", rate, "°/s", "%.3f")


def sec_gyro_acc(d, quiet_thr):
    print("\n── ③ 陀螺 / 加速度计（判静态段） ──")
    gyro = d[:, COL_GYRO]
    gmag = np.linalg.norm(gyro, axis=1)
    stats("|gyro|", gmag, "rad/s", "%.5f")
    # 逐轴均值才是"标定零偏还剩多少"的直接证据：零偏扣干净时静止段均值应≈0。
    # 只看模长会把三轴噪声混在一起，看不出哪一轴没标好
    for i, ax in enumerate("xyz"):
        p = np.percentile(gyro[:, i], [5, 95])
        print(f"    gyro_{ax}: mean={gyro[:, i].mean():+.6f} std={gyro[:, i].std():.6f}"
              f"  p5~p95=[{p[0]:+.6f}, {p[1]:+.6f}] rad/s"
              f"  (≈{np.degrees(gyro[:, i].mean()):+.3f} °/s)")
    quiet, thr = still_mask(d)
    pct(f"静态帧（残余速率<{thr:.5f} rad/s）", quiet)

    acc, an = unit(d[:, COL_ACC])
    stats("|acc|", an, "m/s²", "%.4f")
    stats(f"|acc| - g (g={EARTH_G})", an - EARTH_G, "m/s²", "%.4f")
    print(f"    |Δ|acc|| < 3.0 (acc_reject) 占比 {100.0 * np.count_nonzero(np.abs(an - EARTH_G) < 3.0) / an.size:6.2f}%")
    stats("acc 残差模长 (CH24)", d[:, COL_ACC_RESID], "", "%.5f")

    bias = d[:, COL_BIAS]
    print("    ---- bias 通道（= 标定值 + 温漂 + ESKF 残余）在静态段上的均值 ----")
    if np.count_nonzero(quiet):
        b = bias[quiet]
        for i, ax in enumerate("xyz"):
            print(f"      b{ax}: mean={b[:, i].mean():+.6f} std={b[:, i].std():.6f} min={b[:, i].min():+.6f} max={b[:, i].max():+.6f} rad/s")
        print(f"      静态段 |gyro| 均值 = {gmag[quiet].mean():.6f} rad/s")
        print("      ↑ 若 |gyro| 均值明显大于 0，说明还有残余零偏（bias 通道没吃掉的部分）——")
        print("        温漂未标（bias_tempco 全 0）时，温度变化会让这个残余漂移")
    return quiet


def sec_mag(d, quiet):
    print("\n── ④ 磁计 ──")
    used = d[:, COL_MAG_USED] > 0.5
    pct("mag_used=1", used)
    if not used.any():
        print("    ⚠ 全程没有一帧用上磁！yaw 等于纯六轴。先查 CH23 恒 0 的原因：")
        print("      · CH20-22 模长是否 ≈ mag_ref_uT？偏离超 mag_reject 会被跳过")
        print("      · 或 |z_m × z_a| < mag_dip_min（磁与重力近平行）——见下面的夹角")
        print("      · 或 mag_new 恒 0（IST8310 时间戳不推进，链路没活）")
    mm, mn = unit(d[:, COL_MAG])
    stats("|mag|（标定后）", mn, "µT", "%.3f")
    stats("mag 残差模长 (CH25)", d[:, COL_MAG_RESID], "", "%.5f")
    # 残差只在"过了门限、真的调了 EskfUpdateM"的帧上才刷新，必须分组看，
    # 否则被大量陈旧值稀释，看不出真实量级
    mr = d[:, COL_MAG_RESID]
    for lab, m in (("mag_used=1", used), ("mag_used=0", ~used)):
        if np.count_nonzero(m):
            v = mr[m]
            print(f"      CH25 在 {lab} 帧上：mean={v.mean():.5f} std={v.std():.5f} "
                  f"min={v.min():.5f} max={v.max():.5f}  → 夹角≈{np.degrees(2 * np.arcsin(np.clip(v.mean() / 2, 0, 1))):.2f}°")
    ar = d[:, COL_ACC_RESID]
    stats("acc 残差模长 (CH24)", ar, "", "%.5f")
    print(f"      → 夹角≈{np.degrees(2 * np.arcsin(np.clip(ar.mean() / 2, 0, 1))):.2f}°"
          f"（0.01 以下才算 acc 与姿态自洽）")

    # 与重力的夹角
    acc, _ = unit(d[:, COL_ACC])
    dot = np.clip(np.sum(mm * acc, axis=1), -1.0, 1.0)
    ang = np.degrees(np.arccos(dot))
    stats("夹角(z_m, z_a)", ang, "°", "%.3f")
    print(f"    |z_m × z_a| = sin(夹角) 最小值 = {np.sin(np.radians(ang)).min():.4f}（mag_dip_min 门限 0.15）")
    print("    ⚠ 这个夹角是**与姿态无关**的物理量：任何机体旋转下 z_m 与 z_a 一起转，夹角不变。")
    print("      所以它只由两件事决定：真实磁倾角 + 磁计↔IMU 的安装旋转。")
    print("      符号约定：acc 静止时测的是比力，指向**上**（≈世界 +Z）；磁矢量在北半球指向**下**，")
    print("      与重力轴成 I 角。故 angle(z_a, z_m) = 90° + I，即 I = 夹角 − 90°。")
    # 套上固件里的安装旋转后再比 —— 这才是滤波器真正看到的量
    mrot = CFG_MOUNT_ROT @ mm.T
    mrot, _ = unit(mrot.T)
    ang2 = np.degrees(np.arccos(np.clip(np.sum(mrot * acc, axis=1), -1.0, 1.0)))
    print(f"      未套 mount_rot          ：{ang.mean():6.2f}° → 隐含 I = {ang.mean() - 90.0:+7.2f}°")
    print(f"      套上 mount_rot（固件现值）：{ang2.mean():6.2f}° → 隐含 I = {ang2.mean() - 90.0:+7.2f}°")
    print(f"      IGRF 真值               ：        I = {CFG_MAG_INC_DEG:+7.2f}°"
          f"（F = {CFG_MAG_REF_UT:.1f} µT，D = {CFG_MAG_DEC_DEG:+.2f}°）")
    resid = (ang2.mean() - 90.0) - CFG_MAG_INC_DEG
    print(f"      → 模型一致性残差 = {resid:+.2f}°"
          f"　{'★ 已对上（<1°），mount_rot 与 mag_inclination 都对' if abs(resid) < 1.0 else '⚠ 还没对上，检查 mount_rot / mag_inclination'}")
    print("      残差 <1° 说明安装旋转与倾角都填对了：滤波器在该模型下的 acc/mag 稳态残差"
          "就是由这一点点不一致平摊出来的。")
    print("      ⚠ 但硬铁造成的**方向**偏差本判据看不见（它随姿态变，静态日志里是个常数偏置）——"
          "补标硬铁前，mag 的真实方向误差可能还有几度。")
    return used


def cube_rotations():
    """立方体的 24 个固有旋转（det=+1 的带符号置换阵），代表所有"轴对齐"安装姿态。

    约定：返回的 R 把**磁计机体系**的矢量映到 **IMU 机体系**（z_imu = R·z_mag），
    正是 IST8310Calib_s.mount_rot 的语义。
    """
    import itertools

    out = []
    for perm in itertools.permutations(range(3)):
        for signs in itertools.product((1.0, -1.0), repeat=3):
            m = np.zeros((3, 3))
            for i in range(3):
                m[i, perm[i]] = signs[i]
            if abs(np.linalg.det(m) - 1.0) < 1e-9:
                out.append(m)
    return out


def _ang_deg(a, v):
    """逐帧夹角（度）；a、v 都是 (N,3) 单位矢量"""
    return np.degrees(np.arccos(np.clip(np.sum(a * v, axis=1), -1.0, 1.0)))


def sec_mount(d, quiet):
    """枚举安装姿态：真安装下 angle(z_a, z_m) 必须是**常数**。

    判据与滤波器无关、也不需要姿态：angle(z_a, z_m) 只有在两个矢量**同处机体系**时才是
    不变量。所以对每个候选 R 算 ang_k = angle(a_k, R·m_k)：
      · 真安装 → 姿态怎么动 ang_k 都不变（离散度≈0），且 ang−90° 就是当地磁倾角 I ——
        这个 I 能拿 IGRF 独立验证。两个条件一起用，比只看均值硬得多；
      · 错安装 → 姿态一动 ang_k 就跟着变，离散度大。
    静止日志里姿态只动了约 1°，判据退化（所有候选离散度都≈0），只能靠「哪个隐含 I 对得
    上当地纬度」筛 —— 第一段日志就是这么定出 180° 翻转的。带姿态多样性的运动日志才能真正
    分辨，也才有资格谈「轴对齐假设准不准」。
    """
    print("\n── ⑥ 安装旋转排查（磁计↔IMU；按「角度是否恒定」判，不依赖滤波器） ──")
    g_tol = 1.0     # m/s²：|acc| 偏离 1g 在此以内的帧，重力方向才可信
    gyr_max = 30.0  # °/s：大角速率时 acc 有线性加速度污染
    an = np.linalg.norm(d[:, COL_ACC], axis=1)
    gn = np.degrees(np.linalg.norm(d[:, COL_GYRO], axis=1))
    sel = (np.abs(an - EARTH_G) < g_tol) & (gn < gyr_max)
    print(f"    判定用帧（|acc|≈g±{g_tol} 且 |gyro|<{gyr_max:.0f}°/s）："
          f"{np.count_nonzero(sel)} / {d.shape[0]}")
    if np.count_nonzero(sel) < 50:
        print("    ⚠ 可用帧太少（<50），判不出来 —— 需要一段姿态有变化的日志")
        return
    a, _ = unit(d[sel][:, COL_ACC])
    m, _ = unit(d[sel][:, COL_MAG])
    a0 = a.mean(axis=0)
    a0 /= np.linalg.norm(a0)
    m0 = m.mean(axis=0)
    m0 /= np.linalg.norm(m0)
    print(f"    姿态均值参考：z_a=({a0[0]:+.3f},{a0[1]:+.3f},{a0[2]:+.3f})"
          f"  z_m=({m0[0]:+.3f},{m0[1]:+.3f},{m0[2]:+.3f})")

    rows = []
    for r in cube_rotations():
        ang = _ang_deg(a, (r @ m.T).T)
        rows.append((ang.std(), ang.mean() - 90.0, r))
    rows.sort(key=lambda x: x[0])
    print(f"    {'离散度':>8} {'隐含I':>8} {'ΔIGRF':>8}   全部 24 个候选（离散度越小越像真安装）")
    for sd, inc, r in rows:
        tag = "" if abs(inc - CFG_MAG_INC_DEG) < 2.0 else "   ← 倾角对不上"
        print(f"    {sd:8.3f}° {inc:+8.2f}° {inc - CFG_MAG_INC_DEG:+8.2f}°   "
              f"[[{r[0][0]:+.0f},{r[0][1]:+.0f},{r[0][2]:+.0f}],"
              f"[{r[1][0]:+.0f},{r[1][1]:+.0f},{r[1][2]:+.0f}],"
              f"[{r[2][0]:+.0f},{r[2][1]:+.0f},{r[2][2]:+.0f}]]{tag}")
    print("    ↑ 只看离散度最小的几个；每个都要**同时**满足「离散度小」与「隐含 I ≈ 当地 IGRF」，")
    print("      只满足一个的候选是假解（静止日志里两者会退化成同一个条件）。")

    if rows[1][0] > 5.0 * rows[0][0] and rows[0][0] < 2.0:
        print(f"    ★ 判据很干净：最优 {rows[0][0]:.3f}° vs 次优 {rows[1][0]:.3f}°"
              f"（差 {rows[1][0] / max(rows[0][0], 1e-6):.0f} 倍），安装唯一确定。")
    else:
        print(f"    ⚠ 最优 {rows[0][0]:.3f}° 与次优 {rows[1][0]:.3f}° 区分度不足："
              f"日志的姿态变化还不足以唯一确定安装，需要绕**不同轴**各转一圈。")
    print(f"    残余不一致 {rows[0][0]:.3f}° 的含义：这就是「轴对齐假设」在本次姿态范围内撑出的")
    print("      摆动量，里面混着磁计噪声 + 未标硬铁 + 真实安装的失准。要分离出「失准了多少」")
    print("      必须在**已扣掉硬铁**的磁数据上做连续拟合，否则拟出来的角度会把硬铁误差也吃掉。")


def still_mask(d):
    """稳健判静态帧：各轴陀螺扣掉自身的全段中位数后，残余速率很小的帧。

    不用固定阈值 —— 标定零偏没吃干净时整条曲线都抬着，固定阈值会把"静止"全判成"运动"。
    """
    g = d[:, COL_GYRO]
    res = g - np.median(g, axis=0, keepdims=True)
    rate = np.linalg.norm(res, axis=1)
    thr = max(3.0 * np.median(rate) + 1e-4, 0.002)
    return rate < thr, thr


def sec_bins(d, used, bin_s):
    print(f"\n── ⑥ 时间分段表（{bin_s:.0f}s 一段） ──")
    tsec = np.cumsum(d[:, COL_DT]) * 1e-3
    yaw = np.unwrap(d[:, COL_YAW])
    gmag = np.linalg.norm(d[:, COL_GYRO], axis=1)
    mn = np.linalg.norm(d[:, COL_MAG], axis=1)
    mr = d[:, COL_MAG_RESID]
    ar = d[:, COL_ACC_RESID]
    print("      区间        帧数  mag_used%  |mag|µT  yaw速率°/s  |gyro|rad/s  acc残差  mag残差")
    nb = int(np.ceil(tsec[-1] / bin_s))
    for b in range(nb):
        sl = (tsec >= b * bin_s) & (tsec < (b + 1) * bin_s)
        if np.count_nonzero(sl) < 5:
            continue
        yy = yaw[sl] - yaw[sl][0]
        rate = np.polyfit(tsec[sl] - tsec[sl][0], yy, 1)[0] if yy.size > 3 else 0.0
        print(
            f"    {b * bin_s:6.1f}~{min((b + 1) * bin_s, tsec[-1]):6.1f}s {np.count_nonzero(sl):6d}"
            f"  {100.0 * np.count_nonzero(used[sl]) / np.count_nonzero(sl):7.1f}%"
            f"  {mn[sl].mean():7.2f}"
            f"  {np.degrees(rate):+10.4f}"
            f"  {gmag[sl].mean():10.5f}"
            f"  {ar[sl].mean():7.4f}  {mr[sl].mean():7.4f}"
        )
    print("      ↑ yaw速率≈0 而 mag_used% 高 → 磁计在压住 yaw；mag_used%=0 时 yaw速率就是纯陀螺漂移")


def sec_world_ref(d, quiet):
    """由数据反推世界系磁参考（倾角/偏角/场强）—— 这是最该照实填的一组参数。"""
    print("\n── ⑤ 世界系磁参考反推（倾角/偏角/场强） ──")
    n = d.shape[0]
    if not np.count_nonzero(quiet):
        print("    ⚠ 没有静态帧（|gyro| 全大），跳过——转起来的时候 acc 不准，反推不可信")
        return
    sel = quiet & np.all(np.isfinite(d[:, COL_QUAT]), axis=1)
    if np.count_nonzero(sel) < 20:
        print("    ⚠ 静态帧不足 20，跳过")
        return

    q = d[sel][:, COL_QUAT]
    q, _ = unit(q)
    mag = d[sel][:, COL_MAG]
    mu, mn = unit(mag)
    # 磁矢量转到世界系：m_w = R(q)·z_m
    mw = quat_rotate(q, mu)
    mw_mu, _ = unit(mw)
    # m_w = [cosI·cosD, -cosI·sinD, -sinI]
    inc = np.degrees(np.arcsin(np.clip(-mw_mu[:, 2], -1.0, 1.0)))
    dec = np.degrees(np.arctan2(-mw_mu[:, 1], mw_mu[:, 0]))
    inc_m = np.degrees(np.arctan2(-mw_mu[:, 2], np.hypot(mw_mu[:, 0], mw_mu[:, 1])))
    print(f"    静态帧数 {np.count_nonzero(sel)}")
    print("    ⚠ 下面这个倾角是用**滤波自己输出的四元数**（CH16-19）把磁矢量转回世界系算的。")
    print("      模型不对时滤波器会给出「自洽但错误」的姿态，本段就跟着报出一个看着很合理、")
    print("      其实错掉的倾角 —— 与最初把北京 58° 填进长沙是同一类陷阱。acc/mag 残差")
    print("      （CH24/CH25）还大的时候别信这一段；与滤波器无关的倾角在 ⑥ 段用不变量算。")
    stats("倾角 I（+下，滤波四元数反推）", inc_m, "°", "%.3f")
    # 偏角有 ±180 回绕：先折到相对均值的偏差再统计离散度
    dec_ref = dec[0]
    stats("偏角 D（+东）", wrap_pi(np.radians(dec - dec_ref)), "rad(相对首帧)", "%.4f")
    stats("|mag| 均值", mn, "µT", "%.3f")
    print("    → mag_declination 本日志只能定到「相对首帧 yaw 的偏置」；")
    print("      若首帧朝向未知，需配合已知真北/视觉世界系才能给出绝对值。")
    print(f"      当前偏差 = {np.degrees(wrap_pi(np.radians(dec - dec_ref))).mean():+.2f}°（含播种时的 yaw 基准）")

    # 与滤波器无关的交叉校验：套上固件里的 mount_rot 后，夹角必须对上 90°+I
    acc, _ = unit(d[sel][:, COL_ACC])
    mrot = CFG_MOUNT_ROT @ mu.T
    mrot, _ = unit(mrot.T)
    ang = np.degrees(np.arccos(np.clip(np.sum(mrot * acc, axis=1), -1.0, 1.0)))
    i_inv = ang.mean() - 90.0
    print(f"    交叉校验（不依赖滤波器）：套用固件 mount_rot 后 angle(z_a, z_m) = {ang.mean():.2f}°"
          f" → I = {i_inv:+.2f}°")
    print(f"      对比 IGRF 真值 {CFG_MAG_INC_DEG:+.2f}°：差 {i_inv - CFG_MAG_INC_DEG:+.2f}°"
          f"　{'★ 对上了（<1°）' if abs(i_inv - CFG_MAG_INC_DEG) < 1.0 else '⚠ 对不上'}")
    print("      上面滤波反推的 I 若与它差得远，那正是「模型错 → 姿态错 → 反推也跟着错」的连锁；"
          "以本行为准。")


def sec_drift(d, used):
    print("\n── ⑥ yaw 漂移（静止段线性回归，看磁计到底有没有起作用） ──")
    tsec = np.cumsum(d[:, COL_DT]) * 1e-3
    yaw = np.unwrap(d[:, COL_YAW])
    dt = d[:, COL_DT] * 1e-3
    # 找连续静止段
    gmag = np.linalg.norm(d[:, COL_GYRO], axis=1)
    quiet = gmag < 0.02
    segs = []
    i = 0
    n = len(quiet)
    while i < n:
        if quiet[i]:
            j = i
            while j < n and quiet[j]:
                j += 1
            if tsec[j - 1] - tsec[i] > 1.0:
                segs.append((i, j))
            i = j
        else:
            i += 1
    if not segs:
        print("    没找到 >1s 的连续静止段")
        return
    print(f"    找到 {len(segs)} 段 >1s 静止段")
    for (i, j) in segs[:8]:
        dur = tsec[j - 1] - tsec[i]
        sl = slice(i, j)
        mu = np.count_nonzero(used[sl]) > 0.5 * (j - i)
        p = np.polyfit(tsec[sl] - tsec[i], np.degrees(yaw[sl]), 1)
        print(
            f"      t={tsec[i]:7.2f}s..{tsec[j - 1]:7.2f}s ({dur:5.1f}s, mag_used={'Y' if mu else 'N'})"
            f"  yaw={np.degrees(yaw[i]):+8.3f}°→{np.degrees(yaw[j - 1]):+8.3f}°"
            f"  漂移 {p[0]:+.4f} °/s ({p[0] * 60:+.3f} °/min)"
        )


def sec_advice(d, quiet, used):
    print("\n── ⑦ 调参建议（照上面实测值给，不是泛泛而谈） ──")
    dt = d[:, COL_DT]
    n_over = np.count_nonzero(dt > 10.0)
    acc_res = d[:, COL_ACC_RESID]
    mag_res = d[:, COL_MAG_RESID]
    mm, mn = unit(d[:, COL_MAG])
    an = np.linalg.norm(d[:, COL_ACC], axis=1)

    print(f"    1) dt_max = 0.01s：本日志超 10ms 的帧 {n_over} 个（{100.0 * n_over / dt.size:.3f}%）"
          f"，{'可保持' if n_over < 0.001 * dt.size else '⚠ 偏多，说明任务有卡顿，先查任务周期'}")

    if np.count_nonzero(quiet):
        arm = np.abs(an - EARTH_G)[quiet]
        print(f"    2) acc_reject = 3.0 m/s²：静止段 |‖acc‖-g| 的 p99 = {np.percentile(arm, 99):.3f} m/s²"
              f"，{'门限合适' if np.percentile(arm, 99) < 3.0 else '⚠ 静止段都快超门限了，说明振动/线性加速度大'}")
        ar = acc_res[quiet]
        ar_sig = ar.std()
        print(f"       静止段 acc 残差 std = {ar_sig:.4f} → 方向噪声 σ ≈ {np.degrees(ar_sig):.2f}°"
              f" → 噪声下限 r_acc ≈ {max(ar_sig ** 2, 1e-6):.2e}")
    else:
        ar_sig = None
        print("    2) 无静止段，acc_reject/r_acc 不好判")

    # 把 r 与时间常数连起来的那个公式 —— 别看 r 的绝对值下结论
    dt_s = np.median(dt[dt > 0.0]) * 1e-3 if np.any(dt > 0.0) else 2e-3
    qa = 1e-6  # 与固件 q_att 同值
    print(f"       ⚠ r 不是「越小越准」：稳态时间常数 τ = √(r·dt/q_att)（本模块 Q=q_att·dt、R=r·I₃）。")
    print(f"         dt≈{dt_s * 1e3:.2f}ms、q_att={qa:g} 时："
          f"r=1e-2（固件现值）→ τ≈{np.sqrt(1e-2 * dt_s / qa):.1f}s")
    if ar_sig:
        r_lo = max(ar_sig ** 2, 1e-6)
        print(f"         r=σ²={r_lo:.1e} → τ≈{np.sqrt(r_lo * dt_s / qa):.2f}s"
              f" —— 快得离谱，振动/线性加速度会直接灌进姿态")
    print("       R 要同时容纳「传感器噪声」和「未建模动态」（振动、线性加速度、未标硬铁），")
    print("       所以取噪声下限的 10²~10³ 倍是**正常**工程取舍，不是「保守」，别照噪声下限调。")

    print(f"    3) mag_reject：|mag| 均值 {mn.mean():.1f} µT，std {mn.std():.2f} µT；"
          f"偏离均值 30% 的有 {100.0 * np.count_nonzero(np.abs(mn - mn.mean()) > 0.3 * mn.mean()) / mn.size:.1f}%")
    if used.any():
        mr = mag_res[used]
        print(f"       mag_used 帧的残差 std = {mr.std():.4f} → σ ≈ {np.degrees(mr.std()):.2f}°"
              f" → 噪声下限 r_mag ≈ {max(mr.std() ** 2, 1e-6):.2e}")
        print("       ⚠ 这只是**噪声**下限，不含未标硬铁引起的**方向**偏差 —— 后者在这类静态日志里"
              "是个常数偏置，根本不进 std。硬铁补标前 r_mag 别照这个收。")
    else:
        print("       ⚠ 全程 mag_used=0，无法从残差定 r_mag；先解决磁为什么没被用上")


# ---------------------------------------------------------------- main


def _skew_stack(w):
    """w:(N,3) → (3N,3)，第 k 个行块是 [w_k]×（约定 [w]× h = w × h）"""
    A = np.zeros((w.shape[0] * 3, 3))
    A[0::3, 1] = -w[:, 2]
    A[0::3, 2] = w[:, 1]
    A[1::3, 0] = w[:, 2]
    A[1::3, 2] = -w[:, 0]
    A[2::3, 0] = -w[:, 1]
    A[2::3, 1] = w[:, 0]
    return A


def sec_interf(d, quiet):
    """磁干扰到底能不能用：分清「常量偏置」（可标定）与「随电流跑的瞬态」（不可标定）。

    模型（矢量 m 在它**自己所在的那个坐标系**里）：m = c + R(q)·B_w。c 是常量偏置 —— 硬铁，
    以及**恒定油门下的直流电流场**；这两者在本判据里无法区分，也不需要区分，都能量出来减掉。
    对时间求导，c 是常量、消失：

        dm/dt = -ω × (m - c)          … ω = 该坐标系里的角速度

    右端是叉乘，恒 ⊥ ω。于是得到一条**与 c、|B|、姿态、磁倾角全都无关**的判据：

        (dm/dt) · ω ≡ 0        （只要机体在转，且转轴不平行于 m）

    一条判据干两件事：
      ① **定安装**：日志 CH20-22 是固件 IST8310MagCorrect **之后**的矢量（已含 mount_rot）。
         若 mount_rot 正确，它就是 IMU 系 → ω 直接取陀螺（「单位」候选）。不正确的话它
         会落到某个带符号置换上。24 个候选全试，评分最小的就是真正在用的那套坐标。
      ② **量洁净度**：最优候选下的残余就是「磁矢量里有多少不是刚性地球场」。判据与姿态
         无关，所以它同时排除了「只是没标硬铁」这个嫌疑人 —— 若噪声地板很低而残余很大，
         那就只能是随负载变的瞬态磁场，标定救不了。
    """
    print("\n── ⑩ 磁干扰能不能用（常量 vs 瞬态；与姿态/倾角/偏置全都无关） ──")
    tsec = np.cumsum(d[:, COL_DT]) * 1e-3
    m = d[:, COL_MAG]
    g = d[:, COL_GYRO]
    mm = np.linalg.norm(m, axis=1)
    used = d[:, COL_MAG_USED] > 0.5

    if np.count_nonzero(used) == 0:
        print("    ⚠ 全程没有一帧磁有效，后面无从谈起")
        return

    # ---------- 0. mag_used 为什么这么低：把三道门限拆开 ----------
    # 磁是零阶保持：驱动层读到同一帧会连续输出同一值 → 只在「值真的变了」处认为来了新帧
    chg = np.any(np.diff(m, axis=0) != 0.0, axis=1)
    isnew = np.zeros(d.shape[0], dtype=bool)
    isnew[1:] = chg
    a_n = np.linalg.norm(d[:, COL_ACC], axis=1)
    dipv = np.ones(d.shape[0])
    okv = (mm > 1e-9) & (a_n > 1e-9)
    dipv[okv] = np.linalg.norm(np.cross(m[okv] / mm[okv, None], d[okv][:, COL_ACC] / a_n[okv, None]), axis=1)
    e_m = np.abs(mm - CFG_MAG_REF_UT)
    n_new = int(np.count_nonzero(isnew))
    n_dip = int(np.count_nonzero(isnew & (dipv >= CFG_MAG_DIP_MIN)))
    n_mag = int(np.count_nonzero(isnew & (dipv >= CFG_MAG_DIP_MIN) & (e_m < CFG_MAG_REJECT_UT)))
    n_used = int(np.count_nonzero(used))
    print(f"    【0】mag_used 归因（总行 {d.shape[0]}，磁刷新中位 {np.median(np.diff(tsec[np.flatnonzero(isnew)])) * 1e3:.1f} ms）")
    print(f"        · 磁来了新帧            n={n_new:6d}（{100.0 * n_new / d.shape[0]:5.1f}% —— mag_new 的上限就到这里）")
    print(f"        · 且不近平行(dip≥{CFG_MAG_DIP_MIN})  n={n_dip:6d}  → 被 dip 门限拒掉 {n_new - n_dip}")
    print(f"        · 且 |e_m|<{CFG_MAG_REJECT_UT:.0f}µT        n={n_mag:6d}  → 被模长门限拒掉 {n_dip - n_mag}")
    print(f"        · 实际 mag_used         n={n_used:6d}（{100.0 * n_used / max(n_new, 1):.1f}% of 新帧）")
    mn = mm[isnew]
    print("        · 新帧 |m| 分位：" + "  ".join(f"p{q}={np.percentile(mn, q):6.2f}" for q in (0, 5, 25, 50, 75, 95, 100)) +
          f" µT（门限带 [{CFG_MAG_REF_UT - CFG_MAG_REJECT_UT:.0f},{CFG_MAG_REF_UT + CFG_MAG_REJECT_UT:.0f}]）")
    print(f"          参考场 {CFG_MAG_REF_UT:.0f} µT（IGRF-14 长沙），中位数偏差 {np.median(mn) - CFG_MAG_REF_UT:+.2f} µT")
    print("        分窗 |m| 中位（每 15 s）—— 偏置是常量的话这条线应该基本平；跟着负载/时间走就是瞬态：")
    tw = tsec[isnew]
    mw = mm[isnew]
    bins, edges = [], np.arange(0.0, np.ceil(tsec[-1] / 15.0) * 15.0 + 1e-9, 15.0)
    row = []
    for b0, b1 in zip(edges[:-1], edges[1:]):
        sl = (tw >= b0) & (tw < b1)
        row.append(f"{np.median(mw[sl]):6.1f}" if np.count_nonzero(sl) > 20 else "   -- ")
    print("          t(s)  " + " ".join(f"{int(b0):>6d}" for b0 in edges[:-1]))
    print("          |m|   " + " ".join(row))

    # ---------- 1. |m| 分布 ----------
    mv = mm[used]
    print("    【1】|m| 分布（磁计有效帧）：" +
          "  ".join(f"p{q}={np.percentile(mv, q):6.2f}" for q in (0, 5, 25, 50, 75, 95, 100)) +
          f"  µT   (参考场 {CFG_MAG_REF_UT:.0f})")

    # 磁是**零阶保持**：驱动层读到同一帧会连续输出同一值 → 只在「值真的变了」处取相邻两帧
    chg = np.any(np.diff(m, axis=0) != 0.0, axis=1)
    idx = np.flatnonzero(chg) + 1
    if idx.size < 20:
        print("    ⚠ 磁矢量几乎没变化，判不出来")
        return
    ref_dt = float(np.median(np.diff(tsec[idx])))
    print(f"    磁刷新中位间隔 = {ref_dt * 1e3:.1f} ms（{1.0 / max(ref_dt, 1e-6):.0f} Hz）")

    # 静止段的帧间跳变 = 纯噪声地板（姿态没动，m 就不该变）——这是后面所有判读的标尺
    gn = np.linalg.norm(g, axis=1)
    i0, i1 = idx[:-1], idx[1:]
    dtv = tsec[i1] - tsec[i0]
    okp = (dtv > 0.002) & (dtv < 0.080) & used[i0] & used[i1]
    still_p = okp & (gn[i0] < 0.05) & (gn[i1] < 0.05)
    if np.count_nonzero(still_p) > 20:
        dms = (m[i1] - m[i0])[still_p]
        noise = np.linalg.norm(dms, axis=1) / np.sqrt(2.0)
        print(f"    【2】静止段磁噪声地板：{noise.std():.3f} µT/轴（n={np.count_nonzero(still_p)}，"
              f"理论 0.3 µT/LSB，AVG_16 后应 ≪1）")
        print(f"        静止时 |Δm| p50={np.median(np.linalg.norm(dms, axis=1)):.3f} µT —— "
              "这就是「机器停着时磁矢量抖多少」")
    else:
        print("    【2】⚠ 静止段太少，噪声地板量不出来")

    # ---------- 3. (dm/dt)·ω 判据 ----------
    # 基线取 ~0.1 s：既让转过的角度足够大（Δm 超过噪声），又让 ω 的均值有意义
    lag = max(1, int(round(0.10 / max(ref_dt, 1e-6))))
    i0, i1 = idx[:-lag], idx[lag:]
    dtv = tsec[i1] - tsec[i0]
    keep = (dtv > 0.02) & (dtv < 0.40) & used[i0] & used[i1]
    i0, i1, dtv = i0[keep], i1[keep], dtv[keep]
    if i0.size < 30:
        print("    ⚠ 可用于判据的帧对太少，本次日志判不了")
        return
    dm = m[i1] - m[i0]
    cs = np.concatenate([np.zeros((1, 3)), np.cumsum(g, axis=0)])
    w = (cs[i1 + 1] - cs[i0]) / (i1 + 1 - i0)[:, None]   # 区间平均角速度
    wnn = np.linalg.norm(w, axis=1)
    dmn = np.linalg.norm(dm, axis=1)
    mmid = 0.5 * (m[i0] + m[i1])
    # ---------- 3. (dm/dt)·ω 判据：|Δm| 阈值扫描 ----------
    # 评分 = 相干投影占比：零均值噪声自行抵消，故对噪声稳健；错框架下偏置不抵消。
    # Δm 越大微分噪声占比越小，区分度应随之变好；若怎么扫都分不开，就是数据本身不干净。
    print(f"    【3】(dm/dt)·ω 判据（|ω|>0.10 rad/s，基线 {np.median(dtv) * 1e3:.0f} ms）")
    print(f"        {'|Δm|>':>8} {'n':>6} {'最优':>9} {'次优':>9} {'区分×':>7} {'单位阵':>8}   最优候选")
    picked = None
    for thr in (2.0, 4.0, 8.0, 15.0, 25.0):
        sl = (wnn > 0.10) & (dmn > thr)
        if np.count_nonzero(sl) < 40:
            print(f"        {thr:6.1f}µT {np.count_nonzero(sl):6d}   样本不足(<40)，到此为止")
            break
        wss, dss, dnss = w[sl], dm[sl], dmn[sl]
        rows = []
        for r in cube_rotations():
            wc = (r.T @ wss.T).T
            wcn = np.linalg.norm(wc, axis=1)
            cosv = np.sum(dss * wc, axis=1) / (dnss * wcn)
            score = abs(float(np.sum(dss * wc) / np.sum(dnss * wcn)))
            rows.append((score, np.median(np.abs(cosv)), cosv, r))
        rows.sort(key=lambda x: x[0])
        ident = next(k for k, r in enumerate(rows) if np.allclose(r[3], np.eye(3))) + 1
        rb = rows[0][3]
        print(f"        {thr:6.1f}µT {np.count_nonzero(sl):6d} {rows[0][0]:9.4f} {rows[1][0]:9.4f} "
              f"{rows[1][0] / max(rows[0][0], 1e-9):7.2f} {ident:6d}/24   "
              f"[[{rb[0][0]:+.0f},{rb[0][1]:+.0f},{rb[0][2]:+.0f}],"
              f"[{rb[1][0]:+.0f},{rb[1][1]:+.0f},{rb[1][2]:+.0f}],"
              f"[{rb[2][0]:+.0f},{rb[2][1]:+.0f},{rb[2][2]:+.0f}]]")
        picked = (thr, rows, ident, sl, wss, dss, dnss)
    if picked is None:
        print("    ⚠ 所有阈值下样本都不足（<40），本次日志判不了")
        return
    thr, rows, ident, sel, ws, dms, dwns = picked
    score0, med0, cos0, rbest = rows[0]

    # ---------- 4. 最高信噪比档：拟合常量偏置 + 残差 ----------
    wc = (rbest.T @ ws.T).T
    A = _skew_stack(wc)
    b = (dms / dtv[sel][:, None] + np.cross(wc, mmid[sel])).ravel()
    cfit, *_ = np.linalg.lstsq(A, b, rcond=None)
    resid = A @ cfit - b
    sig = np.cross(wc, mmid[sel]).ravel()
    rfrac = float(np.linalg.norm(resid) / max(np.linalg.norm(sig), 1e-9))
    # 同口径噪声地板：dm/dt 的噪声（σ_m·√2/Δt）堆 3n 个分量 / ‖sig‖；σ_m 取单次 0.3µT（偏保守）
    nfloor = float(np.sqrt(2.0) * 0.3 / np.median(dtv[sel]) * np.sqrt(3 * np.count_nonzero(sel))
                   / max(np.linalg.norm(sig), 1e-9))
    print(f"    【4】取 |Δm|>{thr:.0f}µT 档（n={np.count_nonzero(sel)}）做定量：")
    print(f"        常量偏置 c = ({cfit[0]:+.2f},{cfit[1]:+.2f},{cfit[2]:+.2f}) µT，"
          f"|c|={np.linalg.norm(cfit):.2f} µT")
    print(f"        模型残差/信号 = {rfrac:.3f}（同口径噪声地板 {nfloor:.3f}）→ "
          f"{'◆ 高于噪声地板 2 倍以上，有真实非刚性成分' if rfrac > 2.0 * nfloor else '≈噪声地板，看不出非刚性成分'}")
    print(f"        逐对 |cos|：p50={np.percentile(np.abs(cos0), 50):.3f} "
          f"p90={np.percentile(np.abs(cos0), 90):.3f}（0=完全刚性地球场，0.5=纯噪声）")

    # ---------- 5. 判读 ----------
    print("    【5】判读")
    if ident == 1:
        print("        · 单位阵评分最低 → 日志里的磁矢量确实在 IMU 系，固件 mount_rot 与数据自洽。")
    else:
        rb = rows[0][3]
        print(f"        · 单位阵只排第 {ident} 名，最优是带符号置换 "
              f"[[{rb[0][0]:+.0f},{rb[0][1]:+.0f},{rb[0][2]:+.0f}],"
              f"[{rb[1][0]:+.0f},{rb[1][1]:+.0f},{rb[1][2]:+.0f}],"
              f"[{rb[2][0]:+.0f},{rb[2][1]:+.0f},{rb[2][2]:+.0f}]]"
              f"（ΔIGRF 需另用 ⑥ 段复核）→ 固件现值与数据不自洽。")
    if score0 < 0.03:
        print(f"        · 评分 {score0:.4f} 极低：磁矢量整体就是个刚性地球场 ——"
              " 干扰只体现为**常量偏置**，标定能吃掉，磁计可用。")
    elif score0 < 0.10:
        print(f"        · 评分 {score0:.4f} 偏低：有可观的非刚性成分，但仍以常量为主。"
              " 标定后配合模长门限可用，yaw 精度会打折。")
    else:
        print(f"        · 评分 {score0:.4f} 偏高：磁矢量被非刚性成分主导（瞬态磁场）。"
              " 这种干扰**离线标定救不了**，yaw 不可信。")


def sec_startup(d, seg_s=1.0):
    """上电/启动瞬态：yaw 是不是在磁计「被用上」的那一刻被拽走？

    这是 yaw 轴独有的问题。roll/pitch 有重力观测，一上电就钉得住；yaw 只能靠磁计，
    所以「播种时有没有磁参与」「之后磁什么时候被接受/拒绝」都会直接变成 yaw 的台阶 ——
    在控制器眼里就是「启动时 yaw 突变」。本段把这个机理量化出来。
    """
    print("\n── ⑧ 启动瞬态与 yaw 台阶 ──")
    tsec = np.cumsum(d[:, COL_DT]) * 1e-3
    yaw = np.degrees(np.unwrap(d[:, COL_YAW]))
    used = d[:, COL_MAG_USED] > 0.5
    an = np.linalg.norm(d[:, COL_ACC], axis=1)

    # 播种判据与模块一致：|acc| 进 seed_acc_tol(2.0) 内即可能播种；再看那帧磁有没有被用
    can_seed = np.abs(an - EARTH_G) < 2.0
    if not can_seed.any():
        print("    ⚠ 全程没有一帧满足播种条件（||acc|-g| < 2.0 m/s²），模块可能一直没播种")
        return
    i0 = int(np.argmax(can_seed))
    print(f"    首次满足播种条件 t={tsec[i0]:.2f}s，该帧 mag_used={int(used[i0])}")
    if not used[i0]:
        print("      → 播种时**没有**磁参与：yaw 基准 = 上电瞬间的物理朝向，被记为 0。")
        print("        之后磁一旦被接受，yaw 就会被拽向磁航向 —— 这个差值就是启动台阶的幅度。")
    else:
        print("      → 播种时磁已参与（TRIAD），yaw 基准 = 磁航向；此时若磁模型有偏，")
        print("        启动 yaw 会随上电时的姿态不同而不同（换个朝向开机，yaw 就不一样）。")

    end = min(tsec[-1], 40.0)
    print(f"    {'区间':>15} {'yaw起':>10} {'yaw止':>10} {'Δ':>9} {'mag%':>7} {'||acc|-g|p50':>12}")
    b = 0.0
    while b < end:
        sl = (tsec >= b) & (tsec < b + seg_s)
        if np.count_nonzero(sl) >= 5:
            y0, y1 = yaw[sl][0], yaw[sl][-1]
            print(f"    {b:5.1f}~{b + seg_s:5.1f}s {y0:+10.3f} {y1:+10.3f} {y1 - y0:+9.3f}"
                  f" {100.0 * np.count_nonzero(used[sl]) / np.count_nonzero(sl):6.1f}%"
                  f" {np.median(np.abs(an[sl] - EARTH_G)):12.3f}")
        b += seg_s

    flips = int(np.count_nonzero(np.diff(used.astype(np.int8)) == 1))
    print(f"    mag_used 由 0→1 的翻转次数 = {flips}")
    print("      每次翻转都是一次 yaw 校正的「开/关」。校正开着时 yaw 以 τ=√(r_mag·dt/q_att)")
    print("      的速率被拉向磁航向（本模块约 4.5~8s），几十度在一两个 τ 内走完 ——")
    print("      控制器看到的就是一次阶跃。所以「启动突变」不是 bug，是磁计可信度在跳。")


def main():
    ap = argparse.ArgumentParser(description="drvlib_bmi088_ist8310_eskf 日志分析与调参")
    ap.add_argument("csv", help="VOFA+ 存的 CSV")
    ap.add_argument("--quiet", type=float, default=0.02, help="静态段判定阈值 |gyro| (rad/s)，缺省 0.02")
    ap.add_argument("--section", default="all",
                    help="all/overview/attitude/imu/mag/mount/world/bins/drift/startup/interf/advice")
    ap.add_argument("--bin", type=float, default=10.0, help="分段表的时间粒度 (s)，缺省 10")
    args = ap.parse_args()

    raw, drop_info, n_raw = load(args.csv)
    n_drop, n_garbage = drop_info
    d = raw
    t = d[:, COL_T]

    print("=" * 78)
    print(f"文件 {args.csv}")
    print("=" * 78)

    s = args.section
    want = lambda k: s in ("all", k)  # noqa: E731

    tsec = None
    if want("overview"):
        tsec = sec_overview(d, t, (n_drop, n_garbage), n_raw)
    quiet = still_mask(d)[0]  # 各节都要用，先统一算
    if want("attitude"):
        sec_attitude(d)
    if want("imu"):
        sec_gyro_acc(d, args.quiet)
    if want("mag"):
        used = sec_mag(d, quiet)
    else:
        used = d[:, COL_MAG_USED] > 0.5
    if want("mount"):
        sec_mount(d, quiet)
    if want("world"):
        sec_world_ref(d, quiet)
    if want("bins"):
        sec_bins(d, used, args.bin)
    if want("drift"):
        sec_drift(d, used)
    if want("startup"):
        sec_startup(d)
    if want("interf"):
        sec_interf(d, quiet)
    if want("advice"):
        sec_advice(d, quiet, used)

    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
