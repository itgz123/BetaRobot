/**
 * @file bsp_dwt_pcsample.h
 * @brief DWT 统计式 PC 采样骨架接口（预留，未实现）
 * @warning 本模块仅为预留占位。采样触发源（比较器 / DebugMonitor）、采样周期、
 *          归并策略均未确定；后续实现不得据本骨架推断寄存器行为。
 * @note 本模块不得调用 DWT_Init()（会清零 CYCCNT，破坏全局时间轴）。
 */

#ifndef __BSP_DWT_PCSAMPLE_H
#define __BSP_DWT_PCSAMPLE_H

#include "stdint.h"

/*------------- 宏定义 --------------*/

/* 固定上限的采样表项数，避免动态内存 */
#ifndef DWT_PCSAMPLE_ENTRY_NUM
#define DWT_PCSAMPLE_ENTRY_NUM 16u
#endif

/*------------- 类型定义 --------------*/

typedef struct
{
    uint32_t pc;
    uint32_t hits;
} DWT_PcSampleEntry_s;

typedef struct
{
    uint32_t total_hits;
    uint32_t entry_num;
    DWT_PcSampleEntry_s entries[DWT_PCSAMPLE_ENTRY_NUM];
} DWT_PcSampleReport_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化 PC 采样模块
 * @return 1=成功，0=失败或未实现
 */
uint8_t DWT_PcSampleInit(void);

/**
 * @brief 启动采样
 * @return 1=成功，0=失败或未实现
 */
uint8_t DWT_PcSampleStart(void);

/**
 * @brief 停止采样
 * @return 1=成功，0=失败或未实现
 */
uint8_t DWT_PcSampleStop(void);

/**
 * @brief 读取采样报告
 * @param report 输出：采样结果（仅返回 1 时有效）
 * @return 1=成功，0=失败或未实现
 */
uint8_t DWT_PcSampleGetReport(DWT_PcSampleReport_s *report);

/**
 * @brief 清零采样数据
 */
void DWT_PcSampleReset(void);

#endif /* __BSP_DWT_PCSAMPLE_H */
