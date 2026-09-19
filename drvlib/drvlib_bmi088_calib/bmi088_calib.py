#!/usr/bin/env python3
"""BMI088 标定脚本：解析 VOFA+ 录的 CSV → 算陀螺零偏 → 打印可粘贴的 C 代码

配合 drvlib_bmi088_{kalman,mahony} 的 `vofa_enable = 1` 使用：
模块每帧把姿态/角速度/加速度/温度/生效零偏写进 VOFA 通道并自己发帧，
VOFA+ 上位机直接存成 CSV，就是这个脚本的输入。

通道约定（两个 drvlib 逐通道相同，详见 drvlib_bmi088_kalman.h 头注释）：
    CH1-3  euler roll/pitch/yaw (rad)      CH11   yaw_rate (rad/s)
    CH4-6  gyro x/y/z (rad/s)  ← 已修正     CH12   温度 (℃)，未采到为 nan
    CH7-9  acc x/y/z (m/s²)    ← 已修正     CH13-15 本帧生效零偏 (rad/s)
    CH10   dt (ms)，0 = 无新陀螺样本        CH16-19 四元数 w/x/y/z（仅 mahony 版）
CSV 第 0 列 I0 是 DWT 时间戳 (us)，第 n 列即 CHn。

⚠ CH4-6 是**已扣掉标定值**的角速度，所以录到的均值是**残差**，不是绝对零偏：
    新零偏 = 旧零偏(CH13-15) + 残差(CH4-6 均值)
  这是一次迭代就够的收敛式；也可以把 app 里 `.gyro.bias` 清零再录一次拿原始值。

用法：
    python3 bmi088_calib.py static.csv              # 全段
    python3 bmi088_calib.py static.csv --skip 20    # 丢掉上电头 20s（热漂）
    python3 bmi088_calib.py static.csv --tail 60    # 只用最后 60s
    python3 bmi088_calib.py static.csv --csv-resid  # 输出消掉零偏后的 csv
"""

import argparse
import sys

import numpy as np

# 文件列号 = 通道号（第 0 列是 I0 时间戳），这里给的是 (起, 止) 闭区间
CH = {
    "euler": (1, 3),
    "gyro": (4, 6),
    "acc": (7, 9),
    "yaw_rate": 11,
    "temp": 12,
    "bias": (13, 15),
    "quat": (16, 19),
}
MIN_COLS = 16  # 到 CH15 才有"生效零偏"列，残差法需要它
FREQ_NOMINAL = 500.0  # 云台任务 2ms
OUTLIER_SIGMA = 5.0  # 静止段筛选：|x - 中位| > 5σ_MAD 的点剔除（防手碰/台面被撞）


def load(path):
    d = np.loadtxt(path, delimiter=",", skiprows=1)
    if d.ndim == 1:
        d = d[None, :]
    return d


def pick(d, name):
    """按通道约定取列，返回 (N, k) 数组；通道不存在返回 None"""
    ch = CH[name]
    if isinstance(ch, int):
        return d[:, ch] if d.shape[1] > ch else None
    lo, hi = ch
    if d.shape[1] <= hi:
        return None
    return d[:, lo:hi + 1]


def seg_table(t, gy, ac, tp, bin_s, n):
    """分段均值表：每档均值应只跳动白噪声量级；单调漂移 = 热漂/平台在动，该 --skip/--tail"""
    print(f"\n── 分段均值（{bin_s:.0f}s 一档）──")
    print("  时间窗(s)   温度(℃)      gx均值       gy均值       gz均值      |acc|")
    for a in np.arange(0.0, t[-1], bin_s):
        m = (t >= a) & (t < a + bin_s)
        if m.sum() < 100:
            continue
        temp = f"{np.nanmean(tp[m]):5.2f}" if tp is not None and not np.all(np.isnan(tp[m])) else "  nan"
        print(f"  {a:>4.0f}-{a + bin_s:<4.0f}   {temp}   "
              f"{gy[m, 0].mean(): .6f}   {gy[m, 1].mean(): .6f}   {gy[m, 2].mean(): .6f}   "
              f"{np.linalg.norm(ac[m], axis=1).mean():.4f}")


