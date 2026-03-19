#include "app_cmd.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/**
 * @brief  Cmd 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartCmdTask(void const *argument)
{
    // 初始化开始
    // 初始化结束
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] CMD Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        // 任务开始
        // 任务结束
        dt = DWT_GetTimeline_ms() - start;
        if (dt > CMD_FREQ_MS)
            LOGERROR("[freeRTOS] CDM Task is being DELAY! dt = [%f]", &dt);
        osDelay(CMD_FREQ_MS);
    }
}