/**
 * @file drv_acmotor_base.h
 * @brief 交流电机抽象基类定义（CAN 总线智能电机）
 *
 * @note DRV 层职责：
 *       1. 定义统一的电机接口（虚函数表）
 *       2. 支持多态调用不同类型电机
 *       3. 提供串级 PID 控制框架
 *       4. 统一电流模式（控制帧只发电流/力矩）
 *       5. 不使用 FreeRTOS
 *
 * @note 数据流：
 *       CAN中断回调 → 填 raw_data + 转换填 data（含低通滤波）
 *       APP任务 → control() → 限幅 + 打包CAN + 发送
 */

#ifndef __DRV_ACMOTOR_BASE_H
#define __DRV_ACMOTOR_BASE_H

#include <stdint.h>
#include "drv_pid.h"
#include "drv_daemon.h"
#include "bsp_can.h"

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
 *              DM 电机型号枚举
 *============================================*/

typedef enum
{
    DM_MODEL_NONE = 0,
    DM_MODEL_4310, // DM-J4310-2EC V1.2
} DMModel_e;

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
 *              原始数据结构体（CAN回调填入）
 *============================================*/

typedef struct
{
    uint32_t raw_encoder;         // 编码器原始值
    int32_t raw_velocity;         // 原始速度值
    int32_t raw_current;          // 原始电流/力矩值
    int8_t raw_temperature_motor; // 线圈温度 (°C)
    int8_t raw_temperature_mos;   // MOS/驱动温度 (°C)，无此传感器填0
    uint8_t error_code;           // 品牌错误码
    uint8_t fresh;                // 新数据标志
} MotorRawData_t;

/*============================================
 *              处理后数据结构体（SI单位制）
 *============================================*/

typedef struct
{
    float position_single; // 单圈位置 (rad) [0, 2π)
    float position_multi;  // 多圈位置 (rad)
    float speed;           // 速度 (rad/s) — 输出轴
    float torque;          // 力矩 (N·m) — 输出轴
    float current;         // 电流 (A)
    float temperature;     // 线圈温度 (°C)
    float temperature_mos; // MOS/驱动温度 (°C)
    uint16_t error_flags;  // 统一错误位掩码
    uint8_t fresh;         // 新数据标志
} MotorData_t;

/*============================================
 *              限制配置结构体
 *============================================*/

typedef struct
{
    float position_min;            // 位置下限 (rad)
    float position_max;            // 位置上限 (rad)
    float speed_max;               // 速度上限 (rad/s)
    float current_max;             // 电流上限 (A)
    uint8_t position_limit_enable; // 位置限位使能
} MotorLimits_t;

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

    float pid_ref; // PID 参考值（对应最外层闭环的目标值）
} MotorController_t;

/*============================================
 *              前向声明
 *============================================*/

typedef struct ACMotorInstance ACMotorInstance;
typedef struct ACMotor_Init_Config_s ACMotor_Init_Config_s;

/*============================================
 *              单电机初始化配置结构体
 *============================================*/

struct ACMotor_Init_Config_s
{
    // CAN 配置
    BoardCAN_e can_e; // CAN 总线 (CAN_1 / CAN_2)
    uint32_t rx_id;   // 接收 ID
    uint32_t tx_id;   // 发送 ID

    // 电机身份
    MotorBrand_e brand;
    uint8_t model;    // DJI_MODEL_M3508 / DM_MODEL_4310 等
    uint8_t motor_id; // DJI 需要 (1-8)

    // 电机特征
    float reduction_ratio; // 减速比
    float torque_coef;     // 扭矩系数 (N·m/A)

    // 控制设置
    MotorLoopType_e outer_loop_type;
    MotorLoopType_e close_loop_type;
    uint8_t motor_reverse;
    uint8_t feedback_reverse;
    FeedbackSource_e angle_feedback_src;
    FeedbackSource_e speed_feedback_src;
    FeedforwardType_e feedforward_flag;

    // PID 参数（可选，NULL 表示不配置）
    PID_Init_Config_s *current_pid_cfg;
    PID_Init_Config_s *speed_pid_cfg;
    PID_Init_Config_s *angle_pid_cfg;

    // 限制
    MotorLimits_t limits;

