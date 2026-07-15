/**
 * @file drv_motor_base.h
 * @brief 电机驱动基类：虚函数表、统一接口宏、通用枚举/结构体
 *
 * @note MotorBase_s 必须是所有电机派生类结构体的第一个成员，VTable 依赖此约定。
 */

#ifndef DRV_MOTOR_BASE_H
#define DRV_MOTOR_BASE_H

#include "stdint.h"
#include "bsp_can.h"
#include "drv_daemon.h"
#include "drv_pid.h"

/*============================================
 *              电机品牌枚举
 *============================================*/
typedef enum : uint8_t
{
    MOTOR_BRAND_OTHER = 0, // 其他电机
    MOTOR_BRAND_DJI,       // DJI电机（大疆）
    MOTOR_BRAND_DM,        // DM电机（达妙）
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
 *              DM电机型号枚举
 *============================================*/
typedef enum : uint8_t
{
    DM_MODEL_DM4310 = 0, // DM4310电机（4310/4310P仅轴承不同，驱动层不区分）
    DM_MODEL_NUM,        // DM电机型号数量
} DMModel_e;

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
 *              统一电机数据结构体
 *
 * MotorGetData() 的返回值类型。
 * position 已含偏置 + 方向修正 + WRAP 归一化。
 * position_single 是原始单圈位置（未修正），可用于调试。
 * torque_current 含义由电机品牌决定（DM: 扭矩Nm, DJI: 电流A）。
 * timestamp_us 来自 DWT 定时器，标记 CAN 帧到达时刻。
 *============================================*/
typedef struct
{
    float position_single; // 原始位置 (rad)，无偏置/方向修正（DJI: [0,2π) 单圈; DM: [-p_max,+p_max] 可跨多圈）
    float position;        // 多圈位置 (rad)，已含偏置 + 方向修正 + WRAP 归一化
    float speed;           // 速度 (rad/s)，已含方向修正
    float torque_current;  // 扭矩 (Nm) 或 电流 (A)，已含方向修正
    uint64_t timestamp_us; // CAN 帧到达时间戳 (us)
} MotorData_s;

/*============================================
 *              原始 CAN 帧缓冲区（通用双缓冲）
 *
 * 所有电机品牌共用此结构体存储原始 CAN 数据。
 * RX 回调（ISR）通过 memcpy 写入，GetData（Task）从就绪帧读取。
 * bytes 的前 8 字节为 CAN 数据帧，具体位域由各品牌驱动解析。
 *============================================*/
typedef struct
{
    uint8_t bytes[8];      // 原始 CAN 帧 8 字节
    uint64_t timestamp_us; // DWT 时间戳 (us)
} MotorRawFrame_s;

/*============================================
 *              虚函数表
 *
 * @note send_cmd: 发送模式命令（使能/失能/归零/清除错误等）。
 *       命令码由各电机品牌定义（如 DM: 0xFC=使能, 0xFD=停止）。
 *       DJI 电机无需此接口，可设为 NULL（MotorSendCmd 宏会判空）。
 * @note get_data: 统一数据获取接口，替代 get_angle/get_speed/get_current。
 *       返回 MotorData_s 结构体包含所有反馈数据。
 *============================================*/
typedef struct MotorVTable_s
{
    void (*enable)(void *inst);                   // 使能电机
    void (*disable)(void *inst);                  // 禁用电机
    void (*set_ref)(void *inst, float ref);       // 设置参考值
    void (*send)(void *inst);                     // 发送控制数据
    MotorData_s (*get_data)(void *inst);          // 统一获取所有反馈数据
    void (*set_offset)(void *inst, float offset); // 设置位置偏置
    void (*send_cmd)(void *inst, uint8_t cmd);    // 发送模式命令（可为 NULL）
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
    uint8_t model;        // 电机型号
    MotorEnable_e enable; // 使能标志

    /* 控制器 */
    MotorControllerSetting_s setting; // 控制器设置
    MotorController_s controller;     // 控制器

    /* 数据 */
    // 原始数据双缓冲（ISR 写入，GetData 读取）
    MotorRawFrame_s raw_frames[2];  // 双缓冲：ISR 写一个，GetData 读另一个
    volatile uint8_t raw_frame_idx; // 当前 ISR 写入的缓冲区索引 (0/1)
    //
    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC
    float position_offset;            // 位置偏置 (rad)，由 MotorSetOffset 写入
    // 解析数据
    MotorData_s data;
    // 上次数据
    float position_single_last; // 上次单圈位置 (rad)，用于 wraps 检测
    float position_multi_last;  // 上次多圈位置 (rad)，用于微分求速度
    int64_t position_cnt;       // 边界穿越累加（DJI: 机械圈数; DM: 协议边界数，每跨 ±p_max 一次）
    float speed_last;           // 上次滤波后速度 (rad/s)
    uint64_t timestamp_last_us; // 上次处理的时间戳 (us)

    /* 统一接口 */
    CANInstance *can;       // CAN 实例指针
    DaemonInstance *daemon; // 守护进程实例
} MotorBase_s;

/*============================================
 *              电机统一接口内联函数
 *
 * 替代原宏定义，通过 MotorBase_s* 类型提供编译期类型检查，
 * 并在运行时检查 inst 非空。
 *
 * @note 所有电机派生类的第一个成员必须是 MotorBase_s base。
 * @note MotorDisable 后应调用 PIDReset。
 * @note DaemonCallback 需重新使能电机。
 * @note MotorSendCmd 内部判空（DJI 电机 send_cmd 为 NULL）。
 *============================================*/
/**
 * @brief 电机统一接口宏
 * @param inst 电机实例，可以是基类或者派生类
 * @param ref 设置的参考值
 * @note 使用下面的宏必须确保所有电机派生类的 base 成员都是结构体的第一个成员
 * @note MotorDisable需要调用PIDReset
 * @note DaemonCallback需要重新给电机发送使能
 * @note MotorSetZero对于增量编码器，在初始化时候保持静止或者用光电门的gpio回调中调用MotorSetOffset(inst, -MotorGetAngle(inst))
 * @note MotorSetZero对于绝对式编码器，只要机械安装后读取零点时偏置，然后在初始化MotorSetOffset(inst, offset)固定偏置即可
 * @note MotorSendCmd 用于发送模式命令（使能/失能/归零/清除错误等），
 *       命令码由各电机品牌定义。DJI 电机可将 send_cmd 设为 NULL，宏会判空。
 */
static inline void MotorEnable(MotorBase_s *inst)
{
    if (!inst)
        return;
    inst->vtable->enable(inst);
}

static inline void MotorDisable(MotorBase_s *inst)
{
    if (!inst)
        return;
    inst->vtable->disable(inst);
}

static inline void MotorSetRef(MotorBase_s *inst, float ref)
{
    if (!inst)
        return;
    inst->vtable->set_ref(inst, ref);
}

static inline void MotorSend(MotorBase_s *inst)
{
    if (!inst)
        return;
    inst->vtable->send(inst);
}

/**
 * @brief 统一获取电机所有反馈数据
 * @param inst 电机基类指针
 * @return MotorData_s 结构体，包含所有反馈数据
 * @note 推荐所有新代码使用此接口，一次调用获取全部数据。
 *       旧 MotorGetAngle/Speed/Current 已改为调用此函数的包装。
 */
static inline MotorData_s MotorGetData(MotorBase_s *inst)
{
    MotorData_s zero = {0};
    if (!inst)
        return zero;
    return inst->vtable->get_data(inst);
}

static inline void MotorSetOffset(MotorBase_s *inst, float offset)
{
    if (!inst)
        return;
    inst->vtable->set_offset(inst, offset);
}

static inline void MotorSendCmd(MotorBase_s *inst, uint8_t cmd)
{
    if (!inst)
        return;
    if (inst->vtable->send_cmd)
        inst->vtable->send_cmd(inst, cmd);
}

#endif // DRV_MOTOR_BASE_H
