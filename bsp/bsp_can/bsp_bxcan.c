/**
 * @file bsp_bxcan.c
 * @brief BxCAN 驱动实现（自 bsp_can.c 拆分，BXCAN 平台专用）
 *
 * @note 仅当 BSP_CAN_IP == BSP_CAN_IP_BXCAN 时参与编译，FDCAN 平台编译 bsp_fdcan.c。
 *       实例管理与公共接口（CANConfig/CANRegister/CANTransmit）在本文件实现，
 *       代码逻辑与拆分前 bsp_can.c 完全一致。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if defined(HAL_CAN_MODULE_ENABLED)
#if CAN_INSTANCE_NUM > 0
#if BSP_CAN_IP == BSP_CAN_IP_BXCAN

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

static uint8_t s_can1_filter_idx = 0;
static uint8_t s_can2_filter_idx = 14;

/* BxCAN 32 位过滤器值构造（扩展帧）：
 * 位布局：bit31=RTR(0), bit30=IDE(1), bits[29:21]=STID=EXTID>>18, bits[20:3]=EXID=EXTID&0x3FFFF */
static uint32_t CANBxcanExtIdValue(uint32_t ext_id)
{
    return (1U << 30) | ((ext_id >> 18U) << 21U) | ((ext_id & 0x3FFFFU) << 3U);
}

/* BxCAN 32 位掩码值构造：位域布局同上，IDE 位(bit30)强制参与匹配 */
static uint32_t CANBxcanExtMaskValue(uint32_t ext_mask)
{
    return (1U << 30) | ((ext_mask >> 18U) << 21U) | ((ext_mask & 0x3FFFFU) << 3U);
}

static HAL_StatusTypeDef CANBxcanAllocFilterBank(CANInstance *instance, uint8_t *filter_bank)
{
    if (instance->map.handle->Instance == CAN1)
    {
        BSP_RETURN_IF_TRUE(s_can1_filter_idx >= 14, HAL_ERROR);
        *filter_bank = s_can1_filter_idx++;
        return HAL_OK;
    }
    else if (instance->map.handle->Instance == CAN2)
    {
        BSP_RETURN_IF_TRUE(s_can2_filter_idx >= 28, HAL_ERROR);
        *filter_bank = s_can2_filter_idx++;
        return HAL_OK;
    }
    return HAL_ERROR;
}

