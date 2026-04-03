/**
 * @file bsp_log.c
 * @brief 日志系统封装实现
 */

#include "bsp_log.h"

void BSPLogInit(void)
{
    SEGGER_RTT_Init();
}

int PrintLog(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = SEGGER_RTT_vprintf(BUFFER_INDEX, fmt, &args);
    va_end(args);
    return n;
}

void Float2Str(char *str, float val)
{
    int flag = val < 0;
    int head = (int)val;
    int point = (int)((val - head) * 1000);
    head = abs(head);
    point = abs(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}