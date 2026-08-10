/**
 * @file bsp.h
 * @brief 板级支持包统一头文件
 *
 * @note 包含板级常量、检查宏、板选逻辑
 *       各开发板的类型/枚举/映射数组定义分别在 hal/xxx/bsp_map.h/.c 中
 */

#ifndef __BSP_H
#define __BSP_H

#include "bsp_assert.h"

// 开发板类型
#define DM_MC02 0
#define DJI_A 1
#define DJI_C 2

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

#endif /* __BSP_H */
