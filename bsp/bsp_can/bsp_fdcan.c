/**
 * @file bsp_fdcan.c
 * @brief FDCAN 驱动实现（自 bsp_can.c 拆分，FDCAN 平台专用）
 *
 * @note 仅当 BSP_CAN_IP == BSP_CAN_IP_FDCAN 时参与编译，BXCAN 平台编译 bsp_bxcan.c。
 *       实例管理与公共接口（CANConfig/CANRegister/CANTransmit）在本文件实现，
 *       代码逻辑与拆分前 bsp_can.c 完全一致。
 *       xrobot 移植（LibXR driver/st/stm32_canfd.cpp）：
 *       多订阅者分发（经典 + FD 双通道）/ 异步发送队列 TxService（FD 优先）/
 *       位时序 SetConfig（NBTP/DBTP/CCCR）/ 错误虚拟帧 / GetErrorState。
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

/*------------- 发送环形队列（classic + FD 双份，2 的幂容量） --------------*/
#define CAN_TX_Q_MASK (CAN_TX_QUEUE_SIZE - 1U)

static inline uint8_t CANClassicQFull(const CANInstance *inst)
{
    return (uint8_t)(((uint8_t)(inst->tx_q_w + 1U)) & CAN_TX_Q_MASK) == inst->tx_q_r;
}

static inline uint8_t CANClassicQEmpty(const CANInstance *inst)
{
    return inst->tx_q_r == inst->tx_q_w;
}

static void CANClassicQPut(CANInstance *inst, const CAN_ClassicPack_s *pack)
{
    __disable_irq();
    inst->tx_queue[inst->tx_q_w] = *pack;
    inst->tx_q_w = (uint8_t)((uint8_t)(inst->tx_q_w + 1U) & CAN_TX_Q_MASK);
    __enable_irq();
}

static void CANClassicQGet(CANInstance *inst, CAN_ClassicPack_s *pack)
{
    __disable_irq();
    *pack = inst->tx_queue[inst->tx_q_r];
    inst->tx_q_r = (uint8_t)((uint8_t)(inst->tx_q_r + 1U) & CAN_TX_Q_MASK);
    __enable_irq();
}

static inline uint8_t CANFdQFull(const CANInstance *inst)
{
    return (uint8_t)(((uint8_t)(inst->tx_fd_q_w + 1U)) & CAN_TX_Q_MASK) == inst->tx_fd_q_r;
}

static inline uint8_t CANFdQEmpty(const CANInstance *inst)
{
    return inst->tx_fd_q_r == inst->tx_fd_q_w;
}

static void CANFdQPut(CANInstance *inst, const CAN_FDPack_s *pack)
{
    __disable_irq();
    inst->tx_queue_fd[inst->tx_fd_q_w] = *pack;
    inst->tx_fd_q_w = (uint8_t)((uint8_t)(inst->tx_fd_q_w + 1U) & CAN_TX_Q_MASK);
    __enable_irq();
}

static void CANFdQGet(CANInstance *inst, CAN_FDPack_s *pack)
{
    __disable_irq();
    *pack = inst->tx_queue_fd[inst->tx_fd_q_r];
    inst->tx_fd_q_r = (uint8_t)((uint8_t)(inst->tx_fd_q_r + 1U) & CAN_TX_Q_MASK);
    __enable_irq();
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
    if (instance->rx_frame_type & CAN_FRAME_EXTENDED)
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

/**
 * @brief 构造经典帧发送头（xrobot STM32CANFD::BuildTxHeader(ClassicPack) 移植）
 */
static void CANFdcanBuildTxHeader(const CAN_ClassicPack_s *pack, FDCAN_TxHeaderTypeDef *h)
{
    uint8_t is_ext = (uint8_t)((pack->type & CAN_FRAME_EXTENDED) != 0U);
    uint8_t is_rtr = (uint8_t)((pack->type & CAN_FRAME_REMOTE) != 0U);

    h->Identifier = pack->id;
    h->IdType = is_ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    h->TxFrameType = is_rtr ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;

    uint32_t bytes = (pack->dlc <= 8U) ? pack->dlc : 8U;
    h->DataLength = CANLengthToFdcanDlc((uint8_t)bytes);

    h->ErrorStateIndicator = FDCAN_ESI_PASSIVE;
    h->BitRateSwitch = FDCAN_BRS_OFF;
    h->FDFormat = FDCAN_CLASSIC_CAN;
    h->TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    h->MessageMarker = 0x01;
}

/**
 * @brief 构造 FD 帧发送头（xrobot STM32CANFD::BuildTxHeader(FDPack) 移植）
 * @note FD 帧固定 FDFormat=FD_CAN + BRS_ON（对应上层 FD 配置）
 */
static void CANFdcanBuildTxHeaderFD(const CAN_FDPack_s *pack, FDCAN_TxHeaderTypeDef *h)
{
    h->Identifier = pack->id;
    h->IdType = ((pack->type & CAN_FRAME_EXTENDED) != 0U) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    h->TxFrameType = FDCAN_DATA_FRAME;

    h->DataLength = CANLengthToFdcanDlc(pack->len);

    h->ErrorStateIndicator = FDCAN_ESI_PASSIVE;
    h->BitRateSwitch = FDCAN_BRS_ON;
    h->FDFormat = FDCAN_FD_CAN;
    h->TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    h->MessageMarker = 0x00;
}

/**
 * @brief 异步发送服务：FD 优先填 Tx FIFO（xrobot STM32CANFD::TxService 移植）
 */
static void CANFdcanTxService(CANInstance *instance)
{
    if (instance == NULL || instance->map.handle == NULL)
    {
        return;
    }

    __disable_irq();
    instance->tx_pend = 1;
    if (instance->tx_lock != 0U)
    {
        __enable_irq();
        return;
    }
    instance->tx_lock = 1;
    __enable_irq();

    for (;;)
    {
        instance->tx_pend = 0;

        while (HAL_FDCAN_GetTxFifoFreeLevel(instance->map.handle) != 0U)
        {
            /* FD 优先 */
            if (!CANFdQEmpty(instance))
            {
                CAN_FDPack_s pfd;
                CANFdQGet(instance, &pfd);

                FDCAN_TxHeaderTypeDef hdr;
                CANFdcanBuildTxHeaderFD(&pfd, &hdr);

                if (HAL_FDCAN_AddMessageToTxFifoQ(instance->map.handle, &hdr, pfd.data) != HAL_OK)
                {
                    CANFdQPut(instance, &pfd); // 发送失败：回队列，不做立即 retry
                    break;
                }
                continue;
            }

            /* Classic */
            if (!CANClassicQEmpty(instance))
            {
                CAN_ClassicPack_s pc;
                CANClassicQGet(instance, &pc);

                FDCAN_TxHeaderTypeDef hdr;
                CANFdcanBuildTxHeader(&pc, &hdr);

                if (HAL_FDCAN_AddMessageToTxFifoQ(instance->map.handle, &hdr, pc.data) != HAL_OK)
                {
                    CANClassicQPut(instance, &pc);
                    break;
                }
                continue;
            }

            break; /* 两个池都空 */
        }

        __disable_irq();
        instance->tx_lock = 0;
        __enable_irq();

        if (instance->tx_pend == 0U)
        {
            return;
        }

        __disable_irq();
        if (instance->tx_lock != 0U)
        {
            __enable_irq();
            return;
        }
        instance->tx_lock = 1;
        __enable_irq();
    }
}

/**
 * @brief 错误帧虚拟事件分发（xrobot STM32CANFD::ProcessErrorStatusInterrupt 的 OnMessage 部分）
 */
static void CANFdcanDispatchError(FDCAN_HandleTypeDef *hfdcan, CANErrorID_e eid)
{
    CAN_ClassicPack_s pack = {0};
    pack.type = CAN_FRAME_ERROR;
    pack.id = CANFromErrorID(eid);
    pack.dlc = 0;

    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *inst = s_can_instance[i];
        if (inst->map.handle != hfdcan)
        {
            continue;
        }
        CANSubscriber_s *s = inst->sub_head;
        while (s != NULL)
        {
            if ((s->type & CAN_FRAME_ERROR) != 0U)
            {
                uint8_t id_ok;
                if (s->mode == CAN_FILTER_MATCH_MASK)
                {
                    id_ok = ((pack.id & s->start_id_mask) == s->end_id_mask);
                }
                else
                {
                    id_ok = (pack.id >= s->start_id_mask && pack.id <= s->end_id_mask);
                }
                if (id_ok && s->cb != NULL)
                {
                    s->cb(inst, &pack, 1u);
                }
            }
            s = s->next;
        }
    }
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

        uint8_t rx_is_ext = (rx_header.IdType == FDCAN_EXTENDED_ID);
        uint8_t rx_is_remote = (rx_header.RxFrameType == FDCAN_REMOTE_FRAME);
        uint8_t rx_is_fd = (rx_header.FDFormat == FDCAN_FD_CAN);
        CANFrameType_e rx_type = (CANFrameType_e)((rx_is_ext ? CAN_FRAME_EXTENDED : CAN_FRAME_STANDARD) |
                                                  (rx_is_remote ? CAN_FRAME_REMOTE : CAN_FRAME_DATA));
        uint32_t rx_len = CANFdcanDlcToLength(rx_header.DataLength);
        if (rx_len > 64U)
        {
            rx_len = 64U;
        }

        for (uint8_t i = 0; i < s_can_idx; i++)
        {
            CANInstance *instance = s_can_instance[i];
            if (instance->map.handle != hfdcan)
            {
                continue;
            }

            uint8_t dispatched = 0;

            /* xrobot 移植：多订阅者软件过滤分发。
             * FD 帧 → FD 订阅者（sub_fd_head）；经典帧 → 经典订阅者（sub_head）。 */
            if (rx_is_fd)
            {
                if (instance->sub_fd_head != NULL)
                {
                    CAN_FDPack_s pack = {0};
                    pack.id = rx_header.Identifier;
                    pack.type = rx_type;
                    pack.len = (uint8_t)rx_len;
                    if (!rx_is_remote)
                    {
                        memcpy(pack.data, rx_data, rx_len);
                    }

                    CANSubscriberFD_s *s = instance->sub_fd_head;
                    while (s != NULL)
                    {
                        if ((s->type & pack.type) != 0U)
                        {
                            uint8_t id_ok;
                            if (s->mode == CAN_FILTER_MATCH_MASK)
                            {
                                id_ok = ((pack.id & s->start_id_mask) == s->end_id_mask);
                            }
                            else
                            {
                                id_ok = (pack.id >= s->start_id_mask && pack.id <= s->end_id_mask);
                            }
                            if (id_ok)
                            {
                                if (s->cb != NULL)
                                {
                                    s->cb(instance, &pack, 1u);
                                }
                                dispatched = 1;
                            }
                        }
                        s = s->next;
                    }
                }
            }
            else
            {
                if (instance->sub_head != NULL)
                {
                    CAN_ClassicPack_s pack = {0};
                    pack.id = rx_header.Identifier;
                    pack.type = rx_type;
                    pack.dlc = (rx_len > 8U) ? 8U : (uint8_t)rx_len;
                    if (!rx_is_remote)
                    {
                        memcpy(pack.data, rx_data, pack.dlc);
                    }

                    CANSubscriber_s *s = instance->sub_head;
                    while (s != NULL)
                    {
                        if ((s->type & pack.type) != 0U)
                        {
                            uint8_t id_ok;
                            if (s->mode == CAN_FILTER_MATCH_MASK)
                            {
                                id_ok = ((pack.id & s->start_id_mask) == s->end_id_mask);
                            }
                            else
                            {
                                id_ok = (pack.id >= s->start_id_mask && pack.id <= s->end_id_mask);
                            }
                            if (id_ok)
                            {
                                if (s->cb != NULL)
                                {
                                    s->cb(instance, &pack, 1u);
                                }
                                dispatched = 1;
                            }
                        }
                        s = s->next;
                    }
                }
            }

            if (dispatched)
            {
                continue;
            }

            // 旧式兼容：按 rx_id_list / rx_mask 匹配（无订阅者命中时，含 FD 帧）
            // 帧类型匹配：ID 类型（标准/扩展）必须一致（防标准实例命中等值扩展帧）
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
                // 记录实际帧类型（位组合），回调中可用 instance->rx_frame_type_matched 区分
                instance->rx_frame_type_matched = rx_type;
                uint8_t rx_len8 = (rx_len > sizeof(instance->rx_buff)) ? (uint8_t)sizeof(instance->rx_buff) : (uint8_t)rx_len;
                instance->rx_len = rx_len8;
                // 经典帧缓冲 rx_pack.data 仅 8 字节，FD 帧需截断
                uint8_t pack_len = (rx_len8 > 8U) ? 8U : rx_len8;
                // 远程帧无数据载荷：rx_len 为请求的数据长度，rx_buff 不填充
                if (!rx_is_remote)
                {
                    memcpy(instance->rx_buff, rx_data, rx_len8);
                    memcpy(instance->rx_pack.data, rx_data, pack_len);
                }
                instance->rx_pack.id = rx_header.Identifier;
                instance->rx_pack.type = rx_type;
                instance->rx_pack.dlc = pack_len;

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

    // 本实例接收 FIFO（FDCAN 由过滤器 FilterConfig 决定，此处记录默认）
    instance->fifo = FDCAN_RX_FIFO0;

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
    // CAN FD 规范不支持远程帧：RTR 只能配经典 CAN
    if ((instance->tx_frame_type & CAN_FRAME_REMOTE) != 0U && instance->tx_frame_format != CAN_FRAME_FORMAT_CLASSIC)
    {
        LOGERROR("[bsp_can] RTR (remote frame) requires CLASSIC format, got tx_frame_format=%d", instance->tx_frame_format);
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
        instance->tx_header.IdType = (instance->tx_frame_type & CAN_FRAME_EXTENDED) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        instance->tx_header.TxFrameType = (instance->tx_frame_type & CAN_FRAME_REMOTE) ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
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

/*------------- xrobot 移植接口 --------------*/

int8_t CANSetConfig(CANInstance *instance, const CAN_Config_s *config)
{
    // 兼容接口：只用仲裁相位参数，FD 数据相位保持不动（xrobot SetConfig(CAN::Configuration)）
    if (instance == NULL || config == NULL)
    {
        return -1;
    }
    CAN_FDConfig_s fd_cfg = {0};
    fd_cfg.bitrate = config->bitrate;
    fd_cfg.sample_point = config->sample_point;
    fd_cfg.bit_timing = config->bit_timing;
    fd_cfg.mode = config->mode;
    // data_timing / fd_mode 全部置 0 → "保持原值"
    return CANSetFDConfig(instance, &fd_cfg);
}

int8_t CANSetFDConfig(CANInstance *instance, const CAN_FDConfig_s *config)
{
    if (instance == NULL || config == NULL || instance->map.handle == NULL)
    {
        return -1;
    }

    FDCAN_HandleTypeDef *hcan = (FDCAN_HandleTypeDef *)instance->map.handle;
    FDCAN_GlobalTypeDef *can = hcan->Instance;
    const CAN_BitTiming_s *bt = &config->bit_timing;
    const CAN_BitTiming_s *dbt = &config->data_timing;
    const CAN_Mode_s *mode = &config->mode;

    // 先关通知：只关本驱动用到的这些
    uint32_t it_mask = 0u;
#if defined(FDCAN_IT_RX_FIFO0_NEW_MESSAGE)
    it_mask |= FDCAN_IT_RX_FIFO0_NEW_MESSAGE;
#endif
#if defined(FDCAN_IT_RX_FIFO1_NEW_MESSAGE)
    it_mask |= FDCAN_IT_RX_FIFO1_NEW_MESSAGE;
#endif
#if defined(FDCAN_IT_TX_FIFO_EMPTY)
    it_mask |= FDCAN_IT_TX_FIFO_EMPTY;
#endif

    if (it_mask != 0u)
    {
        (void)HAL_FDCAN_DeactivateNotification(hcan, it_mask);
    }

    // 停止 FDCAN，进入 INIT / CCE 配置状态
    if (HAL_FDCAN_Stop(hcan) != HAL_OK)
    {
        return -1;
    }

#if defined(FDCAN_CCCR_INIT)
    SET_BIT(can->CCCR, FDCAN_CCCR_INIT);
#endif
#if defined(FDCAN_CCCR_CCE)
    SET_BIT(can->CCCR, FDCAN_CCCR_CCE);
#endif

    // 模式配置：one-shot / loopback / listen-only
#if defined(FDCAN_CCCR_DAR)
    if (mode->one_shot)
    {
        SET_BIT(can->CCCR, FDCAN_CCCR_DAR); // 禁用自动重发
    }
    else
    {
        CLEAR_BIT(can->CCCR, FDCAN_CCCR_DAR);
    }
#endif

    // triple_sampling 对 FDCAN 没意义，直接忽略
    (void)mode->triple_sampling;

#if defined(FDCAN_CCCR_TEST) && defined(FDCAN_TEST_LBCK)
    if (mode->loopback)
    {
        SET_BIT(can->CCCR, FDCAN_CCCR_TEST); // 内部回环
        SET_BIT(can->TEST, FDCAN_TEST_LBCK);
    }
    else
    {
        CLEAR_BIT(can->TEST, FDCAN_TEST_LBCK);
        CLEAR_BIT(can->CCCR, FDCAN_CCCR_TEST);
    }
#endif

#if defined(FDCAN_CCCR_MON)
    if (mode->listen_only)
    {
        SET_BIT(can->CCCR, FDCAN_CCCR_MON); // 总线监控（只听）
    }
    else
    {
        CLEAR_BIT(can->CCCR, FDCAN_CCCR_MON);
    }
#endif

    /* 仲裁相位时序范围校验并写入 NBTP（0 = 保持原值） */
#if defined(FDCAN_NBTP_NBRP_Msk)
    {
        const uint32_t NBRP_FIELD_MAX = (FDCAN_NBTP_NBRP_Msk >> FDCAN_NBTP_NBRP_Pos);
        const uint32_t NTSEG1_FIELD_MAX = (FDCAN_NBTP_NTSEG1_Msk >> FDCAN_NBTP_NTSEG1_Pos);
        const uint32_t NTSEG2_FIELD_MAX = (FDCAN_NBTP_NTSEG2_Msk >> FDCAN_NBTP_NTSEG2_Pos);
        const uint32_t NSJW_FIELD_MAX = (FDCAN_NBTP_NSJW_Msk >> FDCAN_NBTP_NSJW_Pos);
        const uint32_t NBRP_MAX = NBRP_FIELD_MAX + 1u;
        const uint32_t NTSEG1_MAX = NTSEG1_FIELD_MAX + 1u;
        const uint32_t NTSEG2_MAX = NTSEG2_FIELD_MAX + 1u;
        const uint32_t NSJW_MAX = NSJW_FIELD_MAX + 1u;

        if (bt->brp != 0u)
        {
            if (bt->brp < 1u || bt->brp > NBRP_MAX)
            {
                return -1;
            }
        }
        uint32_t ntseg1 = bt->prop_seg + bt->phase_seg1;
        if (bt->prop_seg != 0u || bt->phase_seg1 != 0u)
        {
            if (ntseg1 < 1u || ntseg1 > NTSEG1_MAX)
            {
                return -1;
            }
        }
        if (bt->phase_seg2 != 0u)
        {
            if (bt->phase_seg2 < 1u || bt->phase_seg2 > NTSEG2_MAX)
            {
                return -1;
            }
        }
        if (bt->sjw != 0u)
        {
            if (bt->sjw < 1u || bt->sjw > NSJW_MAX)
            {
                return -1;
            }
            if (bt->phase_seg2 != 0u && bt->sjw > bt->phase_seg2)
            {
                return -1;
            }
        }

        uint32_t nbtp_old = can->NBTP;
        uint32_t nbtp_new = nbtp_old;
        uint32_t nbtp_mask = 0u;

        if (bt->brp != 0u)
        {
            uint32_t nbrp = (bt->brp - 1u) & NBRP_FIELD_MAX;
            nbtp_mask |= FDCAN_NBTP_NBRP_Msk;
            nbtp_new &= ~FDCAN_NBTP_NBRP_Msk;
            nbtp_new |= (nbrp << FDCAN_NBTP_NBRP_Pos);
        }
        if (bt->prop_seg != 0u || bt->phase_seg1 != 0u)
        {
            uint32_t nts = (ntseg1 - 1u) & NTSEG1_FIELD_MAX;
            nbtp_mask |= FDCAN_NBTP_NTSEG1_Msk;
            nbtp_new &= ~FDCAN_NBTP_NTSEG1_Msk;
            nbtp_new |= (nts << FDCAN_NBTP_NTSEG1_Pos);
        }
        if (bt->phase_seg2 != 0u)
        {
            uint32_t nts2 = (bt->phase_seg2 - 1u) & NTSEG2_FIELD_MAX;
            nbtp_mask |= FDCAN_NBTP_NTSEG2_Msk;
            nbtp_new &= ~FDCAN_NBTP_NTSEG2_Msk;
            nbtp_new |= (nts2 << FDCAN_NBTP_NTSEG2_Pos);
        }
        if (bt->sjw != 0u)
        {
            uint32_t nsjw = (bt->sjw - 1u) & NSJW_FIELD_MAX;
            nbtp_mask |= FDCAN_NBTP_NSJW_Msk;
            nbtp_new &= ~FDCAN_NBTP_NSJW_Msk;
            nbtp_new |= (nsjw << FDCAN_NBTP_NSJW_Pos);
        }

        if (nbtp_mask != 0u)
        {
            nbtp_old &= ~nbtp_mask;
            nbtp_old |= (nbtp_new & nbtp_mask);
            can->NBTP = nbtp_old;
        }
    }
#endif

    /* 数据相位时序范围校验并写入 DBTP（仅 CAN FD） */
#if defined(FDCAN_DBTP_DBRP_Msk)
    {
        const uint32_t DBRP_FIELD_MAX = (FDCAN_DBTP_DBRP_Msk >> FDCAN_DBTP_DBRP_Pos);
        const uint32_t DTSEG1_FIELD_MAX = (FDCAN_DBTP_DTSEG1_Msk >> FDCAN_DBTP_DTSEG1_Pos);
        const uint32_t DTSEG2_FIELD_MAX = (FDCAN_DBTP_DTSEG2_Msk >> FDCAN_DBTP_DTSEG2_Pos);
        const uint32_t DSJW_FIELD_MAX = (FDCAN_DBTP_DSJW_Msk >> FDCAN_DBTP_DSJW_Pos);
        const uint32_t DBRP_MAX = DBRP_FIELD_MAX + 1u;
        const uint32_t DTSEG1_MAX = DTSEG1_FIELD_MAX + 1u;
        const uint32_t DTSEG2_MAX = DTSEG2_FIELD_MAX + 1u;
        const uint32_t DSJW_MAX = DSJW_FIELD_MAX + 1u;

        if (dbt->brp != 0u)
        {
            if (dbt->brp < 1u || dbt->brp > DBRP_MAX)
            {
                return -1;
            }
        }
        uint32_t dtseg1 = dbt->prop_seg + dbt->phase_seg1;
        if (dbt->prop_seg != 0u || dbt->phase_seg1 != 0u)
        {
            if (dtseg1 < 1u || dtseg1 > DTSEG1_MAX)
            {
                return -1;
            }
        }
        if (dbt->phase_seg2 != 0u)
        {
            if (dbt->phase_seg2 < 1u || dbt->phase_seg2 > DTSEG2_MAX)
            {
                return -1;
            }
        }
        if (dbt->sjw != 0u)
        {
            if (dbt->sjw < 1u || dbt->sjw > DSJW_MAX)
            {
                return -1;
            }
            if (dbt->phase_seg2 != 0u && dbt->sjw > dbt->phase_seg2)
            {
                return -1;
            }
        }

        uint32_t dbtp_old = can->DBTP;
        uint32_t dbtp_new = dbtp_old;
        uint32_t dbtp_mask = 0u;

        if (dbt->brp != 0u)
        {
            uint32_t dbrp = (dbt->brp - 1u) & DBRP_FIELD_MAX;
            dbtp_mask |= FDCAN_DBTP_DBRP_Msk;
            dbtp_new &= ~FDCAN_DBTP_DBRP_Msk;
            dbtp_new |= (dbrp << FDCAN_DBTP_DBRP_Pos);
        }
        if (dbt->prop_seg != 0u || dbt->phase_seg1 != 0u)
        {
            uint32_t dt1 = (dtseg1 - 1u) & DTSEG1_FIELD_MAX;
            dbtp_mask |= FDCAN_DBTP_DTSEG1_Msk;
            dbtp_new &= ~FDCAN_DBTP_DTSEG1_Msk;
            dbtp_new |= (dt1 << FDCAN_DBTP_DTSEG1_Pos);
        }
        if (dbt->phase_seg2 != 0u)
        {
            uint32_t dt2 = (dbt->phase_seg2 - 1u) & DTSEG2_FIELD_MAX;
            dbtp_mask |= FDCAN_DBTP_DTSEG2_Msk;
            dbtp_new &= ~FDCAN_DBTP_DTSEG2_Msk;
            dbtp_new |= (dt2 << FDCAN_DBTP_DTSEG2_Pos);
        }
        if (dbt->sjw != 0u)
        {
            uint32_t dsjw = (dbt->sjw - 1u) & DSJW_FIELD_MAX;
            dbtp_mask |= FDCAN_DBTP_DSJW_Msk;
            dbtp_new &= ~FDCAN_DBTP_DSJW_Msk;
            dbtp_new |= (dsjw << FDCAN_DBTP_DSJW_Pos);
        }

        if (dbtp_mask != 0u)
        {
            dbtp_old &= ~dbtp_mask;
            dbtp_old |= (dbtp_new & dbtp_mask);
            can->DBTP = dbtp_old;
        }
    }
#else
    (void)dbt;
#endif

    // 数据相位 FDMode：这里不动寄存器，只保留在上层语义中使用
    (void)config->fd_mode;

    // 重新启动 FDCAN
    if (HAL_FDCAN_Start(hcan) != HAL_OK)
    {
        return -1;
    }

    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_ERROR_PASSIVE, 0);
    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_ERROR_WARNING, 0);
    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_BUS_OFF, 0);
    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);
    (void)HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_TX_FIFO_EMPTY, 0);

    instance->mode = config->mode;
    instance->fd_mode = config->fd_mode;
    return 0;
}

int8_t CANSubscribe(CANInstance *instance, CANFrameType_e type, CANFilterMatchMode_e mode,
                    uint32_t start_id_mask, uint32_t end_id_mask, CANSubscriberCb_t cb)
{
    if (instance == NULL)
    {
        return -1;
    }

    // 归一化并校验帧类型（支持单独订阅 CAN_FRAME_ERROR）
    CANFrameType_e t = CANFrameTypeNormalize(type);
    if (!CANFrameTypeIsValid(t))
    {
        return -1;
    }

    // 静态池分配空槽（复用已释放槽位）
    CANSubscriber_s *s = NULL;
    for (uint8_t i = 0; i < CAN_SUBSCRIBER_NUM; i++)
    {
        if (!instance->subscriber[i].in_use)
        {
            s = &instance->subscriber[i];
            break;
        }
    }
    if (s == NULL)
    {
        LOGERROR("[bsp_can] subscriber pool full! can_e=%d", instance->can_e);
        return -1;
    }

    s->in_use = 1;
    s->mode = mode;
    s->start_id_mask = start_id_mask;
    s->end_id_mask = end_id_mask;
    s->type = t;
    s->cb = cb;

    // 头插链表
    s->next = instance->sub_head;
    instance->sub_head = s;
    instance->sub_count++;
    return 0;
}

int8_t CANUnsubscribe(CANInstance *instance, CANSubscriberCb_t cb)
{
    if (instance == NULL)
    {
        return -1;
    }

    CANSubscriber_s *prev = NULL;
    CANSubscriber_s *s = instance->sub_head;
    while (s != NULL)
    {
        if (s->cb == cb && s->in_use)
        {
            // 从链表摘除
            if (prev != NULL)
            {
                prev->next = s->next;
            }
            else
            {
                instance->sub_head = s->next;
            }
            s->in_use = 0;
            if (instance->sub_count > 0U)
            {
                instance->sub_count--;
            }
            return 0;
        }
        prev = s;
        s = s->next;
    }
    return -1; // 未找到
}

int8_t CANSubscribeFD(CANInstance *instance, CANFrameType_e type, CANFilterMatchMode_e mode,
                      uint32_t start_id_mask, uint32_t end_id_mask, CANSubscriberFDCb_t cb)
{
    if (instance == NULL)
    {
        return -1;
    }

    CANFrameType_e t = CANFrameTypeNormalize(type);
    // FD 帧仅支持数据帧（标准|数据 / 扩展|数据），不支持远程帧与错误帧
    if ((t & CAN_FRAME_ERROR) != 0U || (t & CAN_FRAME_REMOTE) != 0U)
    {
        return -1;
    }
    if (!CANFrameTypeIsValid(t))
    {
        return -1;
    }

    // 静态池分配空槽（复用已释放槽位）
    CANSubscriberFD_s *s = NULL;
    for (uint8_t i = 0; i < CAN_SUBSCRIBER_NUM; i++)
    {
        if (!instance->subscriber_fd[i].in_use)
        {
            s = &instance->subscriber_fd[i];
            break;
        }
    }
    if (s == NULL)
    {
        LOGERROR("[bsp_can] FD subscriber pool full! can_e=%d", instance->can_e);
        return -1;
    }

    s->in_use = 1;
    s->mode = mode;
    s->start_id_mask = start_id_mask;
    s->end_id_mask = end_id_mask;
    s->type = t;
    s->cb = cb;

    // 头插链表
    s->next = instance->sub_fd_head;
    instance->sub_fd_head = s;
    instance->sub_fd_count++;
    return 0;
}

int8_t CANUnsubscribeFD(CANInstance *instance, CANSubscriberFDCb_t cb)
{
    if (instance == NULL)
    {
        return -1;
    }

    CANSubscriberFD_s *prev = NULL;
    CANSubscriberFD_s *s = instance->sub_fd_head;
    while (s != NULL)
    {
        if (s->cb == cb && s->in_use)
        {
            // 从链表摘除
            if (prev != NULL)
            {
                prev->next = s->next;
            }
            else
            {
                instance->sub_fd_head = s->next;
            }
            s->in_use = 0;
            if (instance->sub_fd_count > 0U)
            {
                instance->sub_fd_count--;
            }
            return 0;
        }
        prev = s;
        s = s->next;
    }
    return -1; // 未找到
}

int8_t CANAddMessage(CANInstance *instance, const CAN_ClassicPack_s *pack)
{
    if (instance == NULL || pack == NULL)
    {
        return -1;
    }
    if ((pack->type & CAN_FRAME_ERROR) != 0U)
    {
        return -1; // 错误帧为虚拟事件，不可发送
    }

    __disable_irq();
    if (CANClassicQFull(instance))
    {
        __enable_irq();
        LOGWARNING("[bsp_can] TX queue full! can_e=%d", instance->can_e);
        return -1;
    }
    instance->tx_queue[instance->tx_q_w] = *pack;
    instance->tx_q_w = (uint8_t)((uint8_t)(instance->tx_q_w + 1U) & CAN_TX_Q_MASK);
    __enable_irq();

    // kick
    CANFdcanTxService(instance);
    return 0;
}

