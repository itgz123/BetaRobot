/**
 * @file drv_rsmotor.h
 * @brief 灵足时代 RS05 准直驱电机驱动（RobStride 05，7.75:1 减速比）
 *
 * @note 协议：CAN MIT 模式（标准帧 11 位 ID），1Mbps，8 字节帧。
 *       ⚠️ 硬前提：RS05 出厂默认私有协议（扩展帧），必须先使用灵足上位机
 *       将 protocol_1 标志位切换为 2（MIT），**重新上电生效**后本驱动方可工作。
 *
 * @note 位置/速度/扭矩范围由灵足上位机设定，本驱动中的参数必须与上位机一致。
 *       默认量程：位置 ±12.57 rad、速度 ±50 rad/s、力矩 ±5.5 Nm。
 *
 * @note 反馈语义为输出轴（7.75:1 减速比已由电机固件折算），驱动内严禁再乘减速比。
 *
 * @note 控制模式：
 *       - 本项目在上位机做 PID 控制（Kp=Kd=0），输出扭矩通过 t_ff 下发
 *       - 使能需发送 0xFC 模式命令，失能需发送 0xFD 停止命令
 */

#ifndef __DRV_RSMOTOR_H
#define __DRV_RSMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_base.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "bsp_math.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct RSMotorInstance RSMotorInstance;

/*============================================
 *              CAN 帧联合体定义
 *
 * RS05 使用 CAN MIT 协议，8 字节帧格式如下。
 * 因存在 12 位跨字节位域，联合体主要用于
 * 文档化帧布局，解析仍使用移位操作。
 *============================================*/

/**
 * @brief RS05 电机 CAN MIT 反馈帧联合体
 *
 * 反馈帧格式（8字节）：
 *   D[0]：电机 canid
 *   D[1..2]：位置 16位 无符号 大端
 *   D[3]：速度高8位 VEL[11:4]
 *   D[4]：bits[7:4]=速度低4位 VEL[3:0], bits[3:0]=扭矩高4位 T[11:8]
 *   D[5]：扭矩低8位 T[7:0]
 *   D[6]：bits[7:6]=模式状态(0 Reset/1 Cali/2 Motor), bit[5]=故障, bit[4]=预警, bits[3:0]=绕组温度高4位 TEMP[11:8]
 *   D[7]：绕组温度低8位 TEMP[7:0]（值 = 温度℃ × 10，解析时需 ÷10）
 */
typedef struct __attribute__((packed))
{
    uint8_t can_id;               // [0]   电机 canid
    uint16_t position_be;         // [1-2] 位置 uint16 大端
    uint8_t vel_hi;               // [3]   速度 VEL[11:4]
    uint8_t vel_lo_and_torque_hi; // [4]   VEL[3:0] | TORQUE[11:8]
    uint8_t torque_lo;            // [5]   扭矩 TORQUE[7:0]
    uint8_t status_and_temp_hi;   // [6]   mode[7:6] | fault[5] | warn[4] | TEMP[11:8]
    uint8_t temp_lo;              // [7]   绕组温度 TEMP[7:0]（= ℃×10）
} RS_FeedbackFrameParts_s;

typedef union
{
    uint8_t raw[8];
    RS_FeedbackFrameParts_s parts;
} RS_FeedbackFrame_u;

/**
 * @brief RS05 电机 MIT 控制帧联合体
 *
 * 控制帧格式（MIT 模式，8字节，与 DM 逐字节相同）：
 *   D[0..1]：p_des 16位 大端
 *   D[2]：v_des[11:4]
 *   D[3]：bits[7:4]=v_des[3:0], bits[3:0]=Kp[11:8]
 *   D[4]：Kp[7:0]
 *   D[5]：Kd[11:4]
 *   D[6]：bits[7:4]=Kd[3:0], bits[3:0]=t_ff[11:8]
 *   D[7]：t_ff[7:0]
 *
 * @note 本项目在上位机做 PID（Kp=Kd=0），仅通过 t_ff 下发扭矩
 */
typedef struct __attribute__((packed))
{
    uint16_t p_des_be;          // [0-1] 位置目标 uint16 大端
    uint8_t v_des_hi;           // [2]   v_des[11:4]
    uint8_t v_des_lo_and_kp_hi; // [3]   v_des[3:0] | kp[11:8]
    uint8_t kp_lo;              // [4]   kp[7:0]
    uint8_t kd_hi;              // [5]   kd[11:4]
    uint8_t kd_lo_and_tff_hi;   // [6]   kd[3:0] | t_ff[11:8]
    uint8_t tff_lo;             // [7]   t_ff[7:0]
} RS_ControlFrameParts_s;

typedef union
{
    uint8_t raw[8];
    RS_ControlFrameParts_s parts;
} RS_ControlFrame_u;

/*============================================
 *              模式命令枚举
 *============================================*/
/**
 * @brief RS05 电机模式命令
 * @note 发送时前7字节填 0xFF，第8字节为命令码（与 DM 相同）
 * @note 改ID命令特殊：Byte6 填新id，Byte7 为命令码
 */
typedef enum : uint8_t
{
    RS_CMD_CLEAR_ERROR = 0xFB,      // 清除电机错误
    RS_CMD_MOTOR_MODE = 0xFC,       // 使能，进入 MIT 控制模式
    RS_CMD_RESET_MODE = 0xFD,       // 停止电机
    RS_CMD_ZERO_POSITION = 0xFE,    // 将当前位置设为编码器零点
    RS_CMD_CHANGE_CAN_ID = 0xFA,    // 修改电机 CANID（调试用，Byte6=新id）
    RS_CMD_CHANGE_MASTER_ID = 0x01, // 修改主机 CANID（调试用，Byte6=新主机id）
} RSMotorModeCmd_e;

