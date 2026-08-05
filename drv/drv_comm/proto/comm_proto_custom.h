/**
 * @file comm_proto_custom.h
 * @brief 统一自定义帧协议（0x5A 家族）
 *
 * 帧格式（参数可裁剪，COMM_ID_LEN 决定 COMM_ID 字段宽度）：
 *   偏移      长度        字段
 *   0         1           SOF = 0x5A
 *   1         COMM_ID_LEN COMM_ID（LE，路由键 = "某设备的某应用"）
 *   1+IDLEN   2           LEN（LE16，载荷长度）
 *   3+IDLEN   1           SEQ（发送序号，丢帧检测）
 *   4+IDLEN   1           HEAD_CRC8（对 COMM_ID + LEN + SEQ）
 *   5+IDLEN   LEN         PAYLOAD
 *   5+IDLEN+LEN 2         FRAME_CRC16（LE16，整帧）
 *
 * 字节流介质：状态机累积处理粘包/拆包；CAN 介质：跨 8 字节帧拼接。
 * 同一状态机通吃两种介质。
 */

#ifndef DRV_COMM_PROTO_CUSTOM_H
#define DRV_COMM_PROTO_CUSTOM_H

#include "comm_proto.h"

#define CUSTOM_SOF 0x5A

/* 头部总长：SOF + COMM_ID + LEN + SEQ + HEAD_CRC8 */
#define CUSTOM_HEAD_LEN (1 + COMM_ID_LEN + 2 + 1 + 1)
#define CUSTOM_CRC_TAIL_LEN 2

typedef struct
{
    CommProto proto; /* 首成员：内嵌基类 */
    /* 解包累积缓冲：头 + 最大载荷 + CRC16 尾 */
    uint8_t rx_buff[CUSTOM_HEAD_LEN + COMM_RING_CHUNK_SIZE + CUSTOM_CRC_TAIL_LEN];
    uint16_t rx_len;         /* 已累积长度 */
    uint16_t expect_frame_len; /* 完整帧总长 */
    uint8_t state;           /* 解包状态机 */
    uint8_t seq_tx;          /* 发送序号 */
    uint64_t frame_timestamp_us; /* 当前帧起始时间戳 */
} CommProtoCustom;

typedef struct
{
    uint16_t proto_id;
    uint16_t max_payload;   /* 载荷上限（≤ COMM_RING_CHUNK_SIZE） */
    DaemonInstance *daemon;
} CommProtoCustom_Config_s;

int8_t CommProtoCustomConfig(CommProto *inst, const CommProtoCustom_Config_s *cfg);

#endif /* DRV_COMM_PROTO_CUSTOM_H */
