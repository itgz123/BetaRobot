/**
 * @file bsp_dwt.c
 * @brief DWT高精度定时器封装实现
 * @author Wang Hongxi
 * @author modified by Neo with annotation
 */

#include "bsp_dwt.h"
// 查看便签待办
/*------------- 私有变量 --------------*/
static DWT_Time_t s_sys_time;
static uint32_t s_cpu_freq_hz;
static uint32_t s_cpu_freq_hz_ms;
static uint32_t s_cpu_freq_hz_us;
static uint32_t s_cyccnt_round_count;
static uint32_t s_cyccnt_last;
static uint64_t s_cyccnt64;

/*------------- 私有函数 --------------*/

/**
 * @brief 检查DWT CYCCNT寄存器是否溢出，并更新计数
 * @attention 此函数假设两次调用之间的时间间隔不超过一次溢出
 */
static void DWT_CNT_Update(void)
{
    static volatile uint8_t s_bit_locker = 0;
    if (!s_bit_locker)
    {
        s_bit_locker = 1;
        volatile uint32_t cnt_now = DWT->CYCCNT;
        if (cnt_now < s_cyccnt_last)
            s_cyccnt_round_count++;

        s_cyccnt_last = DWT->CYCCNT;
        s_bit_locker = 0;
    }
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

    /* 使用 bsp_cfg.h 中的 CPU_FREQ_MHZ 配置 */
    s_cpu_freq_hz = CPU_FREQ_MHZ * 1000000;
    s_cpu_freq_hz_ms = s_cpu_freq_hz / 1000;
    s_cpu_freq_hz_us = s_cpu_freq_hz / 1000000;
    s_cyccnt_round_count = 0;

    DWT_CNT_Update();
}

float DWT_GetDeltaT(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)s_cpu_freq_hz);
    *cnt_last = cnt_now;

    DWT_CNT_Update();

    return dt;
}

double DWT_GetDeltaT64(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)s_cpu_freq_hz);
    *cnt_last = cnt_now;

    DWT_CNT_Update();

    return dt;
}

void DWT_SysTimeUpdate(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    static uint64_t s_cnt_temp1, s_cnt_temp2, s_cnt_temp3;

    DWT_CNT_Update();

    s_cyccnt64 = (uint64_t)s_cyccnt_round_count * (uint64_t)UINT32_MAX + (uint64_t)cnt_now;
    s_cnt_temp1 = s_cyccnt64 / s_cpu_freq_hz;
    s_cnt_temp2 = s_cyccnt64 - s_cnt_temp1 * s_cpu_freq_hz;
    s_sys_time.s = s_cnt_temp1;
    s_sys_time.ms = s_cnt_temp2 / s_cpu_freq_hz_ms;
    s_cnt_temp3 = s_cnt_temp2 - s_sys_time.ms * s_cpu_freq_hz_ms;
    s_sys_time.us = s_cnt_temp3 / s_cpu_freq_hz_us;
}

float DWT_GetTimeline_s(void)
{
    DWT_SysTimeUpdate();

    float timeline = s_sys_time.s + s_sys_time.ms * 0.001f + s_sys_time.us * 0.000001f;

    return timeline;
}

float DWT_GetTimeline_ms(void)
{
    DWT_SysTimeUpdate();

    float timeline = s_sys_time.s * 1000 + s_sys_time.ms + s_sys_time.us * 0.001f;

    return timeline;
}

uint64_t DWT_GetTimeline_us(void)
{
    DWT_SysTimeUpdate();

    uint64_t timeline = s_sys_time.s * 1000000 + s_sys_time.ms * 1000 + s_sys_time.us;

    return timeline;
}

void DWT_Delay(float delay)
{
    uint32_t tickstart = DWT->CYCCNT;

    while ((DWT->CYCCNT - tickstart) < delay * (float)s_cpu_freq_hz)
        ;
}