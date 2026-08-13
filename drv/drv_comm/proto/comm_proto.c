/**
 * @file comm_proto.c
 * @brief 通信框架-协议层（Protocol）基类实现
 *
 * 纯 vtable 分发封装（头文件只声明）。proto 基类本身无状态，
 * 具体打包/解包由后端实现（如空协议 CommProtoRaw）。
 */

#include "comm_proto.h"

#ifdef DRV_COMM_USED

int8_t ProtoPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff)
{
    if (!self || !self->vtable || !self->vtable->pack)
        return -1;
    return self->vtable->pack(self, payload, out_buff);
}

/* 只解包：返回解出的 payload 指针（NULL = 丢弃）；on_frame 由 comm 层统一调 */
const uint8_t *ProtoUnpack(CommProto *self, const uint8_t *data)
{
    if (!self || !self->vtable || !self->vtable->unpack)
        return NULL;
    return self->vtable->unpack(self, data);
}

void ProtoReset(CommProto *self)
{
    if (!self || !self->vtable || !self->vtable->reset)
        return;
    self->vtable->reset(self);
}

#endif /* DRV_COMM_USED */
