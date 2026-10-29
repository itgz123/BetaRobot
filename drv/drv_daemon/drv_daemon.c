#include "drv_daemon.h"

#ifdef DAEMON_USED

#include "bsp_log.h"
#include "bsp_dwt.h"

// 用于保存所有的daemon instance
static DaemonInstance *s_daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t s_idx = 0;
#ifndef DRV_DAEMON_LOG_LIMIT
#define DRV_DAEMON_LOG_LIMIT 10
#endif                                                              // !DRV_DAEMON_LOG_LIMIT
LOG_INSTANCE_DEF(g_daemon_log, "drv_daemon", DRV_DAEMON_LOG_LIMIT); // Daemon 日志实例

/* 故障动作表：由 app 在 DaemonInit 时登记，本模块只认函数指针，不认识具体设备 */
static const Daemon_ActionTable_s *s_actions = NULL;
// 调试用聚合标志：给人 Watch 的"全体在线"快照，本文件只写不读。
// 代码里要判某个模块在不在线，请查它自己的 is_online
static uint8_t all_daemon_is_online = 1;

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config)
{
    if (!inst || !config)
        return;

    if (config->fault_action >= DAEMON_FAULT_NUM)
    {
        BSPLOG(&g_daemon_log, LOG_LEVEL_ERROR, "Invalid fault_action: %d, max: %d", (int)config->fault_action,
               (int)DAEMON_FAULT_BUZZER_EMERGENCY);
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
    uint8_t all_daemon_is_online_temp = 1;
    /* 本拍统一取一次时刻：下面按"距上次喂狗的实耗毫秒数"判离线，而不是按本函数被调用的次数。
     * 原因：调用方是 app 的周期任务，旧版 FreeRTOS 的 vTaskDelayUntil 落后时不会跳到当前时刻，
     * 而是背靠背补拍；加上离线回调本身可能耗时（如 USART 残留态的 Abort 要自旋等 tick），
     * 一次跨周期就会让所有实例的计数被连扣若干拍 —— 刚刚喂过狗的 CAN 云台链路（reload=10）、
     * BMI088（reload=20）会被瞬时误判离线，后果是无中生有的停车/失能。按实耗时间判离线后，
     * 补拍期间 elapsed≈0，不产生任何误判，`reload_count` 也回归它文档里的本义：毫秒。 */
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
        /* 喂狗时间戳可能"晚于"本拍的 now_us：now_us 只在本函数开头取一次，而喂狗
         * （DaemonReload）会在**任意高优先级上下文**里插进来——SBUS/DBUS/comm 的喂狗都在
         * UART/CAN 接收 ISR 内，而 ISR 无条件抢占本任务（优先级 0 = 最低）。若某次喂狗正好
         * 落在"取完 now_us"与"读 last_reload_us"之间，last_reload_us 就比 now_us 更晚，
         * 无符号相减**下溢**成 ≈2^64 → 被当成"远超超时"→ 凭空置 is_online=0、打离线日志、
         * 每拍触发故障动作/离线回调；直到下一帧喂狗（DaemonReload）把它置回 1 才结束。
         * 表现是按链路帧周期（SBUS ~8ms）周期性假离线，而链路诊断量（帧计数/丢帧标志）全正常。
         * 这里把"喂狗晚于快照"判为"刚喂过"（实耗记 0，其余算法不变）：真实离线要求
         * last_reload_us 确实落后 ≥timeout_us，故此判据只堵下溢，不会漏报。 */
        uint64_t last_reload_us = dins->last_reload_us;
        uint64_t elapsed_us = (last_reload_us > now_us) ? 0u : (now_us - last_reload_us);
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
            /* 离线期间每拍都调：动作实现需自身幂等（如 BuzzerPlay(HIGH) 同序列正在播则忽略），
             * 于是"持续掉线"自然表现为报警循环，无需 daemon 记额外的边沿状态 */
            if (s_actions != NULL && (uint8_t)dins->fault_action < (uint8_t)DAEMON_FAULT_NUM)
            {
                daemon_action_fn action = s_actions->action[dins->fault_action];
                if (action != NULL)
                    action();
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
}

/*==================== 初始化 ====================*/

void DaemonInit(const Daemon_ActionTable_s *table)
{
    /* 本模块不创建任务：周期调用 DaemonTask() 的任务由 app 注册（drv 层不依赖 FreeRTOS）。
     * 动作表只存指针：它必须由 app 以静态存储期提供（static const Daemon_ActionTable_s）。 */
    s_actions = table;
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

void DaemonInit(const Daemon_ActionTable_s *table)
{
    (void)table;
}

#endif // DAEMON_USED
