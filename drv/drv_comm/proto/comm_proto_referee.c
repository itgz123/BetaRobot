/**
 * @file comm_proto_referee.c
 * @brief 裁判系统协议插件实现
 *
 * CRC 说明：
 *   - 帧头 CRC8 为裁判专用（非反射，poly 0x31，init 0xFF），
 *     与 bsp_crc 的 CRC-8/MAXIM（反射）不同，此处独立实现。
 *   - 帧尾 CRC16 为 CCITT（poly 0x1021，init 0xFFFF），与 BSP_CRC16 一致。
 * 多帧粘包：EMIT 后回到 WAIT_SOF，剩余字节若为 0xA5 则继续解帧。
 */

#include "comm_proto_referee.h"
#include "drv_comm.h"

#include "bsp_crc.h"
#include "bsp_uart_log.h"
#include <string.h>

enum
{
    REFEREE_ST_WAIT_SOF = 0,
    REFEREE_ST_COLLECT_HEAD,
    REFEREE_ST_COLLECT_DATA,
};

/* 裁判专用 CRC8：非反射 poly 0x31，init 0xFF */
static uint8_t RefereeCrc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t RefereeReadLen(const uint8_t *buf) /* buf 指向 SOF */
{
    return (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
}

static int16_t RefereePack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf)
{
    (void)inst;
    if (len > inst->max_payload)
    {
        return -1;
    }
    uint16_t dl = (uint16_t)(len + REFEREE_COMM_CMDID_LEN); /* DL 含 CMD_ID */
    uint16_t frame_len = REFEREE_COMM_HEAD_LEN + dl + REFEREE_COMM_TAIL_LEN;

    out_buf[0] = REFEREE_COMM_SOF;
    out_buf[1] = (uint8_t)(dl & 0xFF);
    out_buf[2] = (uint8_t)(dl >> 8);
    out_buf[3] = 0; /* SEQ */
    out_buf[4] = RefereeCrc8(out_buf, 4); /* CRC8 对 [0..3] */
    out_buf[5] = (uint8_t)(comm_id & 0xFF);
    out_buf[6] = (uint8_t)(comm_id >> 8);
    if (payload && len)
    {
        memcpy(&out_buf[7], payload, len);
    }
    uint16_t crc = BSP_CRC16(out_buf, REFEREE_COMM_HEAD_LEN + dl, 0xFFFF);
    out_buf[frame_len - 2] = (uint8_t)(crc & 0xFF);
    out_buf[frame_len - 1] = (uint8_t)(crc >> 8);
    return (int16_t)frame_len;
}

