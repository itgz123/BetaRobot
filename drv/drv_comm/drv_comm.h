/**
 * @file drv_comm.h
 * @brief 通用通信驱动框架总入口头文件
 *
 * 三层架构（参考网络协议分层 / OSI 7 层映射）：
 *   - 硬件层 media/ : 抽象所有硬件介质（CAN/UART/USB/内存流）为统一接口，
 *                     新增一种介质只需新增一个后端（vtable + 适配钩子）。
 *   - 协议层 proto/ : 处理协议相关（帧格式/打包解包/校验/粘包拆包/多帧重组）。
 *   - 引擎层 engine/: 统一函数调用（vtable 分发）+ 消息路由 + ISR->任务通道。
 *
 * 消息路由使用 comm_id（位宽可配置），类似 "IP地址+端口" 简化为一个 ID，
 * 直接表示"某个设备的某个应用"。解包成功的一帧按 comm_id 分发给消费者。
 *
 * 实例定义宏（仿 animal_def(name, type)）：
 *   COMM_MEDIA_DEF(uart1, MEDIA_USART);   // 定义派生实例 uart1_child + 基类指针 uart1
 *   COMM_PROTO_DEF(p1, PROTO_CUSTOM);     // 定义派生实例 p1_child + 基类指针 p1
 * 派生结构体内嵌基类作首成员，基类 vtable 指向派生 vtable，双向可定位。
 */

#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>
#include <stddef.h>

/*============================================
 *              comm_id 位宽配置
 *============================================*/
#ifndef COMM_ID_LEN
#define COMM_ID_LEN 1 /* 1 = uint8_t, 2 = uint16_t */
#endif

#if COMM_ID_LEN == 1
typedef uint8_t CommId_t;
#define COMM_ID_ANY ((CommId_t)0xFF) /* 消费者通配匹配（注意此时 0xFF 不可用作有效路由） */
#elif COMM_ID_LEN == 2
typedef uint16_t CommId_t;
#define COMM_ID_ANY ((CommId_t)0xFFFF)
#else
#error "COMM_ID_LEN must be 1 or 2"
#endif

/*============================================
 *              数量配置（app_cfg.h 可覆盖）
 *============================================*/
#ifndef MEDIA_INSTANCE_NUM
#define MEDIA_INSTANCE_NUM 8
#endif
#ifndef PROTO_INSTANCE_NUM
#define PROTO_INSTANCE_NUM 8
#endif
#ifndef ENGINE_CONSUMER_NUM
#define ENGINE_CONSUMER_NUM 16
#endif
#ifndef MEDIA_PROTO_MAX
#define MEDIA_PROTO_MAX 4
#endif
#ifndef COMM_RING_CHUNK_NUM
#define COMM_RING_CHUNK_NUM 16
#endif
#ifndef COMM_RING_CHUNK_SIZE
#define COMM_RING_CHUNK_SIZE 64
#endif
#ifndef COMM_TX_BUF_SIZE
#define COMM_TX_BUF_SIZE 256
#endif

/* 派生结构体内嵌基类作首成员（偏移 0），基类指针可直接反推派生实例 */
#define COMM_CONTAINER_OF(ptr, type) ((type *)(void *)(ptr))

#include "media/comm_media.h"
#include "proto/comm_proto.h"
#include "engine/comm_ring.h"
#include "engine/comm_engine.h"
/* 注意：派生类型头（comm_media_can/usart/usb/mem、comm_proto_custom/sbus/seasky/referee）
   需由使用方显式 include，避免 include 环（派生头依赖基类完整定义）。 */

#endif /* DRV_COMM_H */
