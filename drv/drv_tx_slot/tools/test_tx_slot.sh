#!/usr/bin/env bash
# test_tx_slot.sh — PC 端 drv_tx_slot 发送槽池检验（gcc 编译，Git Bash 下可运行）
#
# 用法：./test_tx_slot.sh
#   -DDRV_TX_SLOT_STANDALONE 临界区退化为空（PC 上没有中断）；
#   模块本身不依赖 bsp/lib，故只需编译真实实现 + 假 transport 的测试文件。
# 输出：test_tx_slot.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

LOG=test_tx_slot.log

# -std=gnu11：本项目所有 enum 都写 `typedef enum : uintN_t`（C23 才正式进标准，
# 之前在 GCC 里是扩展），与 CMake 的 C_EXTENSIONS ON 保持一致。
gcc -O2 -Wall -Wextra -std=gnu11 \
    -DDRV_TX_SLOT_STANDALONE \
    -I .. -o test_tx_slot \
    test_tx_slot.c ../drv_tx_slot.c

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    exit 1
fi

./test_tx_slot > "$LOG" 2>&1
rc=$?
rm -f test_tx_slot          # 目录里只留源码 + 日志（对齐 lib_*/drv_* tools 约定）
tail -n 8 "$LOG"
echo "[log] $LOG"
if [ $rc -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc
