/**
 * @file comm_media_usb_simple.c
 * @brief 通信框架-硬件层（Media）USB(CDC) 后端实现·短帧免序号版
 *
 * 与 comm_media_usb 的唯一差别在收发判定：
 *   - 发送：整帧 ≤ 64B（CDC FS 单包上限）→ 免序号，直接 USBTransmit 整帧（单包）；
 *           整帧 > 64B → 分包（每包 = [pkt_idx][数据片 ≤ 63B]），行为同 usb 后端
 *   - 接收：rx_frame_len ≤ 64B → 整包透传（包长须恰好 == 帧长，不剥序号）后上交；
 *           rx_frame_len > 64B → 按分包序号连续重组（行为同 usb 后端）
 * 适用视觉 PC 端固定长度、单包无序号线协议（如 50B/57B 视觉包）的直接对接。
 *
 * @note bsp 在接收回调前已重挂接收（同一缓冲），回调返回后缓冲可能被下一包覆盖，
 *       因此接收处理必须在回调上下文内同步 memcpy 累积，不能延迟引用 usb->rx_buff。
 */

#include "comm_media_usb_simple.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include <string.h>

#ifdef DRV_COMM_USED

/* 长帧分包单包数据片上限 = 64 - 1（分包序号占 1B）。短帧免序号时整帧可一次发满
 * USB_TX_BUF_SIZE（64B），判定阈值用 USB_TX_BUF_SIZE 而非本宏 */
#define USB_MEDIA_PAYLOAD_PER_PKT (USB_TX_BUF_SIZE - 1) /* 63 */

static int8_t MediaUsbSimpleSend(CommMedia *media, const uint8_t *data);
static void MediaUsbSimpleRxHook(USBInstance *usb);
static void MediaUsbSimpleFrameComplete(CommMediaUsbSimple *m);

static const CommMediaVTable_s s_usb_simple_vtable = {
    .send = MediaUsbSimpleSend,
};

/* vtable 发送实现：整帧 ≤ 64B 免序号单包透传；> 64B 分包加序号（同 usb 后端）。
 * 1) data = comm 打包缓冲（本函数运行期间一直有效），直接引用整帧，无需拷贝
 * 2) 短帧：USBTransmit(data, tx_frame_len) 一次发出（无 [pkt_idx] 前缀，唯一差别点）
 * 3) 长帧：循环切片每片 ≤ 63B，pkt_idx 从 0 起递增，每包单独 USBTransmit */
static int8_t MediaUsbSimpleSend(CommMedia *media, const uint8_t *data)
{
    CommMediaUsbSimple *m = (CommMediaUsbSimple *)media;
    USBInstance *usb;
    uint16_t remain;
    uint8_t pkt_idx = 0;
    uint8_t pkt[USB_TX_BUF_SIZE]; /* 长帧分包单包暂存 = [pkt_idx][数据片] */

    /* 先判空再解引用（m==NULL 时不能先访问 m->base.media） */
    if (m == NULL || data == NULL || m->tx_frame_len == 0)
        return -1;
    usb = (USBInstance *)m->base.media;
    if (usb == NULL)
        return -1;

    /* 短帧（整帧 ≤ 64B，单包装下）：免分包序号，整帧一包透传 */
    if (m->tx_frame_len <= USB_TX_BUF_SIZE)
    {
        USBTransmit(usb, data, m->tx_frame_len); /* bsp 未枚举时静默丢弃 */
        return 0;
    }

    /* 长帧（> 63B）：分包发送，每包 = [分包序号][数据片 ≤ 63B]（行为同 usb 后端） */
    remain = m->tx_frame_len;
    while (remain > 0)
    {
        uint16_t chunk = (remain > USB_MEDIA_PAYLOAD_PER_PKT) ? USB_MEDIA_PAYLOAD_PER_PKT : remain;

        pkt[0] = pkt_idx++; /* 分包序号（0,1,2,...） */
        memcpy(&pkt[1], &data[m->tx_frame_len - remain], chunk);
        USBTransmit(usb, pkt, (uint16_t)(chunk + 1)); /* bsp 未枚举时静默丢弃 */
        remain -= chunk;
    }
    return 0;
}

