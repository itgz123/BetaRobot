/**
 * @file bsp_freertos_status.h
 * @brief FreeRTOS 运行状态模块：钩子函数 + 空闲任务把内核状态抄进全局变量，供调试器读取
 *
 * @note 设计目的：
 *       1. 集中实现 FreeRTOS 的 4 个钩子函数（空闲 / Tick / 内存分配失败 /
 *          栈溢出）。CubeMX 生成的 cubemx/xxx/Core/Src/freertos.c 里这些函数是
 *          __weak 空实现，本模块的强符号自动覆盖，不需要改生成代码。
 *       2. 空闲钩子按周期把内核状态写进全局结构体 bsp_freertos_status：钩子计数、
 *          调度器状态、任务快照（每个任务的名字 / 状态 / 优先级 / 栈余量 /
 *          运行时间 / TCB 地址）。写的是**结构化变量**，不是格式化字符串——
 *          调试器把 bsp_freertos_status 加到 watch 窗口或 Variables 视图里直接
 *          展开看，不经过日志、不占串口、不依赖任何调试插件。
 *          （写法参考 bsp_sys_status：结构体 + 累计计数 + 开关宏）
 *       3. 空闲钩子里不做重活：任务快照由 BSP_FREERTOS_STATUS_PERIOD_MS 限频，
 *          空闲钩子永远立即返回。
 *       4. 开关 BSP_FREERTOS_STATUS_USED（在 app_cfg.h 中定义）：未定义时钩子、
 *          结构体、接口全部不编译，零代码零 RAM（与 bsp_freertos_status.c 的
 *          #if 一致）。
 *
 * @note 本模块不需要动态内存分配：FreeRTOS 自带的 vTaskList() /
 *       vTaskGetRunTimeStats() 要求 configSUPPORT_DYNAMIC_ALLOCATION == 1 且依赖
 *       sprintf（它们内部先 pvPortMalloc 一块 TaskStatus_t 数组），本项目全静态
 *       分配，内核根本不含这两个函数。这里改用只被 configUSE_TRACE_FACILITY == 1
 *       卡着的 uxTaskGetSystemState()（tasks.c 里没有任何 pvPortMalloc/sprintf）：
 *       静态数组 s_task_status[BSP_FREERTOS_STATUS_TASK_MAX] 自己提供，
 *       取到快照后逐字段抄进 bsp_freertos_status.tasks[]。
 *
 * @note 与调试器侧"内核感知"功能的对比（mcu-debug.rtos-views、OpenOCD 的
 *       rtos FreeRTOS）：那两套都是**调试器侧**方案，不发一行业务代码——第一次停在
 *       Stopped 时按符号名读内核自己的变量，读到了就认为"这是 FreeRTOS"
 *       （.vscode/launch.json 里的 "rtos": "FreeRTOS" 走的是 OpenOCD 的 RTOS
 *       awareness，喂 Threads/Call Stack 视图；rtos-views 是另一个独立插件，两者
 *       并存）。它们读的符号：pxReadyTasksLists、xDelayedTaskList1/2、
 *       xSuspendedTaskList、xTasksWaitingTermination、uxCurrentNumberOfTasks、
 *       pxCurrentTCB（任务视图）、xQueueRegistry[]（队列视图），再顺链表读 TCB 的
 *       pcTaskName / pxStack / pxEndOfStack / uxBasePriority / ulRunTimeCounter。
 *       这些符号在 tasks.c / queue.c 里都是 file-static，能读到只因为带 -g 的 ELF
 *       里有 DWARF 信息。代价：只能在程序 Stopped 时探测（GDB 后端不允许运行中
 *       查询，一次约 1 秒），所以拿不到任何"事件计数"类信息。
 *
 *       本模块是**目标侧**方案：内核自己跑 API 取快照、写进普通 RAM 变量，调试器
 *       只读变量，不需要插件、不需要符号、可以边跑边看。两者的覆盖对比：
 *
 *       | rtos-views 视图                         | 本模块            | 说明                                        |
 *       | --------------------------------------- | ----------------- | ------------------------------------------- |
 *       | 任务列表（名字/状态/优先级/栈余量/时间） | tasks[]           | 本模块多基础优先级、栈余量全局最小值、当前任务 |
 *       | 运行时间 / CPU 占用                      | tasks[].run_time_*| 计数源同为 DWT（强符号由本模块提供，见下）   |
 *       | 栈溢出 / 栈水位                          | tasks[] + 钩子计数| 本模块另有溢出历史与溢出任务名               |
 *       | 事件计数（idle/tick 次数、malloc 失败）  | 钩子计数字段      | 内核不保存这些，只有目标侧能给               |
 *       | 运行时（不停机）刷新                     | 周期刷新          | rtos-views 必须停机                          |
 *       | 队列 / 信号量 / 互斥量                   | 未实现            | 见下方"队列视图"                             |
 *       | 堆                                       | 不适用            | 全静态分配没有堆，free_heap 字段也不编译     |
 *       | 定时器（Tmr Svc）                        | 不适用            | configUSE_TIMERS 未开                        |
 *
 * @note 队列视图（唯一真正缺的项）：内核把队列登记在 xQueueRegistry[] 里（queue.c，
 *       源码注释原文是 "just a means for kernel aware debuggers"），但**没人调用
 *       vQueueAddToRegistry() 时它是空表**——本工程现在正是如此，所以 rtos-views
 *       的队列视图目前是空的。要补的话两条路，都不需要动态内存：
 *       A. 只在 bsp_freertos.c 的 QueueRegister() 里加一行
 *          vQueueAddToRegistry(inst->handle, inst->name)（QueueInstance 再加个
 *          name 字段）→ rtos-views 的队列视图自动就有内容；
 *       B. 在 A 的基础上再遍历注册表（xQueueRegistry 是外部链接符号，extern 一个
 *          同构结构体即可）把 uxMessagesWaiting / uxQueueSpacesAvailable 抄进本
 *          结构体 → 不装插件也能看。
 *       堆视图要 heap_x.c（本项目不引入），定时器视图要 configUSE_TIMERS，均不适用。
 *
 * @note 钩子函数与写入字段（对照《CubeMX_FreeRTOS 配置指南》第 8 节）：
 *
 *       | 钩子函数                      | FreeRTOSConfig.h 开关          | 字段                |
 *       | ----------------------------- | ------------------------------ | ------------------- |
 *       | vApplicationIdleHook          | configUSE_IDLE_HOOK            | idle_cnt + 周期刷新 |
 *       | vApplicationTickHook          | configUSE_TICK_HOOK            | tick_cnt            |
 *       | vApplicationMallocFailedHook  | configUSE_MALLOC_FAILED_HOOK   | malloc_fail_cnt     |
 *       | vApplicationStackOverflowHook | configCHECK_FOR_STACK_OVERFLOW | stack_overflow_cnt  |
 *
 * @note 状态字段与前提配置（对照第 3、7、10、11 节）。不满足前提的字段不编译
 *       （字段整个不存在，省 RAM），也不需要改本模块代码：
 *
 *       | 字段             | 需要的 FreeRTOSConfig.h 配置                                              |
 *       | ---------------- | ------------------------------------------------------------------------- |
 *       | task_num         | 无（uxTaskGetNumberOfTasks 恒可用）                                       |
 *       | free_heap        | configSUPPORT_DYNAMIC_ALLOCATION == 1（本项目全静态分配，无堆 → 不编译）   |
 *       | stack_high_water | INCLUDE_uxTaskGetStackHighWaterMark == 1（第 10 节）                      |
 *       | tasks[] 全部字段 | configUSE_TRACE_FACILITY == 1（第 7 节）；全程静态分配，不需要动态内存     |
 *       | tasks[].run_time | 上一行 + configGENERATE_RUN_TIME_STATS == 1（第 7、11.1 节）               |
 *
 *       configUSE_TRACE_FACILITY 若为 0，tasks[] 与 task_count 一起不编译——
 *       省下来的是快照数组那段 RAM。
 *
 * @note tasks[].run_time_us 的计数源：configGENERATE_RUN_TIME_STATS == 1 且定义
 *       BSP_DWT_USED 时，本模块提供 getRunTimeCounterValue() /
 *       configureTimerForRunTimeStats() 强符号（覆盖 CubeMX 生成的返回 0 的
 *       __weak 版本），计数源为 DWT 微秒计数——否则 run_time_us / pct 会全是 0。
 *
 * @note 用法（调试）：
 *       1. 打开开关后什么都不用做：钩子由内核调用，结构体由空闲钩子周期刷新，
 *          调试器把 bsp_freertos_status 加到 watch 窗口即可。
 *       2. 想看某个任务自己的栈高水位（空闲钩子刷新的是空闲任务的）：
 *          在该任务里调用 BSP_FreeRTOSStatusRefresh()，再读
 *          bsp_freertos_status.stack_high_water；或者直接看 tasks[] 里
 *          该任务的 stack_free_words。
 */

