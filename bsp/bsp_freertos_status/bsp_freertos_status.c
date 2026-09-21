/**
 * @file bsp_freertos_status.c
 * @brief FreeRTOS 运行状态模块实现
 *
 * @note 使用示例：
 *       1. 打开开关即可：钩子由内核调用，结构体由空闲钩子周期刷新，
 *          调试器查看 bsp_freertos_status（tasks[] 里是每个任务的快照）。
 *       2. 任务内看本任务的栈高水位：BSP_FreeRTOSStatusRefresh()，
 *          再读 bsp_freertos_status.stack_high_water。
 *
 * @note 任务快照走 uxTaskGetSystemState() + 静态数组（详见头文件说明）：
 *       不用 vTaskList / vTaskGetRunTimeStats（它们要动态分配 + sprintf），
 *       也不用格式化字符串——快照直接按字段抄进结构体，调试器展开就能看。
 */

#include "bsp_freertos_status.h"

#ifdef BSP_FREERTOS_STATUS_USED

#include <string.h>

/* 运行时间统计的计数源（DWT 微秒计数）：仅统计功能真正可用时才需要 */
#if (configGENERATE_RUN_TIME_STATS == 1) && defined(BSP_DWT_USED)
#include "bsp_dwt.h"
#endif

/*============================================
 *              运行状态变量
 *============================================*/

/* 运行状态全局变量定义（供调试器读取） */
BSP_FreeRTOSStatus_s bsp_freertos_status = {0};

/* 上次刷新时的 tick：空闲钩子限频用（只在空闲钩子与刷新接口里访问） */
static TickType_t s_last_refresh_tick = 0;

#if (BSP_FREERTOS_STATUS_HAS_TASK_TABLE == 1)
/* 任务快照数组：uxTaskGetSystemState() 的输出缓冲。这里是静态数组（不是 vTaskList
 * 内部的 pvPortMalloc），所以静态分配下也能取任务快照 */
static TaskStatus_t s_task_status[BSP_FREERTOS_STATUS_TASK_MAX];

/**
 * @brief 取一份任务快照，逐字段抄进 bsp_freertos_status.tasks[]
 * @note 任务数超过 BSP_FREERTOS_STATUS_TASK_MAX 时整个快照不刷新（保留上一次的），
 *       task_skip_cnt 累加提示：调大宏即可
 */
static void prv_refresh_task_table(void)
{
    UBaseType_t i;
    UBaseType_t n;
    uint32_t total_run_time = 0;
    uint32_t total_div_100 = 0;
    uint32_t stack_free_min = 0xFFFFFFFFu;

    if (uxTaskGetNumberOfTasks() > (UBaseType_t)BSP_FREERTOS_STATUS_TASK_MAX)
    {
        bsp_freertos_status.task_skip_cnt++;
        return;
    }

    /* 快照：n 个任务的信息写进 s_task_status，total_run_time 是系统总运行计数 */
    n = uxTaskGetSystemState(s_task_status, (UBaseType_t)BSP_FREERTOS_STATUS_TASK_MAX, &total_run_time);
    if (n == 0u)
    {
        /* 快照期间任务数变多（uxTaskGetSystemState 装不下就直接返回 0） */
        bsp_freertos_status.task_skip_cnt++;
        return;
    }

    /* 先清空：任务数变少时不会残留上一次的条目，任务名的 '\0' 也由这里补齐 */
    memset(bsp_freertos_status.tasks, 0, sizeof(bsp_freertos_status.tasks));

    bsp_freertos_status.task_count = (uint32_t)n;
    bsp_freertos_status.current_task_idx = BSP_FREERTOS_STATUS_NO_TASK;

#if (BSP_FREERTOS_STATUS_HAS_RUN_TIME_STATS == 1)
    /* 先除 100 再相除（同内核写法）：避免"先乘 100"在 32 位计数下溢出 */
    total_div_100 = total_run_time / 100u;
    bsp_freertos_status.run_time_total_us = total_run_time;
#endif

    for (i = 0u; i < n; i++)
    {
        BSP_FreeRTOSTaskStatus_s *task = &bsp_freertos_status.tasks[i];
        uint32_t k = 0;

        /* 任务名：拷贝（最多 configMAX_TASK_NAME_LEN - 1 个字符，末尾留 '\0'） */
        if (s_task_status[i].pcTaskName != NULL)
        {
            while ((k < ((uint32_t)configMAX_TASK_NAME_LEN - 1u)) && (s_task_status[i].pcTaskName[k] != '\0'))
            {
                task->name[k] = s_task_status[i].pcTaskName[k];
                k++;
            }
        }

        task->state = s_task_status[i].eCurrentState;
        task->priority = s_task_status[i].uxCurrentPriority;
        task->base_priority = s_task_status[i].uxBasePriority;
        task->stack_free_words = (uint32_t)s_task_status[i].usStackHighWaterMark;
        task->task_number = (uint32_t)s_task_status[i].xTaskNumber;
        task->stack_base = s_task_status[i].pxStackBase;
        task->handle = s_task_status[i].xHandle;

#if (BSP_FREERTOS_STATUS_HAS_RUN_TIME_STATS == 1)
        task->run_time_us = s_task_status[i].ulRunTimeCounter;
        task->run_time_pct = (total_div_100 > 0u) ? (s_task_status[i].ulRunTimeCounter / total_div_100) : 0u;
#endif

        /* 当前正在跑的任务：vTaskGetInfo 只对 pxCurrentTCB 填 eRunning，据此定位 */
        if (task->state == eRunning)
        {
            bsp_freertos_status.current_task_idx = (uint32_t)i;
        }

        if (task->stack_free_words < stack_free_min)
        {
            stack_free_min = task->stack_free_words;
        }
    }

    bsp_freertos_status.stack_free_min_words = stack_free_min;
}
#endif /* BSP_FREERTOS_STATUS_HAS_TASK_TABLE == 1 */

