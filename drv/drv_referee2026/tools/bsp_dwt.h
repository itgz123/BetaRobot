/**
 * @file bsp_dwt.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 签名抄自 bsp/bsp_dwt/bsp_dwt.h，实现在 test_referee2026.c 里：一个由测试自己驱动的
 * 假微秒时基（可冻结、可跳变），用来验"限速窗口"与"tick 时间戳"这类依赖时间的判据。
 * **不要**把本目录加进固件的 include 路径。
 */

#ifndef __BSP_DWT_PC_STUB_H
#define __BSP_DWT_PC_STUB_H

#include <stdint.h>

/** 自增的微秒计数（真板上是 DWT 的 CYCCNT 换算，不受中断影响） */
uint64_t DWT_GetTimeUs(void);

#endif /* __BSP_DWT_PC_STUB_H */
