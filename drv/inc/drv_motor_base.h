/**
 * @file drv_motor_base.h
 * @brief 电机抽象基类定义
 *
 * @note DRV 层职责：
 *       1. 定义统一的电机接口（虚函数表）
 *       2. 支持多态调用不同类型电机
 *       3. 不使用 FreeRTOS
 */

#ifndef __DRV_MOTOR_BASE_H
#define __DRV_MOTOR_BASE_H

#include <stdint.h>

/*============================================
 *              电机类型枚举
 *============================================*/

typedef enum
{
    MOTOR_TYPE_DC = 0, // 直流有刷电机
    MOTOR_TYPE_DJI,    // 大疆电机 (M3508/M2006/GM6020)
    MOTOR_TYPE_DM,     // 达妙电机 (DM4310等)
    MOTOR_TYPE_LK,     // 龙科电机 (LK9025)
} MotorType_e;

/*============================================
 *              闭环类型枚举
 *============================================*/

typedef enum
{
    MOTOR_LOOP_OPEN = 0,
    MOTOR_LOOP_CURRENT = 0x01,
    MOTOR_LOOP_SPEED = 0x02,
    MOTOR_LOOP_ANGLE = 0x04,
} MotorLoopType_e;

/*============================================
 *              电机状态结构体
 *============================================*/

typedef struct
{
    float speed;       // 当前速度 (rad/s)
    float angle;       // 当前角度 (rad)
    float current;     // 当前电流/扭矩 (A 或 Nm)
    float temperature; // 温度 (C)
    uint8_t online;    // 在线状态 (1=在线, 0=离线)
} MotorStatus_t;

/*============================================
 *              前向声明
 *============================================*/

typedef struct MotorInstance MotorInstance;

/*============================================
 *              电机虚函数表
 *============================================*/

/**
 * @brief 电机虚函数表
 * @note 用于实现多态，不同类型电机实现各自的接口函数
 *       dt 由各电机驱动内部计算
 */
typedef struct
{
    void (*enable)(MotorInstance *inst);
    void (*stop)(MotorInstance *inst);
    void (*set_ref)(MotorInstance *inst, float ref);
    void (*set_speed)(MotorInstance *inst, float speed);
    void (*set_outer_loop)(MotorInstance *inst, MotorLoopType_e loop);
    void (*get_status)(MotorInstance *inst, MotorStatus_t *status);
} MotorInterface_s;

/*============================================
 *              电机实例基类
 *============================================*/

/**
 * @brief 电机实例基类
 * @note 具体电机类型应将此结构体放在首位以实现继承
 *       例如: typedef struct { MotorInstance base; ... } DCMotorInstance;
 */
struct MotorInstance
{
    const MotorInterface_s *vtable; // 虚函数表指针
    void *impl;                     // 具体实现实例指针（可选）
    MotorType_e type;               // 电机类型
    MotorStatus_t status;           // 电机状态（缓存）
};

/*============================================
 *              内联辅助函数
 *============================================*/

/**
 * @brief 使能电机
 */
static inline void MotorEnable(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->enable)
    {
        inst->vtable->enable(inst);
    }
}

/**
 * @brief 停止电机
 */
static inline void MotorStop(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->stop)
    {
        inst->vtable->stop(inst);
    }
}

/**
 * @brief 设置参考值
 */
static inline void MotorSetRef(MotorInstance *inst, float ref)
{
    if (inst && inst->vtable && inst->vtable->set_ref)
    {
        inst->vtable->set_ref(inst, ref);
    }
}

/**
 * @brief 设置速度（PID 控制）
 * @note dt 由电机驱动内部计算
 */
static inline void MotorSetSpeed(MotorInstance *inst, float speed)
{
    if (inst && inst->vtable && inst->vtable->set_speed)
    {
        inst->vtable->set_speed(inst, speed);
    }
}

/**
 * @brief 设置外层闭环类型
 */
static inline void MotorSetOuterLoop(MotorInstance *inst, MotorLoopType_e loop)
{
    if (inst && inst->vtable && inst->vtable->set_outer_loop)
    {
        inst->vtable->set_outer_loop(inst, loop);
    }
}

/**
 * @brief 获取电机状态
 */
static inline void MotorGetStatus(MotorInstance *inst, MotorStatus_t *status)
{
    if (inst && inst->vtable && inst->vtable->get_status)
    {
        inst->vtable->get_status(inst, status);
    }
    else if (inst && status)
    {
        *status = inst->status;
    }
}

#endif // __DRV_MOTOR_BASE_H