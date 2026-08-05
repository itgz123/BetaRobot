/**
 * @file comm_ring.h
 * @brief 无锁环形 chunk 缓冲（接收统一通道）
 *
 * 所有介质 ISR 把收到的字节段/CAN 帧作为 chunk 写入本环形缓冲，
 * 引擎 RX 任务从中取 chunk 喂给协议层。静态分配，ISR 安全。
 */

#ifndef DRV_COMM_RING_H
#define DRV_COMM_RING_H

#include "drv_comm.h"

typedef struct
{
    uint8_t data[COMM_RING_CHUNK_SIZE];
    uint16_t len;
    uint32_t media_id;
} CommRingChunk_s;

typedef struct
{
    CommRingChunk_s chunks[COMM_RING_CHUNK_NUM];
    volatile uint16_t head; /* 生产者写位置 */
    volatile uint16_t tail; /* 消费者读位置 */
} CommRing;

void CommRingInit(CommRing *ring);
uint8_t CommRingPush(CommRing *ring, uint32_t media_id, const uint8_t *data, uint16_t len);
int8_t CommRingPop(CommRing *ring, uint32_t *media_id, uint8_t *out, uint16_t *len);
uint16_t CommRingCount(CommRing *ring);

#endif /* DRV_COMM_RING_H */
