/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include <string.h>
#include "bsp_dwt.h"
#include "bsp_uart_log.h"

/* 加热控制 */
#ifdef BMI088_HEAT_USED

#if DEVELOPMENT_BOARD == DM_MC02
/*
 * ── DM-MC02：DMA+OPM 安全关断方案 ──
 *
 * CubeMX 删除了 TIM3 配置，在此处完整操作寄存器完成初始化。
 * 每次加热触发一次 DMA，占空比数组最后一项为 0，
 * OPM 发完所有脉冲后硬件自动清零 CEN，输出锁定低电平。
 * CPU 暂停(断点调试)不影响硬件发送脉冲序列。
 */
#include "tim.h"

#ifndef BMI088_HEAT_PULSE_NUM
#define BMI088_HEAT_PULSE_NUM 50 // 每次加热脉冲数
#endif
#ifndef BMI088_HEAT_DUTY_PERCENT
#define BMI088_HEAT_DUTY_PERCENT 1                           // 加热占空比百分比 (0-100)
#define BMI088_HEAT_DUTY (BMI088_HEAT_DUTY_PERCENT / 100.0f) // 加热占空比 (0-1)
#endif
#ifndef BMI088_HEAT_TEMP_HYST
#define BMI088_HEAT_TEMP_HYST 1.0f // 温度回滞
#endif
#ifndef BMI088_TEMP_TARGET
#define BMI088_TEMP_TARGET 30 // 加热目标温度 (℃)
#endif
#define BMI088_HEAT_CLOSE_DUTY 0 // 关闭占空比 (0-1)

// 初始化配置
#define BMI088_HEAT_TIM_PRC (80 - 1)
#define BMI088_HEAT_TIM_PRD 60000
#define BMI088_HEAT_DUTY_CCR ((uint32_t)((uint32_t)BMI088_HEAT_DUTY_PERCENT * BMI088_HEAT_TIM_PRD / 100))

static TIM_HandleTypeDef s_htim3;
static DMA_HandleTypeDef s_hdma_tim3_ch4;
static DMA_RAM uint32_t s_heat_duty[BMI088_HEAT_PULSE_NUM] = {0};

/* DMA2_Stream1 中断处理 — 避免启动文件弱符号跳 HardFault */
void DMA2_Stream1_IRQHandler(void)
{
    extern DMA_HandleTypeDef s_hdma_tim3_ch4;
    HAL_DMA_IRQHandler(&s_hdma_tim3_ch4);
}

/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
    HAL_TIM_PWM_Stop_DMA(&s_htim3, TIM_CHANNEL_4);
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, 0);
}

int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    /* ── TIM3 时钟 ── */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* ── GPIO PB1 → TIM3_CH4 (AF2) ── */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {
        .Pin = GPIO_PIN_1,
        .Mode = GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = GPIO_AF2_TIM3,
    };
    HAL_GPIO_Init(GPIOB, &gpio);

    /* ── TIM3 基础配置（同之前 CubeMX：PSC=80-1, ARR=60000, ~50Hz）─ */
    s_htim3.Instance = TIM3;
    s_htim3.Init.Prescaler = BMI088_HEAT_TIM_PRC;
    s_htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim3.Init.Period = BMI088_HEAT_TIM_PRD;
    s_htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&s_htim3) != HAL_OK)
        return -1;

    /* ── OPM 单脉冲模式 ── */
    if (HAL_TIM_OnePulse_Init(&s_htim3, TIM_OPMODE_SINGLE) != HAL_OK)
        return -1;

    /* ── PWM 通道 4 ── */
    TIM_OC_InitTypeDef oc = {
        .OCMode = TIM_OCMODE_PWM1,
        .Pulse = BMI088_HEAT_CLOSE_DUTY,
        .OCPolarity = TIM_OCPOLARITY_HIGH,
        .OCFastMode = TIM_OCFAST_DISABLE,
    };
    if (HAL_TIM_PWM_ConfigChannel(&s_htim3, &oc, TIM_CHANNEL_4) != HAL_OK)
        return -1;

    /* HAL 强制置位了 OC4PE，关闭预装载使 DMA 写入立即生效 */
    s_htim3.Instance->CCMR2 &= ~TIM_CCMR2_OC4PE;

    /* ── DMA2_Stream1：TIM3_CH4, Normal, Word ── */
    s_hdma_tim3_ch4.Instance = DMA2_Stream1;
    s_hdma_tim3_ch4.Init.Request = DMA_REQUEST_TIM3_CH4;
    s_hdma_tim3_ch4.Init.Direction = DMA_MEMORY_TO_PERIPH;
    s_hdma_tim3_ch4.Init.PeriphInc = DMA_PINC_DISABLE;
    s_hdma_tim3_ch4.Init.MemInc = DMA_MINC_ENABLE;
    s_hdma_tim3_ch4.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    s_hdma_tim3_ch4.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    s_hdma_tim3_ch4.Init.Mode = DMA_NORMAL;
    s_hdma_tim3_ch4.Init.Priority = DMA_PRIORITY_LOW;
    s_hdma_tim3_ch4.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&s_hdma_tim3_ch4) != HAL_OK)
        return -1;

    __HAL_LINKDMA(&s_htim3, hdma[TIM_DMA_ID_CC4], s_hdma_tim3_ch4);

    /* ── NVIC ── */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

    /* ── 保存映射、确保输出低电平 ── */
    inst->heater_pwm->map.htim = &s_htim3;
    inst->heater_pwm->map.channel = TIM_CHANNEL_4;
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, BMI088_HEAT_CLOSE_DUTY);

    return 0;
}

