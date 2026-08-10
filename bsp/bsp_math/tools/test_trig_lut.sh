#!/usr/bin/env bash
# test_trig_lut.sh — PC 端查表三角函数一键检验（gcc 编译，Git Bash 下运行）
#
# 用法：./test_trig_lut.sh [大小...]        # 默认 256 1024 4096
# 输出：每个档位生成 test_trig_lut_<M>.log（含全部检验结果），脚本退出码
#       反映所有档位是否全部 PASS。
set -u
cd "$(dirname "$0")" || exit 1

if [ $# -eq 0 ]; then
    SIZES=(256 1024 2048 4096)
else
    SIZES=("$@")
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

overall=0
for M in "${SIZES[@]}"; do
    echo "===== BSP_MATH_TRIG_TABLE_SIZE = $M ====="
    tgt=${TARGET[$M]:-1.1920928955078125e-07}
    gcc -O2 -Wall -Wextra -std=c11 \
        -DBSP_MATH_TRIG_TABLE_SIZE="$M" \
        -DTEST_MODE_A_TARGET="$tgt" \
        -I .. -o test_trig_lut test_trig_lut.c ../bsp_math_trig_lut.c -lm
    if [ $? -ne 0 ]; then
        echo "[FAIL] M=$M 编译失败"
        overall=1
        continue
    fi
    ./test_trig_lut > "test_trig_lut_$M.log" 2>&1
    rc=$?
    if [ $rc -ne 0 ]; then
        overall=1
    fi
    tail -n 14 "test_trig_lut_$M.log"
    echo "[log] test_trig_lut_$M.log"
    echo ""
done

if [ $overall -eq 0 ]; then
    echo "ALL PASS"
else
    echo "SOME FAILED"
fi
exit $overall
