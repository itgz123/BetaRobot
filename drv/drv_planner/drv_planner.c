/**
 * @file drv_planner.c
 * @brief 单轴运动规划器驱动实现
 * @author TRW
 * @date 2026-08-22
 */

#include "drv_planner.h"
#include "bsp_dwt.h"
#include "bsp_math.h"

/* 积分保护：相邻两次调用间隔超过该值时按该值积分，避免位置跳变 */
#define PLANNER_DT_MAX_S (0.05f)

int8_t PlannerInit(PlannerInstance *inst, const Planner_Init_Config_s *cfg)
{
    if (inst == NULL || cfg == NULL)
    {
        return -1;
    }

    inst->cfg = *cfg;                     // 结构体整体拷贝（仅基本类型，无指针/联合体，安全）
    inst->last_time_us = DWT_GetTimeUs(); // 记录初始化时刻，首次 Calculate 即得到正确 dt
    inst->init_flag = 1;

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
    float ref_speed = BSP_Math_Clamp(in->target_cmd * cfg->max_speed, -cfg->max_speed, cfg->max_speed);

    // ======== 3. 设定加速度：当前加速度 + 速度差纠正，限幅到 ±max_acc ========
    // 稳态（设定速度==当前速度）时输出当前加速度；有速度差时叠加纠正项
    float ref_acc = in->current_acceleration;
    if (dt > 0.0f)
    {
        ref_acc += (ref_speed - in->current_speed) / dt;
    }
    ref_acc = BSP_Math_Clamp(ref_acc, -cfg->max_acc, cfg->max_acc);

    // ======== 4. 设定位置：当前位置 + 设定速度积分，再按位置模式处理 ========
    float ref_pos = in->current_position + (ref_speed * dt);
    switch (cfg->position_mode)
    {
    case PLANNER_POS_LIMITED:
        // 限幅模式：位置限幅到 [min, max]
        if (cfg->pos_limit_min < cfg->pos_limit_max)
        {
            ref_pos = BSP_Math_Clamp(ref_pos, cfg->pos_limit_min, cfg->pos_limit_max);
        }
        break;
    case PLANNER_POS_WRAP:
        // 环绕模式：位置归一化到 [min, max)
        ref_pos = BSP_Math_WrapAngle(ref_pos, cfg->pos_limit_min, cfg->pos_limit_max);
        break;
    case PLANNER_POS_CONTINUOUS:
    default:
        // 连续模式：不限幅
        break;
    }

    out->position = ref_pos;
    out->speed = ref_speed;
    out->acceleration = ref_acc;
}
