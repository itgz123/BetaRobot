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

#endif // !DRV_MOTOR_DEF_H
