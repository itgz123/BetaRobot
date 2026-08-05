/**
 * @file comm_proto_sbus.h
 * @brief DBUS/SBUS 协议插件：切 25 字节遥控器帧
 *
 * SBUS 帧：0x0F 帧头 + 22 字节通道数据 + 1 字节标志 + 1 字节帧尾 = 25 字节。
 * 本插件只做"切帧 + 帧尾校验"，通道解码复用 drv_sbus.h 的
 * SBUS_GetChannelRaw / SBUS_RawFrame_u（上层消费者解析）。
 * comm_id 固定为 0（SBUS 无消息类型字段）。
 */

#ifndef DRV_COMM_PROTO_SBUS_H
#define DRV_COMM_PROTO_SBUS_H

#include "comm_proto.h"

#define SBUS_COMM_FRAME_SIZE 25
#define SBUS_COMM_HEADER 0x0F

typedef struct
{
    CommProto proto; /* 首成员：内嵌基类 */
    uint8_t rx_buff[SBUS_COMM_FRAME_SIZE];
    uint8_t rx_len;
    uint8_t state;
} CommProtoSbus;

typedef struct
{
    uint16_t proto_id;
    DaemonInstance *daemon;
} CommProtoSbus_Config_s;

int8_t CommProtoSbusConfig(CommProto *inst, const CommProtoSbus_Config_s *cfg);

#endif /* DRV_COMM_PROTO_SBUS_H */
