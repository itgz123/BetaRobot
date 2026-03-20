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
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] CMD Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        CmdTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > CMD_FREQ_MS)
            LOGERROR("[freeRTOS] CMD Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(CMD_FREQ_MS));
    }
}