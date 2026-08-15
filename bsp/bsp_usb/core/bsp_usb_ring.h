/**
 * @file bsp_usb_ring.h
 * @brief USB 数据通路环形队列：RX 字节环 + TX op 环（无锁，单生产者/单消费者）
 *
 *   - RX 字节环：ISR（HAL_PCD_DataOutStage）写 head，应用层（USBReceive）读 tail。
 *   - TX op 环：应用层（USBTransmit）push op（head++），发送状态机（ISR）pop op（tail++）。
 * head/tail 单调递增（不掩码），capacity/size 不必为 2 的幂；总操作次数 < 2^32。
 */

#ifndef __BSP_USB_RING_H
#define __BSP_USB_RING_H

#include "stdint.h"
#include "bsp_usb_types.h"

/*------------- RX 字节环 --------------*/
typedef struct
{
    uint8_t *buff;
    uint32_t size; /* 容量 */
    uint32_t head; /* 写索引（ISR） */
    uint32_t tail; /* 读索引（app） */
} USBByteRing_t;

void USBRingByteInit(USBByteRing_t *ring, uint8_t *buff, uint32_t size);
uint32_t USBRingByteUsed(USBByteRing_t *ring);
uint32_t USBRingByteFree(USBByteRing_t *ring);
int32_t USBRingBytePush(USBByteRing_t *ring, const uint8_t *data, uint32_t len); /* 满返回 -1 */
uint32_t USBRingBytePop(USBByteRing_t *ring, uint8_t *data, uint32_t len);
uint32_t USBRingBytePeek(const USBByteRing_t *ring, uint8_t *data, uint32_t len);

/*------------- TX op 环 --------------*/
typedef struct
{
    const uint8_t *data;                      /* 待发送数据源（应用缓冲，须在发送完成前有效） */
    uint16_t len;                             /* 总长度 */
    void (*on_done)(void *ctx, uint16_t len); /* 可选：该 op 发送完成回调（NULL=无；
                                                 对照 XRUSB WritePort::Finish 每次上报） */
    void *done_ctx;                           /* 完成回调上下文（通常传 USBInstance） */
} USB_TXOp_t;

typedef struct
{
    USB_TXOp_t *ops;   /* op 数组 */
    uint32_t capacity; /* op 数量 */
    uint32_t head;     /* 写索引（app push） */
    uint32_t tail;     /* 读索引（ISR pop） */
} USBOpRing_t;

void USBOpRingInit(USBOpRing_t *ring, USB_TXOp_t *ops, uint32_t capacity);
uint32_t USBOpRingUsed(const USBOpRing_t *ring);
uint32_t USBOpRingFree(const USBOpRing_t *ring);
int32_t USBOpRingPush(USBOpRing_t *ring, const USB_TXOp_t *op); /* 满返回 -1 */
int32_t USBOpRingPop(USBOpRing_t *ring, USB_TXOp_t *op);        /* 空返回 -1 */

#endif /* __BSP_USB_RING_H */
