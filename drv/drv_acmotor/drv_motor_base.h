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
 *              电机品牌枚举
 *============================================*/

typedef enum
{
    MOTOR_BRAND_NONE = 0,
    MOTOR_BRAND_DJI, // 大疆
    MOTOR_BRAND_DM,  // 达妙
    MOTOR_BRAND_LK,  // 龙科
    MOTOR_BRAND_HT,  // 海泰
} MotorBrand_e;

/*============================================
 *              DJI 电机型号枚举
 *============================================*/

typedef enum
{
    DJI_MODEL_NONE = 0,
    DJI_MODEL_M3508,  // C620 电调
    DJI_MODEL_M2006,  // C610 电调
    DJI_MODEL_GM6020, // 云台电机
} DJIModel_e;

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
typedef struct MotorGroupInstance MotorGroupInstance;

/*============================================
 *              单电机虚函数表
 *============================================*/

typedef struct
{
    int8_t (*reg)(MotorInstance *inst);  // 注册函数
    int8_t (*init)(MotorInstance *inst); // 初始化函数
    void (*enable)(MotorInstance *inst);
    void (*stop)(MotorInstance *inst);
    void (*set_ref)(MotorInstance *inst, float ref);
    void (*set_outer_loop)(MotorInstance *inst, MotorLoopType_e loop);
    void (*get_status)(MotorInstance *inst, MotorStatus_t *status);
    int8_t (*set_pid)(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);
    void (*change_feedback)(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src);
    void (*control)(MotorInstance *inst); // 控制并发送
} MotorInterface_s;

/*============================================
 *              电机组虚函数表
 *============================================*/

typedef struct
{
    int8_t (*reg)(MotorGroupInstance *inst); // 注册函数
    int8_t (*init)(MotorGroupInstance *inst);
    void (*enable)(MotorGroupInstance *inst, uint8_t motor_idx);
    void (*stop)(MotorGroupInstance *inst, uint8_t motor_idx);
    void (*set_ref)(MotorGroupInstance *inst, uint8_t motor_idx, float ref);
    void (*get_status)(MotorGroupInstance *inst, uint8_t motor_idx, MotorStatus_t *status);
    void (*control)(MotorGroupInstance *inst); // 统一发送
} MotorGroupInterface_s;

/*============================================
 *              单电机实例基类
 *============================================*/

struct MotorInstance
{
    const MotorInterface_s *vtable; // 虚函数表指针
    MotorBrand_e brand;             // 品牌
    uint8_t model;                  // 型号
    MotorStatus_t status;           // 电机状态
    MotorControlSetting_t settings; // 控制设置
    MotorController_t controller;   // 控制器
    float dt;                       // 控制周期
    void *priv;                     // 私有数据指针
};

/*============================================
 *              电机组实例基类
 *============================================*/

struct MotorGroupInstance
{
    const MotorGroupInterface_s *vtable; // 虚函数表指针
    MotorBrand_e brand;                  // 品牌
    uint8_t model;                       // 型号
    uint8_t motor_count;                 // 电机数量 (1-4)
    MotorStatus_t status[4];             // 4 个电机状态
    MotorController_t controller[4];     // 4 个控制器
    float dt;                            // 控制周期
    void *priv;                          // 私有数据指针
};

/*============================================
 *              基类统一接口
 *============================================*/

/**
 * @brief 串级 PID 计算
 * @param inst 电机实例指针
 * @param dt   时间间隔 (秒)
 * @return 控制输出
 */
float MotorCascadePID(MotorInstance *inst, float dt);

/**
 * @brief 设置 PID 参数
 */
int8_t MotorSetPID(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);

/*============================================
 *              单电机内联辅助函数
 *============================================*/

static inline int8_t MotorRegister(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->reg)
        return inst->vtable->reg(inst);
    return -1;
}

static inline void MotorEnable(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->enable)
        inst->vtable->enable(inst);
    if (inst)
        inst->status.enable = 1;
}

static inline void MotorStop(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->stop)
        inst->vtable->stop(inst);
    if (inst)
        inst->status.enable = 0;
}

static inline void MotorSetRef(MotorInstance *inst, float ref)
{
    if (inst && inst->vtable && inst->vtable->set_ref)
        inst->vtable->set_ref(inst, ref);
    else if (inst)
        inst->controller.pid_ref = ref;
}

static inline void MotorSetOuterLoop(MotorInstance *inst, MotorLoopType_e loop)
{
    if (inst && inst->vtable && inst->vtable->set_outer_loop)
        inst->vtable->set_outer_loop(inst, loop);
    else if (inst)
        inst->settings.outer_loop_type = loop;
}

static inline void MotorGetStatus(MotorInstance *inst, MotorStatus_t *status)
{
    if (inst && inst->vtable && inst->vtable->get_status)
        inst->vtable->get_status(inst, status);
    else if (inst && status)
        *status = inst->status;
}

static inline void MotorControl(MotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->control)
        inst->vtable->control(inst);
}

static inline uint8_t MotorIsOnline(MotorInstance *inst)
{
    return (inst) ? inst->status.online : 0;
}

/*============================================
 *              电机组内联辅助函数
 *============================================*/

static inline int8_t MotorGroupRegister(MotorGroupInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->reg)
        return inst->vtable->reg(inst);
    return -1;
}

static inline void MotorGroupEnable(MotorGroupInstance *inst, uint8_t motor_idx)
{
    if (inst && inst->vtable && inst->vtable->enable)
        inst->vtable->enable(inst, motor_idx);
    if (inst && motor_idx < 4)
        inst->status[motor_idx].enable = 1;
}

static inline void MotorGroupStop(MotorGroupInstance *inst, uint8_t motor_idx)
{
    if (inst && inst->vtable && inst->vtable->stop)
        inst->vtable->stop(inst, motor_idx);
    if (inst && motor_idx < 4)
        inst->status[motor_idx].enable = 0;
}

static inline void MotorGroupSetRef(MotorGroupInstance *inst, uint8_t motor_idx, float ref)
{
    if (inst && inst->vtable && inst->vtable->set_ref)
        inst->vtable->set_ref(inst, motor_idx, ref);
    else if (inst && motor_idx < 4)
        inst->controller[motor_idx].pid_ref = ref;
}

static inline void MotorGroupGetStatus(MotorGroupInstance *inst, uint8_t motor_idx, MotorStatus_t *status)
{
    if (inst && inst->vtable && inst->vtable->get_status)
        inst->vtable->get_status(inst, motor_idx, status);
    else if (inst && status && motor_idx < 4)
        *status = inst->status[motor_idx];
}

static inline void MotorGroupControl(MotorGroupInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->control)
        inst->vtable->control(inst);
}

#endif // __DRV_MOTOR_BASE_H
