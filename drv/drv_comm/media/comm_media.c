/**
 * @file comm_media.c
 * @brief 通信框架-硬件层（Media）基类实现
 *
 * media 基类只提供统一发送分发（vtable 转发）。
 * 接收分发是 comm 层的职责：media 后端适配钩子收到完整数据单元后，
 * 直接调用 comm 层接收入口 CommMediaRxHook（经 media->parent 反查），
 * 不再经过 media 基类中转，接收链只有 bsp → media 子类 → comm。
 */

#include "comm_media.h"

#ifdef DRV_COMM_USED

int8_t MediaSend(CommMedia *media, const uint8_t *data)
{
    if (!media || !media->vtable || !media->vtable->send)
        return -1;
    return media->vtable->send(media, data); /* 后端实现：CAN 拆帧/组头，UART 直发 */
}

#endif /* DRV_COMM_USED */