/* 重组出一整帧（短帧整包透传直接触发 / 长帧 rx_cnt 已达 rx_frame_len）：上交 comm 层并复位累积 */
static void MediaUsbSimpleFrameComplete(CommMediaUsbSimple *m)
{
    /* 跳过 media 基类，直连 comm 层接收入口；m->rx_buff 为完整协议帧（无分包序号）
     * @warning UNPACK_IN_ISR 下 payload 指向累积缓冲，回调返回后即被下一包覆盖，
     *          on_frame 必须同步消费（解析/拷贝） */
    CommMediaRxHook(&m->base, m->rx_buff);
    m->rx_cnt = 0;
    m->rx_expect_pkt = 0;
}

/* bsp 接收适配钩子：短帧对话整包透传（免序号）；长帧对话按分包序号连续重组（同 usb 后端） */
static void MediaUsbSimpleRxHook(USBInstance *usb)
{
    CommMediaUsbSimple *m = (CommMediaUsbSimple *)usb->parent; /* media 层设置的反向指针 */
    uint8_t pkt_idx;
    uint16_t data_len;

    if (m == NULL || usb->rx_len < 1)
        return;

    /* 短帧对话（rx_frame_len ≤ 64）：整帧一包免序号，包长须恰好 == 帧长。
     * 上位机一次写完整帧即触发（虚拟串口按 USB 包回调，跨包写会分段→长度不符丢弃，
     * 与原始视觉协议"固定长度整包"约定一致）。 */
    if (m->rx_frame_len <= USB_TX_BUF_SIZE)
    {
        if (usb->rx_len != m->rx_frame_len)
        {
            m->lost_frames++; /* 长度不符：丢弃（上位机未整包一次发完） */
            return;
        }
        memcpy(m->rx_buff, usb->rx_buff, m->rx_frame_len);
        MediaUsbSimpleFrameComplete(m);
        return;
    }

    /* 长帧对话（> 63B）：分包序号校验，期望连续。错位说明丢包/错乱 → 丢弃当前帧累积，重新同步 */
    pkt_idx = usb->rx_buff[0];
    data_len = (uint16_t)(usb->rx_len - 1);

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
        MediaUsbSimpleFrameComplete(m);
}

int8_t MediaUsbSimpleRegister(CommMediaUsbSimple *media)
{
    USBInstance *usb;

    if (media == NULL || media->rx_buff == NULL)
        return -1;
    if (media->rx_frame_len == 0 || media->tx_frame_len == 0)
        return -1; /* 收发协议帧长须非 0 */

    usb = (USBInstance *)media->base.media; /* COMM_MEDIA_USB_SIMPLE_DEF 已绑定 */
    if (usb == NULL)
        return -1;

    /* bsp 注册（防重复注册；USB_INSTANCE_NUM=1，本函数不可重入） */
    if (USBRegister(usb) != 0)
        return -1;

    media->base.vtable = &s_usb_simple_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    usb->parent = media; /* 反向指针：适配钩子据此取回 media */

    /* 清接收累积与分包序号状态（初始：期望首包 pkt_idx=0） */
    media->rx_cnt = 0;
    media->rx_expect_pkt = 0;
    media->lost_frames = 0;
    return 0;
}

int8_t MediaUsbSimpleConfig(CommMediaUsbSimple *media, USB_Config_s *cfg)
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
    local_cfg.rx_callback = MediaUsbSimpleRxHook; /* 保证接收统一进 comm 层接收入口 */
    local_cfg.parent = media;                     /* 反向指针：接收钩子据此取回 media（USBConfig 写入实例） */
    if (USBConfig(usb, &local_cfg) != 0)
        return -1;
    return 0;
}

#endif /* DRV_COMM_USED */
