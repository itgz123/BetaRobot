#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_base.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "bsp_math.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DJIMotorInstance DJIMotorInstance;

/*============================================
 *              数据结构体
 *============================================*/
typedef struct
{
    DJIMotorInstance *motors[4]; // 组内4个电机指针
    uint8_t motor_init_flag[4];  // 电机是否初始化标志
} DJIMotorSendGroup_s;

/**
 * @brief DJI 电机 CAN 帧联合体
 * @note CAN 总线为大端字节序（MSB first）
 *       发送帧：4 通道 × int16_t 电流原始值（大端）
 *       接收帧：角度/速度/电流/温度/错误码（大端 int16/int8）
 */
typedef union
{
    uint8_t raw[8]; // 原始字节
    struct          // 发送帧：4通道电流（大端 int16）
    {
        uint8_t ch1_h, ch1_l; // 通道1电流 MSB, LSB
        uint8_t ch2_h, ch2_l; // 通道2电流 MSB, LSB
        uint8_t ch3_h, ch3_l; // 通道3电流 MSB, LSB
        uint8_t ch4_h, ch4_l; // 通道4电流 MSB, LSB
    } tx;                     // 发送布局
    struct                    // 接收帧：DJI 电机反馈（大端 int16）
    {
        uint8_t encoder_h, encoder_l;   // 编码器 (uint16)
        uint8_t velocity_h, velocity_l; // 转速 (int16)
        uint8_t current_h, current_l;   // 电流 (int16)
        int8_t temperature;             // 温度
        uint8_t error_code;             // 错误码
    } rx;                               // 接收布局
} DJIMotorCanFrame_u;

/*============================================
 *              电机参数结构体
 *============================================*/
typedef struct
{
    uint16_t current_max;        // 电流最大值 (原始值)
    float current_max_a;         // 电流最大值 (安培)
    uint16_t encoder_resolution; // 编码器分辨率
    float no_load_speed;         // 空载转速 (rad/s)
} DJIMotorParams_s;

typedef struct
{
    uint16_t raw_encoder;         // 编码器原始值 (0-8191)
    int16_t raw_velocity;         // 速度原始值 (RPM)
    int16_t raw_current;          // 电流原始值
    int8_t raw_temperature_motor; // 线圈温度 (°C)
    uint8_t error_code;           // 错误码
} DJIMotorRawData_s;

typedef struct
{
    float position_single; // 单圈位置 (rad) [0, 2π)
    int64_t position_cnt;  // 圈数（支持负数）
    float position_multi;  // 多圈位置 (rad)
    // 如果增量式 (3508,2006) 使用光电门校准；如果绝对式 (6020) 安装后调零一次即可。
    float position_offset;    // 位置偏置 (rad) 。反馈的是加上偏置的位置
    float speed;              // 速度 (rad/s)
    float last_speed;         // 上次速度 (rad/s)
    float current;            // 电流 (A)
    float temperature;        // 线圈温度 (°C)
    uint64_t last_time_stamp; // 上次接收时间戳
} DJIMotorData_s;             // 速度和位置是转子的速度和位置，不是减速箱输出轴速度和位置

/*============================================
 *              DJI 电机实例结构体
 *============================================*/
struct DJIMotorInstance
{
    MotorBase_s base; // 基类（继承）

    /* DJI 基本属性 */
    uint8_t motor_id; // 电机 ID (1-8)

    /* 数据缓冲 (DJI 特有) */
    DJIMotorRawData_s data_raw;
    DJIMotorData_s data;

    /* 分组发送 (DJI 特有) */
    DJIMotorSendGroup_s *sender_group;
    uint8_t motor_idx_in_group;
};

/*============================================
 *              初始化配置结构体
 *============================================*/
typedef struct
{
    BoardCAN_e can_e;                 // 板载CAN枚举
    DJIModel_e model;                 // 型号
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作
    uint8_t motor_id;                 // 电机 ID (1-8)
    MotorSpeedSrc_e speed_src;        // 速度来源：反馈速度/位置微分
    MotorSpeedLpf_e speed_lpf_enable; // 速度低通滤波使能
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC

    /* 控制器设置 */
    MotorControllerSetting_s controller_setting; // 控制器设置

    /* PID 设置 */
    PID_Init_Config_s pid_speed_setting; // 速度环 PID 设置
    PID_Init_Config_s pid_angle_setting; // 位置环 PID 设置
} DJIMotor_Init_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/

#define DJIMOTOR_INSTANCE_DEF(name)     \
    CAN_INSTANCE_DEF(name##_can);       \
    DAEMON_INSTANCE_DEF(name##_daemon); \
    static DJIMotorInstance name = {    \
        .base.can = &name##_can,        \
        .base.daemon = &name##_daemon,  \
    }

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg);
void DJIMotorEnable(void *inst);
void DJIMotorDisable(void *inst);
void DJIMotorSetRef(void *inst, float ref);
float DJIMotor_GetAngle(void *inst);
float DJIMotor_GetSpeed(void *inst);
float DJIMotor_GetCurrent(void *inst);
void DJIMotor_SetOffset(void *inst, float offset);
void DJIMotorSend(void *inst); // 按照can的接收id分组，只要调用同1组的任意一个电机的发送函数，即可发送整组电机

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
