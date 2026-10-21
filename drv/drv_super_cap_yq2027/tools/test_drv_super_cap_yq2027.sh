#!/usr/bin/env bash
# drv_super_cap_yq2027 离线测试：用主机 gcc 编译真实驱动实现 + 桩
set -u

cd "$(dirname "$0")" || exit 1

LOG=test_drv_super_cap_yq2027.log
ROOT=../../..
DRV=$ROOT/drv/drv_super_cap_yq2027
BIN=test_drv_super_cap_yq2027

# -I . 必须放最前：让引号包含命中本目录的桩，而不是工程里的真头文件
gcc -O2 -Wall -Wextra -std=gnu11 \
    -I . \
    -I "$DRV" \
    -o "$BIN" \
    test_drv_super_cap_yq2027.c \
    "$DRV/drv_super_cap_yq2027.c" \
    > "$LOG" 2>&1

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    cat "$LOG"
    rm -f "$BIN"
    exit 1
fi

./"$BIN" >> "$LOG" 2>&1
rc=$?
rm -f "$BIN"

tail -n 8 "$LOG"
echo "[log] $LOG"
if [ $rc -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc
