#ifndef __BSP_UART_LOG_H
#define __BSP_UART_LOG_H

#include <stdio.h>
#include <stdarg.h>

#define DISABLE_UART_LOG_SYSTEM 1

void BSPLogInit(void);

#if DISABLE_UART_LOG_SYSTEM
#define LOGINFO(format, ...) ((void)0)
#define LOGWARNING(format, ...) ((void)0)
#define LOGERROR(format, ...) ((void)0)
#else
// information level
#define LOGINFO(format, ...) LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)
// warning level
#define LOGWARNING(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
// error level
#define LOGERROR(format, ...) LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)
#endif /* DISABLE_UART_LOG_SYSTEM */

#endif /* __BSP_UART_LOG_H */
