/**
 * @file drv_lkmotor_broadcast.h
 * @brief LK（瓴控/翎控）MF 系列电机「一拖四 / 广播模式」驱动（协议 V2.35）
 *
 * @note 广播模式与一对一的区别：
 *       - 发送：主控只发一帧 0x280，同一总线最多 4 个电机共用该帧，
 *         电机 id 的数据放在 data[(id-1)*2]（int16 小端）。
 *       - 接收：各电机仍以 0x140 + id 单独回复，载荷为「状态2」小端格式。
 *       - 开启广播模式与波特率需用 LK 上位机（motor tool）配置并保存重启，总线 ≥ 500Kbps。
 *
 * @note 本驱动只实现 0x280 力矩广播：本项目在 MCU 端做 PID 级联，只下发扭矩。
 *       内部 PID 输出为扭矩 Nm，Send 时按 Kt 与 iq 分辨率换算为 int16 原始值并限幅 ±2000。
 *
 * @note 本文件与一对一驱动 drv_lkmotor.{c,h} 完全独立（类型/常量自持），
 *       二者协议不兼容，切勿在同一电机上混用。
 *
 * @note MF 为直驱（无减速机），反馈与下发均为电机轴量，严禁乘减速比。
 */

#ifndef __DRV_LKMOTOR_BROADCAST_H
#define __DRV_LKMOTOR_BROADCAST_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_base.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "lib_math.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct LKMotorBroadcastInstance LKMotorBroadcastInstance;

/*============================================
 *              协议常量
 *============================================*/
#define LK_BROADCAST_SLOTS 4u // 0x280 帧内的槽位数

/*============================================
 *              运行状态枚举
 *============================================*/
typedef enum : uint8_t
{
    BROADCAST_STATE_DISABLED = 0, // 已关闭（0x80 后）
    BROADCAST_STATE_ENABLED = 1,  // 已开启（0x88 后，上电默认为此）
} LKMotorBroadcastState_e;

/*============================================
 *              状态2 回复帧联合体
 *
 * 8 字节帧格式（小端）。仅用于文档化布局，解析仍使用移位操作。
 *   D[0]    命令回显（广播 0x280 的回复推断为 0xA1）
 *   D[1]    温度 int8, 1℃/LSB
 *   D[2..3] iq int16 小端, MF 33/4096 A/LSB
 *   D[4..5] 转速 int16 小端, 1dps/LSB
 *   D[6..7] 编码器 uint16 小端, 整圈 0~65535（实测转一圈恰好回绕一次）
 *============================================*/
#pragma pack(push, 1)
typedef struct
{
    uint8_t echo;         // [0]   命令回显
    int8_t temperature;   // [1]   温度 (℃)
    int16_t iq_le;        // [2-3] iq int16 小端 (A/LSB 见 proto_map)
    int16_t speed_dps_le; // [4-5] 转速 int16 小端 (dps)
    uint16_t encoder_le;  // [6-7] 编码器 uint16 小端
} LKBroadcastStatusFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    LKBroadcastStatusFrameParts_s parts;
} LKBroadcastStatusFrame_u;

/*============================================
 *              组发送结构体
 * @note 一条总线（can_e）一组，槽位 = motor_id-1
 *============================================*/
typedef struct
{
    LKMotorBroadcastInstance *motors[LK_BROADCAST_SLOTS]; // 组内 4 个电机指针
    uint8_t motor_init_flag[LK_BROADCAST_SLOTS];          // 槽位是否已占用
} LKMotorBroadcastSendGroup_s;

/*============================================
 *              协议映射结构体（实例内部）
 * @note Config 时根据用户配置计算，热路径只做乘加
 *============================================*/
typedef struct
{
    float torque_constant;     // 转矩常数 (Nm/A)，由用户标定（直驱，无减速比）
    float inv_torque_constant; // 1/torque_constant，预计算
} LKMotorBroadcastProtocolMap_s;

/*============================================
 *              LK 广播电机实例结构体
 *============================================*/
struct LKMotorBroadcastInstance
{
    MotorBase_s base; // 基类（必须是第一个成员，VTable依赖）

    /* LK 基本属性 */
    uint8_t motor_id; // 电机 ID 1~4

    /* 协议映射配置 */
    LKMotorBroadcastProtocolMap_s proto_map;

    /* 特有反馈数据 */
    int8_t temperature;                  // 温度 (℃)，DATA[1] int8
    float iq_A;                          // q 轴电流 (A)
    uint8_t error_state;                 // 错误状态（预留，0=正常）
    LKMotorBroadcastState_e motor_state; // 0=关闭 / 1=开启，记录已下达的状态

    /* 组发送（同一总线共用一帧 0x280） */
    LKMotorBroadcastSendGroup_s *send_group;
    uint8_t slot; // = motor_id - 1，在 0x280 帧内的槽位
};

/*============================================
 *              初始化配置结构体
 * @note 可重复调用 LKMotorBroadcastConfig 运行时修改参数
 * @note CAN 发送 ID 固定 0x280、回复 ID 由 motor_id 推导，均不单独暴露
 *============================================*/
typedef struct
{
    BoardCAN_e can_e; // 板载CAN枚举（用于查找硬件映射）
    LKModel_e model;  // 电机型号（仅 LK_MODEL_MF）
    uint8_t motor_id; // 电机 ID 1~4（广播模式一拖四），回复 ID = 0x140 + motor_id

    /* 物理量换算参数（iq 分辨率与编码器量纲对全系列 MF 固定，见驱动内常量） */
    float torque_constant; // 转矩常数 (Nm/A)，必须 > 0（直驱，无减速比）

    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC
    float position_offset;            // 位置偏置 (rad)，默认 0

    /* 控制器设置 */
    MotorControllerSetting_s controller_setting;

    /* PID 设置 */
    PID_Init_Config_s pid_speed_setting; // 速度环 PID 设置
    PID_Init_Config_s pid_angle_setting; // 位置环 PID 设置

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作

    uint32_t timeout_ms; // CAN 发送超时(ms)
} LKMotorBroadcast_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define LKMOTOR_BROADCAST_INSTANCE_DEF(name) \
    CAN_INSTANCE_DEF(name##_can);            \
    DAEMON_INSTANCE_DEF(name##_daemon);      \
    static LKMotorBroadcastInstance name = { \
        .base.can = &name##_can,             \
        .base.daemon = &name##_daemon,       \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t LKMotorBroadcastRegister(LKMotorBroadcastInstance *inst);
int8_t LKMotorBroadcastConfig(LKMotorBroadcastInstance *inst, LKMotorBroadcast_Config_s *cfg);
void LKMotorBroadcast_Enable(void *inst);
void LKMotorBroadcast_Disable(void *inst);
void LKMotorBroadcast_SetRef(void *inst, float ref);
void LKMotorBroadcast_Send(void *inst); /* 组发送：同一总线（can_e）上任意一个电机调用一次即可，整组一起发出 0x280 */
MotorData_s LKMotorBroadcast_GetData(void *inst);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* __DRV_LKMOTOR_BROADCAST_H */
