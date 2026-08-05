/**
 * @file comm_proto_sbus.c
 * @brief DBUS/SBUS 协议插件实现
 *
 * SBUS 帧无 CRC，仅靠帧头 0x0F 与帧尾标志切帧；粘包/错位由状态机 + 找帧头处理。
 */

#include "comm_proto_sbus.h"
#include "drv_comm.h"

#include "bsp_uart_log.h"
#include <string.h>

enum
{
    SBUS_COMM_ST_WAIT_HEAD = 0,
    SBUS_COMM_ST_COLLECT,
};

static int16_t SbusPack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf)
{
    (void)inst;
    (void)comm_id;
    (void)out_buf;
    /* SBUS 只收不发 */
    (void)payload;
    (void)len;
    return -1;
}

static ProtoParseResult_e SbusUnpack(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg)
{
    CommProtoSbus *ps = COMM_CONTAINER_OF(inst, CommProtoSbus);

    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        switch (ps->state)
        {
        case SBUS_COMM_ST_WAIT_HEAD:
            if (b == SBUS_COMM_HEADER)
            {
                ps->rx_buff[0] = b;
                ps->rx_len = 1;
                ps->state = SBUS_COMM_ST_COLLECT;
            }
            break;

        case SBUS_COMM_ST_COLLECT:
            ps->rx_buff[ps->rx_len++] = b;
            if (ps->rx_len == SBUS_COMM_FRAME_SIZE)
            {
                uint8_t foot = ps->rx_buff[SBUS_COMM_FRAME_SIZE - 1];
                ps->state = SBUS_COMM_ST_WAIT_HEAD;
                ps->rx_len = 0;
                if (foot == 0x00 || foot == 0x04 || foot == 0x08)
                {
                    msg->comm_id = 0;
                    msg->payload = ps->rx_buff;
                    msg->len = SBUS_COMM_FRAME_SIZE;
                    msg->ctx = ps;
                    return PROTO_PARSE_OK;
                }
            }
            break;

        default:
            ps->state = SBUS_COMM_ST_WAIT_HEAD;
            ps->rx_len = 0;
            break;
        }
    }
    return PROTO_PARSE_NEED_MORE;
}

static int16_t SbusFindHeader(CommProto *inst, const uint8_t *data, uint16_t len)
{
    (void)inst;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] == SBUS_COMM_HEADER)
        {
            return (int16_t)i;
        }
    }
    return -1;
}

static uint16_t SbusCalcLen(CommProto *inst, const uint8_t *buf)
{
    (void)inst;
    (void)buf;
    return SBUS_COMM_FRAME_SIZE;
}

static uint8_t SbusVerifyCrc(CommProto *inst, const uint8_t *buf, uint16_t len)
{
    (void)inst;
    (void)buf;
    (void)len;
    return 1; /* SBUS 无 CRC */
}

static void SbusReset(CommProto *inst)
{
    CommProtoSbus *ps = COMM_CONTAINER_OF(inst, CommProtoSbus);
    ps->state = SBUS_COMM_ST_WAIT_HEAD;
    ps->rx_len = 0;
}

static const CommProtoVTable_s s_proto_sbus_vtable = {
    .pack = SbusPack,
    .unpack = SbusUnpack,
    .find_header = SbusFindHeader,
    .calc_len = SbusCalcLen,
    .verify_crc = SbusVerifyCrc,
    .reset = SbusReset,
};

int8_t CommProtoSbusConfig(CommProto *inst, const CommProtoSbus_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        LOGERROR("[comm_proto_sbus] inst or cfg is NULL!");
        return -1;
    }
    inst->vtable = &s_proto_sbus_vtable;
    inst->proto_id = cfg->proto_id;
    inst->max_payload = SBUS_COMM_FRAME_SIZE;
    inst->daemon = cfg->daemon;
    inst->is_frame_protocol = 0;
    SbusReset(inst);
    return 0;
}
