#!/usr/bin/env python3
"""IST8310 磁标定脚本：椭球拟合 → 硬铁/软铁 → 打印可粘贴的 C 代码

配合 drvlib_bmi088_ist8310_eskf 的 `vofa_enable = 1` 使用：模块每帧把数据写进
VOFA 通道并自己发帧，VOFA+ 上位机存成 CSV，就是这个脚本的输入。

通道约定（详见 drvlib_bmi088_ist8310_eskf.h 头注释）：
    CH16-19 名义四元数 w/x/y/z            CH23  mag_used (0/1)
    CH20-22 标定修正后 mag x/y/z (µT)     CH24  acc 残差模长
                                          CH25  mag 残差模长
CSV 第 0 列 I0 是 DWT 时间戳 (us)，第 n 列即 CHn；本脚本默认取第 20-22 列。

⚠ 拟合的是**当前标定之后**的残差畸变。流程是迭代两遍收敛的：
    ① 把 app 里 `s_ist8310_calib` 全 0（= 不修正）→ CH20-22 就是**原始**磁数据，录一段；
    ② 跑本脚本，把打印的 hard_iron/soft_iron 填进 app，重烧；
    ③ 再录一段，跑 `--check` 看残差球半径 std/mean 是否 <2%；不达标就把 ②③ 再来一遍
       （新结果与旧值**复合**，不要只看新值）。
  也可以直接给定 `.hard_iron` 现值，用 `--base-hard/--base-soft` 让脚本做复合。

✦ 纯旋转不可观测：把整条链乘任意旋转矩阵，椭球不变、残差一模一样，所以**安装旋转
  mount_rot 本脚本定不了**，必须另做已知姿态实验标定（见 drvlib_ist8310_calib.h 头注释）。
  本脚本输出的 mount_rot 恒为全 0（= 单位阵），请勿把拟合出来的软铁当成安装旋转。

✦ 绝对模长靠"参考场强"钉死：椭球拟合只能定**形状**（各轴比例），定不了绝对大小，
  必须外部给一个真值。默认用录制段的 |m| 中位，可用 `--field` 覆盖为当地的
  IGRF/WMM 值（如北纬 30° 约 46 µT，北纬 40° 约 52 µT）。脚本会把用到的值打印出来，
  请让 app 里的 `.mag_ref_uT` 与之一致（融合算法的模长门限看的就是它）。

用法：
    python3 ist8310_calib.py mag_raw.csv                    # 拟合并打印 C 代码
    python3 ist8310_calib.py mag_raw.csv --field 50.0       # 用已知场强钉死绝对模长
    python3 ist8310_calib.py mag_after.csv --check          # 只看质检，不要求出系数
    python3 ist8310_calib.py mag_raw.csv --ch 20,21,22      # 改通道（默认即 20-22）
    python3 ist8310_calib.py --selftest                     # 无需 CSV：合成数据自检拟合数学
"""

import argparse
import sys

import numpy as np

CH_DEFAULT = (20, 21, 22)  # 默认取的列号 == 通道号
FIELD_PLAUSIBLE = (15.0, 80.0)  # 地表磁感应强度合理范围 (µT)：通道接错时必然离谱
COVER_MIN = 0.10  # 最小/最大特征值之比下限：低于此说明轨迹近似平面，椭球拟合退化
RESID_OK = 0.02  # 修正后球半径 std/mean 上限


# ══════════════════════ 拟合数学 ══════════════════════

def fit_ellipsoid(m):
    """椭球线性最小二乘：(m-h)ᵀ M (m-h) = 1 ⇒ 硬铁 h、形状矩阵 M = SᵀS

    模型 m'ᵀAm' + bᵀm' = 1（m' = m - mean，把数据先居中，保证 gauge c=-1 成立：
    样本均值就在椭球心附近，原点落在椭球内部，于是"在原点取 -1"这个归一化才合理）。
    配平方得 (m' - c₀)ᵀA(m' - c₀) = k，k = 1 + c₀ᵀAc₀，c₀ = -½A⁻¹b。

    返回 (h, M) 或 (None, 失败原因字符串)。
    """
    mu = m.mean(axis=0)
    d = m - mu
    x, y, z = d[:, 0], d[:, 1], d[:, 2]
    # 每一项是二次型的一个系数，最后一个 1 移到右边作为非齐次项（rg 里的 9 个未知量）
    D = np.column_stack([x * x, y * y, z * z, 2 * x * y, 2 * x * z, 2 * y * z, x, y, z])
    q, *_ = np.linalg.lstsq(D, np.ones(len(d)), rcond=None)

    A = np.array([[q[0], q[3], q[4]],
                  [q[3], q[1], q[5]],
                  [q[4], q[5], q[2]]])
    b = q[6:9]

    ev = np.linalg.eigvalsh(A)
    if ev.min() <= 0:
        # 混合符号 = 单叶/双叶双曲面；全负 = 归一化尺度反了。都不是椭球。
        return None, (f"拟合出的二次型不是正定椭球（特征值 {ev[0]:.3e}, {ev[1]:.3e}, {ev[2]:.3e}）；"
                      "多半是姿态覆盖太窄，或这几列不是磁数据")
    cond = ev.max() / ev.min()
    if cond > 1e4:
        return None, f"椭球病态（条件数 {cond:.1e}）：数据近似平面，绕单轴转不出三维覆盖"

    c0 = -0.5 * np.linalg.solve(A, b)
    k = 1.0 + c0 @ A @ c0
    if k <= 0:
        return None, f"归一化常数 k={k:.3e} ≤ 0，拟合退化"

    return mu + c0, A / k, {"cond": cond}


