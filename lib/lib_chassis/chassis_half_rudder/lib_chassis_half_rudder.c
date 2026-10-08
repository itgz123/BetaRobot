/**
 * @file lib_chassis_half_rudder.c
 * @brief 半舵底盘几何封装实现
 * @author TRW
 * @date 2026-10-08
 */

#include "lib_chassis_half_rudder.h"
#include "app_cfg.h"

#ifdef LIB_CHASSIS_HALF_RUDDER_USED

#include "lib_math.h"

int8_t LibChassisHalfRudderInit(LibChassisInstance_s *inst, const LibChassisHalfRudderGeom_s *geom)
{
    if (inst == NULL || geom == NULL)
        return -1;
    if (geom->radius <= 0.0f)
        return -1;

    LibChassisConfig_s cfg = {0};
    cfg.num = LIB_CHASSIS_HALF_RUDDER_NUM;

    const float px[LIB_CHASSIS_HALF_RUDDER_NUM] = {geom->l_pos_x, geom->r_pos_x};
    const float py[LIB_CHASSIS_HALF_RUDDER_NUM] = {geom->l_pos_y, geom->r_pos_y};

    for (uint8_t i = 0; i < LIB_CHASSIS_HALF_RUDDER_NUM; i++)
    {
        cfg.wheel[i].x = px[i];
        cfg.wheel[i].y = py[i];
        cfg.wheel[i].mount_angle = 0.0f;    // 舵轮忽略：轮面方向由 state.steer_angle 给
        cfg.wheel[i].roller_angle = M_PI_2; // 无斜滚子
        cfg.wheel[i].radius = geom->radius;
        cfg.wheel[i].kind = LIB_CHASSIS_WHEEL_RUDDER;
    }

    return LibChassisInit(inst, &cfg);
}

#endif /* LIB_CHASSIS_HALF_RUDDER_USED */
