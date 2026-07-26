/**
 * @file bsp_adc.h
 * @brief ADC外设封装层
 *
 * @note BSP层职责：
 *       1. 管理ADC实例注册
 *       2. 提供ADC读取接口
 *       3. 硬件通道配置由 ADCConfig 负责，不在 Register 中配置
 *
 * @note 使用流程：
 *       1. 使用 ADC_INSTANCE_DEF 宏定义实例
 *       2. 调用 ADCRegister() 注册实例（仅注册，不配置硬件）
 *       3. 调用 ADCConfig() 配置硬件参数（通道/校准）
 *       4. 调用 ADCGetValue() 读取ADC值
 */

#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "bsp_map.h"

#ifdef HAL_ADC_MODULE_ENABLED

/*============================================
 *              ADC实例结构体
 *============================================*/
/**
 * @brief ADC实例结构体（轮询模式）
 */
typedef struct ADCInstance
{
    void *parent;      // 父实例指针（DRV层设置）
    BoardADC_e adc_e;  // 板载ADC枚举（Config时查找映射）
    ADC_Map_t adc_map; // ADC硬件映射（Config时自动填充）
} ADCInstance;

/*============================================
 *              实例定义宏
 *============================================*/
/**
 * @brief 静态定义ADC实例
 * @param name 实例名称
 */
#define ADC_INSTANCE_DEF(name) static ADCInstance name

/*============================================
 *              配置结构体
 *============================================*/
/**
 * @brief ADC 运行时配置结构体（用于 ADCConfig）
 * @note 配置板载枚举和触发硬件通道配置及校准流程。
 *       后续可扩展：采样时间、回调等。
 */
typedef struct
{
    BoardADC_e adc_e; // 板载ADC枚举（用于查找硬件映射）
} ADC_Config_s;

/*============================================
 *              接口函数声明
 *============================================*/
/**
 * @brief 注册ADC实例（仅调用一次）
 * @param instance ADC实例指针
 * @return 0:成功 -1:失败
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不配置硬件参数（由 ADCConfig 负责）。
 */
int8_t ADCRegister(ADCInstance *instance);

/**
 * @brief 配置ADC实例（可重复调用）
 * @param instance ADC实例指针
 * @param config   配置结构体指针（含 adc_e 板载枚举）
 * @return 0:成功 -1:失败
 * @note 填充硬件映射、配置 ADC 通道参数并执行校准。
 *       不修改 static 管理数组。
 *       要求在 ADCRegister 之后调用。
 *       可重复调用以重新配置硬件（如切换通道）。
 */
int8_t ADCConfig(ADCInstance *instance, const ADC_Config_s *config);

/**
 * @brief 获取ADC转换值（轮询模式）
 * @param instance ADC实例指针
 * @return ADC转换值（16位）
 */
uint16_t ADCGetValue(ADCInstance *instance);

#endif // BSP_ADC_MODULE_ENABLED

#endif // __BSP_ADC_H