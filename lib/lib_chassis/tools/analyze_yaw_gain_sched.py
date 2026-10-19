#!/usr/bin/env python3
"""分析力控半舵底盘 yaw 增益调度日志（VOFA 29 列）。

核心量：yaw 环的静态环路增益代理
    gain = kp_eff * |sin(phi)| / (a * F_trans)
其中 phi 是实际平移合力方向，F_trans = hypot(fx, fy)，a = 0.20225 m。
kp_eff = kp_nominal * (1 - sched_k * |vy|/|v|)^power。

用法:
    python analyze_yaw_gain_sched.py <csv> --power 3
    python analyze_yaw_gain_sched.py <csv> --power 1 --kp 6 --sched-k 0.5
    python analyze_yaw_gain_sched.py <csv> --power 3 --detail

列布局（app_chassis_force）:
    I1/I2 左舵目标/实际角(deg)，I3/I4 右舵
    I5/I6 左轮力矩目标/实际，I7/I8 右轮
    I9 combo，I10..I13 cost，I14 locked，I15 state(2=SAFE,3=RUN)
    I16/I17 align L/R
    I18..I20 指令 vx/vy/w
    I21..I23 反馈 vx/vy/w（w 为 IMU 底盘系）
    I24..I26 fx/fy/mw
    I27/I28 轮速
"""

from __future__ import annotations

import argparse

import numpy as np
import pandas as pd

A_HALF_TRACK = 0.20225  # m，舵轮中心到车体中心（y 方向）
STEER_RATE_LIMIT_DEG_S = 50.0 / 8.0 * 180.0 / np.pi  # 电机 50 rad/s / 8，约 358°/s


def wrap180_deg(x):
    return (x + 180.0) % 360.0 - 180.0


def circmean(a):
    return np.arctan2(np.sin(a).mean(), np.cos(a).mean())


