/**
 * @file bsp_tim.h
 * @brief 定时器驱动封装，提供PWM和编码器实例管理功能
 *
 * @note 硬件配置（频率/周期/通道/编码器模式等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_TIM_H
#define __BSP_TIM_H

#include "bsp.h"

#ifdef HAL_TIM_MODULE_ENABLED

#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief PWM实例结构体
 * @note 注册即启动，不用时设置 dutyratio = 0 即可
 */
typedef struct PWMInstance
{
    BoardTIM_e tim_e; // 板载TIM枚举（注册时用于查找映射）
    TIM_Map_t map;    // 硬件映射（注册时自动填充）
    float dutyratio;  // 占空比（0~1）
} PWMInstance;

/**
 * @brief 编码器实例结构体
 * @note 注册即启动，自动使能溢出中断
 */
typedef struct EncoderInstance
{
    BoardTIM_e tim_e;         // 板载TIM枚举（注册时用于查找映射）
    TIM_Map_t map;            // 硬件映射（注册时自动填充）
    uint32_t arr;             // 溢出周期（ARR + 1）
    int32_t overflow_count;   // 溢出次数
    int64_t total_count;      // 扩展总计数（用于速度计算）
    int64_t last_total_count; // 上次总计数
    float speed;              // 当前速度（脉冲/秒）
    float last_time;          // 上次更新时间
} EncoderInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief PWM实例静态定义宏
 * @param name    实例名称
 * @param tim_idx 板载TIM枚举（TIM_PWM_1~4）
 */
#define PWM_INSTANCE_DEF(name, tim_idx) \
    PWMInstance name = {                \
        .tim_e = tim_idx,               \
        .map = {NULL, 0},               \
        .dutyratio = 0.0f}

/**
 * @brief 编码器实例静态定义宏
 * @param name    实例名称
 * @param tim_idx 板载TIM枚举（TIM_ENCODER_1~4）
 */
#define ENCODER_INSTANCE_DEF(name, tim_idx) \
    EncoderInstance name = {                \
        .tim_e = tim_idx,                   \
        .map = {NULL, 0},                   \
        .arr = 0,                           \
        .overflow_count = 0,                \
        .total_count = 0,                   \
        .last_total_count = 0,              \
        .speed = 0.0f,                      \
        .last_time = 0.0f}

/*------------- PWM接口声明 --------------*/

/**
 * @brief 注册PWM实例
 * @param instance PWM实例指针
 * @return 0成功，-1失败
 * @note 注册即启动PWM输出
 */
int8_t PWMRegister(PWMInstance *instance);

/**
 * @brief 设置PWM占空比
 * @param instance   PWM实例指针
 * @param dutyratio  占空比（0~1），超出范围自动钳位
 */
void PWMSetDutyRatio(PWMInstance *instance, float dutyratio);

/*------------- 编码器接口声明 --------------*/

/**
 * @brief 注册编码器实例
 * @param instance 编码器实例指针
 * @return 0成功，-1失败
 * @note 注册即启动编码器，自动使能更新中断
 */
int8_t EncoderRegister(EncoderInstance *instance);

/**
 * @brief 获取当前速度（脉冲/秒）
 * @param instance 编码器实例指针
 * @return 速度值，失败返回0
 * @note 此函数会更新 instance->speed 成员
 */
float EncoderGetSpeed(EncoderInstance *instance);

/**
 * @brief 清零编码器计数
 * @param instance 编码器实例指针
 */
void EncoderClearCount(EncoderInstance *instance);

/*------------- 溢出回调接口（供main.c调用） --------------*/

/**
 * @brief 定时器溢出回调分发接口
 * @param htim 定时器句柄
 * @note 在 main.c 的 HAL_TIM_PeriodElapsedCallback 中调用此函数
 * @example
 *   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
 *   {
 *       if (htim->Instance == TIM6) { HAL_IncTick(); }
 *       BSPTim_PeriodElapsedCallback(htim);
 *   }
 */
void BSPTim_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif // BSP_TIM_MODULE_ENABLED

#endif /* __BSP_TIM_H */