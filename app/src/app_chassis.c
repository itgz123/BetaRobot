#include "app_chassis.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

static void ChassisInit(void)
{
}

ITCM_RAM static void ChassisTask(void)
{
}

/**
 * @brief  Chassis 任务函数
 * @param  argument: 未使用
 * @retval None
 */
ITCM_RAM __attribute__((noreturn)) void StartChassisTask(void *argument)
{
    ChassisInit();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] CHASSIS Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        ChassisTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > CHASSIS_FREQ_MS)
            LOGERROR("[freeRTOS] CHASSIS Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(CHASSIS_FREQ_MS));
    }
}