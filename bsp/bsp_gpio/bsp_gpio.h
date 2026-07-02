/**
 * @file bsp_gpio.h
 * @brief GPIO驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（上拉/下拉/中断等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include "bsp_map.h"

#ifdef HAL_GPIO_MODULE_ENABLED

#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief GPIO实例结构体
 */
typedef struct GPIOInstance
{
    void *parent;                            // 父实例指针（由 DRV 层设置）
    BoardGPIO_e gpio_e;                      // 板载GPIO枚举（注册时用于查找映射）
    GPIO_Map_t map;                          // GPIO映射（注册时自动填充）
    GPIO_PinState pin_state;                 // 引脚状态
    void (*callback)(struct GPIOInstance *); // EXTI中断回调函数
} GPIOInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief GPIO 初始化配置结构体
 */
typedef struct
{
    BoardGPIO_e gpio_e;                      // 板载GPIO枚举（注册时用于查找映射）
    void (*callback)(struct GPIOInstance *); // EXTI中断回调函数（可为NULL）
} GPIO_Init_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义GPIO实例（仅身份绑定）
 * @param name     实例名称
 * @param gpio_idx 板载GPIO枚举（BoardGPIO_e）
 */
#define GPIO_INSTANCE_DEF(name) static GPIOInstance name

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册GPIO实例
 * @param instance GPIO实例指针（需先通过宏定义）
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限）
 */
int8_t GPIORegister(GPIOInstance *instance, const GPIO_Init_Config_s *config);

/**
 * @brief 翻转GPIO电平
 * @param instance GPIO实例
 */
void GPIOToggle(GPIOInstance *instance);

/**
 * @brief 设置GPIO为高电平
 * @param instance GPIO实例
 */
void GPIOSet(GPIOInstance *instance);

/**
 * @brief 设置GPIO为低电平
 * @param instance GPIO实例
 */
void GPIOReset(GPIOInstance *instance);

/**
 * @brief 读取GPIO电平
 * @param instance GPIO实例
 * @return 引脚状态（GPIO_PinState）
 */
GPIO_PinState GPIORead(GPIOInstance *instance);

/**
 * @brief 写入GPIO电平
 * @param instance GPIO实例
 * @param state 引脚状态
 */
void GPIOWrite(GPIOInstance *instance, GPIO_PinState state);

#endif // BSP_GPIO_MODULE_ENABLED

#endif /* __BSP_GPIO_H */
