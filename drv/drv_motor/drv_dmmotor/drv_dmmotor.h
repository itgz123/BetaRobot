/**
 * @file drv_dmmotor.h
 * @brief DM 电机驱动（4310 / 4310P）
 *
 * @note DM 电机使用 CAN MIT 协议，数据采用无符号整数线性映射。
 *       4310 和 4310P 协议完全相同，差异仅在物理参数（转速、扭矩等）。
 *
 * @note 位置/速度/扭矩范围由 DM 调试助手设定，本驱动中的参数必须与调试助手一致。
 *
 * @note 控制模式：
 *       - 本项目在上位机做 PID 控制（Kp=Kd=0），输出扭矩通过 t_ff 下发
 *       - 使能需发送 0xFC 模式命令，失能需发送 0xFD 停止命令
 */

#ifndef __DRV_DMMOTOR_H
#define __DRV_DMMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_base.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "bsp_math.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DMMotorInstance DMMotorInstance;

/*============================================
 *              CAN 帧联合体定义
 *
 * DM 电机使用 CAN MIT 协议，8 字节帧格式如下。
 * 因存在 12 位跨字节位域，联合体主要用于
 * 文档化帧布局，解析仍使用移位操作。
 *============================================*/

/**
 * @brief DM 电机 CAN MIT 反馈帧联合体
 *
 * 反馈帧格式（8字节）：
 *   D[0]：bits[3:0]=电机ID, bits[7:4]=错误状态
 *   D[1..2]：位置 16位 无符号 大端
 *   D[3]：速度高8位 VEL[11:4]
 *   D[4]：bits[7:4]=速度低4位 VEL[3:0], bits[3:0]=扭矩高4位 T[11:8]
 *   D[5]：扭矩低8位 T[7:0]
 *   D[6]：MOS温度 (°C)
 *   D[7]：线圈温度 (°C)
 */
typedef union
{
    uint8_t raw[8];
    struct
    {
        uint8_t id_and_error;         // [0]   bits[7:4]=error, bits[3:0]=id
        uint16_t position_be;         // [1-2] 位置 uint16 大端
        uint8_t vel_hi;               // [3]   速度 VEL[11:4]
        uint8_t vel_lo_and_torque_hi; // [4]   VEL[3:0] | TORQUE[11:8]
        uint8_t torque_lo;            // [5]   扭矩 TORQUE[7:0]
        int8_t temp_mos;              // [6]   MOS 温度 (°C)
        int8_t temp_coil;             // [7]   线圈温度 (°C)
    } parts;
} DM_FeedbackFrame_u;

/**
 * @brief DM 电机 MIT 控制帧联合体
 *
 * 控制帧格式（MIT 模式，8字节）：
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
typedef union
{
    uint8_t raw[8];
    struct
    {
        uint16_t p_des_be;          // [0-1] 位置目标 uint16 大端
        uint8_t v_des_hi;           // [2]   v_des[11:4]
        uint8_t v_des_lo_and_kp_hi; // [3]   v_des[3:0] | kp[11:8]
        uint8_t kp_lo;              // [4]   kp[7:0]
        uint8_t kd_hi;              // [5]   kd[11:4]
        uint8_t kd_lo_and_tff_hi;   // [6]   kd[3:0] | t_ff[11:8]
        uint8_t tff_lo;             // [7]   t_ff[7:0]
    } parts;
} DM_ControlFrame_u;

/*============================================
 *              模式命令枚举
 *============================================*/
/**
 * @brief DM 电机模式命令
 * @note 发送时前7字节填 0xFF，第8字节为命令码
 */
typedef enum : uint8_t
{
    DM_CMD_CLEAR_ERROR = 0xFB,   // 清除电机过热错误
    DM_CMD_MOTOR_MODE = 0xFC,    // 使能，进入 MIT 控制模式
    DM_CMD_RESET_MODE = 0xFD,    // 停止电机
    DM_CMD_ZERO_POSITION = 0xFE, // 将当前位置设为编码器零点
} DMMotorModeCmd_e;

/*============================================
 *              错误状态枚举
 *============================================*/
/**
 * @brief DM 电机错误状态码（反馈帧 D[0] 高4位）
 */
typedef enum : uint8_t
{
    DM_STATE_DISABLED = 0x0,       // 失能
    DM_STATE_ENABLED = 0x1,        // 使能（正常）
    DM_STATE_OVER_VOLTAGE = 0x8,   // 过压
    DM_STATE_UNDER_VOLTAGE = 0x9,  // 欠压
    DM_STATE_OVER_CURRENT = 0xA,   // 过电流
    DM_STATE_MOS_OVER_TEMP = 0xB,  // MOS 过温
    DM_STATE_COIL_OVER_TEMP = 0xC, // 电机线圈过温
    DM_STATE_LOST_COMM = 0xD,      // 通讯丢失
    DM_STATE_OVER_LOAD = 0xE,      // 过载
} DMMotorState_e;

