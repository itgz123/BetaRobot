/**
 * @file bsp_gpio.h
 * @brief GPIO驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（上拉/下拉/中断等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include "bsp_cfg.h"

#ifdef BSP_GPIO_MODULE_ENABLED

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

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义GPIO实例（使用板载枚举）
 * @param name     实例名称
 * @param gpio_idx 板载GPIO枚举（BoardGPIO_e）
 * @param cb       回调函数（可为NULL）
 *
 * @example
 *   GPIO_INSTANCE_DEF(led_green, GPIO_LED_GREEN, led_callback);
 */
#define GPIO_INSTANCE_DEF(name, gpio_idx, cb) \
    GPIOInstance name = {                     \
        .parent = NULL,                       \
        .gpio_e = gpio_idx,                   \
        .map = {0},                           \
        .pin_state = GPIO_PIN_RESET,          \
        .callback = cb}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册GPIO实例
 * @param instance GPIO实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限）
 */
int8_t GPIORegister(GPIOInstance *instance);

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