/*============================================
 *              模式状态枚举（反馈帧 D[6] bits[7:6]）
 *============================================*/
/**
 * @brief RS05 电机模式状态（反馈帧 D[6] 高2位）
 * @note mode_state == CALI 期间电机不接受 MIT 控制命令，
 *       由操作者确认状态进入 MOTOR 后再使能扭矩。
 */
typedef enum : uint8_t
{
    RS_STATE_RESET = 0, // 复位中
    RS_STATE_CALI = 1,  // 标定中（不接受 MIT 控制）
    RS_STATE_MOTOR = 2, // 正常运行（接受 MIT 控制）
} RSMotorModeState_e;

/*============================================
 *              协议映射结构体（实例内部）
 *============================================*/
/**
 * @brief 协议映射配置（初始化时根据用户配置计算，存入实例）
 * @note  存储用户配置的范围 + 预计算的 scale 因子
 * @note  uint→float: raw * to_float_scale - range  (1 mul + 1 sub)
 * @note  float→uint: (val + range) * to_uint_scale (1 add + 1 mul)
 */
typedef struct
{
    float p_max;   // 位置范围 ±p_max (rad)，必须与灵足上位机一致（默认 12.57）
    float v_range; // 速度范围 ±v_range (rad/s)（默认 50）
    float t_range; // 扭矩范围 ±t_range (Nm)（默认 5.5）

    /* 预计算转换因子 */
    float pos_to_float_scale; // = 2*p_max / 65535    (uint16→float)
    float vel_to_float_scale; // = 2*v_range / 4095   (uint12→float)
    float vel_to_uint_scale;  // = 4095 / (2*v_range)  (float→uint12)
    float t_to_float_scale;   // = 2*t_range / 4095
    float t_to_uint_scale;    // = 4095 / (2*t_range)

    float inv_wrap_span; // = 1.0f / (2.0f * p_max)  穿越跨度倒数，多圈位置检测用
} RSMotorProtocolMap_s;

/*============================================
 *              RS05 电机实例结构体
 *============================================*/
struct RSMotorInstance
{
    MotorBase_s base; // 基类（必须是第一个成员，VTable依赖）

    /* RS 基本属性 */
    uint16_t can_id;    // stm32->motor | tx
    uint16_t master_id; // motor->stm32 | rx

    /* 协议映射配置 */
    RSMotorProtocolMap_s proto_map; // 用户配置范围 + 预计算 scale（初始化时计算）

    /* 特有数据 */
    float temperature_winding; // 绕组温度 (°C)，原始 12 位值 = ℃×10
    uint8_t mode_state;        // 模式状态（RS_STATE_RESET/CALI/MOTOR）
    uint8_t fault_flag;        // 故障标志（1 = 故障）
    uint8_t warn_flag;         // 预警标志（1 = 预警）
    uint8_t error;             // 错误状态码（兼容字段 = fault_flag）
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief RS05 电机配置结构体（Config 函数使用）
 *
 * @note 可重复调用 RSMotorConfig 运行时修改 PID 参数、控制器设置等。
 * @note pos_max/vel_range/t_range 传 0 时自动取 RS05 默认量程
 *       （12.57 rad / 50 rad/s / 5.5 Nm）。
 */
typedef struct
{
    BoardCAN_e can_e;                 // 板载CAN枚举（用于查找硬件映射）
    RSModel_e model;                  // 电机型号（RS_MODEL_RS05）
    uint16_t can_id;                  // stm32->motor | tx
    uint16_t master_id;               // motor->stm32 | rx
    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC
    float position_offset;            // 位置偏置 (rad)，默认 0

    /* 协议映射范围（必须与灵足上位机一致，传 0 取默认） */
    float pos_max;   // 位置范围 ±pos_max (rad)，默认 12.57
    float vel_range; // 速度范围 ±vel_range (rad/s)，默认 50
    float t_range;   // 扭矩范围 ±t_range (Nm)，默认 5.5

    /* 控制器设置 */
    MotorControllerSetting_s controller_setting; // 控制器设置

    /* PID 设置 */
    PID_Init_Config_s pid_speed_setting; // 速度环 PID 设置
    PID_Init_Config_s pid_angle_setting; // 位置环 PID 设置

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作
} RSMotor_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define RSMOTOR_INSTANCE_DEF(name)      \
    CAN_INSTANCE_DEF(name##_can);       \
    DAEMON_INSTANCE_DEF(name##_daemon); \
    static RSMotorInstance name = {     \
        .base.can = &name##_can,        \
        .base.daemon = &name##_daemon,  \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t RSMotorRegister(RSMotorInstance *inst);
int8_t RSMotorConfig(RSMotorInstance *inst, RSMotor_Config_s *cfg);
void RSMotor_Enable(void *inst);
void RSMotor_Disable(void *inst);
void RSMotor_SetRef(void *inst, float ref);
void RSMotor_Send(void *inst);
MotorData_s RSMotor_GetData(void *inst);

/* 模式命令（调试用） */
void RSMotor_SendModeCmd(void *inst, uint8_t cmd);

/* 调试接口：修改电机/主机 CANID（改后需重新 RSMotorConfig 生效） */
void RSMotor_ChangeCanID(void *inst, uint16_t can_id);
void RSMotor_ChangeMasterCanID(void *inst, uint16_t can_id);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* __DRV_RSMOTOR_H */
