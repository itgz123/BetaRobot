/**
 * @file bsp_usb_ring.c
 * @brief 环形队列实现（见 bsp_usb_ring.h）
 *
 * 索引单调递增，环形下标用 % size。满判定预留 1 个空位，
 * 使 head==tail 唯一表示"空"。
 */

#include "bsp_usb_ring.h"
#include "string.h"

/*------------- RX 字节环 --------------*/

void USBRingByteInit(USBByteRing_t *ring, uint8_t *buff, uint32_t size)
{
    ring->buff = buff;
    ring->size = size;
    ring->head = 0;
    ring->tail = 0;
}

uint32_t USBRingByteUsed(USBByteRing_t *ring)
{
    return ring->head - ring->tail;
}

uint32_t USBRingByteFree(USBByteRing_t *ring)
{
    return ring->size - 1 - USBRingByteUsed(ring);
}

int32_t USBRingBytePush(USBByteRing_t *ring, const uint8_t *data, uint32_t len)
{
    if (len > USBRingByteFree(ring))
    {
        return -1;
    }

    uint32_t h = ring->head % ring->size;
    uint32_t first = ring->size - h;
    if (first > len)
    {
        first = len;
    }

    memcpy(&ring->buff[h], data, first);
    if (len > first)
    {
        memcpy(&ring->buff[0], data + first, len - first);
    }

    ring->head += len;
    return 0;
}

uint32_t USBRingBytePop(USBByteRing_t *ring, uint8_t *data, uint32_t len)
{
    uint32_t used = USBRingByteUsed(ring);
    if (len > used)
    {
        len = used;
    }

    uint32_t t = ring->tail % ring->size;
    uint32_t first = ring->size - t;
    if (first > len)
    {
        first = len;
    }

    memcpy(data, &ring->buff[t], first);
    if (len > first)
    {
        memcpy(data + first, &ring->buff[0], len - first);
    }

    ring->tail += len;
    return len;
}

uint32_t USBRingBytePeek(const USBByteRing_t *ring, uint8_t *data, uint32_t len)
{
    uint32_t used = USBRingByteUsed((USBByteRing_t *)ring);
    if (len > used)
    {
        len = used;
    }

    uint32_t t = ring->tail % ring->size;
    uint32_t first = ring->size - t;
    if (first > len)
    {
        first = len;
    }

    memcpy(data, &ring->buff[t], first);
    if (len > first)
    {
        memcpy(data + first, &ring->buff[0], len - first);
    }

    return len;
}

/*------------- TX op 环 --------------*/

void USBOpRingInit(USBOpRing_t *ring, USB_TXOp_t *ops, uint32_t capacity)
{
    ring->ops = ops;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
}

uint32_t USBOpRingUsed(const USBOpRing_t *ring)
{
    return ring->head - ring->tail;
}

uint32_t USBOpRingFree(const USBOpRing_t *ring)
{
    return ring->capacity - 1 - USBOpRingUsed(ring);
}

int32_t USBOpRingPush(USBOpRing_t *ring, const USB_TXOp_t *op)
{
    if (USBOpRingFree(ring) == 0)
    {
        return -1;
    }

    ring->ops[ring->head % ring->capacity] = *op;
    ring->head++;
    return 0;
}

int32_t USBOpRingPop(USBOpRing_t *ring, USB_TXOp_t *op)
{
    if (USBOpRingUsed(ring) == 0)
    {
        return -1;
    }

    *op = ring->ops[ring->tail % ring->capacity];
    ring->tail++;
    return 0;
}
