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
#include "drv_pid.h"
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

    // PID 控制器
    PIDInstance pid;

    // 前馈模型参数 (由用户根据电机特性配置)
    // 建议使用下边界直线参数，前馈略大，PID只需负向修正
    float feedforward_k;      // 前馈系数 (duty/speed)，约 0.00655
    float feedforward_offset; // 启动偏移，下边界约 0.25

    // 速度控制参数
    float max_speed;    // 最大转速 (rad/s)，用于 PID 归一化
    float target_speed; // 目标速度 (rad/s)
} DCMotorInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 DC 电机实例（仅设置外设实例）
 * @param name         实例名称
 * @param pwm_idx      板载 PWM TIM 枚举（TIM_PWM_1~4）
 * @param encoder_idx  板载编码器 TIM 枚举（TIM_ENCODER_1~4）
 * @param in1_idx      方向控制 IN1 GPIO 枚举
 * @param in2_idx      方向控制 IN2 GPIO 枚举
 *
 * @note 使用流程：
 *       1. DCMOTOR_INSTANCE_DEF() 定义实例
 *       2. DCMotorRegister() 注册外设实例
 *       3. DCMotorInit() 设置基本电机参数
 *       4. DCMotorSetPID() 设置PID和前馈参数（如需速度控制）
 *
 * @example
 *   DCMOTOR_INSTANCE_DEF(motor1, TIM_PWM_1, TIM_ENCODER_1,
 *                        GPIO_MOTOR_1_IN1, GPIO_MOTOR_1_IN2);
 */
// clang-format off
#define DCMOTOR_INSTANCE_DEF(name, pwm_idx, encoder_idx, in1_idx, in2_idx) \
    DCMotorInstance name = {                                               \
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
        .pid = {0},                                                        \
        .feedforward_k = 0.0f,                                             \
        .feedforward_offset = 0.0f,                                        \
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
 *
 * @note 必须在 DCMotorRegister 后调用
 */
int8_t DCMotorInit(DCMotorInstance *instance, uint16_t encoder_ppr, float reduction_ratio, float lpf_alpha);

/**
 * @brief 设置 PID 和前馈参数
 * @param instance        DC 电机实例指针
 * @param kp              比例系数
 * @param ki              积分系数
 * @param kd              微分系数
 * @param integral_limit  积分限幅（建议设为 1/ki）
 * @param feedforward_k   前馈系数 (duty/speed)
 * @param feedforward_offset 启动偏移
 * @param max_speed       最大转速 (rad/s)，用于 PID 归一化
 *
 * @note 调用此函数后才能使用 DCMotorSetSpeed 进行速度控制
 */
void DCMotorSetPID(DCMotorInstance *instance, float kp, float ki, float kd, float integral_limit, float feedforward_k, float feedforward_offset, float max_speed);

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

/**
 * @brief 设置电机转速（PID + 前馈控制）
 * @param instance     DC 电机实例指针
 * @param target_speed 目标转速，正值正转，负值反转
 * @param dt           时间间隔 (秒)，由 APP 层通过 DWT 获取后传入
 * @note 需在任务中周期性调用，使用前需配置 pid 参数 (kp, ki, kd, integral_limit)
 *       前馈公式：feedforward = feedforward_k * |target_speed| + feedforward_offset * sign(target_speed)
 */
void DCMotorSetSpeed(DCMotorInstance *instance, float target_speed, float dt);

#endif /* __DRV_DCMOTOR_H */