#include "app_chassis.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "bsp_adc.h"
#include "app_cfg.h"

// ADC实例定义（PC04 电池电压）
ADC_INSTANCE_DEF(bat_voltage_adc, ADC_BAT_VOLTAGE);

// ADC值存储
static float s_bat_voltage_raw = 0;

static void ChassisInit(void)
{
    // 注册ADC实例
    ADCRegister(&bat_voltage_adc);
}

__attribute__((section(".itcmram"))) static void ChassisTask(void)
{
    // 读取PC04 ADC值
    s_bat_voltage_raw = (float)ADCGetValue(&bat_voltage_adc) * VOLTAGE_TRANFER;
}

/**
 * @brief  Chassis 任务函数
 * @param  argument: 未使用
 * @retval None
 */
__attribute__((section(".itcmram"), noreturn)) void StartChassisTask(void *argument)
{
    ChassisInit();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] CHASSIS Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        ChassisTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > CHASSIS_FREQ_MS)
            LOGERROR("[freeRTOS] CHASSIS Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(CHASSIS_FREQ_MS));
    }
}