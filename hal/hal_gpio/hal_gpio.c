/**
 * @file hal_gpio.c
 * @brief GPIO 外设重配置层实现
 *
 * @note 实现策略：
 *       ① 先打开端口时钟——写未开时钟的 GPIO 寄存器会被硬件忽略，且 DeInit 也依赖时钟；
 *       ② HAL_GPIO_DeInit 清掉该引脚的 AF/EXTI 残留与挂起标志；
 *       ③ 把 init 拷贝一份、强制 Pin = 传入的 pin，再 HAL_GPIO_Init。
 *       - 本工程未开启 USE_FULL_ASSERT，HAL 的 assert_param 是空操作，
 *         故先在本层做参数范围校验，非法参数直接返回 HAL_ERROR 而不静默写寄存器。
 *       - 本层默认不被 bsp 层调用（bsp 直接使用 CubeMX 初始化结果），只作为 app 层
 *         可选覆盖入口。
 */

#include "hal_gpio.h"

#if defined(HAL_GPIO_MODULE_ENABLED)

/*------------- 局部校验辅助 --------------*/

static uint8_t HalGpioIsSingleBitPin(uint16_t pin)
{
    return (uint8_t)((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

static uint8_t HalGpioIsValidMode(uint32_t mode)
{
    switch (mode)
    {
    case GPIO_MODE_INPUT:
    case GPIO_MODE_OUTPUT_PP:
    case GPIO_MODE_OUTPUT_OD:
    case GPIO_MODE_AF_PP:
    case GPIO_MODE_AF_OD:
    case GPIO_MODE_ANALOG:
    case GPIO_MODE_IT_RISING:
    case GPIO_MODE_IT_FALLING:
    case GPIO_MODE_IT_RISING_FALLING:
    case GPIO_MODE_EVT_RISING:
    case GPIO_MODE_EVT_FALLING:
    case GPIO_MODE_EVT_RISING_FALLING:
        return 1;
    default:
        return 0;
    }
}

static uint8_t HalGpioIsValidPull(uint32_t pull)
{
    return (uint8_t)((pull == GPIO_NOPULL) || (pull == GPIO_PULLUP) || (pull == GPIO_PULLDOWN));
}

static uint8_t HalGpioIsValidSpeed(uint32_t speed)
{
    return (uint8_t)((speed == GPIO_SPEED_FREQ_LOW) || (speed == GPIO_SPEED_FREQ_MEDIUM) || (speed == GPIO_SPEED_FREQ_HIGH) ||
                     (speed == GPIO_SPEED_FREQ_VERY_HIGH));
}

/**
 * @brief 打开端口时钟
 * @retval 1 端口已识别并开时钟；0 未知端口
 * @note 用 #if defined(GPIOx) 兜住不同封装/系列：F4 没有 GPIOJ/K，H7 有。
 */
static uint8_t HalGpioEnablePortClock(GPIO_TypeDef *port)
{
#if defined(GPIOA)
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOB)
    if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOC)
    if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOD)
    if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOE)
    if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOF)
    if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOG)
    if (port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOH)
    if (port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOI)
    if (port == GPIOI)
    {
        __HAL_RCC_GPIOI_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOJ)
    if (port == GPIOJ)
    {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(GPIOK)
    if (port == GPIOK)
    {
        __HAL_RCC_GPIOK_CLK_ENABLE();
        return 1;
    }
#endif
    return 0;
}

/*------------- 重配置实现 --------------*/

HAL_StatusTypeDef HalGpioReconfigure(GPIO_TypeDef *port, uint16_t pin, const GPIO_InitTypeDef *init)
{
    GPIO_InitTypeDef cfg;

    if ((port == NULL) || (init == NULL) || (HalGpioIsSingleBitPin(pin) == 0U))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if ((HalGpioIsValidMode(init->Mode) == 0U) || (HalGpioIsValidPull(init->Pull) == 0U) || (HalGpioIsValidSpeed(init->Speed) == 0U))
    {
        return HAL_ERROR;
    }
    if (((init->Mode == GPIO_MODE_AF_PP) || (init->Mode == GPIO_MODE_AF_OD)) && (init->Alternate > 0x0FU))
    {
        return HAL_ERROR;
    }

    if (HalGpioEnablePortClock(port) == 0U)
    {
        return HAL_ERROR;
    }

    /* 清掉原配置（AF 复用 / EXTI 线 / 挂起标志），再按新配置初始化 */
    HAL_GPIO_DeInit(port, pin);

    cfg = *init;
    cfg.Pin = pin; /* 以入参 pin 为准，避免 init->Pin 与 port/pin 不一致 */
    HAL_GPIO_Init(port, &cfg);

    return HAL_OK;
}

#endif /* HAL_GPIO_MODULE_ENABLED */
