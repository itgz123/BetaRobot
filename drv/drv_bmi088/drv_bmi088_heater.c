/**
 * @file drv_bmi088_heater.c
 * @brief BMI088 加热器驱动 — TIM8 OPM+RCR 硬件安全方案
 *
 * ── 设计目标 ──
 * DM-MC02 开发板的 BMI088 加热 PWM 控制 24V 加热电路（三极管驱动）。
 * 加热引脚（PB1, TIM8_CH3N）高电平时开启加热，持续加热会损坏电路。
 *
 * CPU 崩溃（HardFault 等）或调试断点（CPU halt）时，本方案用 TIM8 的
 * OPM 单脉冲模式 + RCR 重复计数器，输出固定数量脉冲后硬件自动关闭，
 * 无需软件干预。
 *
 * ── 方案：TIM8 OPM + RCR ──
 *
 * TIM8：APB2 高级定时器，CH3N→PB1 输出加热 PWM
 *   - PWM2 模式，OPM 单脉冲模式
 *   - RCR 重复计数器：RCR=N 意味着 N+1 次计数器溢出后产生 UEV
 *   - OPM=1：UEV 发生时硬件自动清零 CEN，定时器停止
 *   - 组合：OPM=1 + RCR=N → 产生 N+1 个 PWM 脉冲后自动停止
 *
 * 流程：
 *   软件设 CCR3(占空比) → UG 重载 RCR → CEN=1 启动
 *   → TIM8 输出 PWM → RCR+1 次溢出后 UEV → OPM 硬件清零 CEN → 停止
 *   = 硬件完成固定数量脉冲后自停
 *
 * ── 安全性分析 ──
 *
 * 【场景 1：正常运行 + 软件崩溃（HardFault/NMI）】
 *   ✓ 受保护。CPU 崩溃不影响 D2 域外设（TIM8 在 D2 域 APB2）。
 *     TIM8 内部时钟继续跑，OPM+RCR 保证固定脉冲数后自动停止。
 *     STM32H7 的 D2 域独立于 CPU(D1) 域运行。
 *
 * 【场景 2：调试断点（CPU halt via debugger）】
 *   ✓ 受保护。DBGMCU Freeze TIM8 在 CPU halt 瞬间：
 *     1. 计数器立即冻结
 *     2. MOE 硬件复位（等同于 MOE=0）
 *     3. OSSI=1 → 输出强制进入空闲状态
 *     4. OIS3N=0 → CH3N 强制 LOW → MOSFET 关闭
 *     关键：Freeze 复位 MOE 是硬件行为，不依赖时钟持续运行。
 *           后续 IWDG 复位 MCU 清理剩余状态。
 *
 * 【场景 3：IWDG 看门狗（最终安全网）】
 *   ✓ IWDG 运行在 LSI（独立于系统时钟），CPU halt 不影响 IWDG。
 *     配置适当超时（如 1s）可在 CPU 长时间 halt 时复位 MCU，
 *     复位后所有外设回到初始状态，加热关闭。
 *
 * ── 为什么 TIM8 而不是 TIM3 ──
 *   TIM3 挂载在 APB1(D2域)，STM32H723 value line 芯片在 CPU halt 时
 *   APB1 时钟会被硬件门控停止。当 DBGMCU Freeze 触发时，若时钟已停，
 *   BDTR/MOE 的硬件操作可能无法完成，输出状态不确定。
 *   TIM8 挂载在 APB2，时钟门控延迟更长（实测约1s），DBGMCU Freeze 在
 *   CPU halt 瞬间立即触发，此时 APB2 时钟仍有效，MOE 可被安全复位。
 *
 * ── TIM8 参数计算 ──
 *   APB2 定时器时钟 ≈ 240MHz（STM32H723, D2PPRE2=DIV2）
 *   f_cnt = 240MHz / (PSC + 1) = 240MHz / 80 = 3MHz
 *   f_pwm = 3MHz / (ARR + 1) = 3MHz / 60001 ≈ 50Hz (20ms 周期)
 *   脉冲数 = RCR + 1，每脉冲 20ms
 *   加热时长 = (RCR + 1) × 20ms
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
#define BMI088_HEAT_DUTY_PERCENT 100 /* 加热占空比百分比 (0-100) */
#endif
#ifndef BMI088_HEAT_DUTY
#define BMI088_HEAT_DUTY (BMI088_HEAT_DUTY_PERCENT / 100.0f) /* 加热占空比 (0-1) */
#endif
#ifndef BMI088_HEAT_TEMP_HYST
#define BMI088_HEAT_TEMP_HYST 1.0f /* 温度回滞 (℃) */
#endif
#ifndef BMI088_TEMP_TARGET
#define BMI088_TEMP_TARGET 50.0f /* 加热目标温度 (℃) */
#endif
#ifndef BMI088_HEAT_TIMEOUT_MS
#define BMI088_HEAT_TIMEOUT_MS 10000U /* 单次加热超时 (ms)，默认 1s */
#endif

