/**
 * @file lib_chassis_omni_t.c
 * @brief 全向轮 T 型几何封装实现
 * @author TRW
 * @date 2026-10-08
 */

#include "lib_chassis_omni_t.h"
#include "app_cfg.h"

#ifdef LIB_CHASSIS_OMNI_T_USED

#include "lib_math.h"

int8_t LibChassisOmniTInit(LibChassisInstance_s *inst, const LibChassisOmniTGeom_s *geom)
{
    if (inst == NULL || geom == NULL)
        return -1;
    if (geom->radius <= 0.0f || geom->half_len <= 0.0f || geom->half_width <= 0.0f)
        return -1;

    LibChassisConfig_s cfg = {0};
    cfg.num = LIB_CHASSIS_WHEEL_4NUM;

    /* 轮序 LF/LB/RB/RF：边中点，驱动方向沿边的法向 */
    const float px[LIB_CHASSIS_WHEEL_4NUM] = {geom->half_len, 0.0f, -geom->half_len, 0.0f};
    const float py[LIB_CHASSIS_WHEEL_4NUM] = {0.0f, geom->half_width, 0.0f, -geom->half_width};
    const float pa[LIB_CHASSIS_WHEEL_4NUM] = {-M_PI_2, 0.0f, M_PI_2, M_PI};

    for (uint8_t i = 0; i < LIB_CHASSIS_WHEEL_4NUM; i++)
    {
        cfg.wheel[i].x = px[i];
        cfg.wheel[i].y = py[i];
        cfg.wheel[i].mount_angle = pa[i];
        cfg.wheel[i].roller_angle = M_PI_2; // 无斜滚子
        cfg.wheel[i].radius = geom->radius;
        cfg.wheel[i].kind = LIB_CHASSIS_WHEEL_OMNI;
    }

    return LibChassisInit(inst, &cfg);
}

#endif /* LIB_CHASSIS_OMNI_T_USED */
