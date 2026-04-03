/**
 * @file bsp_dwt.h
 * @brief DWT高精度定时器封装，提供不受中断影响的延时和时间轴功能
 * @author Wang Hongxi
 * @author modified by NeoZng
 */

#ifndef __BSP_DWT_H
#define __BSP_DWT_H

#include "main.h"
#include "bsp_cfg.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief DWT时间结构体
 */
typedef struct
{
    uint32_t s;  // 秒
    uint16_t ms; // 毫秒
    uint16_t us; // 微秒
} DWT_Time_t;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化DWT
 * @note 使用 bsp_cfg.h 中的 CPU_FREQ_MHZ 自动配置
 */
void DWT_Init(void);

/**
 * @brief 获取两次调用之间的时间间隔
 * @param cnt_last 上一次调用的时间戳指针
 * @return 时间间隔，单位：秒
 */
float DWT_GetDeltaT(uint32_t *cnt_last);

/**
 * @brief 获取两次调用之间的时间间隔（高精度）
 * @param cnt_last 上一次调用的时间戳指针
 * @return 时间间隔，单位：秒
 */
double DWT_GetDeltaT64(uint32_t *cnt_last);

/**
 * @brief 获取当前时间轴（秒）
 * @return 初始化后的时间，单位：秒
 */
float DWT_GetTimeline_s(void);

/**
 * @brief 获取当前时间轴（毫秒）
 * @return 初始化后的时间，单位：毫秒
 */
float DWT_GetTimeline_ms(void);

/**
 * @brief 获取当前时间轴（微秒）
 * @return 初始化后的时间，单位：微秒
 */
uint64_t DWT_GetTimeline_us(void);

/**
 * @brief DWT延时函数
 * @attention 该函数不受中断是否开启的影响，可以在临界区和关闭中断时使用
 * @note 禁止在 __disable_irq() 和 __enable_irq() 之间使用 HAL_Delay()，应使用本函数
 * @param delay 延时时间，单位：秒
 */
void DWT_Delay(float delay);

/**
 * @brief 更新时间轴
 * @attention 如果长时间不调用 timeline 函数，需要手动调用该函数更新时间轴
 *             否则 CYCCNT 溢出后定时和时间轴不准确
 */
void DWT_SysTimeUpdate(void);

#endif /* __BSP_DWT_H */