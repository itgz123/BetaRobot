#!/usr/bin/env bash
# bench_comm_proto_ext.sh — comm_proto_ext 单帧耗时测量（评估 ISR / 控制周期预算）
#
# 用法：./bench_comm_proto_ext.sh
#   与 test_comm_proto_ext.sh 同样的编译方式（真实实现 + 同目录 app_cfg.h 桩，
#   -I . 必须排在前面，否则引号包含会先命中工程里的真 app_cfg.h）。
# 输出：bench_comm_proto_ext.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

LOG=bench_comm_proto_ext.log
ROOT=../../../..

gcc -O2 -Wall -Wextra -std=c11 \
    -I . \
    -I .. \
    -I "$ROOT/lib/lib_hamming" \
    -I "$ROOT/lib/lib_crc" \
    -o bench_comm_proto_ext \
    bench_comm_proto_ext.c \
    ../comm_proto.c ../comm_proto_raw.c ../comm_proto_custom.c ../comm_proto_ext.c \
    "$ROOT/lib/lib_hamming/lib_hamming.c" "$ROOT/lib/lib_hamming/lib_hamming_ext.c" \
    "$ROOT/lib/lib_crc/lib_crc.c" "$ROOT/lib/lib_crc/lib_crc_tables.c"

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    exit 1
fi

./bench_comm_proto_ext > "$LOG" 2>&1
rc=$?
rm -f bench_comm_proto_ext     # 目录里只留源码 + 日志（对齐 lib_math tools 约定）
cat "$LOG"
echo "[log] $LOG"
exit $rc
