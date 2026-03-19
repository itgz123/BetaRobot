#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartMotorTask(void const *argument)
{
    // 初始化开始
    // 初始化结束
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        // 任务开始
        // 任务结束
        dt = DWT_GetTimeline_ms() - start;
        if (dt > MOTOR_FREQ_MS)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = [%f]", &dt);
        osDelay(MOTOR_FREQ_MS);
    }
}
