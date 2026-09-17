/**
 * @file bsp_sys_status.c
 * @brief 系统状态模块实现
 *
 * @note 使用示例：
 *       1. app 调用 bsp/drv 注册/配置函数时自动检查返回值：
 *          BSP_ASSERT_APP_CALL(CANRegister(&inst));
 *       2. app 任务超时上报：
 *          BSP_ASSERT_TASK_TIMEOUT();
 */

#include "bsp_sys_status.h"

#ifdef BSP_SYS_STATUS_USED

/* 系统状态全局变量定义 */
BSP_SysStatus_s bsp_sys_status = {0};

void BSP_SysStatusAppInitErrCount(void)
{
    /* 在此打断点：app 初始化异常会停在这里，查看调用堆栈定位异常模块 */
    bsp_sys_status.app_init_err++;
}

void BSP_SysStatusAppTaskTimeoutCount(void)
{
    /* 在此打断点：app 任务超时 */
    bsp_sys_status.app_task_timeout++;
}

#endif /* BSP_SYS_STATUS_USED */
