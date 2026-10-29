/**
 * @file hal_tim.c
 * @brief TIM 外设重配置层实现
 *
 * @note 实现策略：覆盖 htim->Init 后重新走 HAL 的初始化链，再单独配置通道。
 *       - 先停通道与计数器：带载改 PSC/ARR 会产生一拍未定义的周期。
 *       - 自己开定时器时钟：CubeMX 的 HAL_TIM_*_MspInit 只匹配它配置过的实例，
 *         未配置实例走空实现。State==RESET 时 HAL 才会调 MspInit，而已配置实例
 *         State 已是 READY，不会重复配置 NVIC/时钟。
 *       - HAL_TIM_PWM_Init 内部也会调用 TIM_Base_SetConfig，这里仍按 CubeMX 的
 *         调用顺序先走 HAL_TIM_Base_Init（补 Base_MspInit 语义），再 PWM_Init。
 *       - 本工程未开启 USE_FULL_ASSERT，HAL 的 assert_param 是空操作，
 *         故先在本层做参数范围校验，非法参数直接返回 HAL_ERROR。
 *       - 本层默认不被 bsp 层调用（bsp 直接使用 CubeMX 初始化结果），只作为 app 层
 *         可选覆盖入口。
 */

#include "hal_tim.h"

#if defined(HAL_TIM_MODULE_ENABLED)

/*------------- 局部校验辅助 --------------*/

/**
 * @brief 打开定时器时钟（同时充当 PWM 能力校验）
 * @retval 1 时钟已开；0 未知实例或基础定时器 TIM6/TIM7（无输出通道）
 * @note 用 #if defined(TIMx) 兜住不同系列：F4 有 TIM9~TIM14，H7 有 TIM15~TIM17/23/24。
 */
static uint8_t HalTimEnableClock(TIM_TypeDef *instance)
{
#if defined(TIM1)
    if (instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM2)
    if (instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM3)
    if (instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM4)
    if (instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM5)
    if (instance == TIM5)
    {
        __HAL_RCC_TIM5_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM8)
    if (instance == TIM8)
    {
        __HAL_RCC_TIM8_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM9)
    if (instance == TIM9)
    {
        __HAL_RCC_TIM9_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM10)
    if (instance == TIM10)
    {
        __HAL_RCC_TIM10_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM11)
    if (instance == TIM11)
    {
        __HAL_RCC_TIM11_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM12)
    if (instance == TIM12)
    {
        __HAL_RCC_TIM12_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM13)
    if (instance == TIM13)
    {
        __HAL_RCC_TIM13_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM14)
    if (instance == TIM14)
    {
        __HAL_RCC_TIM14_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM15)
    if (instance == TIM15)
    {
        __HAL_RCC_TIM15_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM16)
    if (instance == TIM16)
    {
        __HAL_RCC_TIM16_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM17)
    if (instance == TIM17)
    {
        __HAL_RCC_TIM17_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM23)
    if (instance == TIM23)
    {
        __HAL_RCC_TIM23_CLK_ENABLE();
        return 1;
    }
#endif
#if defined(TIM24)
    if (instance == TIM24)
    {
        __HAL_RCC_TIM24_CLK_ENABLE();
        return 1;
    }
#endif
    return 0;
}

/* TIM6/TIM7 是基础定时器（无输出通道），刻意不在 HalTimEnableClock 的白名单里 */

static uint8_t HalTimIsValidChannel(uint32_t channel)
{
    return (uint8_t)((channel == TIM_CHANNEL_1) || (channel == TIM_CHANNEL_2) || (channel == TIM_CHANNEL_3) || (channel == TIM_CHANNEL_4));
}

static uint8_t HalTimIsValidBase(const TIM_Base_InitTypeDef *base)
{
    if (base->Prescaler > 0xFFFFU)
    {
        return 0;
    }
    if (base->RepetitionCounter > 0xFFU)
    {
        return 0;
    }
    if ((base->CounterMode != TIM_COUNTERMODE_UP) && (base->CounterMode != TIM_COUNTERMODE_DOWN) && (base->CounterMode != TIM_COUNTERMODE_CENTERALIGNED1) &&
        (base->CounterMode != TIM_COUNTERMODE_CENTERALIGNED2) && (base->CounterMode != TIM_COUNTERMODE_CENTERALIGNED3))
    {
        return 0;
    }
    if ((base->ClockDivision != TIM_CLOCKDIVISION_DIV1) && (base->ClockDivision != TIM_CLOCKDIVISION_DIV2) && (base->ClockDivision != TIM_CLOCKDIVISION_DIV4))
    {
        return 0;
    }
    if ((base->AutoReloadPreload != TIM_AUTORELOAD_PRELOAD_ENABLE) && (base->AutoReloadPreload != TIM_AUTORELOAD_PRELOAD_DISABLE))
    {
        return 0;
    }
    return 1;
}

static uint8_t HalTimIsValidOc(const TIM_OC_InitTypeDef *oc)
{
    if ((oc->OCMode != TIM_OCMODE_PWM1) && (oc->OCMode != TIM_OCMODE_PWM2))
    {
        return 0;
    }
    if ((oc->OCPolarity != TIM_OCPOLARITY_HIGH) && (oc->OCPolarity != TIM_OCPOLARITY_LOW))
    {
        return 0;
    }
    if ((oc->OCNPolarity != TIM_OCNPOLARITY_HIGH) && (oc->OCNPolarity != TIM_OCNPOLARITY_LOW))
    {
        return 0;
    }
    if ((oc->OCFastMode != TIM_OCFAST_DISABLE) && (oc->OCFastMode != TIM_OCFAST_ENABLE))
    {
        return 0;
    }
    if ((oc->OCIdleState != TIM_OCIDLESTATE_SET) && (oc->OCIdleState != TIM_OCIDLESTATE_RESET))
    {
        return 0;
    }
    if ((oc->OCNIdleState != TIM_OCNIDLESTATE_SET) && (oc->OCNIdleState != TIM_OCNIDLESTATE_RESET))
    {
        return 0;
    }
    return 1;
}

/*------------- 重配置实现 --------------*/

HAL_StatusTypeDef HalTimReconfigurePwm(TIM_HandleTypeDef *htim, uint32_t channel, const TIM_Base_InitTypeDef *base, const TIM_OC_InitTypeDef *oc)
{
    if ((htim == NULL) || (htim->Instance == NULL) || (base == NULL) || (oc == NULL))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if (HalTimIsValidChannel(channel) == 0U)
    {
        return HAL_ERROR;
    }
    if (HalTimIsValidBase(base) == 0U)
    {
        return HAL_ERROR;
    }
    if (HalTimIsValidOc(oc) == 0U)
    {
        return HAL_ERROR;
    }

    /* 开时钟，同时拒绝未知实例与无输出通道的基础定时器 */
    if (HalTimEnableClock(htim->Instance) == 0U)
    {
        return HAL_ERROR;
    }

    /* 带载改 PSC/ARR 会有未定义周期：先停通道，再停计数器 */
    (void)HAL_TIM_PWM_Stop(htim, channel);
    __HAL_TIM_DISABLE(htim);

    /* 覆盖时基后重新走 HAL 初始化链 */
    htim->Init = *base;
    if (HAL_TIM_Base_Init(htim) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_TIM_PWM_Init(htim) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_TIM_PWM_ConfigChannel(htim, oc, channel) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

#endif /* HAL_TIM_MODULE_ENABLED */
