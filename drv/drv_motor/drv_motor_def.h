#ifndef DRV_MOTOR_DEF_H
#define DRV_MOTOR_DEF_H

#include "stdint.h"
#include "bsp_can.h"
#include "drv_daemon.h"
#include "drv_pid.h"

#define MotorEnable(inst) ((inst)->base.vtable->enable(inst))
#define MotorDisable(inst) ((inst)->base.vtable->disable(inst))
#define MotorSetRef(inst, ref) ((inst)->base.vtable->set_ref(inst, ref))
#define MotorSend(inst) ((inst)->base.vtable->send(inst))

/*============================================
 *              电机品牌枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_BRAND_DJI = 0, // DJI电机
    MOTOR_BRAND_OTHER,   // 预留其他电机
    MOTOR_BRAND_NUM,     // 电机型号数量
} MotorBrand_e;

/*============================================
 *              DJI电机型号枚举
 *============================================*/
typedef enum : uint8_t
{
    DJI_MODEL_M3508 = 0, // C620 电调
    DJI_MODEL_M2006,     // C610 电调
    DJI_MODEL_GM6020,    // GM6020 电机
    DJI_MODEL_NUM,       // DJI电机型号数量
} DJIModel_e;

/*============================================
 *              控制模式枚举 (位掩码)
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_LOOP_OPEN = 0x00,  // 开环：setref → out
    MOTOR_LOOP_SPEED = 0x01, // 速度闭环
    MOTOR_LOOP_ANGLE = 0x02, // 位置闭环
} MotorLoopType_e;

/*============================================
 *              反馈来源枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_FEEDBACK_MOTOR = 0, // 使用电机自身传感器
    MOTOR_FEEDBACK_EXTERNAL,  // 使用外部传感器（如 IMU）
} MotorFeedbackSrc_e;

/*============================================
 *              前馈来源枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_FEEDFORWARD_DISABLE = 0, // 不使用前馈
    MOTOR_FEEDFORWARD_EXTERNAL,    // 使用外部前馈
} MotorFeedforwardSrc_e;

/*============================================
 *              速度来源枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_SPEED_SRC_FEEDBACK = 0, // 使用电机反馈速度
    MOTOR_SPEED_SRC_DIFF = 1,     // 使用位置微分计算速度
} MotorSpeedSrc_e;

/*============================================
 *              电机方向枚举
 *============================================*/
typedef enum : int8_t
{
    MOTOR_DIRECTION_NORMAL = 1,   // 正向
    MOTOR_DIRECTION_REVERSE = -1, // 反向
} MotorDirection_e;

/*============================================
 *              使能枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_DISABLE = 0, // 禁用
    MOTOR_ENABLE = 1,  // 使能
} MotorEnable_e;

/*============================================
 *              位置限位使能枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_ANGLE_LIMIT_DISABLE = 0, // 禁用位置限位
    MOTOR_ANGLE_LIMIT_ENABLE = 1,  // 启用位置限位
} MotorAngleLimit_e;

/*============================================
 *              电机参数结构体
 *============================================*/
typedef struct
{
    uint16_t current_max;        // 电流最大值 (原始值)
    float current_max_a;         // 电流最大值 (安培)
    uint16_t encoder_resolution; // 编码器分辨率
    float no_load_speed;         // 空载转速 (rad/s)
} MotorParams_s;

/*============================================
 *              前向声明
 *============================================*/
typedef struct MotorController_s MotorController_s;
typedef struct MotorControllerSetting_s MotorControllerSetting_s;
typedef struct MotorBase_s MotorBase_s;
typedef struct MotorVTable_s MotorVTable_s;
/*============================================
 *              虚函数表
 *============================================*/
struct MotorVTable_s
{
    void (*enable)(void *inst);             // 使能电机
    void (*disable)(void *inst);            // 禁用电机
    void (*set_ref)(void *inst, float ref); // 设置参考值
    void (*send)(void *inst);               // 发送控制数据
};

/*============================================
 *              控制器设置结构
 *============================================*/
struct MotorControllerSetting_s
{
    /* 控制设置 */
    MotorLoopType_e loop_type;           // 控制模式
    MotorDirection_e motor_direction;    // 电机方向
    MotorDirection_e feedback_direction; // 反馈方向

    /* 位置限位 */
    MotorAngleLimit_e angle_limit_enable; // 位置限位使能
    float angle_limit_min;                // 位置下限
    float angle_limit_max;                // 位置上限

    /* 前馈来源 */
    MotorFeedforwardSrc_e speed_feedforward_src;    // 速度前馈来源
    MotorFeedforwardSrc_e position_feedforward_src; // 位置前馈来源

    /* 外部前馈指针 */
    float *speed_feedforward_ptr;    // 速度前馈指针
    float *position_feedforward_ptr; // 位置前馈指针

    /* 反馈来源 */
    MotorFeedbackSrc_e angle_src; // 角度反馈来源
    MotorFeedbackSrc_e speed_src; // 速度反馈来源

    /* 外部反馈指针 */
    float *angle_external_ptr; // 外部角度反馈指针
    float *speed_external_ptr; // 外部速度反馈指针
};

/*============================================
 *              控制器结构体
 *============================================*/
struct MotorController_s
{
    /* PID 控制器实例 */
    PIDInstance pid_speed; // 速度环 PID 实例
    PIDInstance pid_angle; // 位置环 PID 实例

    /* 控制状态 */
    float ref;    // 当前控制参考值 (速度环: rad/s, 位置环: rad)
    float output; // 控制输出原始值 (用于CAN发送)
};

/*============================================
 *              电机基类结构体
 *============================================*/
struct MotorBase_s
{
    /* 虚函数表 */
    const MotorVTable_s *vtable;

    /* 基本属性 */
    MotorBrand_e brand;   // 电机品牌
    DJIModel_e model;     // 电机型号
    MotorEnable_e enable; // 使能标志

    /* 控制器 */
    MotorControllerSetting_s setting; // 控制器设置
    MotorController_s controller;     // 控制器

    /* 统一接口 */
    CANInstance *can;       // CAN 实例指针
    DaemonInstance *daemon; // 守护进程实例
};

#endif // !DRV_MOTOR_DEF_H
