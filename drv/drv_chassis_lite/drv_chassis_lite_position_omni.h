/**
 * @file drv_chassis_lite_position_omni.h
 * @brief 轻量级全向/麦轮底盘运动学（纯计算，不依赖电机）
 * @author TRW
 * @date 2026-09-13
 *
 * @note 支持轮系：OMNI_T / OMNI_X / MECANUM_O / MECANUM_X
 *       舵轮见 drv_chassis_lite_position_half_rudder / drv_chassis_lite_position_all_rudder
 *
 * @note 坐标系：x+向前, y+向左, w+逆时针；轮序 [LF, LB, RB, RF]
 * @note 本模块不做归一化：全向/麦轮的轮速是矢量分解结果，不存在舵角多解
 * @note 单位：ChassisLitePositionRef_t.drive_speed 为电机侧角速度 [rad/s]
 *
 * @note 轻量封装：ChassisLitePositionOmniInverse 只算不解电机，结果写进 ChassisLitePositionRef_t；
 *       正解所需轮速由 app 层读好后传入，由 app 手动调用 MotorSetRef 下发
 */

#ifndef __DRV_CHASSIS_LITE_POSITION_OMNI_H
#define __DRV_CHASSIS_LITE_POSITION_OMNI_H

#include <stdint.h>
#include "drv_chassis_def.h"
#include "drv_chassis_lite_def.h"

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    float wheel_radius;         // 轮子半径 (m)
    float reduction_ratio;      // 电机减速比
    float x;                    // 矩形前后距离 (m)
    float y;                    // 矩形左右距离 (m)
    ChassisType_e chassis_type; // 轮系，仅 OMNI_T / OMNI_X / MECANUM_O / MECANUM_X
} ChassisLitePositionOmni_Cfg_s;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    float wheel_radius;         // 轮子半径 (m)
    float reduction_ratio;      // 电机减速比
    float x;                    // 矩形前后距离 (m)
    float y;                    // 矩形左右距离 (m)
    float r;                    // 着地点到中心距离 (m)，Init 自动计算
    ChassisType_e chassis_type; // 轮系
} ChassisLitePositionOmniInstance_t;

/*============================================
 *              实例定义宏
 *============================================*/
#define CHASSIS_LITE_POSITION_OMNI_INSTANCE_DEF(name) \
    static ChassisLitePositionOmniInstance_t name = {0}

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief  初始化底盘运动学实例
 * @param  inst 实例指针
 * @param  cfg  配置参数（尺寸/轮径/轮系）
 * @retval 0    成功
 * @retval <0   失败：-1 空指针 / -2 轮径非法 / -3 减速比非法 / -4 尺寸非法 / -5 轮系不支持
 */
int8_t ChassisLitePositionOmniInit(ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionOmni_Cfg_s *cfg);

/**
 * @brief  底盘逆解: 速度指令 → 4 个轮子的目标驱动速度
 * @param  inst 实例指针
 * @param  cmd  速度指令 (vx, vy, w)，传值；x+向前, y+向左, w+逆时针
 * @param  out  解算结果；写入 drive_speed[WHEEL_NUM] 与 valid（steer_target 置 0）
 * @note   不读 state，不碰电机
 */
void ChassisLitePositionOmniInverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out);

/**
 * @brief  底盘正解: 4 个轮子的当前转速 → 估算底盘速度
 * @param  inst 实例指针
 * @param  in   反馈，仅使用 drive_speed[WHEEL_NUM]（电机侧 [rad/s]）
 * @return 估算的底盘速度 (vx, vy, w)
 */
ChassisCmd_t ChassisLitePositionOmniForward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in);

#endif // __DRV_CHASSIS_LITE_POSITION_OMNI_H
