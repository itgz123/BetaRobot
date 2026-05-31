#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_def.h"
#include "drv_daemon.h"
/*============================================
 *              常量定义
 *============================================*/

// 编码器分辨率 (14位)
#define DJI_ENCODER_RESOLUTION 8192

// 电流范围
#define DJI_M3508_CURRENT_MAX 16384
#define DJI_M2006_CURRENT_MAX 10000
#define DJI_GM6020_VOLTAGE_MAX 30000

// 扭矩系数 (N·m/A)
#define DJI_M3508_TORQUE_COEF 0.114f
#define DJI_M2006_TORQUE_COEF 0.18f
#define DJI_GM6020_TORQUE_COEF 0.114f

// 减速比
#define DJI_M3508_REDUCTION_RATIO 6.0f
#define DJI_M2006_REDUCTION_RATIO 36.0f
#define DJI_GM6020_REDUCTION_RATIO 1.0f

/*============================================
 *              DJI 单电机实例结构体
 *============================================*/
typedef struct
{
    uint8_t model;
    CANInstance *can;
    DaemonInstance *daemon;
    float set_ref;
    uint8_t enable;
    uint8_t motor_id; // 电机 ID (1-8)
} DJIMotorInstance;

typedef struct
{
    uint8_t model;                    // 型号
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback lost_callback;   // 异常处理函数（可为NULL）
    uint8_t motor_id;                 // 电机 ID (1-8)
} DJIMotor_Init_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/

#define DJIMOTOR_INSTANCE_DEF(name, can_idx) \
    CAN_INSTANCE_DEF(name##_can, can_idx);   \
    DAEMON_INSTANCE_DEF(name##_daemon);      \
    static DJIMotorInstance name = {         \
        .can = &name##_can,                  \
        .daemon = &name##_daemon,            \
    }

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg);
void DJIMotorEnable(DJIMotorInstance *inst);
void DJIMotorDisable(DJIMotorInstance *inst);
void DJIMotorSetRef(DJIMotorInstance *inst, float ref);
void DJIMotorSend(DJIMotorInstance *inst); // 按照can的接收id分组，只要调用同1组的任意一个电机的发送函数，即可发送整组电机

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
