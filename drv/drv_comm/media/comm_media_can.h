/**
 * @file comm_media_can.h
 * @brief 硬件层 CAN 后端：包装 bsp_can（BxCAN / FDCAN 通吃）
 */

#ifndef DRV_COMM_MEDIA_CAN_H
#define DRV_COMM_MEDIA_CAN_H

#include "drv_comm.h"
#include "bsp_can.h"

typedef struct
{
    CommMedia media; /* 首成员：内嵌基类 */
    CANInstance can;
} CommMediaCan;

typedef struct
{
    BoardCAN_e can_e;          /* 板载 CAN 枚举（bsp_map） */
    uint32_t tx_id;            /* 发送标准 ID */
    CANFilterMode_e filter_mode; /* 过滤器模式（掩码/列表） */
    uint32_t rx_id_list[4];    /* 列表模式接收 ID；CAN_ID_UNUSED 为无效槽 */
    uint32_t rx_mask;          /* 掩码模式掩码值 */
    uint8_t media_id;          /* 引擎路由用 */
    uint8_t unpack_in_isr;     /* 1=ISR 直通解包 */
} CommMediaCan_Config_s;

int8_t MediaCanConfig(CommMedia *inst, const CommMediaCan_Config_s *cfg);

#endif /* DRV_COMM_MEDIA_CAN_H */
