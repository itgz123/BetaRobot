#include "drv_daemon.h"

#ifdef DAEMON_USED

#include "bsp_log.h"
#include "bsp_freertos.h"
#include "bsp_dwt.h"
#include "bsp_tim.h"

// 用于保存所有的daemon instance
static DaemonInstance *s_daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t s_idx = 0;
#ifndef DRV_DAEMON_LOG_LIMIT
#define DRV_DAEMON_LOG_LIMIT 10
#endif                                                              // !DRV_DAEMON_LOG_LIMIT
LOG_INSTANCE_DEF(g_daemon_log, "drv_daemon", DRV_DAEMON_LOG_LIMIT); // Daemon 日志实例

// 蜂鸣器鸣叫声音表格（预留：当前全 0、无代码读取。要按 fault 类型播不同节奏时填这里 ——
// 现在只实现了 DAEMON_FAULT_BUZZER_SHORT，靠下面的 buzzer_flag + PWM 占空比）
const uint8_t voice_map[DAEMON_FAULT_NUM][12] = {0};
static uint8_t buzzer_flag = 0; // TODO:这个之后用位域实现
// 调试用聚合标志：给人 Watch 的"全体在线"快照，本文件只写不读。
// 代码里要判某个模块在不在线，请查它自己的 is_online
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
        BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Invalid fault_action: %d, max: %d", config->fault_action,
               DAEMON_FAULT_RESERVED_7);
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
    if (instance == NULL)
        return 0;

    /* reload_count == 0 = 未启用监控（DaemonTask 整个跳过该实例，见 DaemonTask）：
     * 没有任何判据可用，按"禁用（不监控，等效恒在线）"的本义返回在线。
     * 不能返回 0：那等于把"没监控"报成"已掉线"，而调用方普遍用它拦控制
     * （如 app_chassis 的 GimbalCommIsOnline），会得到"配 0 反而永久停车"的反直觉结果。 */
    if (instance->reload_count == 0)
        return 1;

    return (uint8_t)(instance->temp_count > 0);
}

void DaemonTask(void)
{
    DaemonInstance *dins;
    buzzer_flag = 0; // 刷新，避免上一轮循环影响
    uint8_t all_daemon_is_online_temp = 1;
    /* 本拍统一取一次时刻：下面按"距上次喂狗的实耗毫秒数"判离线，而不是按本函数被调用的次数。
     * 原因：本工程 FreeRTOS 较老，vTaskDelayUntil 落后时不会跳到当前时刻，而是背靠背补拍
     * （见 DaemonTaskFunc 的 "is being DELAY!" 日志），加上离线回调本身可能耗时（如 USART
     * 残留态的 Abort 要自旋等 tick），一次跨周期就会让所有实例的计数被连扣若干拍 —— 刚刚
     * 喂过狗的 CAN 云台链路（reload=10）、BMI088（reload=20）会被瞬时误判离线，
     * 后果是无中生有的停车/失能。按实耗时间扣拍后，补拍期间 elapsed≈0，不产生任何扣减，
     * `reload_count` 也回归它文档里的本义：毫秒。 */
    uint64_t now_us = DWT_GetTimeUs();

    for (size_t i = 0; i < s_idx; ++i)
    {
        dins = s_daemon_instances[i];
        /* reload_count=0 表示禁用（不监控）：跳过离线判定与故障动作，等效恒在线。
         * 使未 DaemonConfig 配过（或配 0）的模块不会一开机就被判离线 / 拉低全局在线标志 */
        if (dins->reload_count == 0)
            continue;
        /* 距上次喂狗（DaemonReload / DaemonConfig 都记时间戳）的实耗时间。
         * 两个量都在 µs 域比较（差值不会因转毫秒丢精度），除法只在剩余量换算时做，
         * 被除数 < timeout_us ≤ 65535ms，结果的量级不会撑破 uint16_t（不会溢出） */
        uint64_t elapsed_us = now_us - dins->last_reload_us;
        uint64_t timeout_us = (uint64_t)dins->reload_count * 1000u;

        if (elapsed_us < timeout_us)
        {
            /* temp_count 保留为"剩余毫秒数"的实时快照（DaemonIsOnline 判的就是它 > 0），
             * 由实耗时间换算而来，不再自己递减。
             * 换算必须**向上取整**：向下取整会把"剩余不足 1ms"这一段截断成 0，于是
             * DaemonIsOnline 会比下面的离线判定（elapsed >= timeout）早最多 1ms 报离线——
             * 同一个阈值出现两套结论（如 app 层据此拦控制，会凭空停车 1ms）。
             * 取整后二者严格等价：temp_count > 0 ⟺ elapsed_us < timeout_us。 */
            dins->temp_count = (uint16_t)((timeout_us - elapsed_us + 999u) / 1000u);
        }
        else
        {
            dins->temp_count = 0;
            if (dins->is_online)
            {
                dins->is_online = 0;
                BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Module 0x%08X OFFLINE", (uint32_t)(uintptr_t)dins->owner_id);
            }
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
    /* 本编译配置下没有 daemon 在监控，与"reload_count == 0 = 不监控 = 等效恒在线"
     * 保持同一结论：返回 0 会让调用方把"没启用监控"当成"已掉线"而永久拦住控制 */
    return 1;
}

void DaemonTask(void)
{
}

void DaemonInit(void)
{
}

#endif // DAEMON_USED
