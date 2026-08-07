#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>
#include "app_cfg.h"
//
#include "comm_media.h"
#include "comm_proto.h"

typedef struct
{
    uint8_t sender_id;         // 发送者id
    uint8_t receiver_id;       // 接收者id
    MediaType_e media_type;    // 介质类型（定义时写入）
    ProtocolType_e proto_type; // 协议类型（定义时写入）
    void *media;               // 介质派生实例指针（首成员为 CommMedia 基类）
    void *proto;               // 协议派生实例指针（首成员为 CommProto 基类）
    uint8_t inited;            // 初始化标志（CommInit 置位）
} CommInstance;

/* 从 CommInstance 取介质/协议基类指针（void* 指向派生实例，首成员即基类） */
#define COMM_INSTANCE_MEDIA(inst) ((CommMedia *)((inst)->media))
#define COMM_INSTANCE_PROTO(inst) ((CommProto *)((inst)->proto))

/**
 * @brief comm 运行时配置（CommConfig 传入；可重入，可反复调用修改）
 */
typedef struct
{
    void *media_cfg;             /* 介质后端配置指针（USART → USART_Config_s*；NULL 跳过介质配置） */
    ProtoFrameCallback on_frame; /* 出帧消费回调（同 proto 覆盖更新；NULL 表示不修改） */
} CommConfig_s;

/**
 * @brief 注册 comm 实例（不可重入：仅可调用一次）
 * @param inst CommInstance 指针（COMM_DEF 定义）
 * @retval 0 成功；-1 失败（参数非法 / 后端注册失败 / 类型未支持）
 *
 * @note 完成三步：1) media 后端注册（USART 内部做 USARTRegister，防重复注册）；
 *       2) proto 后端 Init（挂 vtable）；3) 引擎接线（EngineAttachMedia +
 *       EngineAttachProtocol）。介质参数与出帧回调由 CommConfig 配置。
 */
int8_t CommRegister(CommInstance *inst);

/**
 * @brief 配置 comm 实例（可重入：可反复调用改介质参数 / 出帧回调）
 * @param inst CommInstance 指针（须先 CommRegister）
 * @param cfg  CommConfig_s 配置（media_cfg / on_frame 均可为 NULL，NULL 则跳过对应项）
 * @retval 0 成功；-1 失败（参数非法 / 未注册 / 配置失败 / 类型未支持）
 *
 * @note media_cfg 经 media 后端下发（USART → MediaUsartConfig → bsp USARTConfig），
 *       运行中可再次调用以切换波特率/发送模式等；on_frame 由引擎层按 proto
 *       覆盖式注册，可运行期修改消费逻辑。
 */
int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg);

#define COMM_DEF(name, media_type_, proto_type_, payload_size)             \
    COMM_##media_type_##_DEF(name##_media,                                 \
                             (payload_size) + COMM_PROTO_OVERHEAD(proto_type_), \
                             (payload_size) + COMM_PROTO_OVERHEAD(proto_type_)); \
    COMM_##proto_type_##_DEF(name##_proto, name##_media, payload_size);    \
    CommInstance name = {                                                  \
        .media_type = media_type_,                                         \
        .proto_type = proto_type_,                                         \
        .media = &name##_media,                                            \
        .proto = &name##_proto,                                            \
        .inited = 0,                                                       \
    }

#endif /* DRV_COMM_H */
