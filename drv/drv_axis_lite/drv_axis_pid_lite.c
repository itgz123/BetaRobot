/**
 * @file drv_axis_pid_lite.c
 * @brief 轻量级单轴关节控制驱动模块实现（位置 + 速度串级双 PID）
 * @author TRW
 * @date 2026-11-06
 */

#include "drv_axis_pid_lite.h"
#include "app_cfg.h"

#if defined(DRV_AXIS_PID_LITE_USED) && defined(LIB_PID_USED)

#include "bsp_dwt.h"
#include "lib_math.h"
#include "drv_vofa.h"
#include <string.h>
#include <math.h>

int8_t AxisPidLiteInit(AxisPidLiteInstance *inst, const AxisPidLite_Init_Config_s *cfg)
{
    if (inst == NULL || cfg == NULL)
    {
        return -1;
    }

    memset(inst, 0, sizeof(AxisPidLiteInstance));
    inst->stage = cfg->stage;
    inst->delay_ms = cfg->delay_ms;
    inst->dt_max = cfg->dt_max;
    inst->vofa_enable = cfg->vofa_enable; // 位掩码：基础块 / 位置环 / 速度环，见 AxisPidLiteVofa_e
    /* 起始通道 0 按 1 处理（CH0 是 DWT 时间戳，不能被数据覆盖）。
     * 基础块上限收敛到 244（base+11）、两个 PID 块收敛到 250（base+5），
     * 保证 uint8_t 通道号相加不溢出。
     * 超出 VOFA_CHANNELS 的部分由 VofaSetChannel 自动丢弃，不会越界写缓冲。 */
    inst->vofa_base_ch = (cfg->vofa_base_ch == 0) ? 1 : cfg->vofa_base_ch;
    if (inst->vofa_base_ch > (uint8_t)(UINT8_MAX - 11))
    {
        inst->vofa_base_ch = (uint8_t)(UINT8_MAX - 11);
    }
    inst->vofa_pos_pid_base_ch = (cfg->vofa_pos_pid_base_ch == 0) ? 1 : cfg->vofa_pos_pid_base_ch;
    if (inst->vofa_pos_pid_base_ch > (uint8_t)(UINT8_MAX - 5))
    {
        inst->vofa_pos_pid_base_ch = (uint8_t)(UINT8_MAX - 5);
    }
    inst->vofa_vel_pid_base_ch = (cfg->vofa_vel_pid_base_ch == 0) ? 1 : cfg->vofa_vel_pid_base_ch;
    if (inst->vofa_vel_pid_base_ch > (uint8_t)(UINT8_MAX - 5))
    {
        inst->vofa_vel_pid_base_ch = (uint8_t)(UINT8_MAX - 5);
    }
    inst->params = cfg->params;

    // 初始化扫频参数
    inst->chirp_params = cfg->chirp_params;

    // 初始化正弦参数
    inst->sine_params = cfg->sine_params;

    // 初始化多正弦叠加参数
    inst->multi_sine_params = cfg->multi_sine_params;

    // 初始化TUNE基准（延时结束后按当前位置记录）
    inst->tune_base_angle = 0.0f;
    inst->tune_base_flag = 0;

    /* 位置环：环绕轴(WRAP)设 2π，位置误差取最短路径 wrap 到 [-π, π)，边界不跳变。
     * 顶层 error_normalize_enable 与 pos_pid 自带的掩码/范围等效，这里统一归一到 pos_pid。 */
    PID_Init_Config_s pos_pid_cfg = cfg->pos_pid;
    if (cfg->error_normalize_enable && cfg->error_normalize_range > 0.0f)
    {
        pos_pid_cfg.error_normalize_range = cfg->error_normalize_range;
        pos_pid_cfg.config_mask |= PID_ENABLE_ERROR_NORMALIZE;
    }
    LibPIDInit(&inst->pos_pid, &pos_pid_cfg);

    // 速度环：输出为力矩，前馈由 Calculate 传入（与 MIT 相同的重力/惯量/摩擦前馈）
    LibPIDInit(&inst->vel_pid, &cfg->vel_pid);

    return 0;
}

/**
 * @brief 计算本次调用间隔 dt
 * @param inst 实例指针
 * @param now_us 当前时间戳 (us)
 * @return 调用间隔 (s)，首帧为 0；超过 dt_max 时按 dt_max 钳位
 */
