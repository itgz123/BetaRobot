/**
 * @file main.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 真 main.h 由 CubeMX 生成，本模块从它那里只要四样东西：HAL 的 UART 模块开关（驱动有
 * 半截身子在这条判据里）、板载外设枚举、UART 初始化结构体、以及经 bsp_map.h 转手的
 * BSP 状态码。桩里就只补这四样。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __MAIN_PC_STUB_H
#define __MAIN_PC_STUB_H

#include <stddef.h>
#include <stdint.h>

/* 真 main.h 经 bsp_map.h 把 BSP 的统一状态码/传输模式带进来，桩里照做（不然
 * bsp_usart.h 就找不到 BSP_Status_e） */
#include "bsp_common.h"

/* HAL 的 UART 模块开关：referee2026 的驱动整节都在这条判据里。语义与全仓一致 ——
 * 定义即开（`#define ..._USED 0` 那种写法在这里帮倒忙）。 */
#define HAL_UART_MODULE_ENABLED

/* 板载 UART 枚举：成员抄自 bsp/bsp_map/DJI_C/bsp_map.h。PC 上不查硬件映射，
 * 只要求"能取到一个值"（测试里用它验 Config 把它原样透传给了 bsp）。 */
typedef enum
{
    UART_SBUS = 0,
    UART_1,
    UART_6,
    UART_NUM_MAX
} BoardUART_e;

/* H7 上落 RAM_D1 以支持 DMA 访问，PC 上无条件为空 */
#define DMA_RAM

/* HAL 的两个类型与 UART 常量。PC 上没有任何东西读这些值（不初始化硬件），
 * 字段名与真 UART_InitTypeDef 一致即可 —— 协议表用的是指定初始化器，名字对得上就填得进。 */
typedef struct
{
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
    uint32_t OneBitSampling; /* H7 独有，F4 上没有 —— 指定初始化器下隐式清零 */
    uint32_t ClockPrescaler; /* 同上 */
} UART_InitTypeDef;

typedef struct
{
    uint32_t unused;
} UART_HandleTypeDef;

#define UART_WORDLENGTH_8B 0u
#define UART_STOPBITS_1 0u
#define UART_PARITY_NONE 0u
#define UART_MODE_TX_RX 1u
#define UART_HWCONTROL_NONE 0u
#define UART_OVERSAMPLING_16 0u

#endif /* __MAIN_PC_STUB_H */
