/**
 * @file comm_proto.c
 * @brief 通信框架-协议层（Protocol）基类实现
 *
 * ProtoSend 统一"pack 打包 → 交给 media 发送"；ProtoPack/ProtoUnpack/ProtoReset
 * 为 vtable 分发封装（static inline，见头文件）。协议具体打包/解包由后端实现
 * （如空协议 CommProtoRaw）。
 */

#include "comm_proto.h"
#include "comm_media.h"

#ifdef DRV_COMM_USED

int8_t ProtoSend(CommProto *self, const uint8_t *payload)
{
    CommMedia *media;

    if (!self || !self->vtable || !self->vtable->pack)
        return -1;
    if (self->vtable->pack(self, payload) != 0)
        return -1;

    media = (CommMedia *)self->media;
    if (media == NULL)
        return -1;
    return MediaSend(media, self->tx_buff);
}

#endif /* DRV_COMM_USED */
