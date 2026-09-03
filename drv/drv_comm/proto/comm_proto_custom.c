/**
 * @file comm_proto_custom.c
 * @brief 通信框架-协议层（Protocol）自定义帧协议后端实现
 *
 * 帧格式（N = payload_size，固定长度）：
 *   [帧头 0xA5] [seq 1B] [payload N] [CRC8 1B] [帧尾 0x5A]
 *   CRC8 校验范围 = seq + payload（帧头后到 CRC 前），poly 0x07、init 0x00，
 *   由 lib_crc 提供（Flash 表 LIB_CRC_TBL_CRC8，每字节 1 次查表）。
 *
 * 接收检测（CustomUnpack，按序校验全过才接受）：
 *   帧头/帧尾/CRC 不符 → 坏帧，丢弃，不更新 seq
 *   seq == rx_last_seq      → 重帧，丢弃
 *   seq != rx_last_seq + 1  → 丢帧，lost_frames += 跳过帧数，仍接受当前帧
 */

#include "comm_proto_custom.h"
#include <string.h>

#ifdef DRV_COMM_USED

static int8_t CustomPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff);
static const uint8_t *CustomUnpack(CommProto *self, const uint8_t *data);
static void CustomReset(CommProto *self);

static const CommProtoVTable_s s_custom_vtable = {
    .pack = CustomPack,
    .unpack = CustomUnpack,
    .reset = CustomReset,
};

/* CRC8（poly 0x07、init 0x00、MSB-first、无反射无异或）两档实现，均来自 lib_crc：
 *   1) lib_crc 表已编译（LIB_CRC_USED + LIB_CRC_TABLES_USED）→ 查表 LIB_CRC_TableCalc
 *   2) 表未编译 → 逐位 LIB_CRC_Direct
 * 协议 CRC 强制依赖 lib_crc（不内置手写实现）；未启用 LIB_CRC_USED 则编译报错。
 * 校验范围统一为 seq 起（&data[1]）到 payload 末，长度 payload_size + 1 */
#if defined(LIB_CRC_USED) && defined(LIB_CRC_TABLES_USED)
#include "lib_crc.h"
#include "lib_crc_tables.h"
#define PROTO_CUSTOM_CRC8(data, len) ((uint8_t)LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, (data), (len)))
#elif defined(LIB_CRC_USED)
#include "lib_crc.h"
static const LIB_CRC_Algo_t s_crc8_algo = {0x00, 8, 0x07, 0x00, 0, 0}; /* CRC-8: poly 0x07/init 0x00 非反射 */
#define PROTO_CUSTOM_CRC8(data, len) ((uint8_t)LIB_CRC_Direct(&s_crc8_algo, (data), (len)))
#else
#error "comm_proto_custom 的 CRC8 依赖 lib_crc（LIB_CRC_USED），请启用 LIB_CRC_USED"
#endif /* LIB_CRC_USED / LIB_CRC_TABLES_USED */

/* 打包：帧头 + seq(自增) + payload + CRC8 + 帧尾 */
static int8_t CustomPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff)
{
    CommProtoCustom *p = (CommProtoCustom *)self;

    if (self == NULL || payload == NULL || out_buff == NULL)
        return -1;

    out_buff[0] = PROTO_CUSTOM_FRAME_HEADER;
    out_buff[1] = p->tx_seq++; /* 发送 seq 自增（255→0 自然回卷） */
    memcpy(&out_buff[2], payload, self->payload_size);
    out_buff[2 + self->payload_size] = PROTO_CUSTOM_CRC8(&out_buff[1], self->payload_size + 1);
    out_buff[3 + self->payload_size] = PROTO_CUSTOM_FRAME_TAIL;
    return 0;
}

/* 只解包：校验后返回解出的 payload 指针（NULL = 丢弃）；on_frame 由 comm 层统一调 */
static const uint8_t *CustomUnpack(CommProto *self, const uint8_t *data)
{
    CommProtoCustom *p = (CommProtoCustom *)self;
    uint8_t seq;
    uint16_t n = self->payload_size;

    if (self == NULL || data == NULL)
        return NULL;

    /* 1. 帧头/帧尾定界（固定长度帧，整帧长 = n + 4） */
    if (data[0] != PROTO_CUSTOM_FRAME_HEADER)
        return NULL;
    if (data[n + 3] != PROTO_CUSTOM_FRAME_TAIL)
        return NULL;
    /* 2. CRC8 校验（seq + payload）；不符 = 坏帧，丢弃且不更新 seq */
    if (PROTO_CUSTOM_CRC8(&data[1], n + 1) != data[n + 2])
        return NULL;

    /* 3. 帧序列检测 */
    seq = data[1];
    if (seq == p->rx_last_seq)
        return NULL; /* 重帧，丢弃 */
    if (seq != (uint8_t)(p->rx_last_seq + 1))
        p->lost_frames += (uint8_t)(seq - (p->rx_last_seq + 1)); /* 跳号记丢帧，仍接受 */
    p->rx_last_seq = seq;

    return data + 2; /* payload 指针（帧头后） */
}

/* 重置解包/打包状态（通讯重连/协议切换时调用） */
static void CustomReset(CommProto *self)
{
    CommProtoCustom *p = (CommProtoCustom *)self;

    if (self == NULL)
        return;
    p->tx_seq = 0;
    p->rx_last_seq = 0xFF; /* 0xFF+1 回卷=0：重置后首帧 seq=0 视为正常新帧而非重帧 */
    p->lost_frames = 0;
}

int8_t CommProtoCustomInit(CommProtoCustom *proto)
{
    if (proto == NULL)
        return -1;
    proto->base.vtable = &s_custom_vtable;
    proto->tx_seq = 0;
    proto->rx_last_seq = 0xFF; /* 0xFF+1 回卷=0：首帧 seq=0 视为正常新帧而非重帧 */
    proto->lost_frames = 0;
    return 0;
}

#endif /* DRV_COMM_USED */
