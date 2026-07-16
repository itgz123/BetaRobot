/**
 * @file bsp_can.c
 * @brief CAN驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件初始化参数配置
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)
#if CAN_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_uart_log.h"
#include "string.h"

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN

/*------------- FDCAN 过滤器索引 --------------*/
#define FDCAN_FILTER_MAX 16 // FDCAN标准过滤器数量限制为128个（0-127）
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
    filter.IdType = FDCAN_STANDARD_ID;
    filter.RxBufferIndex = 0;
    filter.IsCalibrationMsg = 0;

    if (instance->filter_mode == CAN_FILTER_MODE_LIST)
    {
        // FDCAN 列表模式：每个 DUAL 过滤器只能匹配 2 个 ID
        // 需要根据 rx_id_count 分配 ceil(rx_id_count/2) 个过滤器以支持最多 4 个 ID
        uint8_t num_filters = (instance->rx_id_count + 1U) / 2U;

        for (uint8_t f = 0; f < num_filters; f++)
        {
            // 检查过滤器索引是否超出上限
            if (s_fdcan_filter_idx[instance->can_e] >= FDCAN_FILTER_MAX)
            {
                LOGERROR("[bsp_can] FDCAN filter index overflow! max=%d, current=%d", FDCAN_FILTER_MAX, s_fdcan_filter_idx[instance->can_e]);
                return HAL_ERROR;
            }

            uint32_t id0 = instance->rx_id_list[f * 2U];
            uint32_t id1 = instance->rx_id_list[f * 2U + 1U];

            // 如果 id1 无效，使用 id0 作为第二个 ID（匹配同一个 ID）
            if (id1 == CAN_ID_UNUSED || id1 > 0x7FF)
            {
                id1 = id0;
            }

            filter.FilterIndex = s_fdcan_filter_idx[instance->can_e]++;
            filter.FilterConfig = (id0 & 1U) ? FDCAN_FILTER_TO_RXFIFO0 : FDCAN_FILTER_TO_RXFIFO1;
            filter.FilterType = FDCAN_FILTER_DUAL;
            filter.FilterID1 = id0;
            filter.FilterID2 = id1;

            if (HAL_FDCAN_ConfigFilter(instance->map.handle, &filter) != HAL_OK)
            {
                return HAL_ERROR;
            }
        }
    }
    else
    {
        // 检查过滤器索引是否超出上限
        if (s_fdcan_filter_idx[instance->can_e] >= FDCAN_FILTER_MAX)
        {
            LOGERROR("[bsp_can] FDCAN filter index overflow! max=%d, current=%d", FDCAN_FILTER_MAX, s_fdcan_filter_idx[instance->can_e]);
            return HAL_ERROR;
        }

        // FDCAN 掩码模式：使用 MASK 类型，FilterID1 为 ID，FilterID2 为掩码
        filter.FilterIndex = s_fdcan_filter_idx[instance->can_e]++;
        filter.FilterConfig = (instance->rx_id_list[0] & 1U) ? FDCAN_FILTER_TO_RXFIFO0 : FDCAN_FILTER_TO_RXFIFO1;
        filter.FilterType = FDCAN_FILTER_MASK;
        filter.FilterID1 = instance->rx_id_list[0];
        filter.FilterID2 = instance->rx_mask;

        return HAL_FDCAN_ConfigFilter(instance->map.handle, &filter);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef CANFdcanInitIfNeeded(FDCAN_HandleTypeDef *handle)
{
    // 检查该CAN外设是否已初始化（已有实例注册）
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i]->map.handle == handle)
        {
            return HAL_OK;
        }
    }

    // 首次注册该CAN外设实例时，执行全局配置
    // 全局过滤器：拒绝所有未匹配专用过滤器的消息（专用过滤器在CANFdcanAddFilter中配置）
    if (HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    // 配置中断线分配：FIFO0+错误 -> IT0, FIFO1 -> IT1
    // 注意：HAL_FDCAN_ConfigInterruptLines 是覆盖式调用，需将同一中断线的所有中断源合并
    uint32_t line0_ints = FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
    if (HAL_FDCAN_ConfigInterruptLines(handle, line0_ints, FDCAN_INTERRUPT_LINE0) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_FDCAN_ConfigInterruptLines(handle, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, FDCAN_INTERRUPT_LINE1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_FDCAN_Start(handle) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_FDCAN_ActivateNotification(handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO1_NEW_MESSAGE | FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE, 0) != HAL_OK)
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
            if (instance->map.handle != hfdcan)
            {
                continue;
            }

            // 跳过未配置接收的实例
            if (instance->filter_mode == CAN_FILTER_MODE_LIST && instance->rx_id_count == 0)
            {
                continue;
            }
            if (instance->filter_mode == CAN_FILTER_MODE_MASK && instance->rx_id_list[0] == CAN_ID_UNUSED)
            {
                continue;
            }

            uint8_t matched = 0;

            if (instance->filter_mode == CAN_FILTER_MODE_LIST)
            {
                // 列表模式：遍历 rx_id_list 匹配
                for (uint8_t j = 0; j < instance->rx_id_count; j++)
                {
                    if (instance->rx_id_list[j] == rx_header.Identifier)
                    {
                        instance->rx_id_matched = rx_header.Identifier;
                        matched = 1;
                        break;
                    }
                }
            }
            else
            {
                // 掩码模式：使用掩码匹配
                if ((rx_header.Identifier & instance->rx_mask) == (instance->rx_id_list[0] & instance->rx_mask))
                {
                    instance->rx_id_matched = rx_header.Identifier;
                    matched = 1;
                }
            }

            if (matched)
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

    // 16位标准帧ID格式：[15]=RTR, [14]=IDE, [13:3]=STID[10:0], [2:0]=EXID[17:15]
    // 标准帧ID左移3位，IDE=0, RTR=0
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterFIFOAssignment = (instance->rx_id_list[0] & 1U) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
    filter.FilterBank = filter_bank;
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (instance->filter_mode == CAN_FILTER_MODE_LIST)
    {
        // 16位列表模式：每个filter bank可配置4个精确ID
        // HAL库16位过滤器寄存器映射：
        // FR1 = (FilterMaskIdLow << 16) | FilterIdLow → 第1ID在FilterIdLow，第2ID在FilterMaskIdLow
        // FR2 = (FilterMaskIdHigh << 16) | FilterIdHigh → 第3ID在FilterIdHigh，第4ID在FilterMaskIdHigh
        // CAN_ID_UNUSED 槽位配置为 0（不匹配有效ID）
        filter.FilterMode = CAN_FILTERMODE_IDLIST;
        filter.FilterIdLow = (instance->rx_id_list[0] != CAN_ID_UNUSED && instance->rx_id_list[0] <= 0x7FF)
                                 ? (instance->rx_id_list[0] & 0x7FFU) << 3
                                 : 0;
        filter.FilterMaskIdLow = (instance->rx_id_list[1] != CAN_ID_UNUSED && instance->rx_id_list[1] <= 0x7FF)
                                     ? (instance->rx_id_list[1] & 0x7FFU) << 3
                                     : 0;
        filter.FilterIdHigh = (instance->rx_id_list[2] != CAN_ID_UNUSED && instance->rx_id_list[2] <= 0x7FF)
                                  ? (instance->rx_id_list[2] & 0x7FFU) << 3
                                  : 0;
        filter.FilterMaskIdHigh = (instance->rx_id_list[3] != CAN_ID_UNUSED && instance->rx_id_list[3] <= 0x7FF)
                                      ? (instance->rx_id_list[3] & 0x7FFU) << 3
                                      : 0;
    }
    else
    {
        // 16位掩码模式：支持范围匹配
        // HAL库16位掩码过滤器寄存器映射：
        // FR1 = (FilterMaskIdLow << 16) | FilterIdLow → FilterIdLow为ID，FilterMaskIdLow为掩码
        filter.FilterMode = CAN_FILTERMODE_IDMASK;
        filter.FilterIdLow = (instance->rx_id_list[0] & 0x7FFU) << 3;
        filter.FilterMaskIdLow = (instance->rx_mask & 0x7FFU) << 3;
        filter.FilterIdHigh = 0;
        filter.FilterMaskIdHigh = 0;
    }

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

    if (HAL_CAN_ActivateNotification(handle, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE) != HAL_OK)
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
            if (instance->map.handle != hcan)
            {
                continue;
            }

            // 跳过未配置接收的实例
            if (instance->filter_mode == CAN_FILTER_MODE_LIST && instance->rx_id_count == 0)
            {
                continue;
            }
            if (instance->filter_mode == CAN_FILTER_MODE_MASK && instance->rx_id_list[0] == CAN_ID_UNUSED)
            {
                continue;
            }

            uint8_t matched = 0;

            if (instance->filter_mode == CAN_FILTER_MODE_LIST)
            {
                // 列表模式：遍历 rx_id_list 匹配
                for (uint8_t j = 0; j < instance->rx_id_count; j++)
                {
                    if (instance->rx_id_list[j] == rx_header.StdId)
                    {
                        instance->rx_id_matched = rx_header.StdId;
                        matched = 1;
                        break;
                    }
                }
            }
            else
            {
                // 掩码模式：使用掩码匹配
                if ((rx_header.StdId & instance->rx_mask) == (instance->rx_id_list[0] & instance->rx_mask))
                {
                    instance->rx_id_matched = rx_header.StdId;
                    matched = 1;
                }
            }

            if (matched)
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

/**
 * @brief 配置CAN实例（可重复调用，不修改 static 管理数组）
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    // 将配置拷贝到实例
    instance->can_e = config->can_e;
    instance->tx_id = config->tx_id;
    instance->filter_mode = config->filter_mode;
    memcpy(instance->rx_id_list, config->rx_id_list, sizeof(config->rx_id_list));
    instance->rx_mask = config->rx_mask;
    instance->rx_callback = config->rx_callback;

    // 检查tx_id范围（-1 表示不发送）
    if (instance->tx_id != CAN_ID_UNUSED && instance->tx_id > 0x7FF)
    {
        LOGERROR("[bsp_can] Invalid tx_id=0x%lX, must be <= 0x7FF for standard frames", instance->tx_id);
        return -1;
    }

    // 检查过滤器模式参数，同时计算有效的 rx_id_count
    instance->rx_id_count = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (instance->rx_id_list[i] != CAN_ID_UNUSED && instance->rx_id_list[i] <= 0x7FF)
        {
            instance->rx_id_count++;
        }
    }
    // rx_id_count == 0 允许（仅发送不接收）

    if (instance->filter_mode != CAN_FILTER_MODE_LIST && instance->filter_mode != CAN_FILTER_MODE_MASK)
    {
        LOGERROR("[bsp_can] Invalid filter_mode=%d", instance->filter_mode);
        return -1;
    }

    // 检查 rx_id_list 中的 ID 范围（跳过 CAN_ID_UNUSED）
    for (uint8_t i = 0; i < 4; i++)
    {
        if (instance->rx_id_list[i] != CAN_ID_UNUSED && instance->rx_id_list[i] > 0x7FF)
        {
            LOGERROR("[bsp_can] Invalid rx_id_list[%d]=0x%lX, must be <= 0x7FF for standard frames", i, instance->rx_id_list[i]);
            return -1;
        }
    }

    // 检查掩码范围
    if (instance->rx_mask > 0x7FF)
    {
        LOGERROR("[bsp_can] Invalid rx_mask=0x%lX, must be <= 0x7FF for standard frames", instance->rx_mask);
        return -1;
    }

    instance->map = can_map[instance->can_e];
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL, check bsp_cfg mapping!"));

    // 判断是否需要配置接收过滤器
    uint8_t need_rx_filter = 1;
    if (instance->filter_mode == CAN_FILTER_MODE_LIST)
    {
        if (instance->rx_id_count == 0)
        {
            need_rx_filter = 0;
        }
    }
    else
    {
        if (instance->rx_id_list[0] == CAN_ID_UNUSED)
        {
            need_rx_filter = 0;
        }
    }

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    // 先清零整个 tx_header 结构体，确保所有字段都有确定值
    memset(&instance->tx_header, 0, sizeof(instance->tx_header));

    if (instance->tx_id != CAN_ID_UNUSED)
    {
        instance->tx_header.Identifier = instance->tx_id;
        instance->tx_header.IdType = FDCAN_STANDARD_ID;
        instance->tx_header.TxFrameType = FDCAN_DATA_FRAME;
        instance->tx_header.DataLength = FDCAN_DLC_BYTES_8;
        instance->tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        instance->tx_header.BitRateSwitch = FDCAN_BRS_OFF;
        instance->tx_header.FDFormat = FDCAN_CLASSIC_CAN;
        instance->tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        instance->tx_header.MessageMarker = 0;
    }

    if (need_rx_filter)
    {
        BSP_RETURN_IF_TRUE_LOG(CANFdcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id_list[0]));
    }

    BSP_RETURN_IF_TRUE_LOG(CANFdcanInitIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN init failed! can_e=%d", instance->can_e));
#else
    // 先清零整个 tx_header 结构体，确保所有字段都有确定值
    memset(&instance->tx_header, 0, sizeof(instance->tx_header));

    if (instance->tx_id != CAN_ID_UNUSED)
    {
        instance->tx_header.StdId = instance->tx_id;
        instance->tx_header.IDE = CAN_ID_STD;
        instance->tx_header.RTR = CAN_RTR_DATA;
        instance->tx_header.DLC = 8;
        // ExtId 和 TransmitGlobalTime 已被 memset 清零
    }

    if (need_rx_filter)
    {
        BSP_RETURN_IF_TRUE_LOG(CANBxcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] CAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id_list[0]));
    }

    BSP_RETURN_IF_TRUE_LOG(CANBxcanStartIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] CAN start/notification init failed! can_e=%d", instance->can_e));
#endif

    return 0;
}

/**
 * @brief 注册CAN实例（仅调用一次，修改 static 管理数组）
 */
int8_t CANRegister(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_idx >= CAN_INSTANCE_NUM, -1, LOGERROR("[bsp_can] Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i] == instance)
        {
            LOGERROR("[bsp_can] Instance already registered!");
            return -1;
        }
    }

    // 调用 Config 完成硬件配置
    if (CANConfig(instance, config) != 0)
    {
        return -1;
    }

    // 检查 tx_id 重复（同一CAN句柄上重复的发送ID是允许的，如DJI电机ID1-4共用tx_id=0x200）
    // 但重复意味着多个实例使用同一个tx_id发送，需确认是设计意图
    if (instance->tx_id != CAN_ID_UNUSED)
    {
        for (uint8_t i = 0; i < s_idx; i++)
        {
            CANInstance *existing = s_can_instance[i];
            if (existing->map.handle == instance->map.handle)
            {
                if (existing->tx_id != CAN_ID_UNUSED && existing->tx_id == instance->tx_id)
                {
                    LOGWARNING("[bsp_can] Duplicate tx_id=0x%lX on same CAN handle!", instance->tx_id);
                    break;
                }
            }
        }
    }

    // 检查 rx_id 冲突（同一CAN句柄上不能有重复的接收ID，跳过 -1）
    for (uint8_t i = 0; i < s_idx; i++)
    {
        CANInstance *existing = s_can_instance[i];
        if (existing->map.handle == instance->map.handle)
        {
            for (uint8_t j = 0; j < 4; j++)
            {
                uint32_t new_id = instance->rx_id_list[j];
                if (new_id == CAN_ID_UNUSED)
                {
                    continue;
                }

                for (uint8_t k = 0; k < 4; k++)
                {
                    uint32_t existing_id = existing->rx_id_list[k];
                    if (existing_id == CAN_ID_UNUSED)
                    {
                        continue;
                    }

                    if (new_id == existing_id)
                    {
                        LOGERROR("[bsp_can] Duplicate rx_id=0x%lX on same CAN handle!", new_id);
                        return -1;
                    }
                }
            }
        }
    }

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

    if (instance->tx_id == CAN_ID_UNUSED)
    {
        LOGWARNING("[bsp_can] CANTransmit: tx_id is CAN_ID_UNUSED (-1), not configured for transmit");
        return 0;
    }

    uint64_t start_time = DWT_GetTimeUs();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    while (HAL_FDCAN_GetTxFifoFreeLevel(instance->map.handle) == 0U)
    {
        if ((DWT_GetTimeUs() - start_time) > timeout_us)
        {
            LOGWARNING("[bsp_can] FDCAN Tx FIFO timeout (can_e=%d, tx_id=0x%lX)", instance->can_e, instance->tx_id);
            return 0;
        }
    }

    if (HAL_FDCAN_AddMessageToTxFifoQ(instance->map.handle, &instance->tx_header, instance->tx_buff) != HAL_OK)
    {
        LOGWARNING("[bsp_can] FDCAN add tx message failed (can_e=%d, tx_id=0x%lX)", instance->can_e, instance->tx_id);
        return 0;
    }
#else
    while (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0U)
    {
        if ((DWT_GetTimeUs() - start_time) > timeout_us)
        {
            LOGWARNING("[bsp_can] CAN mailbox timeout (can_e=%d, tx_id=0x%lX)", instance->can_e, instance->tx_id);
            return 0;
        }
    }

    if (HAL_CAN_AddTxMessage(instance->map.handle, &instance->tx_header, instance->tx_buff, &instance->tx_mailbox) != HAL_OK)
    {
        LOGWARNING("[bsp_can] CAN add tx message failed (can_e=%d, tx_id=0x%lX)", instance->can_e, instance->tx_id);
        return 0;
    }
#endif

    return 1;
}

/*------------- HAL回调函数重写 --------------*/
// 有关错误回调：
// can总线发生错误，can外设的硬件寄存器和hal库的软件变量都会有记录。
// 所以需要清除硬件和软件错误。
// BxCAN (F4)：
// - AutoBusOff = ENABLE：硬件自动清除 Bus-off 状态
// - 回调中调用 HAL_CAN_ResetError：清除软件错误标志
// - 回调中记录日志
// FDCAN (H7)：
// - 无 AutoBusOff 配置，但硬件会自动清除错误
// - HAL_FDCAN_IRQHandler 自动清除软件错误标志
// - 回调只需记录日志
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

void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    uint32_t error = HAL_FDCAN_GetError(hfdcan);

    if (error != HAL_FDCAN_ERROR_NONE)
    {
        LOGWARNING("[bsp_can] FDCAN Error: 0x%08lX", error);
    }
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
    {
        LOGERROR("[bsp_can] FDCAN Bus-off!");
    }
    else if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
    {
        LOGWARNING("[bsp_can] FDCAN Error Passive!");
    }
    else if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
    {
        LOGWARNING("[bsp_can] FDCAN Error Warning!");
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

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t error = HAL_CAN_GetError(hcan);

    if (error != HAL_CAN_ERROR_NONE)
    {
        uint32_t esr = hcan->Instance->ESR;
        uint8_t tec = (esr >> 16) & 0xFF;
        uint8_t rec = (esr >> 24) & 0x7F;

        if (error & HAL_CAN_ERROR_BOF)
        {
            LOGERROR("[bsp_can] CAN Bus-off! TEC=%d, REC=%d", tec, rec);
        }
        else if (error & HAL_CAN_ERROR_EPV)
        {
            LOGWARNING("[bsp_can] CAN Error Passive! TEC=%d, REC=%d", tec, rec);
        }
        else if (error & HAL_CAN_ERROR_EWG)
        {
            LOGWARNING("[bsp_can] CAN Error Warning! TEC=%d, REC=%d", tec, rec);
        }
        else
        {
            LOGWARNING("[bsp_can] CAN Error: 0x%08lX, TEC=%d, REC=%d", error, tec, rec);
        }

        (void)tec;
        (void)rec;

        // 清除软件错误标志
        HAL_CAN_ResetError(hcan);
    }
}

#endif /* BSP_CAN_IP */

#endif /* CAN_INSTANCE_NUM > 0 */
#endif /* HAL_CAN/FDCAN_MODULE_ENABLED */

#endif /* BSP_CAN_USED */
