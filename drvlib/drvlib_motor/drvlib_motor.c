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
static void MotorSeedStage(DrvlibMotorInstance *inst, DrvlibMotorStage_e stage, float setpoint, float measure);

/*============================ 内部函数实现 ============================*/

/**
 * @brief 反馈后处理：多圈累加 → 偏置 →(WRAP 归一化)；速度/电流各自一阶低通
 * @note 顺序照搬旧 drv_motor（累加-偏置-方向-归一化），方向由 cfg.feedback_direction 声明。
 * @note 回绕检测用"速度辅助"：一帧位置变化与 speed*dt 的差除以跨度取整即圈数；
 *       dt<=0 时退化为按 ±半个跨度判向（与旧实现一致）。
 * @note **同帧去重**：同一 timestamp_us 的帧只处理一次。dt 取帧时间差（不是 app 每次
 *       调 SetRef 的周期），否则控制快于反馈时会重复滤波、重复累加圈数。
 * @note **中断恢复**：帧间隔超过 stale_gap（缺省 dt_max）时重新播种各环微分状态与低通状态。
 */
static void MotorProcessFeedback(DrvlibMotorInstance *inst, const DrvlibMotorFeedback_s *fb, float dt)
{
    const DrvlibMotor_Config_s *cfg = &inst->cfg;
    const float span = cfg->position_span;
    const float raw = fb->position;                      /* 协议原始量：回绕检测与 LPF 状态都基于未修正值 */
    const float fb_dir = (float)cfg->feedback_direction; /* Config 已归一化为 ±1 */

    /* 同帧去重：app 的控制频率可以高于 CAN 帧率，同一个 fb 会被反复喂进来。
     * 重复处理会让低通多滤一次、并让回绕判据按非零 speed*dt 凭空数圈（同帧 diff=0）。
     * 注意这里只跳过后处理；闭环的 PID 仍由 SetRef 每拍计算。 */
    if (fb->timestamp_us != 0 && inst->feedback_last_ts == fb->timestamp_us)
    {
        return;
    }

    /* 反馈侧 dt：优先用帧时间差（与调用频率解耦），拿不到时间基准时退回 app 传入的 dt。
     * 例：控制 1 kHz、反馈 500 Hz 时 app 的 dt=1 ms，而真实帧间隔是 2 ms。 */
    float fb_dt = (dt > 0.0f) ? dt : 0.0f;
    if (inst->feedback_last_ts != 0 && fb->timestamp_us > inst->feedback_last_ts)
    {
        fb_dt = (float)(fb->timestamp_us - inst->feedback_last_ts) * 1e-6f;
    }

    /* 中断恢复：断链期间的测量值已经不可信，重新播种各环（见 MotorSeedStage）与低通状态，
     * 避免恢复首拍按 (断前 last_measure − 当前 measure)/dt 打出 D 尖峰。 */
    const float gap_limit = (cfg->stale_gap > 0.0f) ? cfg->stale_gap : cfg->dt_max;
    if (inst->has_last && gap_limit > 0.0f && fb_dt > gap_limit)
    {
        memset(inst->pid_seeded, 0, sizeof(inst->pid_seeded));
        inst->lpf_seeded = 0;
    }

    int32_t wraps = 0;
    if (span > 0.0f && inst->has_last)
    {
        float diff = raw - inst->position_single_last;
        if (fb_dt > 0.0f)
        {
            float expected = fb->speed * fb_dt;
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

    /* 累加 → 偏置 → 方向 →（WRAP 才归一化） */
    double pos = (double)inst->position_cnt * (double)span + (double)raw + (double)cfg->position_offset;
    pos *= (double)fb_dir; /* 方向：累加-偏置后、归一化前（照搬旧 drv_motor 顺序） */
    /* 连续值：角度环微分用（跨窗口边界不跳变），对外不暴露 */
    inst->position_continuous = pos;
    if (cfg->position_mode == DRVLIB_MOTOR_POS_WRAP)
    {
        pos = (double)Lib_Math_WrapAngle((float)pos, cfg->angle_limit_min, cfg->angle_limit_max);
    }

    /* 速度/电流：各自一阶低通（速度 lpf_rc，电流 current_lpf_rc，<=0 透传）。
     * 未播种（Config/Reset/中断恢复后首帧）或没有时间基准时透传，并把状态同步成原始值：
     * 下一帧从真实样本起步，不会出现"状态从 0 开始爬"的瞬态（旧实现的状态从 0 起）。 */
    const uint8_t can_filter = (uint8_t)((inst->lpf_seeded && fb_dt > 0.0f) ? 1u : 0u);

    float speed;
    if (can_filter && cfg->lpf_enable == DRVLIB_MOTOR_LPF_ENABLE && cfg->lpf_rc > 0.0f)
    {
        float alpha = fb_dt / (cfg->lpf_rc + fb_dt);
        speed = fb->speed * alpha + inst->speed_lpf_state * (1.0f - alpha);
    }
    else
    {
        speed = fb->speed;
    }
    inst->speed_lpf_state = speed;

    float current;
    if (can_filter && cfg->current_lpf_rc > 0.0f)
    {
        float alpha = fb_dt / (cfg->current_lpf_rc + fb_dt);
        current = fb->current * alpha + inst->current_lpf_state * (1.0f - alpha);
    }
    else
    {
        current = fb->current;
    }
    inst->current_lpf_state = current;
    inst->lpf_seeded = 1;

    DrvlibMotor_Data_t *d = &inst->data;
    d->position = pos; /* 已含 feedback_direction */
    d->position_single = raw;
    d->position_cnt = inst->position_cnt;
    d->speed = speed * fb_dir;     /* LPF 状态存未修正值，方向只作用在输出 */
    d->current = current * fb_dir; /* 同上 */
    d->timestamp_us = fb->timestamp_us;

    inst->position_single_last = raw;
    inst->feedback_last_ts = fb->timestamp_us;
    inst->has_last = 1;
}

/**
 * @brief 首次进入某个环时播种 PID 的 last_measure / last_error
 * @note Config/Disable 会把 PID 清零（last_measure = 0）。若当前测量非 0，
 *       第一拍"微分先行"会按 (0 − measure)/dt 打出一个尖峰；在第一次计算前
 *       把 last_measure 对齐到当前测量值即可。每个环只播种一次（pid_seeded）。
 */
static void MotorSeedStage(DrvlibMotorInstance *inst, DrvlibMotorStage_e stage, float setpoint, float measure)
{
    PIDInstance *pid = &inst->pid[stage];

    pid->measure = measure;
    pid->last_measure = measure;
    pid->last_error = setpoint - measure;
    /* 微分与微分滤波状态一并清零：重新播种常发生在失能/中断恢复之后，
     * 断前最后一拍的 D 值也是陈旧的，留着会让微分滤波从旧值慢慢衰减。 */
    pid->d_out = 0.0f;
    pid->last_d_out = 0.0f;
    inst->pid_seeded[stage] = 1u;
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
        /* 测量值：WRAP 模式用连续的 position_continuous（见头文件注释）。 */
        float measure = (float)d->position;

        switch (cfg->position_mode)
        {
        case DRVLIB_MOTOR_POS_LIMITED:
            if (cfg->angle_limit_min < cfg->angle_limit_max)
            {
                s = Lib_Math_Clamp(s, cfg->angle_limit_min, cfg->angle_limit_max);
            }
            break;
        case DRVLIB_MOTOR_POS_WRAP: {
            const float range = cfg->angle_limit_max - cfg->angle_limit_min;
            if (range > 0.0f)
            {
                /* 参考值取离连续位置最近的等价代表：误差仍是"最短角差"，
                 * 但 measure 不跨窗跳变，微分先行不会打出整周期尖峰。 */
                const float cont = (float)inst->position_continuous;
                const float half = range * 0.5f;
                s = cont + Lib_Math_WrapAngle(s - cont, -half, half);
                measure = cont;
            }
            else
            {
                s = Lib_Math_WrapAngle(s, cfg->angle_limit_min, cfg->angle_limit_max);
            }
            break;
        }
        case DRVLIB_MOTOR_POS_CONTINUOUS:
        default:
            break;
        }

        if (inst->pid_seeded[DRVLIB_MOTOR_STAGE_ANGLE] == 0u)
        {
            MotorSeedStage(inst, DRVLIB_MOTOR_STAGE_ANGLE, s, measure);
        }

        float ff = (cfg->angle_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->angle_feedforward_ptr) ? *cfg->angle_feedforward_ptr : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_ANGLE], s, measure, ff, dt);
    }

    /* 速度环（中间） */
    if (cfg->loop_type & DRVLIB_MOTOR_LOOP_SPEED)
    {
        if (inst->pid_seeded[DRVLIB_MOTOR_STAGE_SPEED] == 0u)
        {
            MotorSeedStage(inst, DRVLIB_MOTOR_STAGE_SPEED, s, d->speed);
        }

        float ff = (cfg->speed_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->speed_feedforward_ptr) ? *cfg->speed_feedforward_ptr : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_SPEED], s, d->speed, ff, dt);
    }

    /* 电流环（最内） */
    if (cfg->loop_type & DRVLIB_MOTOR_LOOP_CURRENT)
    {
        if (inst->pid_seeded[DRVLIB_MOTOR_STAGE_CURRENT] == 0u)
        {
            MotorSeedStage(inst, DRVLIB_MOTOR_STAGE_CURRENT, s, d->current);
        }

        float ff = (cfg->current_feedforward_src == DRVLIB_MOTOR_FF_EXTERNAL && cfg->current_feedforward_ptr) ? *cfg->current_feedforward_ptr : 0.0f;
        s = LibPIDCalculate(&inst->pid[DRVLIB_MOTOR_STAGE_CURRENT], s, d->current, ff, dt);
    }

    /* 输出方向：修正电机安装方向。反馈侧已由 feedback_direction 修正，故闭环仍是负反馈 */
    return s * (float)cfg->motor_direction;
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

    /* 方向归一化：只认 REVERSE(-1)，其余（含未初始化的 0）一律 NORMAL(+1)。
     * 存量配置不写方向字段时保持"正向"原行为，也免去每拍再判一次。 */
    if (inst->cfg.motor_direction != DRVLIB_MOTOR_DIRECTION_REVERSE)
    {
        inst->cfg.motor_direction = DRVLIB_MOTOR_DIRECTION_NORMAL;
    }
    if (inst->cfg.feedback_direction != DRVLIB_MOTOR_DIRECTION_REVERSE)
    {
        inst->cfg.feedback_direction = DRVLIB_MOTOR_DIRECTION_NORMAL;
    }

    /* PID 初始化：只初始化参与环路的 stage。
     * 复刻旧 drv_motor：位置/速度环自动补梯形积分；WRAP 时位置环自动启用误差归一化。
     * CURRENT 环为新环，不做这些默认（保持 app 配置原样）。 */
    if (inst->cfg.loop_type & DRVLIB_MOTOR_LOOP_ANGLE)
    {
        inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE].config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        if (inst->cfg.position_mode == DRVLIB_MOTOR_POS_WRAP)
        {
            inst->cfg.pid[DRVLIB_MOTOR_STAGE_ANGLE].error_normalize_range = inst->cfg.angle_limit_max - inst->cfg.angle_limit_min;
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
    inst->position_continuous = 0.0;
    inst->speed_lpf_state = 0.0f;
    inst->current_lpf_state = 0.0f;
    inst->feedback_last_ts = 0;
    inst->lpf_seeded = 0;
    inst->has_last = 0;
    memset(inst->pid_seeded, 0, sizeof(inst->pid_seeded));
    memset(&inst->data, 0, sizeof(inst->data));

    BSPLOG(&g_drvlib_motor_log, LOG_LEVEL_INFO, "Config: loop=0x%02X pos_mode=%u span=%d mrad", inst->cfg.loop_type, inst->cfg.position_mode,
           (int)(inst->cfg.position_span * 1000.0f));
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

    const uint8_t has_fb = (uint8_t)((fb != NULL && fb->timestamp_us != 0) ? 1u : 0u);

    /* 反馈后处理与使能/环路无关：多圈累加、方向、偏置、低通必须持续跑。
     * - 失能时：否则手动转动电机后 position_cnt 与真实位置脱节，再使能会跳。
     * - 开环时：旧 drv_motor 在开环下同样 GetData，app 读 inst->data 才是新的。 */
    if (has_fb)
    {
        MotorProcessFeedback(inst, fb, dt);
    }

    if (!inst->enable)
    {
        MotorVofaOutput(inst);
        return 0.0f;
    }

    /* 开环（一位闭环位都没选）：ref 直接作输出。
     * 方向仍按安装方向修正 —— 与闭环走同一个 motor_direction，避免"开环标方向"时对不上。
     * 用掩码判断而不是 == DRVLIB_MOTOR_LOOP_OPEN：未定义的位不会被误判成闭环。 */
    if ((inst->cfg.loop_type & DRVLIB_MOTOR_LOOP_ALL) == 0)
    {
        MotorVofaOutput(inst);
        return ref * (float)inst->cfg.motor_direction;
    }

    /* 闭环需要有效反馈：无数据则输出 0 */
    if (!has_fb)
    {
        MotorVofaOutput(inst);
        return 0.0f;
    }

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
    /* 下次某环第一次计算时重新播种 last_measure（见 MotorSeedStage） */
    memset(inst->pid_seeded, 0, sizeof(inst->pid_seeded));
    inst->speed_lpf_state = 0.0f;
    inst->current_lpf_state = 0.0f;
    /* 清帧时间戳：Reset 后即使 app 又喂了同一帧，也要重新处理；
     * 清播种标志：下一帧用原始值播种低通，而不是从 0 爬。 */
    inst->feedback_last_ts = 0;
    inst->lpf_seeded = 0;
}

#endif /* DRVLIB_MOTOR_USED && LIB_PID_USED && LIB_MATH_USED */
