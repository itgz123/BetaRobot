/**
 * @file hal_can.c
 * @brief CAN 外设重配置层实现
 *
 * @note 实现策略：逐字段覆盖 handle->Init.* 后重新调用 HAL_FDCAN_Init / HAL_CAN_Init。
 *       - 再次 Init 时 State 已是 READY，HAL 不会重复执行 MspInit（时钟/GPIO/NVIC 保持不变）。
 *       - HAL_FDCAN_Init 会清空分配区 Message RAM 并重写 SIDFC/XIDFC 等基址，
 *         因此本层必须在配置过滤器之前调用（由调用方保证时序）。
 *       - 本层默认不被 bsp 层调用（bsp 直接使用 CubeMX 初始化结果），只作为 app 层可选覆盖入口。
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

HAL_StatusTypeDef HalCanReconfigureFdcan(FDCAN_HandleTypeDef *hfdcan, const FDCAN_InitTypeDef *init)
{
    if ((hfdcan == NULL) || (init == NULL))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if ((init->frame_format != FDCAN_FRAME_CLASSIC) &&
        (init->frame_format != FDCAN_FRAME_FD_NO_BRS) &&
        (init->frame_format != FDCAN_FRAME_FD_BRS))
    {
        return HAL_ERROR;
    }
    if ((init->mode != FDCAN_MODE_NORMAL) &&
        (init->mode != FDCAN_MODE_RESTRICTED_OPERATION) &&
        (init->mode != FDCAN_MODE_BUS_MONITORING) &&
        (init->mode != FDCAN_MODE_INTERNAL_LOOPBACK) &&
        (init->mode != FDCAN_MODE_EXTERNAL_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(init->auto_retransmission) ||
        !HalCanIsValidFunctionalState(init->transmit_pause) ||
        !HalCanIsValidFunctionalState(init->protocol_exception))
    {
        return HAL_ERROR;
    }

    if ((init->nominal_prescaler < 1U) || (init->nominal_prescaler > 512U) ||
        (init->nominal_sync_jump_width < 1U) || (init->nominal_sync_jump_width > 128U) ||
        (init->nominal_time_seg1 < 1U) || (init->nominal_time_seg1 > 256U) ||
        (init->nominal_time_seg2 < 1U) || (init->nominal_time_seg2 > 128U))
    {
        return HAL_ERROR;
    }
    if ((init->data_prescaler < 1U) || (init->data_prescaler > 32U) ||
        (init->data_sync_jump_width < 1U) || (init->data_sync_jump_width > 16U) ||
        (init->data_time_seg1 < 1U) || (init->data_time_seg1 > 32U) ||
        (init->data_time_seg2 < 1U) || (init->data_time_seg2 > 16U))
    {
        return HAL_ERROR;
    }

    if (init->message_ram_offset > 2560U) /* H723 FDCAN Message RAM 共 2560 字 */
    {
        return HAL_ERROR;
    }
    if ((init->std_filters_nbr > 128U) || (init->ext_filters_nbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((init->rx_fifo0_elmts_nbr > 64U) || (init->rx_fifo1_elmts_nbr > 64U) ||
        (init->rx_buffers_nbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((init->tx_events_nbr > 32U) ||
        ((init->tx_buffers_nbr + init->tx_fifo_queue_elmts_nbr) > 32U))
    {
        return HAL_ERROR;
    }
    if ((init->rx_fifo0_elmts_nbr > 0U) && !HalCanIsValidFdcanDataSize(init->rx_fifo0_elmt_size))
    {
        return HAL_ERROR;
    }
    if ((init->rx_fifo1_elmts_nbr > 0U) && !HalCanIsValidFdcanDataSize(init->rx_fifo1_elmt_size))
    {
        return HAL_ERROR;
    }
    if ((init->rx_buffers_nbr > 0U) && !HalCanIsValidFdcanDataSize(init->rx_buffer_size))
    {
        return HAL_ERROR;
    }
    if ((init->tx_buffers_nbr + init->tx_fifo_queue_elmts_nbr) > 0U)
    {
        if (!HalCanIsValidFdcanDataSize(init->tx_elmt_size))
        {
            return HAL_ERROR;
        }
    }
    if ((init->tx_fifo_queue_mode != FDCAN_TX_FIFO_OPERATION) &&
        (init->tx_fifo_queue_mode != FDCAN_TX_QUEUE_OPERATION))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hfdcan->Init.FrameFormat = init->frame_format;
    hfdcan->Init.Mode = init->mode;
    hfdcan->Init.AutoRetransmission = init->auto_retransmission;
    hfdcan->Init.TransmitPause = init->transmit_pause;
    hfdcan->Init.ProtocolException = init->protocol_exception;

    hfdcan->Init.NominalPrescaler = init->nominal_prescaler;
    hfdcan->Init.NominalSyncJumpWidth = init->nominal_sync_jump_width;
    hfdcan->Init.NominalTimeSeg1 = init->nominal_time_seg1;
    hfdcan->Init.NominalTimeSeg2 = init->nominal_time_seg2;

    hfdcan->Init.DataPrescaler = init->data_prescaler;
    hfdcan->Init.DataSyncJumpWidth = init->data_sync_jump_width;
    hfdcan->Init.DataTimeSeg1 = init->data_time_seg1;
    hfdcan->Init.DataTimeSeg2 = init->data_time_seg2;

    hfdcan->Init.MessageRAMOffset = init->message_ram_offset;
    hfdcan->Init.StdFiltersNbr = init->std_filters_nbr;
    hfdcan->Init.ExtFiltersNbr = init->ext_filters_nbr;
    hfdcan->Init.RxFifo0ElmtsNbr = init->rx_fifo0_elmts_nbr;
    hfdcan->Init.RxFifo0ElmtSize = init->rx_fifo0_elmt_size;
    hfdcan->Init.RxFifo1ElmtsNbr = init->rx_fifo1_elmts_nbr;
    hfdcan->Init.RxFifo1ElmtSize = init->rx_fifo1_elmt_size;
    hfdcan->Init.RxBuffersNbr = init->rx_buffers_nbr;
    hfdcan->Init.RxBufferSize = init->rx_buffer_size;
    hfdcan->Init.TxEventsNbr = init->tx_events_nbr;
    hfdcan->Init.TxBuffersNbr = init->tx_buffers_nbr;
    hfdcan->Init.TxFifoQueueElmtsNbr = init->tx_fifo_queue_elmts_nbr;
    hfdcan->Init.TxFifoQueueMode = init->tx_fifo_queue_mode;
    hfdcan->Init.TxElmtSize = init->tx_elmt_size;

    /* 重新初始化外设（State=READY 不会重跑 MspInit；会重写 Message RAM 布局） */
    return HAL_FDCAN_Init(hfdcan);
}

#endif /* HAL_FDCAN_MODULE_ENABLED */

#if defined(HAL_CAN_MODULE_ENABLED)

/*------------- BxCAN 重配置实现 --------------*/

HAL_StatusTypeDef HalCanReconfigureBxcan(CAN_HandleTypeDef *hcan, const CAN_InitTypeDef *init)
{
    if ((hcan == NULL) || (init == NULL))
    {
        return HAL_ERROR;
    }

    /* 参数范围校验（HAL assert_param 在此工程为空操作，必须自行校验） */
    if ((init->mode != CAN_MODE_NORMAL) &&
        (init->mode != CAN_MODE_LOOPBACK) &&
        (init->mode != CAN_MODE_SILENT) &&
        (init->mode != CAN_MODE_SILENT_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if ((init->sync_jump_width != CAN_SJW_1TQ) &&
        (init->sync_jump_width != CAN_SJW_2TQ) &&
        (init->sync_jump_width != CAN_SJW_4TQ))
    {
        return HAL_ERROR;
    }
    if ((init->time_seg1 > CAN_BS1_16TQ) || (init->time_seg2 > CAN_BS2_8TQ))
    {
        return HAL_ERROR;
    }
    if ((init->prescaler < 1U) || (init->prescaler > 1024U))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(init->time_triggered_mode) ||
        !HalCanIsValidFunctionalState(init->auto_bus_off) ||
        !HalCanIsValidFunctionalState(init->auto_wake_up) ||
        !HalCanIsValidFunctionalState(init->auto_retransmission) ||
        !HalCanIsValidFunctionalState(init->receive_fifo_locked) ||
        !HalCanIsValidFunctionalState(init->transmit_fifo_priority))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hcan->Init.Prescaler = init->prescaler;
    hcan->Init.Mode = init->mode;
    hcan->Init.SyncJumpWidth = init->sync_jump_width;
    hcan->Init.TimeSeg1 = init->time_seg1;
    hcan->Init.TimeSeg2 = init->time_seg2;
    hcan->Init.TimeTriggeredMode = init->time_triggered_mode;
    hcan->Init.AutoBusOff = init->auto_bus_off;
    hcan->Init.AutoWakeUp = init->auto_wake_up;
    hcan->Init.AutoRetransmission = init->auto_retransmission;
    hcan->Init.ReceiveFifoLocked = init->receive_fifo_locked;
    hcan->Init.TransmitFifoPriority = init->transmit_fifo_priority;

    /* 重新初始化外设（State=READY 不会重跑 MspInit） */
    return HAL_CAN_Init(hcan);
}

#endif /* HAL_CAN_MODULE_ENABLED */
