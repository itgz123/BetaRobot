/**
 * @file drv_pid.c
 * @brief PID 驱动层实现：封装 lib_pid 纯算法 + DWT 自动 dt
 *
 * @note dt 计算沿用旧 drv_pid 语义：本次调用与上次调用的时间间隔 (us → s)，
 *       时间戳记录在 PIDInstance.time_us（lib 算法不读取该字段）。
 *       PIDReset 后 time_us 清零，因此下一次 PIDCalculate 的 dt=0（首帧）。
 */

#include "drv_pid.h"
#include "bsp_dwt.h"

/*------------- 外部接口实现 --------------*/

void PIDInit(PIDInstance *instance, const PID_Init_Config_s *config)
{
    LibPIDInit(instance, config);
}

void PIDReset(PIDInstance *instance)
{
    LibPIDReset(instance);

    if (instance != NULL)
    {
        instance->time_us = 0; // 复位 dt 起点，使下次 PIDCalculate dt=0
    }
}

float PIDCalculate(PIDInstance *instance, float setpoint, float measure, float feedforward)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 自动测量时间间隔 (us)，换算为秒后交给纯算法
    uint64_t now_us = DWT_GetTimeUs();
    float dt = 0.0f;
    if (instance->time_us > 0)
    {
        dt = (float)(now_us - instance->time_us) * 1e-6f;
    }
    instance->time_us = now_us;

    return LibPIDCalculate(instance, setpoint, measure, feedforward, dt);
}
