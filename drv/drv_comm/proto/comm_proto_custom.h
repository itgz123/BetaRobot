/**
 * @file comm_proto_custom.h
 * @brief 通信框架-协议层（Protocol）自定义帧协议后端
 *
 * 固定长度帧协议，帧格式（N = payload_size，编译期确定）：
 *   [帧头 0xA5] [seq 1B] [payload N] [CRC8 1B] [帧尾 0x5A]
 *   整帧长 = N + 4（PROTO_CUSTOM_OVERHEAD，见 comm_proto.h）。
 *
 * 帧序列（seq）检测：
 *   - 发送：pack 时 seq 自增（uint8_t，255→0 回卷）
 *   - 接收：unpack 时校验
 *       seq == 上次 + 1 → 正常新帧，接受
 *       seq == 上次      → 重帧，丢弃（返回 NULL）
 *       seq 跳号         → 丢帧，累计 lost_frames 后仍接受当前帧
 *     坏帧（帧头/帧尾/CRC 不符）直接丢弃，不更新 seq。
 *
 * @note 长度模型固定：payload_size 编译期确定，无帧内长度域。
 * @note COMM_DEF 通过 token 拼接 COMM_##proto_type_##_DEF 分发到本宏。
 */

#ifndef COMM_PROTO_CUSTOM_H
#define COMM_PROTO_CUSTOM_H

#include "comm_proto.h"

#ifdef DRV_COMM_USED

/* 帧头/帧尾定界（CRC8 校验范围 = seq + payload，见 comm_proto_custom.c） */
#define PROTO_CUSTOM_FRAME_HEADER 0xA5
#define PROTO_CUSTOM_FRAME_TAIL 0x5A

/* 自定义帧协议派生结构体（首成员必须为 CommProto 基类） */
typedef struct
{
    CommProto base;       /* 基类（首成员） */
    uint8_t tx_seq;       /* 发送帧序列（pack 时自增写入帧） */
    uint8_t rx_last_seq;  /* 上次收到的帧序列（unpack 检测丢帧/重帧） */
    uint32_t lost_frames; /* 累计丢帧数（跳号时 += 跳过帧数；重帧/坏帧不计） */
} CommProtoCustom;

/**
 * @brief 静态定义自定义帧协议实例
 * @param name        实例名称
 * @param media_      media 实例（发送用，指向 CommMedia 派生实例）
 * @param payload_sz  payload 长度（编译期确定；整帧长 = payload_sz + PROTO_CUSTOM_OVERHEAD）
 *
 * @note media_ 以指针绑定，运行时无需另传；发送缓冲由 comm 层提供（inst->tx_buff，
 *       大小由 COMM_DEF 按开销推算）。vtable 与收发序列状态由 CommProtoCustomInit 初始化。
 *
 * @example
 *   COMM_PROTO_CUSTOM_DEF(proto_cmd, uart_comm, 8);
 */
#define COMM_PROTO_CUSTOM_DEF(name, media_, payload_sz) \
    static CommProtoCustom name = {                     \
        .base.payload_size = payload_sz,                \
        .base.media = (void *)&media_} /* 尾部无分号，调用处加 */

/**
 * @brief 初始化自定义帧协议后端（挂 vtable + 清零收发序列状态）
 * @param proto CommProtoCustom 实例指针（COMM_PROTO_CUSTOM_DEF 定义）
 * @retval 0 成功；-1 参数非法
 */
int8_t CommProtoCustomInit(CommProtoCustom *proto);

#endif /* DRV_COMM_USED */
#endif /* COMM_PROTO_CUSTOM_H */
