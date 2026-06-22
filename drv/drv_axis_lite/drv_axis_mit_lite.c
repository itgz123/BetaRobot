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

    // 初始化多正弦叠加参数
    inst->multi_sine_params = cfg->multi_sine_params;

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
 * @brief 计算延时结束后的时间
 * @param inst 实例指针
 * @param now_us 当前时间戳 (us)
 * @return 相对于延时结束后的时间 (s)
 * @note 仅当 init_flag 已置位、延时已通过后调用
 */
static inline float CalcTimeSinceDelay(AxisMitLiteInstance *inst, uint64_t now_us)
{
    uint64_t delay_us = (uint64_t)inst->delay_ms * 1000ULL;
    return (float)(now_us - inst->init_time_us - delay_us) * 1e-6f;
}

/**
 * @brief 安全获取外部设定值，NAN时返回默认值
 */
static inline float SafeGetRef(float *ptr, float default_val)
{
    float val = (ptr != NULL) ? *ptr : default_val;
    return isfinite(val) ? val : default_val;
}

/**
 * @brief 计算惯量前馈和摩擦前馈，累加到总前馈
 */
static inline void CalcFeedforward(AxisMitLiteInstance *inst, float ref_acc, float speed)
{
    inst->params.inertia_ff = inst->params.inertia * ref_acc;
    if (speed > 0.0f)
    {
        inst->params.friction_ff = inst->params.friction_viscous_pos * speed + inst->params.friction_coulomb_pos;
    }
    else if (speed < 0.0f)
    {
        inst->params.friction_ff = inst->params.friction_viscous_neg * speed - inst->params.friction_coulomb_neg;
    }
    else
    {
        inst->params.friction_ff = 0.0f;
    }
    inst->params.total_ff = inst->params.gravity_ff + inst->params.inertia_ff + inst->params.friction_ff;
    if (!isfinite(inst->params.total_ff))
    {
        inst->params.total_ff = inst->params.gravity_ff;
    }
}

/**
 * @brief 生成多正弦叠加力矩设定值
 * @param params 多正弦参数结构体指针
 * @param t 当前时间 (s)
 * @return 叠加力矩 (Nm)
 * @note 多正弦信号: τ(t) = Σ A_i * sin(2π * f_i * t)
 *       用于时域正交可分离最小二乘法辨识，频率需满足正交性条件
 */
static inline float GenerateMultiSineTorque(MultiSineParam_s *params, float t)
{
    float torque = 0.0f;
    float w, phase;

    for (uint8_t i = 0; i < params->num_freqs && i < AXIS_LITE_MULTI_SINE_MAX_FREQS; i++)
    {
        w = M_2PI * params->freqs[i]; // rad/s
        phase = w * t;                // rad
        torque += params->amplitudes[i] * BSP_Math_Sin(phase);
    }

    return torque;
}

