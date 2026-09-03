#!/usr/bin/env bash
# test_trig_lut.sh — PC 端查表三角函数一键检验（gcc 编译，Git Bash 下运行）
#
# 用法：./test_trig_lut.sh [quarter|full] [prec...]
#       默认测全部 8 种组合（SPEED 0/1 × PREC 0..3）；
#       quarter 默认测 SPEED=0（四分之一表）的 PREC 0 1 2 3（256/512/1024/2048）；
#       full    默认测 SPEED=1（全周期表）的 PREC 0 1 2 3（1024/2048/4096/8192）。
# 选档走头文件高层入口：-DLIB_MATH_TRIG_LUT_STANDALONE 跳过 app_cfg.h（PC 端不引入
# 工程配置），命令行 -D SPEED/PREC 与固件 app_cfg.h 走同一映射。
# 输出：每档生成 test_trig_lut_<kind>_<SIZE>.log，脚本退出码反映全部档位是否 PASS。
set -u
cd "$(dirname "$0")" || exit 1

KIND=all
if [ $# -gt 0 ]; then
    case "$1" in
        quarter) KIND=quarter; shift ;;
        full)    KIND=full;    shift ;;
        *) ;;
    esac
fi
PRECS=()
if [ $# -gt 0 ]; then
    PRECS=("$@")
fi

# 各档 ModeA 目标：来自 gen_trig_lut.py 实测，满精度档(>=2048)取 float32 机器精度 ε=2^-23
declare -A TARGET=(
    [256]=2.0e-5
    [512]=1.3e-6
    [1024]=1.3e-6
    [2048]=1.1920928955078125e-07
    [4096]=1.1920928955078125e-07
    [8192]=1.1920928955078125e-07
)

# SPEED/PREC → 表区间数 SIZE（与 lib_math_trig_lut.h 高层映射一致）
size_for() {
    case "$1:$2" in
        1:0) echo 1024 ;; 1:1) echo 2048 ;; 1:2) echo 4096 ;; 1:3) echo 8192 ;;
        0:0) echo 256  ;; 0:1) echo 512  ;; 0:2) echo 1024 ;; 0:3) echo 2048 ;;
    esac
}

overall=0
test_one() {
    local sp=$1 pr=$2 size kindname tgt rc
    size=$(size_for "$sp" "$pr")
    if [ "$sp" = 1 ]; then
        kindname=full
    else
        kindname=quarter
    fi
    if [ "$sp" = 1 ]; then
        # FULL 档 N 与 QUARTER 档 M=N/4 同 h 同误差，目标按 N/4 映射到 QUARTER 同精度档
        tgt=${TARGET[$((size / 4))]:-1.1920928955078125e-07}
    else
        tgt=${TARGET[$size]:-1.1920928955078125e-07}
    fi
    echo "===== SPEED=$sp PREC=$pr → KIND=$kindname, SIZE = $size ====="
    gcc -O2 -Wall -Wextra -std=c11 \
        -DLIB_MATH_TRIG_LUT_USED \
        -DLIB_MATH_TRIG_LUT_STANDALONE \
        -DLIB_MATH_TRIG_LUT_SPEED="$sp" \
        -DLIB_MATH_TRIG_LUT_PREC="$pr" \
        -DTEST_MODE_A_TARGET="$tgt" \
        -I .. -o test_trig_lut test_trig_lut.c ../lib_math_trig_lut.c -lm
    if [ $? -ne 0 ]; then
        echo "[FAIL] SPEED=$sp PREC=$pr 编译失败"
        overall=1
        return
    fi
    ./test_trig_lut > "test_trig_lut_${kindname}_$size.log" 2>&1
    rc=$?
    if [ $rc -ne 0 ]; then
        overall=1
    fi
    tail -n 14 "test_trig_lut_${kindname}_$size.log"
    echo "[log] test_trig_lut_${kindname}_$size.log"
    echo ""
}

if [ "$KIND" = all ]; then
    for sp in 0 1; do
        for pr in 0 1 2 3; do
            test_one "$sp" "$pr"
        done
    done
else
    if [ ${#PRECS[@]} -eq 0 ]; then
        PRECS=(0 1 2 3)
    fi
    if [ "$KIND" = full ]; then
        sp=1
    else
        sp=0
    fi
    for pr in "${PRECS[@]}"; do
        test_one "$sp" "$pr"
    done
fi

if [ $overall -eq 0 ]; then
    echo "ALL PASS"
else
    echo "SOME FAILED"
fi
exit $overall
