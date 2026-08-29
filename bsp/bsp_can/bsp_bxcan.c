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
#include "bsp_dwt.h"

/*------------- 私有变量 --------------*/
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

// 发送溯源表：每个 CAN 的每个邮箱当前属于哪个实例（发送完成回调据此回调）
static CANInstance *s_can_tx_owner[CAN_NUM_MAX][CAN_TX_MAILBOX_NUM] = {{NULL}};

// 外设启动标志：全部初始化步骤成功后才置位。HAL State 中途失败后无法据此重试，必须用独立标志兜底
static uint8_t s_can_started[CAN_NUM_MAX] = {0};

/*------------- CAN 外设状态/错误统计（调试用，调试器直接 Watch s_bxcan_status） --------------*/

/**
 * @brief BxCAN 外设状态与错误统计（每 CAN 一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：错误回调（①）、RxFIFO0/1 FULL 与 overrun（②）、CANTransmit 返回 -1（③）、
 *       发送完成回调，外加便于"判断当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;   /* 发送完成次数（TxMailbox*CompleteCallback） */
    uint32_t tx_fail; /* CANTransmit 返回 -1 次数 */
    uint32_t rx_ok;   /* 成功收帧次数 */
    uint32_t rx_full; /* RxFIFO0/1 FULL 事件次数 */
    uint32_t rx_lost; /* RxFIFO0/1 overrun 丢帧次数（FOV0/FOV1） */
    /* 错误计数 */
    uint32_t err_bus_off;  /* bus-off 进入次数 */
    uint32_t err_passive;  /* error passive 进入次数 */
    uint32_t err_warning;  /* error warning 进入次数 */
    uint32_t err_protocol; /* 协议错误次数（LEC: STF/FOR/ACK/BR/BD/CRC） */
    uint32_t err_tx;       /* 发送错误次数（ALST/TERR，仲裁失败或发送错误） */
    /* 实时状态快照（最近一次错误回调采样） */
    uint8_t bus_off;       /* ESR.BOFF */
    uint8_t error_passive; /* ESR.EPVF */
    uint8_t error_warning; /* ESR.EWGF */
    uint8_t lec;           /* ESR.LEC（上一次错误码） */
    uint8_t tec;           /* ESR.TEC bit16-23 */
    uint8_t rec;           /* ESR.REC bit24-31 */
    uint8_t tx_free;       /* 空闲邮箱数（发送后采样） */
    uint8_t rx_fifo0_fill; /* RF0R.FMP */
    uint8_t rx_fifo1_fill; /* RF1R.FMP */
} CAN_BxcanStatus_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile CAN_BxcanStatus_s s_bxcan_status[CAN_NUM_MAX];

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
 * @brief hcan → can_e 索引（CAN_1/CAN_2 对应 can_map 下标）
 * @retval CAN_NUM_MAX 未找到
 */
static uint8_t CAN_HcanToIndex(const CAN_HandleTypeDef *hcan)
{
    uint8_t i;

    for (i = 0; i < CAN_NUM_MAX; i++)
    {
        if (can_map[i].handle == hcan)
            return i;
    }
    return CAN_NUM_MAX;
}

/**
 * @brief 发送完成处理：查溯源表，调用所属实例的发送完成回调
 * @param hcan       硬件句柄
 * @param mailbox_idx 邮箱索引（0/1/2）
 */
