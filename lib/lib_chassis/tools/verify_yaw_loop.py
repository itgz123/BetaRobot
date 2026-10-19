#!/usr/bin/env python3
"""离线校验底盘 yaw 速率 PI 闭环（app/half_rudder_chassis 的 ChassisYawLoopStep）。

与固件同构的模型
----------------
偏航环近似成一阶滞后（舵向位置环 + 车体惯量）：

    d(rate)/dt = (w_eff + disturb - rate) / tau

控制器（dt 与固件 CHASSIS 任务周期一致）：

    rate_lpf += alpha * (rate - rate_lpf)          alpha = dt / (lpf_rc + dt)
    e        = w_cmd - rate_lpf
    I       += KI * e * dt                         条件积分抗饱和 + 限幅
    corr     = clamp(KP * e + I, ±corr_max)        再按 slew 限变化率
    w_eff    = w_cmd + corr

⚠ 本模型**不含**固件的偏航权限门控（两轮 |cos(舵角)| 门限）和舵向重定向延迟，
   它只回答"增益/限幅在标称一阶偏航对象上稳不稳、漂移压得住吗"。首份实车日志里
   的极限环来自横移段没有权限还硬修，那类问题要靠门控+实车复测，仿真复现不了。

固件里 w_cmd 与 rate 都是物理约定（w>0 = 俯视逆时针），所以 `-rate` 就是负反馈。

场景
----
0 ~ t_step1         w_cmd = 0，恒定扰动 disturb（对应“不给自旋还往一个方向漂”）
t_step1 ~ t_step2   w_cmd = step（持续转弯）
t_step2 ~ t_end     w_cmd = 0（松杆，看会不会冲过头）

用法
----
    python3 lib/lib_chassis/tools/verify_yaw_loop.py
    python3 lib/lib_chassis/tools/verify_yaw_loop.py --tau 0.02
    python3 lib/lib_chassis/tools/verify_yaw_loop.py --kp 0.5 --ki 1.5 --lpf 0.02
    python3 lib/lib_chassis/tools/verify_yaw_loop.py --disturb 0.12 --step 0.3
    python3 lib/lib_chassis/tools/verify_yaw_loop.py --sweep-tau 0.02 0.05 0.1 0.2
"""

from __future__ import annotations

import argparse
import math

# 与 app_chassis_def.h 的默认值保持一致（改固件参数时同步改这里）
DEF_KP = 0.5
DEF_KI = 2.0
DEF_LPF_RC = 0.020
DEF_DT = 0.002
DEF_TAU = 0.050
DEF_DISTURB = 0.12  # rad/s，约 6.9°/s，取自实测倒车漂移
DEF_STEP = 0.3      # rad/s，与 CHASSIS_YAW_CORR_MAX 同量级
DEF_INTEG_MAX = 0.15
DEF_CORR_MAX = 0.30
DEF_SLEW = 2.0      # rad/s²，对应 CHASSIS_YAW_CORR_SLEW
DEF_T_END = 8.0
DEF_T_STEP1 = 2.0
DEF_T_STEP2 = 4.0


def wrap_pi(angle: float) -> float:
    """wrap 到 (-pi, pi]（固件用 Lib_Math_WrapAngleNegPIToPI）。"""
    return math.remainder(angle, 2.0 * math.pi)


