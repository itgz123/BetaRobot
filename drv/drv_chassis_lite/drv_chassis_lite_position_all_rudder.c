/**
 * @file drv_chassis_lite_position_all_rudder.c
 * @brief 轻量级全舵底盘运动学实现（4 组舵轮，纯计算，不依赖电机）
 * @author TRW
 * @date 2026-09-13
 */

#include "drv_chassis_lite_position_all_rudder.h"
#include "app_cfg.h"

#ifdef DRV_CHASSIS_LITE_POSITION_ALL_RUDDER_USED

#include "drv_chassis_lite_position_rudder_core.h"

int8_t ChassisLitePositionAllRudderInit(ChassisLitePositionAllRudderInstance_t *inst, const ChassisLitePositionAllRudder_Cfg_s *cfg)
{
    if (inst == NULL || cfg == NULL)
        return -1;
    if (cfg->wheel_radius <= 0.0f)
        return -2;
    if (cfg->reduction_ratio <= 0.0f)
        return -3;

    /* 最小二乘正解的条件数：n·Σr² - (Σx)² - (Σy)² 为 0 即回转中心全重合（退化） */
    float Sx = 0.0f, Sy = 0.0f, Srr = 0.0f;
    for (int i = 0; i < CHASSIS_LITE_POSITION_ALL_RUDDER_NUM; i++)
    {
        Sx += cfg->pos_x[i];
        Sy += cfg->pos_y[i];
        Srr += cfg->pos_x[i] * cfg->pos_x[i] + cfg->pos_y[i] * cfg->pos_y[i];
    }
    float N = (float)CHASSIS_LITE_POSITION_ALL_RUDDER_NUM;
    if (N * Srr - Sx * Sx - Sy * Sy < 1e-6f)
        return -4;

    inst->wheel_radius = cfg->wheel_radius;
    inst->reduction_ratio = cfg->reduction_ratio;
    for (int i = 0; i < CHASSIS_LITE_POSITION_ALL_RUDDER_NUM; i++)
    {
        inst->pos_x[i] = cfg->pos_x[i];
        inst->pos_y[i] = cfg->pos_y[i];
    }

    return 0;
}

void ChassisLitePositionAllRudderInverse(const ChassisLitePositionAllRudderInstance_t *inst, ChassisCmd_t cmd,
                                         const ChassisLiteState_t *st, ChassisLitePositionRef_t *out)
{
    if (inst == NULL || st == NULL || out == NULL)
        return;

    out->valid = 1;
    for (int i = 0; i < WHEEL_NUM; i++)
    {
        out->steer_target[i] = 0.0f;
        out->drive_speed[i] = 0.0f;
    }

    ChassisLitePositionRudderInverseCore(CHASSIS_LITE_POSITION_ALL_RUDDER_NUM, inst->pos_x, inst->pos_y,
                                         inst->wheel_radius, inst->reduction_ratio,
                                         cmd, st->steer_angle, out);
}

ChassisCmd_t ChassisLitePositionAllRudderForward(const ChassisLitePositionAllRudderInstance_t *inst,
                                                 const ChassisLitePositionRef_t *in, const ChassisLiteState_t *st)
{
    ChassisCmd_t cmd = {0};

    if (inst == NULL || in == NULL || st == NULL)
        return cmd;

    return ChassisLitePositionRudderForwardCore(CHASSIS_LITE_POSITION_ALL_RUDDER_NUM, inst->pos_x, inst->pos_y,
                                                inst->wheel_radius, inst->reduction_ratio,
                                                in->drive_speed, st->steer_angle);
}

#endif /* DRV_CHASSIS_LITE_POSITION_ALL_RUDDER_USED */
