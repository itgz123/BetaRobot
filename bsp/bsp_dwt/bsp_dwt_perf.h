/**
 * @file bsp_dwt_perf.h
 * @brief DWT 性能分析计数器接口
 * @note 覆盖 CPICNT / EXCCNT / SLEEPCNT / LSUCNT / FOLDCNT 五类事件计数。
 * @note 计数器为 8 位，达到 255 后回绕到 0；返回的是上次清零以来的原始计数值，
 *       调用方需要自行处理回绕与窗口差分。
 * @note 依赖 DWT_Init() 已完成（使能 DEMCR.TRCENA）；本模块不得调用 DWT_Init()
 *       （会清零 CYCCNT，破坏全局时间轴）。
 */

#ifndef __BSP_DWT_PERF_H
#define __BSP_DWT_PERF_H

#include "stdint.h"

/*------------- 类型定义 --------------*/

typedef enum : uint8_t
{
    DWT_PERF_CPICNT = 0, /* 每指令额外周期 */
    DWT_PERF_EXCCNT,     /* 异常处理额外周期 */
    DWT_PERF_SLEEPCNT,   /* 睡眠周期 */
    DWT_PERF_LSUCNT,     /* 加载/存储额外周期 */
    DWT_PERF_FOLDCNT,    /* 折叠指令周期 */
    DWT_PERF_COUNTER_NUM
} DWT_PerfCounter_e;

typedef struct
{
    uint32_t cpicnt;
    uint32_t exccnt;
    uint32_t sleepcnt;
    uint32_t lsucnt;
    uint32_t foldcnt;
} DWT_PerfSnapshot_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化性能计数器：清零并使能五类事件计数
 * @return 1=成功，0=未初始化 DWT 或内核不支持性能计数器
 */
uint8_t DWT_PerfInit(void);

/**
 * @brief 读取单个性能计数器
 * @param counter 计数器编号
 * @param value   输出：原始计数值 0~255（仅返回 1 时有效）
 * @return 1=成功，0=参数非法
 */
uint8_t DWT_PerfRead(DWT_PerfCounter_e counter, uint32_t *value);

/**
 * @brief 读取全部性能计数器的快照
 * @param snapshot 输出：快照（仅返回 1 时有效）
 * @return 1=成功，0=参数非法
 */
uint8_t DWT_PerfGetSnapshot(DWT_PerfSnapshot_s *snapshot);

/**
 * @brief 清零性能计数器
 * @note 不影响 CYCCNT 时间轴，也不改变各计数器的使能状态
 */
void DWT_PerfReset(void);

#endif /* __BSP_DWT_PERF_H */
