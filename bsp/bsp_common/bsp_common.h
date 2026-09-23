/**
 * @file bsp_common.h
 * @brief bsp 层公共定义：统一状态码、传输模式、超时计时器
 *
 * @note 纯头文件（无 .c），由 uart / iic / spi 等协议共用。各协议只保留
 *       自己的实例结构与功能接口，跨协议相同的部分放这里，避免每个协议
 *       各造一套枚举和返回码。
 *
 * @note 命名约定（跨模块统一，见 ignore/todo.md）：
 *       - 实例结构体名 XxxInstance，其父指针成员统一命名 parent、类型 void *
 *       - 回调统一为 void (*xxx_callback)(struct XxxInstance *)：
 *         rx_callback / tx_callback / err_callback
 *       - 枚举后缀 _e、结构体后缀 _s
 */

#ifndef __BSP_COMMON_H
#define __BSP_COMMON_H

#include "bsp_dwt.h"
#include <stdint.h>

/*------------- 统一状态码 --------------*/

/**
 * @brief BSP 层统一返回状态码
 * @note 底层类型为 int8_t，错误码取负值：既支持 `!= BSP_OK` 判断，也与各协议
 *       旧接口的 int8_t 0/-1 惯例 ABI 兼容（旧代码 `ret != 0` 仍然成立）。
 */
typedef enum : int8_t
{
    BSP_OK = 0,         /* 成功（IT/DMA：已受理并启动，完成与否看回调） */
    BSP_BUSY = -1,      /* 外设/资源忙（timeout_ms == 0 时不等待，忙即返回它） */
    BSP_TIMEOUT = -2,   /* 等就绪耗尽 timeout_ms（实现已顺手复位卡死状态，可立即重试） */
    BSP_PARAM_ERR = -3, /* 参数非法：空指针 / 长度越界 / mode 非法 / 该口无对应 DMA */
    BSP_HW_ERR = -4,    /* HAL 启动失败或硬件错误（已按 err_callback 契约收尾） */
} BSP_Status_e;

/*------------- 传输模式 --------------*/

/**
 * @brief 传输模式（发送/接收每次调用传参，不再在 Config 期固定）
 * @note 三种模式的含义对收发一致；DMA 模式的缓冲须位于 DMA 可访问 RAM
 *       （见 bsp_map.h 的 DMA_RAM）且生存期覆盖整个传输。
 */
typedef enum : uint8_t
{
    BSP_BLOCK_MODE = 0, /* 阻塞：函数返回时传输已完成 */
    BSP_IT_MODE = 1,    /* 中断：启动即返回，完成走回调 */
    BSP_DMA_MODE = 2,   /* DMA：启动即返回，完成走回调 */
} BSP_Transfer_Mode_e;

/*------------- 超时计时器 --------------*/

/**
 * @brief 超时计时器（起点 + 上限）
 * @note 只封装计时与换算——这部分各协议完全相同且不依赖 HAL；"等就绪循环"
 *       不在此封装，因为各外设的就绪条件与超时后的纠错动作完全不同。
 *       计时用 DWT 64 位微秒（不受中断影响、无回绕），不用 HAL_GetTick。
 */
typedef struct
{
    uint64_t start_us;   /* 计时起点（DWT 微秒） */
    uint64_t timeout_us; /* 超时上限（微秒），0 = 不等待 */
} BSP_Timeout_s;

/**
 * @brief 启动计时
 * @param t 计时器
 * @param timeout_ms 超时上限（毫秒），0 表示不等待（首次判断即过期）
 */
static inline void BSP_TimeoutStart(BSP_Timeout_s *t, uint32_t timeout_ms)
{
    t->start_us = DWT_GetTimeUs();
    t->timeout_us = (uint64_t)timeout_ms * 1000u;
}

/**
 * @brief 判断是否已超时
 * @param t 计时器
 * @retval 1 已超时；0 未超时
 * @note timeout_ms == 0（未开始等待）时恒返回 1，对应"不等待，只判一次"的语义。
 */
static inline uint8_t BSP_TimeoutExpired(const BSP_Timeout_s *t)
{
    return ((DWT_GetTimeUs() - t->start_us) >= t->timeout_us) ? 1u : 0u;
}

#endif /* __BSP_COMMON_H */
