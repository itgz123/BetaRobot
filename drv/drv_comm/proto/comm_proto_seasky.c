/**
 * @file comm_proto_seasky.c
 * @brief 视觉（seasky）协议插件实现
 */

#include "comm_proto_seasky.h"
#ifdef DRV_COMM_USED
#include "drv_comm.h"

#include "bsp_crc.h"
#include "bsp_uart_log.h"
#include <string.h>

enum
{
    SEASKY_ST_WAIT_SOF = 0,
    SEASKY_ST_COLLECT,
};

static int16_t SeaskyPack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf)
{
    (void)inst;
    (void)comm_id;
    if (len > inst->max_payload)
    {
        return -1;
    }
    uint16_t frame_len = 2 + len + 2; /* 0x5A + 0x00 + data + CRC16 */
    out_buf[0] = SEASKY_SOF_TX;
    out_buf[1] = 0x00;
    if (payload && len)
    {
        memcpy(&out_buf[2], payload, len);
    }
    uint16_t crc = BSP_CRC16(out_buf, 2 + len, 0xFFFF);
    out_buf[2 + len] = (uint8_t)(crc & 0xFF);
    out_buf[2 + len + 1] = (uint8_t)(crc >> 8);
    return (int16_t)frame_len;
}

static ProtoParseResult_e SeaskyUnpack(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg)
{
    CommProtoSeasky *ps = COMM_CONTAINER_OF(inst, CommProtoSeasky);

    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        switch (ps->state)
        {
        case SEASKY_ST_WAIT_SOF:
            if (b == SEASKY_SOF_RX)
            {
                ps->rx_buff[0] = b;
                ps->rx_len = 1;
                ps->state = SEASKY_ST_COLLECT;
            }
            break;

        case SEASKY_ST_COLLECT:
            ps->rx_buff[ps->rx_len++] = b;
            if (ps->rx_len == SEASKY_RX_FRAME_SIZE)
            {
                uint16_t crc = BSP_CRC16(ps->rx_buff, SEASKY_RX_FRAME_SIZE - 2, 0xFFFF);
                uint16_t rx = (uint16_t)ps->rx_buff[SEASKY_RX_FRAME_SIZE - 2] |
                              ((uint16_t)ps->rx_buff[SEASKY_RX_FRAME_SIZE - 1] << 8);
                ps->state = SEASKY_ST_WAIT_SOF;
                ps->rx_len = 0;
                if (crc == rx)
                {
                    msg->comm_id = 0;
                    msg->payload = &ps->rx_buff[1];
                    msg->len = SEASKY_RX_FRAME_SIZE - 3; /* 数据：byte1..speed，7 字节 */
                    msg->ctx = ps;
                    return PROTO_PARSE_OK;
                }
            }
            break;

        default:
            ps->state = SEASKY_ST_WAIT_SOF;
            ps->rx_len = 0;
            break;
        }
    }
    return PROTO_PARSE_NEED_MORE;
}

static int16_t SeaskyFindHeader(CommProto *inst, const uint8_t *data, uint16_t len)
{
    (void)inst;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] == SEASKY_SOF_RX)
        {
            return (int16_t)i;
        }
    }
    return -1;
}

static uint16_t SeaskyCalcLen(CommProto *inst, const uint8_t *buf)
{
    (void)inst;
    (void)buf;
    return SEASKY_RX_FRAME_SIZE;
}

static uint8_t SeaskyVerifyCrc(CommProto *inst, const uint8_t *buf, uint16_t len)
{
    (void)inst;
    if (len < SEASKY_RX_FRAME_SIZE)
    {
        return 0;
    }
    uint16_t crc = BSP_CRC16(buf, SEASKY_RX_FRAME_SIZE - 2, 0xFFFF);
    uint16_t rx = (uint16_t)buf[SEASKY_RX_FRAME_SIZE - 2] | ((uint16_t)buf[SEASKY_RX_FRAME_SIZE - 1] << 8);
    return (crc == rx) ? 1 : 0;
}

static void SeaskyReset(CommProto *inst)
{
    CommProtoSeasky *ps = COMM_CONTAINER_OF(inst, CommProtoSeasky);
    ps->state = SEASKY_ST_WAIT_SOF;
    ps->rx_len = 0;
}

static const CommProtoVTable_s s_proto_seasky_vtable = {
    .pack = SeaskyPack,
    .unpack = SeaskyUnpack,
    .find_header = SeaskyFindHeader,
    .calc_len = SeaskyCalcLen,
    .verify_crc = SeaskyVerifyCrc,
    .reset = SeaskyReset,
};

int8_t CommProtoSeaskyConfig(CommProto *inst, const CommProtoSeasky_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        LOGERROR("[comm_proto_seasky] inst or cfg is NULL!");
        return -1;
    }
    inst->vtable = &s_proto_seasky_vtable;
    inst->proto_id = cfg->proto_id;
    inst->max_payload = SEASKY_RX_FRAME_SIZE;
    inst->daemon = cfg->daemon;
    inst->is_frame_protocol = 0;
    SeaskyReset(inst);
    return 0;
}
#endif /* DRV_COMM_USED */
