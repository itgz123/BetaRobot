/**
 * @file comm_proto.h
 * @brief 通信框架-协议层（Protocol）基类
 *
 * 职责：把"任意长度数据单元"抽象为"payload 打包/解包"的统一通道。
 *   - 发送：vtable->pack(payload, out_buff) 把 payload 打包进 out_buff
 *           （comm 层 CommSend 提供的打包缓冲，再由 MediaSend 拷入 media->tx_buff 发出）
 *   - 接收：unpack(data) 只解包，返回解出的 payload 指针（NULL 丢弃）；
 *           调用 on_frame 由 comm 层统一控制（CommMediaRxHook 按 unpack_mode 分流）
 *   - 长度模型：固定长度，payload 大小编译期确定（DEF 宏写入），接口不显式传 len
 * 基类不感知介质物理限制（属 media 层），不解析 payload 内容（属引擎/应用）。
 */

#ifndef COMM_PROTO_H
#define COMM_PROTO_H

#include <stdint.h>
#include "app_cfg.h"
/* 依赖单向：drv_comm.h → comm_proto.h；本头自包含，勿 include comm_media.h / drv_comm.h（防环） */

/* 协议类型枚举（COMM_DEF / 后端 Init 用） */
typedef enum : uint8_t
{
    PROTO_RAW = 0, /* 空协议：payload 占 100%，无任何开销 */
    PROTO_CUSTOM,  /* 自定义帧协议（预留） */
} ProtocolType_e;

/* 每个协议定义自己的开销宏（字节）：整帧长度 = payload_size + 开销 */
#define PROTO_RAW_OVERHEAD 0 /* 空协议：无帧头/长度域/校验，payload 占 100% */

/* 按协议类型取开销（COMM_DEF 推算 media rx/tx 缓冲大小用；新协议在枚举加类型并在此补开销） */
#define COMM_PROTO_OVERHEAD(proto_type_) \
    ((proto_type_) == PROTO_RAW ? PROTO_RAW_OVERHEAD : 0)

typedef struct CommProto CommProto;

/* 出帧回调：解出一条完整 payload 时调用。
 * @param payload 解出的完整 payload 指针
 * @note 1) 不带 proto 参数：每个 comm 实例通过 CommConfig.on_frame 传入自己
 *          的函数，天然知道是哪条对话来的，无需靠 proto 指针区分。
 *       2) payload 生命周期：UNPACK_IN_ISR 下指向接收缓冲，回调返回后即被
 *          bsp 清空并重启接收——必须同步消费（解析/拷贝），不可保存指针异步用。 */
typedef void (*ProtoFrameCallback)(const uint8_t *payload);

/* 虚函数表（派生结构体首成员为 vtable，drv_motor_base 约定） */
typedef struct
{
    int8_t (*pack)(CommProto *self, const uint8_t *payload, uint8_t *out_buff); /* 打包 payload → out_buff */
    const uint8_t *(*unpack)(CommProto *self, const uint8_t *data);             /* 只解包：返回解出的 payload 指针，NULL 丢弃 */
    void (*reset)(CommProto *self);                                             /* 重置解包状态 */
} CommProtoVTable_s;

/* 协议基类（派生结构体内嵌作首成员） */
struct CommProto
{
    const CommProtoVTable_s *vtable; /* 必须首成员 */
    ProtocolType_e type;             /* 协议类型 */
    uint16_t payload_size;           /* payload 长度（编译期确定，DEF 宏写入） */
    void *parent;                    /* 指向 comm 实例 */
    void *media;                     /* 指向 media 实例（发送用，MediaSend 定位后端） */
    ProtoFrameCallback on_frame;     /* 出帧回调（CommConfig 挂接） */
};

/* 公共接口（vtable 分发封装，实现见 comm_proto.c） */
int8_t ProtoPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff);
const uint8_t *ProtoUnpack(CommProto *self, const uint8_t *data);
void ProtoReset(CommProto *self);

#endif /* COMM_PROTO_H */