#ifndef __BSP_FREERTOS_STATUS_H
#define __BSP_FREERTOS_STATUS_H

#include "FreeRTOS.h"
#include "task.h"
#include "app_cfg.h"

/*============================================
 *              调试参数
 *============================================*/

/* 任务快照数组容量（任务个数）：uxTaskGetSystemState 的输出缓冲，
 * 每个 TaskStatus_t 约 36 字节（运行时用）+ 每条快照 52 字节（结构体里）。
 * 任务数超过它时快照整个不刷新（保留上一次的），并累加 task_skip_cnt 提示调大 */
#ifndef BSP_FREERTOS_STATUS_TASK_MAX
#define BSP_FREERTOS_STATUS_TASK_MAX 8
#endif

/* 空闲钩子刷新周期（ms）：刷新含任务快照，不宜过密 */
#ifndef BSP_FREERTOS_STATUS_PERIOD_MS
#define BSP_FREERTOS_STATUS_PERIOD_MS 1000
#endif

#ifdef BSP_FREERTOS_STATUS_USED

/*============================================
 *          内核能力探测（内部宏）
 *============================================*/

/* 任务快照是否可用：uxTaskGetSystemState() 在 tasks.c 里只被 configUSE_TRACE_FACILITY
 * 卡着（不像 vTaskList 还要 configUSE_STATS_FORMATTING_FUNCTIONS +
 * configSUPPORT_DYNAMIC_ALLOCATION + sprintf），静态分配下也能用 */