static ProtoParseResult_e RefereeUnpack(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg)
{
    CommProtoReferee *pr = COMM_CONTAINER_OF(inst, CommProtoReferee);

    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        switch (pr->state)
        {
        case REFEREE_ST_WAIT_SOF:
            if (b == REFEREE_COMM_SOF)
            {
                pr->rx_buff[0] = b;
                pr->rx_len = 1;
                pr->state = REFEREE_ST_COLLECT_HEAD;
            }
            break;

        case REFEREE_ST_COLLECT_HEAD:
            pr->rx_buff[pr->rx_len++] = b;
            if (pr->rx_len == REFEREE_COMM_HEAD_LEN)
            {
                if (RefereeCrc8(pr->rx_buff, REFEREE_COMM_HEAD_LEN - 1) != pr->rx_buff[REFEREE_COMM_HEAD_LEN - 1])
                {
                    pr->state = REFEREE_ST_WAIT_SOF;
                    pr->rx_len = 0;
                    break;
                }
                uint16_t dl = RefereeReadLen(pr->rx_buff);
                pr->expect_frame_len = REFEREE_COMM_HEAD_LEN + dl + REFEREE_COMM_TAIL_LEN;
                if (pr->expect_frame_len > sizeof(pr->rx_buff))
                {
                    pr->state = REFEREE_ST_WAIT_SOF;
                    pr->rx_len = 0;
                    break;
                }
                pr->state = REFEREE_ST_COLLECT_DATA;
            }
            break;

        case REFEREE_ST_COLLECT_DATA:
            pr->rx_buff[pr->rx_len++] = b;
            if (pr->rx_len == pr->expect_frame_len)
            {
                uint16_t dl = RefereeReadLen(pr->rx_buff);
                uint16_t crc = BSP_CRC16(pr->rx_buff, pr->expect_frame_len - REFEREE_COMM_TAIL_LEN, 0xFFFF);
                uint16_t rx = (uint16_t)pr->rx_buff[pr->expect_frame_len - 2] |
                              ((uint16_t)pr->rx_buff[pr->expect_frame_len - 1] << 8);
                CommId_t cmd_id = (CommId_t)((uint16_t)pr->rx_buff[5] | ((uint16_t)pr->rx_buff[6] << 8));
                pr->state = REFEREE_ST_WAIT_SOF;
                pr->rx_len = 0;
                if (crc == rx)
                {
                    msg->comm_id = cmd_id;
                    msg->payload = &pr->rx_buff[REFEREE_COMM_DATA_OFFSET];
                    msg->len = (dl > REFEREE_COMM_CMDID_LEN) ? (uint16_t)(dl - REFEREE_COMM_CMDID_LEN) : 0;
                    msg->ctx = pr;
                    return PROTO_PARSE_OK;
                }
            }
            break;

        default:
            pr->state = REFEREE_ST_WAIT_SOF;
            pr->rx_len = 0;
            break;
        }
    }
    return PROTO_PARSE_NEED_MORE;
}

static int16_t RefereeFindHeader(CommProto *inst, const uint8_t *data, uint16_t len)
{
    (void)inst;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] == REFEREE_COMM_SOF)
        {
            return (int16_t)i;
        }
    }
    return -1;
}

static uint16_t RefereeCalcLen(CommProto *inst, const uint8_t *buf)
{
    (void)inst;
    return (uint16_t)(REFEREE_COMM_HEAD_LEN + RefereeReadLen(buf) + REFEREE_COMM_TAIL_LEN);
}

static uint8_t RefereeVerifyCrc(CommProto *inst, const uint8_t *buf, uint16_t len)
{
    (void)inst;
    if (len < REFEREE_COMM_HEAD_LEN + REFEREE_COMM_TAIL_LEN)
    {
        return 0;
    }
    uint16_t crc = BSP_CRC16(buf, len - REFEREE_COMM_TAIL_LEN, 0xFFFF);
    uint16_t rx = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    return (crc == rx) ? 1 : 0;
}

static void RefereeReset(CommProto *inst)
{
    CommProtoReferee *pr = COMM_CONTAINER_OF(inst, CommProtoReferee);
    pr->state = REFEREE_ST_WAIT_SOF;
    pr->rx_len = 0;
    pr->expect_frame_len = 0;
}

static const CommProtoVTable_s s_proto_referee_vtable = {
    .pack = RefereePack,
    .unpack = RefereeUnpack,
    .find_header = RefereeFindHeader,
    .calc_len = RefereeCalcLen,
    .verify_crc = RefereeVerifyCrc,
    .reset = RefereeReset,
};

int8_t CommProtoRefereeConfig(CommProto *inst, const CommProtoReferee_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        LOGERROR("[comm_proto_referee] inst or cfg is NULL!");
        return -1;
    }
    inst->vtable = &s_proto_referee_vtable;
    inst->proto_id = cfg->proto_id;
    inst->max_payload = REFEREE_COMM_MAX_FRAME - REFEREE_COMM_HEAD_LEN - REFEREE_COMM_CMDID_LEN - REFEREE_COMM_TAIL_LEN;
    inst->daemon = cfg->daemon;
    inst->is_frame_protocol = 0;
    RefereeReset(inst);
    return 0;
}