static inline float CalcDt(AxisPidLiteInstance *inst, uint64_t now_us)
{
    float dt = 0.0f;
    if (inst->calc_flag && now_us > inst->last_calc_us)
    {
        dt = (float)(now_us - inst->last_calc_us) * 1e-6f;
    }
    inst->last_calc_us = now_us;
    inst->calc_flag = 1;

    if (inst->dt_max > 0.0f && dt > inst->dt_max)
    {
        dt = inst->dt_max;
    }
    return dt;
}

/**
 * @brief 计算延时结束后的时间
 * @param inst 实例指针
 * @param now_us 当前时间戳 (us)
 * @return 相对于延时结束后的时间 (s)
 * @note 仅当 init_flag 已置位、延时已通过后调用
 */
static inline float CalcTimeSinceDelay(const AxisPidLiteInstance *inst, uint64_t now_us)
{
    uint64_t delay_us = (uint64_t)inst->delay_ms * 1000ULL;
    return (float)(now_us - inst->init_time_us - delay_us) * 1e-6f;
}

/**
 * @brief 安全获取外部设定值，NAN时返回默认值
 */
static inline float SafeGetRef(const float *ptr, float default_val)
{
    float val = (ptr != NULL) ? *ptr : default_val;
    return isfinite(val) ? val : default_val;
}

/**
 * @brief 计算惯量前馈和摩擦前馈，累加到总前馈
 * @note 与 drv_axis_mit_lite 的前馈完全一致
 */
static inline void CalcFeedforward(AxisPidLiteInstance *inst, float ref_acc, float speed)
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
 * @brief 生成多正弦叠加力矩设定值（封闭形式，O(1) 计算）
 * @param params 多正弦参数结构体指针
 * @param t 当前时间 (s)
 * @return 叠加力矩 (Nm)
 * @note τ(t) = A · Σ sin(2π·i·t/T) = A · sin(Nπt/T) · sin((N+1)πt/T) / sin(πt/T)
 *       三角恒等封闭形式，O(1) 计算，num_freqs 不影响执行时间。
 */
static inline float GenerateMultiSineTorque(const MultiSineParam_s *params, float t)
{
    // 保护：duration 非正时无法定义频率，直接返回 0，避免除零
    if (params->duration <= 0.0f)
    {
        return 0.0f;
    }

    float theta = M_2PI * t / params->duration; // ω₀·t = 2π·t/T
    float half = 0.5f * theta;                  // π·t/T
    float sin_half = Lib_Math_Sin(half);

    // sin(πt/T) = 0 时（t = kT），所有 sin(i·θ) = 0，总和为 0
    if (sin_half == 0.0f)
        return 0.0f;

    float N = (float)params->num_freqs;
    float sum = Lib_Math_Sin(N * half) * Lib_Math_Sin((N + 1.0f) * half) / sin_half;

    return params->amplitude * sum;
}

/**
 * @brief 播种 PID 微分状态，避免首次闭环按 (0 - 当前值)/dt 打出 D 尖峰
 * @note 与 drvlib_motor 的 MotorSeedStage 一致：把 last_error/last_measure 对齐到当前值，
 *       并清掉微分及微分滤波状态；积分状态由 LibPIDInit 清零，这里不动。
 */
static inline void SeedPid(PIDInstance *pid, float setpoint, float measure)
{
    float error = setpoint - measure;
    if ((pid->config_mask & PID_ENABLE_ERROR_NORMALIZE) && pid->error_normalize_range > 0.0f)
    {
        float half = pid->error_normalize_range * 0.5f;
        error = Lib_Math_WrapAngle(error, -half, half);
    }

    pid->measure = measure;
    pid->last_measure = measure;
    pid->last_error = error;
    pid->d_out = 0.0f;
    pid->last_d_out = 0.0f;
}

/**
 * @brief 串级双环闭环计算
 * @param inst 实例指针
 * @param angle 反馈位置 (rad)
 * @param speed 反馈速度 (rad/s)
 * @param ref_pos 位置设定值 (rad)
 * @param ref_vel 速度设定值 (rad/s)，同时作为位置环前馈
 * @param dt 调用间隔 (s)
 * @return 力矩 (Nm)
 * @note vel_set = POS_PID(ref_pos, angle, ref_vel, dt)
 *       output  = VEL_PID(vel_set, speed, total_ff, dt)
 */
