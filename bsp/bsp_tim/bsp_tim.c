/**
 * @file bsp_tim.c
 * @brief 定时器驱动封装实现（PWM + 编码器）
 *
 * @note 只负责实例管理、回调分发，不负责硬件配置
 */

#include "bsp_tim.h"
#include "app_cfg.h"

#ifdef BSP_TIM_USED

#if (ENCODER_INSTANCE_NUM > 0) || (PWM_INSTANCE_NUM > 0)

#include "bsp_log.h"
#include "bsp_dwt.h"
#include "string.h"

/*============= PWM 部分 =============*/

/*------------- 私有变量 --------------*/
static uint8_t s_pwm_idx = 0;
#ifndef BSP_TIM_LOG_LIMIT
#define BSP_TIM_LOG_LIMIT 10
#endif                                                     // !BSP_TIM_LOG_LIMIT
LOG_INSTANCE_DEF(g_tim_log, "bsp_tim", BSP_TIM_LOG_LIMIT); /* TIM 日志实例（PWM + 编码器共用） */
#if PWM_INSTANCE_NUM > 0
static PWMInstance *s_pwm_instance[PWM_INSTANCE_NUM] = {NULL};
#else
static PWMInstance **s_pwm_instance = NULL;
#endif

/*------------- PWM 私有辅助 --------------*/

/* CCMR1/CCMR2 的 OCxM[2:0]：输出比较模式 */
#define PWM_OCM_FORCED_INACTIVE 4u /* 100：引脚强制无效电平（恒低） */
#define PWM_OCM_FORCED_ACTIVE 5u   /* 101：引脚强制有效电平（恒高） */
#define PWM_OCM_PWM1 6u            /* 110：PWM 模式 1（与 CubeMX 一致） */

/**
 * @brief 取定时器的输入时钟（Hz）
 * @note APB 预分频为 1 时定时器时钟 = PCLK，否则 = 2*PCLK（F4/H7 同一规则）。
 *       用外设基址区间判 APB2：两族的 APB2 外设都从 APB2PERIPH_BASE 起 64KB，
 *       因此不需要按器件宏分支（F4 的 TIM1/8~11、H7 的 TIM1/8/15~17 都在其中）。
 */
static uint32_t BSPTim_GetTimerClock(TIM_TypeDef *tim)
{
    uintptr_t addr = (uintptr_t)tim;
    uintptr_t apb2_begin = (uintptr_t)APB2PERIPH_BASE;
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t pclk = (addr >= apb2_begin && addr < (apb2_begin + 0x10000UL)) ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();

    return (pclk == hclk) ? pclk : (pclk * 2u);
}

/**
 * @brief 写通道输出比较模式（CCMR1/CCMR2 的 OCxM）
 * @param ocm 3 位模式值，见上方 PWM_OCM_* 宏
 * @note 与上次写入相同时直接返回（占空比每拍都设时避免重复写 CCMR）。
 *       强制电平要真正输出到引脚，还要求 CCxE=1（HAL_TIM_PWM_Start 已置位），
 *       高级定时器另需 MOE=1（同由 PWM_Start 置位）。
 */
static void PWM_SetOcMode(PWMInstance *instance, uint32_t ocm)
{
    TIM_TypeDef *tim = instance->map.htim->Instance;
    volatile uint32_t *ccmr;
    uint32_t shift;
    uint32_t value;

    if (instance->oc_mode == (uint8_t)ocm)
        return;

    if (instance->map.channel == TIM_CHANNEL_1 || instance->map.channel == TIM_CHANNEL_2)
        ccmr = &tim->CCMR1;
    else if (instance->map.channel == TIM_CHANNEL_3 || instance->map.channel == TIM_CHANNEL_4)
        ccmr = &tim->CCMR2;
    else
        return; // 非法通道

    shift = (instance->map.channel == TIM_CHANNEL_1 || instance->map.channel == TIM_CHANNEL_3) ? 4u : 12u;
    value = (uint32_t)((*ccmr & (uint32_t)~(7UL << shift)) | (ocm << shift));
    *ccmr = value;
    instance->oc_mode = (uint8_t)ocm;
}

