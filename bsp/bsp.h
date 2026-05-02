/**
 * @file bsp.h
 * @brief 板级支持包统一头文件
 *
 * @note 包含板级常量、检查宏、板选逻辑
 *       各开发板的类型/枚举/映射数组定义分别在 hal/xxx/bsp_map.h/.c 中
 */

#ifndef __BSP_H
#define __BSP_H

#include "app_cfg.h"

/*============================================
 *              内核类型常量
 *============================================*/
#define CORTEX_M4 0
#define CORTEX_M7 1

/*============================================
 *              CAN 控制器类型常量
 *============================================*/
#define BSP_CAN_IP_BXCAN 0
#define BSP_CAN_IP_FDCAN 1

/*============================================
 *              通用超时宏
 *============================================*/
#define USART_BLOCK_TIMEOUT_MS 100
#define SPI_BLOCK_TIMEOUT_MS 100

/*============================================
 *              通用断言宏
 *============================================*/
#define BSP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BSP_STATIC_ASSERT_MAP_SIZE(arr, max) _Static_assert(BSP_ARRAY_SIZE(arr) == (max), #arr " size must equal " #max)

#define BSP_RETURN_IF_TRUE(cond, ret) \
    do { if (cond) { return (ret); } } while (0)

#define BSP_RETURN_IF_TRUE_LOG(cond, ret, log_expr) \
    do { if (cond) { log_expr; return (ret); } } while (0)

/*============================================
 *              板级映射（根据开发板选择）
 *============================================*/
#if DEVELOPMENT_BOARD == STM32F407VET6
#include "hal/STM32F407VET6/bsp_map.h"
#elif DEVELOPMENT_BOARD == DM_MC02
#include "hal/DM_MC02/bsp_map.h"
#elif DEVELOPMENT_BOARD == DJI_A
#include "hal/DJI_A/bsp_map.h"
#elif DEVELOPMENT_BOARD == DJI_C
#include "hal/DJI_C/bsp_map.h"
#else
#error "Unsupported DEVELOPMENT_BOARD"
#endif

#endif /* __BSP_H */
