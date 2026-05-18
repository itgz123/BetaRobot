#include "drv_daemon.h"

#ifdef DAEMON_USED

#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_dwt.h"

// 用于保存所有的daemon instance
static DaemonInstance *s_daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t s_idx = 0;

void DaemonRegister(DaemonInstance *inst, const Daemon_Init_Config_s *config)
{
    if (!inst || !config || s_idx >= DAEMON_MX_CNT)
        return;

    if (config->fault_action > DAEMON_FAULT_RESERVED_7)
    {
        LOGERROR("[DAEMON] Invalid fault_action: %d, max: %d", config->fault_action, DAEMON_FAULT_RESERVED_7);
        return;
    }

    if (config->owner_id == NULL)
    {
        LOGWARNING("[DAEMON] owner_id is NULL, daemon may not identify offline module");
    }

    inst->reload_count = config->reload_count;
    inst->fault_action = config->fault_action;
    inst->callback = config->callback;
    inst->owner_id = config->owner_id;
    inst->temp_count = config->reload_count;
    inst->last_reload_us = DWT_GetTimeUs();

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
    instance->last_reload_us = DWT_GetTimeUs();
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

                // 执行离线故障动作
                switch (dins->fault_action)
                {
                case DAEMON_FAULT_BUZZER_SHORT:
                    break;
                case DAEMON_FAULT_BUZZER_LONG:
                    break;
                case DAEMON_FAULT_LIGHT_SHORT:
                    break;
                case DAEMON_FAULT_LIGHT_LONG:
                    break;
                case DAEMON_FAULT_RESERVED_5:
                    break;
                case DAEMON_FAULT_RESERVED_6:
                    break;
                case DAEMON_FAULT_RESERVED_7:
                    break;
                case DAEMON_FAULT_NONE:
                default:
                    break;
                }

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
        start = DWT_GetTimeUs();
        DaemonTask();
        dt = DWT_GetTimeUs() - start;
        if ((dt / 1000) > DAEMON_FREQ_MS)
            LOGERROR("[freeRTOS] DAEMON Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(DAEMON_FREQ_MS));
    }
}

void DaemonInit(void)
{
    s_daemonTaskHandle = xTaskCreateStatic(StartDaemonTask, "daemonTask", DAEMON_STACK_SIZE, NULL, DAEMON_TASK_PRIORITY, s_daemonTaskStack, &s_daemonTaskTCB);
}

#else

void DaemonRegister(DaemonInstance *inst, const Daemon_Init_Config_s *config)
{
    if (!inst || !config)
        return;
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

void DaemonInit(void)
{
}

#endif // DAEMON_USED
