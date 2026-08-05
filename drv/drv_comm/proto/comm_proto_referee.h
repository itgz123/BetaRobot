/**
 * @file comm_proto_referee.h
 * @brief 裁判系统协议插件（RoboMaster 官方协议）
 *
 * 帧格式：
 *   0x00  1    SOF = 0xA5
 *   0x01  2    DATA_LENGTH（LE16，数据段长度，含 CMD_ID 2 字节）
 *   0x03  1    SEQ（包序号）
 *   0x04  1    CRC8（对 [0..3]，裁判专用非反射 poly 0x31，init 0xFF）
 *   0x05  2    CMD_ID（LE16）
 *   0x07  DL-2  DATA（纯数据）
 *   ...   2    CRC16（LE16，对 [0 .. 7+DL-1)）
 * 总帧长 = 9 + DATA_LENGTH。
 * comm_id = CMD_ID。
 */

#ifndef DRV_COMM_PROTO_REFEREE_H
#define DRV_COMM_PROTO_REFEREE_H

#include "comm_proto.h"

#define REFEREE_COMM_SOF 0xA5
#define REFEREE_COMM_HEAD_LEN 5      /* SOF + DL + SEQ + CRC8 */
#define REFEREE_COMM_CMDID_LEN 2
#define REFEREE_COMM_TAIL_LEN 2      /* CRC16 */
#define REFEREE_COMM_DATA_OFFSET 7   /* 纯数据起点 */

#ifndef REFEREE_COMM_MAX_FRAME
#define REFEREE_COMM_MAX_FRAME 300   /* 9 + 最大 DL */
#endif

typedef struct
{
    CommProto proto; /* 首成员：内嵌基类 */
    uint8_t rx_buff[REFEREE_COMM_MAX_FRAME];
    uint16_t rx_len;
    uint16_t expect_frame_len;
    uint8_t state;
} CommProtoReferee;

typedef struct
{
    uint16_t proto_id;
    DaemonInstance *daemon;
} CommProtoReferee_Config_s;

int8_t CommProtoRefereeConfig(CommProto *inst, const CommProtoReferee_Config_s *cfg);

#endif /* DRV_COMM_PROTO_REFEREE_H */
