#include "drv_daemon.h"

#ifdef DAEMON_USED

#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_dwt.h"

// 用于保存所有的daemon instance
static DaemonInstance *s_daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t s_idx = 0;

void DaemonRegister(DaemonInstance *inst)
{
    if (!inst || s_idx >= DAEMON_MX_CNT)
        return;
    s_daemon_instances[s_idx++] = inst;
}

void DaemonReload(DaemonInstance *instance)
{
    if (!instance)
        return;

    if (!instance->is_online)
    {
        instance->is_online = 1;
        LOGINFO("[DAEMON] Module 0x%08X back ONLINE", (uint32_t)(uintptr_t)instance->owner_id);
    }

    instance->temp_count = instance->reload_count;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance ? instance->temp_count > 0 : 0;
}

void DaemonTask(void)
{
    DaemonInstance *dins;
    for (size_t i = 0; i < s_idx; ++i)
    {
        dins = s_daemon_instances[i];
        if (dins->temp_count > 0)
        {
            dins->temp_count--;
            if (dins->temp_count == 0)
            {
                dins->is_online = 0;
                LOGERROR("[DAEMON] Module 0x%08X OFFLINE", (uint32_t)(uintptr_t)dins->owner_id);
                if (dins->callback)
                    dins->callback(dins->owner_id);
            }
        }
    }
}

/*==================== RTOS 任务 ====================*/

static TaskHandle_t s_daemonTaskHandle;
static StackType_t s_daemonTaskStack[DAEMON_STACK_SIZE];
static StaticTask_t s_daemonTaskTCB;

ITCM_RAM __attribute__((noreturn)) static void StartDaemonTask(void *argument)
{
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] DAEMON Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        DaemonTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > DAEMON_FREQ_MS)
            LOGERROR("[freeRTOS] DAEMON Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(DAEMON_FREQ_MS));
    }
}

void DaemonInit(uint32_t priority)
{
    s_daemonTaskHandle = xTaskCreateStatic(StartDaemonTask, "daemonTask", DAEMON_STACK_SIZE, NULL, priority, s_daemonTaskStack, &s_daemonTaskTCB);
}

#else

void DaemonRegister(DaemonInstance *inst)
{
    (void)inst;
}

void DaemonReload(DaemonInstance *instance)
{
    (void)instance;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    (void)instance;
    return 0;
}

void DaemonTask(void)
{
}

void DaemonInit(uint32_t priority)
{
    (void)priority;
}

#endif // DAEMON_USED
