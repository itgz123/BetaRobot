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
#include "bsp_dwt.h"

/*============================================
 *              私有函数声明
 *============================================*/

// 虚函数实现（静态，用于填充 vtable）
static void DCMotor_Enable(MotorInstance *inst);
static void DCMotor_Stop(MotorInstance *inst);
static void DCMotor_SetRef(MotorInstance *inst, float ref);
static void DCMotor_SetSpeed_VTable(MotorInstance *inst, float speed);
static void DCMotor_GetStatus(MotorInstance *inst, MotorStatus_t *status);

/*============================================
 *              虚函数表定义
 *============================================*/

const MotorInterface_s g_dc_motor_interface = {
    .enable = DCMotor_Enable,
    .stop = DCMotor_Stop,
    .set_ref = DCMotor_SetRef,
    .set_speed = DCMotor_SetSpeed_VTable,
    .set_outer_loop = NULL, // DC 电机固定速度环
    .get_status = DCMotor_GetStatus,
};

/*============================================
 *              虚函数实现
 *============================================*/

static void DCMotor_Enable(MotorInstance *inst)
{
    // DC 电机不需要显式使能，直接返回
    (void)inst;
}

static void DCMotor_Stop(MotorInstance *inst)
{
    if (inst == NULL)
    {
        return;
    }
    DCMotorInstance *dc_inst = (DCMotorInstance *)inst;
    DCMotorSetDutyRatio(dc_inst, 0.0f);
}

static void DCMotor_SetRef(MotorInstance *inst, float ref)
{
    // DC 电机的 set_ref 等同于 set_speed
    DCMotor_SetSpeed_VTable(inst, ref);
}

static void DCMotor_SetSpeed_VTable(MotorInstance *inst, float speed)
{
    if (inst == NULL)
    {
        return;
    }
    DCMotorInstance *dc_inst = (DCMotorInstance *)inst;
    DCMotorSetSpeed(dc_inst, speed);
}

static void DCMotor_GetStatus(MotorInstance *inst, MotorStatus_t *status)
{
    if (inst == NULL || status == NULL)
    {
        return;
    }
    DCMotorInstance *dc_inst = (DCMotorInstance *)inst;
    status->speed = dc_inst->speed;
    status->angle = 0.0f;       // DC 电机无角度反馈
    status->current = 0.0f;     // DC 电机无电流反馈
    status->temperature = 0.0f; // DC 电机无温度反馈
    status->online = 1;         // 假设始终在线
}

/*============================================
 *              外部接口实现
 *============================================*/

int8_t DCMotorInit(DCMotorInstance *instance, uint16_t encoder_ppr, float reduction_ratio, float lpf_alpha)
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
    if (encoder_ppr == 0)
    {
        LOGERROR("[drv_dcmotor] encoder_ppr cannot be 0!");
        return -1;
    }

    if (reduction_ratio <= 0.0f)
    {
        LOGERROR("[drv_dcmotor] reduction_ratio must be positive!");
        return -1;
    }

    if (lpf_alpha < 0.0f || lpf_alpha > 1.0f)
    {
        LOGWARNING("[drv_dcmotor] lpf_alpha should be in [0, 1], using 0.5");
        lpf_alpha = 0.5f;
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
    GPIOReset(&instance->in1_inst);
    GPIOReset(&instance->in2_inst);

    // 设置基本参数
    instance->encoder_ppr = encoder_ppr;
    instance->reduction_ratio = reduction_ratio;
    instance->alpha = lpf_alpha;

    // 初始化速度状态
    instance->speed = 0.0f;
    instance->target_speed = 0.0f;
    instance->last_time = 0;

    // 初始化 PID 控制器（参数为 0，由 DCMotorSetPID 配置）
    PIDInit(&instance->pid, 0.0f, 0.0f, 0.0f);

    // 设置虚函数表指针（已在宏中设置，此处确保）
    instance->base.vtable = &g_dc_motor_interface;
    instance->base.type = MOTOR_TYPE_DC;
    instance->base.impl = instance;

    LOGINFO("[drv_dcmotor] DC Motor initialized");
    return 0;
}

