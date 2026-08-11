/**
 * @file comm_media.h
 * @brief 通信框架-硬件层（Media）基类
 *
 * 职责：把物理介质抽象为"可发送任意长度数据单元"的统一通道。
 *   - 发送：vtable->send(data)，data 任意长度数据单元
 *   - 接收：后端适配钩子把收到的数据单元交给 MediaHandleRx → rx_cb 分发
 *   - 归属：一个 media 只属于一个 comm 实例（经 parent 反查）
 * 基类不解析内容（SOF/CRC/comm_id 属协议层），不感知介质物理限制（属后端）。
 */

#ifndef COMM_MEDIA_H
#define COMM_MEDIA_H

#include <stdint.h>
#include "app_cfg.h"
/* 依赖单向：drv_comm.h → comm_media.h；本头自包含，勿 include drv_comm.h（防环） */

/* 介质类型枚举（COMM_DEF / 后端 Config 用） */
typedef enum : uint8_t
{
    MEDIA_CAN = 0,
    MEDIA_USART,
    MEDIA_USB,
} MediaType_e;

typedef struct CommMedia CommMedia;

/* 统一接收回调：收到一条完整数据单元时调用（comm 层挂接收钩子） */
typedef void (*MediaRxCallback)(CommMedia *media, const uint8_t *data);

/* 虚函数表（派生结构体首成员为 vtable，drv_motor_base 约定） */
typedef struct
{
    int8_t (*send)(CommMedia *self, const uint8_t *data); /* 发送任意长度数据单元 */
} CommMediaVTable_s;

/* 介质基类（派生结构体内嵌作首成员） */
struct CommMedia
{
    const CommMediaVTable_s *vtable; /* 必须首成员 */
    MediaType_e type;                /* 介质类型 */
    MediaRxCallback rx_cb;           /* 接收钩子（comm 层挂 CommMediaRxHook） */
    void *parent;                    /* 指向 comm 实例 */
    void *media;                     /* 指向 bsp 实例 */
};

/* 公共接口 */
void MediaHandleRx(CommMedia *media, const uint8_t *data); /* 统一接收分发（后端适配钩子调用） */
int8_t MediaSend(CommMedia *media, const uint8_t *data);   /* 统一发送分发 */

#endif /* COMM_MEDIA_H */
