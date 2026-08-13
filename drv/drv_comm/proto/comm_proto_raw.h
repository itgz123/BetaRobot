/**
 * @file comm_proto_raw.h
 * @brief 通信框架-协议层（Protocol）空协议后端
 *
 * 最简单的协议：payload 占 100%，无帧头/长度域/校验/comm_id 等任何开销。
 *   - 发送：pack 即 memcpy（payload 直接是整帧）
 *   - 接收：unpack 收到的数据就是完整 payload，直接返回（on_frame 由 comm 层统一调）
 *
 * @note 长度模型固定：payload_size 编译期确定，收发同长。
 * @note COMM_DEF 通过 token 拼接 COMM_##proto_type_##_DEF 分发到本宏。
 */

#ifndef COMM_PROTO_RAW_H
#define COMM_PROTO_RAW_H

#include "comm_proto.h"

#ifdef DRV_COMM_USED

/* 空协议派生结构体（首成员必须为 CommProto 基类） */
typedef struct
{
    CommProto base; /* 基类（首成员） */
} CommProtoRaw;

/**
 * @brief 静态定义空协议实例
 * @param name         实例名称
 * @param media_       media 实例（发送用，指向 CommMedia 派生实例）
 * @param payload_size payload 长度（编译期确定；空协议无开销，整帧 = payload 直接写 media->tx_buff）
 *
 * @note media_ 以指针绑定，运行时无需另传；发送缓冲由 media 提供（media->tx_buff），
 *       协议分包直接写入。vtable 由 CommProtoRawInit 挂接。
 *
 * @example
 *   COMM_PROTO_RAW_DEF(proto_comm, uart_comm, 32);
 */
#define COMM_PROTO_RAW_DEF(name, media_, payload_sz) \
    static CommProtoRaw name = {                     \
        .base.payload_size = payload_sz,             \
        .base.media = (void *)&media_} /* 尾部无分号，调用处加 */

/**
 * @brief 初始化空协议后端
 * @param proto CommProtoRaw 实例指针（COMM_PROTO_RAW_DEF 定义）
 * @retval 0 成功；-1 参数非法
 */
int8_t CommProtoRawInit(CommProtoRaw *proto);

#endif /* DRV_COMM_USED */
#endif /* COMM_PROTO_RAW_H */
