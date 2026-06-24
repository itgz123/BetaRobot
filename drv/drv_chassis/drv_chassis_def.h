#ifndef __DRV_CHASSIS_DEF_H
#define __DRV_CHASSIS_DEF_H

#include <stdint.h>

// 底盘类型
typedef enum : uint8_t
{
    CHASSISTYPE_OMNI_T = 0,  // 全向 (前后左右)
    CHASSISTYPE_OMNI_X,      // 全向 (4个角)
    CHASSISTYPE_MECANUM_O,   // 麦轮 (俯视时锟子为O型)
    CHASSISTYPE_MECANUM_X,   // 麦轮 (俯视时锟子为X型)
    CHASSISTYPE_HALF_RUDDER, // 半舵
    CHASSISTYPE_ALL_RUDDER,  // 全舵
} ChassisType_e;

// 轮子顺序
typedef enum : uint8_t
{
    WHEEL_LF = 0, // 左前 (前)
    WHEEL_LB,     // 左后 (左)
    WHEEL_RB,     // 右后 (后)
    WHEEL_RF,     // 右前 (右)
    WHEEL_NUM,    // 轮子数量
} ChassisWheelSequence_e;

// 电机轴指向眼睛逆时针为电机正方向
// x+向前，y+向左，w+逆时针
typedef struct
{
    float vx;
    float vy;
    float w;
    uint8_t enable; // 先留着，可以用来disable电机
} ChassisCmd_t;

#endif // __DRV_CHASSIS_DEF_H
