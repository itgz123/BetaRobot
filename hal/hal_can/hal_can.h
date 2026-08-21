/**
 * @file hal_can.h
 * @brief CAN 外设重配置层（可选覆盖层）
 *
 * @note 职责仅封装"重配置"：在 CubeMX 初始化（MX_FDCAN1/2/3_Init / MX_CAN1/2_Init）之后，
 *       用配置结构体再次覆盖外设寄存器参数（波特率 / FrameFormat / 各元素数量尺寸等）。
 *       - 即使不调用 hal_can 的函数，CubeMX 的初始化依然生效，hal_can 是可选覆盖层。
 *       - 不内置默认值，配置由 bsp 层逐路 CAN 显式提供（见 bsp_map 的 can_cfg_map）。
 *       - 无状态，handle 一律参数传入。
 */

#ifndef __HAL_CAN_H
#define __HAL_CAN_H

#include "main.h"

/*------------- FDCAN 重配置 --------------*/

#if defined(HAL_FDCAN_MODULE_ENABLED)

/**
 * @brief FDCAN 重配置结构体（对应用户配置表的全部可重配置项）
 * @note 字段值直接对应 FDCAN_InitTypeDef；位时序为原始 TQ 数值，元素尺寸为 FDCAN_DATA_BYTES_xx 枚举
 */
typedef struct
{
    uint32_t frame_format;               /* FDCAN_FRAME_CLASSIC / FDCAN_FD_NO_BRS / FDCAN_FD_BRS */
    uint32_t mode;                       /* FDCAN_MODE_NORMAL ... */
    FunctionalState auto_retransmission; /* ENABLE / DISABLE */
    FunctionalState transmit_pause;      /* ENABLE / DISABLE */
    FunctionalState protocol_exception;  /* ENABLE / DISABLE */

    uint32_t nominal_prescaler;       /* 1..512 */
    uint32_t nominal_sync_jump_width; /* 1..128 */
    uint32_t nominal_time_seg1;       /* 2..256 */
    uint32_t nominal_time_seg2;       /* 2..128 */

    uint32_t data_prescaler;       /* 1..32 */
    uint32_t data_sync_jump_width; /* 1..16 */
    uint32_t data_time_seg1;       /* 1..32 */
    uint32_t data_time_seg2;       /* 1..16 */

    uint32_t message_ram_offset;      /* 0..2560（字），FDCAN1/2/3 必须不同 */
    uint32_t std_filters_nbr;         /* 0..128 */
    uint32_t ext_filters_nbr;         /* 0..64 */
    uint32_t rx_fifo0_elmts_nbr;      /* 0..64 */
    uint32_t rx_fifo0_elmt_size;      /* FDCAN_DATA_BYTES_8..64 */
    uint32_t rx_fifo1_elmts_nbr;      /* 0..64 */
    uint32_t rx_fifo1_elmt_size;      /* FDCAN_DATA_BYTES_8..64 */
    uint32_t rx_buffers_nbr;          /* 0..64 */
    uint32_t rx_buffer_size;          /* FDCAN_DATA_BYTES_8..64 */
    uint32_t tx_events_nbr;           /* 0..32 */
    uint32_t tx_buffers_nbr;          /* 0..32 */
    uint32_t tx_fifo_queue_elmts_nbr; /* 0..32 */
    uint32_t tx_fifo_queue_mode;      /* FDCAN_TX_FIFO_OPERATION / FDCAN_TX_QUEUE_OPERATION */
    uint32_t tx_elmt_size;            /* FDCAN_DATA_BYTES_8..64 */
} HalCan_FDCAN_Config_s;

/**
 * @brief 重配置 FDCAN 外设（覆盖 CubeMX 初始化）
 * @param hfdcan FDCAN 句柄（&hfdcan1/2/3）
 * @param cfg    配置结构体指针
 * @retval HAL_OK 成功
 * @retval HAL_ERROR 参数非法或 HAL_FDCAN_Init 失败
 *
 * @note 逐字段覆盖 hfdcan->Init.* 后重新调用 HAL_FDCAN_Init。
 *       再次 Init 不会重复配置时钟/GPIO/NVIC（State 已是 READY），
 *       但会清空分配区 Message RAM 并重写 SIDFC/XIDFC 等基址，
 *       故本函数必须在配置过滤器之前调用。
 */
HAL_StatusTypeDef HalCanReconfigureFdcan(FDCAN_HandleTypeDef *hfdcan, const HalCan_FDCAN_Config_s *cfg);

#endif /* HAL_FDCAN_MODULE_ENABLED */

/*------------- BxCAN 重配置 --------------*/

#if defined(HAL_CAN_MODULE_ENABLED)

/**
 * @brief BxCAN 重配置结构体（对应用户配置表的全部可重配置项）
 * @note 位时序为 HAL 编码常量（同 CubeMX can.c 写法），如 CAN_SJW_1TQ / CAN_BS1_10TQ
 */
typedef struct
{
    uint32_t prescaler;                     /* 1..1024 */
    uint32_t mode;                          /* CAN_MODE_NORMAL ... */
    uint32_t sync_jump_width;               /* CAN_SJW_1TQ..CAN_SJW_4TQ */
    uint32_t time_seg1;                     /* CAN_BS1_1TQ..CAN_BS1_16TQ */
    uint32_t time_seg2;                     /* CAN_BS2_1TQ..CAN_BS2_8TQ */
    FunctionalState time_triggered_mode;    /* ENABLE / DISABLE */
    FunctionalState auto_bus_off;           /* ENABLE / DISABLE */
    FunctionalState auto_wake_up;           /* ENABLE / DISABLE */
    FunctionalState auto_retransmission;    /* ENABLE / DISABLE */
    FunctionalState receive_fifo_locked;    /* ENABLE / DISABLE */
    FunctionalState transmit_fifo_priority; /* ENABLE / DISABLE */
} HalCan_CAN_Config_s;

/**
 * @brief 重配置 BxCAN 外设（覆盖 CubeMX 初始化）
 * @param hcan CAN 句柄（&hcan1/2）
 * @param cfg  配置结构体指针
 * @retval HAL_OK 成功
 * @retval HAL_ERROR 参数非法或 HAL_CAN_Init 失败
 *
 * @note 逐字段覆盖 hcan->Init.* 后重新调用 HAL_CAN_Init。
 *       再次 Init 不会重复配置时钟/GPIO/NVIC（State 已是 READY），
 *       但会重置外设状态，故本函数必须在配置过滤器之前调用。
 */
HAL_StatusTypeDef HalCanReconfigureBxcan(CAN_HandleTypeDef *hcan, const HalCan_CAN_Config_s *cfg);

#endif /* HAL_CAN_MODULE_ENABLED */

#endif /* __HAL_CAN_H */
