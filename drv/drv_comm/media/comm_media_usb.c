/**
 * @file comm_media_usb.c
 * @brief 通信框架-硬件层（Media）USB(CDC) 后端实现
 *
 * 发送：整帧按 63B/片分包，每片前加 1B 分包序号（该片在整个帧的第几包，0 起递增），
 *       每包 = [pkt_idx][数据片 ≤ 63B]（≤64B，CDC FS 单包上限）经 USBTransmit 发出。
 * 接收：USB OUT 中断 → CDC_Receive_HS → bsp_usb_rx_handler（0~64B 一包）
 *       → 适配钩子 MediaUsbRxHook → 按分包序号连续重组整帧（错位丢帧重同步）
 *       → CommMediaRxHook（comm 层接收入口）。
 *
 * @note bsp 在接收回调前已重挂接收（同一缓冲），回调返回后缓冲可能被下一包覆盖，
 *       因此接收处理必须在回调上下文内同步 memcpy 累积，不能延迟引用 usb->rx_buff。
 * @note 分包序号是 media 层协议元数据，不进入协议内容；接收重组到固定协议帧长
 *       （rx_frame_len）即完成一帧。
 *
 * 错误处理与自恢复（bsp_usb 完成重构后）：
 *   - 发送按返回码分流：BSP_BUSY（ring 放不下整帧，零字节写入）是**背压**，退避后重发整帧即可；
 *     其余非 BSP_OK 才算发送失败。旧版把两者压成同一个 -1，无法区分"退避一下就好"与"主机不在"。
 *   - 分包失败即中止本轮（bsp 现在是整包原子入队，继续切下一片必然还是满的）。
 *   - 发送入口前置 USBRecoverTxIfStuck、离线钩子里补 USBRecoverRxIfStalled/发送卡死收尾，
 *     判据/限频/纠正动作全在 bsp 内（与 UART 后端同构）。
 */

#include "comm_media_usb.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include "bsp_log.h"
#include <string.h>

#ifdef DRV_COMM_USED

/* 单 USB 包数据片上限 = 64 - 1（分包序号），与 CDC FS 最大包长（USB_TX_BUF_SIZE）对齐 */
#define USB_MEDIA_PAYLOAD_PER_PKT (USB_TX_BUF_SIZE - 1) /* 63 */

/* 接收重新武装的重试周期（ms）：离线期间 DaemonTask 每 1ms 调一次 offline 钩子，
 * 全靠这个周期限频。取值应小于上层判定"链路失效"的时间尺度（本工程对端 2ms 发一帧）。 */
#ifndef DRV_COMM_MEDIA_USB_RX_RESTART_PERIOD_MS
#define DRV_COMM_MEDIA_USB_RX_RESTART_PERIOD_MS 20
#endif

#ifndef DRV_COMM_MEDIA_USB_LOG_LIMIT
#define DRV_COMM_MEDIA_USB_LOG_LIMIT 10
#endif // !DRV_COMM_MEDIA_USB_LOG_LIMIT
LOG_INSTANCE_DEF(g_media_usb_log, "comm_media_usb", DRV_COMM_MEDIA_USB_LOG_LIMIT);

static int8_t MediaUsbSend(CommMedia *media, const uint8_t *data);
static void MediaUsbRxHook(USBInstance *usb);
static void MediaUsbErrHook(USBInstance *usb, USB_ErrReason_e reason);
static void MediaUsbOfflineHook(void *owner);
static void MediaUsbFrameComplete(CommMediaUsb *m);

static const CommMediaVTable_s s_usb_vtable = {
    .send = MediaUsbSend,
    .offline = MediaUsbOfflineHook,
};

/* vtable 发送实现：整帧协议帧按 63B/片分包发送，每包 = [分包序号][数据片]。
 * 1) data = comm 打包缓冲（本函数运行期间一直有效），直接引用整帧，无需拷贝
 * 2) 循环切片：每片 ≤ 63B，pkt_idx 从 0 起递增（该片在整个帧的第几包）
 * 3) 每包单独 USBTransmit（≤64B，bsp 一次发出；多包经 USBTransmit ring 完成中断续发）
 * @note USBTransmit 同步拷入 ring，发送期间 data 无失效风险，故 media 不持 staging 缓冲 */
static int8_t MediaUsbSend(CommMedia *media, const uint8_t *data)
{
    CommMediaUsb *m = (CommMediaUsb *)media;
    USBInstance *usb;
    uint16_t remain;
    uint8_t pkt_idx = 0;
    int8_t ret = 0;
    BSP_Status_e st;
    uint8_t pkt[USB_TX_BUF_SIZE]; /* 单包暂存 = [pkt_idx][数据片] */

    /* 先判空再解引用（m==NULL 时不能先访问 m->base.media） */
    if (m == NULL || data == NULL || m->tx_frame_len == 0)
        return -1;
    usb = (USBInstance *)m->base.media;
    if (usb == NULL)
        return -1;

    /* 发送卡死自检：必须早于实际发送。ring 一旦推不动（CDC IN 端点的在途传输卡死），
     * USBTransmit 会一路 BSP_BUSY，而"发送失败"没有任何中断能把它救回来——本入口是
     * 每帧必经之处，在任务上下文（comm 周期任务）给 bsp 一次收尾机会。
     * 判据/限频/动作都在 bsp 的 USBRecoverTxIfStuck 里，这里原样调用即可。 */
    (void)USBRecoverTxIfStuck(usb, 0);

    /* 分包发送：整帧 = data，tx_frame_len > 63B 时拆成多包，seq = 该片在整个帧的第几包（0 起）。
     * 失败即中止本轮：缺包由接收端按序号错位丢帧重同步（只损失一帧）。 */
    remain = m->tx_frame_len;
    while (remain > 0)
    {
        uint16_t chunk = (remain > USB_MEDIA_PAYLOAD_PER_PKT) ? USB_MEDIA_PAYLOAD_PER_PKT : remain;

        pkt[0] = pkt_idx++; /* 分包序号（0,1,2,...） */
        memcpy(&pkt[1], &data[m->tx_frame_len - remain], chunk);

        st = USBTransmit(usb, pkt, (uint16_t)(chunk + 1));
        if (st != BSP_OK)
        {
            /* BSP_BUSY = ring 放不下这一包（bsp 保证零字节写入）→ 背压，退避后重发整帧即可；
             * 其余（BSP_HW_ERR 未枚举 / BSP_PARAM_ERR）= 本帧确实发不出去。两者分开计数，
             * 调试时"tx_busy 涨"是主机读得慢，"tx_fail 涨"才是链路真出问题。 */
            if (st == BSP_BUSY)
                m->tx_busy++;
            else
                m->tx_fail++;
            ret = -1;
            break; /* 整包原子入队后，继续切下一片必然还是满的 */
        }
        remain -= chunk;
    }
    return ret;
}

/* 重组出一整帧（rx_cnt 已达 rx_frame_len）：上交 comm 层并复位累积 */
static void MediaUsbFrameComplete(CommMediaUsb *m)
{
    /* 跳过 media 基类，直连 comm 层接收入口；m->rx_buff 为完整协议帧（无分包序号）
     * @warning UNPACK_IN_ISR 下 payload 指向累积缓冲，回调返回后即被下一包覆盖，
     *          on_frame 必须同步消费（解析/拷贝） */
    CommMediaRxHook(&m->base, m->rx_buff);
    m->rx_cnt = 0;
    m->rx_expect_pkt = 0;
}

/* bsp 接收适配钩子：每包 = [pkt_idx][数据片]，按分包序号连续重组整帧。
 * 一包 ≤ 64B（一个 CDC bulk 传输），包序号错位说明丢包 → 丢帧重同步等新帧首包。 */
static void MediaUsbRxHook(USBInstance *usb)
{
    CommMediaUsb *m = (CommMediaUsb *)usb->parent; /* media 层设置的反向指针 */
    uint8_t pkt_idx;
    uint16_t data_len;

    if (m == NULL || usb->rx_len < 1)
        return;

    pkt_idx = usb->rx_buff[0];
    data_len = (uint16_t)(usb->rx_len - 1);

    /* 分包序号校验：期望连续。错位说明丢包/错乱 → 丢弃当前帧累积，重新同步 */
    if (pkt_idx != m->rx_expect_pkt)
    {
        if (m->rx_cnt > 0)
            m->lost_frames++; /* 帧中途丢包 */
        m->rx_cnt = 0;
        m->rx_expect_pkt = 0;
        if (pkt_idx != 0)
        {
            m->lost_frames++; /* 非新帧首包错位 */
            return;           /* 丢弃当前包，等新帧首包（pkt_idx=0） */
        }
    }

    /* 追加数据片（超出帧长部分截断，防御配置不符） */
    if (data_len > 0)
    {
        uint16_t space = m->rx_frame_len - m->rx_cnt; /* 剩余目标字节数 */

        if (data_len > space)
            data_len = space;
        if (data_len > 0)
            memcpy(&m->rx_buff[m->rx_cnt], &usb->rx_buff[1], data_len);
        m->rx_cnt += data_len;
        m->rx_expect_pkt++;
    }

    /* 累积到完整协议帧长：上交一帧 */
    if (m->rx_cnt == m->rx_frame_len)
        MediaUsbFrameComplete(m);
}

/* bsp 错误回调（任务上下文）：只做观测计数，再转发给用户在 cfg 里给的回调（若有）。
 * @note **刻意不在这里清任何状态**：bsp 已经把"要不要清、清什么"做完并据此调用的
 *       （TX 卡死收尾后才报 TX_STUCK、重新武装失败才报 RX_STALLED），本层再清一遍
 *       只会引入"错误广播清掉正在途的发送"这种误伤。本层要复位的东西只有 rx_cnt/
 *       rx_expect_pkt，而它们由 RxHook 的序号错位逻辑自己管。 */
