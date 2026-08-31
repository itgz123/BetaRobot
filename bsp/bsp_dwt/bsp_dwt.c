/**
 * @file bsp_dwt.c
 * @brief DWT高精度定时器封装实现
 * @author Wang Hongxi
 * @author modified by Neo with annotation
 */

#include "bsp_dwt.h"
#include "bsp_math_int64.h"
#include "app_cfg.h"

#ifdef BSP_DWT_USED

/*------------- 私有变量 --------------*/
static uint32_t s_cyccnt_round_count;
static uint32_t s_cyccnt_last;
static uint32_t s_cycles_per_us; /* SystemCoreClock / 1e6：每微秒周期数（DWT_Init 计算） */
static uint64_t s_us_magic;      /* ceil(2^64 / s_cycles_per_us)：us 换算魔数（DWT_Init 计算） */

/*------------- 私有函数 --------------*/

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

    /* 预计算 us 换算因子（魔数法，见 bsp_math_int64.h）：
     * 旧实现每次 DWT_GetTimeUs 都做 64 位除法 cyccnt64 / cycles_per_us，
     * gcc 对"uint64_t / 运行时变量"生成软件除法库 __aeabi_uldivmod（慢）；
     * 改为 DWT_Init 算一次魔数 magic = ceil(2^64/cycles_per_us)，热路径用
     * 定点小数字因子：us = cyccnt64 × (magic/2^64) = BSP_Math_U64Div(...)，
     * 全 umull+移位+减法，零除法指令。 */
    s_cycles_per_us = SystemCoreClock / 1000000u;
    s_us_magic = BSP_Math_Magic(s_cycles_per_us);

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
    return BSP_Math_U64Div(cyccnt64, s_cycles_per_us, s_us_magic);
}

void DWT_Delay(float delay)
{
    uint32_t tickstart = DWT->CYCCNT;

    while ((DWT->CYCCNT - tickstart) < delay * (float)SystemCoreClock)
        ;
}

#endif /* BSP_DWT_USED */
