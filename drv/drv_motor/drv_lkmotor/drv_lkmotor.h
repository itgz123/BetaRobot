/**
 * @file drv_lkmotor.h
 * @brief LK（瓴控/翎控）MF 系列一体化直驱电机驱动
 *
 * @note 协议：一对一 CAN 总线通讯协议 V2.36（标准帧 11 位 ID，默认 1Mbps，8 字节帧）。
 *       CAN ID = 0x140 + 电机ID(1~32)，命令与回复使用同一 ID，无应用层 CRC。
 *
 * @note 控制方式：沿用本项目框架，MCU 端做 PID 级联，通过 0xA1 转矩闭环命令下发 iq。
 *       发送 0xA1 后电机回复「读取电机状态2（0x9C）」格式的帧，因此无需额外轮询。
 *
 * @note ⚠️ 只有 0xA0~0xA8 控制命令与 0x9C 的回复是状态2格式；
 *       0x80/0x88/0x81 的回复是「与主机发送相同」的全零回显，必须过滤，否则位置/速度会被误读为 0。
 *
 * @note MF 为直驱（无减速机），反馈的位置/速度/力矩均为电机轴量，驱动内严禁乘减速比。
 *
 * @note 接口与广播驱动 drv_lkmotor_broadcast.{c,h} 保持一致：iq 分辨率与编码器量纲
 *       对全系列 MF 固定，已常量化（见 .c 顶部），Config 只暴露随型号变的 torque_constant。
 *
 * @note 首版仅支持 MF 系列；MS 系列无转矩闭环（仅开环 power）且无相电流采样，暂不实现。
 */

#ifndef __DRV_LKMOTOR_H
#define __DRV_LKMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_base.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "lib_math.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct LKMotorInstance LKMotorInstance;

/*============================================
 *              状态2 / 控制回复帧联合体
 *
 * 8 字节帧格式（小端）。仅用于文档化布局，解析仍使用移位操作。
 *   D[0]    命令回显（0xA1/0x9C 等）
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
} LK_StatusFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    LK_StatusFrameParts_s parts;
} LK_StatusFrame_u;

/*============================================
 *              模式命令枚举
 * @note 帧格式：DATA[0]=命令码，其余字节 0x00（与 RS/DM 的前7字节0xFF不同）
 *============================================*/
typedef enum : uint8_t
{
    LK_CMD_DISABLE = 0x80,      // 关闭：开启→关闭，清除电机内部圈数及旧指令，仍回复但不动作
    LK_CMD_STOP = 0x81,         // 停止（不清除运行状态，可再次发控制指令）
    LK_CMD_ENABLE = 0x88,       // 开启：关闭→开启（上电默认为开启）
    LK_CMD_CLEAR_ERROR = 0x9B,  // 清除错误标志
    LK_CMD_READ_STATUS2 = 0x9C, // 读取电机状态2（温度/iq/转速/编码器）
    LK_CMD_TORQUE = 0xA1,       // 转矩闭环（仅 MF/MH/MG 实现），DATA[4..5]=iqControl
} LKMotorModeCmd_e;

/*============================================
 *              运行状态枚举
 *============================================*/
typedef enum : uint8_t
{
    LK_STATE_DISABLED = 0, // 已关闭（0x80 后）
    LK_STATE_ENABLED = 1,  // 已开启（0x88 后，上电默认为此）
} LKMotorState_e;

/*============================================
 *              协议映射结构体（实例内部）
 * @note Config 时根据用户配置计算，热路径只做乘加
 *============================================*/
typedef struct
{
    float torque_constant;     // 转矩常数 (Nm/A)，由用户标定（直驱，无减速比）
    float inv_torque_constant; // 1/torque_constant，预计算
} LKMotorProtocolMap_s;

/*============================================
 *              LK 电机实例结构体
 *============================================*/
struct LKMotorInstance
{
    MotorBase_s base; // 基类（必须是第一个成员，VTable依赖）

    /* LK 基本属性 */
    uint8_t motor_id; // 电机 ID 1~32（CAN_ID = 0x140 + motor_id，tx 与 rx 同一 ID）

    /* 协议映射配置 */
    LKMotorProtocolMap_s proto_map;

    /* 特有反馈数据 */
    int8_t temperature;         // 温度 (℃)，DATA[1] int8
    float iq_A;                 // q 轴电流 (A)
    uint8_t error_state;        // 错误状态（预留，0=正常）
    LKMotorState_e motor_state; // 0=关闭 / 1=开启，记录已下达的状态
};

/*============================================
 *              初始化配置结构体
 * @note 可重复调用 LKMotorConfig 运行时修改参数
 * @note CAN ID 由 motor_id 推导，不单独暴露
 *============================================*/
typedef struct
{
    BoardCAN_e can_e; // 板载CAN枚举（用于查找硬件映射）
    LKModel_e model;  // 电机型号（首版 LK_MODEL_MF）
    uint8_t motor_id; // 电机 ID 1~32，can_id 由驱动计算 = 0x140 + motor_id

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
} LKMotor_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define LKMOTOR_INSTANCE_DEF(name)      \
    CAN_INSTANCE_DEF(name##_can);       \
    DAEMON_INSTANCE_DEF(name##_daemon); \
    static LKMotorInstance name = {     \
        .base.can = &name##_can,        \
        .base.daemon = &name##_daemon,  \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t LKMotorRegister(LKMotorInstance *inst);
int8_t LKMotorConfig(LKMotorInstance *inst, LKMotor_Config_s *cfg);
void LKMotor_Enable(void *inst);
void LKMotor_Disable(void *inst);
void LKMotor_SetRef(void *inst, float ref);
void LKMotor_Send(void *inst);
MotorData_s LKMotor_GetData(void *inst);

/* 模式命令（调试用，如 LK_CMD_READ_STATUS2 / LK_CMD_CLEAR_ERROR） */
void LKMotor_SendModeCmd(void *inst, uint8_t cmd);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* __DRV_LKMOTOR_H */
