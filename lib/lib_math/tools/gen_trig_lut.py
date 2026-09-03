#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_trig_lut.py — lib_math 查表三角函数表生成器

为 lib_math_trig_lut.h 生成正弦查表 C 文件 lib_math_trig_lut.c。

表设计
------
- QUARTER（四分之一）：只存储 sin 在 [0, π/2] 上的采样值：M 个区间，M+1 个 float32 条目，
  table[i] = float32(sin(i·(π/2)/M))。cos 通过 sin(x+π/2) 复用同表；奇象限反向索引镜像。
- FULL（2π 完整周期）：存储 sin 在 [0, 2π) 上的采样值：N 个区间，N+1 个 float32 条目，
  table[i] = float32(sin(i·2π/N))。无象限映射，按归一化角度直接索引；cos 用 θ+π/2 移位复用。
- 同一 .c 内先按 LIB_MATH_TRIG_TABLE_KIND（QUARTER/FULL）分派外层 #if，
  再按 `#if LIB_MATH_TRIG_TABLE_SIZE == <M 或 N>` 切换精度档，只编译被选中的表。

精度分析
--------
- float32 机器精度 ε = 2^-23 ≈ 1.19e-7（约 7.2 位有效十进制）。
- 线性插值误差上界 h²/8（|sin″|≤1）。要求 < ε ⇒ h < sqrt(8ε) ≈ 9.77e-4。
- 本脚本用 numpy 复刻 C 管线，实测每档：
  * 插值+量化(纯)：精确 float64 相位位置 + float32 表值 + double 插值 ——
    只含"查表线性插值"与表值量化的贡献，是判定满精度档的标准；
  * 整管误差：float32 输入走完整管线（归一/象限/插值/符号），
    分 [0,2π) / 负区间报告（其中含索引计算的 float32 舍入，约 ε 为下限）。
- 找出满足"插值+量化 < ε"的最小档位 M（满精度档）。

用法
----
    python gen_trig_lut.py                          # 分析两套，生成 QUARTER 256/1024/2048/4096 + FULL 1024/2048/4096/8192
    python gen_trig_lut.py --sizes 256 1024 2048 -o ../lib_math_trig_lut.c
    python gen_trig_lut.py --full-sizes 4096 8192 -o ../lib_math_trig_lut.c
    python gen_trig_lut.py --target-eps 1.19e-7     # 自定义目标误差
    python gen_trig_lut.py --no-verify              # 生成后跳过自检
