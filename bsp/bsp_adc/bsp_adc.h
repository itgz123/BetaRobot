/**
 * @file bsp_adc.h
 * @brief ADC外设封装层
 *
 * @note BSP层职责：
 *       1. 管理ADC实例注册
 *       2. 提供ADC读取接口
 *       3. 不配置硬件（由CubeMX/HAL层负责）
 *
 * @note 使用流程：
 *       1. 使用 ADC_INSTANCE_DEF 宏定义实例
 *       2. 调用 ADCRegister() 注册实例
 *       3. 调用 ADCGetValue() 读取ADC值
 */

#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "bsp_cfg.h"

#ifdef BSP_ADC_MODULE_ENABLED

/*============================================
 *              ADC实例结构体
 *============================================*/
/**
 * @brief ADC实例结构体（轮询模式）
 */
typedef struct ADCInstance
{
    void *parent;      // 父实例指针（DRV层设置）
    BoardADC_e adc_e;  // 板载ADC枚举（注册时查找映射）
    ADC_Map_t adc_map; // ADC硬件映射（注册时自动填充）
} ADCInstance;

/*============================================
 *              实例定义宏
 *============================================*/
/**
 * @brief 静态定义ADC实例
 * @param name  实例名称
 * @param adc_idx 板载ADC枚举（BoardADC_e）
 */
#define ADC_INSTANCE_DEF(name, adc_idx) \
    static ADCInstance name = {         \
        .parent = NULL,                 \
        .adc_e = adc_idx,               \
        .adc_map = {0}}

/*============================================
 *              接口函数声明
 *============================================*/
/**
 * @brief 注册ADC实例
 * @param instance ADC实例指针
 * @return 0:成功 -1:失败
 * @note 注册时会执行ADC校准
 */
int8_t ADCRegister(ADCInstance *instance);

/**
 * @brief 获取ADC转换值（轮询模式）
 * @param instance ADC实例指针
 * @return ADC转换值（16位）
 */
uint16_t ADCGetValue(ADCInstance *instance);

#endif // BSP_ADC_MODULE_ENABLED

#endif // __BSP_ADC_H