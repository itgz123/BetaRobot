/**
 * @file bsp_can.c
 * @brief CAN驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件初始化参数配置
 */

#include "bsp_can.h"

#ifdef BSP_CAN_MODULE_ENABLED
#if CAN_INSTANCE_NUM > 0

#include "bsp_check.h"
#include "bsp_log.h"
#include "string.h"

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN

/*------------- FDCAN 过滤器索引 --------------*/
static uint8_t s_fdcan_filter_idx[CAN_NUM_MAX] = {0};

/*------------- FDCAN 私有函数 --------------*/

static uint8_t CANFdcanDlcToLength(uint32_t dlc)
{
    switch (dlc)
    {
    case FDCAN_DLC_BYTES_1:
        return 1;
    case FDCAN_DLC_BYTES_2:
        return 2;
    case FDCAN_DLC_BYTES_3:
        return 3;
    case FDCAN_DLC_BYTES_4:
        return 4;
    case FDCAN_DLC_BYTES_5:
        return 5;
    case FDCAN_DLC_BYTES_6:
        return 6;
    case FDCAN_DLC_BYTES_7:
        return 7;
    case FDCAN_DLC_BYTES_8:
    default:
        return 8;
    }
}

static uint32_t CANLengthToFdcanDlc(uint8_t len)
{
    switch (len)
    {
    case 1:
        return FDCAN_DLC_BYTES_1;
    case 2:
        return FDCAN_DLC_BYTES_2;
    case 3:
        return FDCAN_DLC_BYTES_3;
    case 4:
        return FDCAN_DLC_BYTES_4;
    case 5:
        return FDCAN_DLC_BYTES_5;
    case 6:
        return FDCAN_DLC_BYTES_6;
    case 7:
        return FDCAN_DLC_BYTES_7;
    case 8:
    default:
        return FDCAN_DLC_BYTES_8;
    }
}

static HAL_StatusTypeDef CANFdcanAddFilter(CANInstance *instance)
{
    if (instance->can_e >= CAN_NUM_MAX)
    {
        return HAL_ERROR;
    }

    FDCAN_FilterTypeDef filter = {0};
    filter.FilterIndex = s_fdcan_filter_idx[instance->can_e]++;
    filter.FilterType = FDCAN_FILTER_DUAL;
    filter.FilterConfig = (instance->rx_id & 1U) ? FDCAN_FILTER_TO_RXFIFO0 : FDCAN_FILTER_TO_RXFIFO1;
    filter.FilterID1 = instance->rx_id;
    filter.FilterID2 = instance->rx_id;
    filter.IdType = FDCAN_STANDARD_ID;
    filter.IsCalibrationMsg = 0;

    return HAL_FDCAN_ConfigFilter(instance->map.handle, &filter);
}

static HAL_StatusTypeDef CANFdcanStartIfNeeded(FDCAN_HandleTypeDef *handle)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i]->map.handle == handle)
        {
            return HAL_OK;
        }
    }

    if (HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_FDCAN_Start(handle) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_FDCAN_ActivateNotification(handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void CANDispatchFdcanMessage(FDCAN_HandleTypeDef *hfdcan, uint32_t fifo)
{
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, fifo) > 0U)
    {
        if (HAL_FDCAN_GetRxMessage(hfdcan, fifo, &rx_header, rx_data) != HAL_OK)
        {
            return;
        }

        for (uint8_t i = 0; i < s_idx; i++)
        {
            CANInstance *instance = s_can_instance[i];
            if (instance->map.handle == hfdcan && instance->rx_id == rx_header.Identifier)
            {
                uint8_t rx_len = CANFdcanDlcToLength(rx_header.DataLength);
                if (rx_len > sizeof(instance->rx_buff))
                {
                    rx_len = sizeof(instance->rx_buff);
                }

                instance->rx_len = rx_len;
                memcpy(instance->rx_buff, rx_data, rx_len);

                if (instance->rx_callback != NULL)
                {
                    instance->rx_callback(instance);
                }
                break;
            }
        }
    }
}

#else

/*------------- BxCAN 私有函数 --------------*/

static uint8_t s_can1_filter_idx = 0;
static uint8_t s_can2_filter_idx = 14;

static HAL_StatusTypeDef CANBxcanAddFilter(CANInstance *instance)
{
    CAN_FilterTypeDef filter = {0};
    uint8_t filter_bank = 0;

    if (instance->map.handle->Instance == CAN1)
    {
        BSP_RETURN_IF_TRUE(s_can1_filter_idx >= 14, HAL_ERROR);
        filter_bank = s_can1_filter_idx++;
    }
    else if (instance->map.handle->Instance == CAN2)
    {
        BSP_RETURN_IF_TRUE(s_can2_filter_idx >= 28, HAL_ERROR);
        filter_bank = s_can2_filter_idx++;
    }
    else
    {
        return HAL_ERROR;
    }

    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterFIFOAssignment = (instance->rx_id & 1U) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
    filter.FilterIdLow = (instance->rx_id & 0x7FFU) << 5;
    filter.FilterBank = filter_bank;
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;

    return HAL_CAN_ConfigFilter(instance->map.handle, &filter);
}

static HAL_StatusTypeDef CANBxcanStartIfNeeded(CAN_HandleTypeDef *handle)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i]->map.handle == handle)
        {
            return HAL_OK;
        }
    }

    if (HAL_CAN_Start(handle) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_CAN_ActivateNotification(handle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static void CANDispatchBxcanMessage(CAN_HandleTypeDef *hcan, uint32_t fifo)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) > 0U)
    {
        if (HAL_CAN_GetRxMessage(hcan, fifo, &rx_header, rx_data) != HAL_OK)
        {
            return;
        }

        for (uint8_t i = 0; i < s_idx; i++)
        {
            CANInstance *instance = s_can_instance[i];
            if (instance->map.handle == hcan && instance->rx_id == rx_header.StdId)
            {
                uint8_t rx_len = (rx_header.DLC <= 8U) ? (uint8_t)rx_header.DLC : 8U;
                instance->rx_len = rx_len;
                memcpy(instance->rx_buff, rx_data, rx_len);

                if (instance->rx_callback != NULL)
                {
                    instance->rx_callback(instance);
                }
                break;
            }
        }
    }
}

#endif

/*------------- 外部接口实现 --------------*/

int8_t CANRegister(CANInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_idx >= CAN_INSTANCE_NUM, -1, LOGERROR("[bsp_can] Exceeded max instance count!"));
    BSP_RETURN_IF_TRUE_LOG(instance->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    instance->map = can_map[instance->can_e];
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL, check bsp_cfg mapping!"));

    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i]->map.handle == instance->map.handle && s_can_instance[i]->rx_id == instance->rx_id)
        {
            LOGERROR("[bsp_can] Duplicate rx_id registration on same CAN handle! rx_id=0x%lX", instance->rx_id);
            return -1;
        }
    }

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    instance->tx_header.Identifier = instance->tx_id;
    instance->tx_header.IdType = FDCAN_STANDARD_ID;
    instance->tx_header.TxFrameType = FDCAN_DATA_FRAME;
    instance->tx_header.DataLength = FDCAN_DLC_BYTES_8;
    instance->tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    instance->tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    instance->tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    instance->tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    instance->tx_header.MessageMarker = 0;

    BSP_RETURN_IF_TRUE_LOG(CANFdcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id));

    BSP_RETURN_IF_TRUE_LOG(CANFdcanStartIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN start/notification init failed! can_e=%d", instance->can_e));
#else
    instance->tx_header.StdId = instance->tx_id;
    instance->tx_header.IDE = CAN_ID_STD;
    instance->tx_header.RTR = CAN_RTR_DATA;
    instance->tx_header.DLC = 8;

    BSP_RETURN_IF_TRUE_LOG(CANBxcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] CAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id));

    BSP_RETURN_IF_TRUE_LOG(CANBxcanStartIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] CAN start/notification init failed! can_e=%d", instance->can_e));
#endif

    s_can_instance[s_idx++] = instance;
    LOGINFO("[bsp_can] CAN instance registered, idx=%d", s_idx - 1);
    return 0;
}

void CANSetDLC(CANInstance *instance, uint8_t length)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_can] CANSetDLC: instance is NULL!");
        return;
    }

    if (length == 0U || length > 8U)
    {
        LOGWARNING("[bsp_can] CANSetDLC: invalid length=%d, only 1~8 is allowed", length);
        return;
    }

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    instance->tx_header.DataLength = CANLengthToFdcanDlc(length);
#else
    instance->tx_header.DLC = length;
#endif
}

uint8_t CANTransmit(CANInstance *instance, uint32_t timeout_ms)
{
    if (instance == NULL || instance->map.handle == NULL)
    {
        LOGWARNING("[bsp_can] CANTransmit: invalid instance");
        return 0;
    }

    uint32_t start_tick = HAL_GetTick();

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    while (HAL_FDCAN_GetTxFifoFreeLevel(instance->map.handle) == 0U)
    {
        if ((HAL_GetTick() - start_tick) > timeout_ms)
        {
            LOGWARNING("[bsp_can] FDCAN Tx FIFO timeout");
            return 0;
        }
    }

    if (HAL_FDCAN_AddMessageToTxFifoQ(instance->map.handle, &instance->tx_header, instance->tx_buff) != HAL_OK)
    {
        LOGWARNING("[bsp_can] FDCAN add tx message failed");
        return 0;
    }
#else
    while (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0U)
    {
        if ((HAL_GetTick() - start_tick) > timeout_ms)
        {
            LOGWARNING("[bsp_can] CAN mailbox timeout");
            return 0;
        }
    }

    if (HAL_CAN_AddTxMessage(instance->map.handle, &instance->tx_header, instance->tx_buff, &instance->tx_mailbox) != HAL_OK)
    {
        LOGWARNING("[bsp_can] CAN add tx message failed");
        return 0;
    }
#endif

    return 1;
}

/*------------- HAL回调函数重写 --------------*/

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U)
    {
        CANDispatchFdcanMessage(hfdcan, FDCAN_RX_FIFO0);
    }
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs)
{
    if ((RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE) != 0U)
    {
        CANDispatchFdcanMessage(hfdcan, FDCAN_RX_FIFO1);
    }
}

#else

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanMessage(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanMessage(hcan, CAN_RX_FIFO1);
}

#endif

#endif
#endif
