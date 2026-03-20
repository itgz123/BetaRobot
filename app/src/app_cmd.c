#include "app_cmd.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

static void CmdInit(void)
{
}

static void CmdTask(void)
{
}

/**
 * @brief  Cmd 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartCmdTask(void *argument)
{
    CmdInit();
    static float start;
    static float dt;
    LOGINFO("[freeRTOS] CMD Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_ms();
        CmdTask();
        dt = DWT_GetTimeline_ms() - start;
        if (dt > CMD_FREQ_MS)
            LOGERROR("[freeRTOS] CMD Task is being DELAY! dt = [%f]", &dt);
        vTaskDelay(pdMS_TO_TICKS(CMD_FREQ_MS));
    }
}