/*============================================
 *              电机参数结构体（硬件极限）
 *============================================*/
/**
 * @brief DM 电机硬件参数（仅速度/扭矩最大极限）
 * @note  只存电机数据手册中的物理上限，不含 scale 因子和位置范围
 */
typedef struct
{
    float v_max; // 电机硬件最大转速 (rad/s)
    float t_max; // 电机硬件最大扭矩 (Nm)
} DMMotorParams_s;

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
    float p_max;   // 位置范围 ±p_max (rad)，必须与 DM 调试助手一致
    float v_range; // 速度范围 ±v_range (rad/s)
    float t_range; // 扭矩范围 ±t_range (Nm)

    /* 预计算转换因子 */
    float pos_to_float_scale; // = 2*p_max / 65535    (uint16→float)
    float vel_to_float_scale; // = 2*v_range / 4095   (uint12→float)
    float vel_to_uint_scale;  // = 4095 / (2*v_range)  (float→uint12)
    float t_to_float_scale;   // = 2*t_range / 4095
    float t_to_uint_scale;    // = 4095 / (2*t_range)

    float inv_wrap_span; // = 1.0f / (2.0f * p_max)  穿越跨度倒数，多圈位置检测用
} DMMotorProtocolMap_s;

/*============================================
 *              DM 电机实例结构体
 *============================================*/
struct DMMotorInstance
{
    MotorBase_s base; // 基类（必须是第一个成员，VTable依赖）

    /* DM 基本属性 */
    uint16_t motor_can_id; // 电机 CAN ID（控制帧目标）
    uint16_t master_id;    // Master ID（反馈帧来源）

    /* 协议映射配置 */
    DMMotorProtocolMap_s proto_map; // 用户配置范围 + 预计算 scale（初始化时计算）

    /* 特有数据 */
    float temperature_mos;  // MOS 温度 (°C)
    float temperature_coil; // 线圈温度 (°C)
    uint8_t error;          // 错误状态码
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief DM 电机配置结构体（Config 函数使用）
 *
 * @note 可重复调用 DMMotorConfig 运行时修改 PID 参数、控制器设置等。
 *       不包含 can_e 和 daemon 相关字段（仅在 Register 时需要）。
 */
typedef struct
{
    DMModel_e model;                  // 电机型号 (DM4310 / DM4310P)
    uint16_t motor_can_id;            // 电机 CAN ID（控制帧发送目标）
    uint16_t master_id;               // Master ID（反馈帧接收ID）
    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC

    /* 协议映射范围（必须与 DM 调试助手一致，≤ 硬件极限） */
    float pos_max;   // 位置范围 ±pos_max (rad)
    float vel_range; // 速度范围 ±vel_range (rad/s)
    float t_range;   // 扭矩范围 ±t_range (Nm)

    /* 控制器设置 */
    MotorControllerSetting_s controller_setting; // 控制器设置

    /* PID 设置 */
    PID_Init_Config_s pid_speed_setting; // 速度环 PID 设置
    PID_Init_Config_s pid_angle_setting; // 位置环 PID 设置
} DMMotor_Config_s;

/**
 * @brief DM 电机注册配置结构体（Register 函数使用）
 *
 * @note 仅调用一次。内嵌 DMMotor_Config_s + CAN/daemon 相关字段。
 */
typedef struct
{
    DMMotor_Config_s motor_config;    // 电机配置（传入 Config）
    BoardCAN_e can_e;                 // 板载CAN枚举
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作
} DMMotor_Register_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define DMMOTOR_INSTANCE_DEF(name)      \
    CAN_INSTANCE_DEF(name##_can);       \
    DAEMON_INSTANCE_DEF(name##_daemon); \
    static DMMotorInstance name = {     \
        .base.can = &name##_can,        \
        .base.daemon = &name##_daemon,  \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t DMMotorConfig(DMMotorInstance *inst, DMMotor_Config_s *cfg);
int8_t DMMotorRegister(DMMotorInstance *inst, const DMMotor_Register_Config_s *reg_cfg);
void DMMotor_Enable(void *inst);
void DMMotor_Disable(void *inst);
void DMMotor_SetRef(void *inst, float ref);
void DMMotor_Send(void *inst);
MotorData_s DMMotor_GetData(void *inst);
void DMMotor_SetOffset(void *inst, float offset);

/* 模式命令（调试用） */
void DMMotor_SendModeCmd(void *inst, uint8_t cmd);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* __DRV_DMMOTOR_H */
