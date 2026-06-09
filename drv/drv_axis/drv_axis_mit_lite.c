/**
 * @file drv_axis_mit_lite.c
 * @brief 轻量级单轴关节控制驱动模块实现
 * @author TRW
 * @date 2026-06-07
 */

#include "drv_axis_mit_lite.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_math.h"
#include <math.h>

/*============================================
 *              私有函数声明
 *============================================*/

/*============================================
 *              公开接口实现
 *============================================*/

int8_t AxisMitLiteInit(AxisLiteInstance *inst, AxisLite_Init_Config_s *cfg)
{
    return 0;
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED
