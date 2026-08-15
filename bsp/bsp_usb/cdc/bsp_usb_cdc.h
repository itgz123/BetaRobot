/**
 * @file bsp_usb_cdc.h
 * @brief CDC-ACM 虚拟串口类（纯 C，参考 XRUSB cdc_base/cdc_uart）
 *
 * 端点分配（配置描述符一致）：
 *   - EP1 OUT 0x01 BULK 64：主机->设备数据（RX，双缓冲）
 *   - EP1 IN  0x81 BULK 64：设备->主机数据（TX，op 环 + 单 op 状态机）
 *   - EP2 IN  0x82 INT  16：SerialState 通知
 *
 * 数据通路不变式：
 *   - RX：仅在 ring free >= 64 时 rearm EP1 OUT，保证完成回调 push 必然成功；
 *     ring 满时 rx_pause=1，应用 USBReceive 取走后恢复 rearm。
 *   - TX：应用 push op 入环，发送状态机一次只处理一个 op，跨块分包，
 *     op.len % 64 == 0 且 > 0 时末尾补 ZLP。
 */

#ifndef __BSP_USB_CDC_H
#define __BSP_USB_CDC_H

#include "stdint.h"
#include "bsp_usb_types.h"
#include "bsp_usb_ring.h"
#include "bsp_usb_core.h" /* USBClassVTable_t */

typedef struct USBInstance USBInstance;

/*------------- CDC-ACM 类实例 --------------*/
typedef struct USBCDC
{
    USBInstance *inst; /* 反向指针（bind 时填充，供回调访问） */

    USB_LineCoding_t line_coding;       /* 默认 115200/8N1 */
    USBCDCControlBit_e ctrl_line_state; /* DTR/RTS 位图 */
    uint8_t configured;                 /* bind 后置 1，unbind/Reset 清 0 */
    uint8_t bound;                      /* bind 幂等标志 */

    /* 端点（bind 时经 USB_EPPoolGet 领取） */
    USBEndpoint *ep_data_in;
    USBEndpoint *ep_data_out;
    USBEndpoint *ep_comm_in;

    /* 描述符块（bind 时填充接口号 + 端点地址，供配置描述符拼接） */
    USB_CDCDescBlock_t desc_block;

    /* RX 通路 */
    USBByteRing_t rx_ring; /* 应用读取（USBReceive） */
    uint8_t rx_pause;      /* 背压：ring 满时暂停 rearm */

    /* TX 通路 */
    USBOpRing_t tx_ops;    /* {data,len} op 环 */
    const uint8_t *tx_cur; /* 当前 op 数据指针 */
    uint16_t tx_cur_len;   /* 当前 op 总长 */
    uint16_t tx_op_sent;   /* 当前 op 已发字节 */
    uint8_t tx_active;     /* 当前有 op 在发 */
    uint8_t tx_need_zlp;   /* 当前 op 需补 ZLP */

    /* 当前 op 完成回调（USB_CDCKick pop 时从 op 拷贝，发送完成时调用；
     * 对照 XRUSB WritePort::Finish 上报一次发送完成） */
    void (*tx_done_cb)(void *ctx, uint16_t len);
    void *tx_done_ctx;

    /* 类回调（USBConfig 注册，可为 NULL） */
    void (*line_coding_cb)(USBInstance *inst, const USB_LineCoding_t *lc);
    void (*ctrl_line_cb)(USBInstance *inst, USBCDCControlBit_e state);
} USBCDC;

/*------------- API --------------*/

/** @brief 获取 CDC 类 vtable（挂在 USBInstance.class_vtable 上） */
const USBClassVTable_t *USB_CDCVTable(void);

/** @brief USB Reset：清 CDC 状态，不触碰端点（由 bsp_usb.c 在 Deinit/Init 之间调用） */
void USB_CDCReset(USBInstance *inst);

/** @brief EP1 OUT 完成（RX 入环 + 背压 rearm），绑定为 ep[1][OUT].on_complete */
void USB_CDCOnEPOut(USBEndpoint *ep, uint32_t actual_len);

/** @brief EP1 IN 完成（TX 续发），绑定为 ep[1][IN].on_complete */
void USB_CDCOnEPIn(USBEndpoint *ep, uint32_t actual_len);

/** @brief 发送（应用层，非阻塞）；满返回 -1，否则 0 */
int32_t USB_CDCTransmit(USBInstance *inst, const uint8_t *data, uint16_t len);

/**
 * @brief 带完成回调的发送（对照 XRUSB WritePort::Finish）
 * @param inst     实例指针
 * @param data     数据源（须在发送完成前有效）
 * @param len      长度
 * @param on_done  发送完成回调（可为 NULL）：该 op 全部数据（含 ZLP）发完后调用一次，
 *                 参数 = 完成回调上下文 + 已发字节数；此时 data 缓冲可安全复用
 * @param done_ctx 回调上下文（通常传 inst）
 * @retval 0 入队成功；-1 失败（未配置 / op 环满）
 */
int32_t USB_CDCTransmitEx(USBInstance *inst, const uint8_t *data, uint16_t len,
                          void (*on_done)(void *ctx, uint16_t len), void *done_ctx);

/** @brief 接收（应用层）：从 RX 环取，取后恢复背压；返回实际长度 */
int32_t USB_CDCReceive(USBInstance *inst, uint8_t *data, uint16_t len);

/**
 * @brief 经通信 INTERRUPT IN 端点（EP2 IN）发送串行状态通知
 *        （对照 XRUSB CDCBase::SendSerialState）
 * @param inst 实例指针
 * @retval 0 提交成功；-1 失败（未配置 / 端点忙）
 * @note SET_CONTROL_LINE_STATE 时会自动调用；应用亦可主动触发
 *       （如收到 DTR 后上报载波状态）。DTR 有效时 serialState=0x03（DCD+DSR）。
 */
int32_t USB_CDCSendSerialState(USBInstance *inst);

#endif /* __BSP_USB_CDC_H */
