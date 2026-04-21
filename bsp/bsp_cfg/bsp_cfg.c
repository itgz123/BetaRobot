/**
 * @file bsp_cfg.c
 * @brief 板级硬件映射实现入口
 *
 * @note 各开发板映射已拆分到 boards 目录下的 .inc 文件
 */

#include "bsp_cfg.h"

#define BSP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BSP_STATIC_ASSERT_MAP_SIZE(arr, max) _Static_assert(BSP_ARRAY_SIZE(arr) == (max), #arr " size must equal " #max)

#if DEVELOPMENT_BOARD == STM32F407VET6
#include "boards/stm32f407vet6_map.inc"
#elif DEVELOPMENT_BOARD == DM_MC02
#include "boards/dm_mc02_map.inc"
#elif DEVELOPMENT_BOARD == DJI_A
#include "boards/dji_a_map.inc"
#elif DEVELOPMENT_BOARD == DJI_C
#include "boards/dji_c_map.inc"
#else
#error "Unsupported DEVELOPMENT_BOARD for bsp_cfg mapping"
#endif