def soft_iron_from(M, field):
    """M = SᵀS → S（上三角，Cholesky 取转置），再乘 field 使 |S·e| = field

    任意满足 SᵀS = M 的 S 都给出同样的 |S·e|（差一个旋转），取上三角只是选一个
    确定的代表元；旋转分量本来就该由 mount_rot 承担（见文件头"纯旋转不可观测"）。
    """
    L = np.linalg.cholesky(M)  # 下三角，M = L·Lᵀ
    return field * L.T         # 上三角，SᵀS = L·Lᵀ = M


def residuals(m, h, S):
    """修正后的球半径：理想情况恒等于 field（贴合完美），故用 std/mean 度量贴合度"""
    r = np.linalg.norm((m - h) @ S.T, axis=1)
    return r, float(r.std() / r.mean()) if r.mean() > 0 else float("inf")


# ══════════════════════ 输入自检 ══════════════════════

def layout_problems(m, t):
    """通道/数据布局自检。逐通道对不上就会**静默算出错的标定值**，宁可拒绝输出。
    返回问题列表（空 = 可信）。"""
    p = []
    n = np.linalg.norm(m, axis=1)
    med = np.median(n)
    if not FIELD_PLAUSIBLE[0] < med < FIELD_PLAUSIBLE[1]:
        p.append(f"|m| 中位 {med:.1f} µT 不像地磁场（合理 {FIELD_PLAUSIBLE[0]:.0f}~"
                 f"{FIELD_PLAUSIBLE[1]:.0f}）→ 这几列可能不是磁数据")
    if np.median(np.diff(t)) <= 0:
        p.append("时间戳不递增 → 取列错位，或 CSV 首行不是表头")

    # 姿态覆盖：逐个轴看变化幅度，全都不动 = 录的时候设备没转，拟合无意义
    span = m.max(axis=0) - m.min(axis=0)
    if span.max() < 5.0 * med / 50.0:
        p.append(f"三轴变化幅度只有 {span.max():.2f} µT → 录制期间几乎没转动，"
                 "椭球拟合需要各个方向都摆到（至少 3 个不平面的姿态簇）")
    cv = np.linalg.eigvalsh(np.cov(m.T))
    if cv.max() > 0 and cv.min() / cv.max() < COVER_MIN:
        p.append(f"样本协方差最小/最大特征值 = {cv.min() / cv.max():.3f} < {COVER_MIN} → "
                 "轨迹近似一条线/一个平面，绕单轴转是标不出三维椭球的")

    # 磁计自身噪声：静止段（相邻点几乎不变）里看抖动，>2µT 说明数据被电机/电源污染
    d = np.linalg.norm(np.diff(m, axis=0), axis=1)
    quiet = d[d < np.percentile(d, 20)]
    if len(quiet) > 10 and np.median(quiet) > 2.0:
        p.append(f"最静止 20% 段的逐点跳变中位仍有 {np.median(quiet):.2f} µT → "
                 "磁数据被干扰（电机电流/电源线），先排除干扰源再标")
    return p


# ══════════════════════ 输出 ══════════════════════

def print_c_code(h, S, field, note):
    print("\n── 粘到 app 的 s_ist8310_calib ──")
    print("/* 磁标定（椭球拟合，PC 端离线完成）：换磁计/拆装/改走线必须重标重烧 */")
    print("static const IST8310Calib_s s_ist8310_calib = {")
    print(f"    .hard_iron = {{{h[0]:.4f}f, {h[1]:.4f}f, {h[2]:.4f}f}},")
    print("    .soft_iron = {")
    for i in range(3):
        print(f"        {{{S[i][0]:.6f}f, {S[i][1]:.6f}f, {S[i][2]:.6f}f}},"
              + ("   /* 把椭球拉回球，行主序 */" if i == 0 else ""))
    print("    },")
    print("    /* .mount_rot 由已知姿态实验单独标定，拟合定不了（纯旋转不可观测）*/")
    print("    /* .scale    椭球拟合已自带归一化，通常留 0（= 不修正）*/")
    print("};")
    print(f"/* 数据: {note} */")
    print(f"/* 参考场强 {field:.2f} µT → app 里 .mag_ref_uT 应与之一致 */")


# ══════════════════════ 自检模式（无需硬件） ══════════════════════

def selftest():
    """用合成数据验证拟合数学：真值硬铁/软铁 + 球面均匀姿态 → 看能否复原。
    软铁只能定到 M = SᵀS（差一个旋转），故比对 M 与硬铁，不比 S 本身。"""
    print("── 合成数据自检 ──")
    rng = np.random.default_rng(20260928)
    field_true = 50.0
    h_true = np.array([12.0, -8.0, 20.0])
    S_true = np.array([[1.10, 0.03, 0.00],
                       [0.00, 0.92, 0.05],
                       [0.02, 0.00, 1.04]])  # 非上三角，考验"只比 M"的说法

    u = rng.normal(size=(4000, 3))
    u /= np.linalg.norm(u, axis=1, keepdims=True)  # 单位球上均匀取向
    m_perfect = field_true * u
    m_raw = m_perfect @ np.linalg.inv(S_true).T + h_true
    m_raw += rng.normal(scale=0.2, size=m_raw.shape)  # 0.2µT 噪声

    h_fit, M_fit, info = fit_ellipsoid(m_raw)
    if h_fit is None:
        print(f"  [FAIL] 拟合失败：{M_fit}")
        return 1
    # 绝对模长：合成数据里 field 已知，直接钉死（实际用时靠 --field 或 |m| 中位）
    S_fit = soft_iron_from(M_fit, field_true)

    err_h = np.abs(h_fit - h_true).max()
    # 软铁只可定到 M = SᵀS（差一个旋转），故比 SᵀS 而不是 S；
    # M_fit 的量纲是 1/µT²（(m-h)ᵀM(m-h)=1），乘 field² 才是 SᵀS 的量纲。
    M_true = S_true.T @ S_true
    M_got = S_fit.T @ S_fit
    err_M = np.abs(M_got - M_true).max() / np.abs(M_true).max()
    _, res = residuals(m_raw, h_fit, S_fit)
    print(f"  硬铁复原误差 max|Δh| = {err_h:.3e} µT（真值 {h_true}）")
    print(f"  软铁复原误差 max|Δ(SᵀS)|/|SᵀS| = {err_M:.3e}（只比 M = SᵀS，差一个旋转）")
    print(f"  修正后球半径 std/mean = {res:.3e}（阈 {RESID_OK}）")
    print(f"  椭球条件数 = {info['cond']:.3f}")
    ok = err_h < 0.1 and err_M < 1e-3 and res < RESID_OK
    print("  结果: " + ("全部 PASS" if ok else "FAIL"))
    return 0 if ok else 1


# ══════════════════════ 主流程 ══════════════════════

