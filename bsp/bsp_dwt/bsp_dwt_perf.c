/**
 * @file bsp_dwt_perf.c
 * @brief DWT 性能分析计数器实现
 */

#include "bsp_dwt_perf.h"
#include "main.h"
#include "app_cfg.h"

#include <stddef.h>

#ifdef BSP_DWT_PERF_USED

/* CoreSight 解锁键：Cortex-M7 的 DWT 可能处于锁定状态 */
#define DWT_LAR_UNLOCK_KEY 0xC5ACCE55u

uint8_t DWT_PerfInit(void)
{
    /* 依赖 DWT_Init() 已使能 DEMCR.TRCENA */
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u)
    {
        return 0u;
    }

#if (__CORTEX_M == 7U)
    DWT->LAR = DWT_LAR_UNLOCK_KEY;
#endif

    /* 本内核未实现性能计数器 */
    if ((DWT->CTRL & DWT_CTRL_NOPRFCNT_Msk) != 0u)
    {
        return 0u;
    }

    DWT->CPICNT = 0u;
    DWT->EXCCNT = 0u;
    DWT->SLEEPCNT = 0u;
    DWT->LSUCNT = 0u;
    DWT->FOLDCNT = 0u;

    DWT->CTRL |= DWT_CTRL_CPIEVTENA_Msk | DWT_CTRL_EXCEVTENA_Msk | DWT_CTRL_SLEEPEVTENA_Msk | DWT_CTRL_LSUEVTENA_Msk | DWT_CTRL_FOLDEVTENA_Msk;

    return 1u;
}

uint8_t DWT_PerfRead(DWT_PerfCounter_e counter, uint32_t *value)
{
    if ((value == NULL) || ((uint32_t)counter >= (uint32_t)DWT_PERF_COUNTER_NUM))
    {
        return 0u;
    }

    switch (counter)
    {
    case DWT_PERF_CPICNT:
        *value = DWT->CPICNT;
        break;
    case DWT_PERF_EXCCNT:
        *value = DWT->EXCCNT;
        break;
    case DWT_PERF_SLEEPCNT:
        *value = DWT->SLEEPCNT;
        break;
    case DWT_PERF_LSUCNT:
        *value = DWT->LSUCNT;
        break;
    case DWT_PERF_FOLDCNT:
        *value = DWT->FOLDCNT;
        break;
    default:
        return 0u;
    }

    return 1u;
}

uint8_t DWT_PerfGetSnapshot(DWT_PerfSnapshot_s *snapshot)
{
    if (snapshot == NULL)
    {
        return 0u;
    }

    snapshot->cpicnt = DWT->CPICNT;
    snapshot->exccnt = DWT->EXCCNT;
    snapshot->sleepcnt = DWT->SLEEPCNT;
    snapshot->lsucnt = DWT->LSUCNT;
    snapshot->foldcnt = DWT->FOLDCNT;

    return 1u;
}

void DWT_PerfReset(void)
{
    DWT->CPICNT = 0u;
    DWT->EXCCNT = 0u;
    DWT->SLEEPCNT = 0u;
    DWT->LSUCNT = 0u;
    DWT->FOLDCNT = 0u;
}

#endif /* BSP_DWT_PERF_USED */
