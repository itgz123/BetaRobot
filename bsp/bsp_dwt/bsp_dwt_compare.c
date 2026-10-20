/**
 * @file bsp_dwt_compare.c
 * @brief DWT 4 通道比较器实现
 */

#include "bsp_dwt_compare.h"
#include "main.h"
#include "app_cfg.h"

#include <stddef.h>

#ifdef BSP_DWT_COMPARE_USED

/* CoreSight 解锁键：Cortex-M7 的 DWT 可能处于锁定状态 */
#define DWT_LAR_UNLOCK_KEY 0xC5ACCE55u

/* ARMv7-M FUNCTION[3:0] 编码（DATAVMATCH=0、CYCMATCH=0） */
#define DWT_FUNCTION_PC_MATCH  0x4u
#define DWT_FUNCTION_DATA_R    0x5u
#define DWT_FUNCTION_DATA_W    0x6u
#define DWT_FUNCTION_DATA_RW   0x7u

/*------------- 私有变量 --------------*/

/* 通道当前配置的模式；DISABLED 表示尚未配置或已关闭 */
static DWT_CompareMode_e s_mode[DWT_COMPARE_CHANNEL_NUM];

/*------------- 私有函数 --------------*/

static uint8_t DWT_CompareChannelValid(DWT_CompareChannel_e channel)
{
    return ((uint32_t)channel < DWT_COMPARE_CHANNEL_NUM) ? 1u : 0u;
}

/**
 * @brief 取通道寄存器（index：0=COMP，1=MASK，2=FUNCTION）
 * @note 寄存器块内 COMP/MASK/FUNCTION 依次 +0x04，相邻通道 +0x10。
 *       **只能用 COMP0 作基址**：HAL 把 COMP1/COMP2/COMP3 定义成了
 *       模拟比较器外设的指针宏，直接写 DWT->COMP1 会被宏替换掉。
 */
static volatile uint32_t *DWT_CompareReg(DWT_CompareChannel_e channel, uint32_t index)
{
    return (volatile uint32_t *)((uintptr_t)&DWT->COMP0
                                 + ((uintptr_t)channel * 0x10u)
                                 + ((uintptr_t)index * 0x04u));
}

static uint32_t DWT_CompareModeToFunction(DWT_CompareMode_e mode)
{
    switch (mode)
    {
    case DWT_COMPARE_MODE_PC:
        return DWT_FUNCTION_PC_MATCH;
    case DWT_COMPARE_MODE_DATA_R:
        return DWT_FUNCTION_DATA_R;
    case DWT_COMPARE_MODE_DATA_W:
        return DWT_FUNCTION_DATA_W;
    case DWT_COMPARE_MODE_DATA_RW:
        return DWT_FUNCTION_DATA_RW;
    case DWT_COMPARE_MODE_CYCCNT:
        /* 通道 0 专用：比较源改为 CYCCNT，动作仍取 watchpoint 编码 */
        return DWT_FUNCTION_PC_MATCH | (uint32_t)DWT_FUNCTION_CYCMATCH_Msk;
    case DWT_COMPARE_MODE_DISABLED:
    default:
        return 0u;
    }
}

/*------------- 外部接口实现 --------------*/

uint8_t DWT_CompareInit(void)
{
    /* 依赖 DWT_Init() 已使能 DEMCR.TRCENA */
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0u)
    {
        return 0u;
    }

#if (__CORTEX_M == 7U)
    DWT->LAR = DWT_LAR_UNLOCK_KEY;
#endif

    /* NUMCOMP = 实现的比较器数 - 1；本模块按 4 通道暴露 */
    uint32_t numcomp = (DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos;
    if ((numcomp + 1u) < DWT_COMPARE_CHANNEL_NUM)
    {
        return 0u;
    }

    for (uint32_t i = 0u; i < DWT_COMPARE_CHANNEL_NUM; i++)
    {
        (void)DWT_CompareDisable((DWT_CompareChannel_e)i);
    }

    return 1u;
}

uint8_t DWT_CompareConfigure(DWT_CompareChannel_e channel,
                             const DWT_CompareConfig_s *config)
{
    if ((DWT_CompareChannelValid(channel) == 0u) || (config == NULL))
    {
        return 0u;
    }

    if ((uint32_t)config->mode > (uint32_t)DWT_COMPARE_MODE_CYCCNT)
    {
        return 0u;
    }

    /* CYCMATCH 是 FUNCTION0 的字段，仅通道 0 支持 */
    if ((config->mode == DWT_COMPARE_MODE_CYCCNT) && (channel != DWT_COMPARE_CH0))
    {
        return 0u;
    }

    /* 改配置前先关闭通道，避免比较过程中写 COMP/MASK */
    (void)DWT_CompareDisable(channel);

    *DWT_CompareReg(channel, 0u) = config->value;
    *DWT_CompareReg(channel, 1u) = config->mask & (uint32_t)DWT_MASK_MASK_Msk;
    s_mode[channel] = config->mode;

    return 1u;
}

uint8_t DWT_CompareEnable(DWT_CompareChannel_e channel)
{
    if (DWT_CompareChannelValid(channel) == 0u)
    {
        return 0u;
    }

    if (s_mode[channel] == DWT_COMPARE_MODE_DISABLED)
    {
        return 0u;
    }

    /* 先写 MATCHED=1 关闭通道并清掉历史匹配（W1C） */
    *DWT_CompareReg(channel, 2u) = (uint32_t)DWT_FUNCTION_MATCHED_Msk;
    *DWT_CompareReg(channel, 2u) = DWT_CompareModeToFunction(s_mode[channel]);

    return 1u;
}

uint8_t DWT_CompareDisable(DWT_CompareChannel_e channel)
{
    if (DWT_CompareChannelValid(channel) == 0u)
    {
        return 0u;
    }

    s_mode[channel] = DWT_COMPARE_MODE_DISABLED;

    /* FUNCTION[3:0]=0 关闭；同时 W1C 清 MATCHED */
    *DWT_CompareReg(channel, 2u) = (uint32_t)DWT_FUNCTION_MATCHED_Msk;

    return 1u;
}

uint8_t DWT_CompareGetMatched(DWT_CompareChannel_e channel, uint8_t *matched)
{
    if ((DWT_CompareChannelValid(channel) == 0u) || (matched == NULL))
    {
        return 0u;
    }

    uint32_t reg = *DWT_CompareReg(channel, 2u);
    *matched = ((reg & (uint32_t)DWT_FUNCTION_MATCHED_Msk) != 0u) ? 1u : 0u;

    /* 读取即消费一次匹配；保留原有 FUNCTION 配置，写回 MATCHED 清除 */
    if (*matched != 0u)
    {
        *DWT_CompareReg(channel, 2u) = reg | (uint32_t)DWT_FUNCTION_MATCHED_Msk;
    }

    return 1u;
}

#endif /* BSP_DWT_COMPARE_USED */
