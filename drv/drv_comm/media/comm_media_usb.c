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
 */

#include "comm_media_usb.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include <string.h>

#ifdef DRV_COMM_USED

/* 单 USB 包数据片上限 = 64 - 1（分包序号），与 CDC FS 最大包长（USB_TX_BUF_SIZE）对齐 */
#define USB_MEDIA_PAYLOAD_PER_PKT (USB_TX_BUF_SIZE - 1) /* 63 */

static int8_t MediaUsbSend(CommMedia *media, const uint8_t *data);
static void MediaUsbRxHook(USBInstance *usb);
static void MediaUsbFrameComplete(CommMediaUsb *m);

static const CommMediaVTable_s s_usb_vtable = {
    .send = MediaUsbSend,
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
    uint8_t pkt[USB_TX_BUF_SIZE]; /* 单包暂存 = [pkt_idx][数据片] */

    /* 先判空再解引用（m==NULL 时不能先访问 m->base.media） */
    if (m == NULL || data == NULL || m->tx_frame_len == 0)
        return -1;
    usb = (USBInstance *)m->base.media;
    if (usb == NULL)
        return -1;

    /* 分包发送：整帧 = data，tx_frame_len > 63B 时拆成多包，seq = 该片在整个帧的第几包（0 起） */
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
    if (USBRegister(usb) != 0)
        return -1;

    media->base.vtable = &s_usb_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    usb->parent = media; /* 反向指针：适配钩子据此取回 media */

    /* 清接收累积与分包序号状态（初始：期望首包 pkt_idx=0） */
    media->rx_cnt = 0;
    media->rx_expect_pkt = 0;
    media->lost_frames = 0;
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
    local_cfg.rx_callback = MediaUsbRxHook; /* 保证接收统一进 comm 层接收入口 */
    local_cfg.parent = media;               /* 反向指针：接收钩子据此取回 media（USBConfig 写入实例） */
    if (USBConfig(usb, &local_cfg) != 0)
        return -1;
    return 0;
}

#endif /* DRV_COMM_USED */
