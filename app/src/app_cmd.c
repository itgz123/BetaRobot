#include "app_cmd.h"

/**
 * @brief  Cmd 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartCmdTask(void const *argument)
{
    /* 无限循环 */
    for (;;)
    {
        osDelay(1);
    }
}