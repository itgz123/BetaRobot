/**
 * @file bsp_dwt_cyccnt.h
 * @brief DWT CYCCNT 计时封装，提供不受中断影响的延时和时间轴功能
 * @author Wang Hongxi
 * @author modified by NeoZng
 *
 * @note DWT_Init() 是整个 DWT 功能族唯一的必需初始化入口（使能 DWT 核心跟踪
 *       与 CYCCNT），其他子模块的 *Init() 不得调用它，也不得清零 CYCCNT。
 */

#ifndef __BSP_DWT_CYCCNT_H
#define __BSP_DWT_CYCCNT_H

#include "main.h"
#include "bsp_map.h"
#include "stdint.h"

/*------------- 外部接口声明 --------------*/

void DWT_Init(void);

/**
 * @brief 获取当前系统时间（微秒）
 * @return 自 DWT_Init 以来的累计时间，单位：微秒
 */
uint64_t DWT_GetTimeUs(void);

/**
 * @brief DWT延时函数
 * @attention 该函数不受中断是否开启的影响，可以在临界区和关闭中断时使用
 * @note 禁止在 __disable_irq() 和 __enable_irq() 之间使用 HAL_Delay()，应使用本函数
 * @param delay 延时时间，单位：秒
 */
void DWT_Delay(float delay);

#endif /* __BSP_DWT_CYCCNT_H */
