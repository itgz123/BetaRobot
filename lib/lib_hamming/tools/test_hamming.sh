#!/usr/bin/env bash
# test_hamming.sh — PC 端汉明码穷举检验（gcc 编译，Git Bash 下可运行）
#
# 用法：./test_hamming.sh
#   -DLIB_HAMMING_STANDALONE 跳过 app_cfg.h（PC 端不引入工程配置）；
#   -DLIB_HAMMING_USED       打开模块实现（与固件 app_cfg.h 同一开关）。
# 输出：test_hamming.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

LOG=test_hamming.log

gcc -O2 -Wall -Wextra -std=c11 \
    -DLIB_HAMMING_USED \
    -DLIB_HAMMING_STANDALONE \
    -I .. -o test_hamming \
    test_hamming.c ../lib_hamming.c ../lib_hamming_ext.c

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    exit 1
fi

./test_hamming > "$LOG" 2>&1
rc=$?
rm -f test_hamming          # 目录里只留源码 + 日志（对齐 lib_math tools 约定）
tail -n 5 "$LOG"
echo "[log] $LOG"
if [ $rc -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc
