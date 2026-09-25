/**
 * @file drv_sbus.c
 * @brief SBUS 遥控器驱动实现
 *
 * @note DRV 层职责：
 *       1. BSP 回调中解析 SBUS 数据并传递到 APP 层
 *       2. 不使用 FreeRTOS（队列由 APP 层管理）
 */

#include "drv_sbus.h"
#include "app_cfg.h"

#ifdef DRV_SBUS_USED

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_log.h"
#include "bsp_dwt.h"

// 函数声明
static void SBUSUARTRxCallback(USARTInstance *usart_inst);
static void SBUSUARTErrCallback(USARTInstance *usart_inst, USART_ErrReason_e reason);
static void SBUSUARTDaemonCallback(void *owner);

/* 接收停摆后自动重启的重试周期（ms）：离线期间 DaemonTask 每 1ms 都会调一次 daemon 回调，
 * 全靠这个周期限频。取值要小于对端判定"遥控器失联"的时间（lost_timeout_ms），
 * 才能在一次失联超时之内把链路拉回来；太小则重启失败时会刷屏日志。 */
#ifndef DRV_SBUS_RX_RESTART_PERIOD_MS
#define DRV_SBUS_RX_RESTART_PERIOD_MS 20
#endif

/* daemon_reload 配 0 时的兜底值（单位：毫秒，即 100ms）：
 * 接收停摆的自恢复搭在 daemon 回调上（见 SBUSUARTDaemonCallback），而 0 的本义是
 * "禁用监控"——DaemonTask 会整个跳过本实例，等于把自恢复一起禁掉。故 Config 把 0 提升为该值。
 * 该值同时是"多久没收到帧判离线"的阈值（离线日志 / fault_action 也按它触发）。 */
#ifndef DRV_SBUS_DAEMON_RELOAD_DEFAULT
#define DRV_SBUS_DAEMON_RELOAD_DEFAULT 100
#endif
/* 兜底值自身不能再是 0 —— 否则"把 0 提升为兜底值"等于没提升，自恢复又会静默失效 */
_Static_assert(DRV_SBUS_DAEMON_RELOAD_DEFAULT != 0,
               "DRV_SBUS_DAEMON_RELOAD_DEFAULT must be non-zero (0 would silently disable RX self-heal)");

#ifndef DRV_SBUS_LOG_LIMIT
#define DRV_SBUS_LOG_LIMIT 10
#endif                                                        // !DRV_SBUS_LOG_LIMIT
LOG_INSTANCE_DEF(g_sbus_log, "drv_sbus", DRV_SBUS_LOG_LIMIT); // SBUS 日志实例

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册 SBUS 实例（仅调用一次）
 */
int8_t SBUSRegister(SBUSInstance *instance)
{

    if (instance == NULL)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "Instance is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "usart_inst is NULL!");
        return -1;
    }

    // 注册 BSP 层 USART 实例（USARTRegister 自身有防重复检查）
    if (USARTRegister(instance->usart_inst) != 0)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "USART register failed!");
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
 * @brief 配置 SBUS 实例（可重复调用）
 */
int8_t SBUSConfig(SBUSInstance *instance, const SBUS_Config_s *config)
{
    if (instance == NULL)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "Instance is NULL!");
        return -1;
    }

    if (config == NULL)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "Config is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "usart_inst is NULL!");
        return -1;
    }

    // 校验通道范围（校准值）
    const SBUS_ChRange_s *range = &config->ch_range;
    if ((range->ch_min >= range->ch_center) || (range->ch_center >= range->ch_max))
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "Invalid ch_range: min=%u center=%u max=%u", range->ch_min,
               range->ch_center, range->ch_max);
        return -1;
    }

    // 配置 USART（parent 经 Config 写入，BSP 回调据此取回 DRV 实例）
    USART_Config_s usart_cfg = {
        .uart_e = config->uart_e,
        .parent = instance,
        .rx_callback = SBUSUARTRxCallback,
        .tx_callback = NULL,
        .err_callback = SBUSUARTErrCallback,
    };
    if (USARTConfig(instance->usart_inst, &usart_cfg) != BSP_OK)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "USART config failed!");
        return -1;
    }

    // 更新 daemon 运行参数（可重入）
    if (instance->daemon)
    {
        uint16_t daemon_reload = config->daemon_reload;

        // 0 = 禁用监控，但接收停摆自恢复搭在 daemon 回调上（见 SBUSUARTDaemonCallback），
        // 禁用等于自恢复失效，故提升为默认值，保证这条通道始终可用
        if (daemon_reload == 0)
        {
            daemon_reload = DRV_SBUS_DAEMON_RELOAD_DEFAULT;
            BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "daemon_reload=0 disables RX self-heal, forced to %u",
                   daemon_reload);
        }

        Daemon_Config_s daemon_cfg = {
            .reload_count = daemon_reload,
            .fault_action = config->daemon_fault,
            .callback = SBUSUARTDaemonCallback, // 接收停摆后的任务上下文重启入口
            .owner_id = instance,
        };
        DaemonConfig(instance->daemon, &daemon_cfg);
    }

    // 初始化信号丢失超时
    instance->lost_timeout_us = (uint64_t)config->lost_timeout_ms * 1000;
    instance->lost_start_time_us = 0;
    instance->signal_lost = 0;

    // 保存通道范围并预计算归一化系数（中断中只做乘加，不做除法）
    instance->ch_range = *range;
    instance->scale_pos = 1.0f / (float)(range->ch_max - range->ch_center);
    instance->scale_neg = 1.0f / (float)(range->ch_center - range->ch_min);

    /* 启动接收常开流（Config 不再自动启动接收）——放在最末：上面的 daemon 与超时参数是
     * 启动失败后唯一的自救通道（SBUSUARTDaemonCallback 按 rx_armed==0 重启接收），
     * 若排在启动之前就 return，失败时既没监控也没超时基准（lost_timeout_us==0 会让
     * 帧丢失判据恒真/恒假），等于把链路彻底放任。 */
    if (USARTReceive(instance->usart_inst, instance->usart_inst->rx_buff_size, BSP_DMA_MODE, 0) != BSP_OK)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_ERROR, "USART receive start failed! (daemon will retry)");
        return -1;
    }

    return 0;
}

