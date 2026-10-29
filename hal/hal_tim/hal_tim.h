/**
 * @file hal_tim.h
 * @brief TIM 外设重配置层（可选覆盖层）
 *
 * @note 职责仅封装"重配置"：默认不调用本层任何函数，bsp 直接使用 CubeMX 初始化
 *       （MX_TIMx_Init / HAL_TIM_MspPostInit）的结果。
 *       若要在运行时把某个定时器通道改作 PWM 输出——典型场景是新增一路 PWM 而
 *       不想改 .ioc——可在 app 层调用本层函数，配置参数直接用 HAL 原生
 *       TIM_Base_InitTypeDef / TIM_OC_InitTypeDef。
 *       - 本层不内置默认值，也不被 bsp 层调用，只提供可选覆盖入口。
 *       - 无状态，handle 一律参数传入；handle 可以是 CubeMX 的 &htimX，
 *         也可以是自建 TIM_HandleTypeDef（但 Instance 必须先赋值）。
 *       - 本层会打开定时器时钟：CubeMX 生成的 MspInit 只处理自己配置过的实例，
 *         对未配置实例是空操作，不打开时钟的话 Init 写寄存器会被丢弃。
 *       - 输出引脚不在这里配置：用 HalGpioReconfigure（hal_gpio.h）配成
 *         GPIO_MODE_AF_PP/OD + GPIO_AFx_TIMx。
 *       - 只做 PWM：编码器/输入捕获/从模式不在此层。
 */

#ifndef __HAL_TIM_H
#define __HAL_TIM_H

#include "main.h"

#if defined(HAL_TIM_MODULE_ENABLED)

/**
 * @brief 重配置一个 TIM 通道为 PWM 输出（覆盖 CubeMX 初始化）
 * @param htim    定时器句柄（Instance 必须有效）
 * @param channel 输出通道，仅支持 TIM_CHANNEL_1 ~ TIM_CHANNEL_4
 * @param base    HAL 原生时基配置（Prescaler / Period / CounterMode / ClockDivision /
 *                RepetitionCounter / AutoReloadPreload）
 * @param oc      HAL 原生输出比较配置（OCMode 必须是 TIM_OCMODE_PWM1 或 PWM2）
 * @retval HAL_OK    成功
 * @retval HAL_ERROR 空指针 / channel 非法 / base 或 oc 字段非法 / 未知或基础定时器
 *
 * @note 执行顺序：开时钟 → 停该通道与计数器 → 覆盖 htim->Init →
 *       HAL_TIM_Base_Init → HAL_TIM_PWM_Init → HAL_TIM_PWM_ConfigChannel。
 *       **不启动输出**：启动/停止仍用 HAL_TIM_PWM_Start/Stop（频率与占空比
 *       可用 __HAL_TIM_SET_PRESCALER / __HAL_TIM_SET_AUTORELOAD / __HAL_TIM_SET_COMPARE）。
 * @note 基本定时器 TIM6/TIM7 没有输出通道，返回 HAL_ERROR。
 *       TIM9~TIM17 的通道数少于 4，调用方需自行保证 channel 在该实例上存在。
 * @note 典型用法（新增一路 PWM，不改 .ioc）：
 *       @code
 *       TIM_HandleTypeDef htimx = {0};
 *       htimx.Instance = TIM2;
 *
 *       TIM_Base_InitTypeDef base = {0};
 *       base.Prescaler = 84 - 1;      // 84 MHz / 84 = 1 MHz
 *       base.CounterMode = TIM_COUNTERMODE_UP;
 *       base.Period = 1000 - 1;       // 1 kHz
 *       base.ClockDivision = TIM_CLOCKDIVISION_DIV1;
 *       base.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
 *
 *       TIM_OC_InitTypeDef oc = {0};
 *       oc.OCMode = TIM_OCMODE_PWM1;
 *       oc.Pulse = 0;
 *       oc.OCPolarity = TIM_OCPOLARITY_HIGH;
 *
 *       HalTimReconfigurePwm(&htimx, TIM_CHANNEL_1, &base, &oc);
 *
 *       GPIO_InitTypeDef gi = {0};
 *       gi.Mode = GPIO_MODE_AF_PP;
 *       gi.Pull = GPIO_NOPULL;
 *       gi.Speed = GPIO_SPEED_FREQ_LOW;
 *       gi.Alternate = GPIO_AF1_TIM2;
 *       HalGpioReconfigure(GPIOA, GPIO_PIN_0, &gi);
 *
 *       HAL_TIM_PWM_Start(&htimx, TIM_CHANNEL_1);
 *       @endcode
 */
HAL_StatusTypeDef HalTimReconfigurePwm(TIM_HandleTypeDef *htim, uint32_t channel,
                                       const TIM_Base_InitTypeDef *base,
                                       const TIM_OC_InitTypeDef *oc);

#endif /* HAL_TIM_MODULE_ENABLED */

#endif /* __HAL_TIM_H */
