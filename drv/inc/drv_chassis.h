/**
 * @file drv_chassis.h
 * @brief 底盘运动学解算驱动模块
 *
 * @note DRV 层职责：
 *       1. 提供运动学正逆解纯函数
 *       2. 不使用 FreeRTOS
 *       3. 底盘类型由 app_cfg.h 中的 CHASSIS_TYPE 宏编译时确定
 */

#ifndef __DRV_CHASSIS_H
#define __DRV_CHASSIS_H

#include <stdint.h>

/*============================================
 *              结构体定义
 *============================================*/

/**

 * @brief 底盘模式枚举

 */

typedef enum
{
    CHASSIS_MODE_DISABLE = 0, // 失能
    CHASSIS_MODE_ENABLE,      // 使能
    CHASSIS_MODE_HEADLESS,    // 无头模式
} ChassisMode_e;

/**

 * @brief 底盘速度命令结构体

 * @note  用于 app_cmd 向 app_chassis 传递速度命令

 */

typedef struct
{
    float vx;           // 前向速度 (m/s)，向前为正
    float vy;           // 横向速度 (m/s)，向左为正
    float w;            // 角速度 (rad/s)，逆时针为正
    ChassisMode_e mode; // 模式
} ChassisVelCmd_t;

/**
 * @brief 轮子速度结构体
 * @note  运动学逆解输出，电机1-4对应左前、左后、右后、右前
 */
typedef struct
{
    float w1; // 左前轮 LF (rad/s)，面朝轮子逆时针为正
    float w2; // 左后轮 LB (rad/s)
    float w3; // 右后轮 RB (rad/s)
    float w4; // 右前轮 RF (rad/s)
} WheelSpeed_t;

/*============================================
 *              函数声明
 *============================================*/

/**
 * @brief  运动学逆解：底盘速度 -> 4轮速度
 * @param  cmd 底盘速度命令指针
 * @return 4个轮子的角速度
 * @note   根据 CHASSIS_TYPE 宏选择解算方式
 */
WheelSpeed_t ChassisInverseKinematics(ChassisVelCmd_t *cmd);

/**
 * @brief  运动学正解：4轮速度 -> 底盘速度
 * @param  wheel 轮子速度指针
 * @return 底盘速度命令
 * @note   用于姿态闭环，根据 CHASSIS_TYPE 宏选择解算方式
 */
ChassisVelCmd_t ChassisForwardKinematics(WheelSpeed_t *wheel);

#endif // __DRV_CHASSIS_H
