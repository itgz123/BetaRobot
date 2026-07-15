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

/*============================================
 *              DJI 电机实例结构体
 *============================================*/
struct DJIMotorInstance
{
    MotorBase_s base; // 基类（继承）

    /* DJI 基本属性 */
    uint8_t motor_id; // 电机 ID (1-8)

    /* 特有数据 */
    float motor_temperature; // 线圈温度 (°C)
    uint8_t error_code;      // 错误码

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
MotorData_s DJIMotor_GetData(void *inst);
void DJIMotor_SetOffset(void *inst, float offset);
void DJIMotorSend(void *inst); // 按照can的接收id分组，只要调用同1组的任意一个电机的发送函数，即可发送整组电机

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
