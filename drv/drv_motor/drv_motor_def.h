#ifndef DRV_MOTOR_DEF_H
#define DRV_MOTOR_DEF_H

#include "stdint.h"

typedef enum
{
    DJI_MODEL_M3508 = 0, // C620 电调
    DJI_MODEL_M2006,     // C610 电调
    DJI_MODEL_GM6020,    // 云台电机
    DJI_MODEL_NUM,       // dji电机数量
} DJIModel_e;
// typedef struct
// {
//     uint32_t raw_encoder;         // 编码器原始值
//     int32_t raw_velocity;         // 原始速度值
//     int32_t raw_current;          // 原始电流/力矩值
//     int8_t raw_temperature_motor; // 线圈温度 (°C)
//     int8_t raw_temperature_mos;   // MOS/驱动温度 (°C)，无此传感器填线圈相同温度值
//     uint8_t error_code;           // 品牌错误码
// } MotorRawData_t;
// 原始数据不管，但是处理后的数据尽量用同一结构体，也不一定吧

// typedef struct
// {
//     float position_single; // 单圈位置 (rad) [0, 2π)
//     float position_multi;  // 多圈位置 (rad)
//     float speed;           // 速度 (rad/s)
//     float torque;          // 力矩 (N·m)
//     float current;         // 电流 (A)
//     float temperature;     // 线圈温度 (°C)
//     float temperature_mos; // MOS/驱动温度 (°C)
//     uint16_t error_flags;  // 统一错误位掩码
// } MotorData_t;


    // 数据好好考虑一下
    // MotorRawData_t data_raw; // 原始数据
    // MotorData_t data_now;    // 当前数据
    // MotorData_t data_last;   // 上次数据

        // 电机特征
    // float reduction_ratio; // 减速比子类中
    // float torque_coef; // 扭矩系数 (N·m/A)
#endif // !DRV_MOTOR_DEF_H
