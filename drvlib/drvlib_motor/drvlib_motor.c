/**
 * @file drvlib_motor.c
 * @brief 电机三级级联 PID 控制核：位置/速度/电流 + 反馈后处理（实现）
 *
 * @note 分层定位、三级级联语义、反馈处理顺序、dt 约定全在 drvlib_motor.h
 *       的头注释里；本文件只记实现层面的取舍（见各函数内注释）。
 */

#include "drvlib_motor.h"

#if defined(DRVLIB_MOTOR_USED) && defined(LIB_PID_USED) && defined(LIB_MATH_USED)

#include <string.h>

#include "bsp_log.h"
#include "drv_vofa.h" /* VOFA_USED / VOFA_UART 未定义时全部是空实现 */

/*============================ 日志实例 ============================*/

/* 本模块日志实例（日志关闭时宏退化为 extern 声明，不分配、不引用） */
#ifndef DRVLIB_MOTOR_LOG_LIMIT
#define DRVLIB_MOTOR_LOG_LIMIT 10
#endif // !DRVLIB_MOTOR_LOG_LIMIT
LOG_INSTANCE_DEF(g_drvlib_motor_log, "drvlib_motor", DRVLIB_MOTOR_LOG_LIMIT);

/*============================ 内部函数声明 ============================*/

static void MotorProcessFeedback(DrvlibMotorInstance *inst, const DrvlibMotorFeedback_s *fb, float dt);
static float MotorApplyCascade(DrvlibMotorInstance *inst, float ref, float dt);
static void MotorVofaOutput(const DrvlibMotorInstance *inst);

/*============================ 内部函数实现 ============================*/

/**
 * @brief 反馈后处理：多圈累加 → 偏置 →(WRAP 归一化)；速度/电流一阶低通
 * @note 顺序照搬旧 drv_motor（累加-偏置-方向-归一化），方向已交给 app，故此处只到归一化。
 * @note 回绕检测用"速度辅助"：一帧位置变化与 speed*dt 的差除以跨度取整即圈数；
 *       dt<=0 时退化为按 ±半个跨度判向（与旧实现一致）。
 */
static void MotorProcessFeedback(DrvlibMotorInstance *inst, const DrvlibMotorFeedback_s *fb, float dt)
{
    const DrvlibMotor_Config_s *cfg = &inst->cfg;
    float span = cfg->position_span;
    float raw = fb->position;

    int32_t wraps = 0;
    if (span > 0.0f && inst->has_last)
    {
        float diff = raw - inst->position_single_last;
        if (dt > 0.0f)
        {
            float expected = fb->speed * dt;
            float wrap_f = (expected - diff) / span;
            wraps = (int32_t)(wrap_f > 0.0f ? wrap_f + 0.5f : wrap_f - 0.5f);
        }
        else
        {
            if (diff > span * 0.5f)
                wraps = -1;
            else if (diff < -span * 0.5f)
                wraps = 1;
        }
    }
    inst->position_cnt += wraps;

    /* 累加 → 偏置 → 归一化 */
    double pos = (double)inst->position_cnt * (double)span + (double)raw + (double)cfg->position_offset;
    if (cfg->position_mode == DRVLIB_MOTOR_POS_WRAP)
    {
        pos = (double)Lib_Math_WrapAngle((float)pos, cfg->angle_limit_min, cfg->angle_limit_max);
    }

    /* 速度/电流：一阶低通。dt<=0、未开 LPF、RC<=0 或首帧时透传且不改状态
     * （首帧不滤波是为对齐旧实现：旧实现首帧 dt=0 走透传，滤波从第二帧才开始） */
    float speed;
    float current;
    if (cfg->lpf_enable == DRVLIB_MOTOR_LPF_ENABLE && cfg->lpf_rc > 0.0f && dt > 0.0f && inst->has_last)
    {
        float alpha = dt / (cfg->lpf_rc + dt);
        speed = fb->speed * alpha + inst->speed_lpf_state * (1.0f - alpha);
        current = fb->current * alpha + inst->current_lpf_state * (1.0f - alpha);
        inst->speed_lpf_state = speed;
        inst->current_lpf_state = current;
    }
    else
    {
        speed = fb->speed;
        current = fb->current;
    }

    DrvlibMotor_Data_t *d = &inst->data;
    d->position = pos;
    d->position_single = raw;
    d->position_cnt = inst->position_cnt;
    d->speed = speed;
    d->current = current;
    d->timestamp_us = fb->timestamp_us;

    inst->position_single_last = raw;
    inst->has_last = 1;
}

