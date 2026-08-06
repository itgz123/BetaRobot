/**
 * @file comm_instance.h
 * @brief 通信实例（CommInstance）定义入口：app 层唯一需要 include 的 comm 头
 *
 * 只向 app 暴露 CommInstance 统一句柄。通过
 *   COMM_DEF(name, media_type, proto_type)
 * 一次定义 3 个实例（介质派生 + 协议派生 + CommInstance 基类实例），
 * 内部连接 2 个 void 指针（media / proto）。
 *
 * 用法：
 *   #include "comm_instance.h"
 *   COMM_DEF(comm_uart1, MEDIA_USART, PROTO_CUSTOM);   // 全局定义
 *   void function_in_main_c(void) { CommInit(&comm_uart1); }
 *
 * 本头在 DRV_COMM_USED 内包含全部派生类型，再定义 CommInstance。
 * 不能把这些塞进 drv_comm.h：派生头反向 include 基类头时（guard 命中跳过
 * 基类）会导致 ProtoType_e / EngineConsumer_Config_s 不可见，产生 include 环。
 */

#ifndef DRV_COMM_INSTANCE_H
#define DRV_COMM_INSTANCE_H

#include "drv_comm.h"

#ifdef DRV_COMM_USED

/* 派生类型头：COMM_DEF 展开需要派生类型完整定义 */
#include "media/comm_media_can.h"
#include "media/comm_media_usart.h"
#include "media/comm_media_usb.h"
#include "media/comm_media_mem.h"
#include "proto/comm_proto_custom.h"
#include "proto/comm_proto_sbus.h"
#include "proto/comm_proto_seasky.h"
#include "proto/comm_proto_referee.h"

/*---------------------------------------------------------------
 * CommInstance：向 app 暴露的统一通信实例句柄
 * 基类信息（media_type / proto_type）+ 介质 void 指针 + 协议 void 指针。
 * 通过 COMM_DEF(name, media_type, proto_type) 定义，一次生成
 *   介质派生实例 name_media、协议派生实例 name_proto、
 *   以及本基类实例 name（media/proto 两个 void 指针已连接）。
 * app 层只需包含本头即可定义与使用，不接触派生类型细节。
 *---------------------------------------------------------------*/
typedef struct CommInstance
{
    MediaType_e media_type; /* 介质类型（定义时写入） */
    ProtoType_e proto_type; /* 协议类型（定义时写入） */
    void *media;            /* 介质派生实例指针（首成员为 CommMedia 基类） */
    void *proto;            /* 协议派生实例指针（首成员为 CommProto 基类） */
    uint8_t inited;         /* 初始化标志（CommInit 置位） */
} CommInstance;

/* 从 CommInstance 取介质/协议基类指针（void* 指向派生实例，首成员即基类） */
#define COMM_INSTANCE_MEDIA(inst) ((CommMedia *)((inst)->media))
#define COMM_INSTANCE_PROTO(inst) ((CommProto *)((inst)->proto))

/**
 * @brief 通信实例接线（register + config）：把 COMM_DEF 定义的实例挂上引擎
 * @param inst COMM_DEF 定义的实例指针
 * @return 0 成功；-1 失败（类型未支持/参数错误）
 * @note 由实例定义方在自己的初始化函数里调用。内部按 media_type/proto_type
 *       分发到对应 Media/Proto 的 Config，并 EngineAttachMedia/EngineAttachProtocol。
 */
int8_t CommRegister(CommInstance *inst);
/* 统一发送入口：以 CommInstance 为句柄（内部取介质 media_id 路由） */
int8_t CommSend(CommInstance *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len);
/* 统一消费者注册：以 CommInstance 为句柄 */
int8_t CommRegisterConsumer(CommInstance *inst, const EngineConsumer_Config_s *cfg);

/**
 * @brief 通信实例定义宏（仿 animal_def）：一次定义介质+协议+CommInstance 三个实例
 * @param name         实例名
 * @param media_type_  介质类型（MEDIA_CAN/MEDIA_USART/MEDIA_USB/MEDIA_MEM）
 * @param proto_type_  协议类型（PROTO_CUSTOM/PROTO_SBUS/PROTO_SEASKY/PROTO_REFEREE）
 * @note 派生类型头已由本头统一包含，app 层无需单独 include。
 * @note 参数名带尾下划线，避免与结构体成员 .media_type/.proto_type 同名
 *       （同名会在宏展开时把成员名也错误替换）。
 * @example COMM_DEF(comm_uart1, MEDIA_USART, PROTO_CUSTOM);
 */
#define COMM_DEF(name, media_type_, proto_type_)  \
    TYPE_##media_type_ name##_media;              \
    TYPE_##proto_type_ name##_proto;              \
    CommInstance name = {                         \
        .media_type = media_type_,                \
        .proto_type = proto_type_,                \
        .media = &name##_media,                   \
        .proto = &name##_proto,                   \
        .inited = 0,                              \
    }

#endif /* DRV_COMM_USED */

#endif /* DRV_COMM_INSTANCE_H */
