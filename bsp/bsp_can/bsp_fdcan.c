/**
 * @file bsp_fdcan.c
 * @brief FDCAN 驱动实现（自 bsp_can.c 拆分，FDCAN 平台专用）
 *
 * @note 仅当 BSP_CAN_IP == BSP_CAN_IP_FDCAN 时参与编译，BXCAN 平台编译 bsp_bxcan.c。
 *       实例管理与公共接口（CANConfig/CANRegister/CANTransmit）在本文件实现，
 *       代码逻辑与拆分前 bsp_can.c 完全一致。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if defined(HAL_FDCAN_MODULE_ENABLED)
#if CAN_INSTANCE_NUM > 0
#if BSP_CAN_IP == BSP_CAN_IP_FDCAN

#include "bsp_dwt.h"
#include "bsp_uart_log.h"
#include "hal_can.h"
#include "string.h"

/*------------- 私有变量 --------------*/
static uint8_t s_can_idx = 0;
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
static uint8_t s_can_started[CAN_NUM_MAX] = {0}; // 标记每个CAN外设是否已启动

/*------------- 公共工具函数 --------------*/

/* 发送长度合法性：0=默认8，合法尺寸 {1..8,12,16,20,24,32,48,64} */
static uint8_t CANIsValidTxLen(uint8_t len)
{
    if (len == 0U)
    {
        return 1;
    }
    switch (len)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 12:
    case 16:
    case 20:
    case 24:
    case 32:
    case 48:
    case 64:
        return 1;
    default:
        return 0;
    }
}

/*------------- FDCAN 过滤器索引 --------------*/
/* 标准/扩展帧各有一套独立过滤器列表（基址分别由 SIDFC/XIDFC 管理） */
static uint8_t s_fdcan_std_filter_idx[CAN_NUM_MAX] = {0}; // 各CAN标准帧过滤器已用数量
static uint8_t s_fdcan_ext_filter_idx[CAN_NUM_MAX] = {0}; // 各CAN扩展帧过滤器已用数量

/*------------- FDCAN 私有函数 --------------*/

static uint8_t CANFdcanDlcToLength(uint32_t dlc)
{
    switch (dlc)
    {
    case FDCAN_DLC_BYTES_0:
        return 0;
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
        return 8;
    case FDCAN_DLC_BYTES_12:
        return 12;
    case FDCAN_DLC_BYTES_16:
        return 16;
    case FDCAN_DLC_BYTES_20:
        return 20;
    case FDCAN_DLC_BYTES_24:
        return 24;
    case FDCAN_DLC_BYTES_32:
        return 32;
    case FDCAN_DLC_BYTES_48:
        return 48;
    case FDCAN_DLC_BYTES_64:
        return 64;
    default:
        return 8;
    }
}

static uint32_t CANLengthToFdcanDlc(uint8_t len)
{
    if (len <= 8U)
    {
        return (uint32_t)(FDCAN_DLC_BYTES_0 + len);
    }
    if (len <= 12U)
    {
        return FDCAN_DLC_BYTES_12;
    }
    if (len <= 16U)
    {
        return FDCAN_DLC_BYTES_16;
    }
    if (len <= 20U)
    {
        return FDCAN_DLC_BYTES_20;
    }
    if (len <= 24U)
    {
        return FDCAN_DLC_BYTES_24;
    }
    if (len <= 32U)
    {
        return FDCAN_DLC_BYTES_32;
    }
    if (len <= 48U)
    {
        return FDCAN_DLC_BYTES_48;
    }
    return FDCAN_DLC_BYTES_64;
}

/* FDCAN 元素尺寸枚举 → 实际字节数 */
static uint8_t CANFdcanElmtSizeToBytes(uint32_t elmt_size)
{
    switch (elmt_size)
    {
    case FDCAN_DATA_BYTES_8:
        return 8;
    case FDCAN_DATA_BYTES_12:
        return 12;
    case FDCAN_DATA_BYTES_16:
        return 16;
    case FDCAN_DATA_BYTES_20:
        return 20;
    case FDCAN_DATA_BYTES_24:
        return 24;
    case FDCAN_DATA_BYTES_32:
        return 32;
    case FDCAN_DATA_BYTES_48:
        return 48;
    case FDCAN_DATA_BYTES_64:
        return 64;
    default:
        return 8;
    }
}

static HAL_StatusTypeDef CANFdcanAddFilter(CANInstance *instance)
{
    if (instance->can_e >= CAN_NUM_MAX)
    {
        return HAL_ERROR;
    }

    FDCAN_HandleTypeDef *handle = instance->map.handle;
    FDCAN_FilterTypeDef filter = {0};
    uint8_t *filter_idx;
    uint8_t filter_max;

    filter.RxBufferIndex = 0;
    filter.IsCalibrationMsg = 0;

    // 标准/扩展帧是两套独立过滤器列表（基址由 SIDFC/XIDFC 分别管理）
    if (instance->rx_id_type == CAN_FRAME_ID_EXT)
    {
        filter.IdType = FDCAN_EXTENDED_ID;
        filter_idx = &s_fdcan_ext_filter_idx[instance->can_e];
        filter_max = (uint8_t)handle->Init.ExtFiltersNbr;
    }
    else
    {
        filter.IdType = FDCAN_STANDARD_ID;
        filter_idx = &s_fdcan_std_filter_idx[instance->can_e];
        filter_max = (uint8_t)handle->Init.StdFiltersNbr;
    }

    if (instance->filter_mode == CAN_FILTER_MODE_LIST)
    {
        // FDCAN 列表模式：每个 DUAL 过滤器只能匹配 2 个 ID
        // 需要根据 rx_id_count 分配 ceil(rx_id_count/2) 个过滤器以支持最多 4 个 ID
        uint8_t num_filters = (instance->rx_id_count + 1U) / 2U;

        for (uint8_t f = 0; f < num_filters; f++)
        {
            // 检查过滤器索引是否超出上限
            if (*filter_idx >= filter_max)
            {
                LOGERROR("[bsp_can] FDCAN filter index overflow! max=%d, current=%d", filter_max, *filter_idx);
                return HAL_ERROR;
            }

            uint32_t id0 = instance->rx_id_list[f * 2U];
            uint32_t id1 = instance->rx_id_list[f * 2U + 1U];

            // 如果 id1 无效（CAN_ID_UNUSED），使用 id0 作为第二个 ID（匹配同一个 ID）
            if (id1 == CAN_ID_UNUSED)
            {
                id1 = id0;
            }

            filter.FilterIndex = (*filter_idx)++;
            filter.FilterConfig = (id0 & 1U) ? FDCAN_FILTER_TO_RXFIFO0 : FDCAN_FILTER_TO_RXFIFO1;
            filter.FilterType = FDCAN_FILTER_DUAL;
            filter.FilterID1 = id0;
            filter.FilterID2 = id1;

            if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK)
            {
                return HAL_ERROR;
            }
        }
    }
    else
    {
        // 检查过滤器索引是否超出上限
        if (*filter_idx >= filter_max)
        {
            LOGERROR("[bsp_can] FDCAN filter index overflow! max=%d, current=%d", filter_max, *filter_idx);
            return HAL_ERROR;
        }

        // FDCAN 掩码模式：使用 MASK 类型，FilterID1 为 ID，FilterID2 为掩码
        filter.FilterIndex = (*filter_idx)++;
        filter.FilterConfig = (instance->rx_id_list[0] & 1U) ? FDCAN_FILTER_TO_RXFIFO0 : FDCAN_FILTER_TO_RXFIFO1;
        filter.FilterType = FDCAN_FILTER_MASK;
        filter.FilterID1 = instance->rx_id_list[0];
        filter.FilterID2 = instance->rx_mask;

        return HAL_FDCAN_ConfigFilter(handle, &filter);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef CANFdcanInitIfNeeded(FDCAN_HandleTypeDef *handle)
{
    // 查找 can_e 索引，检查该CAN外设是否已启动
    uint8_t can_idx = 0;
    uint8_t found = 0;
    for (can_idx = 0; can_idx < CAN_NUM_MAX; can_idx++)
    {
        if (can_map[can_idx].handle == handle)
        {
            found = 1;
            break;
        }
    }
    if (!found)
    {
        return HAL_ERROR;
    }

    if (s_can_started[can_idx])
        return HAL_OK;

    // 首次启动该CAN外设时，先执行 hal_can 重配置（覆盖 CubeMX，若 bsp_map 配置存在）。
    // 注意：HAL_FDCAN_Init 会清空分配区 Message RAM 并重写 SIDFC/XIDFC 基址，
    // 故重配置必须先于过滤器配置（CANFdcanAddFilter 在调用本函数之后执行）。
    if (can_cfg_map[can_idx] != NULL)
    {
        if (HalCanReconfigureFdcan(handle, can_cfg_map[can_idx]) != HAL_OK)
        {
            LOGERROR("[bsp_can] FDCAN hal_can reconfigure failed! can_e=%d", can_idx);
            return HAL_ERROR;
        }
    }

    // 全局过滤器：拒绝所有未匹配专用过滤器的消息（专用过滤器在CANFdcanAddFilter中配置）
    if (HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    // 配置中断线分配：FIFO0+TX完成+错误 -> IT0, FIFO1 -> IT1
    // 注意：HAL_FDCAN_ConfigInterruptLines 是覆盖式调用，需将同一中断线的所有中断源合并
    uint32_t line0_ints = FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_TX_FIFO_EMPTY | FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
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

    // 全部启动步骤（reconfig/全局过滤/中断线/Start/通知）成功后才标记已启动；
    // 任一步失败都不置位，允许下次 CANConfig 重试完整初始化。
    s_can_started[can_idx] = 1;

    return HAL_OK;
}

static void CANDispatchFdcanMessage(FDCAN_HandleTypeDef *hfdcan, uint32_t fifo)
{
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[64];

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, fifo) > 0U)
    {
        if (HAL_FDCAN_GetRxMessage(hfdcan, fifo, &rx_header, rx_data) != HAL_OK)
        {
            return;
        }

        for (uint8_t i = 0; i < s_can_idx; i++)
        {
            CANInstance *instance = s_can_instance[i];
            if (instance->map.handle != hfdcan)
            {
                continue;
            }

            // 帧类型匹配：标准/扩展必须一致（防标准实例命中等值扩展帧）
            if ((rx_header.IdType == FDCAN_EXTENDED_ID && instance->rx_id_type != CAN_FRAME_ID_EXT) ||
                (rx_header.IdType == FDCAN_STANDARD_ID && instance->rx_id_type != CAN_FRAME_ID_STD))
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

/*------------- 外部接口实现 --------------*/

/**
 * @brief 配置CAN实例（填充硬件映射 + 运行时参数 + 滤波器/启动，可重复调用）
 * @note 要求先调用 CANRegister 注册实例
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    // 填充枚举和硬件映射
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];

    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL, check bsp_cfg mapping!"));

    // 将配置拷贝到实例
    instance->tx_id = config->tx_id;
    instance->tx_id_type = config->tx_id_type;
    instance->tx_frame_format = config->tx_frame_format;
    instance->filter_mode = config->filter_mode;
    memcpy(instance->rx_id_list, config->rx_id_list, sizeof(config->rx_id_list));
    instance->rx_mask = config->rx_mask;
    instance->rx_id_type = config->rx_id_type;
    instance->rx_callback = config->rx_callback;
    instance->tx_complete_callback = config->tx_complete_callback;

    // 校验帧类型/格式枚举合法性
    if (instance->tx_id_type != CAN_FRAME_ID_STD && instance->tx_id_type != CAN_FRAME_ID_EXT)
    {
        LOGERROR("[bsp_can] Invalid tx_id_type=%d", instance->tx_id_type);
        return -1;
    }
    if (instance->rx_id_type != CAN_FRAME_ID_STD && instance->rx_id_type != CAN_FRAME_ID_EXT)
    {
        LOGERROR("[bsp_can] Invalid rx_id_type=%d", instance->rx_id_type);
        return -1;
    }
    if (instance->tx_frame_format != CAN_FRAME_FORMAT_CLASSIC &&
        instance->tx_frame_format != CAN_FRAME_FORMAT_FD &&
        instance->tx_frame_format != CAN_FRAME_FORMAT_FD_BRS)
    {
        LOGERROR("[bsp_can] Invalid tx_frame_format=%d", instance->tx_frame_format);
        return -1;
    }

    // 检查发送长度：合法尺寸 {1..8,12,16,20,24,32,48,64}，0 表示默认 8
    if (!CANIsValidTxLen(config->tx_len))
    {
        LOGERROR("[bsp_can] Invalid tx_len=%d, legal: 1~8,12,16,20,24,32,48,64 (0=default 8)", config->tx_len);
        return -1;
    }
    uint8_t tx_len = (config->tx_len == 0U) ? 8U : config->tx_len;

    // FD 组合校验：长度>8 必须配 FD 帧；BxCAN 不支持 FD 帧
    if (tx_len > 8U && instance->tx_frame_format == CAN_FRAME_FORMAT_CLASSIC)
    {
        LOGERROR("[bsp_can] tx_len=%d requires FD frame format, but tx_frame_format=CLASSIC", tx_len);
        return -1;
    }
    // 各 ID 范围上限取决于帧类型
    uint32_t tx_id_max = (instance->tx_id_type == CAN_FRAME_ID_EXT) ? 0x1FFFFFFFU : 0x7FFU;
    uint32_t rx_id_max = (instance->rx_id_type == CAN_FRAME_ID_EXT) ? 0x1FFFFFFFU : 0x7FFU;

    // 检查tx_id范围（-1 表示不发送）
    if (instance->tx_id != CAN_ID_UNUSED && instance->tx_id > tx_id_max)
    {
        LOGERROR("[bsp_can] Invalid tx_id=0x%lX, must be <= 0x%lX for %s frames", instance->tx_id, tx_id_max, instance->tx_id_type == CAN_FRAME_ID_EXT ? "extended" : "standard");
        return -1;
    }

    // 检查过滤器模式参数，同时计算有效的 rx_id_count
    instance->rx_id_count = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (instance->rx_id_list[i] != CAN_ID_UNUSED && instance->rx_id_list[i] <= rx_id_max)
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
        if (instance->rx_id_list[i] != CAN_ID_UNUSED && instance->rx_id_list[i] > rx_id_max)
        {
            LOGERROR("[bsp_can] Invalid rx_id_list[%d]=0x%lX, must be <= 0x%lX for %s frames", i, instance->rx_id_list[i], rx_id_max, instance->rx_id_type == CAN_FRAME_ID_EXT ? "extended" : "standard");
            return -1;
        }
    }

    // 检查掩码范围
    if (instance->rx_mask > rx_id_max)
    {
        LOGERROR("[bsp_can] Invalid rx_mask=0x%lX, must be <= 0x%lX for %s frames", instance->rx_mask, rx_id_max, instance->rx_id_type == CAN_FRAME_ID_EXT ? "extended" : "standard");
        return -1;
    }

    // FDCAN：检查发送长度不超过外设 Tx 元素尺寸。
    // 优先用 can_cfg_map 的重配置值（CANFdcanInitIfNeeded 重配置后会覆盖 handle->Init.TxElmtSize），
    // 配置不存在时退回当前 handle->Init 值（CubeMX）。若直接用重配置前的 handle->Init 值，
    // 当用户仅加大 can_cfg_map 元素尺寸而 CubeMX 未同步时，会误拒合法的 FD 配置。
    // 警告：若 tx_len 超过最终 TxElmtSize，HAL_FDCAN_AddMessageToTxFifoQ 内部 FDCAN_CopyMessageToRAM
    //       会按 tx_len 越界写 Message RAM（内存破坏），因此这里直接拒绝配置而非仅警告。
    if (tx_len > 8U)
    {
        uint32_t elmt_size = (can_cfg_map[instance->can_e] != NULL)
                                 ? can_cfg_map[instance->can_e]->tx_elmt_size
                                 : instance->map.handle->Init.TxElmtSize;
        uint8_t elmt_bytes = CANFdcanElmtSizeToBytes(elmt_size);
        if (tx_len > elmt_bytes)
        {
            LOGERROR("[bsp_can] tx_len=%d exceeds FDCAN TxElmtSize=%d bytes (can_e=%d)", tx_len, elmt_bytes, instance->can_e);
            return -1;
        }
    }

    // 检查 tx_id 重复（同一CAN句柄上重复的发送ID可能是有意为之，仅警告）
    if (instance->tx_id != CAN_ID_UNUSED)
    {
        for (uint8_t i = 0; i < s_can_idx; i++)
        {
            CANInstance *existing = s_can_instance[i];
            if (existing == instance)
            {
                continue;
            }
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

    // 检查 rx_id 冲突（同一CAN句柄上不能有重复的接收ID，跳过自身）
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *existing = s_can_instance[i];
        if (existing == instance)
        {
            continue;
        }
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

    // 先清零整个 tx_header 结构体，确保所有字段都有确定值
    memset(&instance->tx_header, 0, sizeof(instance->tx_header));

    if (instance->tx_id != CAN_ID_UNUSED)
    {
        instance->tx_header.Identifier = instance->tx_id;
        instance->tx_header.IdType = (instance->tx_id_type == CAN_FRAME_ID_EXT) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        instance->tx_header.TxFrameType = FDCAN_DATA_FRAME;
        instance->tx_header.DataLength = CANLengthToFdcanDlc(tx_len);
        instance->tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        instance->tx_header.BitRateSwitch = (instance->tx_frame_format == CAN_FRAME_FORMAT_FD_BRS) ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
        instance->tx_header.FDFormat = (instance->tx_frame_format == CAN_FRAME_FORMAT_CLASSIC) ? FDCAN_CLASSIC_CAN : FDCAN_FD_CAN;
        instance->tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        instance->tx_header.MessageMarker = 0;
    }

    // 关键重排序：先启动（含 hal_can 重配置），后配置过滤器。
    // HAL_FDCAN_Init 会清空分配区 Message RAM 并重写 SIDFC/XIDFC 基址，
    // 因此必须在 CANFdcanAddFilter 之前完成。
    BSP_RETURN_IF_TRUE_LOG(CANFdcanInitIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN init failed! can_e=%d", instance->can_e));

    if (need_rx_filter)
    {
        BSP_RETURN_IF_TRUE_LOG(CANFdcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] FDCAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id_list[0]));
    }

    return 0;
}

/**
 * @brief 注册CAN实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 CANConfig 负责）
 */
int8_t CANRegister(CANInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_can_idx >= CAN_INSTANCE_NUM, -1, LOGERROR("[bsp_can] Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        if (s_can_instance[i] == instance)
        {
            LOGERROR("[bsp_can] Instance already registered!");
            return -1;
        }
    }

    s_can_instance[s_can_idx++] = instance;
    LOGINFO("[bsp_can] CAN instance registered, idx=%d", s_can_idx - 1);
    return 0;
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

    // 需要发送完成回调时，激活 Tx FIFO Empty 通知（每次发送都重新激活：
    // HAL 在 FIFO 变空触发回调后会自动清除该中断使能位）。
    // 注意：多实例共用同一 FDCAN 时，FIFO 全空触发一次，分发遍历所有实例，
    //       因此同一时刻最多一帧在途的异步分包发送者都能正确收到回调。
    if (instance->tx_complete_callback != NULL)
    {
        (void)HAL_FDCAN_ActivateNotification(instance->map.handle, FDCAN_IT_TX_FIFO_EMPTY, 0);
    }

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

/* FDCAN Tx FIFO 全空中断：FIFO 中所有帧已发出。分发到共享该外设的所有实例。
 * @note 本层只负责分发，不感知"发的是哪一帧"——media 异步分包发送者每次只提交
 *       一包、等此回调后再提交下一包，故 FIFO 全空即意味着自己上一包已发送完成。 */
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
{
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *instance = s_can_instance[i];
        if (instance->map.handle == hfdcan && instance->tx_complete_callback != NULL)
            instance->tx_complete_callback(instance);
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
    // 三个错误状态标志可同时置位（如 Bus-off 同时伴随 Warning/Passive），须用独立 if 分别记录
    if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
    {
        LOGERROR("[bsp_can] FDCAN Bus-off!");
    }
    if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
    {
        LOGWARNING("[bsp_can] FDCAN Error Passive!");
    }
    if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
    {
        LOGWARNING("[bsp_can] FDCAN Error Warning!");
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_FDCAN */
#endif /* CAN_INSTANCE_NUM > 0 */
#endif /* HAL_FDCAN_MODULE_ENABLED */

#endif /* BSP_CAN_USED */
