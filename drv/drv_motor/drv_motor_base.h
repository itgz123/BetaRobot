/**
 * @file drv_motor_base.h
 * @brief 电机抽象基类定义
 *
 * @note DRV 层职责：
 *       1. 定义统一的电机接口（虚函数表）
 *       2. 支持多态调用不同类型电机
 *       3. 提供串级 PID 控制框架
 *       4. 不使用 FreeRTOS
 */

#ifndef __DRV_MOTOR_BASE_H
#define __DRV_MOTOR_BASE_H

#include <stdint.h>
#include "drv_pid.h"

/*============================================
 *              电机类型枚举
 *============================================*/

typedef enum
{
    MOTOR_TYPE_NONE = 0,
    MOTOR_TYPE_DC, // 直流有刷电机
    MOTOR_TYPE_DJI_M3508,
    MOTOR_TYPE_DJI_M2006,
    MOTOR_TYPE_DJI_GM6020,
    MOTOR_TYPE_DM4310, // 达妙电机
    MOTOR_TYPE_LK9025, // 龙科电机
    MOTOR_TYPE_HT04,   // 海泰电机
} MotorType_e;

/*============================================
 *              闭环类型枚举
 *============================================*/

typedef enum
{
    MOTOR_LOOP_OPEN = 0b0000,
    MOTOR_LOOP_CURRENT = 0b0001,
    MOTOR_LOOP_SPEED = 0b0010,
    MOTOR_LOOP_ANGLE = 0b0100,
} MotorLoopType_e;

/*============================================
 *              反馈来源枚举
 *============================================*/

typedef enum
{
    MOTOR_FEED = 0,
    OTHER_FEED,
} FeedbackSource_e;

/*============================================
 *              前馈类型枚举
 *============================================*/

typedef enum
{
    FEEDFORWARD_NONE = 0b00,
    CURRENT_FEEDFORWARD = 0b01,
    SPEED_FEEDFORWARD = 0b10,
} FeedforwardType_e;

/*============================================
 *              电机状态结构体
 *============================================*/

typedef struct
{
    float angle;       // 当前角度 (rad)
    float speed;       // 当前速度 (rad/s)
    float current;     // 当前电流 (A)
    float temperature; // 温度 (°C)
    uint8_t enable;    // 使能状态 (1=使能, 0=失能)
    uint8_t online;    // 在线状态 (1=在线, 0=离线)
} MotorStatus_t;

/*============================================
 *              电机控制设置结构体
 *============================================*/

typedef struct
{
    MotorLoopType_e outer_loop_type;     // 外层闭环类型
    MotorLoopType_e close_loop_type;     // 启用的闭环组合（位掩码）
    uint8_t motor_reverse;               // 电机正反转标志 (0=正常, 1=反转)
    uint8_t feedback_reverse;            // 反馈量正反标志 (0=正常, 1=反转)
    FeedbackSource_e angle_feedback_src; // 角度反馈来源
    FeedbackSource_e speed_feedback_src; // 速度反馈来源
    FeedforwardType_e feedforward_flag;  // 前馈标志
} MotorControlSetting_t;

/*============================================
 *              电机控制器结构体
 *============================================*/

typedef struct
{
    float *angle_feedback_ptr; // 外部角度反馈指针
    float *speed_feedback_ptr; // 外部速度反馈指针
    float *speed_ff_ptr;       // 速度前馈指针
    float *current_ff_ptr;     // 电流前馈指针

    PIDInstance current_pid; // 电流环 PID
    PIDInstance speed_pid;   // 速度环 PID
    PIDInstance angle_pid;   // 位置环 PID

    float pid_ref; // PID 参考值
} MotorController_t;

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
 */
typedef struct
{
    int8_t (*init)(MotorInstance *inst);
    void (*enable)(MotorInstance *inst);
    void (*stop)(MotorInstance *inst);
    void (*set_ref)(MotorInstance *inst, float ref);
    void (*set_outer_loop)(MotorInstance *inst, MotorLoopType_e loop);
    void (*get_status)(MotorInstance *inst, MotorStatus_t *status);
    int8_t (*set_pid)(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);
    void (*change_feedback)(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src);
} MotorInterface_s;

/*============================================
 *              电机实例基类
 *============================================*/

/**
 * @brief 电机实例基类
 * @note 具体电机类型应将此结构体放在首位以实现继承
 *       例如: typedef struct { MotorInstance base; ... } DJIMotorInstance;
 */
struct MotorInstance
{
    const MotorInterface_s *vtable; // 虚函数表指针
    void *impl;                     // 具体实现实例指针（可选）
    MotorType_e type;               // 电机类型
    MotorStatus_t status;           // 电机状态（缓存）
    MotorControlSetting_t settings; // 控制设置
    MotorController_t controller;   // 控制器
    float dt;                       // 控制周期
};

/*============================================
 *              初始化配置结构体
 *============================================*/

typedef struct
{
    MotorType_e type;
    MotorLoopType_e outer_loop_type;
    MotorLoopType_e close_loop_type;
    uint8_t motor_reverse;
    uint8_t feedback_reverse;

    // PID 参数
    float current_kp, current_ki, current_kd;
    float speed_kp, speed_ki, speed_kd;
    float angle_kp, angle_ki, angle_kd;

    // 外部反馈指针
    float *angle_feedback_ptr;
    float *speed_feedback_ptr;
    float *speed_ff_ptr;
    float *current_ff_ptr;
} MotorInitConfig_t;

/*============================================
 *              基类统一接口
 *============================================*/

/**
 * @brief 初始化电机基类
 * @param inst 电机实例指针
 * @param config 初始化配置
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t MotorBaseInit(MotorInstance *inst, const MotorInitConfig_t *config);

/**
 * @brief 设置 PID 参数（统一入口）
 * @param inst           电机实例指针
 * @param loop           闭环类型 (MOTOR_LOOP_SPEED / MOTOR_LOOP_ANGLE)
 * @param kp             比例系数
 * @param ki             积分系数
 * @param kd             微分系数
 * @param integral_limit 积分限幅
 * @param max_out        最大输出
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t MotorSetPID(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);

/**
 * @brief 串级 PID 计算
 * @param inst 电机实例指针
 * @param dt   时间间隔 (秒)
 * @return 控制输出
 */
float MotorCascadePID(MotorInstance *inst, float dt);

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
    if (inst)
    {
        inst->status.enable = 1;
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
    if (inst)
    {
        inst->status.enable = 0;
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
    else if (inst)
    {
        inst->controller.pid_ref = ref;
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
    else if (inst)
    {
        inst->settings.outer_loop_type = loop;
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

/**
 * @brief 切换反馈源
 */
static inline void MotorChangeFeedback(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src)
{
    if (inst && inst->vtable && inst->vtable->change_feedback)
    {
        inst->vtable->change_feedback(inst, loop, src);
    }
    else if (inst)
    {
        if (loop == MOTOR_LOOP_ANGLE)
            inst->settings.angle_feedback_src = src;
        else if (loop == MOTOR_LOOP_SPEED)
            inst->settings.speed_feedback_src = src;
    }
}

/**
 * @brief 检查电机是否在线
 */
static inline uint8_t MotorIsOnline(MotorInstance *inst)
{
    return (inst) ? inst->status.online : 0;
}

/**
 * @brief 检查电机是否使能
 */
static inline uint8_t MotorIsEnabled(MotorInstance *inst)
{
    return (inst) ? inst->status.enable : 0;
}

#endif // __DRV_MOTOR_BASE_H
