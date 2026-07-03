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

/* 公共宏定义（所有开发板通用） */
#ifndef BMI088_HEAT_PULSE_NUM
#define BMI088_HEAT_PULSE_NUM 500 /* 每次加热脉冲数 */
#endif
#ifndef BMI088_HEAT_DUTY_PERCENT
#define BMI088_HEAT_DUTY_PERCENT 0 /* 加热占空比百分比 (0-100) */
#endif
#ifndef BMI088_HEAT_DUTY
#define BMI088_HEAT_DUTY (BMI088_HEAT_DUTY_PERCENT / 100.0f) /* 加热占空比 (0-1) */
#endif
#ifndef BMI088_HEAT_TEMP_HYST
#define BMI088_HEAT_TEMP_HYST 1.0f /* 温度回滞 */
#endif
#ifndef BMI088_TEMP_TARGET
#define BMI088_TEMP_TARGET 38.0f /* 加热目标温度 (℃) */
#endif

#define BMI088_HEAT_CLOSE_DUTY 0 /* 关闭占空比 (0-1) */

#if DEVELOPMENT_BOARD == DM_MC02
/*
 * ── DM-MC02：TIM3+TIM4 双定时器链路安全方案 ──
 *
 * TIM3 输出 PWM（PB1），TIM4 作为硬件脉冲计数器。
 * 纯硬件控制，不依赖 CPU/DMA/中断，CPU halt 时继续跑完剩余脉冲后自停。
 *
 * 内部链路（STM32H723，参考 RM0468 Table 317）：
 *   TIM3(主:TRGO=UEV) ──ITR2──→ TIM4(从:外部时钟,ARR=49,OPM=1)
 *   TIM3(从:门控,ITR3) ←──ITR2── TIM4(主:TRGO=OC1REF)
 *
 * 流程：
 *   启 TIM3(门控低不计时) → 启 TIM4(CEN=1→OC1REF=HIGH→TRGO=高→门控开)
 *   → TIM3 输出 PWM，每周期 UEV 递增 TIM4 CNT
 *   → 50 个脉冲后 TIM4 CNT=49=ARR → UEV → OPM 清 CEN
 *   → TIM4 CEN=0 → OC1REF=LOW → TRGO=低 → TIM3 门控关 → 停止
 *   = 硬件完成 50 个脉冲，最多 50×20ms=1000ms
 *
 * 安全性（CPU halt 时）：
 *   - CPU halt 不影响硬件计数器链路
 *   - 剩余脉冲继续跑完，最多 1000ms 后硬件自停
 *   - 无需 DBGMCU 冻结，无需 DMA/中断
 */
#include "tim.h"

/* 定时器配置：PSC=79, ARR=60000 → 480MHz/2/80/60000=50Hz */
#define BMI088_HEAT_TIM_PRC (80 - 1)
#define BMI088_HEAT_TIM_PRD 60000
#define BMI088_HEAT_ACTIVE_TICKS 1U
#define BMI088_HEAT_OFF_CCR (BMI088_HEAT_TIM_PRD + 1U)
#define BMI088_HEAT_DUTY_CCR (BMI088_HEAT_OFF_CCR - BMI088_HEAT_ACTIVE_TICKS)

static TIM_HandleTypeDef s_htim3 = {0};
static TIM_HandleTypeDef s_htim4 = {0};
uint32_t test = 0;

/* 无 DMA，无中断，纯硬件链路 */

/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
    __HAL_TIM_DISABLE(&s_htim4);                                         /* 停 TIM4，门控信号变低 */
    __HAL_TIM_DISABLE(&s_htim3);                                         /* 停 TIM3 */
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, BMI088_HEAT_OFF_CCR); /* 强制输出低电平 */
}

