#!/usr/bin/env bash
# test_eskf.sh — lib_eskf PC 端测试一键检验（gcc 编译，Git Bash 下运行）
#
# 用法：./test_eskf.sh
# 说明：-DLIB_ESKF_STANDALONE 跳过 app_cfg.h（PC 端不引入工程配置），
#       -DLIB_ESKF_USED 与固件 app_cfg.h 走同一门控。
#       本测试自带独立四元数真值模型，只依赖 lib_eskf 本身与 libm。
#       目录：lib/lib_kf/lib_eskf/tools/，内核头在 lib/lib_kf/（-I ../..）。
# 输出：test_eskf.log（入库），脚本退出码反映是否全部 PASS。
set -u
cd "$(dirname "$0")" || exit 1

CC=${CC:-gcc}
LOG=test_eskf.log
BIN=test_eskf

$CC -O2 -Wall -Wextra -std=c11 \
    -DLIB_ESKF_USED -DLIB_ESKF_STANDALONE \
    -I .. -I ../.. -I ../../../lib_math \
    -o "$BIN" test_eskf.c ../lib_eskf.c -lm
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