def main():
    ap = argparse.ArgumentParser(description="IST8310 磁标定（椭球拟合 → 硬铁/软铁）")
    ap.add_argument("csv", nargs="?", help="VOFA+ 录的 CSV")
    ap.add_argument("--ch", default="20,21,22", help="mag 三列列号（= 通道号），默认 20,21,22")
    ap.add_argument("--skip", type=float, default=0.0, help="丢掉开头 N 秒")
    ap.add_argument("--tail", type=float, default=0.0, help="只取末尾 N 秒")
    ap.add_argument("--field", type=float, default=None,
                    help="参考场强 (µT)，建议填当地 IGRF 值；缺省用 |m| 中位")
    ap.add_argument("--check", action="store_true", help="只做质检与残差评估，不要求出系数")
    ap.add_argument("--selftest", action="store_true", help="无需 CSV：合成数据自检拟合数学")
    a = ap.parse_args()

    if a.selftest:
        return selftest()
    if not a.csv:
        ap.error("需要 CSV（或用 --selftest）")

    cols = [int(c) for c in a.ch.split(",")]
    if len(cols) != 3:
        ap.error("--ch 需要三个列号")

    d = np.loadtxt(a.csv, delimiter=",", skiprows=1)
    if d.ndim == 1:
        d = d[None, :]
    t = d[:, 0] * 1e-6
    t -= t[0]
    if d.shape[1] <= max(cols):
        sys.exit(f"CSV 只有 {d.shape[1]} 列，取不到第 {max(cols)} 列（确认 vofa_enable 已打开）")
    m_all = d[:, cols]

    print(f"文件: {a.csv}")
    print(f"数据: {len(d)} 点 / {t[-1]:.1f}s / {d.shape[1] - 1} 通道，mag 取第 {cols} 列")

    i0 = int(np.searchsorted(t, a.skip))
    if a.tail > 0:
        i0 = max(i0, int(np.searchsorted(t, t[-1] - a.tail)))
    if len(d) - i0 < 100:
        sys.exit(f"窗口太短（{len(d) - i0} 点），检查 --skip/--tail")
    sl = slice(i0, len(d))
    m = m_all[sl]
    print(f"分析窗口: {t[i0]:.1f}~{t[-1]:.1f}s（{len(m)} 点）")

    print("\n── 原始数据 ──")
    n = np.linalg.norm(m, axis=1)
    print(f"  |m| 均值 {n.mean():.2f} µT，范围 {n.min():.2f}~{n.max():.2f}，"
          f"σ {n.std():.3f}   ← 已标定好的话应≈0（球），未标定时椭球越扁越大")
    for k, name in enumerate("xyz"):
        print(f"  m{name}: 均值 {m[:, k].mean():8.3f}  σ {m[:, k].std():7.3f}  "
              f"范围 {m[:, k].min():8.3f}~{m[:, k].max():8.3f}")
    cv = np.linalg.eigvalsh(np.cov(m.T))
    print(f"  协方差特征值 {cv[0]:.2f} / {cv[1]:.2f} / {cv[2]:.2f}"
          f"（比值 {cv[0] / cv[2]:.3f}，越高姿态覆盖越好）")

    probs = layout_problems(m, t[sl])
    h_fit = M_fit = None
    if probs and not a.check:
        print("\n⚠ 输入自检没通过 —— 拒绝输出标定值（粘进去就是错的）：")
        for x in probs:
            print(f"   - {x}")
        print("   确认是 vofa_enable=1 的固件录的，且录制时把设备在各个方向都摆到了")
        return 2
    for x in probs:
        print(f"  ⚠ {x}")

    h_fit, M_fit, info = fit_ellipsoid(m)
    if h_fit is None:
        print(f"\n✗ 椭球拟合失败：{M_fit}")
        return 2

    field = a.field if a.field is not None else float(np.median(n))
    S_fit = soft_iron_from(M_fit, field)
    r, res = residuals(m, h_fit, S_fit)

    print("\n── 拟合结果 ──")
    print(f"  硬铁偏置 h = [{h_fit[0]:8.4f}, {h_fit[1]:8.4f}, {h_fit[2]:8.4f}] µT")
    if a.field is None:
        print(f"  参考场强: {field:.2f} µT（缺省 = |m| 中位；建议用 --field 填当地 IGRF 值）")
    else:
        print(f"  参考场强: {field:.2f} µT（--field 给定）")
    print(f"  软铁矩阵 S（上三角代表元，SᵀS = M）:")
    for i in range(3):
        print(f"      [{S_fit[i][0]:9.6f} {S_fit[i][1]:9.6f} {S_fit[i][2]:9.6f}]")
    sv = np.linalg.svd(S_fit, compute_uv=False)
    print(f"  S 奇异值 {sv[0]:.4f} / {sv[1]:.4f} / {sv[2]:.4f}"
          f"（比值 {sv[0] / sv[2]:.4f}，1.0 = 不需要软铁修正）")
    print(f"  椭球条件数 {info['cond']:.3f}")

    print("\n── 修正效果 ──")
    print(f"  修正后球半径: 均值 {r.mean():.3f} µT  σ {r.std():.4f}  "
          f"std/mean {res * 100:.3f}%（阈 {RESID_OK * 100:.0f}%）")
    print("  判定: " + ("OK，椭球拉成了球" if res < RESID_OK else
                       "⚠ 贴合还不够 —— 姿态覆盖不全或存在非线性干扰（电机电流随位置变），"
                       "把两个来源排除后重录"))

    if a.check:
        return 0 if res < RESID_OK else 2

    print_c_code(h_fit, S_fit, field,
                 f"{a.csv}，{t[i0]:.0f}~{t[-1]:.0f}s（{len(m)} 点）")
    print("\n提示: 这是**当前标定之后**的残差。若是第二次跑，请把结果与 app 里已有的")
    print("      hard_iron/soft_iron **复合**（h_new = h_old + S_old⁻¹·h_fit，")
    print("      S_new = S_fit·S_old 的 M 形式叠加），而不是直接覆盖。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
