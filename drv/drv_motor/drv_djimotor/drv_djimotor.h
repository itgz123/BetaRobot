#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_def.h"
#include "drv_daemon.h"
#include "drv_pid.h"
#include "bsp_math.h"

/*============================================
 *              电机参数表（外部声明）
 *============================================*/
extern const MotorParams_s dji_motor_params[DJI_MODEL_NUM];
extern const uint16_t can_tx_id[DJI_MODEL_NUM][2];
extern const uint16_t can_rx_id_base[DJI_MODEL_NUM];

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

typedef struct
{
    uint16_t raw_encoder;         // 编码器原始值 (0-8191)
    int16_t raw_velocity;         // 速度原始值 (RPM)
    int16_t raw_current;          // 电流原始值
    int8_t raw_temperature_motor; // 线圈温度 (°C)
    uint8_t error_code;           // 错误码
} DJIMotorRawData_s;

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
    float current;     // 电流 (A)
    float temperature; // 线圈温度 (°C)

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
    uint8_t model;                    // 型号
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作
    uint8_t motor_id;                 // 电机 ID (1-8)
    MotorSpeedSrc_e speed_src;        // 速度来源：反馈速度/位置微分
    float speed_lpf_rc;               // 速度低通滤波时间常数 RC (0=禁用)

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
void DJIMotorSend(void *inst); // 按照can的接收id分组，只要调用同1组的任意一个电机的发送函数，即可发送整组电机

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