static void MediaUsbErrHook(USBInstance *usb, USB_ErrReason_e reason)
{
    CommMediaUsb *m = (CommMediaUsb *)usb->parent; /* media 层设置的反向指针 */

    if (m == NULL)
        return;

    m->err_count++;
    m->last_err = (uint8_t)reason;

    if (m->user_err_callback != NULL)
        m->user_err_callback(usb, reason);
}

/**
 * @brief 介质离线钩子（DaemonTask 任务上下文，1ms 一次）——发送卡死收尾 + 接收重新武装
 * @param owner DaemonConfig 写入的 owner_id（comm 实例，CommConfig 填）
 *
 * @note 为什么搭在 daemon 上：发送卡死与接收停摆都会让链路**再也没有任何周期性事件**
 *       （TX 完成回调 / rx_callback 都不再来），而 daemon 恰好是"没喂狗（没收到帧）"
 *       才被触发的，且跑在 DaemonTask（任务上下文）——是介质唯一现成的、与收发无关的
 *       任务上下文时基。bsp 明确要求这两条恢复只在任务上下文调用。
 * @note 判据、限频、纠正动作全在 bsp 的 USBRecoverTxIfStuck / USBRecoverRxIfStalled 里
 *       （本模块只负责把它俩调起来）；返回 BSP_OK 才表示"本次刚做了恢复动作"，
 *       链路健康时返回 BSP_BUSY，故不会刷日志。
 * @note 依赖 daemon 已启用：CommConfig 把 daemon_reload 配 0 提升为默认值只对挂了
 *       offline 钩子的后端生效，本后端现在挂了，故该通道不会被"禁用监控"误关。
 */
static void MediaUsbOfflineHook(void *owner)
{
    CommInstance *inst = (CommInstance *)owner;
    CommMediaUsb *m;
    USBInstance *usb;

    if (inst == NULL || inst->media == NULL)
        return;

    m = (CommMediaUsb *)inst->media; /* vtable 只挂在 USB 介质上，可达 */
    usb = (USBInstance *)m->base.media;
    if (usb == NULL)
        return; /* 介质未绑定 bsp 实例 */

    /* 发送侧：ring 里有货却久推不动 = 卡死 */
    if (USBRecoverTxIfStuck(usb, 0) == BSP_OK)
        BSPLOG(&g_media_usb_log, LOG_LEVEL_WARNING, "TX stuck, flushed by offline hook");

    /* 接收侧：久无收帧则重新武装 OUT 端点 */
    if (USBRecoverRxIfStalled(usb, DRV_COMM_MEDIA_USB_RX_RESTART_PERIOD_MS) == BSP_OK)
        BSPLOG(&g_media_usb_log, LOG_LEVEL_WARNING, "RX stalled, re-armed by offline hook");
}

int8_t MediaUsbRegister(CommMediaUsb *media)
{
    USBInstance *usb;

    if (media == NULL || media->rx_buff == NULL)
        return -1;
    if (media->rx_frame_len == 0 || media->tx_frame_len == 0)
        return -1; /* 收发协议帧长须非 0 */

    usb = (USBInstance *)media->base.media; /* COMM_MEDIA_USB_DEF 已绑定 */
    if (usb == NULL)
        return -1;

    /* bsp 注册（防重复注册；USB_INSTANCE_NUM=1，本函数不可重入） */
    if (USBRegister(usb) != BSP_OK)
        return -1;

    media->base.vtable = &s_usb_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    usb->parent = media; /* 反向指针：适配钩子据此取回 media */

    /* 清接收累积与分包序号状态（初始：期望首包 pkt_idx=0）及统计 */
    media->rx_cnt = 0;
    media->rx_expect_pkt = 0;
    media->lost_frames = 0;
    media->tx_fail = 0;
    media->tx_busy = 0;
    media->err_count = 0;
    media->last_err = 0;
    media->user_err_callback = NULL;
    return 0;
}

int8_t MediaUsbConfig(CommMediaUsb *media, USB_Config_s *cfg)
{
    USBInstance *usb;
    USB_Config_s local_cfg;

    if (media == NULL)
        return -1;
    usb = (USBInstance *)media->base.media;
    if (usb == NULL)
        return -1;

    /* USB 无运行期参数：cfg 可传 NULL；拷贝一份并强制接管接收回调与反向指针 */
    if (cfg != NULL)
        local_cfg = *cfg;
    else
        local_cfg = (USB_Config_s){0};
    media->user_err_callback = local_cfg.err_callback; /* 用户自己的错误回调：本层钩子转发给它（可为 NULL） */
    local_cfg.rx_callback = MediaUsbRxHook;            /* 保证接收统一进 comm 层接收入口 */
    local_cfg.err_callback = MediaUsbErrHook;          /* 本层观测计数 + 转发（见 ErrHook 说明） */
    local_cfg.parent = media;                          /* 反向指针：接收钩子据此取回 media（USBConfig 写入实例） */
    if (USBConfig(usb, &local_cfg) != BSP_OK)
        return -1;
    return 0;
}

#endif /* DRV_COMM_USED */
