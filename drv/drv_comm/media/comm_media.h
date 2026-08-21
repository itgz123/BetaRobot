/**
 * @file comm_media.h
 * @brief 通信框架-硬件层（Media）基类
 *
 * 职责：把物理介质抽象为"可发送任意长度数据单元"的统一通道。
 *   - 发送：vtable->send(data)，data 任意长度数据单元
 *   - 接收：后端适配钩子把收到的数据单元直接交给 comm 层接收入口
 *           （CommMediaRxHook，经 parent 反查），media 基类不做接收分发
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
    MEDIA_CAN = 0, /* 旧占位：无实现；保留为 0 使未初始化 media_type 落 default 大声失败 */
    MEDIA_USART,
    MEDIA_USB,
    MEDIA_USB_SIMPLE, /* USB(CDC) 短帧免序号版：整帧 ≤ 63B 免分包序号单包透传；> 63B 行为同 MEDIA_USB */
    MEDIA_CAN_PKT0,   /* 经典 CAN 8B 分包：data[0]=分包序号，data[1..7]=数据片 */
    MEDIA_CAN_IDSEQ,  /* 经典 CAN 8B 分包：CAN ID 低 seq_bits 位为分包序号（ID = base_id + seq） */
} MediaType_e;

typedef struct CommMedia CommMedia;

/* 虚函数表（派生结构体首成员为 vtable，drv_motor_base 约定） */
typedef struct
{
    int8_t (*send)(CommMedia *self, const uint8_t *data); /* 发送任意长度数据单元 */
} CommMediaVTable_s;

/* 介质基类（派生结构体内嵌作首成员） */
struct CommMedia
{
    const CommMediaVTable_s *vtable; /* 必须首成员 */
    void *parent;                    /* 指向 comm 实例（接收分发经此反查） */
    void *media;                     /* 指向 bsp 实例 */
};

/* 公共接口 */
int8_t MediaSend(CommMedia *media, const uint8_t *data); /* 统一发送分发（vtable 转发；发送缓冲/长度由各后端自持：USART 拷入自持 staging，USB 直接引用 data 分包） */

#endif /* COMM_MEDIA_H */