static void CAN_TxCompleteHandler(CAN_HandleTypeDef *hcan, uint8_t mailbox_idx)
{
    uint8_t can_idx = CAN_HcanToIndex(hcan);
    CANInstance *inst;

    if (can_idx >= CAN_NUM_MAX)
        return;

    // 状态统计：邮箱完成发送 = 一帧已真正发出
    s_bxcan_status[can_idx].tx_ok++;

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

    // 首次配置：硬过滤全通 + 启动外设 + 使能接收/发送/错误中断（全部成功后才置位 s_can_started）
    // 用独立标志而非 HAL State 判断：任一步失败返回后，重试 CANConfig 会重走完整初始化（见下 Stop 归一）
    if (!s_can_started[instance->can_e])
    {
        CAN_FilterTypeDef hw_filter = {0};
        uint32_t it_mask;

        // 上次中途失败可能停在 LISTENING（Start 已成功、后续步骤失败）：先停回 READY，
        // 否则 ConfigFilter/Start 等 READY 门控的 HAL 调用会再次失败，重试永远不成功
        if (instance->map.handle->State == HAL_CAN_STATE_LISTENING)
        {
            BSP_RETURN_IF_TRUE_LOG(HAL_CAN_Stop(instance->map.handle) != HAL_OK, -1,
                                   LOGERROR("[bsp_can] CAN Stop failed, can't retry init (can_e=%d)!", instance->can_e));
        }

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
        // RxFIFO0/1 满 / 溢出中断：满计数在 FullCallback（需 FULL IT），丢帧（overrun）在
        // ErrorCallback 经 HAL_CAN_ERROR_RX_FOV0/1 统计（需 OVERRUN IT，见 IRQHandler 各 FIFO 分支）
        it_mask |= CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN |
                   CAN_IT_RX_FIFO1_FULL | CAN_IT_RX_FIFO1_OVERRUN;
        // 错误状态中断：Error Warning / Error Passive / Bus-off / Last Error Code（HAL_CAN_ErrorCallback 上报）
        // 注意：必须同时使能 CAN_IT_ERROR（IER.ERRIE 主开关），否则 IRQHandler 的错误分支不执行
        it_mask |= CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE | CAN_IT_ERROR;

        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ConfigFilter(instance->map.handle, &hw_filter) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ConfigFilter failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_Start(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ActivateNotification(instance->map.handle, it_mask) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ActivateNotification failed!"));

        // 全部初始化步骤成功后才置位：任一步失败返回，标志保持 0，下次 CANConfig 可完整重试
        s_can_started[instance->can_e] = 1;
    }

    return 0;
}

/**
 * @brief CANTransmit 失败路径统一计数后返回 -1
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏内 `return (ret)` 会求值），
 *       使所有 -1 返回点都统计进 s_bxcan_status[].tx_fail，日志行为不变。
 */
static int8_t CAN_BxcanTxFailThenRet(const CANInstance *instance)
{
    if (instance != NULL && instance->can_e < CAN_NUM_MAX)
        s_bxcan_status[instance->can_e].tx_fail++;
    return -1;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param timeout_ms    发送资源等待超时（ms）：三个邮箱全满时最多等待其空闲；传 0 表示不等待，满即失败
 * @param tx_mailbox    出参：本次发送使用的邮箱索引（0/1/2，对应 HAL CAN_TX_MAILBOX0/1/2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余空闲邮箱数（0~CAN_TX_MAILBOX_NUM）；可为 NULL
 * @retval 0  发送成功
 * @retval -1 失败（参数非法 / 长度超限 / 帧类型非法 / 等待邮箱超时 / 加入邮箱失败）
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t mailbox;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, CAN_BxcanTxFailThenRet(instance), LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, CAN_BxcanTxFailThenRet(instance), LOGERROR("[bsp_can] CAN handle is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, CAN_BxcanTxFailThenRet(instance), LOGERROR("[bsp_can] Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 8, CAN_BxcanTxFailThenRet(instance), LOGERROR("[bsp_can] Length %d exceeds classic CAN max (8)!", pack->len));

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
        return CAN_BxcanTxFailThenRet(instance);
    }
    tx_header.DLC = pack->len;

    // 邮箱空闲等待：三个发送邮箱全满则轮询等待其释放（timeout_ms 上限，0 表示不等待、满即失败）。
    // 无总线信号/对端离线时邮箱持续被占满（帧发不出去），不能死等，超时返回失败
    if (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0)
    {
        if (timeout_ms == 0)
        {
            LOGERROR("[bsp_can] TX mailboxes full!");
            return CAN_BxcanTxFailThenRet(instance);
        }
        else
        {
            uint64_t start_time = DWT_GetTimeUs();
            uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

            while (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0)
            {
                if ((DWT_GetTimeUs() - start_time) > timeout_us)
                {
                    LOGWARNING("[bsp_can] CAN TX mailbox timeout (can_e=%d, id=0x%lX)!", instance->can_e, (unsigned long)pack->id);
                    return CAN_BxcanTxFailThenRet(instance);
                }
            }
        }
    }

    // 加入发送邮箱
    if (HAL_CAN_AddTxMessage(instance->map.handle, &tx_header, pack->data, &mailbox) != HAL_OK)
    {
        LOGERROR("[bsp_can] HAL_CAN_AddTxMessage failed!");
        return CAN_BxcanTxFailThenRet(instance);
    }

    // 溯源：记录该邮箱当前属于哪个实例（发送完成回调据此调用其回调）
    s_can_tx_owner[instance->can_e][CAN_MailboxIndex(mailbox)] = instance;

    // 出参：使用的邮箱索引 + 发送后剩余空闲邮箱数（顺带更新状态快照）
    if (tx_mailbox != NULL)
        *tx_mailbox = CAN_MailboxIndex(mailbox);
    s_bxcan_status[instance->can_e].tx_free = (uint8_t)HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);
    if (tx_free_level != NULL)
        *tx_free_level = s_bxcan_status[instance->can_e].tx_free;

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
    uint8_t can_idx = CAN_HcanToIndex(hcan);
    uint8_t i;

    if (HAL_CAN_GetRxMessage(hcan, fifo, &rx_header, rx_data) != HAL_OK)
        return;

    // 状态统计：收帧成功 + 更新对应 FIFO 填充快照
    if (can_idx < CAN_NUM_MAX)
    {
        s_bxcan_status[can_idx].rx_ok++;
        if (fifo == CAN_RX_FIFO0)
            s_bxcan_status[can_idx].rx_fifo0_fill = (uint8_t)HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0);
        else
            s_bxcan_status[can_idx].rx_fifo1_fill = (uint8_t)HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO1);
    }

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