static inline float CalcCascadePid(AxisPidLiteInstance *inst, float angle, float speed, float ref_pos, float ref_vel, float dt)
{
    // 首次闭环：位置环按当前测量值播种，避免 D 项从 0 跳到当前误差
    if (!inst->pid_seeded)
    {
        SeedPid(&inst->pos_pid, ref_pos, angle);
    }

    // 位置环：前馈为设定速度，输出即速度环设定值
    float vel_set = LibPIDCalculate(&inst->pos_pid, ref_pos, angle, ref_vel, dt);

    // 速度环设定值此时才确定，同样在首次闭环时播种
    if (!inst->pid_seeded)
    {
        SeedPid(&inst->vel_pid, vel_set, speed);
        inst->pid_seeded = 1;
    }

    // 速度环：前馈为重力/惯量/摩擦力矩，输出为最终力矩
    return LibPIDCalculate(&inst->vel_pid, vel_set, speed, inst->params.total_ff, dt);
}

float AxisPidLiteCalculate(AxisPidLiteInstance *inst, const AxisLiteState_s *state, const AxisLiteRef_s *ref)
{
    if (inst == NULL || state == NULL || ref == NULL)
    {
        return 0.0f;
    }

    // ======== 公共反馈与重力前馈 ========
    float gear = inst->params.gear_ratio;
    // 保护：减速比为 0 或非有限值时按直驱 (1.0) 处理，避免除零产生 Inf/NaN
    if (!isfinite(gear) || gear == 0.0f)
    {
        gear = 1.0f;
    }

    // 反馈从电机侧转换到输出侧（减速比影响位置/速度/加速度/力矩）
    float angle_motor = state->position;                             // 电机侧 rad
    float speed_motor = state->speed;                                // 电机侧 rad/s
    float angle = isfinite(angle_motor) ? angle_motor / gear : 0.0f; // 输出侧 rad
    float speed = isfinite(speed_motor) ? speed_motor / gear : 0.0f; // 输出侧 rad/s

    inst->params.gravity_ff = inst->params.gravity * Lib_Math_Cos(angle); // Nm

    uint64_t now_us = DWT_GetTimeUs();
    float dt = CalcDt(inst, now_us); // s，首帧为 0

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
            // 延时中：仅重力前馈（gear 已在上方保护为合法值）
            setref = inst->params.gravity_ff / gear;
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

    case AXIS_LITE_STAGE_IDENTIFY: {
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
        float amp_t = (A_end > 0.0f && T > 0.0f) ? A_start + (A_end - A_start) * t / T : A_start;

        float chirp = amp_t * Lib_Math_Sin(phase); // Nm

        inst->params.friction_ff = chirp;
        inst->params.total_ff = inst->params.gravity_ff + chirp;
        output = inst->params.total_ff;
        break;
    }

    case AXIS_LITE_STAGE_IDENTIFY_OLS: {
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

    case AXIS_LITE_STAGE_TUNE: {
        float t = CalcTimeSinceDelay(inst, now_us); // s

        // 首次进入 TUNE（延时结束后）记录当前角度作为正弦参考中心
        // 环绕轴角度归一化到 ±π，以当前位置为基准可避免起始误差过大导致 kp 饱和
        if (!inst->tune_base_flag)
        {
            inst->tune_base_angle = angle;
            inst->tune_base_flag = 1;
        }

        float A = inst->sine_params.amplitude;    // rad
        float w = M_2PI * inst->sine_params.freq; // rad/s
        float base = inst->tune_base_angle;

        ref_pos = base + A * Lib_Math_Sin(w * t);   // rad
        ref_vel = A * w * Lib_Math_Cos(w * t);      // rad/s
        ref_acc = -A * w * w * Lib_Math_Sin(w * t); // rad/s²

        CalcFeedforward(inst, ref_acc, speed);

        output = CalcCascadePid(inst, angle, speed, ref_pos, ref_vel, dt);
        if (!isfinite(output))
        {
            LibPIDReset(&inst->pos_pid);
            LibPIDReset(&inst->vel_pid);
            inst->pid_seeded = 0; // 重新播种，避免下一拍 D 项从陈旧值起跳
            output = inst->params.gravity_ff;
        }
        break;
    }

    case AXIS_LITE_STAGE_NORMAL: {
        ref_pos = SafeGetRef(&ref->position, angle);    // rad
        ref_vel = SafeGetRef(&ref->speed, 0.0f);        // rad/s
        ref_acc = SafeGetRef(&ref->acceleration, 0.0f); // rad/s²

        CalcFeedforward(inst, ref_acc, speed);

        output = CalcCascadePid(inst, angle, speed, ref_pos, ref_vel, dt);
        if (!isfinite(output))
        {
            LibPIDReset(&inst->pos_pid);
            LibPIDReset(&inst->vel_pid);
            inst->pid_seeded = 0; // 重新播种，避免下一拍 D 项从陈旧值起跳
            output = inst->params.gravity_ff;
        }
        break;
    }

    default:
        // 未知阶段：仅重力前馈（与 MIT 版空设定值的输出一致）
        output = inst->params.gravity_ff;
        break;
    }

    setref = output / gear; // gear 已在上方保护为合法值

vofa_output:
    if (inst->vofa_enable & AXIS_PID_LITE_VOFA_BASE)
    {
        /* 起始通道 B = vofa_base_ch，多轴实例各自偏移，一帧即可发多轴数据 */
        uint8_t b = inst->vofa_base_ch;
        /* B+0 ~ B+2: 测量值 */
        VofaSetChannel((uint8_t)(b + 0), angle);                // 反馈位置 (rad)
        VofaSetChannel((uint8_t)(b + 1), speed);                // 反馈速度 (rad/s)
        VofaSetChannel((uint8_t)(b + 2), state->torque * gear); // 轴侧实际力矩 (Nm)
        /* B+3 ~ B+5: 设定值 */
        VofaSetChannel((uint8_t)(b + 3), ref_pos); // 位置设定值 (rad)
        VofaSetChannel((uint8_t)(b + 4), ref_vel); // 速度设定值 (rad/s)
        VofaSetChannel((uint8_t)(b + 5), ref_acc); // 加速度设定值 (rad/s^2)
        /* B+6 ~ B+8: 前馈分量 */
        VofaSetChannel((uint8_t)(b + 6), inst->params.gravity_ff);  // 重力前馈 (Nm)
        VofaSetChannel((uint8_t)(b + 7), inst->params.inertia_ff);  // 惯量前馈 (Nm)
        VofaSetChannel((uint8_t)(b + 8), inst->params.friction_ff); // 摩擦前馈 / chirp (Nm)
        /* B+9 ~ B+10: 串级 PID 输出 */
        VofaSetChannel((uint8_t)(b + 9), inst->pos_pid.output);  // 位置环输出 = 速度环设定值 (rad/s)
        VofaSetChannel((uint8_t)(b + 10), inst->vel_pid.output); // 速度环输出 (Nm)
        /* B+11: setref值，最终发送给电机的电流/力矩值 */
        VofaSetChannel((uint8_t)(b + 11), setref);
    }

    /* 位置环 / 速度环内部变量：各占 6 通道
     * （error / p_out / i_out / d_out / feedforward_out / output），
     * 起始通道分别由两个 base 指定。
     * output = p_out + i_out + d_out + feedforward_out（再经输出滤波/限幅）。
     * error 已做误差归一化；非串级阶段 PID 未运行，输出保持上次值。 */
    if (inst->vofa_enable & AXIS_PID_LITE_VOFA_POS_PID)
    {
        uint8_t p = inst->vofa_pos_pid_base_ch;
        VofaSetChannel((uint8_t)(p + 0), inst->pos_pid.error);           // 误差 (rad)
        VofaSetChannel((uint8_t)(p + 1), inst->pos_pid.p_out);           // P 项 (rad/s)
        VofaSetChannel((uint8_t)(p + 2), inst->pos_pid.i_out);           // I 项 (rad/s)
        VofaSetChannel((uint8_t)(p + 3), inst->pos_pid.d_out);           // D 项 (rad/s)
        VofaSetChannel((uint8_t)(p + 4), inst->pos_pid.feedforward_out); // 前馈 (rad/s)
        VofaSetChannel((uint8_t)(p + 5), inst->pos_pid.output);          // 总输出 (rad/s)
    }
    if (inst->vofa_enable & AXIS_PID_LITE_VOFA_VEL_PID)
    {
        uint8_t v = inst->vofa_vel_pid_base_ch;
        VofaSetChannel((uint8_t)(v + 0), inst->vel_pid.error);           // 误差 (rad/s)
        VofaSetChannel((uint8_t)(v + 1), inst->vel_pid.p_out);           // P 项 (Nm)
        VofaSetChannel((uint8_t)(v + 2), inst->vel_pid.i_out);           // I 项 (Nm)
        VofaSetChannel((uint8_t)(v + 3), inst->vel_pid.d_out);           // D 项 (Nm)
        VofaSetChannel((uint8_t)(v + 4), inst->vel_pid.feedforward_out); // 前馈 (Nm)
        VofaSetChannel((uint8_t)(v + 5), inst->vel_pid.output);          // 总输出 (Nm)
    }

    return setref;
}

#endif /* DRV_AXIS_PID_LITE_USED && LIB_PID_USED */