/**
 * @brief 解析 SBUS 原始帧
 * @param inst SBUS 实例指针（提供通道校准范围与归一化系数）
 * @param data 原始帧数据
 * @param len  数据长度
 * @return 解析后的通道数据
 */
static SBUS_Data_t SBUSDecodeFrame(const SBUSInstance *inst, const uint8_t *data, uint16_t len)
{
    SBUS_Data_t result = {0};

    // 参数检查
    if (data == NULL || len < SBUS_FRAME_SIZE)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "Invalid frame data, len=%d", len);
        result.frame_lost = 1;
        return result;
    }

    // 使用联合体映射原始帧，支持位域访问
    const SBUS_RawFrame_u *frame = (const SBUS_RawFrame_u *)data;

    // 帧头检查
    if (frame->frame.header != SBUS_HEADER)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "Invalid frame header: 0x%02X", frame->frame.header);
        result.frame_lost = 1;
        return result;
    }

    // 帧尾检查
    if (frame->frame.footer != SBUS_FOOTER && frame->frame.footer != SBUS_FOOTER_FRAME_LOST &&
        frame->frame.footer != SBUS_FOOTER_FAILSAFE)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "Invalid frame footer: 0x%02X", frame->frame.footer);
        result.frame_lost = 1;
        return result;
    }

    // 解析 16 个模拟通道（每通道 11 位）并归一化到 -1.0 ~ 1.0
    // 分段归一化：raw >= center 用正向跨度 (max-center)，raw < center 用负向跨度 (center-min)
    // 系数已在 SBUSConfig 中预计算；超出 min/max 的原始值限幅到 ±1.0
    const int32_t center = (int32_t)inst->ch_range.ch_center;
    const float scale_pos = inst->scale_pos;
    const float scale_neg = inst->scale_neg;

    for (uint8_t i = 0; i < SBUS_CHANNEL_COUNT; i++)
    {
        // 从 SBUS 原始帧提取第 i 个通道的 11 位原始值 (0-2047)
        uint16_t bit_off = (uint16_t)i * 11; // 从 ch_data 起始的位偏移
        uint8_t byte_off = bit_off / 8;      // 从 raw[1] 起的字节偏移
        uint8_t bit_rem = bit_off % 8;       // 字节内位偏移
        // 从 raw[1 + byte_off] 读取最多 3 字节，超出的位被 0x07FF 清除
        uint16_t raw = (uint16_t)(((uint32_t)frame->raw[1 + byte_off] >> bit_rem) |
                                  ((uint32_t)frame->raw[1 + byte_off + 1] << (8 - bit_rem)) |
                                  ((uint32_t)frame->raw[1 + byte_off + 2] << (16 - bit_rem))) &
                       0x07FF;
        int32_t offset = (int32_t)raw - center;
        float ch = (float)offset * (offset >= 0 ? scale_pos : scale_neg);

        // 限幅到 -1.0 ~ 1.0
        if (ch > 1.0f)
        {
            ch = 1.0f;
        }
        else if (ch < -1.0f)
        {
            ch = -1.0f;
        }
        result.ch[i] = ch;
    }

    // 使用位域解析数字通道和标志位（字节 23）
    result.digital_ch[0] = frame->frame.flags.ch17 ? 1 : 0;
    result.digital_ch[1] = frame->frame.flags.ch18 ? 1 : 0;
    result.frame_lost = frame->frame.flags.frame_lost ? 1 : 0;
    result.failsafe = frame->frame.flags.failsafe ? 1 : 0;

    // 帧尾标志也包含状态信息
    if (frame->frame.footer == SBUS_FOOTER_FRAME_LOST)
    {
        result.frame_lost = 1;
    }
    else if (frame->frame.footer == SBUS_FOOTER_FAILSAFE)
    {
        result.failsafe = 1;
    }

    return result;
}

