/**
 * @file comm_media_usart.c
 * @brief 通信框架-硬件层（Media）UART 后端实现
 *
 * 发送：USARTTransmit，按 bsp 实例 tx_mode 执行；
 *       阻塞模式整帧发完，IT/DMA 异步发送（timeout 仅用于等待就绪）。
 * 接收：HAL_UARTEx_RxEventCallback（DMA+IDLE）→ bsp rx_callback → 适配钩子
 *       MediaUsartRxHook → CommMediaRxHook（comm 层接收入口）。
 *
 * @note bsp 在接收回调后立即 memset(rx_buff) 并重启接收，因此接收处理
 *       必须在回调上下文内同步消费或 memcpy 走，不能延迟引用 rx_buff。
 */

#include "comm_media_usart.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include "bsp_log.h"
#include <string.h>

#ifdef DRV_COMM_USED

/* 接收停摆后自动重启的重试周期（ms）：离线期间 DaemonTask 每 1ms 调一次 offline 钩子，
 * 全靠这个周期限频。取值应小于上层判定"链路失效"的时间尺度（本工程对端 2ms 发一帧）。 */
#ifndef DRV_COMM_MEDIA_USART_RX_RESTART_PERIOD_MS
#define DRV_COMM_MEDIA_USART_RX_RESTART_PERIOD_MS 20
#endif

#ifndef DRV_COMM_MEDIA_USART_LOG_LIMIT
#define DRV_COMM_MEDIA_USART_LOG_LIMIT 10
#endif // !DRV_COMM_MEDIA_USART_LOG_LIMIT
LOG_INSTANCE_DEF(g_media_usart_log, "comm_media_usart", DRV_COMM_MEDIA_USART_LOG_LIMIT);

static int8_t MediaUsartSend(CommMedia *media, const uint8_t *data);
static void MediaUsartOfflineHook(void *owner);

static const CommMediaVTable_s s_usart_vtable = {
    .send = MediaUsartSend,
    .offline = MediaUsartOfflineHook,
};

/* vtable 发送实现：UART 无分包，一帧 = 整个缓冲。
 * 拷贝 comm 打包缓冲 → m->tx_buff（后端自持 staging，DMA 异步发送期间须常驻）后整帧发出。
 * 自恢复：每次进来先调 bsp 的 USARTRecoverTxIfStuck 自检发送卡死（见函数内注释），
 * 复位成功后本帧即可照常发出；启动失败的瞬态（BSP_TIMEOUT/BSP_HW_ERR）再由下面的重试兜底。
 * BSP_BUSY 是背压（上层应排队），立即重试无意义。 */
static int8_t MediaUsartSend(CommMedia *media, const uint8_t *data)
{
    CommMediaUsart *m = (CommMediaUsart *)media;
    USARTInstance *usart;
    BSP_Status_e ret;

    /* 先判空再解引用（m==NULL 时不能先访问 m->base.media） */
    if (m == NULL || data == NULL || m->tx_buff_size == 0)
        return -1;
    usart = (USARTInstance *)m->base.media;
    if (usart == NULL || usart->handle == NULL)
        return -1;
    /* 发送卡死自检：必须早于下面的忙判定 —— 本后端在"忙"时直接返回（不动 memcpy），
     * 一旦上一次发送卡死（gState 停在 BUSY_TX、完成回调不会再来），这里就会永远返回 -1，
     * 永远走不到 USARTTransmit 里的卡死复位，链路发送侧永久静默。此处的探测能在
     * 任务上下文把状态复位（复位后 gState 即 READY，本帧就能照常发出去）。
     * ISR 里调用安全（bsp 入口直接返回 BSP_BUSY）；空闲时只做一次 DWT 读。 */
    (void)USARTRecoverTxIfStuck(usart, 0);
    /* 忙判定必须**早于** memcpy：上一帧的 DMA 正在从 m->tx_buff 取数，
     * 先拷贝会把在途帧的内容改掉（发出去的是新旧混合的残帧）。
     * 这里与 bsp_log/vofa/terminal 一样用 gState 作就绪判据 —— 本实例的发送只有
     * 本函数一个发起者（tx_callback 不发送），检查与启动之间无人能插进来。 */
    if (usart->handle->gState != HAL_UART_STATE_READY)
    {
        m->tx_fail++; /* 背压：上层按自己的周期重发，本帧丢弃 */
        return -1;
    }
    memcpy(m->tx_buff, data, m->tx_buff_size);

    ret = USARTTransmit(usart, m->tx_buff, m->tx_buff_size, m->tx_mode, m->timeout_ms);
    if (ret == BSP_OK)
        return 0;

    /* 只对"值得重试"的失败重试一次：
     *   BSP_TIMEOUT   —— bsp 已强制中止卡死的发送并复位状态，此刻外设空闲，重试有意义；
     *   BSP_HW_ERR    —— HAL 启动失败，可能是瞬态；
     *   BSP_BUSY      —— 背压（上层应排队），立即重试必然还是忙；
     *   BSP_PARAM_ERR —— 参数/配置错误（如该口根本没配 TX DMA），重试永远不会成功。 */
    if ((ret == BSP_TIMEOUT || ret == BSP_HW_ERR) &&
        USARTTransmit(usart, m->tx_buff, m->tx_buff_size, m->tx_mode, m->timeout_ms) == BSP_OK)
        return 0;

    m->tx_fail++;
    return -1;
}