#define BMI088_HEAT_CLOSE_DUTY 0 /* 关闭占空比 (0-1) */

#if DEVELOPMENT_BOARD == DM_MC02

/* ═══════════════════ TIM8 OPM+RCR 参数 ═══════════════════ */
/*
 * APB2 定时器时钟 ≈ 240MHz（STM32H723, D2PPRE2=DIV2）
 * PSC=79, f_cnt=240MHz/80=3MHz
 * ARR=60000, f_pwm=3MHz/60001≈50Hz (20ms 周期)
 *
 * OPM + RCR:
 *   RCR=N, OPM=1 → N+1 次计数器溢出后 UEV → 硬件自动清零 CEN → 停止
 *   脉冲数 = RCR + 1, 每脉冲 20ms
 *   加热时长 = (RCR + 1) × 20ms
 */
#define BMI088_HEAT_TIM8_PSC 79U    /* PSC = 80 - 1 */
#define BMI088_HEAT_TIM8_ARR 60000U /* 50Hz PWM */
/* PWM2: CNT<CCR→LOW, CNT>=CCR→HIGH; CH3N OC3N=OC3REF */
#define BMI088_HEAT_TIM8_CCR_OFF (BMI088_HEAT_TIM8_ARR + 1U) /* 恒为 LOW */

/* 脉冲数：超时时间 / 每脉冲周期，向上取整 */
#define BMI088_HEAT_TIM8_PULSE_NUM \
    (((BMI088_HEAT_TIMEOUT_MS) + 20U - 1U) / 20U)
#define BMI088_HEAT_TIM8_RCR (BMI088_HEAT_TIM8_PULSE_NUM - 1U)

static TIM_HandleTypeDef s_htim8 = {0};

/* 无 DMA，无中断，纯硬件链路 */

/* daemon 离线回调：确保加热关闭（软件安全网） */
void BMI088_HeaterFaultCallback(void *owner)
{
    (void)owner;
    /*
     * 无条件禁用主输出 → CH3N 进入高阻态
     * 板级外部下拉 → 低电平 → MOSFET 关闭
     * 同时停止计数器，阻止下次意外启动
     */
    __HAL_TIM_MOE_DISABLE_UNCONDITIONALLY(&s_htim8);
    TIM8->CR1 &= ~TIM_CR1_CEN;
}