/**
 * @brief 按缓存的 dutyratio 把占空比落到硬件
 * @note 0/1 用强制电平保证严格 0%/100%；中间值回 PWM1 并写 CCR = duty*(ARR+1)。
 */
static void PWM_ApplyDuty(PWMInstance *instance)
{
    TIM_HandleTypeDef *htim = instance->map.htim;
    float duty = instance->dutyratio;
    uint32_t arr;
    uint32_t ccr;

    if (duty <= 0.0f)
    {
        PWM_SetOcMode(instance, PWM_OCM_FORCED_INACTIVE);
        return;
    }
    if (duty >= 1.0f)
    {
        PWM_SetOcMode(instance, PWM_OCM_FORCED_ACTIVE);
        return;
    }

    PWM_SetOcMode(instance, PWM_OCM_PWM1);

    arr = htim->Instance->ARR;
    ccr = (uint32_t)(duty * (float)(arr + 1u) + 0.5f);
    if (ccr > arr)
        ccr = arr;
    __HAL_TIM_SET_COMPARE(htim, instance->map.channel, ccr);
}

/*------------- PWM接口实现 --------------*/

/**
 * @brief 注册PWM实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 PWMConfig 负责）
 */
int8_t PWMRegister(PWMInstance *instance)
{

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_pwm_idx >= PWM_INSTANCE_NUM, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_pwm_idx; i++)
    {
        if (s_pwm_instance[i] == instance)
        {
            BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM instance already registered!");
            return -1;
        }
    }

    s_pwm_instance[s_pwm_idx++] = instance;

    BSPLOG(&g_tim_log, LOG_LEVEL_INFO, "PWM instance registered, idx=%d", s_pwm_idx - 1);
    return 0;
}

/**
 * @brief 配置PWM实例（填充硬件映射 + 启动PWM，可重复调用）
 * @note 要求先调用 PWMRegister 注册实例
 */