#if (configUSE_TRACE_FACILITY == 1)
#define BSP_FREERTOS_STATUS_HAS_TASK_TABLE 1
#else
#define BSP_FREERTOS_STATUS_HAS_TASK_TABLE 0
#endif

/* 任务运行时间：同一份快照里的 ulRunTimeCounter 与总运行计数 */
#if (BSP_FREERTOS_STATUS_HAS_TASK_TABLE == 1) && (configGENERATE_RUN_TIME_STATS == 1)
#define BSP_FREERTOS_STATUS_HAS_RUN_TIME_STATS 1
#else
#define BSP_FREERTOS_STATUS_HAS_RUN_TIME_STATS 0
#endif

/* current_task_idx 的"没找到"取值（调度器还没跑起来时就是这个） */
#define BSP_FREERTOS_STATUS_NO_TASK 0xFFFFFFFFu

/*============================================
 *              运行状态结构体
 *============================================*/

/**
 * @brief 单个任务的快照
 * @note 调试器展开 bsp_freertos_status.tasks[i] 看到的每一列——
 *       定长字段，无需解析字符串；name 是拷贝（任务被删掉也不会悬空）
 */
typedef struct
{
    char name[configMAX_TASK_NAME_LEN]; /* 任务名（调试器按字符串显示） */
    eTaskState state;                   /* 内核状态枚举：eRunning/eReady/eBlocked/eSuspended/eDeleted */
    UBaseType_t priority;               /* 当前优先级（被互斥量继承时会高于基础优先级） */
    UBaseType_t base_priority;          /* 基础优先级（TaskInstance 里配的那个） */
    uint32_t stack_free_words;          /* 栈剩余最小值（字，1 字 = 4 字节）：越接近 0 越危险 */
    uint32_t run_time_us;               /* 累计运行计数（µs，DWT）；统计关闭时恒 0 */
    uint32_t run_time_pct;              /* 占系统总运行时间的百分比 */
    uint32_t task_number;               /* 内核给的任务序号（uxTCBNumber） */
    StackType_t *stack_base;            /* 栈底地址（可对照 .map / ld 看地址范围） */
    TaskHandle_t handle;                /* 任务句柄 = TCB 地址（想手挖内核字段时用） */
} BSP_FreeRTOSTaskStatus_s;

