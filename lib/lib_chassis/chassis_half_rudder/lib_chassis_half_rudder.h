/**
 * @file lib_chassis_half_rudder.h
 * @brief 半舵底盘（2 组舵轮）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note 半舵 = 左/右各 1 组舵轮，每组 1 个舵向电机 + 1 个驱动电机。
 *       本文件只负责把"左右舵轮回转中心位置"填成内核几何配置；
 *       正/逆解与力分配全部走 lib_chassis 内核（见 lib_chassis.h）。坐标系 x+前 y+左 w+逆时针。
 * @note 轮序 [0]=左舵, [1]=右舵。
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_HALF_RUDDER_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_HALF_RUDDER_H
#define __LIB_CHASSIS_HALF_RUDDER_H

#include <stdint.h>
#include "lib_chassis.h"

#define LIB_CHASSIS_HALF_RUDDER_NUM 2 // 舵轮组数

/** @brief 半舵轮序 */
typedef enum : uint8_t
{
    LIB_CHASSIS_HALF_RUDDER_L = 0, // 左舵
    LIB_CHASSIS_HALF_RUDDER_R,     // 右舵
} LibChassisHalfRudderWheel_e;

/*============================================
 *              几何配置
 *============================================*/
typedef struct
{
    float l_pos_x; // 左舵回转中心 x (m)，前+
    float l_pos_y; // 左舵回转中心 y (m)，左+
    float r_pos_x; // 右舵回转中心 x (m)
    float r_pos_y; // 右舵回转中心 y (m)
    float radius;  // 驱动轮半径 (m)，>0
} LibChassisHalfRudderGeom_s;

/**
 * @brief 初始化半舵底盘内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 * @note 两组回转中心重合会退化（方位不可分辨），Init 不做几何判据，
 *       退化会在 LibChassisForward/AllocateTorque 时由 JᵀJ 判据返回 -1
 */
int8_t LibChassisHalfRudderInit(LibChassisInstance_s *inst, const LibChassisHalfRudderGeom_s *geom);

#endif // !__LIB_CHASSIS_HALF_RUDDER_H
