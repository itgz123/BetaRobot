/**
 * @file drv_dbus.c
 * @brief DBUS 遥控器驱动实现
 *
 * @note DRV 层职责：
 *       1. BSP 回调中解析 DBUS 数据并传递到 APP 层
 *       2. 不使用 FreeRTOS（队列由 APP 层管理）
 *
 * @note 协议参考 RoboMaster 遥控器（接收机）用户手册"字节控制帧结构"：
 *       帧长 18 字节，ch0~ch3 各 11bit、S1/S2 各 2bit、鼠标 3×16bit、
 *       鼠标左右键各 8bit、键盘 16bit、保留(拨轮) 16bit。
 */

#include "drv_dbus.h"
#include "app_cfg.h"

#ifdef DRV_DBUS_USED

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_log.h"
#include "bsp_dwt.h"

// 函数声明
static void DBUSUARTRxCallback(USARTInstance *usart_inst);
static void DBUSUARTErrCallback(USARTInstance *usart_inst, USART_ErrReason_e reason);
static void DBUSUARTDaemonCallback(void *owner);

/* 接收停摆后自动重启的重试周期（ms）：离线期间 DaemonTask 每 1ms 都会调一次 daemon 回调，
 * 全靠这个周期限频。取值要小于对端判定"遥控器失联"的时间（lost_timeout_ms），
 * 才能在一次失联超时之内把链路拉回来；太小则重启失败时会刷屏日志。 */
#ifndef DRV_DBUS_RX_RESTART_PERIOD_MS
#define DRV_DBUS_RX_RESTART_PERIOD_MS 20
#endif

/* daemon_reload 配 0 时的兜底值（单位：毫秒，即 100ms）：
 * 接收停摆的自恢复搭在 daemon 回调上（见 DBUSUARTDaemonCallback），而 0 的本义是
 * "禁用监控"——DaemonTask 会整个跳过本实例，等于把自恢复一起禁掉。故 Config 把 0 提升为该值。
 * 该值同时是"多久没收到帧判离线"的阈值（离线日志 / fault_action 也按它触发）。 */
#ifndef DRV_DBUS_DAEMON_RELOAD_DEFAULT
#define DRV_DBUS_DAEMON_RELOAD_DEFAULT 100
#endif
/* 兜底值自身不能再是 0 —— 否则"把 0 提升为兜底值"等于没提升，自恢复又会静默失效 */
_Static_assert(DRV_DBUS_DAEMON_RELOAD_DEFAULT != 0,
               "DRV_DBUS_DAEMON_RELOAD_DEFAULT must be non-zero (0 would silently disable RX self-heal)");

#ifndef DRV_DBUS_LOG_LIMIT
#define DRV_DBUS_LOG_LIMIT 10
#endif                                                        // !DRV_DBUS_LOG_LIMIT
LOG_INSTANCE_DEF(g_dbus_log, "drv_dbus", DRV_DBUS_LOG_LIMIT); // DBUS 日志实例

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册 DBUS 实例（仅调用一次）
 */
int8_t DBUSRegister(DBUSInstance *instance)
{

    if (instance == NULL)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "Instance is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "usart_inst is NULL!");
        return -1;
    }

    // 注册 BSP 层 USART 实例（USARTRegister 自身有防重复检查）
    if (USARTRegister(instance->usart_inst) != 0)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "USART register failed!");
        return -1;
    }

    // 注册 daemon（占位，Config 更新运行参数）
    if (instance->daemon)
    {
        DaemonRegister(instance->daemon);
    }

    return 0;
}

/**
 * @brief 配置 DBUS 实例（可重复调用）
 */