int8_t PWMConfig(PWMInstance *instance, const PWM_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->tim_e >= TIM_NUM_MAX, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM tim_e out of range!"));

    // 填充枚举和硬件映射
    instance->tim_e = config->tim_e;
    instance->map = tim_map[instance->tim_e];
    /* 板级映射允许留空（如 DM_MC02 的 TIM_HEATER = {NULL, 0}），此处必须挡掉，
     * 否则后面的 htim->Instance 会解引用空指针 */
    BSP_RETURN_IF_TRUE_LOG(instance->map.htim == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM tim_e has no hardware mapping!"));

    // 重复注册检查（同一 htim+channel 不能重复注册）
    for (uint8_t i = 0; i < s_pwm_idx; i++)
    {
        if (s_pwm_instance[i] == instance)
            continue;
        if (s_pwm_instance[i]->map.htim == instance->map.htim && s_pwm_instance[i]->map.channel == instance->map.channel)
        {
            BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM htim+channel already registered!");
            return -1;
        }
    }

    // 记录定时器时钟与 CubeMX 配置的当前频率（调频时不用再查 RCC）
    instance->tim_clock_hz = BSPTim_GetTimerClock(instance->map.htim->Instance);
    instance->frequency_hz =
        (float)instance->tim_clock_hz / ((float)(instance->map.htim->Instance->PSC + 1u) * (float)(instance->map.htim->Instance->ARR + 1u));
    /* CubeMX 生成的是 PWM1 且 HAL_TIM_PWM_ConfigChannel 已写过 CCMR，缓存之，
     * 避免首拍占空比再写一次同样的模式 */
    instance->oc_mode = PWM_OCM_PWM1;

    // 启动PWM
    HAL_TIM_PWM_Start(instance->map.htim, instance->map.channel);

    // 可选：按配置改频（0 = 沿用 CubeMX）
    if (config->frequency_hz > 0.0f)
        (void)PWMSetFrequency(instance, config->frequency_hz);

    // 设置初始占空比
    PWMSetDutyRatio(instance, instance->dutyratio);

    BSPLOG(&g_tim_log, LOG_LEVEL_INFO, "PWM config success, idx=%d", s_pwm_idx - 1);
    return 0;
}

void PWMSetDutyRatio(PWMInstance *instance, float dutyratio)
{
    if (instance == NULL || instance->map.htim == NULL)
    {
        BSPLOG(&g_tim_log, LOG_LEVEL_WARNING, "PWM instance is NULL or not configured!");
        return;
    }

    // 钳位到有效范围
    if (dutyratio < 0.0f)
        dutyratio = 0.0f;
    if (dutyratio > 1.0f)
        dutyratio = 1.0f;

    instance->dutyratio = dutyratio;
    PWM_ApplyDuty(instance);
}

int8_t PWMSetFrequency(PWMInstance *instance, float frequency_hz)
{
    if (instance == NULL || instance->map.htim == NULL || instance->tim_clock_hz == 0)
    {
        BSPLOG(&g_tim_log, LOG_LEVEL_WARNING, "PWM instance is NULL or not configured!");
        return -1;
    }
    if (!(frequency_hz > 0.0f))
    {
        BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM frequency must be > 0!");
        return -1;
    }

    /* 总分频 = 定时器时钟 / 目标频率；再拆成 PSC 与 ARR。
     * 优先让 ARR 占满 16 位（PSC 取满足 ARR<=65535 的最小值），占空比分辨率最高。
     * float 在 2.4e8 量级仍有 ~1e-7 相对精度，足够。 */
    {
        float div_f = (float)instance->tim_clock_hz / frequency_hz;
        uint32_t div;
        uint32_t psc_div;
        uint32_t arr;
        float real_hz;
        TIM_HandleTypeDef *htim = instance->map.htim;

        if (!(div_f >= 2.0f) || div_f > 4.0e9f)
        {
            BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "PWM frequency out of range!");
            return -1;
        }
        div = (uint32_t)(div_f + 0.5f);
        /* ceil(div / 65536)，写成 (div-1)/65536+1 避免 div+65535 溢出 */
        psc_div = (div - 1u) / 65536u + 1u;
        arr = div / psc_div - 1u;
        if (arr < 1u)
            arr = 1u;

        __HAL_TIM_SET_PRESCALER(htim, psc_div - 1u);
        __HAL_TIM_SET_AUTORELOAD(htim, arr);
        /* PSC 是预装载的，靠一次 UG 立即生效并让计数器从新周期起点开始。
         * 不用 HAL_TIM_GenerateEvent：它会把 htim->State 改成 READY，语义不对。 */
        htim->Instance->EGR = TIM_EGR_UG;
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);

        real_hz = (float)instance->tim_clock_hz / ((float)psc_div * (float)(arr + 1u));

        /* 频率是定时器级属性：同一 htim 的其它通道共用 PSC/ARR，占空比要按新 ARR 重算 */
        for (uint8_t i = 0; i < s_pwm_idx; i++)
        {
            if (s_pwm_instance[i] == NULL || s_pwm_instance[i]->map.htim != htim)
                continue;
            s_pwm_instance[i]->frequency_hz = real_hz;
            PWM_ApplyDuty(s_pwm_instance[i]);
        }
    }

    return 0;
}

/*============= 编码器 部分 =============*/

/*------------- 私有变量 --------------*/
static uint8_t s_encoder_idx = 0;
#if ENCODER_INSTANCE_NUM > 0
static EncoderInstance *s_encoder_instance[ENCODER_INSTANCE_NUM] = {NULL};
#else
static EncoderInstance **s_encoder_instance = NULL;
#endif

/*------------- 编码器接口实现 --------------*/

/**
 * @brief 注册编码器实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 EncoderConfig 负责）
 */
