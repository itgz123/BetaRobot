#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_motor_def.h"
#include "drv_daemon.h"
#include "bsp_math.h"

/*============================================
 *              常量定义
 *============================================*/

// 编码器分辨率 (14位)
#define DJI_ENCODER_RESOLUTION 8192
// 电机参数查表
static const struct
{
    uint16_t current_max;    // 电流最大值
    uint16_t can_rx_id_base; // CAN接收ID基地址
    float torque_coef;       // 扭矩系数 (N·m/A)
    float reduction_ratio;   // 减速比
} dji_motor_params[DJI_MODEL_NUM] = {
    [DJI_MODEL_M2006] = {10000, 0x200, 0.0f, 36.0f}, // 未知扭矩系数
    [DJI_MODEL_M3508] = {16384, 0x200, 0.0f, 6.0f},  // 未知扭矩系数
    [DJI_MODEL_GM6020] = {16384, 0x204, 0.741f, 1.0f},
};

const uint16_t can_tx_id[DJI_MODEL_NUM][2] = {
    [DJI_MODEL_M2006][0] = 0x200,  // 1-4号
    [DJI_MODEL_M2006][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_M3508][0] = 0x200,  // 1-4号
    [DJI_MODEL_M3508][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_GM6020][0] = 0x1fe, // 1-4号
    [DJI_MODEL_GM6020][1] = 0x2fe, // 5-7号
};
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
    // 原始数据
    uint16_t raw_encoder;         // 编码器原始值
    int16_t raw_velocity;         // 原始速度值
    int16_t raw_current;          // 原始电流/力矩值
    int8_t raw_temperature_motor; // 线圈温度 (°C)
    uint8_t error_code;           // 品牌错误码
    // 标准单位制数据
    float position_single; // 单圈位置 (rad) [0, 2π)
    float position_multi;  // 多圈位置 (rad)
    float speed;           // 速度 (rad/s)
    float torque;          // 力矩 (N·m)
    float current;         // 电流 (A)
    float temperature;     // 线圈温度 (°C)
    uint16_t error_flags;  // 统一错误码
} DJIMotorInstance;

typedef struct
{
    BoardCAN_e can_e;                 // 板载CAN枚举（注册时用于查找映射）
    uint8_t model;                    // 型号
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback lost_callback;   // 异常处理函数（可为NULL）
    uint8_t motor_id;                 // 电机 ID (1-8)
} DJIMotor_Init_Config_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/

#define DJIMOTOR_INSTANCE_DEF(name)     \
    CAN_INSTANCE_DEF(name##_can);       \
    DAEMON_INSTANCE_DEF(name##_daemon); \
    static DJIMotorInstance name = {    \
        .can = &name##_can,             \
        .daemon = &name##_daemon,       \
    }

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg);
void DJIMotorEnable(DJIMotorInstance *inst);
void DJIMotorDisable(DJIMotorInstance *inst);
void DJIMotorSetRef(DJIMotorInstance *inst, float ref);
void DJIMotorSend(DJIMotorInstance *inst); // 按照can的接收id分组，只要调用同1组的任意一个电机的发送函数，即可发送整组电机

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