int8_t DBUSConfig(DBUSInstance *instance, const DBUS_Config_s *config)
{
    if (instance == NULL)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "Instance is NULL!");
        return -1;
    }

    if (config == NULL)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "Config is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "usart_inst is NULL!");
        return -1;
    }

    // 配置 USART（parent 经 Config 写入，BSP 回调据此取回 DRV 实例）
    USART_Config_s usart_cfg = {
        .uart_e = config->uart_e,
        .parent = instance,
        .rx_callback = DBUSUARTRxCallback,
        .tx_callback = NULL,
        .err_callback = DBUSUARTErrCallback,
    };
    if (USARTConfig(instance->usart_inst, &usart_cfg) != BSP_OK)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "USART config failed!");
        return -1;
    }

    // 更新 daemon 运行参数（可重入）
    if (instance->daemon)
    {
        uint16_t daemon_reload = config->daemon_reload;

        // 0 = 禁用监控，但接收停摆自恢复搭在 daemon 回调上（见 DBUSUARTDaemonCallback），
        // 禁用等于自恢复失效，故提升为默认值，保证这条通道始终可用
        if (daemon_reload == 0)
        {
            daemon_reload = DRV_DBUS_DAEMON_RELOAD_DEFAULT;
            BSPLOG(&g_dbus_log, LOG_LEVEL_WARNING, "daemon_reload=0 disables RX self-heal, forced to %u",
                   daemon_reload);
        }

        Daemon_Config_s daemon_cfg = {
            .reload_count = daemon_reload,
            .fault_action = config->daemon_fault,
            .callback = DBUSUARTDaemonCallback, // 接收停摆后的任务上下文重启入口
            .owner_id = instance,
        };
        DaemonConfig(instance->daemon, &daemon_cfg);
    }

    // 初始化信号丢失超时
    instance->lost_timeout_us = (uint64_t)config->lost_timeout_ms * 1000;
    instance->lost_start_time_us = 0;
    instance->signal_lost = 0;

    /* 启动接收常开流（Config 不再自动启动接收）——放在最末：上面的 daemon 与超时参数是
     * 启动失败后唯一的自救通道（DBUSUARTDaemonCallback 按 rx_armed==0 重启接收），
     * 若排在启动之前就 return，失败时既没监控也没超时基准（lost_timeout_us==0 会让
     * 帧丢失判据恒真/恒假），等于把链路彻底放任。 */
    if (USARTReceive(instance->usart_inst, instance->usart_inst->rx_buff_size, BSP_DMA_MODE, 0) != BSP_OK)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_ERROR, "USART receive start failed! (daemon will retry)");
        return -1;
    }

    return 0;
}

static DBUS_Data_t DBUSDecodeFrame(const uint8_t *data, uint16_t len)
{
    DBUS_Data_t result = {0};

    /* 拒绝本帧的唯一出口：此时 result 是零值帧，只有 frame_lost 有效。
     * 调用方（DBUSUARTRxCallback）据它不刷新通道数据、不喂狗、并启动失控计时。 */
    if (data == NULL || len < DBUS_FRAME_SIZE)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_WARNING, "Invalid frame data, len=%d (expected %d)", len, DBUS_FRAME_SIZE);
        result.frame_lost = 1;
        return result;
    }

    // 解析 4 个摇杆通道（每通道 11 位）并归一化到 -1.0 ~ 1.0
    // 归一化公式：(raw - center) / (max - center)
    // 注意 ch2 涉及 data[4]<<10，中间量需用 uint32_t 防止 int 提升溢出
    static const float scale = 1.0f / (float)(DBUS_CH_MAX - DBUS_CH_CENTER);

    uint32_t raw_ch[DBUS_CHANNEL_COUNT];
    raw_ch[0] = (uint32_t)(data[0] | (data[1] << 8)) & 0x07FFu;                          //!< 位偏移 0
    raw_ch[1] = (uint32_t)((data[1] >> 3) | (data[2] << 5)) & 0x07FFu;                   //!< 位偏移 11
    raw_ch[2] = (uint32_t)((data[2] >> 6) | (data[3] << 2) | (data[4] << 10)) & 0x07FFu; //!< 位偏移 22
    raw_ch[3] = (uint32_t)((data[4] >> 1) | (data[5] << 7)) & 0x07FFu;                   //!< 位偏移 33

    for (uint8_t i = 0; i < DBUS_CHANNEL_COUNT; i++)
    {
        result.ch[i] = (float)((int32_t)raw_ch[i] - DBUS_CH_CENTER) * scale;
    }

    // 开关 S1/S2（字节 5 的高 4 位，位偏移 44/46）
    result.s2 = (DBUS_SW_e)((data[5] >> 4) & 0x03);        //!< S2 低位 (偏移 44)
    result.s1 = (DBUS_SW_e)(((data[5] >> 4) & 0x0C) >> 2); //!< S1 高位 (偏移 46)

    // 鼠标解析（16 位有符号，位偏移 48/64/80）
    result.mouse_x = (int16_t)(data[6] | (data[7] << 8));
    result.mouse_y = (int16_t)(data[8] | (data[9] << 8));
    result.mouse_z = (int16_t)(data[10] | (data[11] << 8));
    result.mouse_press_l = data[12]; //!< 位偏移 96
    result.mouse_press_r = data[13]; //!< 位偏移 104

    // 键盘解析（16 位位域，位偏移 112，W/S/A/D/Q/E/Shift/Ctrl 在低 8 位）
    result.key.value = (uint16_t)(data[14] | (data[15] << 8));

    // 侧边拨轮（保留字段，位偏移 128，取 11 位，中心 1024，归一化）
    result.dial = (float)((int32_t)((uint16_t)(data[16] | (data[17] << 8)) & 0x07FFu) - DBUS_CH_CENTER) * scale;

    return result;
}

/*------------- 私有函数实现 --------------*/

/**
 * @brief BSP 层 UART 错误回调（ISR 或任务上下文）
 * @param usart_inst USART 实例指针
 *
 * @note 失联保护必须能覆盖"完全没有数据流"的情形，否则等于没有：
 *       帧丢失判据写在 DBUSUARTRxCallback 里，而 RX 一旦停摆（bsp 续收最终失败）
 *       该回调就再也不会被调用 —— `signal_lost` 会永远冻结在上一次的取值，
 *       通常是 0（正常），**失控保护就此静默失效**，操作手失去控制权而系统不知情。
 *
 * @note 只对"接收已停摆"（`rx_armed == 0`，即续收最终失败、DMA 已停）做 fail-safe。
 *       偶发硬件错误（ORE/FE/NE）bsp 会自行续收，此时 `rx_armed` 仍为 1、数据流很快恢复；
 *       那种情况若也置 `signal_lost`，会因一次总线噪声就触发失控保护，属过激反应 ——
 *       这类错误只由 bsp 的 `s_usart_status[].err_*` 计数供观察，不在这里动作。
 *       （bsp 保证 err_callback 触发时 `rx_armed` 已反映本次续收结果。）
 *
 * @note 停摆是不可自愈的（HAL 侧残留状态不会自己清），但本回调可能跑在 ISR 里，
 *       不能在这里调 USARTReceive —— 真正的重启交给 DBUSUARTDaemonCallback（任务上下文）。
 *
 * @note 只认接收侧的原因：`USART_ERR_TX_ABORT` 是本实例的**发送**被中止，与遥控器失联无关。
 *       本驱动根本不发（tx_callback 为 NULL），但 bsp 的发送卡死自恢复是按 UART 实例
 *       触发的，误当失联会让失控保护凭空动作。
 */
static void DBUSUARTErrCallback(USARTInstance *usart_inst, USART_ErrReason_e reason)
{
    DBUSInstance *dbus_inst;

    if (usart_inst == NULL || reason == USART_ERR_TX_ABORT)
    {
        return;
    }

    /* 判据是"接收已停摆"（rx_armed == 0）：偶发硬件错误（ORE/FE/NE）bsp 会自行续收，
     * 此时 rx_armed 仍为 1、数据流很快恢复，不该触发失控保护（见上方长注释） */
    if (usart_inst->rx_armed)
    {
        return;
    }

    dbus_inst = (DBUSInstance *)usart_inst->parent;
    if (dbus_inst == NULL)
    {
        return;
    }

    dbus_inst->signal_lost = 1;
    dbus_inst->lost_start_time_us = 0;
}

/**
 * @brief 失控保护的确认计时（帧被拒 / 链路静默两条入口共用）
 * @param dbus_inst DBUS 实例
 *
 * @note 判据是"持续无有效帧达到 lost_timeout_us"：lost_start_time_us 是这段异常窗口的起点
 *       （0 = 窗口未开），由**第一个**异常事件打点，之后的调用只做超时比较。
 * @note 为什么必须有"静默"入口：遥控器关机/接收机断线时根本不会有帧进来，rx_callback
 *       一次都不会被调用 —— 只在 rx_callback 里计时，`signal_lost` 会永远冻结在 0，
 *       失控保护静默失效（这正是 DBUSUARTErrCallback 顶部那段长注释说的事，只是那一路
 *       只覆盖"接收停摆"，覆盖不到"接收正常但对面不说话了"）。静默由 daemon 探知
 *       （没喂狗 = 超时没收到有效帧），故本函数在 daemon 离线回调里也被调用。
 *       daemon 的 reload_count（默认 100ms）是本函数的触发时基，lost_timeout_us（默认
 *       1000ms）是确认窗口，两者量级不同、互不替代：前者要快（早发现早重启接收），
 *       后者要稳（避免一次抖动就判定失控）。
 * @note 本函数有**两个上下文**在跑同一实例（rx_callback 在中断里、daemon 离线回调在任务里，
 *       见上面两段），而 lost_start_time_us 是 64 位：Cortex-M 上"读—比较—写"不是一条指令，
 *       任务侧写一半被中断抢走时，中断侧可能读到半新半旧的巨大值，`now - lost_start_time` 随
 *       即溢出成"已超时"，凭空置一次 signal_lost（失控保护误动作，代价是停车）。
 *       故整段用临界区保护：只有 5 条指令、内部不调用任何函数。
 *       （`signal_lost` 本身是单字节标志，且只在"确认失控"与"收到有效帧"两处写，不需要保护。） */
static void DBUS_CheckLostTimeout(DBUSInstance *dbus_inst)
{
    uint64_t now_us = DWT_GetTimeUs();
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (dbus_inst->lost_start_time_us == 0)
    {
        dbus_inst->lost_start_time_us = now_us;
    }

    /* lost_timeout_ms 配 0 = 立即标志（见 DBUS_Config_s）：首帧异常即满足 now - now >= 0 */
    if ((now_us - dbus_inst->lost_start_time_us) >= dbus_inst->lost_timeout_us)
    {
        dbus_inst->signal_lost = 1;
    }
    if (primask == 0U)
        __enable_irq();
}

/**
 * @brief daemon 离线回调（DaemonTask 任务上下文，1ms 一次）——接收停摆的自动重启
 * @param owner 所属模块实例（DBUSConfig 写入的 Daemon_Config_s.owner_id）
 *
 * @note 为什么用 daemon 回调做重启入口：接收一旦停摆，rx_callback 不再触发，链路再无任何
 *       周期性事件可挂 —— 而 daemon 恰好是"没喂狗（没收到帧）"才被触发的，且运行在
 *       DaemonTask（任务上下文，1ms），是唯一现成的、与接收无关的任务上下文时基。
 *       bsp 明确禁止在 ISR 里重启接收（Abort 要自旋等 tick），本回调正合适。
 *
 * @note 判据（`rx_armed == 0` 才是真停摆，遥控器没开机不算）与限频都收在 bsp 的
 *       USARTRecoverRxIfStalled 里，四条 UART 链路共用同一份实现；这里只给限频周期。
 *       依赖 daemon 已启用：daemon_reload 配 0 会被 DBUSConfig 提升为
 *       DRV_DBUS_DAEMON_RELOAD_DEFAULT，故本通道不会被"禁用监控"误关。
 *
 * @note 本回调在"离线持续期间每 1ms 都被调用"（见 DaemonTask），故它同时是失控保护在
 *       **完全静默**（遥控器关机、接收机掉线）下的唯一计时时基：那种情况没有任何帧、
 *       连坏帧都没有，只有这里能发现"已经这么久没有有效帧了"。
 */
static void DBUSUARTDaemonCallback(void *owner)
{
    DBUSInstance *dbus_inst = (DBUSInstance *)owner;

    if (dbus_inst == NULL || dbus_inst->usart_inst == NULL)
    {
        return;
    }

    /* 静默路径的失控确认：没喂狗 = 超过 daemon_reload 没收到有效帧 */
    DBUS_CheckLostTimeout(dbus_inst);

    /* 接收在跑 / 未到限频周期 → BSP_BUSY，空转；只有真重启成功才打日志 */
    if (USARTRecoverRxIfStalled(dbus_inst->usart_inst, DRV_DBUS_RX_RESTART_PERIOD_MS) == BSP_OK)
    {
        BSPLOG(&g_dbus_log, LOG_LEVEL_WARNING, "RX stalled, receive restarted");
    }
}

/**
 * @brief BSP 层 UART 接收回调
 * @param usart_inst USART 实例指针
 * @note 通过 parent 字段获取 DBUSInstance，调用 APP 回调
 *
 * @note 与 SBUS 的关键差别：DBUS 帧内**没有** frame_lost / failsafe 位（那是 SBUS 的
 *       flags 字节），接收机也不会替我们标"这一帧不可信"，所以本驱动的失控判据只能是
 *       "有没有按期收到可解析的 18 字节帧"，靠计时而不是靠帧内容（见 DBUS_CheckLostTimeout）。
 *       帧被拒（长度/参数不合法）算"没收到"：既不喂狗，也不覆盖上一份有效通道数据。
 */
static void DBUSUARTRxCallback(USARTInstance *usart_inst)
{
    DBUSInstance *dbus_inst;
    DBUS_Data_t frame;

    if (usart_inst == NULL)
    {
        return;
    }

    dbus_inst = (DBUSInstance *)usart_inst->parent;
    if (dbus_inst == NULL)
    {
        return;
    }

    /* 在中断上下文中解析原始数据为通道数据（长度不合法时 decode 内部报错并返回
     * frame_lost=1 的零值帧，故调用方必须按 frame_lost 分流） */
    frame = DBUSDecodeFrame(usart_inst->rx_buff, usart_inst->rx_len);

    if (frame.frame_lost)
    {
        /* 本帧不可信：不喂狗（daemon 会因此判离线，静默路径的失控计时随即启动），
         * 也不把零值写进 dbus_data —— 覆盖掉的话，失控保护触发之前的那段时间里
         * 上层会看到杆位全部为 0（归一化后是 -1.0 而不是中位），比"保持上一帧"更危险。 */
        dbus_inst->dbus_data.frame_lost = 1;
        DBUS_CheckLostTimeout(dbus_inst);
        return;
    }

    dbus_inst->dbus_data = frame;    /* 有效帧：整份覆盖（帧内无失控位，flags 恒 0） */
    DaemonReload(dbus_inst->daemon); /* 只有可解析的有效帧才证明对端在线 */

    // 信号恢复正常：清除计时和丢失标志
    dbus_inst->lost_start_time_us = 0;
    dbus_inst->signal_lost = 0;
}

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* DRV_DBUS_USED */
