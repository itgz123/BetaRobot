/**
 * @file drv_planner.c
 * @brief 单轴运动规划器驱动实现
 * @author TRW
 * @date 2026-08-22
 */

#include "drv_planner.h"
#include "bsp_dwt.h"
#include "lib_math.h"

/* 积分保护：相邻两次调用间隔超过该值时按该值积分，避免位置跳变 */
#define PLANNER_DT_MAX_S (0.05f)

/* 速度误差 → 加速度前馈的默认增益 (1/s)。
 * 旧实现用 1/dt：500 Hz 下是 500 1/s，实测速度的编码器量化/测速噪声（±0.04 rad/s）
 * 会被放大成 ±20 rad/s² 并顶到 max_acc，再经轴侧 J·a 变成 ±0.135 Nm 反作用于轴，
 * 形成 44 Hz 自激极限环（ig/v_e.csv 实测）。
 * 有限增益下速度环等效阻尼 J·K：K=20 时 0.27 Nm/(rad/s)，
 * 穿越频率 ≈ K + kv/J ≈ 42 rad/s（6.7 Hz），对几 ms 的 CAN/任务延迟仍有足够相位裕度；
 * 44 Hz 处环增益 J·K/(Jω) ≈ 0.15 << 1，不会自激。 */
#define PLANNER_ACC_ERROR_GAIN_DEFAULT (20.0f)

int8_t PlannerInit(PlannerInstance *inst, const Planner_Init_Config_s *cfg)
{
    if (inst == NULL || cfg == NULL)
    {
        return -1;
    }

    inst->cfg = *cfg;                     // 结构体整体拷贝（仅基本类型，无指针/联合体，安全）
    inst->last_time_us = DWT_GetTimeUs(); // 记录初始化时刻，首次 Calculate 即得到正确 dt
    inst->init_flag = 1;
    inst->ref_position = 0.0f; // 累加器清零
    inst->position_valid = 0;  // 未播种：首次 Calculate 会用反馈位置播种

    return 0;
}

void PlannerCalculate(PlannerInstance *inst, const PlannerInput_s *in, PlannerOutput_s *out)
{
    if (out == NULL)
    {
        return;
    }

    // 输入/实例异常时输出安全默认值
    if (inst == NULL || in == NULL)
    {
        out->position = (in != NULL) ? in->current_position : 0.0f;
        out->speed = 0.0f;
        out->acceleration = 0.0f;
        return;
    }

    // ======== 1. 计算实际时间差 dt ========
    uint64_t now_us = DWT_GetTimeUs();
    float dt = 0.0f;
    if (inst->init_flag && now_us > inst->last_time_us)
    {
        dt = (float)(now_us - inst->last_time_us) * 1e-6f;
        if (dt > PLANNER_DT_MAX_S)
        {
            dt = PLANNER_DT_MAX_S; // 防长时间未调用导致位置跳变
        }
    }
    inst->last_time_us = now_us;
    inst->init_flag = 1;

    const Planner_Init_Config_s *cfg = &inst->cfg;

    // ======== 2. 设定速度：通道值(-1~1) × max_speed = 目标速度，限幅到 ±max_speed ========
    float ref_speed = Lib_Math_Clamp(in->target_cmd * cfg->max_speed, -cfg->max_speed, cfg->max_speed);

    // ======== 3. 设定加速度：当前加速度 + 速度差 × 有限增益，限幅到 ±max_acc ========
    // 稳态（设定速度==当前速度）时输出当前加速度；有速度差时叠加纠正项。
    // ⚠ 这里不能用 (ref_speed - current_speed)/dt：1/dt 在 500 Hz 下等于 500 1/s，
    //   实测速度的 ±0.04 rad/s 抖动会变成 ±20 rad/s²，顶到 max_acc 后经惯量前馈
    //   反过来驱动轴，形成 44 Hz 自激（见 ig/v_e.csv 与 PLANNER_ACC_ERROR_GAIN_DEFAULT）。
    const float acc_gain = (cfg->acc_error_gain > 0.0f) ? cfg->acc_error_gain : PLANNER_ACC_ERROR_GAIN_DEFAULT;
    float ref_acc = in->current_acceleration + acc_gain * (ref_speed - in->current_speed);
    ref_acc = Lib_Math_Clamp(ref_acc, -cfg->max_acc, cfg->max_acc);

    // ======== 4. 设定位置：内部累加器开环积分（不读反馈），再按位置模式处理 ========
    // 播种：失能期间/视觉交还时（in->seed）用当前反馈位置重置累加器；首次调用
    // （position_valid=0）也必须播种，否则从 0 开始积分，使能瞬间目标会猛扑向实际位置
    if (in->seed || !inst->position_valid)
    {
        inst->ref_position = in->current_position;
        inst->position_valid = 1;
    }
    else if (dt > 0.0f)
    {
        inst->ref_position += ref_speed * dt;
    }

    float ref_pos = inst->ref_position;
    switch (cfg->position_mode)
    {
    case PLANNER_POS_LIMITED:
        // 限幅模式：位置限幅到 [min, max]
        if (cfg->pos_limit_min < cfg->pos_limit_max)
        {
            ref_pos = Lib_Math_Clamp(ref_pos, cfg->pos_limit_min, cfg->pos_limit_max);
        }
        break;
    case PLANNER_POS_WRAP:
        // 环绕模式：位置归一化到 [min, max)
        ref_pos = Lib_Math_WrapAngle(ref_pos, cfg->pos_limit_min, cfg->pos_limit_max);
        break;
    case PLANNER_POS_CONTINUOUS:
    default:
        // 连续模式：不限幅（注意累加器无界，长时间运行会丢浮点精度）
        break;
    }
    // 写回累加器：LIMITED 防积分饱和（顶到限位继续推不积累"欠账"，反向立即响应），
    // WRAP 防多圈发散/精度丢失（累加器恒在 [min, max) 内）
    inst->ref_position = ref_pos;

    /* 顶到位置限位时把外向速度/加速度清零：位置目标不越界，但下游 MIT 的
     * kd*(ref_vel - 实测速度) 在摇杆打满、轴已停住时仍会把轴持续往限位上顶。
     * 只清"继续向外"的那一半：ref_speed 符号一反转（摇杆反向）就立刻恢复，
     * 不会把已到边的轴锁死。ref_acc 是同一拍按 ref_speed 算的惯量前馈，必须联动清零，
     * 否则速度已停、前馈还在往外推。
     * 仅 LIMITED 模式：WRAP 的 ±π 是环绕点不是机械止点，到边仍要继续转。 */
    if ((cfg->position_mode == PLANNER_POS_LIMITED) && (cfg->pos_limit_min < cfg->pos_limit_max))
    {
        uint8_t push_out = ((ref_pos <= cfg->pos_limit_min) && (ref_speed < 0.0f)) || ((ref_pos >= cfg->pos_limit_max) && (ref_speed > 0.0f));
        if (push_out)
        {
            ref_speed = 0.0f;
            ref_acc = 0.0f;
        }
    }

    out->position = ref_pos;
    out->speed = ref_speed;
    out->acceleration = ref_acc;
}
