#!/usr/bin/env python3
"""VOFA+ 通道日志分析：定位「设定值突然掉到 0」的事件并给出前后文。

配套 app_shoot.c 的调试通道表（**阶跃/扫档版本**：拨盘还有目标转速通道的那一版）：
    CH1 目标转速(rad/s)  CH2 实际转速  CH3 误差  CH4 总输出/电流指令(A)
    CH5 P项  CH6 I项  CH7 D项  CH8 实测电流(A)  CH9 本拍 dt(ms)
    CH10 摩擦前馈(A)  CH11 原始旋钮(ch9, -1~1)  CH12 原始反馈转速(rad/s，未过低通)

⚠ 2026-10-10 起拨盘改「定速巡航」，通道变了：CH1 = 累计行程、CH11 = 状态机状态，日志里
   **没有目标转速通道**。拿新日志跑本脚本会按默认列名把"行程"当"设定值"读，列自检过得去但
   结果全是假象；分析新日志请用 analyze_trigger_cruise.py（专为定速巡航段写的）。

CSV 布局（VOFA+ JustFloat 导出）：首行表头 I0,I1,...；I0 = 时间戳(DWT µs)，
I1~ 依次对应 CH1~。

用法：
    python3 analyze_vofa.py ../../ignore/vofa+.csv
    python3 analyze_vofa.py log.csv --setpoint I1 --measure I2 --dt I9 --eps-ratio 0.01
    python3 analyze_vofa.py log.csv --dump-events events.csv
    python3 analyze_vofa.py log.csv --tune          # 阶跃响应 + 负载线 + PID 建议
    python3 analyze_vofa.py log.csv --map           # ch9 → 稳态转速 映射/线性度/低速卡住
    python3 analyze_vofa.py log.csv --stiction      # 静摩擦突破电流（"卡→动"跳变前一拍的输出）
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# 默认列名 = app_shoot.c 当前调试块
DEFAULT_COLS = {
    "time": "I0",
    "setpoint": "I1",
    "measure": "I2",
    "error": "I3",
    "output": "I4",
    "p": "I5",
    "i": "I6",
    "d": "I7",
    "current": "I8",
    "dt": "I9",
}

def load_csv(path: Path) -> pd.DataFrame:
    """读 CSV 并自检布局（表头必须是 I0,I1,... 顺序编号）。"""
    if not path.is_file():
        sys.exit(f"[错误] 文件不存在: {path}")
    df = pd.read_csv(path)
    cols = list(df.columns)
    if not cols or cols[0] != "I0":
        sys.exit(f"[错误] 表头第一列应为 I0，实际为 {cols[0] if cols else '(空)'}")
    expect = [f"I{k}" for k in range(len(cols))]
    if cols != expect:
        sys.exit(f"[错误] 表头不是连续编号 I0..I{len(cols) - 1}: {cols[:6]}...")
    if len(df) < 3:
        sys.exit(f"[错误] 数据行太少: {len(df)}")
    return df


def to_seconds(ts: np.ndarray, unit: str) -> tuple[np.ndarray, str]:
    """时间戳 → 秒。unit=auto 时按列内中位间隔猜单位。"""
    t = ts.astype(np.float64)
    d = np.diff(t)
    d = d[d > 0]
    med = float(np.median(d)) if d.size else 0.0
    if unit == "auto":
        # DWT_GetTimeUs 给的是 µs：500Hz 任务的中位间隔 ~2000
        if med >= 1000:
            unit = "us"
        elif med >= 1:
            unit = "ms"
        else:
            unit = "s"
    scale = {"us": 1e-6, "ms": 1e-3, "s": 1.0}[unit]
    return (t - t[0]) * scale, unit


def find_dropouts(setpoint: np.ndarray, eps: float, bounds: np.ndarray | None = None) -> list[tuple[int, int]]:
    """返回「非零运行中间夹着 0」的样本下标（两侧都有显著输出的才是异常掉 0）。

    bounds 处的行索引（时间戳中断）强制断开：否则断点两侧的 0/非 0 会被误判成一次掉落。
    """
    active = np.abs(setpoint) > eps
    hard = set(int(x) for x in (bounds if bounds is not None else ()))
    out = []
    n = len(setpoint)
    i = 0
    while i < n:
        if active[i]:
            i += 1
            continue
        j = i
        while j < n and not active[j] and j not in hard:
            j += 1
        if j == i:  # 当前位置本身就是硬边界，跳过它
            i += 1
            continue
        # [i, j) 是一段连续"零"。要求它两边都有活跃样本，才算运行中的掉落
        if i > 0 and active[i - 1] and j < n and active[j]:
            out.append((i, j))
        i = j
    return out


def runs_of(active: np.ndarray, bounds: np.ndarray | None = None) -> list[tuple[int, int]]:
    """连续的活跃区间 [start, end) 列表。bounds 处的行索引强制断开。"""
    hard = set(int(x) for x in (bounds if bounds is not None else ()))
    out = []
    n = len(active)
    i = 0
    while i < n:
        if not active[i]:
            i += 1
            continue
        j = i + 1
        while j < n and active[j] and j not in hard:
            j += 1
        out.append((i, j))
        i = j
    return out


def find_levels(ref: np.ndarray, tol: float, min_samples: int, bounds: np.ndarray | None = None) -> list[tuple[int, int]]:
    """把设定值切成"近似恒定"的电平段 [start, end)。

    判据是相邻样本跳变 > tol：遥控器旋钮量化后的相邻档位差远大于 tol，
    而同一档位内的抖动（如 767.05/768.17 两个相邻 ADC 码）远小于 tol，
    所以这个切法既能把不同大小的阶跃分开，又不会把一档切成许多段。
    bounds 处的行索引（时间戳中断）作为硬边界并入，保证段不跨断点。
    """
    edges = list(np.flatnonzero(np.abs(np.diff(ref)) > tol) + 1)
    if bounds is not None:
        edges = sorted(set(edges) | {int(x) for x in bounds})
    cuts = [0, *edges, len(ref)]
    return [(a, b) for a, b in zip(cuts[:-1], cuts[1:]) if b - a >= min_samples]


def ss_slice(a: int, b: int, t_s: np.ndarray, frac: float, max_s: float) -> slice:
    """取段尾的稳态窗口（段长的 frac，但不超过 max_s）。"""
    dur = t_s[b - 1] - t_s[a]
    win = min(max(dur * frac, 0.2), max_s)
    i = int(np.searchsorted(t_s[:b], t_s[b - 1] - win, side="left"))
    return slice(max(i, a), b)


def tune_report(t_s, ref, meas, cur, out, args, period_s: float, eps: float, bounds=None,
                pch=None, ich=None, ffch=None) -> None:
    """阶跃响应 / 稳态负载线 / PID 参数建议（--tune）。

    稳态平衡式 out_ss = ff_ss + p_ss + i_ss（kd=0 时 d=0），逐项列出来就能判断
    「前馈补到位没有」：前馈补得准 → i_ss ≈ 0；补过头 → i_ss 反号去抵消。
    """
    min_samples = max(int((args.min_level_ms / 1000.0) / period_s), 10)
    levels = find_levels(ref, args.level_tol, min_samples, bounds)
    if not levels:
        print("\n== 调参分析 ==\n  （没有找到足够长的恒定电平段，检查 --level-tol/--min-level-ms）")
        return
    rows = []
    for k, (a, b) in enumerate(levels):
        sl = ss_slice(a, b, t_s, args.ss_frac, args.ss_max_ms / 1000.0)
        ref_ss = float(np.median(ref[sl]))
        meas_ss = float(np.median(meas[sl]))
        dur = t_s[b - 1] - t_s[a]
        rows.append(
            {
                "idx": k,
                "t0": t_s[a],
                "dur_s": dur,
                "ref_ss": ref_ss,
                "meas_ss": meas_ss,
                "err_ss": ref_ss - meas_ss,
                "droop_pct": 100.0 * (ref_ss - meas_ss) / ref_ss if abs(ref_ss) > 1e-6 else 0.0,
                "cur_ss": float(np.median(np.abs(cur[sl]))),
                "out_ss": float(np.median(out[sl])),
                "p_ss": float(np.median(pch[sl])) if pch is not None else float("nan"),
                "i_ss": float(np.median(ich[sl])) if ich is not None else float("nan"),
                "ff_ss": float(np.median(ffch[sl])) if ffch is not None else float("nan"),
                "ripple": float(np.std(meas[sl])),
                "ref_std": float(np.std(ref[sl])),
                "a": a,
                "b": b,
            }
        )
    print(f"\n== 电平分段（容差 {args.level_tol:.2f}，最短 {args.min_level_ms} ms）: {len(levels)} 段 ==")
    lv = pd.DataFrame(rows)
    show = lv[["idx", "t0", "dur_s", "ref_ss", "meas_ss", "err_ss", "droop_pct", "cur_ss", "out_ss", "ripple", "ref_std"]]
    with pd.option_context("display.max_columns", None, "display.width", 200, "display.float_format", "{:.3f}".format):
        print(show.to_string(index=False))
    print("  （err_ss = 段尾稳态的 目标-实际；cur_ss = 实测电流中位；ripple = 实际转速标准差）")
    if pch is not None:
        print("\n== 稳态分量平衡（out_ss = ff_ss + p_ss + i_ss；kd=0 故 d 略）==")
        print("  idx   ref_ss    out_ss    ff_ss     p_ss     i_ss   ff+pid  校验(out-和)")
        for r in rows:
            s = r["ff_ss"] + r["p_ss"] + r["i_ss"]
            print(f"  {r['idx']:<5} {r['ref_ss']:8.2f} {r['out_ss']:9.3f} {r['ff_ss']:8.3f} "
                  f"{r['p_ss']:8.3f} {r['i_ss']:8.3f} {s:8.3f} {r['out_ss'] - s:11.3f}")
        on = [r for r in rows if abs(r["ref_ss"]) > eps and abs(r["i_ss"]) == abs(r["i_ss"])]
        if on:
            w = np.array([r["meas_ss"] for r in on])
            iv = np.array([r["i_ss"] for r in on])
            print(f"\n  积分项余量 i_ss：中位 {np.median(iv):+.4f} A，|中位| {abs(np.median(iv)):.4f} A，"
                  f"最大 |i_ss| {np.max(np.abs(iv)):.4f} A")
            print("  （i_ss 是「前馈没补掉的那部分」的直接读数：≈0 = 前馈补到位；"
                  "系统性地同号 = 前馈幅值不够；反号 = 补过头）")

    # --- 上升沿动态：逐段把"上一段"当阶跃起点 ---
    print("\n== 阶跃上升沿（含电流饱和段与去饱和后的表观闭环 τ）==")
    print("  电平段  ref      v0     t10-90   ovs%   settle±2%  cur_pk  out_pk   t(去饱和)  τ_cl")
    taus = []
    for r in rows:
        a, b = r["a"], r["b"]
        if r["ref_ss"] <= 0.0 or abs(r["ref_ss"]) < eps:
            continue
        v0 = float(np.median(meas[max(a - 5, 0):a + 1])) if a >= 1 else 0.0
        dv = r["meas_ss"] - v0
        if abs(dv) < 0.2 * abs(r["ref_ss"]):
            continue  # 不是从近零起步的阶跃（段内小台阶），跳过
        lo, hi = v0 + 0.1 * dv, v0 + 0.9 * dv
        seg_meas = meas[a:b]
        t_seg = t_s[a:b]
        i10 = np.flatnonzero(seg_meas >= lo)
        i90 = np.flatnonzero(seg_meas >= hi)
        t10_90 = (t_seg[i90[0]] - t_seg[i10[0]]) * 1e3 if (i10.size and i90.size) else float("nan")
        ovs = 100.0 * (float(np.max(seg_meas)) - r["meas_ss"]) / abs(dv)
        band = max(0.02 * abs(dv), 2.0 * r["ripple"])
        dev = np.abs(seg_meas - r["meas_ss"]) > band
        i_set = int(np.flatnonzero(dev)[-1] + 1) if np.any(dev) else 0
        settle = (t_seg[min(i_set, seg_meas.size - 1)] - t_seg[0]) * 1e3
        cur_abs = np.abs(cur[a:b])
        # 电流饱和期间是开环（等加速），这一段上升时间由"可用电流 vs 惯量"决定，与 kp 无关，
        # 不能拿它拟合闭环 τ。去饱和点取"电流峰之后首次跌到平台 90%"——那一刻误差已小到
        # P 项不再顶到驱动限幅，之后进入线性区；再用"距稳态还差 63%"的穿越时间当 τ
        # （一阶逼近是精确的，且比 log 拟合抗噪声）。
        ipk = int(np.argmax(cur_abs))
        plateau = float(cur_abs[ipk])
        tau, t_desat = float("nan"), float("nan")
        tail = np.flatnonzero(cur_abs[ipk:] < 0.9 * plateau) if plateau > 1.0 else np.array([], dtype=int)
        resid = r["meas_ss"] - seg_meas  # 上升段恒为正：距最终值还差多少
        if tail.size and ipk + int(tail[0]) < seg_meas.size - 5:
            d = ipk + int(tail[0])
            t_desat = (t_seg[d] - t_seg[0]) * 1e3
            cross = np.flatnonzero(resid[d:] <= 0.368 * resid[d]) if resid[d] > 0 else np.array([], dtype=int)
            if cross.size:
                tau = float(t_seg[d + int(cross[0])] - t_seg[d])
                taus.append((r["idx"], r["ref_ss"], tau))
        print(
            f"  #{r['idx']:<5} {r['ref_ss']:8.2f} {v0:7.2f} {t10_90:8.1f}ms {ovs:6.1f} {settle:8.1f}ms "
            f"{plateau:7.2f} {float(np.max(np.abs(out[a:b]))):7.2f}   {t_desat:8.1f}ms {tau * 1e3:8.1f}ms"
        )
    print("  （t10-90 = 10%→90% 上升时间，受电流饱和限制，与 kp 无关；cur_pk = 电流平台=驱动限幅；")
    print("    t(去饱和) = 从阶跃到电流回落（误差小到不再顶限幅）的时间；τ_cl = 之后距稳态 63% 的穿越时间）")

    # --- 长段分块剖面：找加载/卸载（电流阶跃 = 负载阶跃）---
    print(f"\n== 长段分块剖面（段长 > {args.chunk_s:.0f}s，块长 {args.chunk_s:.0f}s）==")
    for r in rows:
        if r["dur_s"] < 2.0 * args.chunk_s:
            continue
        a, b = r["a"], r["b"]
        print(f"  段 #{r['idx']} t={t_s[a]:.2f}~{t_s[b - 1]:.2f}s  {r['ref_ss']:.2f} rad/s   块中位: t_mid / ref / meas / err / 电流")
        n = int(np.ceil((t_s[b - 1] - t_s[a]) / args.chunk_s))
        step = max(int(args.chunk_s / period_s), 1)
        for c in range(n):
            lo, hi = a + c * step, min(a + (c + 1) * step, b)
            if hi - lo < 20:
                continue
            print(
                f"    {0.5 * (t_s[lo] + t_s[hi - 1]):8.2f}  {np.median(ref[lo:hi]):8.2f}  {np.median(meas[lo:hi]):8.2f}  "
                f"{np.median(ref[lo:hi] - meas[lo:hi]):8.2f}  {np.median(np.abs(cur[lo:hi])):7.3f}"
            )

    # --- 稳态扰动事件：段内转速被外部负载拉低/顶飞后的跌落与恢复 ---
    if not args.no_disturb:
        print("\n== 稳态扰动事件（段内 |meas-ref| 超门限；抗扰动）==")
        print("   段   t_start    t_min     跌落  跌落%  到最低   恢复±2%  电流峰   持续")
        total = 0
        gap = int((args.disturb_gap_ms / 1000.0) / period_s)
        for r in rows:
            if abs(r["ref_ss"]) < eps:  # 相对活跃门限，量纲无关（拨盘 20 rad/s 也适用）
                continue
            a, b = r["a"], r["b"]
            seg, ref_seg = meas[a:b], ref[a:b]
            thr = max(args.disturb_ratio * abs(r["ref_ss"]), args.disturb_min, 2.5 * r["ripple"])
            bad = np.abs(seg - ref_seg) > thr
            i = 0
            while i < bad.size:
                if not bad[i]:
                    i += 1
                    continue
                j, last_bad = i + 1, i
                while j < bad.size:
                    if bad[j]:
                        last_bad = j
                        j += 1
                    elif j - last_bad <= gap:
                        j += 1
                    else:
                        break
                u, v = i, last_bad + 1
                i = j
                # 段首的启动阶跃不是扰动（目标从 0 拉到平台，meas 追赶天经地义）
                if t_s[a + u] - t_s[a] < 0.05:
                    continue
                kmin = u + int(np.argmin(seg[u:v]))
                dip = float(ref_seg[kmin] - seg[kmin])
                cur_pk = float(np.max(np.abs(cur[a + u:a + v])))
                band = max(0.02 * abs(r["ref_ss"]), 2.0 * r["ripple"])
                back = np.flatnonzero(np.abs(seg[kmin:v] - ref_seg[kmin:v]) <= band)
                rec = (t_s[a + kmin + int(back[0])] - t_s[a + kmin]) * 1e3 if back.size else float("nan")
                print(f"  #{r['idx']:<3} {t_s[a + u]:8.3f}s {t_s[a + kmin]:8.3f}s {dip:8.1f} "
                      f"{100 * dip / abs(r['ref_ss']):6.2f} {(t_s[a + kmin] - t_s[a + u]) * 1e3:7.1f}ms "
                      f"{rec:8.1f}ms {cur_pk:6.2f} {(t_s[a + v - 1] - t_s[a + u]) * 1e3:7.1f}ms")
                total += 1
        print(f"  合计 {total} 次；门限 = max(0.5%·ref, {args.disturb_min:.0f}, 2.5·ripple)；"
              f"恢复±2% = 从最低点回到稳态带用时（nan=到段尾未回）")

    # --- 稳态负载线：维持转速所需的电流 = f(转速) ---
    # 段尾稳态点（#22 的稳态窗在段尾，落在卸载之后），故这是"空载负载线"。
    # 隐含 kp = 电流/稳态差，应当逐点等于配置的 kp（0.1）——对不上说明通道/量纲有问题。
    pts = lv[(lv["meas_ss"].abs() > eps) & (lv["ref_ss"].abs() > eps)].copy()
    if len(pts) >= 2:
        print("\n== 稳态工作点（段尾稳态；带载段的电流会含负载分量，不再是空载负载线）==")
        print("   ref_ss    meas_ss    err_ss   droop%   电流[A]   隐含kp=电流/err")
        for _, r in pts.sort_values("meas_ss").iterrows():
            kp_imp = r["cur_ss"] / r["err_ss"] if abs(r["err_ss"]) > 1e-6 else float("nan")
            print(f"  {r['ref_ss']:8.2f} {r['meas_ss']:10.2f} {r['err_ss']:9.2f} {r['droop_pct']:8.3f} {r['cur_ss']:9.3f} {kp_imp:14.4f}")
        c1, c0 = np.polyfit(pts["meas_ss"], pts["cur_ss"], 1)
        pred = c1 * pts["meas_ss"] + c0
        ss_tot = float(np.sum((pts["cur_ss"] - pts["cur_ss"].mean()) ** 2))
        r2 = 1.0 - float(np.sum((pts["cur_ss"] - pred) ** 2)) / ss_tot if ss_tot > 0 else 0.0
        print(f"\n  拟合 I(ω) = {c0:+.4f} + {c1 * 1000:.4f}e-3·ω  [A]，R²={r2:.4f}")
        print("  （R² 不是 1 是因为摩擦随温度/时间在漂——正是这个漂移需要积分去消）")
        wmax = float(pts["meas_ss"].max())
        print(f"  ⇒ 纯比例时 {wmax:.0f} rad/s 处的稳态差 = I/kp：")
        for kp in (0.1, 0.2, 0.3, 0.5):
            e = (c0 + c1 * wmax) / kp
            print(f"      kp={kp:<4} err≈{e:6.2f} rad/s ({100 * e / wmax:.2f}%)，需要积分补 {c0 + c1 * wmax:.2f} A")
        if taus:
            print(f"\n== τ_cl（{len(taus)} 个阶跃，去饱和后的表观闭环时间常数）==")
            for i, rv, tau in taus:
                print(f"  段 #{i:<4} ref={rv:8.2f} → τ={tau * 1e3:7.1f} ms")
            print("  积分零点压在闭环带宽的 1/5：T_i = 5·τ_cl，ki = kp/T_i")
            for kp in (0.2, 0.3, 0.5):
                for tau in sorted({round(t[2], 3) for t in taus}):
                    print(f"      kp={kp:<5} τ_cl={tau * 1e3:6.1f}ms → T_i={5 * tau * 1e3:6.1f}ms, ki={kp / (5 * tau):.3f} 1/s")


def map_report(t_s, ref, meas, knob, args, period_s: float, eps: float, bounds=None) -> None:
    """旋钮 → 目标 → 实测转速 的静态映射（--map），用来回答"速度和 ch9 是不是线性的"。

    把日志按旋钮档位切段（同一档位内的 ADC 抖动远小于 --map-tol），每段取段尾稳态转速，
    于是得到"旋钮 → 稳态转速"的曲线点；再逐点算增益 转速/旋钮 —— 增益恒定为线性、
    低档位增益塌下来即非线性（死区/静摩擦）。只统计"旋钮给了正指令"的样本：
    ch7 断开时 ref 被清 0，那不是映射的一部分。
    """
    keps = args.knob_eps
    min_samples = max(int((args.min_level_ms / 1000.0) / period_s), 10)
    # 活跃判据只看 ref（真正的指令），不能看旋钮符号 —— 半行程映射下低转速对应负旋钮，
    # 用"旋钮>0"筛会把整个低速段漏掉。ch7 断开时 ref=0，天然被排除。
    on = np.abs(ref) > eps
    n_on = int(on.sum())
    print(f"\n== 旋钮 → 转速 映射（--map；旋钮列 {args.knob}）==")
    print(f"  有指令的样本 {n_on}/{knob.size}（{100.0 * n_on / knob.size:.1f}%）；"
          f"这些样本的旋钮范围 [{knob[on].min():.3f}, {knob[on].max():.3f}]"
          f"（全程 [{knob.min():.3f}, {knob.max():.3f}]）")
    if n_on == 0:
        print("  （没有任何 ref 非零的样本：确认固件里 CH11 真的写了旋钮）")
        return
    # 映射自检：ref 与旋钮的关系是哪种约定
    for name, pred in (("全行程 (ch+1)/2", np.clip((knob + 1.0) * 0.5, 0, 1) * 100.0),
                       ("半行程 clamp(ch,0,1)", np.clip(knob, 0, 1) * 100.0)):
        dev = float(np.abs(pred[on] - ref[on]).max())
        print(f"  映射自检 ref=?{name}: 最大偏差 {dev:.4f} rad/s{'  ← 命中' if dev < 0.05 else ''}")

    rows = []
    for a, b in runs_of(on, bounds):
        for c, d in find_levels(knob[a:b], args.map_tol, min_samples):
            lo, hi = a + c, a + d
            sl = ss_slice(lo, hi, t_s, args.ss_frac, args.ss_max_ms / 1000.0)
            k_med = float(np.median(knob[lo:hi]))
            r_med = float(np.median(ref[lo:hi]))
            m_ss = float(np.median(meas[sl]))
            pp = float(np.max(meas[sl]) - np.min(meas[sl]))
            # 卡住占比：整档内 |实测| 低于带宽的样本比例。低速档极限环的头部指标 ——
            # 平均转速可能还"跟得上"，但大部分时间其实死住不动、偶尔窜一下。
            stuck_pct = 100.0 * float(np.mean(np.abs(meas[lo:hi]) < args.stick_band))
            rows.append(
                {
                    "t0": t_s[lo],
                    "dur_s": t_s[hi - 1] - t_s[lo],
                    "n": hi - lo,
                    "knob": k_med,
                    "ref": r_med,
                    "meas_ss": m_ss,
                    "pp": pp,
                    "pp_ref": pp / abs(r_med) if abs(r_med) > 1e-6 else float("nan"),
                    "stuck_pct": stuck_pct,
                    "gain": m_ss / k_med if abs(k_med) > 1e-9 else float("nan"),
                    "droop_pct": 100.0 * (r_med - m_ss) / r_med if abs(r_med) > 1e-6 else float("nan"),
                }
            )
    if not rows:
        print("  （切不出足够长的档位段，调小 --min-level-ms 或放大 --map-tol）")
        return
    mp = pd.DataFrame(rows).sort_values("ref").reset_index(drop=True)
    show = mp[["t0", "dur_s", "n", "knob", "ref", "meas_ss", "pp", "pp_ref", "stuck_pct", "droop_pct"]]
    with pd.option_context("display.max_columns", None, "display.width", 200, "display.float_format", "{:.3f}".format):
        print(show.to_string(index=False))
    print(f"  （meas_ss = 稳态窗中位转速；pp = 该窗峰峰；pp_ref = pp/ref —— 这个比到大就说明"
          f"已经不是「跟踪」，而是粘滑/极限环；stuck_pct = 整档内 |实测| < {args.stick_band:.2f} rad/s "
          f"的样本比例（死住不动的时间占比）；droop_pct = (ref-实测)/ref）")

    # 线性度：转速 vs 目标（映射的目的就是这条线直不直）
    x = mp["ref"].to_numpy()
    y = mp["meas_ss"].to_numpy()
    if x.size >= 3:
        c1, c0 = np.polyfit(x, y, 1)
        pred = c1 * x + c0
        ss_tot = float(np.sum((y - y.mean()) ** 2))
        r2 = 1.0 - float(np.sum((y - pred) ** 2)) / ss_tot if ss_tot > 0 else float("nan")
        print(f"\n  最小二乘 稳态转速 = {c1:.4f}·目标 {c0:+.3f}  [rad/s]，R²={r2:.4f}"
              f"（理想：斜率 1、截距 0、R²=1）")
        bad = mp[mp["pp_ref"] > 0.5]
        if len(bad):
            print(f"  峰峰超设定值 50% 的档位（不是跟踪、是粘滑/极限环）: {len(bad)} 个 —— "
                  f"目标 {bad['ref'].min():.2f}~{bad['ref'].max():.2f} rad/s，"
                  f"pp_ref 中位 {bad['pp_ref'].median():.2f}，最大 {bad['pp_ref'].max():.2f}")
    else:
        print("\n  （档位少于 3 个，跳过线性拟合）")


def stiction_report(t_s, ref, meas, out, cur, args, period_s: float, eps: float, bounds=None) -> None:
    """静摩擦突破电流（--stiction）：低速粘滑时，"卡住"到"动起来"那一拍之前给了多大电流。

    现象：低速档转速长时间死在 0，输出一路爬升，直到某个阈值才脱困、然后猛冲一下。
    那个阈值就是**静摩擦突破电流**，是纯机械量、与 PID 无关 —— 它决定了低速档到底
    能不能跟踪：突破值远大于动摩擦维持值时，只有积分器能爬到突破值（时间 ≈ F_s/(ki·|e|)），
    于是低速档卡、高速档不卡，看起来像"速度不线性"，其实是积分器在储能。

    只统计**档位中途**的卡住（卡住期间 ref 恒定）：档位切换/使能瞬间 ref 从 0 跳起来，
    那一拍本来就在 0 转速上，不是静摩擦脱困，混进来会把统计拉偏。
    """
    band = args.stick_band
    stuck = np.abs(meas) < band
    brk = np.flatnonzero(stuck[:-1] & ~stuck[1:])  # 卡 → 动
    rows = []
    for i in brk:
        if abs(ref[i]) < eps:  # 关断段/无指令，跳过
            continue
        j = i
        while j > 0 and stuck[j]:
            j -= 1
        run = slice(j + 1, i + 1)
        if (t_s[i] - t_s[j + 1]) < args.stick_min_ms / 1000.0:
            continue  # 太短，多半是反馈量化毛刺
        if np.ptp(ref[run]) > args.level_tol:
            continue  # 卡住期间目标在变 → 是档位切换的起始段，不是静摩擦脱困
        rows.append(
            {
                "t": t_s[i],
                "ref": float(np.median(ref[run])),
                "stuck_ms": 1000.0 * (t_s[i] - t_s[j + 1]),
                "out_break": abs(float(out[i])),  # 脱困前一拍的输出 = 突破电流下限
                "cur_break": abs(float(cur[i])),  # 同拍实测电流（核对不是指令假的）
                "meas_after": float(meas[i + 1]),
            }
        )
    print(f"\n== 静摩擦突破（--stiction；卡住判据 |实测| < {band:.2f} rad/s）==")
    if not rows:
        print("  （没有档位中途的卡住→脱困事件）")
        return
    ev = pd.DataFrame(rows).sort_values("out_break")
    with pd.option_context("display.max_columns", None, "display.width", 200,
                           "display.float_format", "{:.3f}".format):
        print(ev.to_string(index=False))
    v = ev["out_break"].to_numpy()
    print(f"\n  突破电流: n={len(v)}  最小 {v.min():.3f}  中位 {np.median(v):.3f}  "
          f"P90 {np.percentile(v, 90):.3f}  最大 {v.max():.3f} A")
    print(f"  卡住时长: 中位 {ev['stuck_ms'].median():.0f} ms，最长 {ev['stuck_ms'].max():.0f} ms")
    print("  判读：突破电流应明显大于动摩擦维持电流（稳态 CH6≈0 时的 CH4），差值就是"
          "「静摩擦突前馈」该补的量；若突破电流随目标速度单调上涨，说明这些「卡住」"
          "其实只是档位起始段，用 --level-tol 收紧或提高 --stick-min-ms 再试。")


def _hann_spectrum(x: np.ndarray, dt: float) -> tuple[np.ndarray, np.ndarray, float]:
    """去均值 + Hann 窗的单边频谱。返回 (f, X, winsum)；X 未做幅度归一。"""
    x = np.asarray(x, dtype=float)
    x = x - x.mean()
    n = x.size
    w = np.hanning(n)
    return np.fft.rfftfreq(n, d=dt), np.fft.rfft(x * w), float(w.sum())


def _dominant(f: np.ndarray, X: np.ndarray, winsum: float, fmin: float, fmax: float):
    """在 [fmin, fmax] 内找主频。返回 (f_peak, amp_peak, j)；全窗无样本则 None。"""
    amp = 2.0 * np.abs(X) / winsum  # 单边幅度，正弦输入 → 峰值
    sel = (f >= fmin) & (f <= fmax)
    if not sel.any():
        return None
    idx = np.flatnonzero(sel)
    j = int(idx[np.argmax(amp[sel])])
    return f[j], amp[j], j


def spectrum_report(t_s, err, meas, out, cur, p, i, runs, dt: float, args) -> None:
    """段内频谱 + out→meas 的传递比/相位/互相关滞后。

    目的：区分「闭环自激（meas 由 out 通过对象驱动）」与「外部激励（meas 摆动但 out
    几乎不响应）」。判据看两处——
      1. 主频处 |meas|/|out| 传递比：比对象直流增益高一个量级 ⇒ out 的小交流就能推出
         meas 的大摆动（小惯量），振荡是闭环的；比值很小 ⇒ out 推不动 meas，另有激励源。
      2. 互相关峰值滞后：负滞后（meas 领先 out）说明是"先动速度、再动电流"的外部扰动。
    """
    fmin, fmax = args.spec_band
    print(f"\n== 频谱 / 相位（段内去均值，Hann 窗；主频搜索 {fmin:g}~{fmax:g} Hz）==")
    print("  段   t0~t1(s)      meas主频/幅   out主频/幅    |meas|/|out|  Δφ(meas-out)  lag(out→meas)")
    any_seg = False
    for k, (a, b) in enumerate(runs):
        if b - a < int(1.0 / dt):
            continue
        sl = slice(a, b)
        fm = _hann_spectrum(meas[sl], dt)
        fo = _hann_spectrum(out[sl], dt)
        if fm is None or fo is None:
            continue
        dm = _dominant(*fm, fmin, fmax)
        do = _dominant(*fo, fmin, fmax)
        if dm is None or do is None:
            continue
        fmp, amp_m, jm = dm
        fop, amp_o, jo = do
        ratio = amp_m / amp_o if amp_o > 1e-9 else float("nan")
        # 同一主频处的相位差与传递比（用 meas 主频，避免 out 主频落到别处）
        Xm = fm[1][jm]
        Xo = fo[1][jm]
        dphi = np.degrees(np.angle(Xm) - np.angle(Xo)) if abs(Xo) > 1e-12 else float("nan")
        ratio_j = (2.0 * abs(Xm) / fm[2]) / (2.0 * abs(Xo) / fo[2]) if abs(Xo) > 1e-12 else float("nan")
        # 互相关峰值滞后（正 = meas 滞后 out）
        xo = out[sl] - out[sl].mean()
        xm = meas[sl] - meas[sl].mean()
        denom = xo.std() * xm.std() * xo.size
        if denom > 1e-12:
            cc = np.correlate(xm, xo, mode="full") / denom
            lags = np.arange(-(xo.size - 1), xo.size)
            lag_ms = float(lags[int(np.argmax(cc))]) * dt * 1e3
        else:
            lag_ms = float("nan")
        print(f"  #{k:<3} {t_s[a]:6.2f}~{t_s[b - 1]:6.2f}  {fmp:6.2f}Hz/{amp_m:7.2f}  "
              f"{fop:6.2f}Hz/{amp_o:6.3f}  {ratio:11.1f}  {dphi:9.1f}°  {lag_ms:8.1f}ms")
        any_seg = True
    if not any_seg:
        print("  （没有足够长、满足搜索频带的平台段）")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", type=Path, help="VOFA+ 导出的 CSV")
    ap.add_argument("--eps-ratio", type=float, default=0.02,
                    help="判定「活跃」的门限 = 该比例 × max|设定值|（默认 0.02）")
    ap.add_argument("--eps", type=float, default=None, help="直接给绝对门限（覆盖 --eps-ratio）")
    ap.add_argument("--time-unit", choices=["auto", "us", "ms", "s"], default="auto")
    ap.add_argument("--top", type=int, default=20, help="最多列出多少个掉落事件")
    ap.add_argument("--dump-events", type=Path, default=None, help="把事件表写成 CSV")
    ap.add_argument("--raw", nargs=2, type=float, metavar=("T0", "T1"),
                    help="把该时间窗（秒）的原始行原样打印出来，用于逐样本核对")
    ap.add_argument("--intervals", action="store_true", help="打印掉落事件之间的间隔分布")
    ap.add_argument("--tune", action="store_true", help="阶跃响应/稳态负载线/闭环 τ 分析（摩擦轮速度环整定用）")
    ap.add_argument("--level-tol", type=float, default=3.0, help="电平分段容差 (rad/s)，默认 3")
    ap.add_argument("--min-level-ms", type=float, default=150.0, help="电平段最短时长 (ms)，默认 150")
    ap.add_argument("--ss-frac", type=float, default=0.25, help="稳态窗口 = 段长的该比例（默认 0.25）")
    ap.add_argument("--ss-max-ms", type=float, default=800.0, help="稳态窗口上限 (ms)，默认 800")
    ap.add_argument("--chunk-s", type=float, default=4.0, help="长段剖面块长 (s)，默认 4")
    ap.add_argument("--gap-factor", type=float, default=10.0,
                    help="时间戳相邻间隔 > 该倍数 × 中位周期 判为「中断」（默认 10；设 0 关闭）")
    ap.add_argument("--disturb-ratio", type=float, default=0.005,
                    help="扰动门限 = 该比例 × 稳态转速（默认 0.005）")
    ap.add_argument("--disturb-min", type=float, default=5.0, help="扰动门限绝对下限 (rad/s)，默认 5")
    ap.add_argument("--disturb-gap-ms", type=float, default=30.0, help="合并相邻扰动片段的间隔 (ms)，默认 30")
    ap.add_argument("--no-disturb", action="store_true", help="关闭稳态扰动事件检测")
    ap.add_argument("--spectrum", action="store_true", help="段内频谱/相位/out→meas 传递比（判断自激还是外部激励）")
    ap.add_argument("--map", action="store_true", help="旋钮(CH11) → 目标 → 稳态转速 的映射与线性度（--knob）")
    ap.add_argument("--knob", default="I11", help="旋钮所在的列名（默认 I11 = CH11 = ch9 原始值）")
    ap.add_argument("--knob-eps", type=float, default=0.02, help="旋钮视为「给了正指令」的门限（默认 0.02）")
    ap.add_argument("--map-tol", type=float, default=0.01, help="旋钮档位切分容差（默认 0.01，旋钮量纲 -1~1）")
    ap.add_argument("--ff", default="I10", help="速度环前馈所在的列名（默认 I10 = CH10）")
    ap.add_argument("--stiction", action="store_true", help="静摩擦突破电流：\"卡→动\"跳变前一拍的输出")
    ap.add_argument("--stick-band", type=float, default=0.4,
                    help="判定「卡住」的转速带宽 (rad/s)，应与固件 TRIGGER_FRICTION_FF_STATIC_BAND 取齐（默认 0.4）")
    ap.add_argument("--stick-min-ms", type=float, default=20.0,
                    help="「卡住」的最短时长 (ms)，短于此判为反馈量化毛刺并丢弃（默认 20）")
    ap.add_argument("--spec-band", nargs=2, type=float, default=(0.5, 200.0), metavar=("FMIN", "FMAX"),
                    help="主频搜索频带 (Hz)，默认 0.5 200")
    for key, col in DEFAULT_COLS.items():
        ap.add_argument(f"--{key}", default=col, help=f"{key} 用的列名（默认 {col}）")
    args = ap.parse_args()

    df = load_csv(args.csv)
    need = {k: getattr(args, k) for k in DEFAULT_COLS}
    missing = [c for c in need.values() if c not in df.columns]
    if missing:
        sys.exit(f"[错误] CSV 里没有这些列: {missing}；可用列: {list(df.columns)[:8]}...")

    # 自检：VOFA+ 导出的首行常是上一段会话的残留缓冲（时间戳跳到另一个量级），
    # 它会把整条时间轴拉偏，还会把"尚未开始测试"的空转段并成一个假的超长事件。
    # 检出即丢弃并明说；其余非递增情况直接报错，避免把乱序文件分析成假象。
    ts_raw = df[need["time"]].to_numpy(dtype=np.float64)
    drops = np.flatnonzero(np.diff(ts_raw) <= 0)
    if drops.size and drops[0] == 0 and np.all(np.diff(ts_raw[1:]) > 0):
        print(f"[自检] 首行时间戳 {ts_raw[0]:.0f} 与其余不在同一量级（疑似 VOFA 残留缓冲），已丢弃该行")
        df = df.iloc[1:].reset_index(drop=True)
        ts_raw = ts_raw[1:]
    elif drops.size:
        sys.exit(f"[错误] 时间戳非递增，出现在行 {drops[:5].tolist()}，请先核对导出文件")

    # 段间中断：时间戳大跳（MCU 复位后重新计时、或两次导出拼接）会把两条独立记录并成一条假轴，
    # 跨断点的"平台段/电平段"时长和稳态窗都会被算错；故检出后作为硬边界下传，强制不跨断点。
    gap_rows = np.array([], dtype=int)
    if args.gap_factor > 0:
        d_raw = np.diff(ts_raw)
        med_raw = float(np.median(d_raw)) if d_raw.size else 0.0
        gap_rows = np.flatnonzero(d_raw > args.gap_factor * med_raw) + 1
        if gap_rows.size:
            print(f"[自检] 检测到 {gap_rows.size} 处时间戳中断（> {args.gap_factor * med_raw:.0f} 原始单位），"
                  f"已按段强制切分，跨断点不连段：")
            scale = 1e-6 if med_raw >= 1000 else (1e-3 if med_raw >= 1 else 1.0)
            for r in gap_rows:
                print(f"        行 {r}: {ts_raw[r - 1]:.0f} → {ts_raw[r]:.0f}"
                      f"（间隔 {d_raw[r - 1]:.0f}，约 {d_raw[r - 1] * scale:.2f} s）")

    t_s, unit = to_seconds(ts_raw, args.time_unit)
    sp = df[need["setpoint"]].to_numpy(dtype=np.float64)
    meas = df[need["measure"]].to_numpy(dtype=np.float64)
    out = df[need["output"]].to_numpy(dtype=np.float64)
    cur = df[need["current"]].to_numpy(dtype=np.float64)
    err = df[need["error"]].to_numpy(dtype=np.float64)
    pch = df[need["p"]].to_numpy(dtype=np.float64)
    ich = df[need["i"]].to_numpy(dtype=np.float64)
    ffch = df[args.ff].to_numpy(dtype=np.float64) if args.ff in df.columns else None
    dtms = df[need["dt"]].to_numpy(dtype=np.float64)
    sp_max = float(np.max(np.abs(sp)))
    eps = args.eps if args.eps is not None else max(args.eps_ratio * sp_max, 1e-9)
    period_ms = float(np.median(np.diff(t_s))) * 1e3

    print(f"文件      : {args.csv}")
    print(f"样本      : {len(df)} 行，时长 {t_s[-1]:.3f} s，中位周期 {period_ms:.3f} ms"
          f"（时间单位按 {unit} 解释）")
    print(f"设定值    : max|ref| = {sp_max:.2f}，活跃门限 {eps:.3f}"
          f"（>=门限的样本 {int(np.sum(np.abs(sp) > eps))} 个）")
    print(f"实际/输出  : max|实际| = {np.max(np.abs(meas)):.2f}，max|电流指令| = {np.max(np.abs(out)):.2f} A")

    # 1. 设定值台阶构成了几段「通」的平台
    runs = runs_of(np.abs(sp) > eps, gap_rows)
    print(f"\n== 设定值「非零平台」: {len(runs)} 段 ==")
    for k, (a, b) in enumerate(runs[:40]):
        seg = sp[a:b]
        print(f"  #{k:<3} t={t_s[a]:8.3f}~{t_s[b - 1]:8.3f}s  时长 {t_s[b - 1] - t_s[a]:7.3f}s "
              f"{b - a:6d} 样本  平台值 中位 {np.median(seg):9.2f} / 均值 {seg.mean():9.2f} "
              f"/ 峰 {np.max(np.abs(seg)):9.2f}")
    if len(runs) > 40:
        print(f"  ...（还有 {len(runs) - 40} 段）")

    # 5. 自激振荡（极限环）：运行区间内 meas 去均值后的周期性摆动。
    #    判据是"幅值远大于纹波、且过零规整"；单纯的大幅摆动也可能是真实扰动，
    #    故同时报周期/过零数供人判断。只在相对目标 >5% 时才列，滤掉正常纹波。
    print("\n== 自激振荡检测（运行区间内 meas 去均值）==")
    found_osc = False
    for k, (a, b) in enumerate(runs):
        if b - a < max(int(0.2 / (period_ms / 1000.0)), 10):
            continue
        ref_med = float(np.median(sp[a:b]))
        x = meas[a:b] - ref_med
        pp = float(x.max() - x.min())
        if pp < 0.05 * abs(ref_med):
            continue
        zc = np.flatnonzero((x[:-1] < 0) & (x[1:] >= 0))
        if zc.size < 3:
            continue
        tt = t_s[a:b]
        per_ms = float(np.median(np.diff(tt[zc]))) * 1e3
        dur = t_s[b - 1] - t_s[a]
        # 后半段峰峰：区分「阶跃暂态」与「持续极限环」——后者在段后半仍不衰减。
        half = a + (b - a) // 2
        xh = meas[half:b] - ref_med
        pp_late = float(xh.max() - xh.min())
        # 衰减包络：把段等分成最多 8 个窗，逐窗报峰峰；单调衰减=欠阻尼暂态，持平=极限环。
        nwin = min(8, max(2, int(dur / 2.0)))
        edges = np.linspace(a, b, nwin + 1).astype(int)
        env = [float(meas[p:q].max() - meas[p:q].min()) for p, q in zip(edges[:-1], edges[1:]) if q > p]
        print(f"  #{k:<3} {t_s[a]:8.3f}~{t_s[b - 1]:8.3f}s  {dur:5.2f}s  ref {ref_med:7.2f}  "
              f"均值 {x.mean() + ref_med:7.2f}  峰峰 {pp:7.2f}（后半 {pp_late:7.2f}）  "
              f"周期 {per_ms:6.1f}ms  {1000.0 / per_ms:6.2f}Hz  过零 {zc.size}")
        print(f"       峰峰包络({nwin} 窗): " + " ".join(f"{e:5.1f}" for e in env))
        found_osc = True
    if not found_osc:
        print("  （运行区间内 meas 相对目标的摆动 <5%，未检出自激振荡）")

    if args.spectrum:
        spectrum_report(t_s, err, meas, out, cur, pch, ich, runs, period_ms / 1000.0, args)

    if args.tune:
        tune_report(t_s, sp, meas, cur, out, args, period_ms / 1000.0, eps, gap_rows, pch, ich, ffch)

    if args.map:
        if args.knob not in df.columns:
            sys.exit(f"[错误] --map 需要旋钮列 {args.knob}，CSV 里没有；可用列: {list(df.columns)[:9]}...")
        map_report(t_s, sp, meas, df[args.knob].to_numpy(dtype=np.float64), args,
                   period_ms / 1000.0, eps, gap_rows)

    if args.stiction:
        stiction_report(t_s, sp, meas, out, cur, args, period_ms / 1000.0, eps, gap_rows)

    # 2. 平台内部掉到 0 的事件
    events = find_dropouts(sp, eps, gap_rows)
    print(f"\n== 运行中「设定值掉到 0」事件: {len(events)} 个 ==")
    rows = []
    for a, b in events:
        before = sp[a - 1]
        after = sp[b]
        dt_seg = dtms[a:b]
        row = {
            "t_start_s": t_s[a],
            "t_end_s": t_s[b - 1],
            "dur_ms": (t_s[b - 1] - t_s[a]) * 1e3,
            "n_samples": b - a,
            "ref_before": before,
            "ref_after": after,
            "ref_min": float(np.min(sp[a:b])),
            "ref_max": float(np.max(sp[a:b])),
            "meas_before": meas[a - 1],
            "meas_min": float(np.min(np.abs(meas[a:b]))),
            "meas_after": meas[b],
            "out_min": float(np.min(out[a:b])),
            "out_max": float(np.max(out[a:b])),
            "current_abs_max": float(np.max(np.abs(cur[a:b]))),
            "dt_min_ms": float(np.min(dt_seg)),
            "dt_max_ms": float(np.max(dt_seg)),
        }
        rows.append(row)
    if rows:
        ev = pd.DataFrame(rows)
        ev["dur_ms"] = ev["dur_ms"].round(3)
        for c in ("ref_before", "ref_after", "ref_min", "ref_max", "meas_before", "meas_min", "meas_after", "out_min", "out_max",
                  "current_abs_max"):
            ev[c] = ev[c].round(2)
        ev["dt_min_ms"] = ev["dt_min_ms"].round(3)
        ev["dt_max_ms"] = ev["dt_max_ms"].round(3)
        show = ev.sort_values("dur_ms", ascending=False).head(args.top)
        with pd.option_context("display.max_columns", None, "display.width", 250):
            print(show.to_string(index=False))

        print("\n  时长分布（按样本数）:")
        vc = ev["n_samples"].value_counts().sort_index()
        for k, v in vc.items():
            print(f"    {int(k):5d} 样本 (= {k * period_ms:8.3f} ms): {v} 个")
        print(f"  时长统计: 中位 {ev['dur_ms'].median():.3f} ms，"
              f"最大 {ev['dur_ms'].max():.3f} ms，合计 {ev['dur_ms'].sum():.3f} ms")
        # 判定：掉落期间实际转速有没有真掉下来
        fell = ev[ev["meas_min"] < 0.5 * ev["meas_before"].abs().clip(lower=1e-6)]
        print(f"  期间实际转速也明显下跌的: {len(fell)}/{len(ev)} 个")

        # 间隔分布：机械式通断是"人手的节奏"，链路/协议故障往往带固定周期
        if args.intervals and len(rows) > 1:
            gaps = np.diff(ev["t_start_s"].to_numpy())
            print(f"\n  事件间隔: 中位 {np.median(gaps):.3f}s，"
                  f"最小 {gaps.min():.3f}s，最大 {gaps.max():.3f}s")
            hist, edges = np.histogram(gaps, bins=min(20, max(4, len(gaps) // 4)))
            for h, lo, hi in zip(hist, edges[:-1], edges[1:]):
                if h:
                    print(f"    {lo:7.3f}~{hi:7.3f}s : {'#' * min(h, 60)} {h}")

        if args.dump_events:
            ev.to_csv(args.dump_events, index=False)
            print(f"\n  事件表已写出: {args.dump_events}")
    else:
        print("  （没有找到：所有 0 值都在平台之外）")

    # 3. dt 通道异常（任务被拖 / 丢帧）
    dt_anom = np.sum(np.abs(dtms - period_ms) > max(0.5 * period_ms, 0.5))
    print(f"\n== dt 通道 ==\n  偏离中位周期 >50% 的样本: {dt_anom} 个"
          f"（dt 是每拍写入，长时间不更新说明该拍没跑到闭环）")

    # 4. 原始行 dump
    if args.raw:
        t0, t1 = args.raw
        m = (t_s >= t0) & (t_s <= t1)
        idx = np.flatnonzero(m)
        print(f"\n== 原始行 t={t0}~{t1}s（{idx.size} 行）==")
        if idx.size == 0:
            print("  （该窗口内没有样本）")
        else:
            sub = df.iloc[idx].copy()
            sub.insert(0, "t_s", np.round(t_s[idx], 4))
            with pd.option_context("display.max_columns", None, "display.width", 250, "display.float_format", "{:.3f}".format):
                print(sub.to_string(index=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
