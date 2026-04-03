/**
 * @file drv_chassis.h
 * @brief 底盘运动学解算驱动模块
 *
 * @note DRV 层职责：
 *       1. 提供运动学正逆解
 *       2. 提供底盘实例接口
 *       3. 不使用 FreeRTOS
 */

#ifndef __DRV_CHASSIS_H
#define __DRV_CHASSIS_H

#include <stdint.h>
#include "drv_motor_base.h"

/*============================================
 *              底盘类型枚举
 *============================================*/

typedef enum
{
    CHASSIS_TYPE_MECANUM_X = 0, // 麦轮 X 型
    CHASSIS_TYPE_MECANUM_O,     // 麦轮 O 型
    CHASSIS_TYPE_OMNI_X,        // 全向轮 X 型
    CHASSIS_TYPE_OMNI_CROSS,    // 全向轮十字型
} ChassisType_e;

/*============================================
 *              底盘模式枚举
 *============================================*/

typedef enum
{
    CHASSIS_MODE_DISABLE = 0, // 失能
    CHASSIS_MODE_ENABLE,      // 使能
    CHASSIS_MODE_HEADLESS,    // 无头模式
} ChassisMode_e;

/*============================================
 *              结构体定义
 *============================================*/

/**
 * @brief 底盘速度命令结构体
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
 * @note  电机1-4对应左前、左后、右后、右前
 */
typedef struct
{
    float w1; // 左前轮 LF (rad/s)
    float w2; // 左后轮 LB (rad/s)
    float w3; // 右后轮 RB (rad/s)
    float w4; // 右前轮 RF (rad/s)
} WheelSpeed_t;

/*============================================
 *              前向声明
 *============================================*/

typedef struct ChassisInstance ChassisInstance;

/*============================================
 *              运动学函数指针类型
 *============================================*/

/**
 * @brief 运动学逆解函数指针类型
 * @param inst 底盘实例指针
 * @param vx   前向速度 (m/s)
 * @param vy   横向速度 (m/s)
 * @param w    角速度 (rad/s)
 * @param out  输出轮子速度
 */
typedef void (*ChassisInverseFunc_t)(ChassisInstance *inst, float vx, float vy, float w, WheelSpeed_t *out);

/**
 * @brief 运动学正解函数指针类型
 * @param inst  底盘实例指针
 * @param wheel 轮子速度指针
 * @return 底盘速度命令
 */
typedef ChassisVelCmd_t (*ChassisForwardFunc_t)(ChassisInstance *inst, WheelSpeed_t *wheel);

/*============================================
 *              底盘实例结构体
 *============================================*/

/**
 * @brief 底盘实例结构体
 */
struct ChassisInstance
{
    // ===== 配置参数 =====
    float wheel_radius; // 轮半径 (m)
    float wheelbase_a;  // 中心到前后轮的纵向距离 (m)
    float wheelbase_b;  // 中心到左右轮的横向距离 (m)
    float chassis_l;    // 中心到轮子距离 (m)
    float cross_a;      // 十字型：中心到前后轮距离 (m)
    float cross_b;      // 十字型：中心到左右轮距离 (m)
    ChassisType_e type; // 底盘类型
    float max_speed;    // 最大线速度 (m/s)
    float max_w;        // 最大角速度 (rad/s)

    // ===== 运动学函数指针 =====
    ChassisInverseFunc_t inverse_kinematics;
    ChassisForwardFunc_t forward_kinematics;

    // ===== 电机指针 =====
    MotorInstance *motors[4]; // 四个电机指针 (LF, LB, RB, RF)

    // ===== 传感器指针 =====
    void *imu; // 姿态传感器指针（预留）（这个也要和电机一样使用多态）

    // ===== 运行状态 =====
    ChassisVelCmd_t vel_cmd;  // 当前速度命令
    WheelSpeed_t wheel_speed; // 当前轮子速度
};

/*============================================
 *              初始化函数声明
 *============================================*/

/**
 * @brief  初始化底盘实例
 * @param  inst           底盘实例指针
 * @param  m1             左前电机指针
 * @param  m2             左后电机指针
 * @param  m3             右后电机指针
 * @param  m4             右前电机指针
 * @param  wheel_radius   轮半径 (m)
 * @param  wheelbase_a    中心到前后轮的纵向距离 (m)
 * @param  wheelbase_b    中心到左右轮的横向距离 (m)
 * @param  chassis_l      中心到轮子距离 (m)
 * @param  cross_a        十字型：中心到前后轮距离 (m)
 * @param  cross_b        十字型：中心到左右轮距离 (m)
 * @param  type           底盘类型
 * @param  max_speed      最大线速度 (m/s)
 * @param  max_w          最大角速度 (rad/s)
 */
void ChassisInit(ChassisInstance *inst, MotorInstance *m1, MotorInstance *m2, MotorInstance *m3, MotorInstance *m4, float wheel_radius, float wheelbase_a, float wheelbase_b, float chassis_l, float cross_a, float cross_b, ChassisType_e type, float max_speed, float max_w);

/*============================================
 *              控制函数声明
 *============================================*/

/**
 * @brief  设置底盘速度
 * @param  inst 底盘实例指针
 * @param  cmd  底盘速度命令指针
 */
void ChassisSetVel(ChassisInstance *inst, ChassisVelCmd_t *cmd);

/**
 * @brief  停止底盘
 * @param  inst 底盘实例指针
 */
void ChassisStop(ChassisInstance *inst);

#endif // __DRV_CHASSIS_H
