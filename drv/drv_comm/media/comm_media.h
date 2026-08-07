/**
 * @file comm_media.h
 * @brief 通信框架-硬件层（Media）基类
 *
 * 职责：把物理介质抽象为"可发送任意长度数据单元"的统一通道。
 *   - 发送：vtable->send(data, len)，len 任意长（CAN 后端内部自动拆帧/组头）
 *   - 接收：后端适配钩子把收到的数据单元交给 MediaHandleRx → rx_cb 分发
 *   - 实例管理：静态注册表 + 一介质多协议挂载（proto_list 供引擎遍历）
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
    MEDIA_MEM,
} MediaType_e;

/* 协议解包位置枚举（接收路径分流：ISR 直解 / 任务解包） */
typedef enum : uint8_t
{
    UNPACK_IN_ISR = 0,  /* 在 ISR 上下文直接解包（低延迟，回调须短小） */
    UNPACK_IN_TASK = 1, /* 搬入接收队列，由 RX 任务解包（不阻塞中断） */
} UnpackMode_e;

typedef struct CommMedia CommMedia;

/* 统一接收回调：收到一条完整数据单元时调用（引擎按 unpack_mode 挂对应钩子） */
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
    UnpackMode_e unpack_mode;        /* 协议解包位置（ISR 直解 / 任务解包） */
    MediaRxCallback rx_cb;           /* 引擎挂的接收钩子（按 unpack_mode 挂不同实现） */
    void *parent;                    /* 指向 comm 实例 */
    void *media;                     /* 指向 bsp 实例 */
};

/* 公共接口 */
void MediaHandleRx(CommMedia *media, const uint8_t *data); /* 统一接收分发（后端适配钩子调用） */
int8_t MediaSend(CommMedia *media, const uint8_t *data);   /* 统一发送分发 */

#endif /* COMM_MEDIA_H */
