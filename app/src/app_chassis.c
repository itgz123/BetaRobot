#include "app_chassis.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

static void ChassisInit(void)
{
}

static void ChassisTask(void)
{
}

/**
 * @brief  Chassis 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartChassisTask(void *argument)
{
    ChassisInit();
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] CHASSIS Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        ChassisTask();
        dt = DWT_GetTimeline_ms() - start;
        if (dt > CHASSIS_FREQ_MS)
            LOGERROR("[freeRTOS] CHASSIS Task is being DELAY! dt = [%f]", &dt);
        vTaskDelay(pdMS_TO_TICKS(CHASSIS_FREQ_MS));
    }
}