def allan_report(gy, fs):
    """Allan 偏差：白噪声段按 1/√τ 下降；转平/上翘处是 bias instability 地板，
    再往后录也不会更准。没有地板就一直按 1/√T 改善（这时决定"录多久"的是耐心）"""
    print(f"\n── 噪声与 Allan 偏差（采样 {fs:.0f}Hz，Allan 看 gz——它决定 yaw）──")
    for k, name in enumerate("xyz"):
        print(f"  gyro {name}: 单点 σ = {gy[:, k].std():.6f} rad/s = "
              f"{np.rad2deg(gy[:, k].std()):.4f} °/s")
    print("\n  τ(s)   Allan σ(rad/s)   白噪声预期    实测/预期   (段数)")
    floor_tau = None
    for tau in (0.5, 1, 2, 5, 10, 20, 50):
        m = int(tau * fs)
        nseg = len(gy) // m
        if m < 2 or nseg < 20:
            continue
        seg = gy[:nseg * m, 2].reshape(nseg, m).mean(axis=1)
        av = np.sqrt(0.5 * np.var(np.diff(seg), ddof=1))
        pred = gy[:, 2].std() / np.sqrt(m)  # 白噪声下 Allan = σ/√m
        print(f"  {tau:>4.1f}   {av:.3e}      {pred:.3e}    {av / pred:5.2f}    ({nseg})")
        if av / pred > 1.5:
            floor_tau = tau
    if floor_tau:
        print(f"  → 平均约 {floor_tau:.0f}s 之后偏离白噪声：这是零偏不稳定性地板，"
              f"再录更久也不会更准，{floor_tau:.0f}~{2 * floor_tau:.0f}s 是最优录制长度")
    else:
        print("  → 全程白噪声主导（没有地板）：均值精度按 1/√T 改善，录多久都有收益；"
              "但 90~120s 之后收益递减，除非要冲 °/h 以下")
    print("\n  录制时长 → gz 零偏精度（= yaw 漂移率，白噪声模型）：")
    for Trec in (10, 30, 60, 120, 300, 600):
        se = gy[:, 2].std() / np.sqrt(fs * Trec)
        print(f"    {Trec:>4d}s → {np.rad2deg(se) * 3600:6.2f} °/h")


def layout_problems(ac, eu, bias_col, yaw_rate_meas, resid_z, se_z, sl):
    """通道表自检。

    老固件（2026-09-19 改 VOFA 之前）的录法通道号完全不同，那时 CSV 里的
    CH13-15 根本不是"生效零偏"。逐通道对不上就会**静默算出错的零偏**，
    所以宁可拒绝输出，也不能让人粘进 app。
    返回问题列表（空 = 布局可信）。"""
    p = []
    acc_n = np.linalg.norm(ac[sl], axis=1).mean()
    if not 9.0 < acc_n < 10.6:
        p.append(f"|acc| 均值 {acc_n:.2f} m/s² 不像 1g → CH7-9 可能不是加速度")
    if max(np.abs(eu[sl, 0]).max(), np.abs(eu[sl, 1]).max()) > 1.6:
        p.append("CH1-3 超出 ±90° 量级 → 可能不是 euler")
    if bias_col is not None:
        sd = bias_col[sl].std(axis=0).max()
        if sd > 1e-5:
            p.append(f"CH13-15 录制中在变（σ={sd:.1e}）→ 不是'生效零偏'列（老固件布局？）")
    # yaw 漂移率应当≈残差 z（yaw 是校正后角速率的纯积分）——通道表不对时这条必然对不上
    if abs(yaw_rate_meas - resid_z) > max(0.3 * abs(resid_z), 5 * se_z):
        p.append(f"yaw 漂移率 {yaw_rate_meas:+.2e} 与残差 z {resid_z:+.2e} rad/s 对不上"
                 f" → CH4-6/CH1-3 对不上号，或录的不是静止段")
    return p