    // Daemon 配置
    uint16_t daemon_reload; // 喂狗超时阈值
    DaemonFaultAction_e daemon_fault_action;
    void (*lost_callback)(void *owner); // 离线回调
};

/*============================================
 *              单电机虚函数表
 *============================================*/

typedef struct
{
    int8_t (*register_)(ACMotorInstance *inst, ACMotor_Init_Config_s *config); // 注册+初始化
    void (*enable)(ACMotorInstance *inst);
    void (*disable)(ACMotorInstance *inst);
    void (*set_ref)(ACMotorInstance *inst, float ref);
    void (*send)(ACMotorInstance *inst); // 限幅+发包
} ACMotorInterface_s;

/*============================================
 *              交流电机实例基类
 *============================================*/

struct ACMotorInstance
{
    const ACMotorInterface_s *vtable; // 虚函数表指针

    // 身份
    MotorBrand_e brand; // 品牌
    uint8_t model;      // 型号

    // CAN 实例（指针，兼容 DEF 宏）
    void *can; // CANInstance*，各品牌 cast 使用

    // 数据（CAN回调填入）
    MotorRawData_t raw_data;
    MotorData_t data;      // 当前数据
    MotorData_t data_last; // 上次数据

    // 控制
    MotorControlSetting_t settings;
    MotorController_t controller; // 三环 PID + 反馈/前馈指针

    // 限制
    MotorLimits_t limits;

    // 看门狗（指针，兼容 DEF 宏）
    DaemonInstance *daemon;

    // 电机特征
    float reduction_ratio; // 减速比（DJI M3508=19.2, M2006=36, DM4310=10, 直驱=1）
    float torque_coef;     // 扭矩系数 (N·m/A)

    // 状态
    uint8_t enable;
    float dt;

    // 品牌私有数据
    void *priv;
};

/*============================================
 *              基类公共函数
 *============================================*/

/**
 * @brief 串级 PID 计算
 */
float ACMotorCascadePID(ACMotorInstance *inst, float dt);

/**
 * @brief 设置 PID 参数
 */
int8_t ACMotorSetPID(ACMotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);

/**
 * @brief 应用限制到 pid_ref
 * @note 电流限制钳位、位置限位时清零 pid_ref
 */
void ACMotorApplyLimits(ACMotorInstance *inst);

/**
 * @brief MIT协议：定点→浮点
 * @param x     定点值
 * @param x_min 物理量下限
 * @param x_max 物理量上限
 * @param bits  定点位数
 * @return 浮点物理量
 */
float ACMotorUintToFloat(uint32_t x, float x_min, float x_max, int bits);

/**
 * @brief MIT协议：浮点→定点
 * @param x     浮点物理量
 * @param x_min 物理量下限
 * @param x_max 物理量上限
 * @param bits  定点位数
 * @return 定点值
 */
uint32_t ACMotorFloatToUint(float x, float x_min, float x_max, int bits);

/*============================================
 *              单电机内联辅助函数
 *============================================*/

// 注册（通过 vtable）
static inline int8_t ACMotorRegister(ACMotorInstance *inst, ACMotor_Init_Config_s *config)
{
    if (inst && inst->vtable && inst->vtable->register_)
        return inst->vtable->register_(inst, config);
    return -1;
}

// 使能（通过 vtable）
static inline void ACMotorEnable(ACMotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->enable)
        inst->vtable->enable(inst);
    if (inst)
        inst->enable = 1;
}

// 失能（通过 vtable）
static inline void ACMotorDisable(ACMotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->disable)
        inst->vtable->disable(inst);
    if (inst)
        inst->enable = 0;
}

// 设置参考值（通过 vtable）
static inline void ACMotorSetRef(ACMotorInstance *inst, float ref)
{
    if (inst && inst->vtable && inst->vtable->set_ref)
        inst->vtable->set_ref(inst, ref);
    else if (inst)
        inst->controller.pid_ref = ref;
}

// 发送（通过 vtable）
static inline void ACMotorSend(ACMotorInstance *inst)
{
    if (inst && inst->vtable && inst->vtable->send)
        inst->vtable->send(inst);
}

#endif // __DRV_ACMOTOR_BASE_H