void BMI088HeaterStart(BMI088Instance *inst)
{
    /* 温度未初始化 或 已达到目标（带回滞）→ 不加热 */
    if (inst->temperature >= BMI088_TEMP_TARGET + BMI088_HEAT_TEMP_HYST)
    {
        return;
    }
    else if (inst->temperature < BMI088_TEMP_TARGET + BMI088_HEAT_TEMP_HYST)
    {
        /* DMA 正忙（上次加热还未完成）→ 等待 */
        if (HAL_DMA_GetState(&s_hdma_tim3_ch4) == HAL_DMA_STATE_BUSY)
            return;

        /* 前 N-1 项填充加热占空比 */
        for (uint16_t i = 0; i < BMI088_HEAT_PULSE_NUM - 1; i++)
            s_heat_duty[i] = BMI088_HEAT_DUTY_CCR;
        /* 最后一项单独赋 0（安全关断） */
        s_heat_duty[BMI088_HEAT_PULSE_NUM - 1] = 0;

        s_htim3.Instance->RCR = BMI088_HEAT_PULSE_NUM - 1;
        HAL_TIM_PWM_Start_DMA(&s_htim3, TIM_CHANNEL_4, s_heat_duty, BMI088_HEAT_PULSE_NUM);
    }
}

#elif DEVELOPMENT_BOARD == DJI_C
/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    PWMSetDutyRatio(inst->heater_pwm, BMI088_HEAT_CLOSE_DUTY);
}
/*
 * ── DJI-C：5V 加热，不会烧，持续 PWM ──
 */
int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    PWM_Init_Config_s cfg = {.tim_e = inst->heater_pwm->tim_e};
    return PWMRegister(inst->heater_pwm, &cfg);
}

void BMI088HeaterStart(BMI088Instance *inst)
{
    if (inst->temperature >= BMI088_TEMP_TARGET + BMI088_HEAT_TEMP_HYST)
    {
        PWMSetDutyRatio(inst->heater_pwm, BMI088_HEAT_CLOSE_DUTY);
    }
    else if (inst->temperature < BMI088_TEMP_TARGET - BMI088_HEAT_TEMP_HYST)
    {
        PWMSetDutyRatio(inst->heater_pwm, BMI088_HEAT_DUTY);
    }
}

#else  /* DEVELOPMENT_BOARD 不是DM_MC02|DJI_C → 空桩 */
/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
}
int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    (void)inst;
    return 0;
}
void BMI088HeaterStart(BMI088Instance *inst)
{
    (void)inst;
}
#endif /* DEVELOPMENT_BOARD */

#else  /* BMI088_HEAT_USED 未定义 → 空桩 */
/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
}
int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    (void)inst;
    return 0;
}
void BMI088HeaterStart(BMI088Instance *inst)
{
    (void)inst;
}
#endif /* BMI088_HEAT_USED */

#endif /* defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */
