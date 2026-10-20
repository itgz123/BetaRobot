/**
 * @file bsp_dwt_pcsample.c
 * @brief DWT 统计式 PC 采样骨架实现（预留，未实现）
 */

#include "bsp_dwt_pcsample.h"
#include "app_cfg.h"

#ifdef BSP_DWT_PCSAMPLE_USED

uint8_t DWT_PcSampleInit(void)
{
    return 0u;
}

uint8_t DWT_PcSampleStart(void)
{
    return 0u;
}

uint8_t DWT_PcSampleStop(void)
{
    return 0u;
}

uint8_t DWT_PcSampleGetReport(DWT_PcSampleReport_s *report)
{
    (void)report;
    return 0u;
}

void DWT_PcSampleReset(void)
{
}

#endif /* BSP_DWT_PCSAMPLE_USED */