/**
 * @brief 三级级联：由外向内，每个参与的环把输出作为下一环的 setpoint
 * @return 最终输出（电流 A）
 */
static float MotorApplyCascade(DrvlibMotorInstance *inst, float ref, float dt)
{
    const DrvlibMotor_Config_s *cfg = &inst->cfg;
    const DrvlibMotor_Data_t *d = &inst->data;

    float s = ref;

    /* 位置环（最外） */
    if (cfg->loop_type & DRVLIB_MOTOR_LOOP_ANGLE)
    {
        switch (cfg->position_mode)
        {
        case DRVLIB_MOTOR_POS_LIMITED:
            if (cfg->angle_limit_min < cfg->angle_limit_max)
            {
                s = Lib_Math_Clamp(s, cfg->angle_limit_min, cfg->angle_limit_max);
            }
            break;
        case DRVLIB_MOTOR_POS_WRAP:
            s = Lib_Math_WrapAngle(s, cfg->angle_limit_min, cfg->angle_limit_max);
            break;
        case DRVLIB_MOTOR_POS_CONTINUOUS:
        default:
            break;
        }

        float ff = (cfg->angle_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->angle_feedforward_ptr)
                       ? *cfg->angle_feedforward_ptr
                       : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_ANGLE], s, (float)d->position, ff, dt);
    }

    /* 速度环（中间） */
    if (cfg->loop_type & DRVLIB_MOTOR_LOOP_SPEED)
    {
        float ff = (cfg->speed_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->speed_feedforward_ptr)
                       ? *cfg->speed_feedforward_ptr
                       : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_SPEED], s, d->speed, ff, dt);
    }

    /* 电流环（最内） */
    if (cfg->loop_type & DRVLIB_MOTOR_LOOP_CURRENT)
    {
        float ff = (cfg->current_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->current_feedforward_ptr)
                       ? *cfg->current_feedforward_ptr
                       : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_CURRENT], s, d->current, ff, dt);
    }

    return s;
}

/**
 * @brief 可选的 VOFA 调试输出（默认关）
 * @note 通道 CH1~CH3 = 位置/速度/电流。与其它模块的通道号可能重叠，
 *       同时只能有一路在写，要同时看就把通道号错开（见头注释）。
 */
static void MotorVofaOutput(const DrvlibMotorInstance *inst)
{
    if (!inst->cfg.vofa_enable)
    {
        return;
    }
    const DrvlibMotor_Data_t *d = &inst->data;
    VofaSetChannel(1, (float)d->position);
    VofaSetChannel(2, d->speed);
    VofaSetChannel(3, d->current);
    VofaSend();
}

/*============================ 公开接口实现 ============================*/

int8_t DrvlibMotorRegister(DrvlibMotorInstance *inst)
{
    if (!inst)
    {
        BSPLOG(&g_drvlib_motor_log, LOG_LEVEL_ERROR, "Register: null instance");
        return -1;
    }
    memset(inst, 0, sizeof(*inst));
    inst->registered = 1;
    return 0;
}

