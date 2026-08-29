/**
 * @file bsp_bxcan.c
 * @brief BxCAN驱动封装实现（F4 平台，经典 CAN）
 *
 * @note 只负责实例管理和外设重配置，不负责滤波器/收发（后续实现）。
 *       本文件仅在 BSP_CAN_IP == BSP_CAN_IP_BXCAN 时编译。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if BSP_CAN_IP == BSP_CAN_IP_BXCAN

#include "bsp_assert.h"
#include "bsp_uart_log.h"

/*------------- 私有变量 --------------*/
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

// 发送溯源表：每个 CAN 的每个邮箱当前属于哪个实例（发送完成回调据此回调）
static CANInstance *s_can_tx_owner[CAN_NUM_MAX][CAN_TX_MAILBOX_NUM] = {{NULL}};

/*------------- 私有函数：发送溯源 --------------*/

/**
 * @brief HAL 邮箱位掩码（CAN_TX_MAILBOX0/1/2 = 1/2/4）→ 索引（0/1/2）
 */
static uint8_t CAN_MailboxIndex(uint32_t mailbox)
{
    if (mailbox == CAN_TX_MAILBOX0)
        return 0;
    if (mailbox == CAN_TX_MAILBOX1)
        return 1;
    return 2; /* CAN_TX_MAILBOX2 */
}

/**
 * @brief 发送完成处理：查溯源表，调用所属实例的发送完成回调
 * @param hcan       硬件句柄
 * @param mailbox_idx 邮箱索引（0/1/2）
 */
static void CAN_TxCompleteHandler(CAN_HandleTypeDef *hcan, uint8_t mailbox_idx)
{
    uint8_t can_idx;
    CANInstance *inst;

    // hcan → can_e 索引（CAN_1/CAN_2）
    for (can_idx = 0; can_idx < CAN_NUM_MAX; can_idx++)
    {
        if (can_map[can_idx].handle == hcan)
            break;
    }
    if (can_idx >= CAN_NUM_MAX)
        return;

    // 先清槽再回调：回调内可能立即再次发送并复用同一邮箱
    inst = s_can_tx_owner[can_idx][mailbox_idx];
    s_can_tx_owner[can_idx][mailbox_idx] = NULL;
    if (inst != NULL && inst->tx_complete_callback != NULL)
        inst->tx_complete_callback(inst, mailbox_idx);
}

/*------------- 外部接口实现 --------------*/

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

    LOGINFO("[bsp_can] CAN Instance registered, idx=%d", s_can_idx - 1);
    return 0;
}

