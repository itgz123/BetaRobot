/**
 * @file bsp_fdcan.c
 * @brief FDCAN驱动封装实现（H7 平台，经典 CAN / CAN FD）
 *
 * @note 与 bsp_bxcan.c 共用统一接口（CANRegister / CANConfig / CANTransmit）。
 *       发送/接收机制跟随 CubeMX 配置（首次配置时读 hfdcan->Init 自动适配）：
 *         - 接收：Rx FIFO0（全局过滤全通，只走 Rx FIFO），收帧后软件过滤分发
 *         - 发送：Tx FIFO（HAL_FDCAN_AddMessageToTxFifoQ），tx_free_level = 剩余可发送元素数
 *         - 发送完成：Tx Event FIFO + MessageMarker（8-bit，池 0~31）溯源，对应 BxCAN 的邮箱索引
 *       本文件仅在 BSP_CAN_IP == BSP_CAN_IP_FDCAN 时编译。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN

#include "bsp_assert.h"
#include "bsp_uart_log.h"

/*------------- 私有宏 --------------*/
#define FDCAN_TX_MARKER_NUM 32 // MessageMarker 为 0~31，对应 BxCAN 的 3 个邮箱索引（TxEventsNbr=32 和 TxFifoQueueElmtsNbr=32都是32）

/* DLC 码（0~15）→ 实际字节数（与 HAL 内部 DLCtoBytes 表一致） */
static const uint8_t s_fdcan_dlc_bytes[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/*------------- 私有变量 --------------*/
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

// 发送溯源表：每个 FDCAN 的每个 MessageMarker 当前属于哪个实例（发送完成回调据此回调）
static CANInstance *s_fdcan_tx_owner[CAN_NUM_MAX][FDCAN_TX_MARKER_NUM] = {{NULL}};

/*------------- 私有函数：工具 --------------*/

/**
 * @brief hfdcan → can_e 索引（hfdcan_1/2/3 对应 can_map 下标）
 */
static uint8_t FDCAN_HcanToIndex(const FDCAN_HandleTypeDef *hfdcan)
{
    uint8_t i;

    for (i = 0; i < CAN_NUM_MAX; i++)
    {
        if (can_map[i].handle == hfdcan)
            return i;
    }
    return CAN_NUM_MAX;
}

/**
 * @brief 字节数 → DLC 码（FD 帧按不小于 len 的档位取整）
 */
static uint32_t FDCAN_BytesToDlc(uint8_t len)
{
    if (len <= 8)
        return len; // DLC 0~8 与字节数一致
    if (len <= 12)
        return FDCAN_DLC_BYTES_12;
    if (len <= 16)
        return FDCAN_DLC_BYTES_16;
    if (len <= 20)
        return FDCAN_DLC_BYTES_20;
    if (len <= 24)
        return FDCAN_DLC_BYTES_24;
    if (len <= 32)
        return FDCAN_DLC_BYTES_32;
    if (len <= 48)
        return FDCAN_DLC_BYTES_48;
    return FDCAN_DLC_BYTES_64;
}

/**
 * @brief 帧类型与工作模式兼容性检查（FD 帧格式没有 RTR 位，不存在远程帧）
 * @param mode       实例工作模式（CAN_Mode_Type_e）
 * @param frame_type 帧类型（CAN_Frame_Type_e）
 * @retval 0  兼容
 * @retval -1 不兼容：非经典模式下出现远程帧
 * @note 经典模式（CAN_FRAME_FORMAT_CLASSIC）允许远程帧；FD/FD_BRS 模式拒绝远程帧。
 */
static int8_t FDCAN_CheckFrameTypeCompatible(CAN_Mode_Type_e mode, CAN_Frame_Type_e frame_type)
{
    if (mode == CAN_FRAME_FORMAT_CLASSIC)
        return 0;
    if (frame_type == CAN_STANDARD_REMOTE_FRAME || frame_type == CAN_EXTENDED_REMOTE_FRAME)
    {
        LOGERROR("[bsp_can] FD 模式(mode=%d)不支持远程帧(frame_type=%d)，FD 帧格式无 RTR 位!", mode, frame_type);
        return -1;
    }
    return 0;
}

/*------------- 私有函数：发送溯源 --------------*/

/**
 * @brief 分配一个空闲 MessageMarker 并登记所属实例
 * @note 先登记再入队：消息入队后可能立即发送、事件随之中断触发，需保证此时槽已登记
 */
static int8_t FDCAN_AllocMarker(uint8_t can_idx, CANInstance *instance, uint32_t *marker)
{
    uint8_t i;

    for (i = 0; i < FDCAN_TX_MARKER_NUM; i++)
    {
        if (s_fdcan_tx_owner[can_idx][i] == NULL)
        {
            s_fdcan_tx_owner[can_idx][i] = instance;
            *marker = i;
            return 0;
        }
    }
    return -1;
}

/**
 * @brief 发送完成处理：查溯源表，调用所属实例的发送完成回调
 * @param can_idx can_e 索引
 * @param marker  MessageMarker（0~31）
 */
static void FDCAN_TxCompleteHandler(uint8_t can_idx, uint32_t marker)
{
    CANInstance *inst;

    if (can_idx >= CAN_NUM_MAX || marker >= FDCAN_TX_MARKER_NUM)
        return;

    // 先清槽再回调：回调内可能立即再次发送并复用同一 marker
    inst = s_fdcan_tx_owner[can_idx][marker];
    s_fdcan_tx_owner[can_idx][marker] = NULL;
    if (inst != NULL && inst->tx_complete_callback != NULL)
        inst->tx_complete_callback(inst, marker);
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
 * @note 要求先调用 CANRegister 注册实例。
 *       FDCAN 发送/接收机制跟随 CubeMX 配置：首次配置读 hfdcan->Init 自动使能对应中断，
 *       无需在软件里硬编码 FIFO/Buffer 数量。
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    FDCAN_HandleTypeDef *hfdcan;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    hfdcan = instance->map.handle;
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, -1, LOGERROR("[bsp_can] FDCAN handle is NULL, check bsp_map mapping!"));

    // 一个 handle 允许多个实例共享（如不同 ID 分组各占一个实例），无需防重
    BSP_RETURN_IF_TRUE_LOG(config->filter_num > 0 && config->filters == NULL, -1, LOGERROR("[bsp_can] filters is NULL but filter_num=%d!", config->filter_num));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters; // 软件过滤器数组（Config 时写入，指向 config 中的数组）
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;

    // 模式与 CubeMX FrameFormat 兼容性检查：
    //  - 非 CLASSIC 实例需控制器启用 FD（CubeMX FrameFormat != CLASSIC，硬件 FDOE=1），否则 FD 帧发送失败
    //  - FD_BRS 实例还需控制器启用 BRS（CubeMX FrameFormat == FD_BRS，硬件 BRSE=1），否则数据段时序/8 Mbps 不生效
    if (instance->mode != CAN_FRAME_FORMAT_CLASSIC)
    {
        BSP_RETURN_IF_TRUE_LOG(hfdcan->Init.FrameFormat == FDCAN_FRAME_CLASSIC, -1,
                               LOGERROR("[bsp_can] 实例模式为 FD(mode=%d) 但 hfdcan FrameFormat=CLASSIC（CubeMX 配置），控制器未启用 FD!", instance->mode));
        BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_FD_BRS && hfdcan->Init.FrameFormat != FDCAN_FRAME_FD_BRS, -1,
                               LOGERROR("[bsp_can] 实例模式为 FD_BRS 但 hfdcan FrameFormat != FD_BRS（CubeMX 配置），控制器未启用 BRS，8 Mbps 数据段时序不会生效!"));
    }

    // 首次配置：硬过滤全通 + 启动外设 + 使能接收/发送中断（HAL_FDCAN_Start 后 State: READY → BUSY）
    if (hfdcan->State == HAL_FDCAN_STATE_READY)
    {
        FDCAN_InitTypeDef *init = &hfdcan->Init;
        uint32_t active_it = 0;

        // 发送统一接口走 Tx FIFO（AddMessageToTxFifoQ + GetTxFifoFreeLevel），需 CubeMX 配置
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueElmtsNbr == 0, -1, LOGERROR("[bsp_can] TxFifoQueueElmtsNbr=0! 统一接口发送走 Tx FIFO，请用 CubeMX 配置（TxFifoQueueElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueMode != FDCAN_TX_FIFO_OPERATION, -1, LOGERROR("[bsp_can] TxFifoQueueMode 需为 FDCAN_TX_FIFO_OPERATION（只要 Tx FIFO，不要 Tx Queue）!"));

        // 硬过滤全通：全部走 FDCAN_ACCEPT_IN_RX_FIFO0（接收只保留 Rx FIFO 路径）
        BSP_RETURN_IF_TRUE_LOG(init->RxFifo0ElmtsNbr == 0, -1, LOGERROR("[bsp_can] RxFifo0ElmtsNbr=0! 接收走 Rx FIFO0，请用 CubeMX 配置（RxFifo0ElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK,
                               -1, LOGERROR("[bsp_can] HAL_FDCAN_ConfigGlobalFilter failed!"));

        // 只使能 Rx FIFO0 新报文中断
        active_it |= FDCAN_IT_RX_FIFO0_NEW_MESSAGE;
        // 发送完成溯源：Tx Event FIFO + MessageMarker（需 CubeMX 配置 TxEventsNbr>0；FULL/LOST 用于检测溯源丢失）
        if (init->TxEventsNbr > 0)
            active_it |= FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST;

        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_Start(hfdcan) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_FDCAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ActivateNotification(hfdcan, active_it, 0) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_FDCAN_ActivateNotification failed!"));
    }

    return 0;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param tx_mailbox    出参：本次发送使用的发送标记（FDCAN=MessageMarker 0~31，对应 BxCAN 邮箱索引 0~2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余可发送元素数（Tx FIFO 空闲数）；可为 NULL
 * @retval 0  发送成功
 * @retval -1 失败（参数非法 / 长度超限 / 帧类型非法 / FIFO 满 / marker 占满 / 加入 FIFO 失败）
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint8_t *tx_mailbox, uint8_t *tx_free_level)
{
    FDCAN_HandleTypeDef *hfdcan;
    FDCAN_TxHeaderTypeDef tx_header = {0};
    uint32_t free_level;
    uint32_t marker = 0;
    uint8_t can_idx;
    uint8_t use_tx_event;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    hfdcan = instance->map.handle;
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, -1, LOGERROR("[bsp_can] FDCAN handle is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, -1, LOGERROR("[bsp_can] Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节，FD 最大 64 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 64, -1, LOGERROR("[bsp_can] Length %d exceeds FD max (64)!", pack->len));
    BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_CLASSIC && pack->len > 8, -1,
                           LOGERROR("[bsp_can] Classic mode but len=%d exceeds 8!", pack->len));

    // 帧类型与工作模式兼容性检查（FD 帧格式无 RTR 位：非经典模式拒绝远程帧）
    if (FDCAN_CheckFrameTypeCompatible(instance->mode, pack->frame_type) != 0)
        return -1;

    // 由帧类型填充发送头（ID 类型 / 帧类型 / ID）
    switch (pack->frame_type)
    {
    case CAN_STANDARD_DATA_FRAME:
        tx_header.IdType = FDCAN_STANDARD_ID;
        tx_header.TxFrameType = FDCAN_DATA_FRAME;
        tx_header.Identifier = pack->id;
        break;
    case CAN_EXTENDED_DATA_FRAME:
        tx_header.IdType = FDCAN_EXTENDED_ID;
        tx_header.TxFrameType = FDCAN_DATA_FRAME;
        tx_header.Identifier = pack->id;
        break;
    case CAN_STANDARD_REMOTE_FRAME:
        tx_header.IdType = FDCAN_STANDARD_ID;
        tx_header.TxFrameType = FDCAN_REMOTE_FRAME;
        tx_header.Identifier = pack->id;
        break;
    case CAN_EXTENDED_REMOTE_FRAME:
        tx_header.IdType = FDCAN_EXTENDED_ID;
        tx_header.TxFrameType = FDCAN_REMOTE_FRAME;
        tx_header.Identifier = pack->id;
        break;
    default:
        LOGERROR("[bsp_can] Invalid frame_type=%d!", pack->frame_type);
        return -1;
    }

    // 帧格式/BRS 由实例工作模式决定
    if (instance->mode == CAN_FRAME_FORMAT_FD_BRS)
    {
        tx_header.FDFormat = FDCAN_FRAME_FD_BRS;
        tx_header.BitRateSwitch = FDCAN_BRS_ON;
    }
    else if (instance->mode == CAN_FRAME_FORMAT_FD)
    {
        tx_header.FDFormat = FDCAN_FRAME_FD_NO_BRS;
        tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    }
    else
    {
        tx_header.FDFormat = FDCAN_FRAME_CLASSIC;
        tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    }
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.DataLength = FDCAN_BytesToDlc(pack->len);

    can_idx = instance->can_e;
    use_tx_event = (hfdcan->Init.TxEventsNbr > 0) ? 1 : 0; // Tx Event FIFO 使能才可溯源
    tx_header.TxEventFifoControl = use_tx_event ? FDCAN_STORE_TX_EVENTS : FDCAN_NO_TX_EVENTS;

    // Tx FIFO 空闲检查（快速失败，避免无效的 marker 分配）
    free_level = HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);
    BSP_RETURN_IF_TRUE_LOG(free_level == 0, -1, LOGERROR("[bsp_can] TX FIFO full!"));

    // 发送完成溯源：分配 MessageMarker 并先登记（入队后中断可能立即触发）
    if (use_tx_event)
    {
        if (FDCAN_AllocMarker(can_idx, instance, &marker) != 0)
        {
            LOGERROR("[bsp_can] All %d TX markers in flight (Tx Event FIFO busy)!", FDCAN_TX_MARKER_NUM);
            return -1;
        }
        tx_header.MessageMarker = marker;
    }

    // 加入发送 Tx FIFO
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &tx_header, pack->data) != HAL_OK)
    {
        if (use_tx_event)
            s_fdcan_tx_owner[can_idx][marker] = NULL; // 加入失败，释放 marker 槽
        LOGERROR("[bsp_can] HAL_FDCAN_AddMessageToTxFifoQ failed!");
        return -1;
    }

    // 出参：发送标记（BxCAN=邮箱索引 / FDCAN=MessageMarker）+ 剩余可发送数
    if (tx_mailbox != NULL)
        *tx_mailbox = marker;
    if (tx_free_level != NULL)
        *tx_free_level = HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);

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
 * @brief HAL 接收头 + 数据 → CAN_Pack_s
 */
