/**
 * @file comm_proto_custom.c
 * @brief 统一自定义帧协议实现
 *
 * 解包状态机（参考 xrobot Topic::Server 三态 + can_comm 拼接）：
 *   WAIT_SOF ->(0x5A)-> COLLECT_HEAD ->验HEAD_CRC8-> COLLECT_PAYLOAD ->验FRAME_CRC16-> EMIT
 *   任一步失败则丢弃当前帧，回到 WAIT_SOF 重新找帧头重同步。
 */

#include "comm_proto_custom.h"
#include "drv_comm.h"

#include "bsp_crc.h"
#include "bsp_dwt.h"
#include "bsp_uart_log.h"
#include <string.h>

enum
{
    CUSTOM_ST_WAIT_SOF = 0,
    CUSTOM_ST_COLLECT_HEAD,
    CUSTOM_ST_COLLECT_PAYLOAD,
};

/* 头部主体（不含 SOF 与 HEAD_CRC8）：COMM_ID + LEN + SEQ，CRC8 计算范围 */
#define CUSTOM_HEAD_BODY_LEN (COMM_ID_LEN + 2 + 1)

static void CustomResetState(CommProtoCustom *pc)
{
    pc->state = CUSTOM_ST_WAIT_SOF;
    pc->rx_len = 0;
    pc->expect_frame_len = 0;
}

static CommId_t ReadCommId(const uint8_t *buf)
{
    CommId_t id = 0;
    for (uint8_t i = 0; i < COMM_ID_LEN; i++)
    {
        id |= (CommId_t)buf[i] << (8 * i);
    }
    return id;
}

static uint16_t ReadPayloadLen(const uint8_t *buf) /* buf 指向 COMM_ID 字段起点 */
{
    return (uint16_t)buf[COMM_ID_LEN] | ((uint16_t)buf[COMM_ID_LEN + 1] << 8);
}

static int16_t CustomPack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf)
{
    CommProtoCustom *pc = COMM_CONTAINER_OF(inst, CommProtoCustom);
    if (len > inst->max_payload)
    {
        return -1;
    }
    uint16_t frame_len = CUSTOM_HEAD_LEN + len + CUSTOM_CRC_TAIL_LEN;
    uint8_t *p = out_buf;

    *p++ = CUSTOM_SOF;
    for (uint8_t i = 0; i < COMM_ID_LEN; i++)
    {
        *p++ = (uint8_t)(comm_id >> (8 * i));
    }
    *p++ = (uint8_t)(len & 0xFF); /* LEN LE16 */
    *p++ = (uint8_t)(len >> 8);
    *p++ = pc->seq_tx++; /* SEQ */
    *p++ = BSP_CRC8(out_buf + 1, CUSTOM_HEAD_BODY_LEN, 0x00); /* HEAD_CRC8 */
    if (payload && len)
    {
        memcpy(p, payload, len);
    }
    p += len;
    uint16_t crc = BSP_CRC16(out_buf, frame_len - CUSTOM_CRC_TAIL_LEN, 0xFFFF); /* FRAME_CRC16 */
    *p++ = (uint8_t)(crc & 0xFF);
    *p++ = (uint8_t)(crc >> 8);
    return (int16_t)frame_len;
}