int8_t EncoderRegister(EncoderInstance *instance)
{

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder instance is NULL!"));
#if ENCODER_INSTANCE_NUM > 0
    BSP_RETURN_IF_TRUE_LOG(s_encoder_idx >= ENCODER_INSTANCE_NUM, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder exceeded max instance count!"));
#else
    /* 本板没有编码器槽位（ENCODER_INSTANCE_NUM == 0）：s_encoder_instance 是空指针，
       必须在这里拒绝，否则下面 s_encoder_instance[...] 会写空指针。
       不能写成 s_encoder_idx >= ENCODER_INSTANCE_NUM —— ENCODER_INSTANCE_NUM==0 时
       那是恒真比较（-Wtype-limits）。 */
    BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder exceeded max instance count!");
    return -1;
#endif

    // 防重复注册检查
    for (uint8_t i = 0; i < s_encoder_idx; i++)
    {
        if (s_encoder_instance[i] == instance)
        {
            BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder instance already registered!");
            return -1;
        }
    }

    s_encoder_instance[s_encoder_idx++] = instance;

    BSPLOG(&g_tim_log, LOG_LEVEL_INFO, "Encoder instance registered, idx=%d", s_encoder_idx - 1);
    return 0;
}

/**
 * @brief 配置编码器实例（填充硬件映射 + 启动编码器，可重复调用）
 * @note 要求先调用 EncoderRegister 注册实例
 */
int8_t EncoderConfig(EncoderInstance *instance, const Encoder_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->tim_e >= TIM_NUM_MAX, -1, BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder tim_e out of range!"));

    // 填充枚举和硬件映射
    instance->tim_e = config->tim_e;
    instance->map = tim_map[instance->tim_e];
    instance->arr = instance->map.htim->Instance->ARR + 1; // 溢出周期 = ARR + 1
    instance->last_time_us = DWT_GetTimeUs();

    // 重复注册检查（同一 htim 不能重复注册）
    for (uint8_t i = 0; i < s_encoder_idx; i++)
    {
        if (s_encoder_instance[i] == instance)
            continue;
        if (s_encoder_instance[i]->map.htim == instance->map.htim)
        {
            BSPLOG(&g_tim_log, LOG_LEVEL_ERROR, "Encoder htim already registered!");
            return -1;
        }
    }

    // 启动编码器
    HAL_TIM_Encoder_Start(instance->map.htim, TIM_CHANNEL_ALL);

    // 单独使能更新中断（用于溢出检测）
    __HAL_TIM_ENABLE_IT(instance->map.htim, TIM_IT_UPDATE);

    BSPLOG(&g_tim_log, LOG_LEVEL_INFO, "Encoder config success, idx=%d", s_encoder_idx - 1);
    return 0;
}

float EncoderGetSpeed(EncoderInstance *instance)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    uint64_t current_time = DWT_GetTimeUs();
    int32_t current_count = (int32_t)__HAL_TIM_GET_COUNTER(instance->map.htim);

    // 计算扩展后的总计数：溢出次数 * 溢出周期 + 当前计数值
    instance->total_count = (int64_t)instance->overflow_count * instance->arr + current_count;

    float dt = (float)(current_time - instance->last_time_us) * 1e-6f;
    if (dt > 0.0001f)
    {
        // 使用扩展后的总计数计算速度，正确处理溢出
        instance->speed = (float)(instance->total_count - instance->last_total_count) / dt;
    }

    instance->last_total_count = instance->total_count;
    instance->last_time_us = current_time;

    return instance->speed;
}

void EncoderClearCount(EncoderInstance *instance)
{
    if (instance == NULL)
    {
        BSPLOG(&g_tim_log, LOG_LEVEL_WARNING, "Encoder instance is NULL!");
        return;
    }

    __HAL_TIM_SET_COUNTER(instance->map.htim, 0);
    instance->overflow_count = 0;
    instance->total_count = 0;
    instance->last_total_count = 0;
}

/*============= 溢出回调分发 =============*/

#endif

#if (ENCODER_INSTANCE_NUM > 0)

void BSPTim_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // 匹配编码器实例
    for (uint8_t i = 0; i < s_encoder_idx; i++)
    {
        if (s_encoder_instance[i]->map.htim == htim)
        {
            // 根据计数方向更新溢出次数
            if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
            {
                s_encoder_instance[i]->overflow_count--;
            }
            else
            {
                s_encoder_instance[i]->overflow_count++;
            }
            return;
        }
    }
}

#else

// 没有编码器实例时提供空实现
void BSPTim_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    (void)htim;
}

#endif /* (ENCODER_INSTANCE_NUM > 0) */

#endif /* BSP_TIM_USED */
