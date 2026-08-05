/**
 * @file comm_media.h
 * @brief 硬件层（Media）：统一介质接口
 *
 * 屏蔽底层硬件差异（CAN 消息帧 / UART/USB 字节流），提供统一的
 * init/send/接收回调签名。新增介质 = 新增枚举 + TYPE_xxx 映射 +
 * 派生结构体（内嵌基类作首成员）+ 一个 media_xxx.c 后端。
 */

#ifndef DRV_COMM_MEDIA_H
#define DRV_COMM_MEDIA_H

#include "drv_comm.h"

/* 介质类型枚举（COMM_MEDIA_DEF 用） */
typedef enum : uint8_t
{
    MEDIA_CAN = 0,
    MEDIA_USART,
    MEDIA_USB,
    MEDIA_MEM,
} MediaType_e;

#define TYPE_MEDIA_CAN CommMediaCan
#define TYPE_MEDIA_USART CommMediaUsart
#define TYPE_MEDIA_USB CommMediaUsb
#define TYPE_MEDIA_MEM CommMediaMem

/* 介质语义 */
typedef enum : uint8_t
{
    MEDIA_KIND_BYTESTREAM = 0, /* UART/USB/mem：字节流，段长不定 */
    MEDIA_KIND_MESSAGE = 1,    /* CAN：消息帧，固定 8 字节 */
} MediaKind_e;

/* 接收事件 */
typedef enum : uint8_t
{
    MEDIA_EVT_RX_STREAM = 0, /* 收到一段字节 */
    MEDIA_EVT_RX_FRAME = 1,  /* 收到一帧消息（CAN） */
    MEDIA_EVT_TX_DONE = 2,
    MEDIA_EVT_ERROR = 3,
} MediaEvent_e;

/* 前向声明 */
typedef struct CommMedia CommMedia;
typedef struct CommProto CommProto;

/* 统一接收回调：所有后端统一签名，ISR 上下文调用 */
typedef void (*MediaRxCallback)(CommMedia *media, MediaEvent_e evt, const uint8_t *data, uint16_t len);

/* 介质虚函数表 */
typedef struct
{
    void (*init)(CommMedia *inst);                                                /* 注册/初始化底层 bsp 实例 */
    int8_t (*send)(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
    void (*deinit)(CommMedia *inst);
} CommMediaVTable_s;

/* 介质基类（派生结构体将其作为首成员内嵌） */
struct CommMedia
{
    const CommMediaVTable_s *vtable;        /* 必须首成员（drv_motor_base 约定） */
    MediaKind_e kind;                       /* 字节流 / 消息帧 */
    MediaType_e type;                       /* CAN / USART / USB / MEM */
    uint8_t media_id;                       /* 引擎路由用 */
    uint8_t unpack_in_isr;                  /* 1=ISR 直通解包（低延迟），0=进 RX 任务 */
    CommProto *proto_list[MEDIA_PROTO_MAX]; /* 一介质多协议 */
    uint8_t proto_count;
    MediaRxCallback rx_cb; /* 引擎设置的接收钩子（默认写 ring） */
    void *user;
};

/**
 * @brief 介质实例定义宏（仿 animal_def）
 * @param name 实例名
 * @param type 介质类型（MEDIA_CAN/MEDIA_USART/MEDIA_USB/MEDIA_MEM）
 * @note 展开为：定义派生实例 name##_child，并定义基类指针 name 指向其内嵌基类。
 *       vtable 在对应后端 Config 函数中挂接。
 * @example COMM_MEDIA_DEF(uart1, MEDIA_USART);
 */
#define COMM_MEDIA_DEF(name, type) \
    TYPE_##type name##_child;      \
    CommMedia *name = &name##_child.media

/* 接口 */
int8_t MediaRegister(CommMedia *inst);
int8_t MediaInit(CommMedia *inst);
int8_t MediaSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int8_t MediaSetRxCb(CommMedia *inst, MediaRxCallback cb);
int8_t MediaAttachProtocol(CommMedia *inst, CommProto *proto);
CommMedia *MediaFindById(uint8_t media_id);

#endif /* DRV_COMM_MEDIA_H */
