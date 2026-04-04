/**
 * @file app_sensor.c
 * @brief 传感器任务模块：IMU 数据采集与处理
 */

#include "app_sensor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/**
 * @brief 传感器任务入口函数
 * @param argument 未使用
 */
ITCM_RAM __attribute__((noreturn)) void StartSensorTask(void *argument)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_FREQ_MS));
    }
}
