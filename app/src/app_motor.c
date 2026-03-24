#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
//
#include "bsp_tim.h"

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
__attribute__((noreturn)) void StartMotorTask(void *argument)
{
    MOTORInit();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        MOTORTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > MOTOR_FREQ_MS)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(MOTOR_FREQ_MS));
    }
}
