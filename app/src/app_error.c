#include "app_error.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/**
 * @brief  Error 任务函数
 * @param  argument: 未使用
 * @retval None
 */
__attribute__((section(".itcmram"), noreturn)) void StartErrorTask(void *argument)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(ERROR_FREQ_MS));
    }
}