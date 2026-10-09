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
 *               ... vTaskDelayUntil 周期唤醒 + 算 dt/时间戳 + DWT 测耗时 + 超时上报 ...
 *               run_(dt_ms, time_stamp_ms);
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
 * @param run_     周期执行体（函数名，宏内补 () 调用，签名须为 void run_(float dt, uint64_t time_stamp)）
 *
 * @note 生成的函数四件事：
 *       1. vTaskDelayUntil 固定周期唤醒（避免 vTaskDelay 的周期漂移）；
 *       2. 每次唤醒取 DWT 时间戳 now，算与上次唤醒的间隔 dt（ms，float）+ 当前时间戳（ms）；
 *       3. 把 dt / time_stamp 作为入参交给 run_，任务内可直接复用，不必各自再取时间；
 *       4. DWT 测本周期的执行耗时，超过周期即判为任务超时，走 APP_TaskDelayReport（日志 + 计数）。
 *
 * @note dt / time_stamp 的语义：dt 是「本次唤醒时刻 - 上次唤醒时刻」的差分（ms），
 *       time_stamp 是本次唤醒时刻（自 DWT_Init 起的 ms）。首拍 dt 为从任务启动到首次唤醒
 *       的间隔（≈freq_ms_）。
 * @note 本框架的 dt 取自唤醒瞬间，受调度抖动影响；若任务需要非常非常精确的 dt
 *       （如积分、位置差分），应在任务内部于真正采样/执行的时刻自行计算
 *       （DWT_GetTimeUs 差分），不要直接沿用这里的 dt。
 * @note 任务实例仍由 TASK_INSTANCE_DEF 声明、TaskRegister 注册。
 * @example APP_TASK_DEF(Chassis, CHASSIS_FREQ_MS, AppChassisRun);
 */
#define APP_TASK_DEF(name_, freq_ms_, run_)                                                                                                                    \
    ITCM_RAM static __attribute__((noreturn)) void Start##name_##Task(void *argument)                                                                          \
    {                                                                                                                                                          \
        uint64_t start;                                                                                                                                        \
        uint64_t now;                                                                                                                                          \
        uint64_t exec_us;                                                                                                                                      \
        uint64_t last = DWT_GetTimeUs();                    /* 上次唤醒时刻(us)，用于算周期 dt */                                                              \
        TickType_t xLastWakeTime = xTaskGetTickCount();     /* 周期锚点(绝对唤醒时刻) */                                                                       \
        const TickType_t xPeriod = pdMS_TO_TICKS(freq_ms_); /* 任务周期(tick) */                                                                               \
        APP_TaskStartLog(#name_);                                                                                                                              \
        for (;;)                                                                                                                                               \
        {                                                                                                                                                      \
            vTaskDelayUntil(&xLastWakeTime, xPeriod); /* 固定周期唤醒，避免周期漂移 */                                                                         \
            now = DWT_GetTimeUs();                                                                                                                             \
            start = now;                                                                                                                                       \
            run_((float)(now - last) * 0.001f, now / 1000ULL); /* dt(ms) + 时间戳(ms) */                                                                       \
            last = now;                                                                                                                                        \
            exec_us = DWT_GetTimeUs() - start;                                                                                                                 \
            if (exec_us > 1000UL * (freq_ms_))                                                                                                                 \
            {                                                                                                                                                  \
                APP_TaskDelayReport(#name_, exec_us);                                                                                                          \
            }                                                                                                                                                  \
        }                                                                                                                                                      \
    }

#endif /* __BSP_APP_H */
