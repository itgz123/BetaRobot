#ifndef DRV_MOTOR_DEF_H
#define DRV_MOTOR_DEF_H

#include "stdint.h"
#include "bsp_can.h"
#include "drv_daemon.h"
#include "drv_pid.h"

// 使用下面的宏必须确保所有电机派生类的 base 成员都是结构体的第一个成员
#define MotorEnable(inst) (((MotorBase_s *)(inst))->vtable->enable(inst))
#define MotorDisable(inst) (((MotorBase_s *)(inst))->vtable->disable(inst))
#define MotorSetRef(inst, ref) (((MotorBase_s *)(inst))->vtable->set_ref(inst, ref))
#define MotorSend(inst) (((MotorBase_s *)(inst))->vtable->send(inst))
#define MotorGetAngle(inst) (((MotorBase_s *)(inst))->vtable->get_angle(inst))
#define MotorGetSpeed(inst) (((MotorBase_s *)(inst))->vtable->get_speed(inst))

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
 *              位置模式枚举
 *
 * 三种位置模式的行为说明：
 *
 * ┌────────────┬─────────────────────┬─────────────────────┬─────────────────────┬─────────────────────┐
 * │    模式    │       应用场景      │    setref处理       │  PID位置反馈来源    │   get_angle返回    │
 * ├────────────┼─────────────────────┼─────────────────────┼─────────────────────┼─────────────────────┤
 * │  LIMITED   │ pitch轴等有机械限位 │ 多圈位置限幅到      │ multi+offset        │ multi+offset        │
 * │  (限幅)    │ 的关节电机          │ [min, max]          │                     │                     │
 * ├────────────┼─────────────────────┼─────────────────────┼─────────────────────┼─────────────────────┤
 * │  WRAP      │ yaw轴等无限旋转但   │ 多圈位置归一化到    │ multi+offset        │ multi+offset        │
 * │  (环绕)    │ 需归一化处理的电机  │ [min, max]          │ 归一化到[min,max]   │ 归一化到[min,max]   │
 * ├────────────┼─────────────────────┼─────────────────────┼─────────────────────┼─────────────────────┤
 * │ CONTINUOUS │ 轮子电机等需要多圈  │ 不限幅              │ multi+offset        │ multi+offset        │
 * │  (连续)    │ 累积位置的电机      │                     │                     │                     │
 * └────────────┴─────────────────────┴─────────────────────┴─────────────────────┴─────────────────────┘
 *
 * 注：multi = 多圈位置 = cnt*2π + single，offset = 位置偏置
 *     angle_limit_min/max: LIMITED模式作为限幅范围，WRAP模式作为归一化范围
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_POSITION_LIMITED = 0,    // 限幅模式
    MOTOR_POSITION_WRAP = 1,       // 环绕模式
    MOTOR_POSITION_CONTINUOUS = 2, // 连续模式
} MotorPositionMode_e;

/*============================================
 *              速度滤波使能枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_SPEED_LPF_DISABLE = 0, // 禁用速度低通滤波
    MOTOR_SPEED_LPF_ENABLE = 1,  // 启用速度低通滤波
} MotorSpeedLpf_e;

/*============================================
 *              虚函数表
 *============================================*/
typedef struct MotorVTable_s
{
    void (*enable)(void *inst);             // 使能电机
    void (*disable)(void *inst);            // 禁用电机
    void (*set_ref)(void *inst, float ref); // 设置参考值
    void (*send)(void *inst);               // 发送控制数据
    float (*get_angle)(void *inst);         // 获取多圈位置
    float (*get_speed)(void *inst);         // 获取速度
} MotorVTable_s;

/*============================================
 *              控制器结构体
 *============================================*/
typedef struct
{
    /* PID 控制器实例 */
    PIDInstance pid_speed; // 速度环 PID 实例
    PIDInstance pid_angle; // 位置环 PID 实例

    /* 控制状态 */
    float ref;    // 当前控制参考值 (速度环: rad/s, 位置环: rad)
    float output; // 控制输出原始值 (用于CAN发送)
} MotorController_s;

/*============================================
 *              控制器设置结构
 *============================================*/
typedef struct
{
    /* 控制设置 */
    MotorLoopType_e loop_type;           // 控制模式
    MotorDirection_e motor_direction;    // 电机方向
    MotorDirection_e feedback_direction; // 反馈方向

    /* 位置模式 */
    MotorPositionMode_e position_mode; // 位置模式
    float angle_limit_min;             // LIMITED: 限幅下限, WRAP: 归一化下限
    float angle_limit_max;             // LIMITED: 限幅上限, WRAP: 归一化上限

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
} MotorControllerSetting_s;

/*============================================
 *              电机基类结构体
 *============================================*/
typedef struct
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

    /* 速度计算 */
    MotorSpeedSrc_e speed_src;        // 速度来源选择
    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC

    /* 统一接口 */
    CANInstance *can;       // CAN 实例指针
    DaemonInstance *daemon; // 守护进程实例
} MotorBase_s;

#endif // !DRV_MOTOR_DEF_H
