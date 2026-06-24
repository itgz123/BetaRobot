#ifndef __DRV_CHASSIS_POSITION_LITE_H
#define __DRV_CHASSIS_POSITION_LITE_H

#include <stdint.h>
#include "drv_chassis_def.h"
#include "drv_motor_def.h"

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    float wheel_radius;            // 轮子半径 (m)
    float reduction_ratio;         // 电机减速比
    float x;                       // 矩形前后距离 (m)
    float y;                       // 矩形左右距离 (m)
    ChassisType_e chassis_type;    // 底盘类型
    MotorBase_s *motor[WHEEL_NUM]; // 4个电机实例指针 [LF, LB, RB, RF]
} ChassisPositionLite_Cfg_s;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    float wheel_radius;            // 轮子半径 (m)
    float reduction_ratio;         // 电机减速比
    float x;                       // 矩形前后距离 (m)
    float y;                       // 矩形左右距离 (m)
    float r;                       // 着地点到中心距离 (m)，Register 自动计算
    ChassisType_e chassis_type;    // 底盘类型
    MotorBase_s *motor[WHEEL_NUM]; // 4个电机实例指针 [LF, LB, RB, RF]
} ChassisPositionLiteInstance_t;

/*============================================
 *              实例定义宏
 *============================================*/
#define CHASSIS_POSITION_LITE_INSTANCE_DEF(name) \
    static ChassisPositionLiteInstance_t name = {0}

/*============================================
 *              函数声明
 *============================================*/

/**
 * @brief  注册/初始化底盘实例
 * @param  inst  底盘实例指针
 * @param  cfg   配置参数 (含电机指针、尺寸等)
 * @retval 0     成功
 * @retval <0    失败 (参数错误)
 */
int8_t ChassisPositionLite_Init(ChassisPositionLiteInstance_t *inst, ChassisPositionLite_Cfg_s *cfg);

/**
 * @brief  底盘逆解: 速度指令 → 4个电机参考值
 * @param  inst  底盘实例指针
 * @param  cmd   速度指令 (vx, vy, w)，传值
 * @note   x+向前, y+向左, w+逆时针
 */
void ChassisPositionLite_Inverse(ChassisPositionLiteInstance_t *inst, ChassisCmd_t cmd);

/**
 * @brief  底盘正解: 4个电机当前速度 → 估算底盘速度
 * @param  inst  底盘实例指针
 * @return 估算的底盘速度 (vx, vy, w)
 * @note   调用 MotorGetSpeed 读取电机速度
 */
ChassisCmd_t ChassisPositionLite_Forward(ChassisPositionLiteInstance_t *inst);

#endif // __DRV_CHASSIS_POSITION_LITE_H
