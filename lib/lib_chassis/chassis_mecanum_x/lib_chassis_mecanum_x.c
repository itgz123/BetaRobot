/**
 * @file lib_chassis_mecanum_x.c
 * @brief 麦轮 X 型几何封装实现
 * @author TRW
 * @date 2026-10-08
 */

#include "lib_chassis_mecanum_x.h"
#include "app_cfg.h"

#ifdef LIB_CHASSIS_MECANUM_X_USED

#include "lib_math.h"

int8_t LibChassisMecanumXInit(LibChassisInstance_s *inst, const LibChassisMecanumXGeom_s *geom)
{
    if (inst == NULL || geom == NULL)
        return -1;
    if (geom->radius <= 0.0f || geom->half_len <= 0.0f || geom->half_width <= 0.0f)
        return -1;

    LibChassisConfig_s cfg = {0};
    cfg.num = LIB_CHASSIS_WHEEL_4NUM;

    /* 轮序 LF/LB/RB/RF：四角，轮面朝前（α=0），滚子对角线成对（X 型） */
    const float hl = geom->half_len;
    const float hw = geom->half_width;
    const float px[LIB_CHASSIS_WHEEL_4NUM] = {hl, -hl, -hl, hl};
    const float py[LIB_CHASSIS_WHEEL_4NUM] = {hw, hw, -hw, -hw};
    const float pb[LIB_CHASSIS_WHEEL_4NUM] = {M_PI / 4.0f, -M_PI / 4.0f, M_PI / 4.0f, -M_PI / 4.0f};

    for (uint8_t i = 0; i < LIB_CHASSIS_WHEEL_4NUM; i++)
    {
        cfg.wheel[i].x = px[i];
        cfg.wheel[i].y = py[i];
        cfg.wheel[i].mount_angle = 0.0f; // 轮面朝前
        cfg.wheel[i].roller_angle = pb[i];
        cfg.wheel[i].radius = geom->radius;
        cfg.wheel[i].kind = LIB_CHASSIS_WHEEL_MECANUM;
    }

    return LibChassisInit(inst, &cfg);
}

#endif /* LIB_CHASSIS_MECANUM_X_USED */
