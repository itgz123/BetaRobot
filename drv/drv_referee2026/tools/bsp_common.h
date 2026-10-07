/**
 * @file bsp_common.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 两个枚举逐字抄自 bsp/bsp_common/bsp_common.h（取值一个不差 —— 它们会进返回值比较，
 * 抄错会让测试"通过"一个假的语义）。真 main.h 经 bsp_map.h 把它们带进来，本目录的
 * main.h 桩照做。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __BSP_COMMON_PC_STUB_H
#define __BSP_COMMON_PC_STUB_H

#include <stdint.h>

/** BSP 层统一返回状态码（真值见 bsp/bsp_common/bsp_common.h） */
typedef enum : int8_t
{
    BSP_OK = 0,
    BSP_BUSY = -1,
    BSP_TIMEOUT = -2,
    BSP_PARAM_ERR = -3,
    BSP_HW_ERR = -4,
} BSP_Status_e;

/** 传输模式 */
typedef enum : uint8_t
{
    BSP_BLOCK_MODE = 0,
    BSP_IT_MODE = 1,
    BSP_DMA_MODE = 2,
} BSP_Transfer_Mode_e;

#endif /* __BSP_COMMON_PC_STUB_H */