def robust_mean(x):
    """中位数附近 5σ_MAD 之外的点剔除后再求均值（防手碰/台面被撞的离群点）。
    返回 (修剪均值, 剔除数, 原始均值)：两个均值差得远说明分布不对称，
    得回去看分段均值表找原因，别默认哪个都对。"""
    med = np.median(x)
    mad = np.median(np.abs(x - med))
    sigma = 1.4826 * mad if mad > 0 else x.std()
    keep = np.abs(x - med) <= OUTLIER_SIGMA * sigma if sigma > 0 else np.ones_like(x, bool)
    return x[keep].mean(), int((~keep).sum()), x.mean()


def main():
    ap = argparse.ArgumentParser(description="BMI088 静态数据标定（陀螺零偏）")
    ap.add_argument("csv", help="VOFA+ 录的 CSV")
    ap.add_argument("--skip", type=float, default=0.0, help="丢掉开头 N 秒（上电热漂段）")
    ap.add_argument("--tail", type=float, default=0.0, help="只取末尾 N 秒")
    ap.add_argument("--csv-resid", metavar="OUT", help="输出消掉残差后的 csv（核对用）")
    ap.add_argument("--no-allan", action="store_true", help="跳过 Allan/噪声分析")
    ap.add_argument("--bin", type=float, default=10.0, help="分段均值表的档宽 (s)，默认 10")
    a = ap.parse_args()

    d = load(a.csv)
    n, ncol = d.shape
    t = d[:, 0] * 1e-6
    t -= t[0]

    gy = pick(d, "gyro")
    ac = pick(d, "acc")
    eu = pick(d, "euler")
    tp = pick(d, "temp")
    if gy is None or ac is None:
        sys.exit("CSV 通道不足：至少需要 CH1-9（euler/gyro/acc），确认 vofa_enable 已打开")

    print(f"文件: {a.csv}")
    print(f"数据: {n} 点 / {t[-1]:.1f}s / {ncol - 1} 通道")

    # ---- 时间戳与采样连续性 ----
    dt_us = np.diff(d[:, 0])
    bad = np.sum(dt_us <= 0)
    print(f"时间戳 dt: 中位 {np.median(dt_us):.0f}us ({1e6 / np.median(dt_us):.1f}Hz)  "
          f"范围 {dt_us.min():.0f}~{dt_us.max():.0f}us  倒走/重复 {bad} 点"
          + ("  ⚠ 有丢帧或 DMA 挤掉，尽量用后面的判据确认数据可用" if bad else ""))
    if ncol > 10:
        # CH10 = 模块按陀螺时间戳差分算的 dt：0 表示这一帧没有新样本（不积分的帧）
        dtm = d[:, 10]
        print(f"模块 dt(CH10): 中位 {np.median(dtm):.3f}ms  范围 {dtm.min():.3f}~{dtm.max():.3f}ms  "
              f"0 值帧 {int(np.sum(dtm == 0))}（{100 * np.sum(dtm == 0) / n:.1f}%，正常）")

    # ---- 全段分段均值：先看有没有热漂/平台在动，再决定要不要 --skip/--tail ----
    seg_table(t, gy, ac, tp, a.bin, n)

    # ---- 选取分析窗口：--skip 从前面丢，--tail 只留末尾，两者取交集 ----
    i0 = int(np.searchsorted(t, a.skip))
    if a.tail > 0:
        i0 = max(i0, int(np.searchsorted(t, t[-1] - a.tail)))
    i1 = n
    if i1 - i0 < 100:
        sys.exit(f"窗口太短（{i1 - i0} 点），检查 --skip/--tail")
    sl = slice(i0, i1)
    print(f"分析窗口: {t[i0]:.1f}~{t[i1 - 1]:.1f}s（{i1 - i0} 点 / {(t[i1 - 1] - t[i0]):.1f}s）")
    if a.skip > 0:
        print("            （已按 --skip 丢掉上电头段；温度没走平前录的数据会污染零偏，"
              "热漂不是零均值噪声、平均不掉）")

    # ---- 静止判定：姿态/加速度不动才谈得上零偏 ----
    acc_n = np.linalg.norm(ac[sl], axis=1)
    print("\n── 静止判定 ──")
    print(f"  euler roll/pitch σ: {eu[sl, 0].std() * 1e3:.2f} / {eu[sl, 1].std() * 1e3:.2f} mrad"
          f"  yaw 变化 {(eu[i1 - 1, 2] - eu[i0, 2]) * 180 / np.pi:+.4f}°")
    print(f"  |acc| {acc_n.mean():.4f} m/s²  (范围 {acc_n.min():.4f}~{acc_n.max():.4f}，"
          f"σ={acc_n.std():.4f})  理论 9.80665")
    for k, name in enumerate("xyz"):
        g = gy[sl, k]
        print(f"  gyro {name}: 中位 {np.median(g): .6f}  σ {g.std():.6f}  "
              f"峰峰 {(g.max() - g.min()):.4f} rad/s ({np.rad2deg(g.max() - g.min()):.2f} °/s)")
    moved = acc_n.std() > 0.05 or eu[sl, 0].std() > 0.005 or eu[sl, 1].std() > 0.005
    print("  判定: " + ("⚠ 有动作/晃动，均值不可信 —— 重录一段真正静止的" if moved
                        else "OK，静止"))

    # ---- 温度：本次能不能标温漂 ----
    if tp is not None and not np.all(np.isnan(tp[sl])):
        tpw = tp[sl]
        print(f"\n温度: {np.nanmin(tpw):.2f}~{np.nanmax(tpw):.2f} ℃（有效 "
              f"{int(np.sum(~np.isnan(tpw)))}/{len(tpw)}）")
        span = np.nanmax(tpw) - np.nanmin(tpw)
        if span < 3.0:
            print(f"  温差只有 {span:.2f}℃，标不出温度系数（需要 30~60min 的自热扫温段）；"
                  "本次只标零偏，对应温度见图")
        else:
            print(f"  温差 {span:.2f}℃，够做温漂拟合（本脚本暂只提示，见 --help 的说明）")
    else:
        print("\n温度: 全为 nan（drv 层还没采到有效温度），本次不涉及温漂")

    # ---- 残差 → 新零偏 ----
    print("\n── 陀螺零偏 ──")
    bias_col = pick(d, "bias")
    resid = np.zeros(3)
    plain = np.zeros(3)
    dropped = 0
    for k in range(3):
        resid[k], drop, plain[k] = robust_mean(gy[sl, k])
        dropped += drop
    applied = bias_col[sl].mean(axis=0) if bias_col is not None else None
    if applied is None:
        print("  ⚠ CSV 里没有 CH13-15（生效零偏），无法用残差法；"
              "请把 app 里的 .gyro.bias 清零重录，或手工填入已知值")
        applied = np.zeros(3)
    print(f"  已应用零偏 (CH13-15): {applied[0]: .6f} {applied[1]: .6f} {applied[2]: .6f} rad/s")
    print(f"  残差 (CH4-6 静止均值): {resid[0]: .6f} {resid[1]: .6f} {resid[2]: .6f} rad/s"
          f"   (剔除离群点 {dropped} 个)")
    print(f"  未修剪的原均值:        {plain[0]: .6f} {plain[1]: .6f} {plain[2]: .6f} rad/s"
          + ("   ✓ 一致" if np.max(np.abs(plain - resid)) < 2e-5
             else "   ⚠ 与修剪均值差得远 → 数据里有慢漂/不对称，看上面的分段均值表"))
    new = applied + resid
    print(f"  新零偏 = 旧 + 残差:   {new[0]: .6f} {new[1]: .6f} {new[2]: .6f} rad/s")

    # ---- 精度与效果 ----
    # 白噪声下均值标准误 = σ/√N；z 轴的零偏误差就是 yaw 的漂移速率
    se = np.array([gy[sl, k].std() / np.sqrt(i1 - i0) for k in range(3)])
    sec = (t[i1 - 1] - t[i0])
    print(f"\n  均值标准误: {se[0]:.2e} {se[1]:.2e} {se[2]:.2e} rad/s"
          f"  → z 轴 {np.rad2deg(se[2]) * 3600:5.2f} °/h（录制 {sec:.0f}s）")
    if sec < 60:
        print("  ⚠ 录制不足 60s：零偏估计还没收敛（白噪声按 1/√T 改善），建议重录 90~120s")
    yaw_unw = np.unwrap(eu[sl, 2])  # yaw wrap 到 (-π,π]，展开后再算漂移
    yaw_drift = yaw_unw[-1] - yaw_unw[0]
    yaw_rate_meas = yaw_drift / sec
    print(f"  yaw 实测漂移 {yaw_drift * 180 / np.pi:+.4f}° / {sec:.0f}s = "
          f"{np.rad2deg(yaw_rate_meas):+.4f} °/s（应≈残差 z = {np.rad2deg(resid[2]):+.4f} °/s）")
    print(f"  标定后预期: 残余 yaw 漂移 ≈ {np.rad2deg(se[2]) * 3600:.1f} °/h（零偏估计误差，"
          "受 bias 重复性与温漂限制，不再受本次这块数据的白噪声限制）")

    # ---- 通道表自检：对不上就拒绝出标定值 ----
    probs = layout_problems(ac, eu, bias_col, yaw_rate_meas, resid[2], se[2], sl)
    if probs:
        print("\n⚠ 通道布局自检没通过 —— 拒绝输出标定值（粘进去就是错的）：")
        for x in probs:
            print(f"   - {x}")
        print("   本脚本按 drvlib_bmi088_kalman.h 的通道表解析；"
              "2026-09-19 加 vofa_enable 之前的老录法通道号完全不同，"
              "请用 vofa_enable=1 的固件重录")
        sys.exit(2)

    # ---- 噪声/Allan：回答"该录多久" ----
    if not a.no_allan:
        fs = 1e6 / np.median(dt_us)
        allan_report(gy[sl], fs)

    # ---- 输出 C 代码 ----
    print("\n── 粘到 app 的 s_bmi088_calib ──")
    print("static const BMI088_Calib_s s_bmi088_calib = {")
    print("    .gyro = {")
    print(f"        .bias = {{{new[0]:.6f}f, {new[1]:.6f}f, {new[2]:.6f}f}},")
    if bias_col is not None:
        print("        // .bias_tempco = {...}, .temp_ref = 25.0f,   // 自热扫温标定后填")
    print("        // .scale = {...}, .misalign = {...},         // yaw 轴自转标定后填")
    print("    },")
    print("    // .acc = { .bias = {...}, .scale = {...}, .misalign = {...} },  // 六位置标定后填")
    print("};")
    temp_note = (f"{np.nanmean(tp[sl]):.1f}℃" if tp is not None and not np.all(np.isnan(tp[sl]))
                 else "温度 N/A")
    print(f"// 数据: {a.csv}，{t[i0]:.0f}~{t[i1 - 1]:.0f}s（{sec:.0f}s 静止），{temp_note}"
          "（换 IMU/拆装/改安装方式必须重标重烧）")

    # ---- 可选：输出消掉残差的 csv，方便在 VOFA 里再确认一遍 ----
    if a.csv_resid:
        out = d.copy()
        out[:, 4:7] = gy - resid
        out[:, 13:16] = np.tile(new, (n, 1))
        np.savetxt(a.csv_resid, out, delimiter=",", header=",".join(f"I{i}" for i in range(ncol)),
                   comments="", fmt="%.6f")
        print(f"\n已写出残差修正后的数据: {a.csv_resid}（CH4-6 变成 ~0，可直接丢进 VOFA 看图）")


if __name__ == "__main__":
    main()
