/**
 * @file drv_acmotor_base.c
 * @brief 电机抽象基类实现
 *
 * @note DRV 层职责：
 *       1. 提供串级 PID 计算框架
 *       2. 不使用 FreeRTOS
 */

#include "drv_acmotor_base.h"
#include <stddef.h>

/*============================================
 *              基类统一接口实现
 *============================================*/

/**
 * @brief 设置 PID 参数
 */
int8_t MotorSetPID(MotorInstance *inst, MotorLoopType_e loop,
                   float kp, float ki, float kd, float integral_limit, float max_out)
{
    if (!inst)
        return -1;

    PIDInstance *pid = NULL;

    switch (loop)
    {
    case MOTOR_LOOP_CURRENT:
        pid = &inst->controller.current_pid;
        break;
    case MOTOR_LOOP_SPEED:
        pid = &inst->controller.speed_pid;
        break;
    case MOTOR_LOOP_ANGLE:
        pid = &inst->controller.angle_pid;
        break;
    default:
        return -1;
    }

    PID_Init_Config_s pid_cfg = {
        .kp = kp,
        .ki = ki,
        .kd = kd,
        .integral_limit = integral_limit,
        .config_mask = integral_limit > 0 ? PID_ENABLE_INTEGRAL_LIMIT : PID_IMPROVE_NONE,
    };
    PIDInit(pid, &pid_cfg);

    return 0;
}

/**
 * @brief 串级 PID 计算
 * @param inst 电机实例指针
 * @param dt   时间间隔 (秒)
 * @return 控制输出
 *
 * @note 计算顺序：位置环 → 速度环 → 电流环
 */
float MotorCascadePID(MotorInstance *inst, float dt)
{
    if (!inst || !inst->status.enable)
        return 0.0f;

    MotorControlSetting_t *settings = &inst->settings;
    MotorController_t *controller = &inst->controller;
    MotorStatus_t *status = &inst->status;

    float pid_ref = controller->pid_ref;

    // 开环模式：直接返回参考值
    if (settings->close_loop_type == MOTOR_LOOP_OPEN)
    {
        if (settings->motor_reverse)
            pid_ref *= -1.0f;
        return pid_ref;
    }

    // 电机反转处理
    if (settings->motor_reverse)
        pid_ref *= -1.0f;

    // 位置环计算
    if ((settings->close_loop_type & MOTOR_LOOP_ANGLE) &&
        settings->outer_loop_type == MOTOR_LOOP_ANGLE)
    {
        float angle_measure;
        if (settings->angle_feedback_src == OTHER_FEED && controller->angle_feedback_ptr)
            angle_measure = *controller->angle_feedback_ptr;
        else
            angle_measure = status->angle;

        pid_ref = PIDCalculate(&controller->angle_pid, pid_ref, angle_measure, dt, 0.0f);
    }

    // 速度环计算
    if ((settings->close_loop_type & MOTOR_LOOP_SPEED) &&
        (settings->outer_loop_type & (MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED)))
    {
        if ((settings->feedforward_flag & SPEED_FEEDFORWARD) && controller->speed_ff_ptr)
            pid_ref += *controller->speed_ff_ptr;

        float speed_measure;
        if (settings->speed_feedback_src == OTHER_FEED && controller->speed_feedback_ptr)
            speed_measure = *controller->speed_feedback_ptr;
        else
            speed_measure = status->speed;

        pid_ref = PIDCalculate(&controller->speed_pid, pid_ref, speed_measure, dt, 0.0f);
    }

    // 电流环计算
    if (settings->close_loop_type & MOTOR_LOOP_CURRENT)
    {
        if ((settings->feedforward_flag & CURRENT_FEEDFORWARD) && controller->current_ff_ptr)
            pid_ref += *controller->current_ff_ptr;

        pid_ref = PIDCalculate(&controller->current_pid, pid_ref, status->current, dt, 0.0f);
    }

    // 反馈反转处理
    if (settings->feedback_reverse)
        pid_ref *= -1.0f;

    return pid_ref;
}