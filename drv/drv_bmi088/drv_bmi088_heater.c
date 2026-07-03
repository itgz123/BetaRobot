/**
 * @file drv_bmi088_heater.c
 * @brief BMI088 加热器驱动 — 双定时器硬件安全方案
 *
 * ── 设计目标 ──
 * DM-MC02 开发板的 BMI088 加热 PWM 控制 24V 加热电路（三极管驱动）。
 * 加热引脚（PB1, TIM3_CH4）高电平时开启加热，持续加热会损坏电路。
 *
 * 断点调试或 CPU 崩溃时，PWM 引脚可能锁死在高电平导致持续加热。
 * 本方案用 TIM3+TIM4 组成纯硬件自停链路，即使 CPU halt 也能在
 * 预设时间内自动关闭加热，无需软件干预。
 *
 * ── 方案：TIM3(PWM+门控) + TIM4(超时+OPM) ──
 *
 * 内部链路（STM32H723, RM0468 Table 317）：
 *   TIM3(从:门控,ITR3) ←── TIM4(主:TRGO=ENABLE)
 *
 * TIM3：PWM 主定时器，CH4→PB1 输出加热 PWM
 *   - PWM2 模式，门控从模式（ITR3 = TIM4 的 TRGO）
 *   - 门控高 → 计数/PWM输出；门控低 → 暂停
 * TIM4：超时定时器，OPM 单脉冲模式
 *   - 内部时钟，TRGO=ENABLE (CNT_EN 信号)
 *   - CEN=1 → CNT_EN=1 → TRGO=高 → TIM3 门控开
 *   - 超时后 OPM 自动清 CEN → TRGO=低 → TIM3 门控关
 *
 * 流程：
 *   软件启 TIM3(CEN=1, 门控低暂不计数) → 启 TIM4(CEN=1, 门控开)
 *   → TIM3 输出 PWM → 超时 → TIM4 OPM 自动清 CEN → 门控关 → 停止
 *   = 硬件完成固定时长加热后自停
 *
 * ── 安全性（CPU halt 或 crash 场景）───
 *   - CPU halt 不会影响 D2 域外设运行
 *   - TIM4 内部时钟继续跑，超时自动关闭门控
 *   - 不需要中断、DMA、软件参与
 *   - 预热建议：CubeMX/IDE 调试配置中确保不冻结 TIM3/TIM4
 *
 * ── 为什么不用外部时钟（脉冲计数）方案 ──
 *   原方案：TIM4 用外部时钟模式 1（ITR2=TIM3_UEV），对 PWM 脉冲计数，
 *   N 个脉冲后 OPM 自动关门。存在"鸡生蛋"死锁：
 *   - TIM4 需要 TIM3 的 UEV 才能计数 → TIM3 需要门控开才能输出 UEV
 *   - 门控需要 TIM4 CEN=1 才能开 → 但 TIM4 的 CNT_EN 在外时钟模式下
 *     可能不会立即变高（需等待第一个外部时钟脉冲）
 *   本方案改用内部时钟超时，完全消除此死锁。
 *
 * ── TIM4 超时计算 ──
 *   APB1 定时器时钟 ≈ 240MHz（STM32H723, 480MHz SYSCLK）
 *   f_cnt = 240MHz / (BMI088_HEAT_TIM4_PSC + 1)
 *   timeout = (BMI088_HEAT_TIM4_ARR + 1) / f_cnt
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
#define BMI088_HEAT_PULSE_NUM 50 /* 每次加热脉冲数（仅供参考，实际用超时控制） */
#endif
#ifndef BMI088_HEAT_DUTY_PERCENT
#define BMI088_HEAT_DUTY_PERCENT 0 /* 加热占空比百分比 (0-100) */
#endif
#ifndef BMI088_HEAT_DUTY
#define BMI088_HEAT_DUTY (BMI088_HEAT_DUTY_PERCENT / 100.0f) /* 加热占空比 (0-1) */
#endif
#ifndef BMI088_HEAT_TEMP_HYST
#define BMI088_HEAT_TEMP_HYST 1.0f /* 温度回滞 (℃) */
#endif
#ifndef BMI088_TEMP_TARGET
#define BMI088_TEMP_TARGET 38.0f /* 加热目标温度 (℃) */
#endif
#ifndef BMI088_HEAT_TIMEOUT_MS
#define BMI088_HEAT_TIMEOUT_MS 10000U /* 单次加热超时 (ms)，默认 1s */
#endif

#define BMI088_HEAT_CLOSE_DUTY 0 /* 关闭占空比 (0-1) */

#if DEVELOPMENT_BOARD == DM_MC02

#include "tim.h"

/* ═══════════════════ TIM3 PWM 参数 ═══════════════════ */
/* PSC=79, ARR=60000 → 50Hz (20ms 周期) */
#define BMI088_HEAT_TIM3_PSC (80 - 1)
#define BMI088_HEAT_TIM3_ARR 60000
/* PWM2 模式：CNT<CCR→无效(LOW), CNT>=CCR→有效(HIGH) */
/* OFF: CCR=ARR+1 → CNT 永远 < CCR → 永远 LOW */
/* HEAT: CCR 根据占空比计算 */
#define BMI088_HEAT_TIM3_CCR_OFF (BMI088_HEAT_TIM3_ARR + 1U)

