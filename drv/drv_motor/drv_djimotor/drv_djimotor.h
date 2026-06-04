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

// 电调电流范围 (原始值)
#define C610_CURRENT_MAX 10000   // C610 (M2006) 电流范围 ±10000 → ±10A
#define C620_CURRENT_MAX 16384   // C620 (M3508) 电流范围 ±16384 → ±20A
#define GM6020_CURRENT_MAX 16384 // GM6020 电流范围 ±16384 → ±3A

// 电流转换系数 (实际电流A / 原始值)
#define C610_CURRENT_SCALE (10.0f / 10000.0f)  // 0.001 A/LSB
#define C620_CURRENT_SCALE (20.0f / 16384.0f)  // ≈0.00122 A/LSB
#define GM6020_CURRENT_SCALE (3.0f / 16384.0f) // ≈0.000183 A/LSB

/*============================================
 *              电机参数表（外部声明）
 *============================================*/
typedef struct
{
    uint16_t current_max;    // 电流最大值 (原始值)
    uint16_t can_rx_id_base; // CAN接收ID基地址
    float torque_coef;       // 扭矩系数 (N·m/A)
    float reduction_ratio;   // 减速比
    float no_load_speed;     // 空载转速 (rpm)
    float rated_speed;       // 额定转速 (rpm)
    float rated_torque;      // 额定扭矩 (N·m)
    float rated_current;     // 额定电流 (A)
    float current_scale;     // 电流转换系数 (A/LSB)
} DJIMotorParams_s;

extern const DJIMotorParams_s dji_motor_params[DJI_MODEL_NUM];
extern const uint16_t can_tx_id[DJI_MODEL_NUM][2];

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
    uint16_t raw_encoder;         // 编码器原始值
    int16_t raw_velocity;         // 原始速度值
    int16_t raw_current;          // 原始电流/力矩值
    int8_t raw_temperature_motor; // 线圈温度 (°C)
    uint8_t error_code;           // 错误码
} DJIMotorRawData_s;

typedef struct
{
    float position_single; // 单圈位置 (rad) [0, 2π)
    float position_multi;  // 多圈位置 (rad)
    float speed;           // 速度 (rad/s)
    float torque;          // 力矩 (N·m)
    float current;         // 电流 (A)
    float temperature;     // 线圈温度 (°C)
} DJIMotorData_s;

/*============================================
 *              DJI 单电机实例结构体
 *============================================*/
struct DJIMotorInstance
{
    uint8_t brand;
    uint8_t model;
    CANInstance *can;
    DaemonInstance *daemon;   // 可以提供时间戳
    uint8_t enable;           // 使能标志
    uint64_t last_rx_time_us; // 上次接收时间戳 (us)，用于位置差分计算速度
    // 控制器
    // 以上属性之后放到电机基类，还要添加一个虚函数表

    float set_ref;
    uint8_t motor_id; // 电机 ID (1-8)
    // 数据
    DJIMotorRawData_s data_raw;
    DJIMotorData_s data[2];
    uint8_t data_now_idx;
    uint8_t speed_src; // 选择速度使用反馈或者位置差分
    // 分组发送设置
    DJIMotorSendGroup_s *sender_group; // 分组
    uint8_t motor_idx_in_group;        // 组内编号
};

typedef struct
{
    BoardCAN_e can_e;                 // 板载CAN枚举（注册时用于查找映射）
    uint8_t model;                    // 型号
    uint16_t reload_count;            // 重载值（喂狗超时阈值）
    DaemonFaultAction_e fault_action; // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback lost_callback;   // 异常处理函数（可为NULL）
    uint8_t motor_id;                 // 电机 ID (1-8)
    uint8_t speed_src;                // 速度来源：0=反馈速度，1=位置差分
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