int8_t BMI088_HeaterInit(BMI088Instance *inst)
{
    /* ── DBGMCU 配置：CPU halt 时立即冻结 TIM8，MOE 硬件复位 → 输出强制 LOW ── */
    /*
     * Freeze TIM8（DBGMCU_APB2FZ1 bit1 = 1）：
     *   当 CPU halt（调试断点）时：
     *   1. TIM8 计数器立即冻结
     *   2. MOE 位被硬件复位（等同于清除 BDTR.MOE）
     *   3. OSSI=1 → OC/OCN 输出强制进入空闲状态
     *   4. OIS3N=0 → CH3N 强制为 LOW → MOSFET 关闭 → 安全
     *
     *   关键优势：MOE 复位发生在 CPU halt 瞬间（时钟仍在运行），
     *   不依赖 1s 后的时钟门控。UnFreeze 方案在时钟停止后才失效，
     *   而 Freeze 在时钟有效时就已确保输出变为安全状态。
     */
    __HAL_DBGMCU_FREEZE_TIM8();
    __DSB();

    /*
     * ── PWR 域控制：保证 CPU halt 时 APB2 总线时钟持续运行 ──
     *
     * Freeze 触发 MOE 硬件复位是 TIM8 内部寄存器操作，依赖 APB2 时钟。
     * 没有时钟，TIM8 无法执行任何硬件动作。因此必须在 PWR 层面明确告诉
     * 硬件：即使 CPU halt，D2（含 APB2）和 D3（含 RCC）域也不得掉电。
     *
     * PWR_CPUCR.RUN_D3 = 1：
     *   强制 D3 域（含 RCC）保持在 Run 模式。D3 控制所有时钟分配，
     *   D3 掉电则 APB1/APB2 全部失去时钟源。
     *
     * PWR_CPUCR.PDDS_Dx = 0：
     *   PDDS_D1=0 → D1 域断电目标为 DStop（非 DStandby）
     *   PDDS_D2=0 → D2 域断电目标为 DStop（非 DStandby）
     *   明确禁止 D1/D2 进入 DStandby，保留外设总线矩阵供电。
     */
    PWR->CPUCR |= PWR_CPUCR_RUN_D3;
    PWR->CPUCR &= ~(PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2);
    __DSB();

    /* ── 时钟使能 ── */
    __HAL_RCC_TIM8_CLK_ENABLE();

    /*
     * ── RCC 低功耗时钟保持：防止 CSleep 期间 TIM8 时钟被门控 ──
     *
     * RCC_APB2LPENR.TIM8LPEN = 1：
     *   根据 RM0433：CPU 处于 CSleep 模式时，每个外设时钟由 PERxLPEN
     *   位单独控制。TIM8LPEN=1 → CPU Sleep 期间 RCC 继续输出 TIM8
     *   内核时钟和总线接口时钟，保证 Freeze 触发的 MOE 复位能实际执行。
     */
    RCC->APB2LPENR |= RCC_APB2LPENR_TIM8LPEN;
    __DSB();

    /* ── GPIO PB1 → TIM8_CH3N (AF3) ── */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF3_TIM8;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*
     * ═══════════════════ TIM8：OPM+RCR 单定时器方案 ═══════════════════
     *
     *   - 时基：PSC=79, ARR=60000 → 50Hz (20ms 周期)
     *   - RCR：重复计数器，RCR 次溢出后产生 UEV
     *   - OPM：单脉冲模式，UEV 发生后硬件自动清零 CEN
     *   - CH3N：PWM2 模式，CC3NP=0 → OC3N = OC3REF
     *     PWM2 OC3REF：CNT <  CCR → LOW
     *                  CNT >= CCR → HIGH
     *   - 安全关断：CCR=ARR+1 → CNT 永远 < CCR → 恒 LOW
     *   - 空闲状态：OSSI=1, OIS3N=0 → CEN=0 后 CH3N 强制 LOW
     *   - DBGMCU Freeze：CPU halt 瞬间冻结计数器+复位 MOE → 输出强制 LOW
     */
    {
        TIM_ClockConfigTypeDef sClockSourceConfig = {0};
        TIM_MasterConfigTypeDef sMasterConfig = {0};
        TIM_OC_InitTypeDef sConfigOC = {0};
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

        /* --- 时基 --- */
        s_htim8.Instance = TIM8;
        s_htim8.Init.Prescaler = BMI088_HEAT_TIM8_PSC;
        s_htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
        s_htim8.Init.Period = BMI088_HEAT_TIM8_ARR;
        s_htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        s_htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        s_htim8.Init.RepetitionCounter = BMI088_HEAT_TIM8_RCR;
        if (HAL_TIM_Base_Init(&s_htim8) != HAL_OK)
            return -1;

        sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&s_htim8, &sClockSourceConfig) != HAL_OK)
            return -1;

        /* PWM 初始化（高级定时器通道配置需要先调用 PWM_Init） */
        if (HAL_TIM_PWM_Init(&s_htim8) != HAL_OK)
            return -1;

        /* 主模式：不使用 */
        sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
        sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
        if (HAL_TIMEx_MasterConfigSynchronization(&s_htim8, &sMasterConfig) != HAL_OK)
            return -1;

        /* --- 通道 3 (CH3N) PWM 配置 --- */
        sConfigOC.OCMode = TIM_OCMODE_PWM2;
        sConfigOC.Pulse = BMI088_HEAT_TIM8_CCR_OFF; /* 初始 = 关闭 */
        sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
        sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH; /* CC3NP=0 → OC3N=OC3REF */
        sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
        sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;   /* OIS3 = 0 */
        sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET; /* OIS3N = 0（空闲=低电平） */
        if (HAL_TIM_PWM_ConfigChannel(&s_htim8, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
            return -1;

        /* --- BDTR：死区/刹车配置 --- */
        sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
        sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE; /* CEN=0 时强制空闲状态 */
        sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
        sBreakDeadTimeConfig.DeadTime = 0;
        sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
        sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_LOW;
        sBreakDeadTimeConfig.BreakFilter = 0;
        sBreakDeadTimeConfig.Break2State = TIM_BREAK_DISABLE;
        sBreakDeadTimeConfig.Break2Polarity = TIM_BREAKPOLARITY_LOW;
        sBreakDeadTimeConfig.Break2Filter = 0;
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
        if (HAL_TIMEx_ConfigBreakDeadTime(&s_htim8, &sBreakDeadTimeConfig) != HAL_OK)
            return -1;

        /*
         * ══ 手动寄存器操作（HAL API 限制，需要直接操作寄存器）══
         */

        /* 1. HAL TIM_OC3_SetConfig 会清零 CC3NE，手动恢复 */
        TIM8->CCER |= TIM_CCER_CC3NE;

        /* 2. OPM：单脉冲模式（HAL OPM API 仅支持 CH1/CH2） */
        TIM8->CR1 |= TIM_CR1_OPM;

        /* 3. UG 生成软件更新事件：加载 RCR→REP_CNT 影子寄存器，清零 CNT
         *    不设 URS，因为 BMI088HeaterStart 中也需要 UG 来重载 RCR */
        TIM8->EGR = TIM_EGR_UG;
        TIM8->SR = ~TIM_SR_UIF; /* 清除 UG 产生的 UIF 标志 */

        /* 4. MOE：主输出使能。HAL_TIMEx_ConfigBreakDeadTime 全写 BDTR
         *    会清零 MOE，必须在 BDTR 配置后手动恢复。
         *    OSSI=1 + OIS3N=0 保证空闲时 CH3N = LOW（安全）。 */
        TIM8->BDTR |= TIM_BDTR_MOE;
    }

    /* ── 保存映射 ── */
    inst->heater_pwm->map.htim = &s_htim8;
    inst->heater_pwm->map.channel = TIM_CHANNEL_3;

    return 0;
}

