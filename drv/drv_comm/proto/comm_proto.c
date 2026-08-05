/**
 * @file comm_proto.c
 * @brief 协议层基类实现：注册表、统一喂 chunk、统一打包入口
 *
 * ProtocolFeed 成功解析一帧后，自动填充来源（media_id/proto_id）并喂 daemon。
 */

#include "comm_proto.h"
#include "drv_comm.h"

#include "bsp_uart_log.h"

static CommProto *s_proto_list[PROTO_INSTANCE_NUM] = {NULL};
static uint8_t s_proto_idx = 0;

int8_t ProtocolRegister(CommProto *inst)
{
    if (!inst)
    {
        LOGERROR("[comm_proto] inst is NULL!");
        return -1;
    }
    if (s_proto_idx >= PROTO_INSTANCE_NUM)
    {
        LOGERROR("[comm_proto] exceeded max instance count!");
        return -1;
    }
    for (uint8_t i = 0; i < s_proto_idx; i++)
    {
        if (s_proto_list[i] == inst)
        {
            LOGERROR("[comm_proto] instance already registered!");
            return -1;
        }
    }
    s_proto_list[s_proto_idx++] = inst;
    return 0;
}

int8_t ProtocolConfig(CommProto *inst, uint16_t proto_id, uint16_t max_payload, DaemonInstance *daemon)
{
    if (!inst)
    {
        return -1;
    }
    inst->proto_id = proto_id;
    inst->max_payload = max_payload;
    inst->daemon = daemon;
    return 0;
}

ProtoParseResult_e ProtocolFeed(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg, uint32_t media_id)
{
    if (!inst || !inst->vtable || !inst->vtable->unpack || !data || len == 0 || !msg)
    {
        return PROTO_PARSE_DROP;
    }
    ProtoParseResult_e r = inst->vtable->unpack(inst, data, len, msg);
    if (r == PROTO_PARSE_OK)
    {
        msg->media_id = media_id;
        msg->proto_id = inst->proto_id;
        msg->timestamp_us = 0; /* 由协议实现填充，此处不覆盖 */
        if (inst->daemon)
        {
            DaemonReload(inst->daemon);
        }
    }
    return r;
}

int16_t ProtocolPack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf)
{
    if (!inst || !inst->vtable || !inst->vtable->pack || !out_buf)
    {
        return -1;
    }
    return inst->vtable->pack(inst, comm_id, payload, len, out_buf);
}
