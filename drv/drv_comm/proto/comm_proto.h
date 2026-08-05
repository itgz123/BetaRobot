/**
 * @file comm_proto.h
 * @brief 协议层（Protocol）：协议相关处理
 *
 * 统一"喂字节 chunk -> 状态机累积 -> 出完整帧"的解包模型。
 * 解包产物为 ProtoMessage_s（comm_id + payload + 时间戳 + 来源），
 * 由引擎按 comm_id 路由分发。
 *
 * 支持多种协议共存：
 *   - 统一自定义帧协议 CommProtoCustom（0x5A 家族，字节流/CAN 通吃）
 *   - 外部既定协议插件 CommProtoSbus / CommProtoSeasky / CommProtoReferee
 */

#ifndef DRV_COMM_PROTO_H
#define DRV_COMM_PROTO_H

#include "drv_comm.h"
#include "drv_daemon.h"

/* 解包结果 */
typedef enum : uint8_t
{
    PROTO_PARSE_NEED_MORE = 0, /* 需要更多字节 */
    PROTO_PARSE_OK = 1,        /* 解析出一整帧，msg 有效 */
    PROTO_PARSE_DROP = 2,      /* 丢弃（校验失败/不属本协议） */
} ProtoParseResult_e;

/* 解析出的完整消息（tag 形式，支持在其他头中前向声明） */
typedef struct ProtoMessage_s ProtoMessage_s;
struct ProtoMessage_s
{
    CommId_t comm_id;       /* 路由键（帧内消息类型） */
    uint16_t len;           /* 载荷长度 */
    const uint8_t *payload; /* 指向协议内部缓冲（本次回调内有效） */
    uint32_t media_id;      /* 来源介质 */
    uint32_t proto_id;      /* 来源协议 */
    uint64_t timestamp_us;  /* 到达时间戳 */
    void *ctx;              /* 协议私有数据 */
};

typedef struct CommProto CommProto;

/* 协议虚函数表 */
typedef struct
{
    int16_t (*pack)(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf); /* 返总长，-1 失败 */
    ProtoParseResult_e (*unpack)(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg);      /* 喂 chunk */
    int16_t (*find_header)(CommProto *inst, const uint8_t *data, uint16_t len);                                /* 重同步，返偏移/-1 */
    uint16_t (*calc_len)(CommProto *inst, const uint8_t *buf);                                                 /* 多帧重组：由头算整帧长 */
    uint8_t (*verify_crc)(CommProto *inst, const uint8_t *buf, uint16_t len);                                  /* 1/0 */
    void (*reset)(CommProto *inst);
} CommProtoVTable_s;

/* 协议基类（派生结构体将其作为首成员内嵌） */
struct CommProto
{
    const CommProtoVTable_s *vtable; /* 必须首成员 */
    uint16_t proto_id;               /* 协议编号（路由/诊断用） */
    uint16_t max_payload;            /* 载荷上限（静态缓冲上限） */
    uint8_t is_frame_protocol;       /* 1=CAN 单帧，0=字节流拼接 */
    DaemonInstance *daemon;          /* 在线检测（可为 NULL） */
    void *ctx;
};

/* 协议类型枚举 + TYPE 映射（COMM_PROTO_DEF 用） */
typedef enum : uint8_t
{
    PROTO_CUSTOM = 0,
    PROTO_SBUS,
    PROTO_SEASKY,
    PROTO_REFEREE,
} ProtoType_e;

#define TYPE_PROTO_CUSTOM CommProtoCustom
#define TYPE_PROTO_SBUS CommProtoSbus
#define TYPE_PROTO_SEASKY CommProtoSeasky
#define TYPE_PROTO_REFEREE CommProtoReferee

/**
 * @brief 协议实例定义宏（仿 animal_def）
 * @param name 实例名
 * @param type 协议类型（PROTO_CUSTOM/PROTO_SBUS/PROTO_SEASKY/PROTO_REFEREE）
 * @example COMM_PROTO_DEF(p1, PROTO_CUSTOM);
 */
#define COMM_PROTO_DEF(name, type) \
    TYPE_##type name##_child;      \
    CommProto *name = &name##_child.proto

/* 接口 */
int8_t ProtocolRegister(CommProto *inst);
int8_t ProtocolConfig(CommProto *inst, uint16_t proto_id, uint16_t max_payload, DaemonInstance *daemon);
ProtoParseResult_e ProtocolFeed(CommProto *inst, const uint8_t *data, uint16_t len, ProtoMessage_s *msg, uint32_t media_id);
int16_t ProtocolPack(CommProto *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len, uint8_t *out_buf);

#endif /* DRV_COMM_PROTO_H */
