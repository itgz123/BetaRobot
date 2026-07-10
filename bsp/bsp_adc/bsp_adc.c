/**
 * @file bsp_adc.c
 * @brief ADC外设封装层实现
 */

#include "main.h" // 确保 HAL 宏已定义
#include "bsp_adc.h"
#include "app_cfg.h"

#ifdef BSP_ADC_USED

#ifdef HAL_ADC_MODULE_ENABLED
#if ADC_INSTANCE_NUM > 0

#include "bsp_uart_log.h"

/*============================================
 *              私有变量
 *============================================*/
static uint8_t s_idx = 0;                             // 已注册实例数量
static ADCInstance *s_adc_instance[ADC_INSTANCE_NUM]; // 实例指针数组

/**
 * @brief 按实例映射配置 ADC 通道
 * @retval HAL_OK 配置成功
 */
static HAL_StatusTypeDef ADCConfigChannel(ADCInstance *instance)
{
    ADC_ChannelConfTypeDef config = {0};

    config.Channel = instance->adc_map.channel;
#if CPU_CORE == CORTEX_M7
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    config.SingleDiff = ADC_SINGLE_ENDED;
    config.OffsetNumber = ADC_OFFSET_NONE;
    config.Offset = 0;
#else
    config.Rank = 1;
    config.SamplingTime = ADC_SAMPLETIME_3CYCLES;
#endif

    return HAL_ADC_ConfigChannel(instance->adc_map.handle, &config);
}

/*============================================
 *              接口函数实现
 *============================================*/
/**
 * @brief 注册ADC实例
 * @note 注册时执行ADC校准
 */
int8_t ADCRegister(ADCInstance *instance, const ADC_Init_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[BSP_ADC] Register failed: instance is NULL"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[BSP_ADC] Register failed: config is NULL"));
    BSP_RETURN_IF_TRUE_LOG(s_idx >= ADC_INSTANCE_NUM, -1, LOGERROR("[BSP_ADC] Register failed: instance num exceeded %d", ADC_INSTANCE_NUM));
    BSP_RETURN_IF_TRUE_LOG(config->adc_e >= ADC_NUM_MAX, -1, LOGERROR("[BSP_ADC] Register failed: adc_e out of range"));

    // 将配置拷贝到实例
    instance->adc_e = config->adc_e;

    // 自动填充硬件映射
    instance->adc_map = adc_map[instance->adc_e];

    BSP_RETURN_IF_TRUE_LOG(instance->adc_map.handle == NULL, -1, LOGERROR("[BSP_ADC] Register failed: ADC handle is NULL"));

    BSP_RETURN_IF_TRUE_LOG(ADCConfigChannel(instance) != HAL_OK, -1, LOGERROR("[BSP_ADC] Register failed: config channel failed, adc_e=%d", instance->adc_e));

    // ADC校准（仅H7系列支持，F4系列无校准API）
    ADC_HandleTypeDef *hadc = instance->adc_map.handle;
#if CPU_CORE == CORTEX_M7
    // H7系列：需要指定校准类型和差分模式
    if (HAL_ADCEx_Calibration_Start(hadc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        LOGERROR("[BSP_ADC] Calibration failed: adc_e=%d", instance->adc_e);
        return -1;
    }
#else
    // F4系列：无校准API，跳过
    (void)hadc; // 避免未使用警告
    LOGINFO("[BSP_ADC] ADC calibration skipped (not supported on F4 series)");
#endif

    // 保存实例指针
    s_adc_instance[s_idx++] = instance;

    LOGINFO("[BSP_ADC] Register success: adc_e=%d, handle=0x%p, channel=%lu", instance->adc_e, instance->adc_map.handle, instance->adc_map.channel);
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

    // 按实例映射确保当前采样通道正确
    if (ADCConfigChannel(instance) != HAL_OK)
    {
        return 0;
    }

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

#endif /* ADC_INSTANCE_NUM > 0 */
#endif /* HAL_ADC_MODULE_ENABLED */

#endif /* BSP_ADC_USED */