#!/usr/bin/env bash
# test_imu_transform.sh — lib_imu_transform（IMU 系→目标系 + 杆臂补偿）PC 端测试
#
# 用法：./test_imu_transform.sh
#   -DLIB_IMU_TRANSFORM_STANDALONE   跳过 app_cfg.h（PC 端不引入工程配置）；
#   -DLIB_IMU_TRANSFORM_USED         打开模块实现（与固件 app_cfg.h 同一开关）；
#   -DLIB_MATH_TRIG_LUT_STANDALONE   lib_math 伞形头同样跳过 app_cfg.h。
# 依赖：lib_math（向量/矩阵内联函数）与标准 <math.h>。
# 输出：test_imu_transform.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_imu_transform.log
BIN=test_imu_transform

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_IMU_TRANSFORM_USED -DLIB_IMU_TRANSFORM_STANDALONE -DLIB_MATH_TRIG_LUT_STANDALONE \
    -I .. -I ../../lib_math \
    -o "$BIN" test_imu_transform.c ../lib_imu_transform.c -lm
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
