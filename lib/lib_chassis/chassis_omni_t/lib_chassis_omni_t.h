/**
 * @file lib_chassis_omni_t.h
 * @brief 全向轮 T 型（4 个全向轮，矩形四边中点布置）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note T 型 = 4 个全向轮分别装在矩形四条边的中点上，驱动方向沿边的法向。
 *       正/逆解与力分配走 lib_chassis 内核（见 lib_chassis.h）。
 * @note 轮位与安装角（车体系 x+前 y+左）：
 *         LF(+hl,  0)  驱动 -y（向右）
 *         LB( 0, +hw)  驱动 +x（向前）
 *         RB(-hl,  0)  驱动 +y（向左）
 *         RF( 0, -hw)  驱动 -x（向后）
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_OMNI_T_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_OMNI_T_H
#define __LIB_CHASSIS_OMNI_T_H

#include <stdint.h>
#include "lib_chassis.h"
#include "lib_chassis_def.h"

/*============================================
 *              几何配置
 *============================================*/
typedef struct
{
    float half_len;   // 前后轮心到车体中心的前后距离 (m)，前=+x
    float half_width; // 左右轮心到车体中心的左右距离 (m)，左=+y
    float radius;     // 轮子半径 (m)，>0
} LibChassisOmniTGeom_s;

/**
 * @brief 初始化全向轮 T 型内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 */
int8_t LibChassisOmniTInit(LibChassisInstance_s *inst, const LibChassisOmniTGeom_s *geom);

#endif // !__LIB_CHASSIS_OMNI_T_H
