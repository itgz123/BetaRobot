/**
 * @file bsp_app.c
 * @brief APP 层任务框架实现（任务启动/超时上报）
 */

#include "bsp_app.h"

/* 任务框架日志实例（日志关闭时为空宏，不占 RAM） */
LOG_INSTANCE_DEF(g_task_log, "task", 255);

void APP_TaskStartLog(const char *name)
{
    BSPLOG(&g_task_log, LOG_LEVEL_INFO, "%s Task Start", name);
}

void APP_TaskDelayReport(const char *name, uint64_t dt_us)
{
    BSPLOG(&g_task_log, LOG_LEVEL_ERROR, "%s Task is being DELAY! dt = %llu(us)", name, dt_us);
    BSP_ASSERT_TASK_TIMEOUT(); /* 任务超时：系统状态计数 +1 */
}
