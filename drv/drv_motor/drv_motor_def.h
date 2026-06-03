#ifndef DRV_MOTOR_DEF_H
#define DRV_MOTOR_DEF_H

#include "stdint.h"

/* 电机品牌枚举 */
typedef enum
{
    MOTOR_BRAND_DJI = 0,
    MOTOR_BRAND_OTHER,
    MOTOR_BRAND_NUM,
} MotorBrand_e;

/* DJI电机型号枚举 */
typedef enum
{
    DJI_MODEL_M3508 = 0,    // C620 电调，带减速
    DJI_MODEL_M3508_DIRECT, // C620 电调，直驱（无减速）
    DJI_MODEL_M2006,        // C610 电调
    DJI_MODEL_GM6020,       // 云台电机
    DJI_MODEL_NUM,          // DJI电机型号数量
} DJIModel_e;

#endif // !DRV_MOTOR_DEF_H