/**
 * @brief 配置CAN实例（填充硬件映射 + 工作模式 + 父指针，可重复调用）
 * @note 要求先调用 CANRegister 注册实例
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL, check bsp_map mapping!"));

    // 一个 handle 允许多个实例共享（如不同 ID 分组各占一个实例），无需防重
    BSP_RETURN_IF_TRUE_LOG(config->filter_num > 0 && config->filters == NULL, -1, LOGERROR("[bsp_can] filters is NULL but filter_num=%d!", config->filter_num));

    // F4 BxCAN 仅支持经典 CAN（FD 帧需 H7 FDCAN），非 CLASSIC 一律拒绝
    BSP_RETURN_IF_TRUE_LOG(config->mode != CAN_FRAME_FORMAT_CLASSIC, -1, LOGERROR("[bsp_can] BxCAN only supports CLASSIC frame format (mode=%d)!", config->mode));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters; // 软件过滤器数组（Config 时写入，指向 config 中的数组）
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;

    // 首次配置：硬过滤全通 + 启动外设 + 使能接收/发送中断（HAL_CAN_Start 后 State: READY → LISTENING）
    if (instance->map.handle->State == HAL_CAN_STATE_READY)
    {
        CAN_FilterTypeDef hw_filter = {0};
        uint32_t it_mask;

        hw_filter.FilterIdHigh = 0;
        hw_filter.FilterIdLow = 0;
        hw_filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 掩码模式
        hw_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 32位
        hw_filter.FilterMaskIdHigh = 0;
        hw_filter.FilterMaskIdLow = 0; // 掩码全 0 = 全通过
        hw_filter.FilterActivation = ENABLE;

        // CAN1 用 bank 0..13/FIFO0，CAN2 用 bank 14..27/FIFO1（SlaveStartFilterBank=14）
        if (instance->map.handle->Instance == CAN1)
        {
            hw_filter.FilterBank = 0;
            hw_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
            it_mask = CAN_IT_RX_FIFO0_MSG_PENDING;
        }
        else
        {
            hw_filter.FilterBank = 14;
            hw_filter.FilterFIFOAssignment = CAN_RX_FIFO1;
            it_mask = CAN_IT_RX_FIFO1_MSG_PENDING;
        }

        // 发送邮箱空中断：邮箱发完释放时触发，发送完成回调查表溯源需要
        it_mask |= CAN_IT_TX_MAILBOX_EMPTY;

        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ConfigFilter(instance->map.handle, &hw_filter) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ConfigFilter failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_Start(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ActivateNotification(instance->map.handle, it_mask) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ActivateNotification failed!"));
    }

    return 0;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param tx_mailbox    出参：本次发送使用的邮箱索引（0/1/2，对应 HAL CAN_TX_MAILBOX0/1/2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余空闲邮箱数（0~CAN_TX_MAILBOX_NUM）；可为 NULL
 * @retval 0  发送成功
 * @retval -1 失败（参数非法 / 长度超限 / 帧类型非法 / 邮箱全满 / 加入邮箱失败）
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t *tx_mailbox, uint8_t *tx_free_level)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t mailbox;
    uint32_t free_level;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, -1, LOGERROR("[bsp_can] Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 8, -1, LOGERROR("[bsp_can] Length %d exceeds classic CAN max (8)!", pack->len));

    // 由帧类型填充发送头（IDE/RTR/ID）
    switch (pack->frame_type)
    {
    case CAN_STANDARD_DATA_FRAME:
        tx_header.IDE = CAN_ID_STD;
        tx_header.RTR = CAN_RTR_DATA;
        tx_header.StdId = pack->id;
        break;
    case CAN_EXTENDED_DATA_FRAME:
        tx_header.IDE = CAN_ID_EXT;
        tx_header.RTR = CAN_RTR_DATA;
        tx_header.ExtId = pack->id;
        break;
    case CAN_STANDARD_REMOTE_FRAME:
        tx_header.IDE = CAN_ID_STD;
        tx_header.RTR = CAN_RTR_REMOTE;
        tx_header.StdId = pack->id;
        break;
    case CAN_EXTENDED_REMOTE_FRAME:
        tx_header.IDE = CAN_ID_EXT;
        tx_header.RTR = CAN_RTR_REMOTE;
        tx_header.ExtId = pack->id;
        break;
    default:
        LOGERROR("[bsp_can] Invalid frame_type=%d!", pack->frame_type);
        return -1;
    }
    tx_header.DLC = pack->len;

    // 邮箱空闲检查：三个发送邮箱全满则拒绝
    free_level = HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);
    BSP_RETURN_IF_TRUE_LOG(free_level == 0, -1, LOGERROR("[bsp_can] TX mailboxes full!"));

    // 加入发送邮箱
    if (HAL_CAN_AddTxMessage(instance->map.handle, &tx_header, pack->data, &mailbox) != HAL_OK)
    {
        LOGERROR("[bsp_can] HAL_CAN_AddTxMessage failed!");
        return -1;
    }

    // 溯源：记录该邮箱当前属于哪个实例（发送完成回调据此调用其回调）
    s_can_tx_owner[instance->can_e][CAN_MailboxIndex(mailbox)] = instance;

    // 出参：使用的邮箱索引 + 发送后剩余空闲邮箱数
    if (tx_mailbox != NULL)
        *tx_mailbox = CAN_MailboxIndex(mailbox);
    if (tx_free_level != NULL)
        *tx_free_level = HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);

    return 0;
}

/*------------- 私有函数：接收过滤 --------------*/

