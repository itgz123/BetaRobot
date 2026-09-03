/**
 * @file bsp_dwt.c
 * @brief DWT高精度定时器封装实现
 * @author Wang Hongxi
 * @author modified by Neo with annotation
 */

#include "bsp_dwt.h"
#include "app_cfg.h"

#ifdef BSP_DWT_USED

/*------------- 私有变量 --------------*/
static uint32_t s_cyccnt_round_count;
static uint32_t s_cyccnt_last;
static uint32_t s_cycles_per_us; /* SystemCoreClock / 1e6：每微秒周期数（DWT_Init 计算） */
static uint64_t s_us_magic;      /* ceil(2^64 / s_cycles_per_us)：us 换算魔数（DWT_Init 计算） */

/*------------- 私有函数 --------------*/

/* 64 位魔数除法（bsp 层自洽实现，不复用 lib_math_int64.h 以解除对 lib 的依赖）。
 * 背景：32 位 ARM（Cortex-M4/M7）无 128 位整数、无硬件 64 位除法，编译器对
 * `uint64_t / 变量` 一律生成软件除法库 __aeabi_uldivmod（逐位循环，慢）。
 * 故把 cyccnt64 / s_cycles_per_us 换成"定点小数字因子 + 高半乘法"的等价实现：
 *   除以 d 等价于"乘以因子 magic/2^64 再取高 64 位"，magic = ceil(2^64/d)
 *   （DWT_Magic 于 DWT_Init 算一次），商 = DWT_U64Div(v, d, magic)，
 *   全程 umull + 移位 + 减法，零除法指令。 */

/**
 * @brief 64 位高半乘法：DWT_UMulH(a,b) = (a*b)>>64
 * @note 按 2^32 基拆成 4 次 32×32→64 umull + 进位累加：
 *       a*b = m3*2^64 + (m1+m2)*2^32 + t
 *       高 64 位 = m3 + floor((m1+m2 + (t>>32))/2^32)。
 *       无符号加法溢出即模 2^64（C 标准定义），逐项模加结果正确。
 */
static inline uint64_t DWT_UMulH(uint64_t a, uint64_t b)
{
    uint32_t al = (uint32_t)a;
    uint32_t ah = (uint32_t)(a >> 32);
    uint32_t bl = (uint32_t)b;
    uint32_t bh = (uint32_t)(b >> 32);

    uint64_t t = (uint64_t)al * bl;  /* 低×低 */
    uint64_t m1 = (uint64_t)ah * bl; /* 高×低 */
    uint64_t m2 = (uint64_t)al * bh; /* 低×高 */
    uint64_t m3 = (uint64_t)ah * bh; /* 高×高 */

    uint64_t mid = (t >> 32) + (uint32_t)m1 + (uint32_t)m2;
    return m3 + (m1 >> 32) + (m2 >> 32) + (mid >> 32);
}

/**
 * @brief 预计算"除以 32 位除数 d"的魔数 = ceil(2^64/d)
 * @param d 除数（32 位）；d<=1 时返回 0，调用方须特判 d<=1 走直接返回
 * @note 仅初始化阶段调用一次，避免每次除法都算。
 *       ceil(2^64/d) = UINT64_MAX/d + 1：d 不整除 2^64 时 UINT64_MAX/d =
 *       floor(2^64/d)，+1 = ceil ✓；d 整除 2^64（d 为 2 的幂）时
 *       UINT64_MAX/d = 2^64/d - 1，+1 = 2^64/d = ceil ✓。
 *       本函数含一次 64 位除法（UINT64_MAX/d），仅此一处、仅执行一次。
 */
static inline uint64_t DWT_Magic(uint32_t d)
{
    if (d <= 1u)
    {
        return 0u;
    }
    return UINT64_MAX / (uint64_t)d + 1u;
}

/**
 * @brief 64 位被除数除以 32 位除数 d（返回商），魔数法，零除法指令
 * @param v     被除数（64 位）
 * @param d     除数（32 位，须与 magic 对应，d>=2）
 * @param magic 除数 d 的魔数（DWT_Magic(d) 预计算）
 * @note 定点小数字因子思路：因子 = magic/2^64，商 = floor(v×因子) =
 *       umulh(v, magic)。对 d 不整除 2^64，q0 ∈ {⌊v/d⌋, ⌊v/d⌋+1}
 *       （高估 ≤ 1：q0 = ⌊v/d + (r/d)·(d-rem)/d⌋ < q + 2），一次修正：
 *       q0·d 溢出（umulh(q0,d)≠0）或大于 v 则回退 1。
 *       全程 umull + 移位 + 减法，无除法指令、无 __aeabi_uldivmod。
 */
static inline uint64_t DWT_U64Div(uint64_t v, uint32_t d, uint64_t magic)
{
    uint64_t q = DWT_UMulH(v, magic);
    uint64_t prod = q * (uint64_t)d;
    if (DWT_UMulH(q, (uint64_t)d) != 0u || prod > v)
    {
        q--;
    }
    return q;
}

/**
 * @brief 检查DWT CYCCNT寄存器是否溢出，并更新计数
 * @attention 此函数假设两次调用之间的时间间隔不超过一次溢出
 */
static void DWT_CNT_Update(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    volatile uint32_t cnt_now = DWT->CYCCNT;
    if (cnt_now < s_cyccnt_last)
        s_cyccnt_round_count++;

    s_cyccnt_last = cnt_now;

    if (!primask)
        __enable_irq();
}

/*------------- 外部接口实现 --------------*/

void DWT_Init(void)
{
    /* 使能DWT外设 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* DWT CYCCNT寄存器计数清0 */
    DWT->CYCCNT = (uint32_t)0u;

    /* 使能Cortex-M DWT CYCCNT寄存器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 预计算 us 换算因子（模块内魔数法，见上方 DWT_Magic/DWT_U64Div）：
     * 直接 64 位除法 cyccnt64 / cycles_per_us 会被 gcc 编译成软件除法库
     * __aeabi_uldivmod（慢）；改为 DWT_Init 算一次魔数 magic = ceil(2^64/cycles_per_us)，
     * 热路径用定点小数字因子：us = cyccnt64 × (magic/2^64) = DWT_U64Div(...)，
     * 全 umull+移位+减法，零除法指令。 */
    s_cycles_per_us = SystemCoreClock / 1000000u;
    s_us_magic = DWT_Magic(s_cycles_per_us);

    s_cyccnt_round_count = 0;

    DWT_CNT_Update();
}

uint64_t DWT_GetTimeUs(void)
{
    DWT_CNT_Update();

    uint64_t cyccnt64 = ((uint64_t)s_cyccnt_round_count << 32) | s_cyccnt_last;
    if (s_cycles_per_us <= 1u)
    {
        return cyccnt64; /* 时钟 ≤1MHz（或未 DWT_Init）：周期数即 us */
    }
    return DWT_U64Div(cyccnt64, s_cycles_per_us, s_us_magic);
}

void DWT_Delay(float delay)
{
    uint32_t tickstart = DWT->CYCCNT;

    while ((DWT->CYCCNT - tickstart) < delay * (float)SystemCoreClock)
        ;
}

#endif /* BSP_DWT_USED */