static ProtoParseResult_e CustomUnpack(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg)
{
    CommProtoCustom *pc = COMM_CONTAINER_OF(inst, CommProtoCustom);
    uint16_t i = 0;

    while (i < len)
    {
        uint8_t b = data[i++];
        switch (pc->state)
        {
        case CUSTOM_ST_WAIT_SOF:
            if (b == CUSTOM_SOF)
            {
                pc->rx_buff[0] = b;
                pc->rx_len = 1;
                pc->state = CUSTOM_ST_COLLECT_HEAD;
                pc->frame_timestamp_us = DWT_GetTimeUs();
            }
            break;

        case CUSTOM_ST_COLLECT_HEAD:
            pc->rx_buff[pc->rx_len++] = b;
            if (pc->rx_len == CUSTOM_HEAD_LEN)
            {
                /* 验 HEAD_CRC8（对 COMM_ID + LEN + SEQ，不含 SOF 与 CRC 本身） */
                uint8_t crc = BSP_CRC8(&pc->rx_buff[1], CUSTOM_HEAD_LEN - 2, 0x00);
                if (crc != pc->rx_buff[CUSTOM_HEAD_LEN - 1])
                {
                    CustomResetState(pc); /* 头校验失败：丢弃重同步 */
                    break;
                }
                uint16_t payload_len = ReadPayloadLen(&pc->rx_buff[1]);
                if (payload_len > inst->max_payload)
                {
                    CustomResetState(pc);
                    break;
                }
                pc->expect_frame_len = CUSTOM_HEAD_LEN + payload_len + CUSTOM_CRC_TAIL_LEN;
                if (pc->expect_frame_len > sizeof(pc->rx_buff))
                {
                    CustomResetState(pc);
                    break;
                }
                pc->state = CUSTOM_ST_COLLECT_PAYLOAD;
            }
            break;

        case CUSTOM_ST_COLLECT_PAYLOAD:
            pc->rx_buff[pc->rx_len++] = b;
            if (pc->rx_len == pc->expect_frame_len)
            {
                uint16_t crc = BSP_CRC16(pc->rx_buff, pc->expect_frame_len - CUSTOM_CRC_TAIL_LEN, 0xFFFF);
                uint16_t rx_crc = (uint16_t)pc->rx_buff[pc->rx_len - 2] | ((uint16_t)pc->rx_buff[pc->rx_len - 1] << 8);
                if (crc == rx_crc)
                {
                    msg->comm_id = ReadCommId(&pc->rx_buff[1]);
                    msg->payload = &pc->rx_buff[CUSTOM_HEAD_LEN];
                    msg->len = ReadPayloadLen(&pc->rx_buff[1]);
                    msg->ctx = pc;
                    msg->timestamp_us = pc->frame_timestamp_us;
                    CustomResetState(pc);
                    return PROTO_PARSE_OK;
                }
                CustomResetState(pc); /* 帧校验失败：丢弃，等待下一帧头 */
            }
            break;

        default:
            CustomResetState(pc);
            break;
        }
    }
    return PROTO_PARSE_NEED_MORE;
}

static int16_t CustomFindHeader(CommProto *inst, const uint8_t *data, uint16_t len)
{
    (void)inst;
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] == CUSTOM_SOF)
        {
            return (int16_t)i;
        }
    }
    return -1;
}

static uint16_t CustomCalcLen(CommProto *inst, const uint8_t *buf)
{
    (void)inst;
    (void)buf;
    return CUSTOM_HEAD_LEN + CUSTOM_CRC_TAIL_LEN; /* 最小帧长 */
}

static uint8_t CustomVerifyCrc(CommProto *inst, const uint8_t *buf, uint16_t len)
{
    (void)inst;
    if (len < CUSTOM_HEAD_LEN + CUSTOM_CRC_TAIL_LEN)
    {
        return 0;
    }
    uint16_t crc = BSP_CRC16(buf, len - CUSTOM_CRC_TAIL_LEN, 0xFFFF);
    uint16_t rx = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    return (crc == rx) ? 1 : 0;
}

static void CustomReset(CommProto *inst)
{
    CommProtoCustom *pc = COMM_CONTAINER_OF(inst, CommProtoCustom);
    CustomResetState(pc);
}

static const CommProtoVTable_s s_proto_custom_vtable = {
    .pack = CustomPack,
    .unpack = CustomUnpack,
    .find_header = CustomFindHeader,
    .calc_len = CustomCalcLen,
    .verify_crc = CustomVerifyCrc,
    .reset = CustomReset,
};

int8_t CommProtoCustomConfig(CommProto *inst, const CommProtoCustom_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        LOGERROR("[comm_proto_custom] inst or cfg is NULL!");
        return -1;
    }
    if (cfg->max_payload > COMM_RING_CHUNK_SIZE)
    {
        LOGERROR("[comm_proto_custom] max_payload exceeds COMM_RING_CHUNK_SIZE!");
        return -1;
    }
    inst->vtable = &s_proto_custom_vtable;
    inst->proto_id = cfg->proto_id;
    inst->max_payload = cfg->max_payload;
    inst->daemon = cfg->daemon;
    inst->is_frame_protocol = 0;
    CustomResetState(COMM_CONTAINER_OF(inst, CommProtoCustom));
    return 0;
}