static void FDCAN_BuildPack(const FDCAN_RxHeaderTypeDef *rx_header, const uint8_t *rx_data, CAN_Pack_s *pack)
{
    uint8_t i;

    pack->id = rx_header->Identifier;
    pack->len = s_fdcan_dlc_bytes[rx_header->DataLength & 0x0F];
    for (i = 0; i < pack->len; i++)
        pack->data[i] = rx_data[i];
    if (rx_header->RxFrameType == FDCAN_REMOTE_FRAME)
        pack->frame_type = (rx_header->IdType == FDCAN_EXTENDED_ID) ? CAN_EXTENDED_REMOTE_FRAME : CAN_STANDARD_REMOTE_FRAME;
    else
        pack->frame_type = (rx_header->IdType == FDCAN_EXTENDED_ID) ? CAN_EXTENDED_DATA_FRAME : CAN_STANDARD_DATA_FRAME;
}

/**
 * @brief 软件过滤分发：遍历本 CAN 上已注册实例，逐个匹配实例内的过滤器数组
 */
static void FDCAN_Dispatch(const FDCAN_HandleTypeDef *hfdcan, const CAN_Pack_s *pack)
{
    uint8_t i;

    for (i = 0; i < s_can_idx; i++)
    {
        CANInstance *inst = s_can_instance[i];
        uint8_t j;

        if (inst->map.handle != hfdcan)
            continue;
        for (j = 0; j < inst->filter_num; j++)
        {
            CAN_Filter_s *f = &inst->filters[j];

            if (f->callback == NULL)
                continue;
            if (CAN_FilterMatch(f, pack))
                f->callback(inst, pack);
        }
    }
}