static HAL_StatusTypeDef CANBxcanAddFilter(CANInstance *instance)
{
    CAN_FilterTypeDef filter = {0};
    uint8_t filter_bank = 0;

    filter.FilterFIFOAssignment = (instance->rx_id_list[0] & 1U) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;
    filter.FilterActivation = CAN_FILTER_ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (instance->rx_frame_type & CAN_FRAME_EXTENDED)
    {
        /* 扩展帧：32 位过滤器（HAL 映射 FR1=(FilterIdHigh<<16)|FilterIdLow，拆高低半字填入） */
        filter.FilterScale = CAN_FILTERSCALE_32BIT;

        if (instance->filter_mode == CAN_FILTER_MODE_LIST)
        {
            // 32位列表模式：每个 filter bank 可配置 2 个精确ID
            uint8_t num_banks = (instance->rx_id_count + 1U) / 2U;
            filter.FilterMode = CAN_FILTERMODE_IDLIST;

            for (uint8_t b = 0; b < num_banks; b++)
            {
                BSP_RETURN_IF_TRUE_LOG(CANBxcanAllocFilterBank(instance, &filter_bank) != HAL_OK, HAL_ERROR, LOGERROR("[bsp_can] CAN filter bank overflow! can_e=%d", instance->can_e));
                filter.FilterBank = filter_bank;

                uint32_t id0 = instance->rx_id_list[b * 2U];
                uint32_t id1 = instance->rx_id_list[b * 2U + 1U];
                if (id1 == CAN_ID_UNUSED)
                {
                    id1 = id0;
                }

                uint32_t val0 = CANBxcanExtIdValue(id0);
                uint32_t val1 = CANBxcanExtIdValue(id1);
                filter.FilterIdLow = (uint16_t)(val0 & 0xFFFFU);
                filter.FilterIdHigh = (uint16_t)((val0 >> 16U) & 0xFFFFU);
                filter.FilterMaskIdLow = (uint16_t)(val1 & 0xFFFFU);
                filter.FilterMaskIdHigh = (uint16_t)((val1 >> 16U) & 0xFFFFU);

                if (HAL_CAN_ConfigFilter(instance->map.handle, &filter) != HAL_OK)
                {
                    return HAL_ERROR;
                }
            }
            return HAL_OK;
        }
        else
        {
            // 32位掩码模式：1 个 bank
            BSP_RETURN_IF_TRUE_LOG(CANBxcanAllocFilterBank(instance, &filter_bank) != HAL_OK, HAL_ERROR, LOGERROR("[bsp_can] CAN filter bank overflow! can_e=%d", instance->can_e));
            filter.FilterMode = CAN_FILTERMODE_IDMASK;
            filter.FilterBank = filter_bank;

            uint32_t val_id = CANBxcanExtIdValue(instance->rx_id_list[0]);
            uint32_t val_mask = CANBxcanExtMaskValue(instance->rx_mask);
            filter.FilterIdLow = (uint16_t)(val_id & 0xFFFFU);
            filter.FilterIdHigh = (uint16_t)((val_id >> 16U) & 0xFFFFU);
            filter.FilterMaskIdLow = (uint16_t)(val_mask & 0xFFFFU);
            filter.FilterMaskIdHigh = (uint16_t)((val_mask >> 16U) & 0xFFFFU);

            return HAL_CAN_ConfigFilter(instance->map.handle, &filter);
        }
    }

    /* 标准帧：16 位过滤器 */
    // 16位标准帧ID格式：[15:5]=STID[10:0], [4]=RTR, [3]=IDE, [2:0]=EXID[17:15]
    // 标准帧ID左移5位，IDE=0, RTR=0
    BSP_RETURN_IF_TRUE_LOG(CANBxcanAllocFilterBank(instance, &filter_bank) != HAL_OK, HAL_ERROR, LOGERROR("[bsp_can] CAN filter bank overflow! can_e=%d", instance->can_e));
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterBank = filter_bank;

    if (instance->filter_mode == CAN_FILTER_MODE_LIST)
    {
        // 16位列表模式：每个filter bank可配置4个精确ID
        // HAL库16位过滤器寄存器映射：
        // FR1 = (FilterMaskIdLow << 16) | FilterIdLow → 第1ID在FilterIdLow，第2ID在FilterMaskIdLow
        // FR2 = (FilterMaskIdHigh << 16) | FilterIdHigh → 第3ID在FilterIdHigh，第4ID在FilterMaskIdHigh
        // 注意：16位槽位无法"禁用"，CAN_ID_UNUSED 槽位填 0 会匹配标准帧 ID=0。
        //       若总线存在 ID=0 的标准帧会误通过硬件过滤，但软件分发按 rx_id_count
        //       二次匹配会将其丢弃，仅多一次中断，不影响正确性。
        filter.FilterMode = CAN_FILTERMODE_IDLIST;
        filter.FilterIdLow = (instance->rx_id_list[0] != CAN_ID_UNUSED && instance->rx_id_list[0] <= 0x7FF)
                                 ? (instance->rx_id_list[0] & 0x7FFU) << 5
                                 : 0;
        filter.FilterMaskIdLow = (instance->rx_id_list[1] != CAN_ID_UNUSED && instance->rx_id_list[1] <= 0x7FF)
                                     ? (instance->rx_id_list[1] & 0x7FFU) << 5
                                     : 0;
        filter.FilterIdHigh = (instance->rx_id_list[2] != CAN_ID_UNUSED && instance->rx_id_list[2] <= 0x7FF)
                                  ? (instance->rx_id_list[2] & 0x7FFU) << 5
                                  : 0;
        filter.FilterMaskIdHigh = (instance->rx_id_list[3] != CAN_ID_UNUSED && instance->rx_id_list[3] <= 0x7FF)
                                      ? (instance->rx_id_list[3] & 0x7FFU) << 5
                                      : 0;
    }
    else
    {
        // 16位掩码模式：支持范围匹配
        // HAL库16位掩码过滤器寄存器映射：
        // FR1 = (FilterMaskIdLow << 16) | FilterIdLow → FilterIdLow为ID，FilterMaskIdLow为掩码
        filter.FilterMode = CAN_FILTERMODE_IDMASK;
        filter.FilterIdLow = (instance->rx_id_list[0] & 0x7FFU) << 5;
        filter.FilterMaskIdLow = (instance->rx_mask & 0x7FFU) << 5;
        filter.FilterIdHigh = 0;
        filter.FilterMaskIdHigh = 0;
    }

    return HAL_CAN_ConfigFilter(instance->map.handle, &filter);
}

