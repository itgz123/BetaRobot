/**
 * @file drv_axis_mit_lite.c
 * @brief 轻量级单轴关节控制驱动模块实现
 * @author TRW
 * @date 2026-06-07
 */

#include "drv_axis_mit_lite.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include "drv_vofa_lite.h"
#include <math.h>

int8_t AxisMitLiteInit(AxisMitLiteInstance *inst, AxisMitLite_Init_Config_s *cfg)
{
    if (inst == NULL || cfg == NULL)
    {
        return -1;
    }

    memset(inst, 0, sizeof(AxisMitLiteInstance));
    inst->motor = cfg->motor;
    inst->stage = cfg->stage;
    inst->delay_ms = cfg->delay_ms;
    inst->params = cfg->params;

    // 初始化扫频参数
    inst->chirp_params = cfg->chirp_params;

    // 初始化正弦参数
    inst->sine_params = cfg->sine_params;

    // 初始化MIT控制器
    MIT_Init_Config_s mit_cfg = {
        .kp = cfg->kp,
        .kd = cfg->kd,
        .out_max = FLT_MAX,
        .out_min = -FLT_MAX,
    };
    MITInit(&inst->mit, &mit_cfg);

    return 0;
}

/**
 * @brief 计算经过的时间
 * @param inst 实例指针
 * @param current_time_us 当前时间戳 (us)
 * @param delay_us 延时时间 (us)
 * @return 相对于延时结束后的时间 (s)，延时中返回负值
 */
static float CalcElapsedTime(AxisMitLiteInstance *inst, uint64_t current_time_us, uint64_t delay_us)
{
    if (inst->init_time_us == 0)
    {
        inst->init_time_us = current_time_us;
    }

    uint64_t elapsed_us = current_time_us - inst->init_time_us;
    return (float)(elapsed_us - delay_us) * 1e-6f;
}

/**
 * @brief 安全获取外部设定值，NAN时返回默认值
 */
static inline float SafeGetRef(float *ptr, float default_val)
{
    float val = (ptr != NULL) ? *ptr : default_val;
    return isfinite(val) ? val : default_val;
}