/* ═══════════════════ TIM4 超时参数 ═══════════════════ */
/*
 * APB1 定时器时钟 ≈ 240MHz（STM32H723）
 *
 * 超时公式：timeout_ms = (PSC+1)*(ARR+1) / 240000
 *
 * 选择 PSC=23999 → f_cnt = 240MHz / 24000 = 10kHz
 * ARR = timeout_ms * 10 - 1
 *
 * 示例：timeout=1000ms → ARR=9999 → 范围 0-65535 ✓
 *       timeout=5000ms → ARR=49999 → 范围 0-65535 ✓
 */
#define BMI088_HEAT_TIM4_PSC 23999U
#define BMI088_HEAT_TIM4_ARR ((BMI088_HEAT_TIMEOUT_MS) * 10U - 1U)

static TIM_HandleTypeDef s_htim3 = {0};
static TIM_HandleTypeDef s_htim4 = {0};

/* 无 DMA，无中断，纯硬件链路 */

/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
    /*
     * 先强制 CCR = OFF（输出立即变低，PWM2 模式下 CCR > ARR 恒为低电平）
     * 再关闭定时器
     */
    __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, BMI088_HEAT_TIM3_CCR_OFF);
    __HAL_TIM_DISABLE(&s_htim4);
    __HAL_TIM_DISABLE(&s_htim3);
}

int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    /*
     * ── DBGMCU 配置：解除 TIM3/TIM4 的调试冻结 ──
     *
     * 调试器（CubeIDE/Keil）默认在 CPU halt 时冻结所有定时器。
     * 清除 APB1LFZ1 中的 TIM3/TIM4 冻结位，使这些定时器在调试暂停时继续运行。
     * 注意：某些调试器可能在下一次 halt 时重新设置这些位。
     * 如果发现调试暂停后加热不停，请在 IDE 的 Debug Configuration 中设置
     * "Timer3/4: NOT stopped during halt"。
     */
    __HAL_DBGMCU_UnFreeze_TIM3();
    __HAL_DBGMCU_UnFreeze_TIM4();
    /*
     * 使能 D1/D3 域调试时钟。TIM3/TIM4 在 D2 域，D2 域时钟本身在 debug halt
     * 时不会停止，但 D1/D3 的时钟可能影响总线矩阵的互联信号传输。
     */
    SET_BIT(DBGMCU->CR, DBGMCU_CR_DBG_CKD1EN | DBGMCU_CR_DBG_CKD3EN);
    __DSB();

    /* ── 时钟使能 ── */
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* ── GPIO PB1 → TIM3_CH4 (AF2) ── */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*
     * ═══════════════════ TIM3：PWM 主定时器 + 门控从模式 ═══════════════════
     *
     *   - 基础：PSC=79, ARR=60000 → 50Hz (20ms 周期)
     *   - 从模式：门控模式，门控源 = ITR3 (TIM4 的 TRGO)
     *     TIM3 ITR3 → 连接 TIM4（RM0468 Table 317）
     *     门控高电平 → 计数器运行；低电平 → 暂停
     *   - 通道 4：PWM2 模式，OCPolarity=HIGH
     *     PWM2：CNT <  CCR → inactive (LOW)
     *           CNT >= CCR → active   (HIGH)
     *     CCR=ARR+1 (60001)：CNT 永远 < CCR → 永远 LOW（安全关断）
     *     加热时根据占空比设置 CCR
     */
    {
        TIM_ClockConfigTypeDef sClockSourceConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};
        TIM_SlaveConfigTypeDef sSlaveConfig = {0};
        TIM_OC_InitTypeDef sConfigOC = {0};

        s_htim3.Instance = TIM3;
        s_htim3.Init.Prescaler = BMI088_HEAT_TIM3_PSC;
        s_htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
        s_htim3.Init.Period = BMI088_HEAT_TIM3_ARR;
        s_htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&s_htim3) != HAL_OK)
            return -1;

        sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&s_htim3, &sClockSourceConfig) != HAL_OK)
            return -1;

        if (HAL_TIM_PWM_Init(&s_htim3) != HAL_OK)
            return -1;

        /* 主模式：TRGO = 更新事件（保留，供将来脉冲计数扩展使用） */
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&s_htim3, &sMasterConfig) != HAL_OK)
            return -1;

        /* 从模式：门控，ITR3 = TIM4 的 TRGO */
        sSlaveConfig.SlaveMode = TIM_SLAVEMODE_GATED;
        sSlaveConfig.InputTrigger = TIM_TS_ITR3;
        sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_NONINVERTED;
        sSlaveConfig.TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1;
        sSlaveConfig.TriggerFilter = 0;
        if (HAL_TIM_SlaveConfigSynchro(&s_htim3, &sSlaveConfig) != HAL_OK)
            return -1;

        /* 通道 4：PWM2 模式，初始占空比 = OFF（CCR > ARR → 永远 LOW） */
        sConfigOC.OCMode = TIM_OCMODE_PWM2;
        sConfigOC.Pulse = BMI088_HEAT_TIM3_CCR_OFF;
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        if (HAL_TIM_PWM_ConfigChannel(&s_htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
            return -1;
    }

    /*
     * ═══════════════════ TIM4：超时门控发生器 ═══════════════════
     *
     *   - 内部时钟 + OPM 单脉冲模式
     *   - TRGO = ENABLE（CNT_EN 信号）
     *   - CEN=1 → CNT_EN=1 → TRGO=高 → 打开 TIM3 门控
     *   - 超时后 OPM 自动清零 CEN → TRGO=低 → 关闭 TIM3 门控
     *   - 不使用从模式（不需要外部时钟，纯内部时钟跑超时）
     */
    {
        TIM_ClockConfigTypeDef sClockSourceConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};

        s_htim4.Instance = TIM4;
        s_htim4.Init.Prescaler = BMI088_HEAT_TIM4_PSC;
        s_htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
        s_htim4.Init.Period = BMI088_HEAT_TIM4_ARR;
        s_htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_Base_Init(&s_htim4) != HAL_OK)
            return -1;

        sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&s_htim4, &sClockSourceConfig) != HAL_OK)
            return -1;

        /* 主模式：TRGO = CNT_EN（计数器使能信号）
           CEN=1 → CNT_EN=1 → TRGO=高 → TIM3 门控开
           CEN=0 → CNT_EN=0 → TRGO=低 → TIM3 门控关 */
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&s_htim4, &sMasterConfig) != HAL_OK)
            return -1;

        /* OPM：单脉冲模式，超时后硬件自动清零 CEN
           HAL 不封装此设置，直接写寄存器 */
        TIM4->CR1 |= TIM_CR1_OPM;
    }

    /* ── 保存映射 ── */
    inst->heater_pwm->map.htim = &s_htim3;
    inst->heater_pwm->map.channel = TIM_CHANNEL_4;

    return 0;
}

void BMI088HeaterStart(BMI088Instance *inst)
{
    /* 温度已达目标 + 回滞 → 无需加热 */
    if (inst->temperature >= BMI088_TEMP_TARGET + BMI088_HEAT_TEMP_HYST)
        return;

    /* TIM4 还在运行 → 上次加热未完成，跳过本次 */
    if (TIM4->CR1 & TIM_CR1_CEN)
        return;

    /*
     * 启动顺序（遵循参考手册"门控模式"要求：先 CEN=1 再开门）：
     *   1. 重置 CNT
     *   2. 设置加热占空比
     *   3. 先启动 TIM3（CEN=1，门控此时为低，不计数）
     *   4. 再启动 TIM4（CEN=1 → TRGO=高 → 门控开 → TIM3 开始输出 PWM）
     */
    __HAL_TIM_SET_COUNTER(&s_htim3, 0);
    __HAL_TIM_SET_COUNTER(&s_htim4, 0);
    __HAL_TIM_CLEAR_FLAG(&s_htim4, TIM_FLAG_UPDATE);

    /* 计算加热 CCR：PWM2 模式下有效时间 = (ARR+1-CCR) 个 tick */
    {
        uint32_t heat_ccr;
        if (BMI088_HEAT_DUTY_PERCENT >= 100)
            heat_ccr = 0; /* 100% 占空比：CNT>=0 永远成立 → 永远 HIGH */
        else if (BMI088_HEAT_DUTY_PERCENT == 0)
            heat_ccr = BMI088_HEAT_TIM3_CCR_OFF; /* 0% 占空比 → 永远 LOW */
        else
            /*
             * PWM2: duty = (ARR+1-CCR) / (ARR+1)
             * CCR = (ARR+1) * (1 - duty/100)
             *     = (ARR+1) * (100 - duty_percent) / 100
             */
            heat_ccr = (uint32_t)((uint64_t)(BMI088_HEAT_TIM3_ARR + 1U) * (100U - (uint32_t)BMI088_HEAT_DUTY_PERCENT) / 100U);
        __HAL_TIM_SET_COMPARE(&s_htim3, TIM_CHANNEL_4, heat_ccr);
    }

    /*
     * 启动：先武装 TIM3（门控模式要求 CEN=1 先于门控开），再启动 TIM4
     * 不能用 HAL_TIM_Base_Start()，其内部状态校验会干扰 CEN 时序
     */
    TIM3->CR1 |= TIM_CR1_CEN;
    TIM4->CR1 |= TIM_CR1_CEN;
    /*
     * 此后完全由硬件控制：
     *   TIM4 内部计数 → 超时 → OPM 自动清零 CEN
     *   → TRGO(ENABLE)→低 → TIM3 门控关 → PWM 停止
     *   软件无需干预，CPU halt 后也会自动完成
     */
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
