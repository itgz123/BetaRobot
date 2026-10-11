#!/usr/bin/env bash
# test_traj.sh — lib_traj（核心 + 规划器）PC 端测试一键检验（gcc 编译）
#
# 用法：./test_traj.sh [--trials N] [--seed S] [--verbose]
#       （参数原样透传给 test_traj）
# 说明：-DLIB_TRAJ_STANDALONE 跳过 app_cfg.h（PC 端不引入工程配置），
#       -DLIB_TRAJ_USED 与固件 app_cfg.h 走同一门控；核心与规划器共用这一个宏。
#       本模块只依赖 <math.h>/<string.h>，不牵 lib_math。
#       目录：lib/lib_traj/tools/，核心头在 lib/lib_traj/（-I ..），
#       规划器在同级 ../lib_traj_jerk。
# 输出：test_traj.log（入库），脚本退出码反映是否全部 PASS。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_traj.log
BIN=test_traj

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_TRAJ_USED -DLIB_TRAJ_STANDALONE \
    -I .. -I ../lib_traj_jerk \
    -o "$BIN" test_traj.c ../lib_traj.c ../lib_traj_jerk/lib_traj_jerk.c -lm
rc=$?
if [ $rc -ne 0 ]; then
    echo "编译失败（rc=$rc）"
    rm -f "$BIN"
    exit $rc
fi

./"$BIN" "$@" > "$LOG" 2>&1
rc=$?
rm -f "$BIN"

cat "$LOG"
if [ $rc -eq 0 ]; then
    echo "==> PASS（日志：$(pwd)/$LOG）"
else
    echo "==> FAIL（rc=$rc，日志：$(pwd)/$LOG）"
fi
exit $rc
