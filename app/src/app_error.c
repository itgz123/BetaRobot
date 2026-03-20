#include "app_error.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/**
 * @brief  Error 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartErrorTask(void *argument)
{
    // 初始化开始
    // 初始化结束
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] ERROR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        // 任务开始
        // 任务结束
        dt = DWT_GetTimeline_ms() - start;
        if (dt > ERROR_FREQ_MS)
            LOGERROR("[freeRTOS] ERROR Task is being DELAY! dt = [%f]", &dt);
        vTaskDelay(pdMS_TO_TICKS(ERROR_FREQ_MS));
    }
}