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

// 只有当 TIM 和 GPIO 模块都使能时才编译直流电机驱动
#if defined(BSP_TIM_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED)

#include "bsp_tim.h"
#include "bsp_gpio.h"
#include "drv_motor_base.h"
#include "drv_pid.h"
#include "bsp_math.h"
#include "stdint.h"

/*============================================
 *              类型定义
 *============================================*/

/**
 * @brief DC 电机实例结构体
 * @note 内嵌 BSP 层实例，直接管理 PWM、编码器、方向控制 GPIO
 *       继承 MotorInstance 基类，支持多态调用
 *
 * @note 用户定义示例：
 *       DCMotorInstance motor_1 = {
 *           .base = { .type = MOTOR_TYPE_DC },
 *           .pwm_inst.tim_e = TIM_PWM_1,
 *           .encoder_inst.tim_e = TIM_ENCODER_1,
 *           .in1_inst.gpio_e = GPIO_MOTOR_IN1,
 *           .in2_inst.gpio_e = GPIO_MOTOR_IN2,
 *           .encoder_ppr = 1000,
 *           .reduction_ratio = 30.0f,
 *           .alpha = 0.3f,
 *           .pid = { .kp = 1.5f, .ki = 0.1f, .kd = 0.0f, .integral_limit = 1000.0f },
 *           .max_speed = 20.0f,
 *           // ... 前馈参数等
 *       };
 *       MotorInit(&motor_1.base);
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

/*============================================
 *              虚函数表声明
 *============================================*/

/**
 * @brief DC 电机虚函数表
 * @note 用于多态调用，在 drv_dcmotor.c 中定义
 */
extern const MotorInterface_s g_dc_motor_interface;

/*============================================
 *              外部接口声明
 *============================================*/

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

#endif // defined(BSP_TIM_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED)

#endif /* __DRV_DCMOTOR_H */
