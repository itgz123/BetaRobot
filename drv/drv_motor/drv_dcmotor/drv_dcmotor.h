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

#ifdef HAL_TIM_MODULE_ENABLED

#include "bsp_cfg.h"

// 只有当 PWM、编码器、GPIO 实例都存在时才编译直流电机驱动
#if (PWM_INSTANCE_NUM > 0) && (ENCODER_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)

#include "bsp_tim.h"
#include "bsp_gpio.h"
#include "drv_motor_base.h"
#include "drv_pid.h"
#include "bsp_math.h"
#include "stdint.h"

/*------------- 宏定义 --------------*/

/*------------- 类型定义 --------------*/

/**
 * @brief DC 电机实例结构体
 * @note 内嵌 BSP 层实例，直接管理 PWM、编码器、方向控制 GPIO
 *       继承 MotorInstance 基类，支持多态调用
 */
typedef struct DCMotorInstance
{
    // ===== 基类（必须放在首位）=====
    MotorInstance base; // 电机基类，支持多态

    // ===== BSP 层实例 =====
    PWMInstance pwm_inst;         // PWM 实例
    EncoderInstance encoder_inst; // 编码器实例
    GPIOInstance in1_inst;        // 方向控制 IN1
    GPIOInstance in2_inst;        // 方向控制 IN2

    // ===== 配置参数 =====
    uint16_t encoder_ppr;  // 编码器线数（每转脉冲数）
    float reduction_ratio; // 减速比（输出轴/电机轴，如 30 表示 1:30 减速）
    float alpha;           // 低通滤波系数（0~1），值越小滤波效果越强

    // ===== 运行状态 =====
    float speed;        // 当前转速
    uint64_t last_time; // 上次调用时间（用于计算 dt）

    // ===== PID 控制器 =====
    PIDInstance pid;

    // ===== 两段前馈参数 =====
    float ff_k_low;       // 低速段前馈系数 (duty/speed)
    float ff_offset_low;  // 低速段启动偏移
    float ff_k_high;      // 高速段前馈系数
    float ff_offset_high; // 高速段启动偏移
    float ff_split_speed; // 分界速度 (rad/s)

    // ===== 速度控制参数 =====
    float max_speed;    // 最大转速 (rad/s)，用于 PID 归一化
    float target_speed; // 目标速度 (rad/s)
} DCMotorInstance;

/*------------- 虚函数表声明 --------------*/

/**
 * @brief DC 电机虚函数表
 * @note 用于多态调用，在 drv_dcmotor.c 中定义
 */
extern const MotorInterface_s g_dc_motor_interface;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 DC 电机实例（仅设置外设实例）
 * @param name         实例名称
 * @param pwm_idx      板载 PWM TIM 枚举（TIM_PWM_1~4）
 * @param encoder_idx  板载编码器 TIM 枚举（TIM_ENCODER_1~4）
 * @param in1_idx      方向控制 IN1 GPIO 枚举
 * @param in2_idx      方向控制 IN2 GPIO 枚举
 */
// clang-format off
#define DCMOTOR_INSTANCE_DEF(name, pwm_idx, encoder_idx, in1_idx, in2_idx) \
    DCMotorInstance name = {                                               \
        .base = {                                                          \
            .vtable = &g_dc_motor_interface,                               \
            .impl = NULL,                                                  \
            .type = MOTOR_TYPE_DC,                                         \
            .status = {0}},                                                \
        .pwm_inst = {                                                      \
            .tim_e = pwm_idx,                                              \
            .map = {NULL, 0},                                              \
            .dutyratio = 0.0f},                                            \
        .encoder_inst = {                                                  \
            .tim_e = encoder_idx,                                          \
            .map = {NULL, 0},                                              \
            .arr = 0,                                                      \
            .overflow_count = 0,                                           \
            .total_count = 0,                                              \
            .last_total_count = 0,                                         \
            .speed = 0.0f,                                                 \
            .last_time = 0.0f},                                            \
        .in1_inst = {                                                      \
            .parent = NULL,                                                \
            .gpio_e = in1_idx,                                             \
            .map = {0},                                                    \
            .pin_state = GPIO_PIN_RESET,                                   \
            .callback = NULL},                                             \
        .in2_inst = {                                                      \
            .parent = NULL,                                                \
            .gpio_e = in2_idx,                                             \
            .map = {0},                                                    \
            .pin_state = GPIO_PIN_RESET,                                   \
            .callback = NULL},                                             \
        .encoder_ppr = 0,                                                  \
        .reduction_ratio = 0.0f,                                           \
        .alpha = 0.0f,                                                     \
        .speed = 0.0f,                                                     \
        .last_time = 0,                                                    \
        .pid = {0},                                                        \
        .ff_k_low = 0.0f,                                                  \
        .ff_offset_low = 0.0f,                                             \
        .ff_k_high = 0.0f,                                                 \
        .ff_offset_high = 0.0f,                                            \
        .ff_split_speed = 0.0f,                                            \
        .max_speed = 0.0f,                                                 \
        .target_speed = 0.0f}
// clang-format on

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化电机基本参数
 * @param instance       DC 电机实例指针
 * @param encoder_ppr    编码器线数（每转脉冲数）
 * @param reduction_ratio 减速比（输出轴/电机轴，如 30 表示 1:30 减速）
 * @param lpf_alpha      低通滤波系数（0~1），值越小滤波效果越强
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t DCMotorInit(DCMotorInstance *instance, uint16_t encoder_ppr, float reduction_ratio, float lpf_alpha);

/**
 * @brief 设置 PID 和两段前馈参数
 */
void DCMotorSetPID(DCMotorInstance *instance, float kp, float ki, float kd, float integral_limit, float max_speed, float ff_k_low, float ff_offset_low, float ff_k_high, float ff_offset_high, float ff_split_speed);

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
 */
float DCMotorGetSpeed(DCMotorInstance *instance);

/**
 * @brief 清零编码器计数
 */
void DCMotorClearEncoder(DCMotorInstance *instance);

/**
 * @brief 设置电机转速（PID + 两段前馈控制）
 * @param instance     DC 电机实例指针
 * @param target_speed 目标转速，正值正转，负值反转
 * @note dt 由内部使用 DWT 自动计算
 */
void DCMotorSetSpeed(DCMotorInstance *instance, float target_speed);

#endif // (PWM_INSTANCE_NUM > 0) && (ENCODER_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)

#endif /* HAL_TIM_MODULE_ENABLED */

#endif /* __DRV_DCMOTOR_H */