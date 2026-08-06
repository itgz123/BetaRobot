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
 * 实例定义宏（仿 animal_def，向 app 暴露统一句柄）：
 *   COMM_DEF(uart1, MEDIA_USART, PROTO_CUSTOM);
 *     一次定义 3 个实例：介质派生实例 uart1_media、协议派生实例 uart1_proto、
 *     CommInstance 基类实例 uart1（内含两个 void 指针分别连接两者）。
 * 定义方在自身初始化函数里调用 CommRegister(&uart1) 完成 register+config。
 * 派生结构体内嵌基类作首成员（偏移 0），void 指针指向派生实例，
 * 可安全 cast 成基类指针使用（COMM_CONTAINER_OF）。
 */

#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>
#include <stddef.h>
#include "app_cfg.h" /* DRV_COMM_USED 总开关（仿 DAEMON_USED） */

/* CommInstance 前向声明（未定义 DRV_COMM_USED 时也可作指针参数） */
typedef struct CommInstance CommInstance;

/**
 * @brief 通信框架引擎初始化（仿 DaemonInit，供 function_in_main_c 调用）
 *
 * 只做引擎级初始化（EngineInit：自建 RX 任务、清空路由表）。无参。
 * COMM_DEF 定义的实例由定义方自己接线：调用 CommRegister(inst) 完成
 * 该实例的 register + config（见 comm_instance.h）。
 * 未定义 DRV_COMM_USED 时本函数为空操作，且整个 comm 模块（含 RX 任务）
 * 不编译、不占资源。
 */
void CommInit(void);

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
#ifndef COMM_RX_STACK_SIZE
#define COMM_RX_STACK_SIZE 512   /* EngineRxTask 栈（字） */
#endif
#ifndef COMM_RX_TASK_PRIORITY
#define COMM_RX_TASK_PRIORITY 4  /* RX 任务优先级（高于业务任务） */
#endif

/*=========== CommInit 默认接线配置（app_cfg.h 可覆盖） ===========*/
#ifndef COMM_DEFAULT_UART
#define COMM_DEFAULT_UART UART_1     /* 默认介质：板载 UART */
#endif
#ifndef COMM_DEFAULT_MEDIA_ID
#define COMM_DEFAULT_MEDIA_ID 1      /* 默认介质 id */
#endif
#ifndef COMM_DEFAULT_PROTO_ID
#define COMM_DEFAULT_PROTO_ID 1      /* 默认协议 id */
#endif
#ifndef COMM_DEFAULT_MAX_PAYLOAD
#define COMM_DEFAULT_MAX_PAYLOAD 8   /* 默认载荷上限（字节） */
#endif

/* 派生结构体内嵌基类作首成员（偏移 0），基类指针可直接反推派生实例 */
#define COMM_CONTAINER_OF(ptr, type) ((type *)(void *)(ptr))

#include "media/comm_media.h"
#include "proto/comm_proto.h"
#include "engine/comm_ring.h"
#include "engine/comm_engine.h"

/* CommInstance 完整定义、COMM_DEF 宏、COMM_INSTANCE_MEDIA/PROTO、
   CommSend/CommRegisterConsumer 声明见 comm_instance.h（app 唯一入口）。
   原因：它们依赖 ProtoType_e / EngineConsumer_Config_s 等派生相关类型，
   放 drv_comm.h 会在派生头反向 include 基类头时产生 include 环。 */

#endif /* DRV_COMM_H */
