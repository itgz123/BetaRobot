/**
 * @file comm_proto.h
 * @brief 通信框架-协议层（Protocol）基类
 *
 * 职责：把"任意长度数据单元"抽象为"payload 打包/解包"的统一通道。
 *   - 发送：vtable->pack(payload, out_buff) 把 payload 打包进 out_buff
 *           （comm 层 CommSend 提供的打包缓冲，再由 MediaSend 交给 media 后端发出）
 *   - 接收：unpack(data) 只解包，返回解出的 payload 指针（NULL 丢弃）；
 *           调用 on_frame 由 comm 层统一控制（CommMediaRxHook 按 unpack_mode 分流）
 *   - 长度模型：固定长度，payload 大小编译期确定（DEF 宏写入），接口不显式传 len
 * 基类不感知介质物理限制（属 media 层），不解析 payload 内容（属引擎/应用）。
 *
 * 协议扩展（支持 app 自定义协议，驱动零改动）：一个协议 = 类型 id + 开销宏 +
 * DEF 宏 + 派生结构体 + vtable + Init。内置 RAW/CUSTOM 预注册进后端注册表；
 * app 自定义协议在 app 层定义（id 取 PROTO_USER 起），启动时调用
 * CommProtoRegisterBackend 登记，COMM_DEF 传协议名 token 即可接线（见下方注册表）。
 */

#ifndef COMM_PROTO_H
#define COMM_PROTO_H

#include <stdint.h>
#include "app_cfg.h"
/* 依赖单向：drv_comm.h → comm_proto.h；本头自包含，勿 include comm_media.h / drv_comm.h（防环） */

/* 协议类型枚举（COMM_DEF / 后端注册表用）
 * @note 0~PROTO_USER-1 为内置协议；PROTO_USER 起为 app 自定义空间
 *       （自定义 id 用宏分配，如 #define PROTO_MYCMD (PROTO_USER + 1)） */
typedef enum : uint8_t
{
    PROTO_RAW = 0,     /* 空协议：payload 占 100%，无任何开销 */
    PROTO_CUSTOM,      /* 自定义帧协议（帧头+seq+CRC8+帧尾，见 comm_proto_custom.h） */
    PROTO_USER = 0x80, /* app 自定义协议 id 起始 */
} ProtocolType_e;

/* 每个协议定义自己的开销宏（字节）：整帧长度 = payload_size + 开销。
 * COMM_DEF 按协议名 token 拼接 PROTO_##name##_OVERHEAD 取开销，内置与自定义协议都须定义
 * （内置见下；自定义见各 app 协议头，如 app_proto_demo.h 的 PROTO_DEMO_OVERHEAD）。 */
#define PROTO_RAW_OVERHEAD 0    /* 空协议：无帧头/长度域/校验，payload 占 100% */
#define PROTO_CUSTOM_OVERHEAD 4 /* 自定义帧协议：帧头(1) + seq(1) + CRC8(1) + 帧尾(1) */

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
    uint16_t payload_size;           /* payload 长度（编译期确定，DEF 宏写入） */
    void *media;                     /* 指向 media 实例（发送用，MediaSend 定位后端） */
    ProtoFrameCallback on_frame;     /* 出帧回调（CommConfig 挂接） */
};

/* 公共接口（vtable 分发封装，实现见 comm_proto.c） */
int8_t ProtoPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff);
const uint8_t *ProtoUnpack(CommProto *self, const uint8_t *data);
void ProtoReset(CommProto *self);

/*============================================
 *             协议后端注册表
 *============================================*/

/* 注册表容量（内置 RAW/CUSTOM 已占 2 项；可在 app_cfg.h 覆盖加大） */
#ifndef COMM_PROTO_BACKEND_MAX
#define COMM_PROTO_BACKEND_MAX 8
#endif

/* 协议后端描述符：协议类型 → 初始化回调（app 自定义协议登记用） */
typedef struct
{
    ProtocolType_e type;         /* 协议 id（PROTO_RAW/PROTO_CUSTOM，或 PROTO_USER 起自定义） */
    int8_t (*init)(void *proto); /* 初始化回调：挂 vtable + 清状态（具体协议 Init 的 void* 封装） */
} CommProtoBackend_t;

/**
 * @brief 注册协议后端（app 自定义协议登记；启动时调用，可多次）
 * @param backend 协议后端描述符（.type >= PROTO_USER，.init 非空）
 * @retval 0 成功；-1 参数非法 / id 重复 / 表满
 */
int8_t CommProtoRegisterBackend(const CommProtoBackend_t *backend);

/**
 * @brief 按协议类型查后端（CommRegister 初始化 rx/tx 协议用；内置 + 自定义统一查表）
 * @param type 协议类型
 * @return 匹配的后端描述符；未注册返回 NULL
 */
const CommProtoBackend_t *CommProtoBackendFind(ProtocolType_e type);

#endif /* COMM_PROTO_H */