static HAL_StatusTypeDef CANBxcanStartIfNeeded(CAN_HandleTypeDef *handle)
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

    // 首次启动该CAN外设时，先执行 hal_can 重配置（覆盖 CubeMX，若 bsp_map 配置存在）
    if (can_cfg_map[can_idx] != NULL)
    {
        if (HalCanReconfigureBxcan(handle, can_cfg_map[can_idx]) != HAL_OK)
        {
            LOGERROR("[bsp_can] CAN hal_can reconfigure failed! can_e=%d", can_idx);
            return HAL_ERROR;
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

    // 全部启动步骤（reconfig/Start/通知）成功后才标记已启动，失败可重试完整初始化
    s_can_started[can_idx] = 1;

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

        // 根据 IDE 提取实际 ID，并确定帧类型
        uint32_t rx_id;
        uint8_t rx_is_ext = (rx_header.IDE == CAN_ID_EXT);
        uint8_t rx_is_remote = (rx_header.RTR == CAN_RTR_REMOTE);
        if (rx_is_ext)
        {
            rx_id = rx_header.ExtId;
        }
        else
        {
            rx_id = rx_header.StdId;
        }

        for (uint8_t i = 0; i < s_can_idx; i++)
        {
            CANInstance *instance = s_can_instance[i];
            if (instance->map.handle != hcan)
            {
                continue;
            }

            // 帧类型匹配：ID 类型（标准/扩展）必须一致
            if (((instance->rx_frame_type & CAN_FRAME_EXTENDED) != 0U) != rx_is_ext)
            {
                continue;
            }
            // 帧类型位匹配：订阅须包含实际收到的帧类型（数据/远程）
            if ((instance->rx_frame_type & (rx_is_remote ? CAN_FRAME_REMOTE : CAN_FRAME_DATA)) == 0U)
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
                    if (instance->rx_id_list[j] == rx_id)
                    {
                        instance->rx_id_matched = rx_id;
                        matched = 1;
                        break;
                    }
                }
            }
            else
            {
                // 掩码模式：使用掩码匹配
                if ((rx_id & instance->rx_mask) == (instance->rx_id_list[0] & instance->rx_mask))
                {
                    instance->rx_id_matched = rx_id;
                    matched = 1;
                }
            }

            if (matched)
            {
                // 记录实际帧类型（位组合），回调中可用 instance->rx_frame_type_matched 区分
                instance->rx_frame_type_matched = (CANFrameType_e)((rx_is_ext ? CAN_FRAME_EXTENDED : CAN_FRAME_STANDARD) |
                                                                   (rx_is_remote ? CAN_FRAME_REMOTE : CAN_FRAME_DATA));
                uint8_t rx_len = (rx_header.DLC <= 8U) ? (uint8_t)rx_header.DLC : 8U;
                instance->rx_len = rx_len;
                // 远程帧无数据载荷：rx_len 为请求的数据长度，rx_buff 不填充
                if (!rx_is_remote)
                {
                    memcpy(instance->rx_buff, rx_data, rx_len);
                }

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
    instance->tx_frame_type = config->tx_frame_type;
    instance->tx_frame_format = config->tx_frame_format;
    instance->filter_mode = config->filter_mode;
    memcpy(instance->rx_id_list, config->rx_id_list, sizeof(config->rx_id_list));
    instance->rx_mask = config->rx_mask;
    instance->rx_frame_type = config->rx_frame_type;
    instance->rx_callback = config->rx_callback;
    instance->tx_complete_callback = config->tx_complete_callback;

    // 归一化帧类型（未设置维度默认标准帧+数据帧）并校验合法性
    instance->tx_frame_type = CANFrameTypeNormalize(instance->tx_frame_type);
    if (!CANFrameTypeIsValid(instance->tx_frame_type))
    {
        LOGERROR("[bsp_can] Invalid tx_frame_type=0x%X", instance->tx_frame_type);
        return -1;
    }
    instance->rx_frame_type = CANFrameTypeNormalize(instance->rx_frame_type);
    if (!CANFrameTypeIsValid(instance->rx_frame_type))
    {
        LOGERROR("[bsp_can] Invalid rx_frame_type=0x%X", instance->rx_frame_type);
        return -1;
    }
    // 校验帧格式枚举合法性
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
    if (instance->tx_frame_format != CAN_FRAME_FORMAT_CLASSIC)
    {
        LOGERROR("[bsp_can] BxCAN does not support FD frames! tx_frame_format=%d", instance->tx_frame_format);
        return -1;
    }

    // 各 ID 范围上限取决于帧类型
    uint32_t tx_id_max = (instance->tx_frame_type & CAN_FRAME_EXTENDED) ? 0x1FFFFFFFU : 0x7FFU;
    uint32_t rx_id_max = (instance->rx_frame_type & CAN_FRAME_EXTENDED) ? 0x1FFFFFFFU : 0x7FFU;

    // 检查tx_id范围（-1 表示不发送）
    if (instance->tx_id != CAN_ID_UNUSED && instance->tx_id > tx_id_max)
    {
        LOGERROR("[bsp_can] Invalid tx_id=0x%lX, must be <= 0x%lX for %s frames", instance->tx_id, tx_id_max, (instance->tx_frame_type & CAN_FRAME_EXTENDED) ? "extended" : "standard");
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
            LOGERROR("[bsp_can] Invalid rx_id_list[%d]=0x%lX, must be <= 0x%lX for %s frames", i, instance->rx_id_list[i], rx_id_max, (instance->rx_frame_type & CAN_FRAME_EXTENDED) ? "extended" : "standard");
            return -1;
        }
    }

    // 检查掩码范围
    if (instance->rx_mask > rx_id_max)
    {
        LOGERROR("[bsp_can] Invalid rx_mask=0x%lX, must be <= 0x%lX for %s frames", instance->rx_mask, rx_id_max, (instance->rx_frame_type & CAN_FRAME_EXTENDED) ? "extended" : "standard");
        return -1;
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
        instance->tx_header.StdId = (instance->tx_frame_type & CAN_FRAME_EXTENDED) ? 0U : instance->tx_id;
        instance->tx_header.IDE = (instance->tx_frame_type & CAN_FRAME_EXTENDED) ? CAN_ID_EXT : CAN_ID_STD;
        instance->tx_header.ExtId = instance->tx_id;
        instance->tx_header.RTR = (instance->tx_frame_type & CAN_FRAME_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
        instance->tx_header.DLC = tx_len;
        // TransmitGlobalTime 已被 memset 清零
    }

    // 关键重排序：先启动（含 hal_can 重配置），后配置过滤器
    BSP_RETURN_IF_TRUE_LOG(CANBxcanStartIfNeeded(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] CAN start/notification init failed! can_e=%d", instance->can_e));

    if (need_rx_filter)
    {
        BSP_RETURN_IF_TRUE_LOG(CANBxcanAddFilter(instance) != HAL_OK, -1, LOGERROR("[bsp_can] CAN filter config failed! can_e=%d rx_id=0x%lX", instance->can_e, instance->rx_id_list[0]));
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

    // 需要发送完成回调时，激活 Mailbox Empty 通知（按 mailbox 区分回调实例）
    if (instance->tx_complete_callback != NULL)
    {
        (void)HAL_CAN_ActivateNotification(instance->map.handle, CAN_IT_TX_MAILBOX_EMPTY);
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

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanMessage(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanMessage(hcan, CAN_RX_FIFO1);
}

/* BxCAN mailbox 发送完成分发：按 mailbox 索引匹配实例，调用其发送完成回调。
 * @note media 异步分包发送者每次只提交一包、等此回调后再提交下一包，
 *       回调按 instance->tx_mailbox 精确命中该实例（一次一包在途）。 */
static void CANDispatchBxcanTxComplete(CAN_HandleTypeDef *hcan, uint32_t tx_mailbox)
{
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *instance = s_can_instance[i];
        if (instance->map.handle == hcan && instance->tx_mailbox == tx_mailbox && instance->tx_complete_callback != NULL)
            instance->tx_complete_callback(instance);
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanTxComplete(hcan, CAN_TX_MAILBOX0);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanTxComplete(hcan, CAN_TX_MAILBOX1);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CANDispatchBxcanTxComplete(hcan, CAN_TX_MAILBOX2);
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

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */
#endif /* CAN_INSTANCE_NUM > 0 */
#endif /* HAL_CAN_MODULE_ENABLED */

#endif /* BSP_CAN_USED */
