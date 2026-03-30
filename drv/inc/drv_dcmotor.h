/**
 * @file drv_dcmotor.h
 * @brief DC 电机驱动封装
 *
 * @note DRV 层职责：
 *       1. 封装 DC 电机控制（PWM 占空比 + 方向）
 *       2. 封装编码器速度读取
 *       3. 不使用 FreeRTOS（队列由 APP 层管理）
 */

#ifndef __DRV_DCMOTOR_H
#define __DRV_DCMOTOR_H

#include "main.h"
#include "bsp_cfg.h"
#include "bsp_tim.h"
#include "bsp_gpio.h"
#include "stdint.h"
#include "math.h"

/*------------- 宏定义 --------------*/

// 速度计算常数
#define DCMOTOR_2PI 6.283185307179586f // 2 * PI

/*------------- 类型定义 --------------*/

/**
 * @brief DC 电机实例结构体
 * @note 内嵌 BSP 层实例，直接管理 PWM、编码器、方向控制 GPIO
 */
typedef struct DCMotorInstance
{
    // BSP 层实例
    PWMInstance pwm_inst;         // PWM 实例
    EncoderInstance encoder_inst; // 编码器实例
    GPIOInstance in1_inst;        // 方向控制 IN1
    GPIOInstance in2_inst;        // 方向控制 IN2

    // 配置参数
    uint16_t encoder_ppr;  // 编码器线数（每转脉冲数）
    float reduction_ratio; // 减速比（输出轴/电机轴，如 30 表示 1:30 减速）
    float alpha;           // 低通滤波系数（0~1），值越小滤波效果越强

    // 运行状态
    float speed; // 当前转速
} DCMotorInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 DC 电机实例
 * @param name         实例名称
 * @param pwm_idx      板载 PWM TIM 枚举（TIM_PWM_1~4）
 * @param encoder_idx  板载编码器 TIM 枚举（TIM_ENCODER_1~4）
 * @param in1_idx      方向控制 IN1 GPIO 枚举
 * @param in2_idx      方向控制 IN2 GPIO 枚举
 * @param ppr          编码器线数（每转脉冲数）
 * @param ratio        减速比（输出轴/电机轴）
 * @param lpf_alpha    低通滤波系数（0~1），建议 0.1~0.5
 *
 * @example
 *   DCMOTOR_INSTANCE_DEF(motor1, TIM_PWM_1, TIM_ENCODER_1,
 *                        GPIO_MOTOR_1_IN1, GPIO_MOTOR_1_IN2,
 *                        512, 30.0f, 0.3f);
 */
// clang-format off
#define DCMOTOR_INSTANCE_DEF(name, pwm_idx, encoder_idx, in1_idx, in2_idx, ppr, ratio, lpf_alpha) \
    DCMotorInstance name = {                                                                      \
        .pwm_inst = {                                                                             \
            .tim_e = pwm_idx,                                                                     \
            .map = {NULL, 0},                                                                     \
            .dutyratio = 0.0f},                                                                   \
        .encoder_inst = {                                                                         \
            .tim_e = encoder_idx,                                                                 \
            .map = {NULL, 0},                                                                     \
            .arr = 0,                                                                             \
            .overflow_count = 0,                                                                  \
            .total_count = 0,                                                                     \
            .last_total_count = 0,                                                                \
            .speed = 0.0f,                                                                        \
            .last_time = 0.0f},                                                                   \
        .in1_inst = {                                                                             \
            .parent = NULL,                                                                       \
            .gpio_e = in1_idx,                                                                    \
            .map = {0},                                                                           \
            .pin_state = GPIO_PIN_RESET,                                                          \
            .callback = NULL},                                                                    \
        .in2_inst = {                                                                             \
            .parent = NULL,                                                                       \
            .gpio_e = in2_idx,                                                                    \
            .map = {0},                                                                           \
            .pin_state = GPIO_PIN_RESET,                                                          \
            .callback = NULL},                                                                    \
        .encoder_ppr = ppr,                                                                       \
        .reduction_ratio = ratio,                                                                 \
        .alpha = lpf_alpha,                                                                       \
        .speed = 0.0f}
// clang-format on

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 DC 电机实例
 * @param instance DC 电机实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 注册时会同时注册 PWM、编码器、GPIO 实例
 */
int8_t DCMotorRegister(DCMotorInstance *instance);

/**
 * @brief 设置 PWM 占空比（自动控制方向）
 * @param instance   DC 电机实例指针
 * @param dutyratio  占空比（-1~1），负值反转，正值正转，0 停止
 */
void DCMotorSetDutyRatio(DCMotorInstance *instance, float dutyratio);

/**
 * @brief 获取电机转速（带低通滤波）
 * @param instance DC 电机实例指针
 * @return 转速，正值为正转，负值为反转
 * @note 从编码器读取脉冲速度并转换为 rad/s，需在任务中周期性调用
 *       滤波公式：speed = alpha * speed_raw + (1 - alpha) * speed_last
 */
float DCMotorGetSpeed(DCMotorInstance *instance);

/**
 * @brief 清零编码器计数
 * @param instance DC 电机实例指针
 */
void DCMotorClearEncoder(DCMotorInstance *instance);

#endif /* __DRV_DCMOTOR_H */