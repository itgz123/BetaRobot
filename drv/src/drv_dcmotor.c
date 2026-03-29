/**
 * @file drv_dcmotor.c
 * @brief DC 电机驱动实现
 *
 * @note DRV 层职责：
 *       1. 封装 PWM 占空比设置和方向控制
 *       2. 封装编码器速度计算
 *       3. 不使用 FreeRTOS（队列由 APP 层管理）
 */

#include "drv_dcmotor.h"
#include "bsp_log.h"
#include "stddef.h"

/*------------- 外部接口实现 --------------*/

int8_t DCMotorRegister(DCMotorInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[drv_dcmotor] Instance is NULL!");
        return -1;
    }

    // 检查 PWM 枚举范围
    if (instance->pwm_inst.tim_e >= TIM_NUM_MAX)
    {
        LOGERROR("[drv_dcmotor] Invalid PWM tim_e: %d", instance->pwm_inst.tim_e);
        return -1;
    }

    // 检查编码器枚举范围
    if (instance->encoder_inst.tim_e >= TIM_NUM_MAX)
    {
        LOGERROR("[drv_dcmotor] Invalid Encoder tim_e: %d", instance->encoder_inst.tim_e);
        return -1;
    }

    // 检查编码器参数
    if (instance->encoder_ppr == 0)
    {
        LOGERROR("[drv_dcmotor] encoder_ppr cannot be 0!");
        return -1;
    }

    if (instance->reduction_ratio <= 0.0f)
    {
        LOGERROR("[drv_dcmotor] reduction_ratio must be positive!");
        return -1;
    }

    // 注册 GPIO 实例
    if (GPIORegister(&instance->in1_inst) != 0)
    {
        LOGERROR("[drv_dcmotor] IN1 GPIO register failed!");
        return -1;
    }

    if (GPIORegister(&instance->in2_inst) != 0)
    {
        LOGERROR("[drv_dcmotor] IN2 GPIO register failed!");
        return -1;
    }

    // 注册 PWM 实例
    if (PWMRegister(&instance->pwm_inst) != 0)
    {
        LOGERROR("[drv_dcmotor] PWM register failed!");
        return -1;
    }

    // 注册编码器实例
    if (EncoderRegister(&instance->encoder_inst) != 0)
    {
        LOGERROR("[drv_dcmotor] Encoder register failed!");
        return -1;
    }

    // 初始化方向为停止
    instance->direction = DCMOTOR_STOP;
    GPIOReset(&instance->in1_inst);
    GPIOReset(&instance->in2_inst);

    LOGINFO("[drv_dcmotor] DC Motor instance registered");
    return 0;
}

void DCMotorSetDutyRatio(DCMotorInstance *instance, float dutyratio)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    PWMSetDutyRatio(&instance->pwm_inst, dutyratio);
}

void DCMotorSetDirection(DCMotorInstance *instance, DCMotorDirection_e direction)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    instance->direction = direction;

    switch (direction)
    {
    case DCMOTOR_FORWARD:
        // IN1=1, IN2=0 正转
        GPIOSet(&instance->in1_inst);
        GPIOReset(&instance->in2_inst);
        break;

    case DCMOTOR_BACKWARD:
        // IN1=0, IN2=1 反转
        GPIOReset(&instance->in1_inst);
        GPIOSet(&instance->in2_inst);
        break;

    case DCMOTOR_STOP:
    default:
        // IN1=0, IN2=0 滑行停止
        GPIOReset(&instance->in1_inst);
        GPIOReset(&instance->in2_inst);
        break;
    }
}

float DCMotorGetSpeed(DCMotorInstance *instance, float alpha)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 钳位滤波系数到有效范围
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;

    // 获取编码器速度（脉冲/秒）
    // EncoderGetSpeed 返回带符号的速度，正值为正转，负值为反转
    float pulse_per_sec = EncoderGetSpeed(&instance->encoder_inst);

    // 计算电机轴转速（rad/s）
    // 公式：speed = (pulse_per_sec / PPR) * 2π / reduction_ratio
    // 注意：STM32 编码器模式为 4 倍频（A/B 两相上升沿和下降沿都计数）
    // 所以实际脉冲数 = 编码器线数 * 4
    float motor_rps = pulse_per_sec / ((float)instance->encoder_ppr * 4.0f);
    float motor_speed_rad_s = motor_rps * DCMOTOR_2PI;

    // 输出轴转速（考虑减速比）
    float speed_raw = motor_speed_rad_s / instance->reduction_ratio;

    // 一阶低通滤波：speed = alpha * speed_raw + (1 - alpha) * speed_last
    instance->speed = alpha * speed_raw + (1.0f - alpha) * instance->speed;

    return instance->speed;
}

void DCMotorClearEncoder(DCMotorInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    EncoderClearCount(&instance->encoder_inst);
}
