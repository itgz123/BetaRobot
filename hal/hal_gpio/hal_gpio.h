/**
 * @file hal_gpio.h
 * @brief GPIO 外设重配置层（可选覆盖层）
 *
 * @note 职责仅封装"重配置"：默认不调用本层任何函数，bsp 直接使用 CubeMX 初始化
 *       （MX_GPIO_Init / HAL_TIM_MspPostInit）的结果。
 *       若要在运行时把某个引脚改作他用——典型场景是 CubeMX 里配置成 PWM 复用脚的
 *       引脚，运行时切成普通 GPIO 输入/输出（无中断）——可在 app 层调用本层函数，
 *       配置参数直接用 HAL 原生 GPIO_InitTypeDef。
 *       - 本层不内置默认值，也不被 bsp 层调用，只提供可选覆盖入口。
 *       - 无状态，端口与引脚一律参数传入。
 *       - 本层会打开该端口时钟，并先 HAL_GPIO_DeInit 再 HAL_GPIO_Init：
 *         引脚原有的 AF 复用、EXTI 线配置与挂起标志都会被清干净。
 *       - 本层不配置 NVIC：Mode 选 IT/EVT 时，中断向量与 IRQHandler 仍须由
 *         CubeMX 生成或调用方手写。
 */

#ifndef __HAL_GPIO_H
#define __HAL_GPIO_H

#include "main.h"

#if defined(HAL_GPIO_MODULE_ENABLED)

/**
 * @brief 重配置一个 GPIO 引脚（覆盖 CubeMX 初始化）
 * @param port GPIO 端口（GPIOA ~ GPIOK 中实际存在的那些）
 * @param pin  引脚掩码，必须是单比特（GPIO_PIN_x，不能是或组合）
 * @param init HAL 原生配置指针：Mode/Pull/Speed/Alternate 照用，Pin 字段会被 pin 覆盖
 * @retval HAL_OK    成功
 * @retval HAL_ERROR 空指针 / pin 非单比特 / init 字段非法 / 端口未知
 *
 * @note 执行顺序：开端口时钟 → HAL_GPIO_DeInit → HAL_GPIO_Init。
 *       DeInit 会让引脚短暂处于复位态（输入浮空），随后立刻按 init 配置。
 * @note 典型用法（把 CubeMX 配成 TIM 复用脚的 PE9 切成上拉输入，无中断）：
 *       @code
 *       GPIO_InitTypeDef gi = {0};
 *       gi.Mode = GPIO_MODE_INPUT;
 *       gi.Pull = GPIO_PULLUP;
 *       HalGpioReconfigure(GPIOE, GPIO_PIN_9, &gi);
 *       @endcode
 * @note 反向（GPIO 切成 PWM 复用脚）同样用本函数：Mode 给 GPIO_MODE_AF_PP/OD，
 *       并把 Alternate 设成对应定时器的 GPIO_AFx_TIMx；定时器侧见 hal_tim.h。
 */
HAL_StatusTypeDef HalGpioReconfigure(GPIO_TypeDef *port, uint16_t pin, const GPIO_InitTypeDef *init);

#endif /* HAL_GPIO_MODULE_ENABLED */

#endif /* __HAL_GPIO_H */
