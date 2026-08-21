/**
 * @file hal_can.c
 * @brief CAN 外设重配置层实现
 *
 * @note 实现策略：逐字段覆盖 handle->Init.* 后重新调用 HAL_FDCAN_Init / HAL_CAN_Init。
 *       - 再次 Init 时 State 已是 READY，HAL 不会重复执行 MspInit（时钟/GPIO/NVIC 保持不变）。
 *       - HAL_FDCAN_Init 会清空分配区 Message RAM 并重写 SIDFC/XIDFC 等基址，
 *         因此本层必须在配置过滤器之前调用（由 bsp_can 保证时序）。
 *       - 本工程未开启 USE_FULL_ASSERT，HAL 的 assert_param 是空操作，
 *         故先在本层做参数范围校验，非法参数直接返回 HAL_ERROR 而不静默写入寄存器。
 */

#include "hal_can.h"

/*------------- 局部校验辅助（两种 CAN 分支共用） --------------*/

static uint8_t HalCanIsValidFunctionalState(FunctionalState state)
{
    return ((state == ENABLE) || (state == DISABLE));
}

#if defined(HAL_FDCAN_MODULE_ENABLED)

/* FDCAN 元素尺寸合法集合：FDCAN_DATA_BYTES_8/12/16/20/24/32/48/64 */
static uint8_t HalCanIsValidFdcanDataSize(uint32_t size)
{
    switch (size)
    {
    case FDCAN_DATA_BYTES_8:
    case FDCAN_DATA_BYTES_12:
    case FDCAN_DATA_BYTES_16:
    case FDCAN_DATA_BYTES_20:
    case FDCAN_DATA_BYTES_24:
    case FDCAN_DATA_BYTES_32:
    case FDCAN_DATA_BYTES_48:
    case FDCAN_DATA_BYTES_64:
        return 1;
    default:
        return 0;
    }
}

/*------------- FDCAN 重配置实现 --------------*/

HAL_StatusTypeDef HalCanReconfigureFdcan(FDCAN_HandleTypeDef *hfdcan, const HalCan_FDCAN_Config_s *cfg)
{
    if ((hfdcan == NULL) || (cfg == NULL))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if ((cfg->frame_format != FDCAN_FRAME_CLASSIC) &&
        (cfg->frame_format != FDCAN_FRAME_FD_NO_BRS) &&
        (cfg->frame_format != FDCAN_FRAME_FD_BRS))
    {
        return HAL_ERROR;
    }
    if ((cfg->mode != FDCAN_MODE_NORMAL) &&
        (cfg->mode != FDCAN_MODE_RESTRICTED_OPERATION) &&
        (cfg->mode != FDCAN_MODE_BUS_MONITORING) &&
        (cfg->mode != FDCAN_MODE_INTERNAL_LOOPBACK) &&
        (cfg->mode != FDCAN_MODE_EXTERNAL_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(cfg->auto_retransmission) ||
        !HalCanIsValidFunctionalState(cfg->transmit_pause) ||
        !HalCanIsValidFunctionalState(cfg->protocol_exception))
    {
        return HAL_ERROR;
    }

    if ((cfg->nominal_prescaler < 1U) || (cfg->nominal_prescaler > 512U) ||
        (cfg->nominal_sync_jump_width < 1U) || (cfg->nominal_sync_jump_width > 128U) ||
        (cfg->nominal_time_seg1 < 1U) || (cfg->nominal_time_seg1 > 256U) ||
        (cfg->nominal_time_seg2 < 1U) || (cfg->nominal_time_seg2 > 128U))
    {
        return HAL_ERROR;
    }
    if ((cfg->data_prescaler < 1U) || (cfg->data_prescaler > 32U) ||
        (cfg->data_sync_jump_width < 1U) || (cfg->data_sync_jump_width > 16U) ||
        (cfg->data_time_seg1 < 1U) || (cfg->data_time_seg1 > 32U) ||
        (cfg->data_time_seg2 < 1U) || (cfg->data_time_seg2 > 16U))
    {
        return HAL_ERROR;
    }

    if (cfg->message_ram_offset > 2560U) /* H723 FDCAN Message RAM 共 2560 字 */
    {
        return HAL_ERROR;
    }
    if ((cfg->std_filters_nbr > 128U) || (cfg->ext_filters_nbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((cfg->rx_fifo0_elmts_nbr > 64U) || (cfg->rx_fifo1_elmts_nbr > 64U) ||
        (cfg->rx_buffers_nbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((cfg->tx_events_nbr > 32U) ||
        ((cfg->tx_buffers_nbr + cfg->tx_fifo_queue_elmts_nbr) > 32U))
    {
        return HAL_ERROR;
    }
    if ((cfg->rx_fifo0_elmts_nbr > 0U) && !HalCanIsValidFdcanDataSize(cfg->rx_fifo0_elmt_size))
    {
        return HAL_ERROR;
    }
    if ((cfg->rx_fifo1_elmts_nbr > 0U) && !HalCanIsValidFdcanDataSize(cfg->rx_fifo1_elmt_size))
    {
        return HAL_ERROR;
    }
    if ((cfg->rx_buffers_nbr > 0U) && !HalCanIsValidFdcanDataSize(cfg->rx_buffer_size))
    {
        return HAL_ERROR;
    }
    if ((cfg->tx_buffers_nbr + cfg->tx_fifo_queue_elmts_nbr) > 0U)
    {
        if (!HalCanIsValidFdcanDataSize(cfg->tx_elmt_size))
        {
            return HAL_ERROR;
        }
    }
    if ((cfg->tx_fifo_queue_mode != FDCAN_TX_FIFO_OPERATION) &&
        (cfg->tx_fifo_queue_mode != FDCAN_TX_QUEUE_OPERATION))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hfdcan->Init.FrameFormat = cfg->frame_format;
    hfdcan->Init.Mode = cfg->mode;
    hfdcan->Init.AutoRetransmission = cfg->auto_retransmission;
    hfdcan->Init.TransmitPause = cfg->transmit_pause;
    hfdcan->Init.ProtocolException = cfg->protocol_exception;

    hfdcan->Init.NominalPrescaler = cfg->nominal_prescaler;
    hfdcan->Init.NominalSyncJumpWidth = cfg->nominal_sync_jump_width;
    hfdcan->Init.NominalTimeSeg1 = cfg->nominal_time_seg1;
    hfdcan->Init.NominalTimeSeg2 = cfg->nominal_time_seg2;

    hfdcan->Init.DataPrescaler = cfg->data_prescaler;
    hfdcan->Init.DataSyncJumpWidth = cfg->data_sync_jump_width;
    hfdcan->Init.DataTimeSeg1 = cfg->data_time_seg1;
    hfdcan->Init.DataTimeSeg2 = cfg->data_time_seg2;

    hfdcan->Init.MessageRAMOffset = cfg->message_ram_offset;
    hfdcan->Init.StdFiltersNbr = cfg->std_filters_nbr;
    hfdcan->Init.ExtFiltersNbr = cfg->ext_filters_nbr;
    hfdcan->Init.RxFifo0ElmtsNbr = cfg->rx_fifo0_elmts_nbr;
    hfdcan->Init.RxFifo0ElmtSize = cfg->rx_fifo0_elmt_size;
    hfdcan->Init.RxFifo1ElmtsNbr = cfg->rx_fifo1_elmts_nbr;
    hfdcan->Init.RxFifo1ElmtSize = cfg->rx_fifo1_elmt_size;
    hfdcan->Init.RxBuffersNbr = cfg->rx_buffers_nbr;
    hfdcan->Init.RxBufferSize = cfg->rx_buffer_size;
    hfdcan->Init.TxEventsNbr = cfg->tx_events_nbr;
    hfdcan->Init.TxBuffersNbr = cfg->tx_buffers_nbr;
    hfdcan->Init.TxFifoQueueElmtsNbr = cfg->tx_fifo_queue_elmts_nbr;
    hfdcan->Init.TxFifoQueueMode = cfg->tx_fifo_queue_mode;
    hfdcan->Init.TxElmtSize = cfg->tx_elmt_size;

    /* 重新初始化外设（State=READY 不会重跑 MspInit；会重写 Message RAM 布局） */
    return HAL_FDCAN_Init(hfdcan);
}

#endif /* HAL_FDCAN_MODULE_ENABLED */

#if defined(HAL_CAN_MODULE_ENABLED)

/*------------- BxCAN 重配置实现 --------------*/

HAL_StatusTypeDef HalCanReconfigureBxcan(CAN_HandleTypeDef *hcan, const HalCan_CAN_Config_s *cfg)
{
    if ((hcan == NULL) || (cfg == NULL))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if ((cfg->mode != CAN_MODE_NORMAL) &&
        (cfg->mode != CAN_MODE_LOOPBACK) &&
        (cfg->mode != CAN_MODE_SILENT) &&
        (cfg->mode != CAN_MODE_SILENT_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if ((cfg->sync_jump_width != CAN_SJW_1TQ) &&
        (cfg->sync_jump_width != CAN_SJW_2TQ) &&
        (cfg->sync_jump_width != CAN_SJW_4TQ))
    {
        return HAL_ERROR;
    }
    if ((cfg->time_seg1 > CAN_BS1_16TQ) || (cfg->time_seg2 > CAN_BS2_8TQ))
    {
        return HAL_ERROR;
    }
    if ((cfg->prescaler < 1U) || (cfg->prescaler > 1024U))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(cfg->time_triggered_mode) ||
        !HalCanIsValidFunctionalState(cfg->auto_bus_off) ||
        !HalCanIsValidFunctionalState(cfg->auto_wake_up) ||
        !HalCanIsValidFunctionalState(cfg->auto_retransmission) ||
        !HalCanIsValidFunctionalState(cfg->receive_fifo_locked) ||
        !HalCanIsValidFunctionalState(cfg->transmit_fifo_priority))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hcan->Init.Prescaler = cfg->prescaler;
    hcan->Init.Mode = cfg->mode;
    hcan->Init.SyncJumpWidth = cfg->sync_jump_width;
    hcan->Init.TimeSeg1 = cfg->time_seg1;
    hcan->Init.TimeSeg2 = cfg->time_seg2;
    hcan->Init.TimeTriggeredMode = cfg->time_triggered_mode;
    hcan->Init.AutoBusOff = cfg->auto_bus_off;
    hcan->Init.AutoWakeUp = cfg->auto_wake_up;
    hcan->Init.AutoRetransmission = cfg->auto_retransmission;
    hcan->Init.ReceiveFifoLocked = cfg->receive_fifo_locked;
    hcan->Init.TransmitFifoPriority = cfg->transmit_fifo_priority;

    /* 重新初始化外设（State=READY 不会重跑 MspInit） */
    return HAL_CAN_Init(hcan);
}

#endif /* HAL_CAN_MODULE_ENABLED */
