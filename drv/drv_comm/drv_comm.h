#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>
#include "app_cfg.h"
//
#include "comm_media.h"
#include "comm_proto.h"

typedef struct
{
    uint8_t sender_id;            // 发送者id（运行期 CommConfig 配置）
    uint8_t receiver_id;          // 接收者id（运行期 CommConfig 配置）
    MediaType_e media_type;       // 介质类型（定义时写入）
    ProtocolType_e rx_proto_type; // 接收协议类型（定义时写入）
    ProtocolType_e tx_proto_type; // 发送协议类型（定义时写入）
    void *media;                  // 介质派生实例指针（首成员为 CommMedia 基类）
    void *rx_proto;               // 接收协议派生实例指针（首成员为 CommProto 基类）
    void *tx_proto;               // 发送协议派生实例指针（首成员为 CommProto 基类）
    uint8_t inited;               // 初始化标志（CommRegister 置位）
} CommInstance;

/* 从 CommInstance 取介质/协议基类指针（void* 指向派生实例，首成员即基类） */
#define COMM_INSTANCE_MEDIA(inst) ((CommMedia *)((inst)->media))
#define COMM_INSTANCE_RX_PROTO(inst) ((CommProto *)((inst)->rx_proto))
#define COMM_INSTANCE_TX_PROTO(inst) ((CommProto *)((inst)->tx_proto))

/* 挂载表容量（默认值，可被 app_cfg.h 覆盖） */
#ifndef COMM_LINK_NUM
#define COMM_LINK_NUM 16 /* media↔proto 挂载表容量 */
#endif

/**
 * @brief comm 运行时配置（CommConfig 传入；可重入，可反复调用修改）
 */
typedef struct
{
    void *media_cfg;             /* 介质后端配置指针（USART → USART_Config_s*；NULL 跳过介质配置） */
    ProtoFrameCallback on_frame; /* 出帧消费回调（NULL 表示不修改） */
} CommConfig_s;

/**
 * @brief 注册 comm 实例（不可重入：仅可调用一次）
 * @param inst CommInstance 指针（COMM_DEF 定义）
 * @retval 0 成功；-1 失败（参数非法 / 后端注册失败 / 类型未支持）
 *
 * @note 完成三步：1) media 后端注册（USART 内部做 USARTRegister，防重复注册）；
 *       2) proto 后端 Init（挂 vtable）；3) 接线分发（media 接管接收钩子 +
 *       proto 接管出帧钩子 + 建挂载）。介质参数与出帧回调由 CommConfig 配置。
 */
int8_t CommRegister(CommInstance *inst);

/**
 * @brief 配置 comm 实例（可重入：可反复调用改介质参数 / 出帧回调）
 * @param inst CommInstance 指针（须先 CommRegister）
 * @param cfg  CommConfig_s 配置（media_cfg / on_frame 均可为 NULL，NULL 则跳过对应项）
 * @retval 0 成功；-1 失败（参数非法 / 未注册 / 配置失败 / 类型未支持）
 *
 * @note media_cfg 经 media 后端下发（USART → MediaUsartConfig → bsp USARTConfig），
 *       运行中可再次调用以切换波特率/发送模式等；on_frame 直接覆盖挂到
 *       接收协议 rx_proto->on_frame，可运行期修改消费逻辑。
 */
int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg);

/**
 * @brief 统一发送：经该实例的发送协议打包后由绑定的 media 发出
 * @param inst    CommInstance 指针（须已 CommRegister）
 * @param payload 待发送 payload 指针（长度 = tx_payload_size，编译期确定）
 * @retval 0 成功；-1 失败（参数非法 / 打包或发送失败）
 *
 * @note tx_proto->media 由 COMM_DEF 宏静态绑定，直接走发送协议的 pack → media 发出。
 */
int8_t CommSend(CommInstance *inst, const uint8_t *payload);

/**
 * @brief 静态定义一条通信（对话）：双向，收发协议与内容可不同
 * @param name            实例名称
 * @param media_type_     介质类型（token 拼接 COMM_##media_type_##_DEF）
 * @param rx_proto_type_  接收协议类型（token 拼接 COMM_##rx_proto_type_##_DEF）
 * @param tx_proto_type_  发送协议类型（token 拼接 COMM_##tx_proto_type_##_DEF）
 * @param rx_size         接收 payload 大小（media rx 缓冲 = rx_size + 协议开销）
 * @param tx_size         发送 payload 大小（media tx 缓冲 = tx_size + 协议开销）
 *
 * @note 只定义编译期必须确定的量；运行期参数（身份 id / 介质配置 / 回调）
 *       由 CommRegister / CommConfig 配置。
 * @example
 *   COMM_DEF(cmd_comm, MEDIA_USART, PROTO_RAW, PROTO_RAW, 8, 16);
 */
#define COMM_DEF(name, media_type_, rx_proto_type_, tx_proto_type_, rx_size, tx_size) \
    COMM_##media_type_##_DEF(name##_media,                                            \
                             (rx_size) + COMM_PROTO_OVERHEAD(rx_proto_type_),         \
                             (tx_size) + COMM_PROTO_OVERHEAD(tx_proto_type_));        \
    COMM_##rx_proto_type_##_DEF(name##_rx_proto, name##_media, rx_size);              \
    COMM_##tx_proto_type_##_DEF(name##_tx_proto, name##_media, tx_size);              \
    CommInstance name = {                                                             \
        .media_type = media_type_,                                                    \
        .rx_proto_type = rx_proto_type_,                                              \
        .tx_proto_type = tx_proto_type_,                                              \
        .media = &name##_media,                                                       \
        .rx_proto = &name##_rx_proto,                                                 \
        .tx_proto = &name##_tx_proto,                                                 \
        .inited = 0,                                                                  \
    }

#endif /* DRV_COMM_H */
