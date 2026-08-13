/**
 * @file comm_proto.c
 * @brief 通信框架-协议层（Protocol）基类实现
 *
 * 纯 vtable 分发封装（头文件只声明）。proto 基类本身无状态，
 * 具体打包/解包由后端实现（如空协议 CommProtoRaw）。
 */

#include "comm_proto.h"
/* 内置协议后端（预注册进注册表） */
#include "comm_proto_raw.h"
#include "comm_proto_custom.h"

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

/*------------- 协议后端注册表 -------------*/

/* 内置协议 Init 的 void* 封装（后端描述符 init 签名统一 void*，避免函数指针类型不匹配） */
static int8_t BackendRawInit(void *proto)
{
    return CommProtoRawInit((CommProtoRaw *)proto);
}

static int8_t BackendCustomInit(void *proto)
{
    return CommProtoCustomInit((CommProtoCustom *)proto);
}

/* 注册表：内置 RAW/CUSTOM 静态预注册，app 自定义协议经 CommProtoRegisterBackend 追加 */
static CommProtoBackend_t s_backends[COMM_PROTO_BACKEND_MAX] = {
    {PROTO_RAW, BackendRawInit},
    {PROTO_CUSTOM, BackendCustomInit},
};
static uint8_t s_backend_cnt = 2;

int8_t CommProtoRegisterBackend(const CommProtoBackend_t *backend)
{
    uint8_t i;

    if (backend == NULL || backend->init == NULL)
        return -1;
    if (backend->type < PROTO_USER)
        return -1; /* 自定义协议 id 须 >= PROTO_USER（不得覆盖内置） */
    for (i = 0; i < s_backend_cnt; i++)
        if (s_backends[i].type == backend->type)
            return -1; /* 重复注册 */
    if (s_backend_cnt >= COMM_PROTO_BACKEND_MAX)
        return -1; /* 表满 */
    s_backends[s_backend_cnt++] = *backend;
    return 0;
}

const CommProtoBackend_t *CommProtoBackendFind(ProtocolType_e type)
{
    uint8_t i;

    for (i = 0; i < s_backend_cnt; i++)
        if (s_backends[i].type == type)
            return &s_backends[i];
    return NULL;
}

#endif /* DRV_COMM_USED */
