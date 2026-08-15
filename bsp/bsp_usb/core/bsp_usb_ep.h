/**
 * @file bsp_usb_ep.h
 * @brief USB 端点对象：open/close/transfer/stall + 双缓冲映射（纯 C，包 HAL_PCD_EP_*）
 *
 * 依赖方向：core → ep → HAL。ep 层不感知实例结构（USBInstance），
 * 只通过 ctx 透传回调上下文；PCD 句柄由调用方（bsp_usb.c / core）传入。
 *
 * 双缓冲语义（对照 XRUSB DoubleBuffer）：
 *   - IN  端点：Transfer 时取 buf[active] 提交 HAL，随后 active^=1（发送完成块变为
 *     Pending）。完成回调里 USB_EPPendingBuffer 即刚发出的数据。
 *   - OUT 端点：Transfer 时取 buf[active] 提交 HAL，不切换；完成回调里 active^=1，
 *     USB_EPPendingBuffer 即刚接收的数据。
 */

#ifndef __BSP_USB_EP_H
#define __BSP_USB_EP_H

#include "stdint.h"
#include "main.h" /* PCD_HandleTypeDef */
#include "bsp_usb_types.h"

struct USBEndpoint
{
    uint8_t number;                                            /* EP 号（0..USB_EP_MAX-1） */
    USBEPDir_e dir;                                            /* USB_EP_DIR_OUT / USB_EP_DIR_IN */
    USBEPType_e type;                                          /* USB_EP_TYPE_* */
    uint16_t max_packet;                                       /* 最大包长 */
    USBEPState_e state;                                        /* USB_EP_STATE_* */
    uint8_t *buf[2];                                           /* 数据缓冲（双缓冲两半；单缓冲 buf[1]=NULL） */
    uint16_t buf_size;                                         /* 单块缓冲大小 */
    uint8_t double_buf;                                        /* 1=双缓冲 */
    uint8_t active;                                            /* 当前活动块索引（0/1） */
    uint16_t last_len;                                         /* 最近一次 Transfer 的长度（IN 回调用） */
    void *ctx;                                                 /* 回调上下文（USBInstance*） */
    void (*on_complete)(USBEndpoint *ep, uint32_t actual_len); /* 传输完成回调 */
};

/*------------- 端点池（对照 XRUSB EndpointPool） --------------
 * 位图记录各端点号/方向占用，类在 bind 时按需领取端点号
 * （USB_EP_NUM_AUTO 自动分配；EP0 不参与分配）。内存仍是静态数组。
 */
typedef struct
{
    uint32_t used; /* 位图：bit[num*2 + dir] = 占用 */
} USB_EPPool_t;

void USB_EPPoolInit(USB_EPPool_t *pool);

/**
 * @brief 领取一个数据端点（EP0 除外）
 * @param pool   端点池
 * @param ep_tab 实例端点表 inst->ep（按 USB_EP_MAX 行排列）
 * @param num    指定端点号；USB_EP_NUM_AUTO 自动分配
 * @param dir    方向
 * @param out    输出领取到的端点指针
 * @retval 0 成功；-1 失败（非法号 / 已占用 / 池满）
 */
int8_t USB_EPPoolGet(USB_EPPool_t *pool, USBEndpoint ep_tab[][2], uint8_t num,
                     USBEPDir_e dir, USBEndpoint **out);

/**
 * @brief 释放端点（清除占用位）
 */
void USB_EPPoolRelease(USB_EPPool_t *pool, USBEndpoint *ep);

/*------------- 端点操作（HAL 层） --------------*/

void USB_EPInit(USBEndpoint *ep, uint8_t number, USBEPDir_e dir, uint8_t *buf0, uint8_t *buf1,
                uint16_t buf_size, uint8_t double_buf);
int8_t USB_EPConfigure(PCD_HandleTypeDef *hpcd, USBEndpoint *ep, USBEPType_e type,
                       uint16_t max_packet);
void USB_EPClose(PCD_HandleTypeDef *hpcd, USBEndpoint *ep);
int8_t USB_EPTransfer(PCD_HandleTypeDef *hpcd, USBEndpoint *ep, uint16_t size);
int8_t USB_EPTransferZLP(PCD_HandleTypeDef *hpcd, USBEndpoint *ep);
int8_t USB_EPStall(PCD_HandleTypeDef *hpcd, USBEndpoint *ep);
int8_t USB_EPUnstall(PCD_HandleTypeDef *hpcd, USBEndpoint *ep); /* 不与 LL USB_EPClearStall 撞名 */

/*------------- 端点状态/缓冲查询 --------------*/

static inline uint16_t USB_EPMaxTransferSize(const USBEndpoint *ep)
{
    /* EP0 单包 = max_packet；非 EP0 = 整块缓冲（多包 bulk 由上层状态机分包） */
    return (ep->number == 0) ? ep->max_packet : ep->buf_size;
}

static inline uint8_t *USB_EPActiveBuffer(const USBEndpoint *ep)
{
    return ep->buf[ep->active];
}

static inline uint8_t *USB_EPPendingBuffer(const USBEndpoint *ep)
{
    return ep->buf[1 - ep->active];
}

static inline uint8_t USB_EPIsBusy(const USBEndpoint *ep)
{
    return ep->state == USB_EP_STATE_BUSY;
}

static inline uint8_t USB_EPIsStalled(const USBEndpoint *ep)
{
    return ep->state == USB_EP_STATE_STALLED;
}

static inline void USB_EPSwitchBuffer(USBEndpoint *ep)
{
    ep->active ^= 1;
}

static inline uint8_t *USB_EPGetAddr(const USBEndpoint *ep)
{
    return USB_EPActiveBuffer(ep);
}

/*------------- 完成处理（由 bsp_usb.c 的 HAL 回调调用） --------------*/

void USB_EPOnTransferComplete(USBEndpoint *ep, uint32_t actual_len);

#endif /* __BSP_USB_EP_H */