int8_t CANAddMessageFD(CANInstance *instance, const CAN_FDPack_s *pack)
{
    if (instance == NULL || pack == NULL)
    {
        return -1;
    }
    if (pack->len > 64U)
    {
        return -1;
    }
    // FD 帧仅支持数据帧（标准|数据 / 扩展|数据）
    if ((pack->type & CAN_FRAME_ERROR) != 0U || (pack->type & CAN_FRAME_REMOTE) != 0U)
    {
        return -1;
    }

    __disable_irq();
    if (CANFdQFull(instance))
    {
        __enable_irq();
        LOGWARNING("[bsp_can] FD TX queue full! can_e=%d", instance->can_e);
        return -1;
    }
    instance->tx_queue_fd[instance->tx_fd_q_w] = *pack;
    instance->tx_fd_q_w = (uint8_t)((uint8_t)(instance->tx_fd_q_w + 1U) & CAN_TX_Q_MASK);
    __enable_irq();

    // kick
    CANFdcanTxService(instance);
    return 0;
}

int8_t CANGetErrorState(CANInstance *instance, CAN_ErrorState_s *state)
{
    if (instance == NULL || state == NULL || instance->map.handle == NULL)
    {
        return -1;
    }

    FDCAN_HandleTypeDef *hcan = (FDCAN_HandleTypeDef *)instance->map.handle;

    FDCAN_ErrorCountersTypeDef counters = {0};
    if (HAL_FDCAN_GetErrorCounters(hcan, &counters) != HAL_OK)
    {
        return -1;
    }

    FDCAN_ProtocolStatusTypeDef proto = {0};
    if (HAL_FDCAN_GetProtocolStatus(hcan, &proto) != HAL_OK)
    {
        return -1;
    }

    state->tx_error_counter = counters.TxErrorCnt;
    state->rx_error_counter = counters.RxErrorCnt;
    state->bus_off = (proto.BusOff != 0u) ? 1u : 0u;
    state->error_passive = (proto.ErrorPassive != 0u) ? 1u : 0u;
    state->error_warning = (proto.Warning != 0u) ? 1u : 0u;

    return 0;
}

