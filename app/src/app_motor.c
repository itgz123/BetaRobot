#include "app_motor.h"

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartMotorTask(void const *argument)
{
    /* 无限循环 */
    for (;;)
    {
        osDelay(1);
    }
}
