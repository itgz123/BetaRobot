/**
 * @file comm_proto_seasky.h
 * @brief 视觉（seasky）协议插件（参考湖南大学 RoboMaster 电控通信协议）
 *
 * 接收帧（RX_PACKET，固定 10 字节）：
 *   0x00  1    header = 0xA5
 *   0x01  1    tracking|id|armors|reserved（位域）
 *   0x02  2+2+2 pitch(int16)/yaw(int16)/speed(int16)
 *   0x08  2    CRC16（覆盖 [0..7]，LE 低字节在前）
 * 发送帧（简化版，变长）：
 *   0x5A | 0x00 | float_data(float_length 字节) | CRC16(2, LE)
 *
 * comm_id 固定为 0（本协议接收无消息类型字段）。
 */

#ifndef DRV_COMM_PROTO_SEASKY_H
#define DRV_COMM_PROTO_SEASKY_H

#include "comm_proto.h"

#define SEASKY_SOF_RX 0xA5   /* 接收帧头 */
#define SEASKY_SOF_TX 0x5A   /* 发送帧头 */
#define SEASKY_RX_FRAME_SIZE 10

typedef struct
{
    CommProto proto; /* 首成员：内嵌基类 */
    uint8_t rx_buff[SEASKY_RX_FRAME_SIZE];
    uint8_t rx_len;
    uint8_t state;
} CommProtoSeasky;

typedef struct
{
    uint16_t proto_id;
    DaemonInstance *daemon;
} CommProtoSeasky_Config_s;

int8_t CommProtoSeaskyConfig(CommProto *inst, const CommProtoSeasky_Config_s *cfg);

#endif /* DRV_COMM_PROTO_SEASKY_H */
