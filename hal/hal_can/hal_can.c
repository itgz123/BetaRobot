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
    if ((init->FrameFormat != FDCAN_FRAME_CLASSIC) &&
        (init->FrameFormat != FDCAN_FRAME_FD_NO_BRS) &&
        (init->FrameFormat != FDCAN_FRAME_FD_BRS))
    {
        return HAL_ERROR;
    }
    if ((init->Mode != FDCAN_MODE_NORMAL) &&
        (init->Mode != FDCAN_MODE_RESTRICTED_OPERATION) &&
        (init->Mode != FDCAN_MODE_BUS_MONITORING) &&
        (init->Mode != FDCAN_MODE_INTERNAL_LOOPBACK) &&
        (init->Mode != FDCAN_MODE_EXTERNAL_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(init->AutoRetransmission) ||
        !HalCanIsValidFunctionalState(init->TransmitPause) ||
        !HalCanIsValidFunctionalState(init->ProtocolException))
    {
        return HAL_ERROR;
    }

    if ((init->NominalPrescaler < 1U) || (init->NominalPrescaler > 512U) ||
        (init->NominalSyncJumpWidth < 1U) || (init->NominalSyncJumpWidth > 128U) ||
        (init->NominalTimeSeg1 < 1U) || (init->NominalTimeSeg1 > 256U) ||
        (init->NominalTimeSeg2 < 1U) || (init->NominalTimeSeg2 > 128U))
    {
        return HAL_ERROR;
    }
    if ((init->DataPrescaler < 1U) || (init->DataPrescaler > 32U) ||
        (init->DataSyncJumpWidth < 1U) || (init->DataSyncJumpWidth > 16U) ||
        (init->DataTimeSeg1 < 1U) || (init->DataTimeSeg1 > 32U) ||
        (init->DataTimeSeg2 < 1U) || (init->DataTimeSeg2 > 16U))
    {
        return HAL_ERROR;
    }

    if (init->MessageRAMOffset > 2560U) /* H723 FDCAN Message RAM 共 2560 字 */
    {
        return HAL_ERROR;
    }
    if ((init->StdFiltersNbr > 128U) || (init->ExtFiltersNbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((init->RxFifo0ElmtsNbr > 64U) || (init->RxFifo1ElmtsNbr > 64U) ||
        (init->RxBuffersNbr > 64U))
    {
        return HAL_ERROR;
    }
    if ((init->TxEventsNbr > 32U) ||
        ((init->TxBuffersNbr + init->TxFifoQueueElmtsNbr) > 32U))
    {
        return HAL_ERROR;
    }
    if ((init->RxFifo0ElmtsNbr > 0U) && !HalCanIsValidFdcanDataSize(init->RxFifo0ElmtSize))
    {
        return HAL_ERROR;
    }
    if ((init->RxFifo1ElmtsNbr > 0U) && !HalCanIsValidFdcanDataSize(init->RxFifo1ElmtSize))
    {
        return HAL_ERROR;
    }
    if ((init->RxBuffersNbr > 0U) && !HalCanIsValidFdcanDataSize(init->RxBufferSize))
    {
        return HAL_ERROR;
    }
    if ((init->TxBuffersNbr + init->TxFifoQueueElmtsNbr) > 0U)
    {
        if (!HalCanIsValidFdcanDataSize(init->TxElmtSize))
        {
            return HAL_ERROR;
        }
    }
    if ((init->TxFifoQueueMode != FDCAN_TX_FIFO_OPERATION) &&
        (init->TxFifoQueueMode != FDCAN_TX_QUEUE_OPERATION))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hfdcan->Init.FrameFormat = init->FrameFormat;
    hfdcan->Init.Mode = init->Mode;
    hfdcan->Init.AutoRetransmission = init->AutoRetransmission;
    hfdcan->Init.TransmitPause = init->TransmitPause;
    hfdcan->Init.ProtocolException = init->ProtocolException;

    hfdcan->Init.NominalPrescaler = init->NominalPrescaler;
    hfdcan->Init.NominalSyncJumpWidth = init->NominalSyncJumpWidth;
    hfdcan->Init.NominalTimeSeg1 = init->NominalTimeSeg1;
    hfdcan->Init.NominalTimeSeg2 = init->NominalTimeSeg2;

    hfdcan->Init.DataPrescaler = init->DataPrescaler;
    hfdcan->Init.DataSyncJumpWidth = init->DataSyncJumpWidth;
    hfdcan->Init.DataTimeSeg1 = init->DataTimeSeg1;
    hfdcan->Init.DataTimeSeg2 = init->DataTimeSeg2;

    hfdcan->Init.MessageRAMOffset = init->MessageRAMOffset;
    hfdcan->Init.StdFiltersNbr = init->StdFiltersNbr;
    hfdcan->Init.ExtFiltersNbr = init->ExtFiltersNbr;
    hfdcan->Init.RxFifo0ElmtsNbr = init->RxFifo0ElmtsNbr;
    hfdcan->Init.RxFifo0ElmtSize = init->RxFifo0ElmtSize;
    hfdcan->Init.RxFifo1ElmtsNbr = init->RxFifo1ElmtsNbr;
    hfdcan->Init.RxFifo1ElmtSize = init->RxFifo1ElmtSize;
    hfdcan->Init.RxBuffersNbr = init->RxBuffersNbr;
    hfdcan->Init.RxBufferSize = init->RxBufferSize;
    hfdcan->Init.TxEventsNbr = init->TxEventsNbr;
    hfdcan->Init.TxBuffersNbr = init->TxBuffersNbr;
    hfdcan->Init.TxFifoQueueElmtsNbr = init->TxFifoQueueElmtsNbr;
    hfdcan->Init.TxFifoQueueMode = init->TxFifoQueueMode;
    hfdcan->Init.TxElmtSize = init->TxElmtSize;

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
    if ((init->Mode != CAN_MODE_NORMAL) &&
        (init->Mode != CAN_MODE_LOOPBACK) &&
        (init->Mode != CAN_MODE_SILENT) &&
        (init->Mode != CAN_MODE_SILENT_LOOPBACK))
    {
        return HAL_ERROR;
    }
    if ((init->SyncJumpWidth != CAN_SJW_1TQ) &&
        (init->SyncJumpWidth != CAN_SJW_2TQ) &&
        (init->SyncJumpWidth != CAN_SJW_4TQ))
    {
        return HAL_ERROR;
    }
    if ((init->TimeSeg1 > CAN_BS1_16TQ) || (init->TimeSeg2 > CAN_BS2_8TQ))
    {
        return HAL_ERROR;
    }
    if ((init->Prescaler < 1U) || (init->Prescaler > 1024U))
    {
        return HAL_ERROR;
    }
    if (!HalCanIsValidFunctionalState(init->TimeTriggeredMode) ||
        !HalCanIsValidFunctionalState(init->AutoBusOff) ||
        !HalCanIsValidFunctionalState(init->AutoWakeUp) ||
        !HalCanIsValidFunctionalState(init->AutoRetransmission) ||
        !HalCanIsValidFunctionalState(init->ReceiveFifoLocked) ||
        !HalCanIsValidFunctionalState(init->TransmitFifoPriority))
    {
        return HAL_ERROR;
    }

    /* 逐字段覆盖 CubeMX 初始化的配置 */
    hcan->Init.Prescaler = init->Prescaler;
    hcan->Init.Mode = init->Mode;
    hcan->Init.SyncJumpWidth = init->SyncJumpWidth;
    hcan->Init.TimeSeg1 = init->TimeSeg1;
    hcan->Init.TimeSeg2 = init->TimeSeg2;
    hcan->Init.TimeTriggeredMode = init->TimeTriggeredMode;
    hcan->Init.AutoBusOff = init->AutoBusOff;
    hcan->Init.AutoWakeUp = init->AutoWakeUp;
    hcan->Init.AutoRetransmission = init->AutoRetransmission;
    hcan->Init.ReceiveFifoLocked = init->ReceiveFifoLocked;
    hcan->Init.TransmitFifoPriority = init->TransmitFifoPriority;

    /* 重新初始化外设（State=READY 不会重跑 MspInit） */
    return HAL_CAN_Init(hcan);
}

#endif /* HAL_CAN_MODULE_ENABLED */