/*------------- 接收 FIFO 满回调（由 CANConfig 激活 CAN_IT_RX_FIFO0/1_FULL；满时新帧覆盖最旧帧） --------------*/

/**
 * @brief FIFO0 满事件回调（状态统计：rx_full）
 */
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan)
{
    uint8_t can_idx = CAN_HcanToIndex(hcan);

    if (can_idx < CAN_NUM_MAX)
        s_bxcan_status[can_idx].rx_full++;
}

/**
 * @brief FIFO1 满事件回调（状态统计：rx_full）
 */
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan)
{
    uint8_t can_idx = CAN_HcanToIndex(hcan);

    if (can_idx < CAN_NUM_MAX)
        s_bxcan_status[can_idx].rx_full++;
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

/*------------- 错误状态中断回调（由 CANConfig 激活 ERROR_WARNING / ERROR_PASSIVE / BUSOFF / LAST_ERROR_CODE） --------------*/

/**
 * @brief CAN 错误状态中断回调：Bus-off / Error Passive / Error Warning / 协议错误上报
 * @note 三个错误状态可同时置位（如 Bus-off 同时伴随 Warning/Passive），必须用独立 if 分别记录，else-if 会漏报。
 *       F4 bus-off 恢复依赖 CubeMX 配置的 AutoBusOff=ENABLE（硬件自动重同步）+ HAL_CAN_ResetError 清软件错误标志。
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t error = HAL_CAN_GetError(hcan);

    if (error != HAL_CAN_ERROR_NONE)
    {
        uint32_t esr = hcan->Instance->ESR;
        uint8_t can_idx = CAN_HcanToIndex(hcan);
        uint8_t tec = (esr >> 16) & 0xFF;
        uint8_t rec = (esr >> 24) & 0xFF; // REC 硬件为 8 位（bit24-31），旧代码误用 0x7F 少记最高位

        // 状态快照 + 错误计数（三个错误状态独立 if，可同时置位）
        if (can_idx < CAN_NUM_MAX)
        {
            s_bxcan_status[can_idx].tec = tec;
            s_bxcan_status[can_idx].rec = rec;
            s_bxcan_status[can_idx].lec = (esr >> 4) & 0x7;
            s_bxcan_status[can_idx].bus_off = (esr >> 2) & 0x1;
            s_bxcan_status[can_idx].error_passive = (esr >> 1) & 0x1;
            s_bxcan_status[can_idx].error_warning = esr & 0x1;

            if (error & HAL_CAN_ERROR_BOF)
                s_bxcan_status[can_idx].err_bus_off++;
            if (error & HAL_CAN_ERROR_EPV)
                s_bxcan_status[can_idx].err_passive++;
            if (error & HAL_CAN_ERROR_EWG)
                s_bxcan_status[can_idx].err_warning++;
            if ((error & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) != 0U)
                s_bxcan_status[can_idx].rx_lost++;
            if (error & (HAL_CAN_ERROR_STF | HAL_CAN_ERROR_FOR | HAL_CAN_ERROR_ACK |
                         HAL_CAN_ERROR_BR | HAL_CAN_ERROR_BD | HAL_CAN_ERROR_CRC))
                s_bxcan_status[can_idx].err_protocol++;
            if (error & (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_ALST2 |
                         HAL_CAN_ERROR_TX_TERR0 | HAL_CAN_ERROR_TX_TERR1 | HAL_CAN_ERROR_TX_TERR2))
                s_bxcan_status[can_idx].err_tx++;
        }

        if (error & HAL_CAN_ERROR_BOF)
            LOGERROR("[bsp_can] CAN Bus-off! TEC=%d, REC=%d", tec, rec);
        if (error & HAL_CAN_ERROR_EPV)
            LOGWARNING("[bsp_can] CAN Error Passive! TEC=%d, REC=%d", tec, rec);
        if (error & HAL_CAN_ERROR_EWG)
            LOGWARNING("[bsp_can] CAN Error Warning! TEC=%d, REC=%d", tec, rec);
        // 其余为 Last Error Code 位集（协议错误）或发送失败标志，汇总上报
        if (error & (HAL_CAN_ERROR_STF | HAL_CAN_ERROR_FOR | HAL_CAN_ERROR_ACK |
                     HAL_CAN_ERROR_BR | HAL_CAN_ERROR_BD | HAL_CAN_ERROR_CRC))
            LOGWARNING("[bsp_can] CAN protocol error: 0x%08lX, TEC=%d, REC=%d", (unsigned long)error, tec, rec);

        // 清软件错误标志（硬件 AutoBusOff=ENABLE 自动完成总线恢复）
        HAL_CAN_ResetError(hcan);
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */

#endif /* BSP_CAN_USED */