uint32_t CANGetClockFreq(CANInstance *instance)
{
    (void)instance;
    // 所有带 FDCAN 的 STM32 都通过 RCCEx 提供核时钟查询
#if defined(RCC_PERIPHCLK_FDCAN)
    return HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN);
#elif defined(RCC_PERIPHCLK_FDCAN1)
    return HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN1);
#else
    return 0u;
#endif
}

/*------------- HAL回调函数重写 --------------*/
// 有关错误回调：
// can总线发生错误，can外设的硬件寄存器和hal库的软件变量都会有记录。
// 所以需要清除硬件和软件错误。
// FDCAN (H7)：
// - 无 AutoBusOff 配置，但硬件会自动清除错误
// - HAL_FDCAN_IRQHandler 自动清除软件错误标志
// - 回调中记录日志 + 分发错误虚拟帧（xrobot 移植）

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
 * @note 本层只负责分发 + 续发异步队列（xrobot 移植）。
 *       media 异步分包发送者每次只提交一包、等此回调后再提交下一包，
 *       故 FIFO 全空即意味着自己上一包已发送完成。 */
void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan)
{
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *instance = s_can_instance[i];
        if (instance->map.handle != hfdcan)
        {
            continue;
        }
        // xrobot 移植：FIFO 腾空后续发异步队列
        CANFdcanTxService(instance);
        if (instance->tx_complete_callback != NULL)
        {
            instance->tx_complete_callback(instance);
        }
    }
}