/**
 * @brief 排空一个接收 FIFO：逐个读报文并分发（中断内一次性取空，避免 IRQ 风暴）
 */
static void FDCAN_ReceiveFifo(FDCAN_HandleTypeDef *hfdcan, uint32_t fifo)
{
    uint32_t fill = HAL_FDCAN_GetRxFifoFillLevel(hfdcan, fifo);

    while (fill > 0)
    {
        FDCAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[64];
        CAN_Pack_s pack = {0};

        if (HAL_FDCAN_GetRxMessage(hfdcan, fifo, &rx_header, rx_data) != HAL_OK)
            break;
        FDCAN_BuildPack(&rx_header, rx_data, &pack);
        FDCAN_Dispatch(hfdcan, &pack);
        fill--;
    }
}

/*------------- 接收中断回调 --------------*/

/**
 * @brief Rx FIFO0 新报文中断回调（接收只走 Rx FIFO0）
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    FDCAN_ReceiveFifo(hfdcan, FDCAN_RX_FIFO0);
}

/*------------- 发送完成回调（Tx Event FIFO + MessageMarker 溯源，由 CANConfig 激活 IT） --------------*/

/**
 * @brief Tx Event FIFO 中断回调：弹出事件并按 MessageMarker 溯源到实例
 */
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxEventFifoITs)
{
    uint8_t can_idx = FDCAN_HcanToIndex(hfdcan);
    FDCAN_TxEventFifoTypeDef event;
    uint32_t fill;

    if (can_idx >= CAN_NUM_MAX)
        return;

    // 事件 FIFO 满/丢失：有事件无法溯源，对应 marker 槽永久占用（发送完成回调会丢失）
    if (TxEventFifoITs & (FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST))
        LOGERROR("[bsp_can] Tx Event FIFO full/lost! 发送完成溯源可能丢失");

    // 逐个弹出事件并分发
    fill = (hfdcan->Instance->TXEFS & FDCAN_TXEFS_EFFL) >> FDCAN_TXEFS_EFFL_Pos;
    while (fill > 0)
    {
        if (HAL_FDCAN_GetTxEvent(hfdcan, &event) != HAL_OK)
            break;
        // MessageMarker 硬件为 8-bit，池内 0~31 用原值即可；越界值由 FDCAN_TxCompleteHandler 边界检查兜底
        FDCAN_TxCompleteHandler(can_idx, event.MessageMarker);
        fill--;
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_FDCAN */

#endif /* BSP_CAN_USED */
