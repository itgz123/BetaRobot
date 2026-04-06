/**
 * @file app_sensor.c
 * @brief 传感器任务模块：IMU 数据采集与处理
 */

#include "app_sensor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

static void Init(void)
{
}

ITCM_RAM static void Run(void)
{
}
/**
 * @brief 传感器任务入口函数
 * @param argument 未使用
 */
ITCM_RAM __attribute__((noreturn)) void StartSensorTask(void *argument)
{
    Init();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] SENSOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        Run();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > SENSOR_FREQ_MS)
            LOGERROR("[freeRTOS] SENSOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(SENSOR_FREQ_MS));
    }
}
