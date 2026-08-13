/**
 * @file comm_proto_raw.c
 * @brief 通信框架-协议层（Protocol）空协议后端实现
 *
 * 空协议：payload 占 100%，无任何协议开销。
 *   - pack：payload 直接拷入 tx_buff（= 整帧，长度 = payload_size）
 *   - unpack：data 即完整 payload（固定长度），直接调 on_frame 回调
 *   - reset：无状态，空操作
 */

#include "comm_proto_raw.h"
#include <string.h>

#ifdef DRV_COMM_USED

static int8_t CommProtoRawPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff)
{
    if (self == NULL || payload == NULL || out_buff == NULL || self->payload_size == 0)
        return -1;
    /* 空协议无开销：payload 直接拷入 comm 层打包缓冲（整帧） */
    memcpy(out_buff, payload, self->payload_size);
    return 0;
}

/* 只解包：空协议 data 即完整 payload，直接返回（on_frame 由 comm 层统一调） */
static const uint8_t *CommProtoRawUnpack(CommProto *self, const uint8_t *data)
{
    if (self == NULL || data == NULL)
        return NULL;
    return data; /* 长度已由 media 后端校验，拿到的总是完整一帧 */
}

static void CommProtoRawReset(CommProto *self)
{
    (void)self; /* 空协议无状态 */
}

static const CommProtoVTable_s s_raw_vtable = {
    .pack = CommProtoRawPack,
    .unpack = CommProtoRawUnpack,
    .reset = CommProtoRawReset,
};

int8_t CommProtoRawInit(CommProtoRaw *proto)
{
    if (proto == NULL || proto->base.media == NULL)
        return -1;
    proto->base.vtable = &s_raw_vtable;
    return 0;
}

#endif /* DRV_COMM_USED */