def simulate(
    kp: float,
    ki: float,
    tau: float,
    dt: float,
    lpf_rc: float,
    disturb: float,
    step: float,
    integ_max: float,
    corr_max: float,
    slew: float,
    t_step1: float,
    t_step2: float,
    t_end: float,
) -> dict:
    """跑一遍场景，返回轨迹与指标。"""
    rate = 0.0
    rate_lpf = 0.0
    integ = 0.0
    corr_prev = 0.0
    heading = 0.0
    heading_ref = 0.0

    n = int(round(t_end / dt))
    n1 = int(round(t_step1 / dt))
    n2 = int(round(t_step2 / dt))

    ts, cmds, rates, effs, integs, headings, errs = [], [], [], [], [], [], []
    peak_step = -1e9
    peak_after = -1e9
    settle_time = None
    drift_rate_sum = 0.0
    drift_rate_n = 0

    for k in range(n):
        t = k * dt
        if k < n1:
            cmd = 0.0
        elif k < n2:
            cmd = step
        else:
            cmd = 0.0

        err = cmd - rate_lpf
        corr_raw = kp * err + integ
        if not ((corr_raw > corr_max and err > 0.0) or (corr_raw < -corr_max and err < 0.0)):
            integ += ki * err * dt
            integ = max(-integ_max, min(integ_max, integ))
        corr = max(-corr_max, min(corr_max, kp * err + integ))
        # 变化率限制（固件 CHASSIS_YAW_CORR_SLEW）
        max_step = slew * dt
        corr = max(corr_prev - max_step, min(corr_prev + max_step, corr))
        corr_prev = corr
        w_eff = cmd + corr

        # 先积分后取样：与固件“本拍用上一拍反馈算、本拍下发”差一拍，2ms 可忽略
        rate += dt * (w_eff + disturb - rate) / tau
        if lpf_rc > 0.0:
            alpha = dt / (lpf_rc + dt)
            rate_lpf += alpha * (rate - rate_lpf)
        else:
            rate_lpf = rate

        heading += rate * dt
        heading_ref += cmd * dt
        heading = wrap_pi(heading)
        heading_ref = wrap_pi(heading_ref)

        ts.append(t)
        cmds.append(cmd)
        rates.append(rate)
        effs.append(w_eff)
        integs.append(integ)
        headings.append(heading)
        errs.append(wrap_pi(heading_ref - heading))

        if n1 <= k < n2:
            peak_step = max(peak_step, rate)
            if settle_time is None and abs(rate - step) > 0.05 * abs(step):
                settle_time = t - t_step1
            elif settle_time is not None and abs(rate - step) > 0.05 * abs(step):
                settle_time = t - t_step1  # 还在带外，持续更新
        if k >= n2:
            peak_after = max(peak_after, rate)
        if k >= n1 + int(round(1.0 / dt)) and k < n1 + int(round(2.0 / dt)):
            drift_rate_sum += abs(rate)
            drift_rate_n += 1

    # 第一段最后 1 s（扰动已收敛）的平均 |rate|
    seg = slice(n1 - int(round(1.0 / dt)), n1)
    steady_drift = sum(abs(r) for r in rates[seg]) / max(1, len(rates[seg]))
    # 阶跃段最后 1 s 的平均速率
    seg2 = slice(n2 - int(round(1.0 / dt)), n2)
    steady_step = sum(rates[seg2]) / max(1, len(rates[seg2]))

    return {
        "ts": ts,
        "cmd": cmds,
        "rate": rates,
        "eff": effs,
        "integ": integs,
        "heading": headings,
        "err": errs,
        "steady_drift": steady_drift,
        "steady_step": steady_step,
        "overshoot": (peak_step - step) / abs(step) if step else float("nan"),
        "settle_time": settle_time,
        "peak_after": peak_after,
        "integ_final": integ,
        "max_abs_integ": max(abs(v) for v in integs),
        "heading_err_final": errs[-1],
    }