void AxisMitLiteCalculate(AxisMitLiteInstance *inst)
{
    if (inst == NULL || inst->motor == NULL)
    {
        return;
    }

    // 获取参数
    float torque_coeff = inst->params.torque_coeff;

    // 计算重力前馈
    float angle = MotorGetAngle(inst->motor); // rad
    if (!isfinite(angle))
    {
        angle = 0.0f;
    }
    inst->params.gravity_ff = inst->params.gravity * BSP_Math_Cos(angle); // Nm

    // 获取速度反馈
    float speed_measure = MotorGetSpeed(inst->motor); // rad/s
    if (!isfinite(speed_measure))
    {
        speed_measure = 0.0f;
    }

    uint64_t current_time_us = DWT_GetTimeUs();
    uint64_t delay_us = (uint64_t)inst->delay_ms * 1000ULL;
    MITInstance *p_mit = &(inst->mit);

    float ref_pos = 0.0f;
    float ref_vel = 0.0f;
    float output = 0.0f;

    switch (inst->stage)
    {
    case AXIS_LITE_STAGE_FIXED_TORQUE:
    {
        // 固定力矩模式：只给重力前馈
        inst->params.inertia_ff = 0.0f;
        inst->params.friction_ff = 0.0f;
        inst->params.total_ff = inst->params.gravity_ff;

        MotorSetRef(inst->motor, torque_coeff * inst->params.total_ff);
        break;
    }

    case AXIS_LITE_STAGE_IDENTIFY:
    {
        // 辨识模式：重力前馈 + 扫频力矩
        float t = CalcElapsedTime(inst, current_time_us, delay_us); // s
        if (t < 0.0f)
        {
            inst->params.inertia_ff = 0.0f;
            inst->params.friction_ff = 0.0f;
            inst->params.total_ff = inst->params.gravity_ff;

            MotorSetRef(inst->motor, torque_coeff * inst->params.total_ff);
            break;
        }

        if (t > inst->chirp_params.duration)
        {
            t = 0.0f;
        }

        // 线性扫频相位: phi(t) = 2*pi * (f0*t + k*t²/2)
        float f0 = inst->chirp_params.start_freq;    // Hz
        float f1 = inst->chirp_params.end_freq;      // Hz
        float T = inst->chirp_params.duration;       // s
        float k = (T > 0.0f) ? (f1 - f0) / T : 0.0f; // Hz/s，防止除零

        float phase = M_2PI * (f0 * t + 0.5f * k * t * t);                       // rad
        float chirp_torque = inst->chirp_params.amplitude * BSP_Math_Sin(phase); // Nm

        inst->params.inertia_ff = 0.0f;
        inst->params.friction_ff = chirp_torque; // 扫频力矩作为摩擦调试输出
        inst->params.total_ff = inst->params.gravity_ff + inst->params.friction_ff;

        MotorSetRef(inst->motor, torque_coeff * inst->params.total_ff);
        break;
    }

    case AXIS_LITE_STAGE_TUNE:
    {
        // 调参模式：正弦波位置设定值
        float t = CalcElapsedTime(inst, current_time_us, delay_us); // s
        if (t < 0.0f)
        {
            inst->params.inertia_ff = 0.0f;
            inst->params.friction_ff = 0.0f;
            inst->params.total_ff = inst->params.gravity_ff;

            MotorSetRef(inst->motor, torque_coeff * inst->params.total_ff);
            break;
        }

        float A = inst->sine_params.amplitude;    // rad
        float w = M_2PI * inst->sine_params.freq; // rad/s

        float sin_phase = BSP_Math_Sin(w * t);
        float cos_phase = BSP_Math_Cos(w * t);

        // 设定值
        ref_pos = A * sin_phase;                // rad
        ref_vel = A * w * cos_phase;            // rad/s
        float ref_acc = -A * w * w * sin_phase; // rad/s²

        // 计算各项前馈
        inst->params.inertia_ff = inst->params.inertia * ref_acc;
        inst->params.friction_ff = inst->params.friction_viscous * speed_measure + inst->params.friction_coulomb * MATH_SIGN(speed_measure);
        inst->params.total_ff = inst->params.gravity_ff + inst->params.inertia_ff + inst->params.friction_ff;
        if (!isfinite(inst->params.total_ff))
        {
            inst->params.total_ff = inst->params.gravity_ff;
        }

        output = MITCalculate(p_mit, ref_vel, speed_measure, ref_pos, angle, inst->params.total_ff); // Nm
        if (!isfinite(output))
        {
            output = inst->params.gravity_ff;
        }
        MotorSetRef(inst->motor, torque_coeff * output);
        break;
    }

    case AXIS_LITE_STAGE_NORMAL:
    {
        // 正常控制模式：使用外部设定值
        ref_pos = SafeGetRef(inst->ref_position, angle);          // rad，无设定时保持当前位置
        ref_vel = SafeGetRef(inst->ref_speed, 0.0f);              // rad/s
        float ref_acc = SafeGetRef(inst->ref_acceleration, 0.0f); // rad/s²

        // 计算各项前馈
        inst->params.inertia_ff = inst->params.inertia * ref_acc;
        inst->params.friction_ff = inst->params.friction_viscous * speed_measure + inst->params.friction_coulomb * MATH_SIGN(speed_measure);
        inst->params.total_ff = inst->params.gravity_ff + inst->params.inertia_ff + inst->params.friction_ff;
        if (!isfinite(inst->params.total_ff))
        {
            inst->params.total_ff = inst->params.gravity_ff;
        }

        output = MITCalculate(p_mit, ref_vel, speed_measure, ref_pos, angle, inst->params.total_ff); // Nm
        if (!isfinite(output))
        {
            output = inst->params.gravity_ff;
        }
        MotorSetRef(inst->motor, torque_coeff * output);
        break;
    }

    default:
        // 调用 MIT 计算（使状态有效）
        output = MITCalculate(p_mit, 0.0f, speed_measure, angle, angle, inst->params.gravity_ff);
        break;
    }

#ifdef AxisMitVofaLiteSetChannelUsed
    // 设定 VOFA 调试通道（13个）
    VofaLiteSetChannel(1, angle);
    VofaLiteSetChannel(2, speed_measure);
    VofaLiteSetChannel(3, ref_pos);
    VofaLiteSetChannel(4, ref_vel);
    VofaLiteSetChannel(5, inst->params.gravity_ff);
    VofaLiteSetChannel(6, inst->params.inertia_ff);
    VofaLiteSetChannel(7, inst->params.friction_ff);
    VofaLiteSetChannel(8, inst->params.total_ff);
    VofaLiteSetChannel(9, inst->mit.pos_error);
    VofaLiteSetChannel(10, inst->mit.speed_error);
    VofaLiteSetChannel(11, inst->mit.pos_output);
    VofaLiteSetChannel(12, inst->mit.speed_output);
    VofaLiteSetChannel(13, inst->mit.output);
#endif
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED
