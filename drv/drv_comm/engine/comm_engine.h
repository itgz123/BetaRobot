/**
 * @file comm_engine.h
 * @brief 引擎层（Engine）：统一函数调用 + 消息路由 + ISR->任务通道
 *
 * 统一 API：
 *   EngineInit            初始化环形缓冲/消费者表
 *   EngineAttachMedia     注册介质，挂接引擎接收钩子（ISR 写 ring）
 *   EngineAttachProtocol  把协议绑到介质上（ProtocolRegister + MediaAttachProtocol）
 *   EngineRegisterConsumer 注册消费者（按 comm_id 路由）
 *   EngineSend            统一发送入口（取介质协议打包后经介质发出）
 *   EngineRxTask          接收任务主体（由 app 包装为 RTOS 任务）
 *
 * 接收链路：bsp ISR -> 介质适配钩子 -> EngineRxHook(写 ring) -> vTaskNotifyGive
 *          -> EngineRxTask 取 chunk -> 该介质 proto_list 逐个 unpack
 *          -> 出帧按 comm_id 分发消费者 + DaemonReload
 */

#ifndef DRV_COMM_ENGINE_H
#define DRV_COMM_ENGINE_H

#include "drv_comm.h"
#include "FreeRTOS.h"
#include "queue.h"

/* 前向声明，避免 include 环（本头只用指针） */
typedef struct CommMedia CommMedia;
typedef struct CommProto CommProto;
typedef struct ProtoMessage_s ProtoMessage_s;

typedef void (*EngineConsumerFn)(const ProtoMessage_s *msg, void *ctx);

/* 消费者类型 */
typedef enum : uint8_t
{
    ENGINE_CONSUMER_QUEUE = 0, /* 载荷拷入 FreeRTOS 静态队列（item_size 需 == 载荷长度） */
    ENGINE_CONSUMER_CALLBACK,  /* 在 RX 任务上下文调用回调 */
    ENGINE_CONSUMER_LATEST,    /* 最新值覆盖缓冲（message_center 深度1 语义） */
} EngineConsumerType_e;

/* 消费者注册配置 */
typedef struct
{
    EngineConsumerType_e type;
    CommId_t comm_id;         /* 匹配的消息 ID；COMM_ID_ANY 为通配 */
    QueueHandle_t queue;      /* QUEUE */
    uint16_t item_size;       /* QUEUE：载荷定长校验（防越界） */
    EngineConsumerFn callback; /* CALLBACK */
    void *ctx;                /* CALLBACK ctx */
    uint8_t *latest_buff;     /* LATEST 缓冲 */
    uint16_t latest_size;     /* LATEST 缓冲大小 */
} EngineConsumer_Config_s;

int8_t EngineInit(void);
int8_t EngineAttachMedia(CommMedia *media);
int8_t EngineAttachProtocol(CommMedia *media, CommProto *proto);
int8_t EngineRegisterConsumer(const EngineConsumer_Config_s *cfg);
int8_t EngineSend(uint8_t media_id, CommId_t comm_id, const uint8_t *payload, uint16_t len);
void EngineRxTask(void); /* 自带 for(;;)，由 app 用 TASK_INSTANCE_DEF 包装 */

#endif /* DRV_COMM_ENGINE_H */