int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    // /*覆盖调试器可能设置的 DBG_TIMx_STOP 冻结位，确保硬件自停链路
    //     在 CPU halt 时仍能跑完剩余脉冲并自动关闭 */
    test = DBGMCU->APB1LFZ1;
    __HAL_DBGMCU_UnFreeze_TIM3();
    test = DBGMCU->APB1LFZ1;
    __HAL_DBGMCU_UnFreeze_TIM4();
    test = DBGMCU->APB1LFZ1;

    /* ── DBGMCU：调试暂停时 TIM3/TIM4 继续运行 ──
       1. 清除 APB1LFZ1 中 TIM3(bit1)/TIM4(bit2) 冻结位
       2. 使能 DBGMCU_CR 中 D1/D3 域调试时钟 (CKD1EN=bit21, CKD3EN=bit22)
          防止 CPU halt 时域时钟被切断导致 D2 外设停摆 */
    CLEAR_BIT(DBGMCU->APB1LFZ1, DBGMCU_APB1LFZ1_DBG_TIM3 | DBGMCU_APB1LFZ1_DBG_TIM4);
    test = DBGMCU->APB1LFZ1;
    SET_BIT(DBGMCU->CR, DBGMCU_CR_DBG_CKD1EN | DBGMCU_CR_DBG_CKD3EN);
    test = DBGMCU->APB1LFZ1;
    __DSB();
    test = DBGMCU->APB1LFZ1;

    /* ── 时钟使能（相当于 CubeMX HAL_TIM_Base_MspInit 中完成）─── */
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* ── GPIO PB1 → TIM3_CH4 (AF2)（相当于 CubeMX HAL_TIM_MspPostInit 中完成）─── */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* ═══════════════════ TIM3：PWM 主定时器 + 门控从模式 ═══════════════════ */
    {
        TIM_ClockConfigTypeDef sClockSourceConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};
        TIM_SlaveConfigTypeDef sSlaveConfig = {0};
        TIM_OC_InitTypeDef sConfigOC = {0};

        /* 步骤 1：基础初始化 */
        s_htim3.Instance = TIM3;
        s_htim3.Init.Prescaler = BMI088_HEAT_TIM_PRC;
        s_htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
        s_htim3.Init.Period = BMI088_HEAT_TIM_PRD;
        s_htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&s_htim3) != HAL_OK)
            return -1;

        /* 步骤 2：时钟源（内部时钟，模仿 CubeMX 标准流程） */
        sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&s_htim3, &sClockSourceConfig) != HAL_OK)
            return -1;

        /* 步骤 3：PWM 初始化（设置通道状态为 READY，消除手动 STATE_SET hack） */
        if (HAL_TIM_PWM_Init(&s_htim3) != HAL_OK)
            return -1;

        /* 步骤 4：主模式 — TRGO = 更新事件（每 PWM 周期输出一个脉冲给 TIM4） */
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&s_htim3, &sMasterConfig) != HAL_OK)
            return -1;

        /* 步骤 5：从模式 — 门控模式，门控源 = ITR3（TIM4 的 TRGO）
           注意：TIM3 的 ITR3 连接 TIM4（RM0468 Table 317），不是 ITR2 */
        sSlaveConfig.SlaveMode = TIM_SLAVEMODE_GATED;
        sSlaveConfig.InputTrigger = TIM_TS_ITR3;
        sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
        sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
        sSlaveConfig.TriggerFilter = 0;
        if (HAL_TIM_SlaveConfigSynchro(&s_htim3, &sSlaveConfig) != HAL_OK)
            return -1;

        /* 使用 PWM2，确保停止在 CNT=0 时输出为低电平。 */
        sConfigOC.OCMode = TIM_OCMODE_PWM2;
        sConfigOC.Pulse = BMI088_HEAT_OFF_CCR;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        if (HAL_TIM_PWM_ConfigChannel(&s_htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
            return -1;
    }

    /* ═══════════════════ TIM4：脉冲计数器 + 门控信号发生器 ═══════════════════ */
    {
        TIM_ClockConfigTypeDef sClockSourceConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};
        TIM_SlaveConfigTypeDef sSlaveConfig = {0};

        /* 步骤 1：基础初始化 */
        s_htim4.Instance = TIM4;
        s_htim4.Init.Prescaler = 0;
        s_htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
        s_htim4.Init.Period = BMI088_HEAT_PULSE_NUM - 1; /* 49，50 个脉冲溢出 */
        s_htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&s_htim4) != HAL_OK)
            return -1;

        /* 步骤 2：时钟源（内部时钟，后续从模式会覆盖） */
        sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&s_htim4, &sClockSourceConfig) != HAL_OK)
            return -1;

        /* 步骤 3：从模式 — 外部时钟模式 1，时钟源 = ITR2（TIM3 的 TRGO=UEV） */
        sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
        sSlaveConfig.InputTrigger = TIM_TS_ITR2;
        sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
        sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
        sSlaveConfig.TriggerFilter = 0;
        if (HAL_TIM_SlaveConfigSynchro(&s_htim4, &sSlaveConfig) != HAL_OK)
            return -1;

        /* 步骤 4：主模式 — 使能信号作为 TRGO → 门控 TIM3
           TIM4 在 OPM 自动清 CEN 后，TRGO 也会随之拉低，硬件关闭 TIM3 门控。 */
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&s_htim4, &sMasterConfig) != HAL_OK)
            return -1;

        /* 步骤 5：单脉冲模式 — N 个脉冲后硬件自动清 CEN（无 HAL 封装） */
        TIM4->CR1 |= TIM_CR1_OPM;
    }

    /* ── 保存映射 ── */
    inst->heater_pwm->map.htim = &s_htim3;
    inst->heater_pwm->map.channel = TIM_CHANNEL_4;

    return 0;
}

void BMI088HeaterStart(BMI088Instance *inst)
{
    if (inst->temperature >= BMI088_TEMP_TARGET + BMI088_HEAT_TEMP_HYST)
        return;

    /* TIM4 还在运行 → 上次加热未完成 */
    if (TIM4->CR1 & TIM_CR1_CEN)
        return;

    /* 重置计数器 */
    __HAL_TIM_SET_COUNTER(&s_htim3, 0);
    __HAL_TIM_SET_COUNTER(&s_htim4, 0);
    __HAL_TIM_CLEAR_FLAG(&s_htim4, TIM_FLAG_UPDATE);

    /* 设回加热占空比（初始化时 CCR4=0） */
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, BMI088_HEAT_DUTY_CCR);

    /* 先启 TIM4（OC1REF=HIGH → TRGO=高 → 门控开），再启 TIM3（门控已开立即计时）
       注意：不能使用 HAL_TIM_Base_Start，其内部状态校验会打乱 CEN 时序 */
    TIM4->CR1 |= TIM_CR1_CEN;
    TIM3->CR1 |= TIM_CR1_CEN;
    /* TIM3 输出 50 个 PWM → 每周期 UEV 递增 TIM4 CNT → CNT=49 → OPM 清 TIM4 CEN
       → OC1REF=LOW → TRGO=低 → TIM3 门控关 → 停止 */
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
