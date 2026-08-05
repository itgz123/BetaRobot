/**
 * @file comm_media_mem.h
 * @brief 硬件层内存流后端（任务间通信）
 *
 * 把任务间通信纳入与板间通信相同的分层链路：
 *   EngineSend(mem) -> 协议打包 -> MEM send（同步注入接收端）
 *   -> 引擎 RX 任务解包 -> 按 comm_id 分发给任务间消费者。
 * 发方/收方不关心底层是串口、CAN 还是内存，同一套语义。
 */

#ifndef DRV_COMM_MEDIA_MEM_H
#define DRV_COMM_MEDIA_MEM_H

#include "drv_comm.h"

typedef struct
{
    CommMedia media; /* 首成员：内嵌基类 */
} CommMediaMem;

typedef struct
{
    uint8_t media_id;      /* 引擎路由用 */
    uint8_t unpack_in_isr; /* 1=ISR 直通解包 */
} CommMediaMem_Config_s;

int8_t MediaMemConfig(CommMedia *inst, const CommMediaMem_Config_s *cfg);

#endif /* DRV_COMM_MEDIA_MEM_H */
