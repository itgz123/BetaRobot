/**
 * @file bsp_uart_log.h
 * @brief UART 日志输出模块
 * @note 使用环形缓冲区 + DMA 发送，支持不等长日志
 *       参考 drv_vofa_lite 设计模式
 *       硬件配置（波特率/中断/DMA）由 CubeMX 负责
 */

#ifndef __BSP_UART_LOG_H
#define __BSP_UART_LOG_H

#include "bsp.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef HAL_UART_MODULE_ENABLED

/*============= 配置宏（可被 app_cfg.h 覆盖）==============*/

/**
 * @brief 日志输出 UART 选择
 * @note 不同开发板使用不同的调试串口
 */
#ifndef UART_LOG_UART
#if DEVELOPMENT_BOARD == DM_MC02
#define UART_LOG_UART UART_10
#elif DEVELOPMENT_BOARD == DJI_C
#error "Please define UART_LOG_UART for this board"
#elif DEVELOPMENT_BOARD == DJI_A
#error "Please define UART_LOG_UART for this board"
#elif DEVELOPMENT_BOARD == STM32F407VET6
#error "Please define UART_LOG_UART for this board"
#else
#error "Please define UART_LOG_UART for this board"
#endif
#endif // !UART_LOG_UART

/**
 * @brief 发送环形缓冲区大小
 */
#ifndef UART_LOG_TX_RING_SIZE
#define UART_LOG_TX_RING_SIZE 2048
#endif

/**
 * @brief 是否启用时间戳
 */
#ifndef UART_LOG_ENABLE_TIMESTAMP
#define UART_LOG_ENABLE_TIMESTAMP 1
#endif

/**
 * @brief 是否启用颜色控制
 */
#ifndef UART_LOG_ENABLE_COLOR
#define UART_LOG_ENABLE_COLOR 1
#endif

/*============= ANSI 颜色宏 =============*/

#if UART_LOG_ENABLE_COLOR

#define LOG_COLOR_RESET "\033[0m"
#define LOG_COLOR_GREEN "\033[32m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_RED "\033[31m"
#define LOG_COLOR_CYAN "\033[36m"

#else

#define LOG_COLOR_RESET ""
#define LOG_COLOR_GREEN ""
#define LOG_COLOR_YELLOW ""
#define LOG_COLOR_RED ""
#define LOG_COLOR_CYAN ""

#endif // UART_LOG_ENABLE_COLOR

/*============= 日志级别枚举 =============*/

typedef enum : uint8_t
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE,
} LogLevel_e;

/**
 * @brief 日志输出级别过滤
 * @note 只输出级别 >= UART_LOG_LEVEL 的日志
 */
#ifndef UART_LOG_LEVEL
#define UART_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

/*============= API 声明 =============*/

/**
 * @brief 初始化 UART 日志模块
 * @note 注册 USART 实例，初始化缓冲区
 */
void BSPLogInit(void);

/**
 * @brief 底层日志发送函数
 * @param level  日志级别
 * @param format 格式化字符串
 * @param ...    可变参数
 * @note 内部使用 DMA 发送，非阻塞
 */
void BSPLogOutput(LogLevel_e level, const char *format, ...);

/*============= 日志宏定义 =============*/

#ifdef UART_LOG_USED

// 日志级别过滤
#if UART_LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOGDEBUG(format, ...) BSPLogOutput(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#else
#define LOGDEBUG(format, ...) ((void)0)
#endif

#if UART_LOG_LEVEL <= LOG_LEVEL_INFO
#define LOGINFO(format, ...) BSPLogOutput(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#else
#define LOGINFO(format, ...) ((void)0)
#endif

#if UART_LOG_LEVEL <= LOG_LEVEL_WARNING
#define LOGWARNING(format, ...) BSPLogOutput(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#else
#define LOGWARNING(format, ...) ((void)0)
#endif

#if UART_LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOGERROR(format, ...) BSPLogOutput(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#else
#define LOGERROR(format, ...) ((void)0)
#endif

#else // UART_LOG_USED != 1

// 日志系统禁用，编译后无代码
#define LOGDEBUG(format, ...) ((void)0)
#define LOGINFO(format, ...) ((void)0)
#define LOGWARNING(format, ...) ((void)0)
#define LOGERROR(format, ...) ((void)0)

#endif // UART_LOG_USED

#endif // HAL_UART_MODULE_ENABLED

#endif /* __BSP_UART_LOG_H */
