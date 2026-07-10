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

    s_cyccnt_round_count = 0;

    DWT_CNT_Update();
}

uint64_t DWT_GetTimeUs(void)
{
    DWT_CNT_Update();

    uint64_t cyccnt64 = ((uint64_t)s_cyccnt_round_count << 32) | s_cyccnt_last;
    return cyccnt64 / (SystemCoreClock / 1000000);
}

void DWT_Delay(float delay)
{
    uint32_t tickstart = DWT->CYCCNT;

    while ((DWT->CYCCNT - tickstart) < delay * (float)SystemCoreClock)
        ;
}

#endif /* BSP_DWT_USED */
