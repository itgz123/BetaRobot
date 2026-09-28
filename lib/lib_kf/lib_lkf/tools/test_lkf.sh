#!/usr/bin/env bash
# test_lkf.sh — lib_lkf PC 端回归测试一键检验（gcc 编译，Git Bash 下运行）
#
# 用法：./test_lkf.sh
# 说明：-DLIB_LKF_STANDALONE 跳过 app_cfg.h（PC 端不引入工程配置），
#       -DLIB_LKF_USED 与固件 app_cfg.h 走同一门控；
#       lib_math.h 伞形头牵入 lib_math_trig_lut.h，故同样要
#       -DLIB_MATH_TRIG_LUT_STANDALONE（见 lib_math/tools/test_trig_lut.sh）。
#       参考实现写在 test_lkf.c 内（双精度、独立公式），用于保护 lib_lkf 把
#       协方差代数下沉到 lib_kf_core.h 的重构。
#       目录：lib/lib_kf/lib_lkf/tools/，内核头在 lib/lib_kf/（-I ../..）。
# 输出：test_lkf.log（入库），脚本退出码反映是否全部 PASS。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_lkf.log
BIN=test_lkf

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_LKF_USED -DLIB_LKF_STANDALONE -DLIB_MATH_TRIG_LUT_STANDALONE \
    -I .. -I ../.. -I ../../../lib_math \
    -o "$BIN" test_lkf.c ../lib_lkf.c -lm
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