void DCMotorSetPID(DCMotorInstance *instance, float kp, float ki, float kd, float integral_limit, float max_speed, float ff_k_low, float ff_offset_low, float ff_k_high, float ff_offset_high, float ff_split_speed)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    // 设置 PID 基本参数
    instance->pid.kp = kp;
    instance->pid.ki = ki;
    instance->pid.kd = kd;

    // 设置积分限幅（integral_limit > 0 时启用）
    PIDSetIntegralLimit(&instance->pid, integral_limit, integral_limit > 0.0001f ? 1 : 0);

    // 设置两段前馈参数
    instance->ff_k_low = ff_k_low;
    instance->ff_offset_low = ff_offset_low;
    instance->ff_k_high = ff_k_high;
    instance->ff_offset_high = ff_offset_high;
    instance->ff_split_speed = ff_split_speed;

    // 设置最大速度
    instance->max_speed = max_speed;

    LOGINFO("[drv_dcmotor] PID configured");
}

void DCMotorSetDutyRatio(DCMotorInstance *instance, float dutyratio)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    // 根据正负自动设置方向
    if (dutyratio > 0.0f)
    {
        // 正转：IN1=1, IN2=0
        GPIOSet(&instance->in1_inst);
        GPIOReset(&instance->in2_inst);
        // 钳位占空比
        if (dutyratio > 1.0f)
            dutyratio = 1.0f;
    }
    else if (dutyratio < 0.0f)
    {
        // 反转：IN1=0, IN2=1
        GPIOReset(&instance->in1_inst);
        GPIOSet(&instance->in2_inst);
        // 钳位占空比（取绝对值）
        dutyratio = -dutyratio;
        if (dutyratio > 1.0f)
            dutyratio = 1.0f;
    }
    else
    {
        // 停止：IN1=0, IN2=0
        GPIOReset(&instance->in1_inst);
        GPIOReset(&instance->in2_inst);
    }

    PWMSetDutyRatio(&instance->pwm_inst, dutyratio);
}

float DCMotorGetSpeed(DCMotorInstance *instance)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 获取编码器速度（脉冲/秒）
    float pulse_per_sec = EncoderGetSpeed(&instance->encoder_inst);

    // 计算电机轴转速（rad/s）
    // STM32 编码器模式为 4 倍频
    float motor_rps = pulse_per_sec / ((float)instance->encoder_ppr * 4.0f);
    float motor_speed_rad_s = motor_rps * DCMOTOR_2PI;

    // 输出轴转速（考虑减速比）
    float speed_raw = motor_speed_rad_s / instance->reduction_ratio;

    // 一阶低通滤波
    instance->speed = instance->alpha * speed_raw + (1.0f - instance->alpha) * instance->speed;

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

void DCMotorSetSpeed(DCMotorInstance *instance, float target_speed)
{
    if (instance == NULL)
    {
        LOGWARNING("[drv_dcmotor] Instance is NULL!");
        return;
    }

    // 检查 max_speed 是否有效
    if (instance->max_speed <= 0.0f)
    {
        LOGWARNING("[drv_dcmotor] max_speed not configured!");
        return;
    }

    // 计算 dt
    uint64_t current_time = DWT_GetTimeline_us();
    float dt = 0.0f;
    if (instance->last_time > 0)
    {
        dt = (current_time - instance->last_time) / 1000000.0f; // us -> s
    }
    instance->last_time = current_time;

    // 防止 dt 为 0 或异常
    if (dt <= 0.0f || dt > 1.0f)
    {
        dt = 0.001f; // 默认 1ms
    }

    // 保存目标速度
    instance->target_speed = target_speed;

    // 获取当前速度并归一化
    float measure = DCMotorGetSpeed(instance);
    float measure_norm = measure / instance->max_speed;

    // 归一化目标速度
    float setpoint_norm = target_speed / instance->max_speed;

    // 计算两段前馈值
    float sign = (target_speed >= 0.0f) ? 1.0f : -1.0f;
    float abs_speed = fabsf(target_speed);
    float feedforward;

    if (abs_speed < instance->ff_split_speed)
    {
        feedforward = instance->ff_k_low * abs_speed + instance->ff_offset_low;
    }
    else
    {
        feedforward = instance->ff_k_high * abs_speed + instance->ff_offset_high;
    }
    feedforward *= sign;

    // 归一化前馈值
    float feedforward_norm = feedforward / instance->max_speed;

    // PID 计算
    float output = PIDCalculate(&instance->pid, setpoint_norm, measure_norm, dt, feedforward_norm);

    // 设置占空比
    DCMotorSetDutyRatio(instance, output);
}