/**
 * @brief FreeRTOS 运行状态结构体
 * @note 各计数字段均为累计值，bsp_freertos_status 为其全局实例——
 *       调试器直接读它即可，不需要额外导出接口
 */
typedef struct
{
    /* ---------- 钩子调用计数（内核不保存这些，只有钩子能给） ---------- */
    uint32_t idle_cnt;                           /* 空闲钩子调用次数：单位时间涨得慢说明 CPU 被占满 */
    uint32_t tick_cnt;                           /* Tick 钩子调用次数（中断内自增）：1 秒应涨 configTICK_RATE_HZ */
    uint32_t malloc_fail_cnt;                    /* 动态分配失败次数：>0 说明堆不够（本项目静态分配，恒 0） */
    uint32_t stack_overflow_cnt;                 /* 栈溢出次数：>0 时系统已停机，见 overflow_task */
    char overflow_task[configMAX_TASK_NAME_LEN]; /* 发生栈溢出的任务名 */

    /* ---------- 空闲钩子周期刷新（内核全局状态） ---------- */
    uint32_t refresh_cnt;      /* 刷新次数 */
    uint32_t tick_count;       /* 系统 tick（xTaskGetTickCount，单位 ms） */
    uint32_t task_num;         /* 内核当前任务数（含空闲任务） */
    uint32_t scheduler_state;  /* taskSCHEDULER_NOT_STARTED(0) / RUNNING(1) / SUSPENDED(2) */
    uint32_t stack_high_water; /* 刷新任务的栈剩余最小值（字） */
    uint32_t free_heap;        /* 剩余堆空间（字节，仅动态分配时存在） */

    /* ---------- 任务快照（configUSE_TRACE_FACILITY == 1 时才有） ---------- */
#if (BSP_FREERTOS_STATUS_HAS_TASK_TABLE == 1)
    uint32_t task_count;           /* 本次快照填了几条（正常等于 task_num） */
    uint32_t task_skip_cnt;        /* 任务数超过 BSP_FREERTOS_STATUS_TASK_MAX、快照未刷新的次数 */
    uint32_t current_task_idx;     /* 当前运行任务在 tasks[] 里的下标（BSP_FREERTOS_STATUS_NO_TASK = 没找到） */
    uint32_t stack_free_min_words; /* 所有任务里最小的栈余量（字）：一眼看有没有任务快溢出了 */
#if (BSP_FREERTOS_STATUS_HAS_RUN_TIME_STATS == 1)
    uint32_t run_time_total_us; /* 系统总运行计数（µs）：run_time_pct 的分母 */
#endif
    BSP_FreeRTOSTaskStatus_s tasks[BSP_FREERTOS_STATUS_TASK_MAX];
#endif
} BSP_FreeRTOSStatus_s;

/* 运行状态全局变量（供调试器读取；也可被其他模块 extern 读取） */
extern BSP_FreeRTOSStatus_s bsp_freertos_status;

/*============================================
 *              接口声明
 *============================================*/

/**
 * @brief 刷新系统状态到 bsp_freertos_status
 * @note 空闲钩子按 BSP_FREERTOS_STATUS_PERIOD_MS 自动调用；也可在任意任务里手动
 *       调用，此时 stack_high_water 记录的就是该任务的栈剩余最小值
 * @attention 不可在中断中调用（内部含不可重入的任务快照），不重入
 */
void BSP_FreeRTOSStatusRefresh(void);

/*============================================
 *           钩子函数（内核回调）
 *============================================*/

/* 以下 4 个函数由 FreeRTOS 内核在对应时机调用，本模块提供强符号实现覆盖
 * CubeMX 生成的 __weak 空实现；签名由内核固定，调用点参数 xTask 未使用 */

void vApplicationIdleHook(void);                                          /* configUSE_IDLE_HOOK == 1 时每轮空闲调用 */
void vApplicationTickHook(void);                                          /* configUSE_TICK_HOOK == 1 时每个 Tick 在中断里调用 */
void vApplicationMallocFailedHook(void);                                  /* configUSE_MALLOC_FAILED_HOOK == 1 时分配失败调用 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName); /* configCHECK_FOR_STACK_OVERFLOW > 0 时栈溢出调用 */

#endif /* BSP_FREERTOS_STATUS_USED */

#endif /* __BSP_FREERTOS_STATUS_H */
