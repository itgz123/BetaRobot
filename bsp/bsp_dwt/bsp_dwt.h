/**
 * @file bsp_dwt.h
 * @brief DWT高精度定时器封装，提供不受中断影响的延时和时间轴功能
 * @author Wang Hongxi
 * @author modified by NeoZng
 */

#ifndef __BSP_DWT_H
#define __BSP_DWT_H

#include "main.h"
#include "bsp.h"
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

#endif /* __BSP_DWT_H */
