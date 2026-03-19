/**
 * @file bsp_log.h
 * @brief 日志系统封装，基于SEGGER RTT实现
 */

#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#include <stdio.h>
#include <stdarg.h>

/*------------- 宏定义 --------------*/
#define BUFFER_INDEX 0

/*------------- 外部接口声明 --------------*/

/**
 * @brief 日志系统初始化
 */
void BSPLogInit(void);

/*------------- 日志输出宏 --------------*/

/**
 * @brief 日志功能原型，供下面的LOGINFO,LOGWARNING,LOGERROR等使用
 */
#define LOG_PROTO(type, color, format, ...)                       \
        SEGGER_RTT_printf(BUFFER_INDEX, "  %s%s" format "\r\n%s", \
                          color,                                  \
                          type,                                   \
                          ##__VA_ARGS__,                          \
                          RTT_CTRL_RESET)

/**
 * @brief 清屏
 */
#define LOG_CLEAR() SEGGER_RTT_WriteString(0, "  " RTT_CTRL_CLEAR)

/**
 * @brief 无颜色日志输出
 */
#define LOG(format, ...) LOG_PROTO("", "", format, ##__VA_ARGS__)

/**
 * @brief 有颜色格式日志输出
 * @attention 这些接口不支持浮点格式化输出，若有需要，请使用 Float2Str() 函数进行转换后再打印
 * @note 在 release 版本上车使用时，与 makefile 中添加的宏 DISABLE_LOG_SYSTEM 一起使用，可以关闭日志系统
 */
#if DISABLE_LOG_SYSTEM
#define LOGINFO(format, ...)
#define LOGWARNING(format, ...)
#define LOGERROR(format, ...)
#else
// information level
#define LOGINFO(format, ...) LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)
// warning level
#define LOGWARNING(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
// error level
#define LOGERROR(format, ...) LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)
#endif /* DISABLE_LOG_SYSTEM */

/**
 * @brief 通过 SEGGER RTT 打印日志，支持格式化输出
 * @attention 此函数不支持浮点格式化，若有需要，请使用 Float2Str() 函数进行转换后再打印
 * @param fmt 格式字符串
 * @param ... 参数列表
 * @return 打印的字符数
 */
int PrintLog(const char *fmt, ...);

/**
 * @brief 将 float 转换为字符串
 * @attention 浮点数需要转换为字符串后才能通过 RTT 打印
 * @param str 转换后的字符串缓冲区
 * @param val 待转换的浮点数
 * @example char str[16];
 * @example Float2Str(str, 3.14f);
 * @example LOGINFO("value = %s", str);
 */
void Float2Str(char *str, float val);

#endif /* __BSP_LOG_H */