"""

import argparse
import sys

import numpy as np

# ---------------- 常数（float64 真值 + float32 逐位镜像） ----------------
PI_F64 = np.float64(np.pi)
TWO_PI_F64 = np.float64(2.0) * PI_F64
QUARTER_F64 = np.float64(0.5) * PI_F64

INV_2PI_F32 = np.float32(1.0 / (2.0 * np.pi))   # 0.159154943f  (1/2π)
INV_QUARTER_F32 = np.float32(2.0 / np.pi)       # 0.636619773f  (2/π)
TWO_PI_F32 = np.float32(2.0 * np.pi)
QUARTER_F32 = np.float32(0.5 * np.pi)


def make_table(M: int) -> np.ndarray:
    """QUARTER 表：table[i] = float32(sin(i·(π/2)/M))，i ∈ [0, M]。"""
    idx = np.arange(M + 1, dtype=np.float64)
    vals = np.sin(idx * (QUARTER_F64 / np.float64(M)))
    return vals.astype(np.float32)


def make_full_table(N: int) -> np.ndarray:
    """FULL 表：table[i] = float32(sin(i·2π/N))，i ∈ [0, N]，末尾=sin(2π)=0=第 0 项。"""
    idx = np.arange(N + 1, dtype=np.float64)
    vals = np.sin(idx * (TWO_PI_F64 / np.float64(N)))
    return vals.astype(np.float32)


def lerp(table: np.ndarray, p: np.ndarray) -> np.ndarray:
    """float32 线性插值，与 C 的 Lib_Math_TrigTableLerp 逐位一致。

    p: float32 数组，取值 [0, M]。p == M 时钳制到 [M-1, 1.0]。
    """
    M = table.size - 1
    i = p.astype(np.uint32)
    frac = p - i.astype(np.float32)
    over = i >= np.uint32(M)
    i = np.where(over, np.uint32(M - 1), i)
    frac = np.where(over, np.float32(1.0), frac)
    v0 = table[i]
    v1 = table[i + 1]
    return v0 + frac * (v1 - v0)


def _err_stats(err: np.ndarray):
    """返回 (max 绝对误差, rms 绝对误差)。"""
    e = err.astype(np.float64)
    return float(np.max(e)), float(np.sqrt(np.mean(e * e)))


def measure_interp_error(M: int, table: np.ndarray):
    """纯插值 + 表量化误差（用户判定"满精度档"的标准）。

    用精确 float64 相位位置 u = φ·(2M/π)（覆盖全部表索引点与每个区间内 32 个偏移），
    取 float32 表值，在 double 下做线性插值，对照 float64 sin(φ)。
    该误差只含"查表线性插值"本身与表值量化的贡献，与索引计算的 float32 舍入无关。
    """
    offsets = np.linspace(0.0, 1.0, 33)[:-1]
    grid = (np.arange(M, dtype=np.float64)[:, None] + offsets[None, :]).ravel()
    grid = np.append(grid, np.float64(M))          # 右端点 φ=π/2
    u = grid                                        # 位置 ∈ [0, M]
    i0 = np.floor(u).astype(np.int64)
    frac = u - i0
    over = i0 >= np.int64(M)                        # u==M 时钳制：i=M-1、frac=1（与 C 一致）
    i = np.where(over, np.int64(M - 1), i0)
    frac = np.where(over, 1.0, frac)
    v0 = table[i].astype(np.float64)
    v1 = table[np.minimum(i + 1, np.int64(M))].astype(np.float64)
    got = v0 + frac * (v1 - v0)
    phi = u / np.float64(M) * QUARTER_F64
    ref = np.sin(phi)
    return _err_stats(np.abs(got - ref))


def measure_full_interp_error(N: int, table: np.ndarray):
    """FULL 表纯插值 + 表量化误差（判定满精度档的标准，与 QUARTER 版对称）。

    用精确 float64 位置 u ∈ [0, N]（覆盖全部索引点与每区间 32 个偏移），
    float32 表值 + double 插值，对照 float64 sin(u/N·2π)。
    """
    offsets = np.linspace(0.0, 1.0, 33)[:-1]
    grid = (np.arange(N, dtype=np.float64)[:, None] + offsets[None, :]).ravel()
    grid = np.append(grid, np.float64(N))
    u = grid
    i0 = np.floor(u).astype(np.int64)
    frac = u - i0
    over = i0 >= np.int64(N)                        # u==N 时钳制：i=N-1、frac=1（与 C 一致）
    i = np.where(over, np.int64(N - 1), i0)
    frac = np.where(over, 1.0, frac)
    v0 = table[i].astype(np.float64)
    v1 = table[np.minimum(i + 1, np.int64(N))].astype(np.float64)
    got = v0 + frac * (v1 - v0)
    phi = u / np.float64(N) * TWO_PI_F64
    ref = np.sin(phi)
    return _err_stats(np.abs(got - ref))


def measure_pipeline(theta64: np.ndarray, M: int, table: np.ndarray):
    """ModeB：整管误差。float32 θ 走完整管线，对照 sin((double)θ_f32)。"""
    scale = np.float32(M) * INV_QUARTER_F32
    m_f = np.float32(M)
    theta = theta64.astype(np.float32)
    # 1) 归一化到 [0, 2π)（int 截断 + 负数补偿，与 C 一致）
    t = theta - TWO_PI_F32 * np.trunc(theta * INV_2PI_F32).astype(np.int32).astype(np.float32)
    t = np.where(t < np.float32(0.0), t + TWO_PI_F32, t)
    # 2) 象限与相位
    qf = t * INV_QUARTER_F32
    q = qf.astype(np.uint32)
    q = np.where(q > np.uint32(3), np.uint32(3), q)
    phi = t - q.astype(np.float32) * QUARTER_F32
    u = phi * scale
    u = np.minimum(u, m_f)
    # 3) 插值 + 符号
    ps = np.where((q & np.uint32(1)) != np.uint32(0), m_f - u, u)
    sv = lerp(table, ps)
    sv = np.where((q & np.uint32(2)) != np.uint32(0), -sv, sv)
    ref = np.sin(theta.astype(np.float64)).astype(np.float32)
    return _err_stats(np.abs(sv - ref))


def fmt_f32(v: np.float32) -> str:
    """float32 → 精确往返的十进制字面量，带 f 后缀，贴合模块 0.0f/1.0f 风格。"""
    f = float(np.float32(v))
    s = format(f, ".9g")
    if "." not in s and "e" not in s and "E" not in s and "n" not in s:
        s += ".0"
    return s + "f"


def _emit_size_branches(lines, var_name, sizes, table_fn):
    """输出内层按 LIB_MATH_TRIG_TABLE_SIZE 切档的一段表（不含外层 KIND 分支）。"""
    for i, S in enumerate(sizes):
        kw = "#if" if i == 0 else "#elif"
        lines.append("%s LIB_MATH_TRIG_TABLE_SIZE == %d" % (kw, S))
        lines.append("const float %s[%d] = {" % (var_name, S + 1))
        vals = [fmt_f32(v) for v in table_fn(S)]
        for j in range(0, len(vals), 12):
            lines.append("    " + ", ".join(vals[j:j + 12]) + ",")
        lines.append("};")
        lines.append("")
    lines.append("#else")
    lines.append(
        '#error "LIB_MATH_TRIG_TABLE_SIZE 必须为 %s 之一"'
        % (" / ".join(str(s) for s in sizes))
    )
    lines.append("#endif /* LIB_MATH_TRIG_TABLE_SIZE */")
    lines.append("")


def gen_c_file(quarter_sizes, full_sizes, out_path) -> None:
    lines = [
        "/**",
        " * @file lib_math_trig_lut.c",
        " * @brief 自研查表三角函数表（四分之一周期 + 2π 完整周期，宏切换）",
        " *",
        " * @note 本文件由 tools/gen_trig_lut.py 自动生成，请勿手动修改",
        " * @note 仅当定义了 LIB_MATH_TRIG_LUT_USED 时编译表（固件来自 app_cfg.h、",
        " *       PC 检验来自命令行 -D）；否则本文件为空 TU",
        " * @note 外层按 LIB_MATH_TRIG_TABLE_KIND（0=QUARTER/1=FULL）选择表结构：",
        " *       - QUARTER：Lib_Math_SinTable，[0,π/2] 四分之一表，SIZE=区间数 M；",
        " *       - FULL：Lib_Math_FullSinTable，[0,2π) 完整周期表，SIZE=区间数 N（=4M 同精度）。",
        " *       只编译被选中的表；表存放于 flash（.rodata）",
        " */",
        "",
        "/* 配置入口由 lib_math_trig_lut.h 统一处理：固件 include app_cfg.h 取 SPEED/PREC；",
        " * PC 独立检验（tools/test_trig_lut.sh）定义 LIB_MATH_TRIG_LUT_STANDALONE 跳过。 */",
        '#include "lib_math_trig_lut.h"',
        "",
    ]
    lines.append("#ifdef LIB_MATH_TRIG_LUT_USED")
    lines.append("")
    lines.append("#if LIB_MATH_TRIG_TABLE_KIND == 0   /* QUARTER：四分之一周期表 */")
    _emit_size_branches(lines, "Lib_Math_SinTable", quarter_sizes, make_table)
    lines.append("#elif LIB_MATH_TRIG_TABLE_KIND == 1   /* FULL：2π 完整周期表 */")
    _emit_size_branches(lines, "Lib_Math_FullSinTable", full_sizes, make_full_table)
    lines.append("#else")
    lines.append('#error "LIB_MATH_TRIG_TABLE_KIND 必须为 0（QUARTER）或 1（FULL）"')
    lines.append("#endif /* LIB_MATH_TRIG_TABLE_KIND */")
    lines.append("")
    lines.append("#endif /* LIB_MATH_TRIG_LUT_USED */")
    lines.append("")
    with open(out_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lines))
    print("[gen] 已生成 %s：QUARTER %s / FULL %s"
          % (out_path, " / ".join(map(str, quarter_sizes)), " / ".join(map(str, full_sizes))))


def analyze_quarter(eps: float):
    """QUARTER 候选档分析：返回 (满精度档 M, 各档插值+量化误差 dict)。"""
    candidates = [128, 256, 512, 1024, 2048, 4096, 8192]
    header = "  M    | 理论插值 | 插值+量化(纯) | 整管[0,2π) | 整管负区间 | 满精度档"
    print("【QUARTER 四分之一周期表】")
    print(header)
    print("-" * len(header))

    full_prec = None
    a_map = {}
    for M in candidates:
        table = make_table(M)
        h = float(QUARTER_F64) / M
        theory = h * h / 8.0
        a_max, _ = measure_interp_error(M, table)
        a_map[M] = a_max
        num = max(4 * M * 8, 1 << 16)
        theta_full = np.linspace(0.0, TWO_PI_F64, num, endpoint=False)
        special = np.array([0.0, QUARTER_F64, np.pi, 3.0 * QUARTER_F64,
                            2.0 * np.pi - 1e-3, np.pi / 2.0 - 1e-4,
                            np.pi / 2.0 + 1e-4, -1e-4, 1e-4], dtype=np.float64)
        b_full, _ = measure_pipeline(np.concatenate([theta_full, special]), M, table)
        theta_neg = np.linspace(-4.0 * TWO_PI_F64, 0.0, num, endpoint=False)
        b_neg, _ = measure_pipeline(theta_neg, M, table)

        star = ""
        if a_max < eps and full_prec is None:
            full_prec = M
            star = "  ★ 满精度"
        elif a_max < eps:
            star = "  (满足)"
        print("  %-4d | %9.2e | %13.2e | %11.2e | %12.2e |%s"
              % (M, theory, a_max, b_full, b_neg, star))
    return full_prec, a_map


def analyze_full(eps: float):
    """FULL 候选档分析：返回 (满精度档 N, 各档插值+量化误差 dict)。

    整管误差与 QUARTER 同源（float32 索引舍入），此处只测插值+量化；
    整管全点由 PC 端 C 检验程序覆盖。
    """
    candidates = [1024, 2048, 4096, 8192, 16384]
    header = "  N    | 理论插值 | 插值+量化(纯) | 满精度档"
    print("【FULL 2π 完整周期表】")
    print(header)
    print("-" * len(header))

    full_prec = None
    a_map = {}
    for N in candidates:
        table = make_full_table(N)
        h = float(TWO_PI_F64) / N
        theory = h * h / 8.0
        a_max, _ = measure_full_interp_error(N, table)
        a_map[N] = a_max
        star = ""
        if a_max < eps and full_prec is None:
            full_prec = N
            star = "  ★ 满精度"
        elif a_max < eps:
            star = "  (满足)"
        print("  %-4d | %9.2e | %13.2e |%s" % (N, theory, a_max, star))
    return full_prec, a_map


def main() -> int:
    ap = argparse.ArgumentParser(description="lib_math 查表三角函数表生成器")
    ap.add_argument("--sizes", type=int, nargs="*", default=[256, 512, 1024, 2048, 4096],
                    help="QUARTER 表写入 .c 的精度档（区间数 M），默认 256 512 1024 2048 4096")
    ap.add_argument("--full-sizes", type=int, nargs="*", default=[1024, 2048, 4096, 8192],
                    help="FULL 表写入 .c 的精度档（区间数 N），默认 1024 2048 4096 8192")
    ap.add_argument("--target-eps", type=float, default=float(2.0 ** -23),
                    help="满精度目标误差，默认 2^-23 ≈ 1.19e-7")
    ap.add_argument("-o", "--out", default="../lib_math_trig_lut.c",
                    help="输出 C 文件路径（相对脚本目录）")
    ap.add_argument("--no-verify", action="store_true", help="生成后跳过自检")
    args = ap.parse_args()

    eps = float(args.target_eps)
    print("=== lib_math 查表三角函数精度分析 ===")
    print("目标：线性插值误差 < float32 机器精度 ε = 2^-23 ≈ %.3e" % eps)
    print("")

    q_full, q_map = analyze_quarter(eps)
    print("")
    f_full, f_map = analyze_full(eps)
    print("")

    if q_full is None or f_full is None:
        print("\n[FAIL] 存在候选档位内均未达到目标误差 %.3e 的表类型，请增大对应档位。" % eps)
        return 1
    print("结论：QUARTER 满精度档 M = %d（表 %d 项 ≈ %.1f KB flash）"
          % (q_full, q_full + 1, (q_full + 1) * 4 / 1024.0))
    print("      FULL    满精度档 N = %d（表 %d 项 ≈ %.1f KB flash）"
          % (f_full, f_full + 1, (f_full + 1) * 4 / 1024.0))
    for name, fp, sizes in (("QUARTER", q_full, args.sizes), ("FULL", f_full, args.full_sizes)):
        if fp not in sizes:
            print("警告：%s 满精度档 %d 不在生成列表 %s 中，建议补入。"
                  % (name, fp, sizes))

    gen_c_file(args.sizes, args.full_sizes, args.out)

    # 生成后自检：两套生成档中各自必须包含满精度档（低档位是速度优先，不要求达 ε）
    if not args.no_verify:
        print("\n=== 生成后自检 ===")
        ok = True
        checks = [("QUARTER", args.sizes, q_full, measure_interp_error, make_table),
                  ("FULL", args.full_sizes, f_full, measure_full_interp_error, make_full_table)]
        for name, sizes, fp, meas, make in checks:
            for S in sizes:
                table = make(S)
                a_max, _ = meas(S, table)
                if a_max < eps or S == fp:
                    status = "满精度(PASS)"
                else:
                    status = "低精度档(速度优先)"
                print("  [%s] %-5s 插值+量化 max=%.3e  [%s]" % (name, S, a_max, status))
            if fp not in sizes:
                print("[WARN] %s 生成档位 %s 不包含满精度档 %d（若刻意只用低档位可忽略）"
                      % (name, sizes, fp))
                ok = False
        if not ok:
            return 1
        print("[OK] 两套生成档位均包含各自满精度档：QUARTER M=%d / FULL N=%d"
              % (q_full, f_full))

    return 0


if __name__ == "__main__":
    sys.exit(main())
