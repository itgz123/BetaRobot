#ifndef __DRV_CHASSIS_POSITION_LITE_H
#define __DRV_CHASSIS_POSITION_LITE_H

#include <stdint.h>
#include "drv_chassis_def.h"
#include "drv_motor_def.h"

typedef struct
{
    float wheel_radius;           // 轮子半径
    float length[WHEEL_NUM];      // 轮子到中心距离
    ChassisType_e chassis_type;   // 底盘类型
    MotorBase_s motor[WHEEL_NUM]; // 4个电机实例
} ChassisPositionLiteInstance_t;

#endif // __DRV_CHASSIS_POSITION_LITE_H
