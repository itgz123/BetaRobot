#!/usr/bin/env bash
# test_ekf.sh — lib_ekf PC 端测试一键检验（gcc 编译，Git Bash 下运行）
#
# 用法：./test_ekf.sh
# 说明：-DLIB_EKF_STANDALONE / -DLIB_LKF_STANDALONE 跳过 app_cfg.h（PC 端不引入
#       工程配置），-DLIB_*_USED 与固件 app_cfg.h 走同一门控；
#       lib_math.h 伞形头牵入 lib_math_trig_lut.h，故同样要
#       -DLIB_MATH_TRIG_LUT_STANDALONE（见 lib_math/tools/test_trig_lut.sh）。
#       用例 1 需要 lib_lkf 作对照（lib_lkf 自身已由
#       lib_kf/lib_lkf/tools/test_lkf.sh 对照独立双精度参考验证），故一并链接 lib_lkf.c。
#       目录：lib/lib_kf/lib_ekf/tools/，内核头在 lib/lib_kf/（-I ../..），
#       lib_lkf 在同级 ../../lib_lkf。
# 输出：test_ekf.log（入库），脚本退出码反映是否全部 PASS。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_ekf.log
BIN=test_ekf

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_EKF_USED -DLIB_EKF_STANDALONE \
    -DLIB_LKF_USED -DLIB_LKF_STANDALONE \
    -DLIB_MATH_TRIG_LUT_STANDALONE \
    -I .. -I ../../lib_lkf -I ../.. -I ../../../lib_math \
    -o "$BIN" test_ekf.c ../lib_ekf.c ../../lib_lkf/lib_lkf.c -lm
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
