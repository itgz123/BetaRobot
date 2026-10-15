#!/usr/bin/env bash
# test_proto_pair_match.sh — PC 端"两端协议名必须一致"检验（PROTO_EXT vs PROTO_CUSTOM 错配）
#
# 用法：./test_proto_pair_match.sh
#   直接编译真实实现（comm_proto_custom.c + comm_proto_ext.c + comm_proto.c + lib_hamming*.c
#   + lib_crc*.c），只有 app_cfg.h 用同目录的桩（避免拉进 bsp/cubemx 头）；-I . 必须排在前面，
#   否则引号包含会先命中工程里的真 app_cfg.h。
# 输出：test_proto_pair_match.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

LOG=test_proto_pair_match.log
ROOT=../../../..

gcc -O2 -Wall -Wextra -std=c11 \
    -I . \
    -I .. \
    -I "$ROOT/lib/lib_hamming" \
    -I "$ROOT/lib/lib_crc" \
    -o test_proto_pair_match \
    test_proto_pair_match.c \
    ../comm_proto.c ../comm_proto_raw.c ../comm_proto_custom.c ../comm_proto_ext.c \
    "$ROOT/lib/lib_hamming/lib_hamming.c" "$ROOT/lib/lib_hamming/lib_hamming_ext.c" \
    "$ROOT/lib/lib_crc/lib_crc.c" "$ROOT/lib/lib_crc/lib_crc_tables.c"

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    exit 1
fi

./test_proto_pair_match > "$LOG" 2>&1
rc=$?
rm -f test_proto_pair_match   # 目录里只留源码 + 日志（对齐 lib_math/lib_hamming tools 约定）
tail -n 12 "$LOG"
echo "[log] $LOG"
if [ $rc -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc
