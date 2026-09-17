/**
 * @file bsp_app.h
 * @brief APP 层任务框架：周期任务函数生成宏 + 任务启动/超时上报
 *
 * @note 原来每个 app 任务都要手写一遍"固定周期唤醒 + 执行耗时监控 + 超时记录"，
 *       现收敛成函数形宏 APP_TASK_DEF，一行定义一个任务函数：
 *
 *           APP_TASK_DEF(Chassis, CHASSIS_FREQ_MS, AppChassisRun);
 *
 *       展开等价于：
 *
 *           ITCM_RAM static __attribute__((noreturn)) void StartChassisTask(void *argument)
 *           {
 *               ... vTaskDelayUntil 周期唤醒 + DWT 测耗时 + 超时上报 ...
 *           }
 *
 *       函数名沿用旧写法 Start<name>Task，TaskRegister 处无需改动。
 *
 * @note 日志实例由本模块自己持有（bsp_app.c 的 g_task_log，模块名 "task"），
 *       不从调用点传入：LOG_INSTANCE_DEF 在日志关闭（BSP_LOG_USED / LOG_UART 未定义）
 *       时展开为空宏，调用点的日志实例符号根本不存在，传进宏里会编译不过。
 */

#ifndef __BSP_APP_H
#define __BSP_APP_H

#include <stdint.h>

#include "bsp_freertos.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "bsp_map.h"
#include "bsp_sys_status.h"

/*============================================
 *              接口声明
 *============================================*/

/**
 * @brief 任务启动日志
 * @param name 任务名（字符串字面量）
 */
void APP_TaskStartLog(const char *name);

/**
 * @brief 任务超时上报：打 ERROR 日志 + 系统状态超时计数 +1
 * @param name  任务名（字符串字面量）
 * @param dt_us 本周期实际执行耗时（us）
 */
void APP_TaskDelayReport(const char *name, uint64_t dt_us);

/*============================================
 *              周期任务框架
 *============================================*/

/**
 * @brief 周期任务框架（函数形宏）：生成 FreeRTOS 任务函数体
 * @param name_    任务名标识符：用于生成函数名 Start<name_>Task，并作为日志中的任务名
 * @param freq_ms_ 任务周期（ms，编译期常量）
 * @param run_     周期执行体（函数名，宏内补 () 调用，如 AppChassisRun）
 *
 * @note 生成的函数三件事：
 *       1. vTaskDelayUntil 固定周期唤醒（避免 vTaskDelay 的周期漂移）；
 *       2. DWT 测每个周期的执行耗时 dt；
 *       3. dt 超过周期即判为任务超时，走 APP_TaskDelayReport（日志 + 计数）。
 * @note 任务实例仍由 TASK_INSTANCE_DEF 声明、TaskRegister 注册。
 * @example APP_TASK_DEF(Chassis, CHASSIS_FREQ_MS, AppChassisRun);
 */
#define APP_TASK_DEF(name_, freq_ms_, run_)                                              \
    ITCM_RAM static __attribute__((noreturn)) void Start##name_##Task(void *argument)    \
    {                                                                                    \
        static uint64_t start;                                                           \
        static uint64_t dt;                                                              \
        TickType_t xLastWakeTime = xTaskGetTickCount();     /* 周期锚点(绝对唤醒时刻) */ \
        const TickType_t xPeriod = pdMS_TO_TICKS(freq_ms_); /* 任务周期(tick) */         \
        APP_TaskStartLog(#name_);                                                        \
        for (;;)                                                                         \
        {                                                                                \
            vTaskDelayUntil(&xLastWakeTime, xPeriod); /* 固定周期唤醒，避免周期漂移 */   \
            start = DWT_GetTimeUs();                                                     \
            run_();                                                                      \
            dt = DWT_GetTimeUs() - start;                                                \
            if (dt > 1000UL * (freq_ms_))                                                \
            {                                                                            \
                APP_TaskDelayReport(#name_, dt);                                         \
            }                                                                            \
        }                                                                                \
    }

#endif /* __BSP_APP_H */