/**
 * @brief 软件过滤器匹配（硬过滤全通，实际过滤在此进行）
 * @note 三种模式对 id0/id1 的语义：
 *       MASK : (id & id0) == (id1 & id0) 命中
 *       LIST : id == id0 || id == id1（id1 = CAN_ID_UNUSED = 0xFFFFFFFF 表示未用，仅匹配 id0）
 *       RANGE: id0 <= id <= id1 命中
 */
static uint8_t CAN_FilterMatch(const CAN_Filter_s *filter, const CAN_Pack_s *pack)
{
    if (filter->frame_type != pack->frame_type)
        return 0;

    switch (filter->mode)
    {
    case CAN_FILTER_MODE_MASK:
        return (pack->id & filter->id0) == (filter->id1 & filter->id0);
    case CAN_FILTER_MODE_LIST:
        if (pack->id == filter->id0)
            return 1;
        return (filter->id1 != CAN_ID_UNUSED) && (pack->id == filter->id1);
    case CAN_FILTER_MODE_RANGE:
        return (pack->id >= filter->id0) && (pack->id <= filter->id1);
    default:
        return 0;
    }
}

/**
 * @brief 接收处理：解析 HAL 报文头为 CAN_Pack_s，并按软件过滤器分发
 * @param hcan 硬件句柄
 * @param fifo 接收 FIFO（CAN_RX_FIFO0 / CAN_RX_FIFO1）
 */
static void CAN_ReceiveHandler(CAN_HandleTypeDef *hcan, uint32_t fifo)
{
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    CAN_Pack_s pack = {0};
    uint8_t i;

    if (HAL_CAN_GetRxMessage(hcan, fifo, &rx_header, rx_data) != HAL_OK)
        return;

    // HAL 报文头 -> CAN_Pack_s
    pack.id = (rx_header.IDE == CAN_ID_EXT) ? rx_header.ExtId : rx_header.StdId;
    pack.len = rx_header.DLC;
    for (i = 0; i < pack.len; i++)
        pack.data[i] = rx_data[i];
    if (rx_header.RTR == CAN_RTR_REMOTE)
        pack.frame_type = (rx_header.IDE == CAN_ID_EXT) ? CAN_EXTENDED_REMOTE_FRAME : CAN_STANDARD_REMOTE_FRAME;
    else
        pack.frame_type = (rx_header.IDE == CAN_ID_EXT) ? CAN_EXTENDED_DATA_FRAME : CAN_STANDARD_DATA_FRAME;

    // 软件过滤分发：遍历本 CAN 上已注册实例，逐个匹配实例内的过滤器数组
    for (i = 0; i < s_can_idx; i++)
    {
        CANInstance *inst = s_can_instance[i];
        uint8_t j;

        if (inst->map.handle != hcan)
            continue;
        for (j = 0; j < inst->filter_num; j++)
        {
            CAN_Filter_s *f = &inst->filters[j];

            if (f->callback == NULL)
                continue;
            if (CAN_FilterMatch(f, &pack))
                f->callback(inst, &pack);
        }
    }
}

/**
 * @brief FIFO0 接收中断回调（CAN1 报文进入）
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_ReceiveHandler(hcan, CAN_RX_FIFO0);
}

/**
 * @brief FIFO1 接收中断回调（CAN2 报文进入）
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_ReceiveHandler(hcan, CAN_RX_FIFO1);
}

/*------------- 发送完成回调（邮箱发完释放时触发，由 CANConfig 激活 CAN_IT_TX_MAILBOX_EMPTY） --------------*/

/**
 * @brief 发送邮箱0完成回调
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxCompleteHandler(hcan, 0);
}

/**
 * @brief 发送邮箱1完成回调
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxCompleteHandler(hcan, 1);
}

/**
 * @brief 发送邮箱2完成回调
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxCompleteHandler(hcan, 2);
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */

#endif /* BSP_CAN_USED */