int8_t DrvlibMotorConfig(DrvlibMotorInstance *inst, const DrvlibMotor_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        BSPLOG(&g_drvlib_motor_log, LOG_LEVEL_ERROR, "Config: null instance/config");
        return -1;
    }

    /* 整份拷贝（含 app 传入的指针，生命周期由 app 保证） */
    inst->cfg = *cfg;

    /* PID 初始化：只初始化参与环路的 stage。
     * 复刻旧 drv_motor：位置/速度环自动补梯形积分；WRAP 时位置环自动启用误差归一化。
     * CURRENT 环为新环，不做这些默认（保持 app 配置原样）。 */
    if (inst->cfg.loop_type & DRVLIB_MOTOR_LOOP_ANGLE)
    {
        inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE].config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        if (inst->cfg.position_mode == DRVLIB_MOTOR_POS_WRAP)
        {
            inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE].error_normalize_range =
                inst->cfg.angle_limit_max - inst->cfg.angle_limit_min;
            inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE].config_mask |= PID_ENABLE_ERROR_NORMALIZE;
        }
        LibPIDInit(&inst->pid[DRVLIB_MOTOR_STAGE_ANGLE], &inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE]);
    }

    if (inst->cfg.loop_type & DRVLIB_MOTOR_LOOP_SPEED)
    {
        inst->cfg.pid[DRVLIB_MOTOR_STAGE_SPEED].config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        LibPIDInit(&inst->pid[DRVLIB_MOTOR_STAGE_SPEED], &inst->cfg.pid[DRVLIB_MOTOR_STAGE_SPEED]);
    }

    if (inst->cfg.loop_type & DRVLIB_MOTOR_LOOP_CURRENT)
    {
        LibPIDInit(&inst->pid[DRVLIB_MOTOR_STAGE_CURRENT], &inst->cfg.pid[DRVLIB_MOTOR_STAGE_CURRENT]);
    }

    /* 状态清零（不动 enable：使能须显式调 DrvlibMotorEnable） */
    inst->position_single_last = 0.0f;
    inst->position_cnt = 0;
    inst->speed_lpf_state = 0.0f;
    inst->current_lpf_state = 0.0f;
    inst->has_last = 0;
    memset(&inst->data, 0, sizeof(inst->data));

    BSPLOG(&g_drvlib_motor_log, LOG_LEVEL_INFO, "Config: loop=0x%02X pos_mode=%u span=%d mrad", inst->cfg.loop_type,
           inst->cfg.position_mode, (int)(inst->cfg.position_span * 1000.0f));
    return 0;
}

float DrvlibMotorSetRef(DrvlibMotorInstance *inst, float ref, const DrvlibMotorFeedback_s *fb, float dt)
{
    if (!inst)
    {
        return 0.0f;
    }

    /* dt 保护：非负 + 上限钳位 */
    if (dt < 0.0f)
    {
        dt = 0.0f;
    }
    if (inst->cfg.dt_max > 0.0f && dt > inst->cfg.dt_max)
    {
        dt = inst->cfg.dt_max;
    }

    if (!inst->enable)
    {
        MotorVofaOutput(inst);
        return 0.0f;
    }

    /* 开环：无需反馈，ref 直接作输出 */
    if (inst->cfg.loop_type == DRVLIB_MOTOR_LOOP_OPEN)
    {
        MotorVofaOutput(inst);
        return ref;
    }

    /* 闭环需要有效反馈：无数据则输出 0 */
    if (!fb || fb->timestamp_us == 0)
    {
        MotorVofaOutput(inst);
        return 0.0f;
    }

    MotorProcessFeedback(inst, fb, dt);
    float out = MotorApplyCascade(inst, ref, dt);
    MotorVofaOutput(inst);
    return out;
}

void DrvlibMotorEnable(DrvlibMotorInstance *inst)
{
    if (!inst)
    {
        return;
    }
    inst->enable = 1;
}

void DrvlibMotorDisable(DrvlibMotorInstance *inst)
{
    if (!inst)
    {
        return;
    }
    inst->enable = 0;
    DrvlibMotorReset(inst);
}

void DrvlibMotorReset(DrvlibMotorInstance *inst)
{
    if (!inst)
    {
        return;
    }
    for (int i = 0; i < DRVLIB_MOTOR_STAGE_NUM; i++)
    {
        LibPIDReset(&inst->pid[i]);
    }
    inst->speed_lpf_state = 0.0f;
    inst->current_lpf_state = 0.0f;
}

#endif /* DRVLIB_MOTOR_USED && LIB_PID_USED && LIB_MATH_USED */
