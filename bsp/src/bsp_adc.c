/**
 * @file bsp_adc.c
 * @brief ADC外设封装层实现
 */

#include "bsp_adc.h"
#include "bsp_log.h"

/*============================================
 *              私有变量
 *============================================*/
static uint8_t s_idx = 0;                             // 已注册实例数量
static ADCInstance *s_adc_instance[ADC_INSTANCE_NUM]; // 实例指针数组

/*============================================
 *              接口函数实现
 *============================================*/
/**
 * @brief 注册ADC实例
 * @note 注册时执行ADC校准
 */
int8_t ADCRegister(ADCInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[BSP_ADC] Register failed: instance is NULL");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= ADC_INSTANCE_NUM)
    {
        LOGERROR("[BSP_ADC] Register failed: instance num exceeded %d", ADC_INSTANCE_NUM);
        return -1;
    }

    // 自动填充硬件映射
    instance->adc_map = adc_map[instance->adc_e];

    // ADC校准
    ADC_HandleTypeDef *hadc = instance->adc_map.handle;
    if (HAL_ADCEx_Calibration_Start(hadc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        LOGERROR("[BSP_ADC] Calibration failed: adc_e=%d", instance->adc_e);
        return -1;
    }

    // 保存实例指针
    s_adc_instance[s_idx++] = instance;

    LOGINFO("[BSP_ADC] Register success: adc_e=%d, handle=0x%p, channel=%lu",
            instance->adc_e, instance->adc_map.handle, instance->adc_map.channel);
    return 0;
}

/**
 * @brief 获取ADC转换值（轮询模式）
 */
uint16_t ADCGetValue(ADCInstance *instance)
{
    if (instance == NULL || instance->adc_map.handle == NULL)
    {
        return 0;
    }

    ADC_HandleTypeDef *hadc = instance->adc_map.handle;

    // 启动ADC转换
    HAL_ADC_Start(hadc);

    // 等待转换完成（超时100ms）
    if (HAL_ADC_PollForConversion(hadc, 100) != HAL_OK)
    {
        HAL_ADC_Stop(hadc);
        return 0;
    }

    // 读取转换值
    uint16_t value = HAL_ADC_GetValue(hadc);

    // 停止ADC
    HAL_ADC_Stop(hadc);

    return value;
}