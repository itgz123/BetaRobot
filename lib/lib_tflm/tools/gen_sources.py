#!/usr/bin/env python3
"""从 TFLM 上游 sources.inc 生成 lib_tflm/CMakeLists.txt 需要的源文件清单。

为什么需要它，而不是在 CMake 里 GLOB：
    TFLM 的库源集合不是"某个目录下的全部 .cc"，而是 sources.inc 里逐条挑选的
    清单（再加 MICROLITE_TEST_SRCS 过滤），并且有两级"同名替换"规则：

      1. kernels/*.cc  ——> kernels/cmsis_nn/*.cc   （OPTIMIZED_KERNEL_DIR=cmsis_nn）
      2. micro/*.cc    ——> cortex_m_generic/*.cc   （debug_log.cc / micro_time.cc）

    直接 GLOB 会带上测试/基准/示例、也拿不到替换规则（会出现重复符号）。
    本脚本把上游 sources.inc 原样翻译一遍，TFLM 升级后重跑即可。

用法：
    python3 gen_sources.py --tflm-root <tflite-micro 路径> [--with-signal]

输出：一段可直接粘进 CMakeLists.txt 的 set(...) 片段（路径相对 lib/）。
"""

import argparse
import glob
import os
import re
import sys


def parse_blocks(text):
    """解析 sources.inc：把 "VAR := \\" 起始、行尾反斜杠续行的块拆出来。"""
    blocks = {}
    cur = None
    for raw in text.splitlines():
        line = raw.rstrip()
        if cur is None:
            m = re.match(r"^([A-Z0-9_]+)\s*:=\s*\\?\s*$", line)
            if m:
                cur = m.group(1)
                blocks[cur] = []
            continue
        if line.endswith("\\"):
            body = line[:-1].strip()
            if body:
                blocks[cur].append(body)
        else:
            if line.strip():
                blocks[cur].append(line.strip())
            cur = None
    return blocks


def expand(lines, root):
    """展开 $(wildcard ...) 与 $(TENSORFLOW_ROOT)，得到绝对路径列表。"""
    out = []
    for line in lines:
        if "$(wildcard" in line:
            m = re.search(r"\$\(wildcard\s+(.*)\)", line)
            pattern = m.group(1).replace("$(TENSORFLOW_ROOT)", root + "/")
            out.extend(sorted(glob.glob(pattern)))
        elif "$(call" in line or "$(recursive_find" in line:
            # 例子/集成测试目录的递归查找，库源用不到
            continue
        else:
            out.append(line.replace("$(TENSORFLOW_ROOT)", root + "/"))
    return out


def specialize(files, directory):
    """同名替换：files 里 basename 在 directory 中存在的，换成 directory 下的那份。"""
    if not os.path.isdir(directory):
        return files
    names = set(os.listdir(directory))
    return [
        os.path.join(directory, os.path.basename(f)) if os.path.basename(f) in names else f
        for f in files
    ]


# 不纳入构建的上游源（连同其同名特化一起丢掉）。
# 理由写在各自的注释里，都是"我们的平台有更合适的实现"这类情况。
DROP_BASENAMES = {
    # cortex_m_generic 版用 DWT 循环计数器，需要额外定义 CMSIS 器件头宏
    # （CMSIS_DEVICE_ARM_CORTEX_M_XX_HEADER_FILE / ARMCMx），且随开发板变化；
    # 我们改由 lib_tflm_port.cc 接 bsp_dwt，不必按开发板映射头文件名。
    "micro_time.cc": "改由 lib/lib_tflm/lib_tflm_port.cc 提供（bsp_dwt 时间源）",
}


def drop(files):
    """丢掉 DROP_BASENAMES 里的文件（含特化后的同名文件）。"""
    kept, dropped = [], []
    for f in files:
        if os.path.basename(f) in DROP_BASENAMES:
            dropped.append(f)
        else:
            kept.append(f)
    return kept, dropped


def rel_to_lib(paths, lib_dir):
    """转成相对 lib/ 的路径（lib 模块的 MODULE_SOURCES 以 lib/ 为基准加前缀）。"""
    rel = []
    for p in paths:
        r = os.path.relpath(os.path.realpath(p), os.path.realpath(lib_dir))
        rel.append(r)
    return rel


def cmsis_nn_sources(cmsisnn_root):
    """CMSIS-NN：Source 下全部 .c，去掉 f16/f32 扩展（与 ARM_NN_ENABLE_F16/F32=0 对齐）。"""
    srcs = glob.glob(os.path.join(cmsisnn_root, "Source", "**", "*.c"), recursive=True)
    return [
        s
        for s in sorted(srcs)
        if not s.endswith("_f16.c")
        and not s.endswith("_f32.c")
        and os.path.basename(s) != "arm_nntables_flt.c"
    ]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tflm-root", required=True, help="tflite-micro 仓库根目录")
    ap.add_argument("--cmsisnn-root", default=None, help="CMSIS-NN 仓库根目录（给出则一并列出）")
    ap.add_argument("--lib-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."),
                    help="lib/ 目录（相对路径基准），默认 lib/lib_tflm/tools/../..")
    ap.add_argument("--with-signal", action="store_true",
                    help="包含 signal/ FFT 内核（需要 kissfft 子模块，默认不含）")
    args = ap.parse_args()

    root = os.path.realpath(args.tflm_root)
    inc = os.path.join(root, "tensorflow/lite/micro/tools/make/sources.inc")
    if not os.path.isfile(inc):
        sys.exit(f"找不到 {inc}，--tflm-root 是否正确？")

    blocks = parse_blocks(open(inc).read())

    base = expand(blocks["MICROLITE_CC_BASE_SRCS"], root)
    tests = set(expand(blocks.get("MICROLITE_TEST_SRCS", []), root))
    bench = set(expand(blocks.get("MICROLITE_BENCHMARK_SRCS", []), root))
    core = [f for f in base if f not in tests and f not in bench]

    kernels = expand(blocks["MICROLITE_CC_KERNEL_SRCS"], root)
    kernels = specialize(kernels, os.path.join(root, "tensorflow/lite/micro/kernels/cmsis_nn"))
    core = specialize(core, os.path.join(root, "tensorflow/lite/micro/cortex_m_generic"))

    all_srcs = core + kernels

    if args.with_signal:
        all_srcs += expand(blocks.get("MICROLITE_CC_SIGNAL_KERNEL_SRCS", []), root)

    all_srcs, dropped = drop(all_srcs)
    for f in dropped:
        print(f"    # 已排除 {os.path.relpath(f, root)}"
              f"：{DROP_BASENAMES[os.path.basename(f)]}", file=sys.stderr)

    if args.cmsisnn_root:
        all_srcs += cmsis_nn_sources(os.path.realpath(args.cmsisnn_root))

    rel = rel_to_lib(all_srcs, args.lib_dir)
    print(f"    # {len(rel)} 个源文件（由 tools/gen_sources.py 生成，勿手改）")
    for r in rel:
        print(f"    {r}")


if __name__ == "__main__":
    main()
