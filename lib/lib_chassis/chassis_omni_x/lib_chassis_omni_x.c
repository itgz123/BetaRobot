/**
 * @file lib_chassis_omni_x.c
 * @brief 全向轮 X 型几何封装实现
 * @author TRW
 * @date 2026-10-08
 */

#include "lib_chassis_omni_x.h"
#include "app_cfg.h"

#ifdef LIB_CHASSIS_OMNI_X_USED

#include "lib_math.h"

int8_t LibChassisOmniXInit(LibChassisInstance_s *inst, const LibChassisOmniXGeom_s *geom)
{
    if (inst == NULL || geom == NULL)
        return -1;
    if (geom->radius <= 0.0f || geom->half_len <= 0.0f || geom->half_width <= 0.0f)
        return -1;

    LibChassisConfig_s cfg = {0};
    cfg.num = LIB_CHASSIS_WHEEL_4NUM;

    /* 轮序 LF/LB/RB/RF：顺时针绕车一圈 */
    const float hl = geom->half_len;
    const float hw = geom->half_width;
    const float px[LIB_CHASSIS_WHEEL_4NUM] = {hl, -hl, -hl, hl};
    const float py[LIB_CHASSIS_WHEEL_4NUM] = {hw, hw, -hw, -hw};

    for (uint8_t i = 0; i < LIB_CHASSIS_WHEEL_4NUM; i++)
    {
        float x = px[i];
        float y = py[i];

        cfg.wheel[i].x = x;
        cfg.wheel[i].y = y;
        cfg.wheel[i].mount_angle = Lib_Math_Atan2(-x, y); // 切向：轮面方向 (y, -x)
        cfg.wheel[i].roller_angle = M_PI_2;               // 无斜滚子
        cfg.wheel[i].radius = geom->radius;
        cfg.wheel[i].kind = LIB_CHASSIS_WHEEL_OMNI;
    }

    return LibChassisInit(inst, &cfg);
}

#endif /* LIB_CHASSIS_OMNI_X_USED */