def load(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    need = {f"I{i}" for i in range(29)}
    if not need.issubset(df.columns):
        raise SystemExit(f"{path}: 列数不是 VOFA 29 列，实际 {len(df.columns)}")
    n_all = len(df)
    df = df[df.I15 == 3].reset_index(drop=True)
    if len(df) == 0:
        raise SystemExit(f"{path}: 没有 RUN(state=3) 数据")
    print(f"{path}: {n_all} 行，RUN {len(df)} 行，时长 {(df.I0.iloc[-1]-df.I0.iloc[0])*1e-6:.1f}s")
    return df


def sched_factor(df: pd.DataFrame, kp: float, sk: float, power: int):
    vx = df.I18.values
    vy = df.I19.values
    v = np.hypot(vx, vy)
    clat = np.where(v > 1e-6, np.abs(vy) / np.maximum(v, 1e-9), 0.0)
    w = np.where(v > 0.02, 1.0 - sk * clat, 1.0)
    return kp * w**power, clat, v


def command_bins(df: pd.DataFrame, kp_eff: np.ndarray, clat: np.ndarray, v: np.ndarray):
    wcmd = df.I20.values
    idle = (v < 0.05) & (np.abs(wcmd) < 0.1)
    spin = (v < 0.05) & (np.abs(wcmd) >= 0.1)
    fwd = (v >= 0.05) & (clat < 0.3)
    diag = (v >= 0.05) & (clat >= 0.3) & (clat < 0.8)
    lat = (v >= 0.05) & (clat >= 0.8)
    fy = df.I25.values
    fx = df.I24.values
    fang = np.arctan2(fy, fx)
    fbuf = df.I23.values
    mw = df.I26.values
    rows = []
    for name, m in [("静止/停车", idle), ("纯旋转", spin), ("前进/后退", fwd),
                    ("斜移", diag), ("横移", lat)]:
        if not m.any():
            continue
        F = np.hypot(fx[m], fy[m]).mean()
        # 用逐拍 |sinφ| 的均值：力方向在振荡中会跨 ±90°，圆均值会骗人
        sinf = np.abs(np.sin(fang[m])).mean()
        combos = df.I9.values[m].astype(int)
        # 纯旋转/静止时平移力≈0，静态耦合公式不适用（yaw 力矩沿轮向，φ 无意义）
        gain = np.nan if v[m].mean() < 0.05 else (
            kp_eff[m].mean() * sinf / (A_HALF_TRACK * max(F, 1e-3)))
        rows.append({
            "类别": name, "n": int(m.sum()), "v": v[m].mean(), "clat": clat[m].mean(),
            "|wcmd|": np.abs(wcmd[m]).mean(), "w_fb sd": fbuf[m].std(),
            "w ptp": np.ptp(fbuf[m]), "mw sd": mw[m].std(),
            "mw p95": np.percentile(np.abs(mw[m]), 95),
            "F(N)": F, "|sinφ|": sinf, "kp_eff": kp_eff[m].mean(),
            "gain": gain,
            "combo": {k: int((combos == k).sum()) for k in range(4)},
        })
    return pd.DataFrame(rows)


def window_table(df: pd.DataFrame, kp_eff: np.ndarray, clat: np.ndarray,
                 v: np.ndarray, win_s: float):
    t = (df.I0.values - df.I0.values[0]) * 1e-6
    n = max(int(round(win_s / 0.01)), 1)
    out = []
    for a in range(0, len(df) - n + 1, n):
        b = a + n
        vm = np.median(v[a:b])
        wc = np.median(df.I20.values[a:b])
        if vm < 0.2 or abs(wc) > 0.15:
            continue
        fx, fy = df.I24.values[a:b], df.I25.values[a:b]
        F = np.hypot(fx, fy).mean()
        if F < 3.0:
            continue
        sinf = np.abs(np.sin(np.arctan2(fy, fx))).mean()
        kp = float(np.median(kp_eff[a:b]))
        gain = kp * sinf / (A_HALF_TRACK * F)
        wfb = df.I23.values[a:b]
        mw = df.I26.values[a:b]
        # 舵速/跟踪
        rt = np.abs(wrap180_deg(np.diff(df.I1.values[a:b]))) / 0.01
        ra = np.abs(wrap180_deg(np.diff(df.I2.values[a:b]))) / 0.01
        err = np.abs(wrap180_deg(df.I1.values[a:b] - df.I2.values[a:b]))
        out.append({
            "t": t[a], "v": vm, "clat": clat[a], "wcmd": wc, "F": F, "sinF": sinf,
            "kp": kp, "gain": gain, "wsd": wfb.std(), "mwsd": mw.std(),
            "tgtRate95": np.percentile(rt, 95), "actRate95": np.percentile(ra, 95),
            "err95": np.percentile(err, 95), "align": df.I16.values[a:b].mean(),
            "v_act": np.hypot(df.I21.values[a:b], df.I22.values[a:b]).mean(),
        })
    return pd.DataFrame(out)


def transition_windows(df: pd.DataFrame, win_s: float):
    """指令已松但车仍在动：当前调度会把 factor 瞬间放回 1。"""
    t = (df.I0.values - df.I0.values[0]) * 1e-6
    n = max(int(round(win_s / 0.01)), 1)
    out = []
    for a in range(0, len(df) - n + 1, n):
        b = a + n
        vcmd = np.hypot(np.median(df.I18.values[a:b]), np.median(df.I19.values[a:b]))
        vact = np.hypot(df.I21.values[a:b], df.I22.values[a:b]).mean()
        F = np.hypot(df.I24.values[a:b], df.I25.values[a:b]).mean()
        if vcmd < 0.02 and vact > 0.15 and F > 5.0:
            out.append((t[a], vact, F, df.I23.values[a:b].std(), df.I26.values[a:b].std()))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--power", type=int, default=3, choices=(1, 3), help="余弦次数")
    ap.add_argument("--kp", type=float, default=6.0, help="额定 kp")
    ap.add_argument("--sched-k", type=float, default=0.5, help="factor = 1-k*clat")
    ap.add_argument("--window", type=float, default=0.5, help="统计窗口 (s)")
    ap.add_argument("--detail", action="store_true", help="输出每个窗口")
    args = ap.parse_args()

    df = load(args.csv)
    kp_eff, clat, v = sched_factor(df, args.kp, args.sched_k, args.power)
    print(f"\n配置: kp={args.kp}, sched_k={args.sched_k}, power={args.power}")
    for c in (0.0, 0.3, 0.5, 0.7, 1.0):
        f = (1.0 - args.sched_k * c) ** args.power
        print(f"  clat={c:.1f} -> factor={f:.3f}, kp_eff={args.kp*f:.2f}")

    print("\n=== 按指令方向分箱 ===")
    print(command_bins(df, kp_eff, clat, v).to_string(index=False,
          float_format=lambda x: f"{x:.2f}"))

    wt = window_table(df, kp_eff, clat, v, args.window)
    print(f"\n=== 平移窗口 (v>=0.2, |wcmd|<=0.15, {args.window}s) ===")
    bins = [0, 0.15, 0.25, 0.35, 0.45, 0.55, 0.7, 1.0, 2.0, 5.0]
    wt["gain_bin"] = pd.cut(wt.gain, bins)
    print(wt.groupby("gain_bin", observed=True).agg(
        n=("gain", "size"),
        wsd_med=("wsd", "median"),
        wsd_p90=("wsd", lambda x: np.percentile(x, 90)),
        mwsd_med=("mwsd", "median"),
        tgtRate95=("tgtRate95", "median"),
        F=("F", "mean"), clat=("clat", "mean"),
    ).round(3).to_string())

    if args.detail:
        print("\n=== 窗口明细 ===")
        print(wt.to_string(index=False, float_format=lambda x: f"{x:.2f}"))

    tw = transition_windows(df, args.window)
    print(f"\n=== 松杆但仍在动（factor 会瞬间回到 1）: {len(tw)} 个窗口 ===")
    for t, va, F, wsd, mwsd in tw:
        print(f"  t={t:5.1f}s v_act={va:.2f} F={F:5.1f}N wsd={wsd:.2f} mwsd={mwsd:.2f}")


if __name__ == "__main__":
    main()
