/**
 * @file comm_media.c
 * @brief 硬件层（Media）基类实现：注册表、统一发送、协议绑定、按 ID 查找
 */

#include "comm_media.h"
#ifdef DRV_COMM_USED
#include "drv_comm.h"

#include "bsp_uart_log.h"

static CommMedia *s_media_list[MEDIA_INSTANCE_NUM] = {NULL};
static uint8_t s_media_idx = 0;

int8_t MediaRegister(CommMedia *inst)
{
    if (!inst)
    {
        LOGERROR("[comm_media] inst is NULL!");
        return -1;
    }
    if (s_media_idx >= MEDIA_INSTANCE_NUM)
    {
        LOGERROR("[comm_media] exceeded max instance count!");
        return -1;
    }
    for (uint8_t i = 0; i < s_media_idx; i++)
    {
        if (s_media_list[i] == inst)
        {
            LOGERROR("[comm_media] instance already registered!");
            return -1;
        }
    }
    s_media_list[s_media_idx++] = inst;
    return 0;
}

int8_t MediaInit(CommMedia *inst)
{
    if (!inst || !inst->vtable || !inst->vtable->init)
    {
        LOGERROR("[comm_media] vtable not set, call XxxConfig first!");
        return -1;
    }
    inst->vtable->init(inst);
    return 0;
}

int8_t MediaSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!inst || !inst->vtable || !inst->vtable->send)
    {
        return -1;
    }
    return inst->vtable->send(inst, data, len, timeout_ms);
}

int8_t MediaSetRxCb(CommMedia *inst, MediaRxCallback cb)
{
    if (!inst)
    {
        return -1;
    }
    inst->rx_cb = cb;
    return 0;
}

int8_t MediaAttachProtocol(CommMedia *inst, CommProto *proto)
{
    if (!inst || !proto)
    {
        return -1;
    }
    if (inst->proto_count >= MEDIA_PROTO_MAX)
    {
        LOGERROR("[comm_media] too many protocols on one media!");
        return -1;
    }
    inst->proto_list[inst->proto_count++] = proto;
    return 0;
}

CommMedia *MediaFindById(uint8_t media_id)
{
    for (uint8_t i = 0; i < s_media_idx; i++)
    {
        if (s_media_list[i]->media_id == media_id)
        {
            return s_media_list[i];
        }
    }
    return NULL;
}
#endif /* DRV_COMM_USED */