void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    uint32_t error = HAL_FDCAN_GetError(hfdcan);

    if (error != HAL_FDCAN_ERROR_NONE)
    {
        LOGWARNING("[bsp_can] FDCAN Error: 0x%08lX", error);
    }

    // xrobot 移植：错误时尝试续发异步队列
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        CANInstance *instance = s_can_instance[i];
        if (instance->map.handle == hfdcan)
        {
            CANFdcanTxService(instance);
        }
    }
}

void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    FDCAN_ProtocolStatusTypeDef protocol_status = {0};
    (void)HAL_FDCAN_GetProtocolStatus(hfdcan, &protocol_status);

#if defined(FDCAN_IT_BUS_OFF) && defined(FDCAN_CCCR_INIT)
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0U && protocol_status.BusOff != 0U)
    {
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
    }
#endif

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

    /* xrobot 移植：错误虚拟帧分发（优先级 BUS_OFF > PASSIVE > WARNING > LEC） */
    CANErrorID_e eid = CAN_ERROR_ID_GENERIC;
    if (protocol_status.BusOff != 0u)
    {
        eid = CAN_ERROR_ID_BUS_OFF;
    }
    else if (protocol_status.ErrorPassive != 0u)
    {
        eid = CAN_ERROR_ID_ERROR_PASSIVE;
    }
    else if (protocol_status.Warning != 0u)
    {
        eid = CAN_ERROR_ID_ERROR_WARNING;
    }
    else
    {
        uint32_t lec = protocol_status.LastErrorCode & 0x7u;
        if (lec == 0u)
        {
            lec = protocol_status.DataLastErrorCode & 0x7u;
        }
        switch (lec)
        {
        case 0x01u:
            eid = CAN_ERROR_ID_STUFF;
            break;
        case 0x02u:
            eid = CAN_ERROR_ID_FORM;
            break;
        case 0x03u:
            eid = CAN_ERROR_ID_ACK;
            break;
        case 0x04u:
            eid = CAN_ERROR_ID_BIT1;
            break;
        case 0x05u:
            eid = CAN_ERROR_ID_BIT0;
            break;
        case 0x06u:
            eid = CAN_ERROR_ID_CRC;
            break;
        default:
            eid = CAN_ERROR_ID_OTHER;
            break;
        }
    }
    CANFdcanDispatchError(hfdcan, eid);
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_FDCAN */
#endif /* CAN_INSTANCE_NUM > 0 */
#endif /* HAL_FDCAN_MODULE_ENABLED */

#endif /* BSP_CAN_USED */