/**
 * @brief 介质离线钩子（DaemonTask 任务上下文，1ms 一次）——重启停摆的接收
 * @param owner DaemonConfig 写入的 owner_id（comm 实例，CommConfig 填）
 *
 * @note 接收一旦停摆，bsp 的 rx_callback 不再触发，链路再无任何周期性事件可挂；
 *       而 daemon 恰好是"没喂狗（没收到帧）"才被触发的，且跑在 DaemonTask（任务上下文），
 *       是介质唯一现成的、与接收无关的任务上下文时基。bsp 禁止在 ISR 里重启接收
 *       （Abort 要自旋等 HAL tick），所以重启放这里。
 *
 * @note 判据、限频、重启参数全在 bsp 的 USARTRecoverRxIfStalled 里（四条 UART 链路共用
 *       同一份实现），本钩子只负责在 daemon 的任务上下文把它调起来、给一个限频周期。
 *       重启参数用 bsp 记着的 rx_xfer_len/rx_mode，本模块不再自持一份 ——
 *       USARTReceive 现在会在**所有失败分支之前**记录它们，故"Config 启动失败"
 *       也不会让参数停在 0/BLOCK（早先自持副本就是为了绕开这一点）。
 *       依赖 daemon 已启用：CommConfig 会把 daemon_reload 配 0 提升为默认值（见 DRV_COMM_DAEMON_RELOAD_DEFAULT），
 *       故本通道不会被"禁用监控"误关。
 */
static void MediaUsartOfflineHook(void *owner)
{
    CommInstance *inst = (CommInstance *)owner;
    CommMediaUsart *m;
    USARTInstance *usart;

    if (inst == NULL || inst->media == NULL)
        return;

    m = (CommMediaUsart *)inst->media; /* vtable 只挂在 UART 介质上，可达 */
    usart = (USARTInstance *)m->base.media;
    if (usart == NULL)
        return; /* 介质未绑定 bsp 实例（handle 为空由 bsp 入口自己判） */

    /* 接收在跑（只是对端没发帧）/ 未到限频周期 → BSP_BUSY，空转 */
    if (USARTRecoverRxIfStalled(usart, DRV_COMM_MEDIA_USART_RX_RESTART_PERIOD_MS) == BSP_OK)
        BSPLOG(&g_media_usart_log, LOG_LEVEL_WARNING, "RX stalled, receive restarted");
}

/* bsp 接收适配钩子：收完一段数据，长度校验后直接交给 comm 层接收入口 */
static void MediaUsartRxHook(USARTInstance *usart)
{
    CommMedia *media = (CommMedia *)usart->parent; /* DRV 层设置的反向指针 */
    CommMediaUsart *m;
    if (media == NULL)
        return;

    /* 固定长度模型：UART 无分包，一帧 = 本次请求的整段长度。
     * 判据必须用 media 自持的 rx_len（= Config 时实际提交给 USARTReceive 的长度），
     * 不能比 rx_buff_size：二者可以不等（rx_len 配得比缓冲小是常见用法，缓冲留余量），
     * 那时 rx_len 永远不会等于 rx_buff_size，本钩子会把每一帧都丢掉、链路收不到任何数据。
     * rx_len == 0 是 IDLE 空事件，同样丢。 */
    m = (CommMediaUsart *)media;
    if (m->rx_len == 0 || usart->rx_len != m->rx_len)
        return;

    CommMediaRxHook(media, usart->rx_buff); /* 跳过 media 基类，直连 comm */
}

int8_t MediaUsartRegister(CommMediaUsart *media)
{
    USARTInstance *usart;

    if (media == NULL || media->tx_buff == NULL)
        return -1;
    usart = (USARTInstance *)media->base.media; /* COMM_MEDIA_USART_DEF 已绑定 */
    if (usart == NULL)
        return -1;

    /* bsp 注册（防重复注册，本函数不可重入） */
    if (USARTRegister(usart) != BSP_OK)
        return -1;

    media->base.vtable = &s_usart_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    media->tx_fail = 0;
    media->rx_len = 0; /* 0 = "整缓冲"，Config 换成真实长度前不接受任何帧（见 MediaUsartRxHook） */
    return 0;
}

int8_t MediaUsartConfig(CommMediaUsart *media, CommMediaUsartConfig_s *cfg)
{
    USARTInstance *usart;
    USART_Config_s ucfg;
    uint16_t rx_len;

    if (media == NULL || cfg == NULL)
        return -1;
    usart = (USARTInstance *)media->base.media;
    if (usart == NULL)
        return -1;

    /* 1. 先校验全部可校验项，再动硬件：失败只返回 -1，不留半应用状态 */
    if (cfg->tx_mode > BSP_DMA_MODE)
        return -1;
    /* rx 只接受 IT/DMA：BLOCK 接收不触发 rx_callback，本链路会静默收不到任何数据；
     * 而零初始化的 rx_mode 正好是 BLOCK，"少配一个字段"就中招（见头文件说明） */
    if (cfg->rx_mode != BSP_IT_MODE && cfg->rx_mode != BSP_DMA_MODE)
        return -1;
    rx_len = (cfg->rx_len != 0) ? cfg->rx_len : usart->rx_buff_size;
    if (rx_len > usart->rx_buff_size)
        return -1;

    /* 2. 总线配置：parent 与 rx_callback 一律覆盖为自身适配钩子，
     * 保证接收统一进 comm 层接收入口（CommMediaRxHook），业务不直接走 bsp 回调 */
    ucfg = cfg->usart;
    ucfg.parent = media; /* 反向指针：适配钩子据此取回 media */
    ucfg.rx_callback = MediaUsartRxHook;
    if (USARTConfig(usart, &ucfg) != BSP_OK)
        return -1;

    /* 3. 先记录发送参数与帧长：帧长（rx_len）要早于接收钩子可能被触发的时刻生效，
     * 发送参数供 MediaUsartSend 每次透传 */
    media->tx_mode = cfg->tx_mode;
    media->timeout_ms = cfg->timeout_ms; /* 完全按 Config 配置的超时时间使用 */
    media->rx_len = rx_len;

    /* 4. 启动接收常开流（bsp 的 USARTConfig 不再自动启动接收） */
    if (USARTReceive(usart, rx_len, cfg->rx_mode, 0) != BSP_OK)
    {
        BSPLOG(&g_media_usart_log, LOG_LEVEL_ERROR,
               "receive start failed (mode=%d, len=%u), offline hook will retry",
               (int)cfg->rx_mode, rx_len);
        return -1;
    }

    return 0;
}

#endif /* DRV_COMM_USED */
