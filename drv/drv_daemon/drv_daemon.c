#include "drv_daemon.h"

#ifdef DAEMON_USED

#include "bsp_log.h"
#include "bsp_freertos.h"
#include "bsp_dwt.h"
#include "bsp_tim.h"

// 用于保存所有的daemon instance
static DaemonInstance *s_daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t s_idx = 0;
LOG_INSTANCE_DEF(g_daemon_log, "drv_daemon", 0); // Daemon 日志实例

// 蜂鸣器鸣叫声音表格
const uint8_t voice_map[DAEMON_FAULT_NUM][12] = {0};
static uint8_t buzzer_flag = 0; // TODO:这个之后用位域实现
static uint8_t all_daemon_is_online = 1;

PWM_INSTANCE_DEF(buzzer_pwm);

// Daemon 任务实例
TASK_INSTANCE_DEF(daemon_task, DAEMON_STACK_SIZE);

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config)
{
    if (!inst || !config)
        return;

    if (config->fault_action > DAEMON_FAULT_RESERVED_7)
    {
        BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Invalid fault_action: %d, max: %d", config->fault_action, DAEMON_FAULT_RESERVED_7);
        return;
    }

    if (config->owner_id == NULL)
    {
        BSPLOG(&g_daemon_log, LOG_LEVEL_WARNING, "owner_id is NULL, daemon may not identify offline module");
    }

    inst->reload_count = config->reload_count;
    inst->fault_action = config->fault_action;
    inst->callback = config->callback;
    inst->owner_id = config->owner_id;
    inst->temp_count = config->reload_count;
    inst->last_reload_us = DWT_GetTimeUs();
}

void DaemonRegister(DaemonInstance *inst)
{
    if (!inst || s_idx >= DAEMON_MX_CNT)
        return;

    // 防重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_daemon_instances[i] == inst)
        {
            BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return;
        }
    }

    s_daemon_instances[s_idx++] = inst;
}

void DaemonReload(DaemonInstance *instance)
{
    if (!instance)
        return;

    if (!instance->is_online)
    {
        instance->is_online = 1;
        BSPLOG(&g_daemon_log, LOG_LEVEL_INFO, "Module 0x%08X back ONLINE", (uint32_t)(uintptr_t)instance->owner_id);
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
    buzzer_flag = 0; // 刷新，避免上一轮循环影响
    uint8_t all_daemon_is_online_temp = 1;

    for (size_t i = 0; i < s_idx; ++i)
    {
        dins = s_daemon_instances[i];
        /* reload_count=0 表示禁用（不监控）：跳过离线判定与故障动作，等效恒在线。
         * 使未 DaemonConfig 配过（或配 0）的模块不会一开机就被判离线 / 拉低全局在线标志 */
        if (dins->reload_count == 0)
            continue;
        if (dins->temp_count > 0)
        {
            dins->temp_count--;
            if (dins->temp_count == 0)
            {
                dins->is_online = 0;
                BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Module 0x%08X OFFLINE", (uint32_t)(uintptr_t)dins->owner_id);
            }
        }
        else
        {
            all_daemon_is_online_temp = 0; // 只要有一个掉线就标志为0
            switch (dins->fault_action)
            {
            case DAEMON_FAULT_BUZZER_SHORT:
                buzzer_flag = 1;
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
            {
                // 每次检查都调用回调（持续掉线状态）
                dins->callback(dins->owner_id);
            }
        }
    }
    if (0 == all_daemon_is_online_temp)
    {
        all_daemon_is_online = 0;
    }
    else if (1 == all_daemon_is_online_temp)
    {
        all_daemon_is_online = 1;
    }

    PWMSetDutyRatio(&buzzer_pwm, (buzzer_flag / 2.0f));
}

/*==================== RTOS 任务 ====================*/

ITCM_RAM static void DaemonTaskFunc(void *argument)
{
    static uint64_t start;
    static uint64_t dt;
    TickType_t xLastWakeTime = xTaskGetTickCount();           // 周期锚点(绝对唤醒时刻)
    const TickType_t xPeriod = pdMS_TO_TICKS(DAEMON_FREQ_MS); // 任务周期(tick)
    BSPLOG(&g_daemon_log, LOG_LEVEL_INFO, "DAEMON Task Start");
    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xPeriod); // 固定周期唤醒，避免 vTaskDelay 的周期漂移
        start = DWT_GetTimeUs();
        DaemonTask();
        dt = DWT_GetTimeUs() - start;
        if (dt > 1000 * DAEMON_FREQ_MS)
            BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "DAEMON Task is being DELAY! dt = %llu(us)", dt);
    }
}

void DaemonInit(void)
{

    Task_Init_Config_s task_cfg = {
        .func = DaemonTaskFunc,
        .priority = DAEMON_TASK_PRIORITY,
    };
    TaskRegister(&daemon_task, &task_cfg);

#if (DEVELOPMENT_BOARD == DM_MC02) || (DEVELOPMENT_BOARD == DJI_C) || (DEVELOPMENT_BOARD == DJI_A)
    PWMRegister(&buzzer_pwm);
    PWM_Config_s pwm_cfg = {.tim_e = TIM_BUZZER};
    PWMConfig(&buzzer_pwm, &pwm_cfg);
#else
#error "without config buzzer"
#endif // #if DEVELOPMENT_BOARD
}

#else

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config)
{
    (void)inst;
    (void)config;
}

void DaemonRegister(DaemonInstance *inst)
{
    if (!inst)
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