/*============================================
 *              接口实现
 *============================================*/

void BSP_FreeRTOSStatusRefresh(void)
{
    bsp_freertos_status.refresh_cnt++;

    /* 内核全局状态：恒可用 */
    bsp_freertos_status.tick_count = (uint32_t)xTaskGetTickCount();
    bsp_freertos_status.task_num = (uint32_t)uxTaskGetNumberOfTasks();
    bsp_freertos_status.scheduler_state = (uint32_t)xTaskGetSchedulerState();

#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
    /* 剩余堆空间（字节）：动态分配关闭时没有堆，此字段保持 0 */
    bsp_freertos_status.free_heap = (uint32_t)xPortGetFreeHeapSize();
#endif

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    /* 当前（调用本函数的）任务栈剩余最小值（字）：越接近 0 越危险 */
    bsp_freertos_status.stack_high_water = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
#endif

#if (BSP_FREERTOS_STATUS_HAS_TASK_TABLE == 1)
    /* 任务快照 */
    prv_refresh_task_table();
#endif
}

/*============================================
 *              钩子函数实现
 *============================================*/

#if (configUSE_IDLE_HOOK == 1)
/**
 * @brief 空闲钩子：计次 + 限频刷新系统状态
 * @note 空闲任务每轮循环都会调用本函数，必须立即返回、不可阻塞（不能调用带阻塞
 *       时间的 API、不能 vTaskDelay）：这里只自增计数并和上次刷新时刻比较，
 *       耗时的任务快照由刷新周期限频
 * @note 空闲钩子调用次数增长变慢，说明 CPU 被高优先级任务占满
 */
void vApplicationIdleHook(void)
{
    TickType_t now;

    bsp_freertos_status.idle_cnt++;

    now = xTaskGetTickCount();
    if ((now - s_last_refresh_tick) >= pdMS_TO_TICKS(BSP_FREERTOS_STATUS_PERIOD_MS))
    {
        s_last_refresh_tick = now;
        BSP_FreeRTOSStatusRefresh();
    }
}
#endif /* configUSE_IDLE_HOOK == 1 */

#if (configUSE_TICK_HOOK == 1)
/**
 * @brief Tick 钩子：只计次
 * @note 在 SysTick 中断里执行，不能阻塞，也不能调用非 FromISR 版本的 API；
 *       这里只自增一个计数，和 configTICK_RATE_HZ 对比即可判断调度器是否还在跑
 */
void vApplicationTickHook(void)
{
    bsp_freertos_status.tick_cnt++;
}
#endif /* configUSE_TICK_HOOK == 1 */

#if (configUSE_MALLOC_FAILED_HOOK == 1)
/**
 * @brief 动态分配失败钩子：计次后停机
 * @note 堆已耗尽（或分配参数异常），继续运行没有意义：关中断停机。
 *       在此打断点，通过调用堆栈定位是谁在分配
 */
void vApplicationMallocFailedHook(void)
{
    bsp_freertos_status.malloc_fail_cnt++;

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* 停机等待调试器：malloc_fail_cnt 记录失败次数 */
    }
}
#endif /* configUSE_MALLOC_FAILED_HOOK == 1 */

#if (configCHECK_FOR_STACK_OVERFLOW > 0)
/**
 * @brief 栈溢出钩子：记录任务名后停机
 * @note 栈已经损坏，钩子不允许返回，所以记录后关中断停机：在此打断点，
 *       用 pcTaskName 或结构体里的 overflow_task 定位是哪个任务，
 *       再放大对应任务的栈（TaskInstance.stack_size）
 * @param xTask 溢出任务的句柄（未使用）
 * @param pcTaskName 溢出任务名（长度不超过 configMAX_TASK_NAME_LEN）
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    uint32_t i = 0;

    (void)xTask;

    bsp_freertos_status.stack_overflow_cnt++;

    if (pcTaskName != NULL)
    {
        for (; (i < ((uint32_t)configMAX_TASK_NAME_LEN - 1u)) && (pcTaskName[i] != '\0'); i++)
        {
            bsp_freertos_status.overflow_task[i] = pcTaskName[i];
        }
    }
    bsp_freertos_status.overflow_task[i] = '\0';

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* 停机等待调试器：overflow_task 记录溢出任务名 */
    }
}
#endif /* configCHECK_FOR_STACK_OVERFLOW > 0 */

/*============================================
 *        运行时间统计时基（可选强符号）
 *============================================*/

#if (configGENERATE_RUN_TIME_STATS == 1) && defined(BSP_DWT_USED)
/**
 * @brief 运行时间统计时基初始化（portCONFIGURE_TIMER_FOR_RUN_TIME_STATS）
 * @note DWT 已由启动流程 DWT_Init() 初始化（app 初始化先于 vTaskStartScheduler），
 *       这里不需要额外配置；本函数只为覆盖 CubeMX 生成的 __weak 空实现
 */
void configureTimerForRunTimeStats(void)
{
}

/**
 * @brief 读取运行时间统计计数（portGET_RUN_TIME_COUNTER_VALUE）
 * @return DWT 微秒计数（按 32 位截断：任务计数与总时间是同一批增量累加、
 *         同步回绕，任务占比仍然正确）
 * @note 不实现（用 CubeMX 的 __weak 版本时恒返回 0，run_time_us / pct 会全是 0）
 */
unsigned long getRunTimeCounterValue(void)
{
    return (unsigned long)DWT_GetTimeUs();
}
#endif /* configGENERATE_RUN_TIME_STATS == 1 && BSP_DWT_USED */

#endif /* BSP_FREERTOS_STATUS_USED */
