#!/usr/bin/env bash
# test_rls.sh — lib_rls（通用 RLS 纯算法库）PC 端测试（gcc 编译）
#
# 用法：./test_rls.sh
#   -DLIB_RLS_STANDALONE            跳过 app_cfg.h（PC 端不引入工程配置）；
#   -DLIB_RLS_USED                  打开模块实现（与固件 app_cfg.h 同一开关）；
#   -DLIB_MATH_TRIG_LUT_STANDALONE  lib_math 伞形头同样跳过 app_cfg.h。
# 依赖：lib_math（Lib_Math_Clamp / Lib_Math_Fabs）与标准 <math.h>。
# 输出：test_rls.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_rls.log
BIN=test_rls

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_RLS_USED -DLIB_RLS_STANDALONE -DLIB_MATH_TRIG_LUT_STANDALONE \
    -I .. -I ../../lib_math \
    -o "$BIN" test_rls.c ../lib_rls.c -lm
rc=$?
if [ $rc -ne 0 ]; then
    echo "编译失败（rc=$rc）"
    rm -f "$BIN"
    exit $rc
fi

./"$BIN" > "$LOG" 2>&1
rc=$?
rm -f "$BIN"

cat "$LOG"
if [ $rc -eq 0 ]; then
    echo "==> PASS（日志：$(pwd)/$LOG）"
else
    echo "==> FAIL（rc=$rc，日志：$(pwd)/$LOG）"
fi
exit $rc