/*------------- 私有函数实现 --------------*/

/**
 * @brief BSP 层 UART 错误回调（ISR 或任务上下文）
 * @param usart_inst USART 实例指针
 *
 * @note 失联保护必须能覆盖"完全没有数据流"的情形，否则等于没有：
 *       帧丢失判据写在 SBUSUARTRxCallback 里，而 RX 一旦停摆（bsp 续收最终失败）
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
 *       不能在这里调 USARTReceive —— 真正的重启交给 SBUSUARTDaemonCallback（任务上下文）。
 *
 * @note 只认接收侧的原因：`USART_ERR_TX_ABORT` 是本实例的**发送**被中止，与遥控器失联无关。
 *       本驱动根本不发（tx_callback 为 NULL），但 bsp 的发送卡死自恢复是按 UART 实例
 *       触发的，误当失联会让失控保护凭空动作。
 */
static void SBUSUARTErrCallback(USARTInstance *usart_inst, USART_ErrReason_e reason)
{
    SBUSInstance *sbus_inst;

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

    sbus_inst = (SBUSInstance *)usart_inst->parent;
    if (sbus_inst == NULL)
    {
        return;
    }

    sbus_inst->signal_lost = 1;
    sbus_inst->lost_start_time_us = 0;
}

/**
 * @brief daemon 离线回调（DaemonTask 任务上下文，1ms 一次）——接收停摆的自动重启
 * @param owner 所属模块实例（SBUSConfig 写入的 Daemon_Config_s.owner_id）
 *
 * @note 为什么用 daemon 回调做重启入口：接收一旦停摆，rx_callback 不再触发，链路再无任何
 *       周期性事件可挂 —— 而 daemon 恰好是"没喂狗（没收到帧）"才被触发的，且运行在
 *       DaemonTask（任务上下文，1ms），是唯一现成的、与接收无关的任务上下文时基。
 *       bsp 明确禁止在 ISR 里重启接收（Abort 要自旋等 tick），本回调正合适。
 *
 * @note 判据（`rx_armed == 0` 才是真停摆，遥控器没开机不算）与限频都收在 bsp 的
 *       USARTRecoverRxIfStalled 里，四条 UART 链路共用同一份实现；这里只给限频周期。
 *       依赖 daemon 已启用：daemon_reload 配 0 会被 SBUSConfig 提升为
 *       DRV_SBUS_DAEMON_RELOAD_DEFAULT，故本通道不会被"禁用监控"误关。
 */
static void SBUSUARTDaemonCallback(void *owner)
{
    SBUSInstance *sbus_inst = (SBUSInstance *)owner;

    if (sbus_inst == NULL || sbus_inst->usart_inst == NULL)
    {
        return;
    }

    /* 接收在跑 / 未到限频周期 → BSP_BUSY，空转；只有真重启成功才打日志 */
    if (USARTRecoverRxIfStalled(sbus_inst->usart_inst, DRV_SBUS_RX_RESTART_PERIOD_MS) == BSP_OK)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "RX stalled, receive restarted");
    }
}

/**
 * @brief BSP 层 UART 接收回调
 * @param usart_inst USART 实例指针
 * @note 通过 parent 字段获取 SBUSInstance，调用 APP 回调
 */
static void SBUSUARTRxCallback(USARTInstance *usart_inst)
{
    // 参数检查
    if (usart_inst == NULL)
    {
        return;
    }

    // 检查帧长度
    if (usart_inst->rx_len != SBUS_FRAME_SIZE)
    {
        BSPLOG(&g_sbus_log, LOG_LEVEL_WARNING, "Frame length error: %d (expected %d)", usart_inst->rx_len,
               SBUS_FRAME_SIZE);
        return;
    }

    // 通过 parent 字段获取 SBUSInstance 指针
    SBUSInstance *sbus_inst = (SBUSInstance *)usart_inst->parent;

    // 调用 APP 层回调（传递解析后的数据）
    if (sbus_inst != NULL)
    {
        // 在中断上下文中解析原始数据为通道数据
        sbus_inst->sbus_data = SBUSDecodeFrame(sbus_inst, usart_inst->rx_buff, usart_inst->rx_len);
        DaemonReload(sbus_inst->daemon);

        // ---- 信号丢失超时检测 ----
        if (sbus_inst->sbus_data.frame_lost || sbus_inst->sbus_data.failsafe)
        {
            // 丢帧/失控状态：如果尚未计时则记录时间戳
            if (sbus_inst->lost_start_time_us == 0)
            {
                sbus_inst->lost_start_time_us = DWT_GetTimeUs();
            }

            // 检查是否超过超时时间
            if ((DWT_GetTimeUs() - sbus_inst->lost_start_time_us) >= sbus_inst->lost_timeout_us)
            {
                sbus_inst->signal_lost = 1;
            }
        }
        else
        {
            // 信号恢复正常：清除计时和丢失标志
            sbus_inst->lost_start_time_us = 0;
            sbus_inst->signal_lost = 0;
        }
    }
}

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* DRV_SBUS_USED */
