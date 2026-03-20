#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

static void MOTORInit(void)
{
}

static void MOTORTask(void)
{
}

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartMotorTask(void *argument)
{
    MOTORInit();
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        MOTORTask();
        dt = DWT_GetTimeline_ms() - start;
        if (dt > MOTOR_FREQ_MS)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = [%f]", &dt);
        vTaskDelay(pdMS_TO_TICKS(MOTOR_FREQ_MS));
    }
}
