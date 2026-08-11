/**
 * @file comm_media.c
 * @brief 通信框架-硬件层（Media）基类实现
 *
 * MediaHandleRx 只负责"收到数据 → 触发 rx_cb"（comm 层设置）。
 * 接收分发是 comm 层的职责（按 unpack_mode 分流），media 基类零依赖、
 * 可独立编译，与协议层完全解耦。
 */

#include "comm_media.h"
#include "bsp_uart_log.h"

#ifdef DRV_COMM_USED

void MediaHandleRx(CommMedia *media, const uint8_t *data)
{
    if (!media || !data)
        return;
    if (media->rx_cb)
    {
        media->rx_cb(media, data); /* comm 层挂的钩子：分流解包 */
        return;
    }
    /* 未挂接收钩子：丢弃（comm 层应设置 rx_cb） */
    LOGWARNING("[media] rx_cb not set, drop");
}

int8_t MediaSend(CommMedia *media, const uint8_t *data)
{
    if (!media || !media->vtable || !media->vtable->send)
        return -1;
    return media->vtable->send(media, data); /* 后端实现：CAN 拆帧/组头，UART 直发 */
}

#endif /* DRV_COMM_USED */
