/**
 * @file lib_chassis_all_rudder.h
 * @brief 全舵底盘（4 组舵轮，矩形四角）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note "全舵" = 4 组舵轮各占矩形一角，每组 1 个舵向电机 + 1 个驱动电机。
 *       所有舵轮位置对称，转向角互不相关，故只需"半轴距/半轮距"两个尺寸。
 *       正/逆解与力分配全部走 lib_chassis 内核（见 lib_chassis.h）。坐标系 x+前 y+左 w+逆时针。
 * @note 轮序沿用四轮约定 LF/LB/RB/RF（见 LibChassisWheel4_e）。
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_ALL_RUDDER_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_ALL_RUDDER_H
#define __LIB_CHASSIS_ALL_RUDDER_H

#include <stdint.h>
#include "lib_chassis.h"
#include "lib_chassis_def.h"

/*============================================
 *              几何配置
 *============================================*/
typedef struct
{
    float half_len;   // 回转中心到车体中心的前后距离 (m)，前=+x
    float half_width; // 回转中心到车体中心的左右距离 (m)，左=+y
    float radius;     // 驱动轮半径 (m)，>0
} LibChassisAllRudderGeom_s;

/**
 * @brief 初始化全舵底盘内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 * @note 轮位按矩形四角对称布置：LF(+half_len,+half_width)、LB(-,+)、RB(-,-)、RF(+,-)
 */
int8_t LibChassisAllRudderInit(LibChassisInstance_s *inst, const LibChassisAllRudderGeom_s *geom);

#endif // !__LIB_CHASSIS_ALL_RUDDER_H