def print_report(tag: str, r: dict, step: float) -> None:
    print(f"── {tag} ──")
    print(f"  扰动段稳态 |rate|（目标 0）        : {r['steady_drift']:.4f} rad/s "
          f"({math.degrees(r['steady_drift']):.2f}°/s)")
    print(f"  阶跃段稳态 rate（目标 {step:.3f}）  : {r['steady_step']:.4f} rad/s")
    print(f"  阶跃超调                          : {100.0 * r['overshoot']:+.2f}%")
    if r["settle_time"] is None:
        print("  进入 ±5% 带时间                   : 未进入")
    else:
        print(f"  进入 ±5% 带时间                   : {r['settle_time']:.3f} s")
    print(f"  松杆后峰值 rate                    : {r['peak_after']:+.4f} rad/s")
    print(f"  结束时刻积分项                     : {r['integ_final']:+.4f} rad/s")
    print(f"  全程 |积分项| 峰值                 : {r['max_abs_integ']:.4f} rad/s")
    print(f"  结束时刻航向误差                   : {math.degrees(r['heading_err_final']):+.2f}°")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--kp", type=float, default=DEF_KP)
    p.add_argument("--ki", type=float, default=DEF_KI)
    p.add_argument("--tau", type=float, default=DEF_TAU, help="偏航环一阶滞后时间常数 [s]")
    p.add_argument("--dt", type=float, default=DEF_DT, help="控制周期 [s]")
    p.add_argument("--lpf", type=float, default=DEF_LPF_RC, help="陀螺低通时间常数 [s]")
    p.add_argument("--disturb", type=float, default=DEF_DISTURB, help="恒定等效偏航扰动 [rad/s]")
    p.add_argument("--step", type=float, default=DEF_STEP, help="自旋阶跃指令 [rad/s]")
    p.add_argument("--integ-max", type=float, default=DEF_INTEG_MAX)
    p.add_argument("--corr-max", type=float, default=DEF_CORR_MAX)
    p.add_argument("--slew", type=float, default=DEF_SLEW, help="修正量变化率上限 [rad/s²]")
    p.add_argument("--t-step1", type=float, default=DEF_T_STEP1)
    p.add_argument("--t-step2", type=float, default=DEF_T_STEP2)
    p.add_argument("--t-end", type=float, default=DEF_T_END)
    p.add_argument("--sweep-tau", type=float, nargs="+", default=None,
                   help="对多个 tau 各跑一遍，只看汇总")
    args = p.parse_args()

    if args.sweep_tau:
        print(f"kp={args.kp} ki={args.ki} lpf={args.lpf:.3f}s disturb={args.disturb:+.3f} "
              f"step={args.step:+.3f}")
        print(f"{'tau[s]':>8} {'扰动段|rate|':>14} {'阶跃稳态':>12} {'超调':>10} "
              f"{'±5%时间[s]':>12} {'松杆峰值':>12} {'末航向误差':>12}")
        for tau in args.sweep_tau:
            r = simulate(args.kp, args.ki, tau, args.dt, args.lpf, args.disturb,
                         args.step, args.integ_max, args.corr_max, args.slew,
                         args.t_step1, args.t_step2, args.t_end)
            st = f"{r['settle_time']:.3f}" if r["settle_time"] is not None else "未进入"
            print(f"{tau:8.3f} {r['steady_drift']:14.4f} {r['steady_step']:12.4f} "
                  f"{100.0 * r['overshoot']:+9.2f}% {st:>12} "
                  f"{r['peak_after']:+12.4f} {math.degrees(r['heading_err_final']):+11.2f}°")
        return

    r = simulate(args.kp, args.ki, args.tau, args.dt, args.lpf, args.disturb,
                 args.step, args.integ_max, args.corr_max, args.slew,
                 args.t_step1, args.t_step2, args.t_end)
    print(f"参数：kp={args.kp} ki={args.ki} tau={args.tau:.3f}s dt={args.dt:.4f}s "
          f"lpf={args.lpf:.3f}s disturb={args.disturb:+.3f} step={args.step:+.3f}")
    print_report(f"tau={args.tau:.3f}s", r, args.step)

    # 简明的判据：扰动段 2s 内残差 < 扰动的 15%；阶跃不应持续振荡；
    # 整段结束时的航向误差应回到 10° 以内（不是看松杆瞬间的 rate——
    # 那本来就是阶跃值，rate 只能按 plant 时间常数衰减）。
    bad = []
    if r["steady_drift"] > 0.15 * abs(args.disturb):
        bad.append("扰动段残差 >15% 扰动（积分太慢或增益太小）")
    if r["overshoot"] > 0.30:
        bad.append("阶跃超调 >30%（kp 偏大或 ki/滤波相位不合适）")
    if abs(math.degrees(r["heading_err_final"])) > 10.0:
        bad.append("末航向误差 >10°（积分退饱和太慢/航向滞后）")
    if bad:
        print("⚠ " + "；".join(bad))
    else:
        print("✅ 该 tau 下默认增益表现正常")


if __name__ == "__main__":
    main()
