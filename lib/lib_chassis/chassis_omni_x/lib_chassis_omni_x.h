/**
 * @file lib_chassis_omni_x.h
 * @brief 全向轮 X 型（4 个全向轮，矩形四角切向布置）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note X 型 = 4 个全向轮装在矩形四角，轮面沿"以车心为圆心的切线"方向，
 *       四个驱动方向在俯视下构成 X 字。正/逆解与力分配走 lib_chassis 内核。
 * @note 安装角 α_i = atan2(-x_i, y_i)，即轮面方向为 (y_i, -x_i)（顺时针切向）。
 *       若实车转向相反，把 lib_chassis_omni_x.c 里的 atan2 两个参数对调即可。
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_OMNI_X_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_OMNI_X_H
#define __LIB_CHASSIS_OMNI_X_H

#include <stdint.h>
#include "lib_chassis.h"
#include "lib_chassis_def.h"

/*============================================
 *              几何配置
 *============================================*/
typedef struct
{
    float half_len;   // 轮心到车体中心的前后距离 (m)，前=+x
    float half_width; // 轮心到车体中心的左右距离 (m)，左=+y
    float radius;     // 轮子半径 (m)，>0
} LibChassisOmniXGeom_s;

/**
 * @brief 初始化全向轮 X 型内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 * @note 轮位 LF(+hl,+hw)、LB(-hl,+hw)、RB(-hl,-hw)、RF(+hl,-hw)
 */
int8_t LibChassisOmniXInit(LibChassisInstance_s *inst, const LibChassisOmniXGeom_s *geom);

#endif // !__LIB_CHASSIS_OMNI_X_H
