#!/usr/bin/env bash
# test_referee2026.sh — PC 端 drv_referee2026 收发内核检验（常规链路 + 非链路）
#
# 用法：./test_referee2026.sh
#   直接编译真实实现（drv_referee2026.c + public/referee2026_proto.c +
#   link_common/referee2026_common.c + link_video/referee2026_video.c +
#   link_none/referee2026_none.c + lib_crc.c），
#   只有 main.h / bsp_common.h / bsp_usart.h / drv_daemon.h / bsp_dwt.h / app_cfg.h
#   用同目录的桩（避免拉进 cubemx / bsp / FreeRTOS 头）；-I . 必须排在前面，否则引号包含
#   会先命中工程里的真 app_cfg.h / main.h。
# 输出：test_referee2026.log，退出码 0=PASS / 非 0=FAIL。
set -u
cd "$(dirname "$0")" || exit 1

LOG=test_referee2026.log
ROOT=../../..          # drv/drv_referee2026/tools → 仓库根
DRV=$ROOT/drv/drv_referee2026

# -std=gnu11 而不是 c11：本项目所有 enum 都写 `typedef enum : uintN_t`（C23 才正式进
# 标准，之前在 GCC 里是扩展），核也是 —— 与 CMake 的 C_EXTENSIONS ON 保持一致。
gcc -O2 -Wall -Wextra -std=gnu11 \
    -I . \
    -I "$ROOT/lib/lib_crc" \
    -I "$DRV" \
    -I "$DRV/public" \
    -I "$DRV/link_common" \
    -I "$DRV/link_video" \
    -I "$DRV/link_none" \
    -o test_referee2026 \
    test_referee2026.c \
    "$DRV/drv_referee2026.c" \
    "$DRV/public/referee2026_proto.c" \
    "$DRV/link_common/referee2026_common.c" \
    "$DRV/link_video/referee2026_video.c" \
    "$DRV/link_none/referee2026_none.c" \
    "$ROOT/lib/lib_crc/lib_crc.c"

if [ $? -ne 0 ]; then
    echo "[FAIL] 编译失败"
    exit 1
fi

./test_referee2026 > "$LOG" 2>&1
rc=$?
rm -f test_referee2026   # 目录里只留源码 + 日志（对齐 lib_math/lib_hamming tools 约定）
tail -n 8 "$LOG"
echo "[log] $LOG"
if [ $rc -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit $rc
