/**
 * @file drv_chassis_lite_position_half_rudder.c
 * @brief 轻量级半舵底盘运动学实现（2 组舵轮，纯计算，不依赖电机）
 * @author TRW
 * @date 2026-09-13
 */

#include "drv_chassis_lite_position_half_rudder.h"
#include "app_cfg.h"

#ifdef DRV_CHASSIS_LITE_POSITION_HALF_RUDDER_USED

#include "drv_chassis_lite_position_rudder_core.h"

int8_t ChassisLitePositionHalfRudderInit(ChassisLitePositionHalfRudderInstance_t *inst, const ChassisLitePositionHalfRudder_Cfg_s *cfg)
{
    if (inst == NULL || cfg == NULL)
        return -1;
    if (cfg->wheel_radius <= 0.0f)
        return -2;
    if (cfg->reduction_ratio <= 0.0f)
        return -3;

    /* 两组回转中心不可重合，否则方位不可分辨（退化） */
    float dx = cfg->pos_x[0] - cfg->pos_x[1];
    float dy = cfg->pos_y[0] - cfg->pos_y[1];
    if (Lib_Math_Sqrt(dx * dx + dy * dy) < 1e-4f)
        return -4;

    inst->wheel_radius = cfg->wheel_radius;
    inst->reduction_ratio = cfg->reduction_ratio;
    for (int i = 0; i < CHASSIS_LITE_POSITION_HALF_RUDDER_NUM; i++)
    {
        inst->pos_x[i] = cfg->pos_x[i];
        inst->pos_y[i] = cfg->pos_y[i];
    }

    return 0;
}

void ChassisLitePositionHalfRudderInverse(const ChassisLitePositionHalfRudderInstance_t *inst, ChassisCmd_t cmd,
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

    ChassisLitePositionRudderInverseCore(CHASSIS_LITE_POSITION_HALF_RUDDER_NUM, inst->pos_x, inst->pos_y,
                                         inst->wheel_radius, inst->reduction_ratio,
                                         cmd, st->steer_angle, out);
}

ChassisCmd_t ChassisLitePositionHalfRudderForward(const ChassisLitePositionHalfRudderInstance_t *inst,
                                                  const ChassisLitePositionRef_t *in, const ChassisLiteState_t *st)
{
    ChassisCmd_t cmd = {0};

    if (inst == NULL || in == NULL || st == NULL)
        return cmd;

    return ChassisLitePositionRudderForwardCore(CHASSIS_LITE_POSITION_HALF_RUDDER_NUM, inst->pos_x, inst->pos_y,
                                                inst->wheel_radius, inst->reduction_ratio,
                                                in->drive_speed, st->steer_angle);
}

#endif /* DRV_CHASSIS_LITE_POSITION_HALF_RUDDER_USED */