void BMI088HeaterStart(BMI088Instance *inst)
{
    float target = BMI088_TEMP_TARGET;
    float hyst = BMI088_HEAT_TEMP_HYST;
    uint8_t is_running = (TIM8->CR1 & TIM_CR1_CEN) ? 1 : 0;

    /*
     * ── 滞回控温逻辑 ──
     *
     *    temp < target - hyst  → 启动加热（开启脉冲串）
     *    temp > target + hyst  → 停止加热（立即关断，不等 OPM 完成）
     *    死区 [target-hyst, target+hyst] → 维持当前状态
     *
     *  例：target=38, hyst=1:
     *    < 37.0℃ → 开始加热
     *     37.0~39.0℃ → 不操作（正在加热则等 OPM 自己停）
     *    > 39.0℃ → 立即关断
     */
    if (inst->temperature < target - hyst)
    {
        /* 温度低于下限：需要加热 */
        if (is_running)
            return; /* 已经在加热，让 OPM 自行完成 */
    }
    else if (inst->temperature > target + hyst)
    {
        /* 温度超过上限：立即关断加热 */
        if (is_running)
        {
            TIM8->CR1 &= ~TIM_CR1_CEN;   /* 停止计数器 */
            TIM8->BDTR &= ~TIM_BDTR_MOE; /* 禁用主输出 → CH3N 强制空闲 */
        }
        return;
    }
    else
    {
        /* 在滞回死区内：不启动新加热，也不打断当前加热 */
        return;
    }

    /*
     * ── 启动加热脉冲串 ──
     *
     * 启动序列：
     *   1. 计算并设置加热占空比（CCR3）
     *   2. UG 重载 RCR→REP_CNT，清零 CNT
     *   3. 恢复 MOE + CEN=1 启动计数器
     *
     * 启动后由硬件控制：
     *   每次溢出 → REP_CNT 递减
     *   REP_CNT → 0 → UEV → OPM 硬件清零 CEN → 停止
     *   OSSI=1, OIS3N=0 → CH3N 强制 LOW → MOSFET 关闭
     */
    {
        uint32_t heat_ccr;
        if (BMI088_HEAT_DUTY_PERCENT >= 100)
            heat_ccr = 0; /* 100% 占空比：CNT>=0 永远成立 → 永远 HIGH */
        else if (BMI088_HEAT_DUTY_PERCENT == 0)
            heat_ccr = BMI088_HEAT_TIM8_CCR_OFF; /* 0% 占空比 → 永远 LOW */
        else
            /*
             * PWM2: duty = (ARR+1-CCR) / (ARR+1)
             * CCR = (ARR+1) * (1 - duty/100)
             *     = (ARR+1) * (100 - duty_percent) / 100
             */
            heat_ccr = (uint32_t)((uint64_t)(BMI088_HEAT_TIM8_ARR + 1U) *
                                  (100U - (uint32_t)BMI088_HEAT_DUTY_PERCENT) / 100U);
        TIM8->CCR3 = heat_ccr;
    }

    /* UG 生成软件更新事件：重载 RCR→REP_CNT，清零 CNT=0
     * CEN=0 时 UG 产生 UEV 不会触发 OPM 停止 */
    TIM8->EGR = TIM_EGR_UG;
    TIM8->SR = ~TIM_SR_UIF;

    /* 恢复 MOE 后启动计数器。Freeze 可能在 CPU halt 时复位了 MOE，
     * 每次启动都必须确保 MOE=1。HAL 函数不能用（状态机不兼容 OPM） */
    TIM8->BDTR |= TIM_BDTR_MOE;
    TIM8->CR1 |= TIM_CR1_CEN;
}

#elif DEVELOPMENT_BOARD == DJI_C
/* daemon 离线回调：确保加热关闭 */
void BMI088_HeaterFaultCallback(void *owner)
{
    BMI088Instance *inst = (BMI088Instance *)owner;
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
