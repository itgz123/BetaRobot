/**
 * @file comm_ring.c
 * @brief 无锁环形 chunk 缓冲实现
 *
 * 单核 Cortex-M 下用临界区保证多生产者（多个外设 ISR）安全；
 * 数据写入顺序先于 head 更新，消费者读 tail 处数据，读后更新 tail。
 */

#include "comm_ring.h"
#include "drv_comm.h"

#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

void CommRingInit(CommRing *ring)
{
    if (!ring)
    {
        return;
    }
    ring->head = 0;
    ring->tail = 0;
}

uint8_t CommRingPush(CommRing *ring, uint32_t media_id, const uint8_t *data, uint16_t len)
{
    if (!ring || !data || len == 0 || len > COMM_RING_CHUNK_SIZE)
    {
        return 0;
    }

    UBaseType_t pm = taskENTER_CRITICAL_FROM_ISR();
    uint16_t next = (uint16_t)((ring->head + 1) % COMM_RING_CHUNK_NUM);
    if (next == ring->tail)
    {
        taskEXIT_CRITICAL_FROM_ISR(pm); /* 满：丢弃 */
        return 0;
    }
    CommRingChunk_s *c = &ring->chunks[ring->head];
    memcpy(c->data, data, len);
    c->len = len;
    c->media_id = media_id;
    ring->head = next; /* 数据先于 head 更新 */
    taskEXIT_CRITICAL_FROM_ISR(pm);
    return 1;
}

int8_t CommRingPop(CommRing *ring, uint32_t *media_id, uint8_t *out, uint16_t *len)
{
    if (!ring || !out || !len)
    {
        return -1;
    }
    if (ring->head == ring->tail)
    {
        return 0; /* 空 */
    }
    CommRingChunk_s *c = &ring->chunks[ring->tail];
    memcpy(out, c->data, c->len);
    *len = c->len;
    if (media_id)
    {
        *media_id = c->media_id;
    }
    ring->tail = (uint16_t)((ring->tail + 1) % COMM_RING_CHUNK_NUM);
    return 1;
}

uint16_t CommRingCount(CommRing *ring)
{
    if (!ring)
    {
        return 0;
    }
    return (uint16_t)((ring->head + COMM_RING_CHUNK_NUM - ring->tail) % COMM_RING_CHUNK_NUM);
}
