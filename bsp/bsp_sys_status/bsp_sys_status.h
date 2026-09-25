/**
 * @file bsp_sys_status.h
 * @brief 系统状态模块：集中记录系统运行状态计数，供各层读取判断健康度
 *
 * @note 设计目的：
 *       1. 定义系统状态结构体 BSP_SysStatus_s 与全局实例 bsp_sys_status；每个
 *          字段是一个 uint32_t 累计计数，读值即可判断系统状态是否健康，如
 *          bsp_sys_status.app_init_err != 0 说明 app 初始化存在异常。
 *       2. 所有 "+1" 都收敛到本模块的计数接口：发现异常时直接在计数函数打断点，
 *          结合调用堆栈即可定位是哪个模块的注册/配置/调用异常。
 *       3. 当前计数项（后续按需扩充）：
 *          - app_init_err     ：app 初始化错误计数（app 调用 bsp/drv 注册/配置/
 *                               初始化返回值非 0 的累计次数）
 *          - app_task_timeout ：app FreeRTOS 任务超时计数
 *       4. 开关 BSP_SYS_STATUS_USED（在 app_cfg.h 中定义）：未定义时结构体与
 *          接口不参与编译，便捷宏全部退化为空实现——BSP_ASSERT_APP_CALL 仍会
 *          执行被调函数（副作用保留），只是不计数（与 bsp_sys_status.c 的 #if 一致）。
 *
 * @note 使用示例（app 层）：
 *       1. 调用 bsp/drv 注册/配置函数，自动检查返回值（非 0 计数 +1）：
 *          BSP_ASSERT_APP_CALL(CANRegister(&can_inst));
 *       2. 只上报一次，不关心返回值：
 *          BSP_ASSERT_APP();
 *       3. 任务超时上报：
 *          BSP_ASSERT_TASK_TIMEOUT();
 */

#ifndef __BSP_SYS_STATUS_H
#define __BSP_SYS_STATUS_H

#include <stdint.h>
#include "app_cfg.h"

/*============================================
 *              系统状态结构体
 *============================================*/

/**
 * @brief 系统状态结构体
 * @note 各成员均为 uint32_t 累计计数，bsp_sys_status 为其全局实例。
 *       原 bsp_assert 的分组计数（bsp/drv/app 三层）中，bsp/drv 分组无调用点，
 *       本次收敛为具名字段：app 初始化错误对应原 BSP_ASSERT_GROUP_APP。
 */
typedef struct
{
    uint32_t app_init_err;     /* app 初始化错误计数：bsp/drv 注册/配置/初始化返回值非 0 累计 */
    uint32_t app_task_timeout; /* app FreeRTOS 任务超时计数 */
} BSP_SysStatus_s;

/* 系统状态全局变量（供各层读取判断运行状态） */
extern BSP_SysStatus_s bsp_sys_status;

/*============================================
 *              接口声明
 *============================================*/

/**
 * @brief app 初始化错误计数 +1（app 上报 bsp/drv 调用异常的唯一入口）
 * @note 调试：在此函数打断点，初始化异常时会在断点停下，
 *       通过调用堆栈定位是哪个模块的注册/配置/调用异常
 */
void BSP_SysStatusAppInitErrCount(void);

/**
 * @brief app FreeRTOS 任务超时计数 +1
 * @note 调试：在此函数打断点，可定位是哪个任务发生超时
 */
void BSP_SysStatusAppTaskTimeoutCount(void);

#ifdef BSP_SYS_STATUS_USED

/*============================================
 *              便捷宏
 *============================================*/

#define BSP_ASSERT_APP() BSP_SysStatusAppInitErrCount()
#define BSP_ASSERT_TASK_TIMEOUT() BSP_SysStatusAppTaskTimeoutCount()

/**
 * @brief 调用函数并断言返回值为 0（0 视为成功，非 0 视为异常）
 * @param func 要调用的 bsp/drv 函数调用表达式
 * @note 用于 app 层调用 bsp/drv 注册/配置/初始化函数时自动检查返回值：
 *       返回值非 0 则调用 BSP_SysStatusAppInitErrCount() 计数 +1。
 *       约定：仅适用于"返回 0 表示成功"的注册/配置类函数
 *       （如 CANRegister / CANConfig 返回 0 成功、-1 失败）。
 * @example BSP_ASSERT_APP_CALL(CANRegister(&can_inst));
 */
#define BSP_ASSERT_APP_CALL(func)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((func) != 0)                                                                                               \
        {                                                                                                              \
            BSP_SysStatusAppInitErrCount();                                                                            \
        }                                                                                                              \
    } while (0)

#else /* 系统状态关闭：BSP_SYS_STATUS_USED 未定义（与 bsp_sys_status.c 的 #if 一致） */

/* 关闭态为空实现：接口不编译，宏吃掉调用点但保留被调函数副作用，零计数零 RAM */
#define BSP_ASSERT_APP() ((void)0)
#define BSP_ASSERT_TASK_TIMEOUT() ((void)0)
#define BSP_ASSERT_APP_CALL(func) ((void)(func))

#endif /* BSP_SYS_STATUS_USED */

#endif /* __BSP_SYS_STATUS_H */