void AxisMitLiteCalculate(AxisMitLiteInstance *inst)
{
    if (inst == NULL || inst->motor == NULL)
    {
        return;
    }

    // ======== 公共反馈与重力前馈 ========
    float angle = MotorGetAngle(inst->motor); // rad
    if (!isfinite(angle))
    {
        angle = 0.0f;
    }

    float speed = MotorGetSpeed(inst->motor); // rad/s
    if (!isfinite(speed))
    {
        speed = 0.0f;
    }

    inst->params.gravity_ff = inst->params.gravity * BSP_Math_Cos(angle); // Nm

    uint64_t now_us = DWT_GetTimeUs();

    // 初始化 total_ff 为仅重力
    inst->params.inertia_ff = 0.0f;
    inst->params.friction_ff = 0.0f;
    inst->params.total_ff = inst->params.gravity_ff;

    // ======== 延时处理（NORMAL 无延时）========
    float ref_pos = 0.0f;
    float ref_vel = 0.0f;
    float ref_acc = 0.0f;
    float output = 0.0f;
    float setref = 0.0f;

    if (inst->stage != AXIS_LITE_STAGE_NORMAL)
    {
        if (!inst->init_flag)
        {
            inst->init_time_us = now_us;
            inst->init_flag = 1;
        }

        uint64_t elapsed_us = now_us - inst->init_time_us;
        uint64_t delay_us = (uint64_t)inst->delay_ms * 1000ULL;

        if (elapsed_us < delay_us)
        {
            // 延时中：仅重力前馈
            MotorSetRef(inst->motor, inst->params.torque_coeff * inst->params.gravity_ff);
            goto vofa_output;
        }
    }

    // ======== 阶段控制 ========
    switch (inst->stage)
    {
    case AXIS_LITE_STAGE_FIXED_TORQUE:
        // 仅重力前馈
        output = inst->params.total_ff;
        break;

    case AXIS_LITE_STAGE_IDENTIFY:
    {
        float t = CalcTimeSinceDelay(inst, now_us); // s
        if (t > inst->chirp_params.duration)
        {
            t = 0.0f;
        }

        // 线性扫频相位: phi(t) = 2*pi * (f0*t + k*t²/2)
        float f0 = inst->chirp_params.start_freq;    // Hz
        float f1 = inst->chirp_params.end_freq;      // Hz
        float T = inst->chirp_params.duration;       // s
        float k = (T > 0.0f) ? (f1 - f0) / T : 0.0f; // Hz/s

        float phase = M_2PI * (f0 * t + 0.5f * k * t * t); // rad

        // 力矩幅值线性递增：A(t) = A_start + (A_end - A_start) * t/T
        // amplitude_end <= 0 时退化为恒幅，兼容旧配置
        float A_start = inst->chirp_params.amplitude_start;
        float A_end = inst->chirp_params.amplitude_end;
        float amp_t = (A_end > 0.0f) ? A_start + (A_end - A_start) * t / T : A_start;

        float chirp = amp_t * BSP_Math_Sin(phase); // Nm

        inst->params.friction_ff = chirp;
        inst->params.total_ff = inst->params.gravity_ff + chirp;
        output = inst->params.total_ff;
        break;
    }

    case AXIS_LITE_STAGE_IDENTIFY_OLS:
    {
        float t = CalcTimeSinceDelay(inst, now_us); // s
        float T = inst->multi_sine_params.duration; // s

        // 循环播放：t 超过时长时重置
        if (T > 0.0f && t > T)
        {
            t = fmodf(t, T);
        }

        // 生成多正弦叠加力矩
        float multi_sine = GenerateMultiSineTorque(&inst->multi_sine_params, t);

        inst->params.friction_ff = multi_sine;
        inst->params.total_ff = inst->params.gravity_ff + multi_sine;
        output = inst->params.total_ff;
        break;
    }

    case AXIS_LITE_STAGE_TUNE:
    {
        float t = CalcTimeSinceDelay(inst, now_us); // s
        float A = inst->sine_params.amplitude;      // rad
        float w = M_2PI * inst->sine_params.freq;   // rad/s

        ref_pos = A * BSP_Math_Sin(w * t);          // rad
        ref_vel = A * w * BSP_Math_Cos(w * t);      // rad/s
        ref_acc = -A * w * w * BSP_Math_Sin(w * t); // rad/s²

        CalcFeedforward(inst, ref_acc, speed);

        output = MITCalculate(&inst->mit, ref_vel, speed, ref_pos, angle, inst->params.total_ff);
        if (!isfinite(output))
        {
            output = inst->params.gravity_ff;
        }
        break;
    }

    case AXIS_LITE_STAGE_NORMAL:
    {
        ref_pos = SafeGetRef(inst->ref_position, angle);    // rad
        ref_vel = SafeGetRef(inst->ref_speed, 0.0f);        // rad/s
        ref_acc = SafeGetRef(inst->ref_acceleration, 0.0f); // rad/s²

        CalcFeedforward(inst, ref_acc, speed);

        output = MITCalculate(&inst->mit, ref_vel, speed, ref_pos, angle, inst->params.total_ff);
        if (!isfinite(output))
        {
            output = inst->params.gravity_ff;
        }
        break;
    }

    default:
        output = MITCalculate(&inst->mit, 0.0f, speed, angle, angle, inst->params.gravity_ff);
        break;
    }

    setref = inst->params.torque_coeff * output;
    MotorSetRef(inst->motor, setref);

vofa_output:
#ifdef AxisMitVofaLiteSetChannelUsed
    /* CH1-CH3: 测量值 */
    VofaLiteSetChannel(1, angle);                        // CH1: 反馈位置 (rad)
    VofaLiteSetChannel(2, speed);                        // CH2: 反馈速度 (rad/s)
    VofaLiteSetChannel(3, MotorGetCurrent(inst->motor)); // CH3: 电机实际电流
    /* CH4-CH6: 设定值 */
    VofaLiteSetChannel(4, ref_pos); // CH4: 位置设定值 (rad)
    VofaLiteSetChannel(5, ref_vel); // CH5: 速度设定值 (rad/s)
    VofaLiteSetChannel(6, ref_acc); // CH6: 加速度设定值 (rad/s^2)
    /* CH7-CH9: 前馈分量 */
    VofaLiteSetChannel(7, inst->params.gravity_ff);  // CH7: 重力前馈 (Nm)
    VofaLiteSetChannel(8, inst->params.inertia_ff);  // CH8: 惯量前馈 (Nm)
    VofaLiteSetChannel(9, inst->params.friction_ff); // CH9: 摩擦前馈 / chirp (Nm)
    /* CH10-CH11: MIT 输出 */
    VofaLiteSetChannel(10, inst->mit.pos_output);   // CH10: MIT 位置环输出 (Nm)
    VofaLiteSetChannel(11, inst->mit.speed_output); // CH11: MIT 速度环输出 (Nm)
    /* CH12: setref值，最终发送给电机的电流/力矩值 */
    VofaLiteSetChannel(12, setref);
#endif
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED
