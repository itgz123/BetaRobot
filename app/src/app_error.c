#include "app_error.h"

/**
 * @brief  Error 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartErrorTask(void const *argument)
{
    /* 无限循环 */
    for (;;)
    {
        osDelay(1);
    }
}