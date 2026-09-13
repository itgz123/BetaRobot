/**
 * @file drv_chassis_lite_position_half_rudder.h
 * @brief 轻量级半舵底盘运动学（2 组舵轮，纯计算，不依赖电机）
 * @author TRW
 * @date 2026-09-13
 *
 * @note "半舵" = 2 组舵轮（左/右），每组 1 个舵向电机 + 1 个驱动电机
 * @note 坐标系：x+向前, y+向左, w+逆时针
 * @note 解算槽位：[0]=左, [1]=右；state.steer_angle 与 out.steer_target 均为
 *       "车体系连续角"（已含安装偏置），到电机侧的换算由 app 负责
 *
 * @note 输出语义（与 ChassisLitePositionRef_t 一致）：
 *       - drive_speed → 驱动轮电机侧角速度 [rad/s]，负值=反转
 *       - steer_target → 目标舵角 [rad]，连续值，取离当前舵角最近的等价方向
 * @note 本模块不闭环：舵向位置环（含积分）由 app 负责
 */

#ifndef __DRV_CHASSIS_LITE_POSITION_HALF_RUDDER_H
#define __DRV_CHASSIS_LITE_POSITION_HALF_RUDDER_H

#include <stdint.h>
#include "drv_chassis_def.h"
#include "drv_chassis_lite_def.h"

#define CHASSIS_LITE_POSITION_HALF_RUDDER_NUM 2 // 舵轮组数（左/右）

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    float wheel_radius;    // 驱动轮半径 (m)
    float reduction_ratio; // 驱动电机减速比
    // 各组舵轮回转中心在车体系中的位置 (m)：x 前+，y 左+
    float pos_x[CHASSIS_LITE_POSITION_HALF_RUDDER_NUM];
    float pos_y[CHASSIS_LITE_POSITION_HALF_RUDDER_NUM];
} ChassisLitePositionHalfRudder_Cfg_s;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    float wheel_radius;                                 // 驱动轮半径 (m)
    float reduction_ratio;                              // 驱动电机减速比
    float pos_x[CHASSIS_LITE_POSITION_HALF_RUDDER_NUM]; // 回转中心 x (m)
    float pos_y[CHASSIS_LITE_POSITION_HALF_RUDDER_NUM]; // 回转中心 y (m)
} ChassisLitePositionHalfRudderInstance_t;

/*============================================
 *              实例定义宏
 *============================================*/
#define CHASSIS_LITE_POSITION_HALF_RUDDER_INSTANCE_DEF(name) \
    static ChassisLitePositionHalfRudderInstance_t name = {0}

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief  初始化半舵底盘运动学实例
 * @retval 0  成功
 * @retval <0 失败：-1 空指针 / -2 轮径非法 / -3 减速比非法 / -4 两组回转中心重合(退化)
 */
int8_t ChassisLitePositionHalfRudderInit(ChassisLitePositionHalfRudderInstance_t *inst, const ChassisLitePositionHalfRudder_Cfg_s *cfg);

/**
 * @brief  底盘逆解: 速度指令 → 2 组的 {目标舵角, 驱动速度}
 * @param  inst 实例指针
 * @param  cmd  速度指令 (vx, vy, w)
 * @param  st   当前状态，使用 steer_angle[0..1]（车体系连续角 [rad]）
 * @param  out  解算结果；仅填 [0]/[1] 两个槽位，其余清零
 * @note   回转中心速度低于阈值时保持当前朝向、驱动置 0
 */
void ChassisLitePositionHalfRudderInverse(const ChassisLitePositionHalfRudderInstance_t *inst, ChassisCmd_t cmd,
                                          const ChassisLiteState_t *st, ChassisLitePositionRef_t *out);

/**
 * @brief  底盘正解: 2 组的 {驱动速度, 当前舵角} → 估算底盘速度（里程计）
 * @param  inst 实例指针
 * @param  in   反馈，使用 drive_speed[0..1]（电机侧 [rad/s]）
 * @param  st   反馈，使用 steer_angle[0..1]（车体系连续角 [rad]）
 * @return 估算的底盘速度 (vx, vy, w)；两组为超定，按最小二乘解
 */
ChassisCmd_t ChassisLitePositionHalfRudderForward(const ChassisLitePositionHalfRudderInstance_t *inst,
                                                  const ChassisLitePositionRef_t *in, const ChassisLiteState_t *st);

#endif // __DRV_CHASSIS_LITE_POSITION_HALF_RUDDER_H
