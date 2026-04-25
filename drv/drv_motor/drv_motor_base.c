/**
 * @file drv_motor_base.c
 * @brief 电机抽象基类实现
 *
 * @note DRV 层职责：
 *       1. 提供基类初始化函数
 *       2. 提供串级 PID 计算框架
 *       3. 不使用 FreeRTOS
 */

#include "drv_motor_base.h"
#include <stddef.h>

/*============================================
 *              基类统一接口实现
 *============================================*/

/**
 * @brief 初始化电机基类
 * @param inst   电机实例指针
 * @param config 初始化配置
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t MotorBaseInit(MotorInstance *inst, const MotorInitConfig_t *config)
{
    if (!inst || !config)
        return -1;

    // 初始化基本属性
    inst->type = config->type;
    // vtable 由具体电机类型设置，不在此处清零
    inst->impl = NULL;
    inst->dt = 0.0f;

    // 初始化状态
    inst->status.angle = 0.0f;
    inst->status.speed = 0.0f;
    inst->status.current = 0.0f;
    inst->status.temperature = 0.0f;
    inst->status.enable = 0;
    inst->status.online = 0;

    // 初始化控制设置
    inst->settings.outer_loop_type = config->outer_loop_type;
    inst->settings.close_loop_type = config->close_loop_type;
    inst->settings.motor_reverse = config->motor_reverse;
    inst->settings.feedback_reverse = config->feedback_reverse;
    inst->settings.angle_feedback_src = MOTOR_FEED;
    inst->settings.speed_feedback_src = MOTOR_FEED;
    inst->settings.feedforward_flag = FEEDFORWARD_NONE;

    // 初始化控制器反馈指针
    inst->controller.angle_feedback_ptr = config->angle_feedback_ptr;
    inst->controller.speed_feedback_ptr = config->speed_feedback_ptr;
    inst->controller.speed_ff_ptr = config->speed_ff_ptr;
    inst->controller.current_ff_ptr = config->current_ff_ptr;
    inst->controller.pid_ref = 0.0f;

    // 初始化三环 PID
    PIDInit(&inst->controller.current_pid, config->current_kp, config->current_ki, config->current_kd);
    PIDInit(&inst->controller.speed_pid, config->speed_kp, config->speed_ki, config->speed_kd);
    PIDInit(&inst->controller.angle_pid, config->angle_kp, config->angle_ki, config->angle_kd);

    return 0;
}

/**
 * @brief 设置 PID 参数
 * @param inst           电机实例指针
 * @param loop           闭环类型
 * @param kp             比例系数
 * @param ki             积分系数
 * @param kd             微分系数
 * @param integral_limit 积分限幅
 * @param max_out        最大输出
 * @retval 0 成功
 * @retval -1 失败
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

    PIDInit(pid, kp, ki, kd);
    PIDSetIntegralLimit(pid, integral_limit, integral_limit > 0 ? 1 : 0);

    return 0;
}

/**
 * @brief 串级 PID 计算
 * @param inst 电机实例指针
 * @param dt   时间间隔 (秒)
 * @return 控制输出
 *
 * @note 计算顺序：位置环 → 速度环 → 电流环
 *       pid_ref 会顺次通过各环作为数据载体
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
        // 速度前馈
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
        // 电流前馈
        if ((settings->feedforward_flag & CURRENT_FEEDFORWARD) && controller->current_ff_ptr)
            pid_ref += *controller->current_ff_ptr;

        pid_ref = PIDCalculate(&controller->current_pid, pid_ref, status->current, dt, 0.0f);
    }

    // 反馈反转处理
    if (settings->feedback_reverse)
        pid_ref *= -1.0f;

    return pid_ref;
}
