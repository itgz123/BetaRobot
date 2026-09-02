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
#include "bsp_log.h"
#include "bsp_dwt.h"

/*------------- 私有宏 --------------*/
#define FDCAN_TX_MARKER_NUM 32 // MessageMarker 为 0~31，对应 BxCAN 的 3 个邮箱索引（TxEventsNbr=32 和 TxFifoQueueElmtsNbr=32都是32）

/* DLC 码（0~15）→ 实际字节数（与 HAL 内部 DLCtoBytes 表一致） */
static const uint8_t s_fdcan_dlc_bytes[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/*------------- 私有变量 --------------*/
LOG_INSTANCE_DEF(g_can_log, "can", 0); // CAN 日志实例
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

// 发送溯源表：每个 FDCAN 的每个 MessageMarker 当前属于哪个实例（发送完成回调据此回调）
static CANInstance *s_fdcan_tx_owner[CAN_NUM_MAX][FDCAN_TX_MARKER_NUM] = {{NULL}};

// 外设启动标志：全部初始化步骤成功后才置位。HAL State 中途失败后无法据此重试，必须用独立标志兜底
static uint8_t s_fdcan_started[CAN_NUM_MAX] = {0};

#if defined(BSP_CAN_LIST_LUT_USED)
/*------------- 接收 LIST 查表加速（标准 ID 直接索引，省双重循环线性扫描） -------------
 * s_fdcan_list_lut[CAN][ID] = 该标准 ID 的 LIST filter 所属实例（CANConfig 时登记）
 * 分发时按 ID 直接索引命中实例（再扫该实例 filters 精确匹配），替代全 CAN 线性扫描。
 * 一个 (CAN, ID) 只存一个实例指针：同 ID 多实例注册时后写覆盖前写（后注册实例优先）。
 */
static CANInstance *s_fdcan_list_lut[CAN_NUM_MAX][0x800]; /* 标准 ID(0~0x7FF) → 实例指针 */

/* 调试辅助：该 CAN 是否已有 LIST 标准帧查表槽位被登记（1=被覆盖过，调试器直接 Watch） */
volatile uint8_t s_fdcan_list_lut_used[CAN_NUM_MAX] = {0};
#endif

/*------------- CAN 外设状态/错误统计（调试用，调试器直接 Watch s_fdcan_status） --------------*/

/**
 * @brief FDCAN 外设状态与错误统计（每 CAN 一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：错误回调（①）、RxFIFO0 FULL/MESSAGE_LOST（②）、CANTransmit 返回 -1（③）、
 *       TxEventFifo FULL/LOST（④），外加便于"判断当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;         /* 发送完成次数（TxEventFifo 弹事件，每事件 +1） */
    uint32_t tx_fail;       /* CANTransmit 返回 -1 次数 */
    uint32_t rx_ok;         /* 成功收帧次数 */
    uint32_t rx_full;       /* RxFIFO0 FULL 事件次数 */
    uint32_t rx_lost;       /* RxFIFO0 MESSAGE_LOST 丢帧次数 */
    uint32_t tx_event_lost; /* TxEventFifo FULL/LOST 次数（溯源表丢失） */
    /* 错误计数 */
    uint32_t err_bus_off;    /* bus-off 进入次数 */
    uint32_t err_passive;    /* error passive 进入次数 */
    uint32_t err_warning;    /* error warning 进入次数 */
    uint32_t err_event;      /* 硬件累计错误事件数（ECR.CEL 差值累加，CEL 饱和 255 但取增量不受影响） */
    uint32_t err_ram_access; /* Message RAM 访问失败次数（IR.IRA，配置级严重故障，正常从不触发） */
    /* 实时状态快照（最近一次采样） */
    uint8_t bus_off;       /* PSR.BO */
    uint8_t error_passive; /* PSR.EP */
    uint8_t error_warning; /* PSR.EW */
    uint8_t lec;           /* PSR.LEC 上次错误码 */
    uint8_t tec;           /* ECR.TEC */
    uint8_t rec;           /* ECR.REC */
    uint8_t tx_free;       /* TXFQS 空闲元素数（发送后采样） */
    uint8_t rx_fifo0_fill; /* RXF0S.F0FL（收帧回调入口采样，反映突发深度） */
} CAN_FdcanStatus_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile CAN_FdcanStatus_s s_fdcan_status[CAN_NUM_MAX];

/* ECR.CEL 上次采样值（用于把硬件错误日志差值累加进 err_event） */
static uint8_t s_fdcan_hw_err_log[CAN_NUM_MAX] = {0};

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
 * @brief 采样并统计 Message RAM 访问失败（IR.IRA 粘滞位）
 * @note IRQHandler 对 RAM_ACCESS_FAILURE 只置 hfdcan->ErrorCode 无回调，故在各回调里采样清零。
 *       RAM 访问失败为配置级严重故障（正常运行时从不触发）。
 */
static void FDCAN_SampleRamAccessFail(uint8_t can_idx, FDCAN_HandleTypeDef *hfdcan)
{
    if (can_idx >= CAN_NUM_MAX)
        return;
    if (hfdcan->ErrorCode & HAL_FDCAN_ERROR_RAM_ACCESS)
    {
        s_fdcan_status[can_idx].err_ram_access++;
        CLEAR_BIT(hfdcan->ErrorCode, HAL_FDCAN_ERROR_RAM_ACCESS);
    }
}

/**
 * @brief 字节数 → DLC 码（FD 帧按不小于 len 的档位取整）
 * @retval >=0 DLC 码（0~15）
 * @retval -1  len > 64，超出 FD 上限（兜底；正常流程 CANTransmit 入口已校验，此处防御非法 DLC 写入）
 */
static int32_t FDCAN_BytesToDlc(uint8_t len)
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
    if (len <= 64)
        return FDCAN_DLC_BYTES_64;
    return -1;
}

/**
 * @brief FDCAN 元素尺寸枚举 → 实际字节数（与 TxElmtSize 越界校验配套）
 * @param elmt_size HAL 数据元素尺寸枚举（FDCAN_DATA_BYTES_*）
 * @retval 对应字节数（未知值退回 8，保证按最小值校验不误拒）
 */
static uint8_t FDCAN_ElmtSizeToBytes(uint32_t elmt_size)
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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] FD 模式(mode=%d)不支持远程帧(frame_type=%d)，FD 帧格式无 RTR 位!", mode, frame_type);
        return -1;
    }
    return 0;
}

#if defined(BSP_CAN_LIST_LUT_USED)
/*------------- 接收 LIST 查表：登记（宏 BSP_CAN_LIST_LUT_USED 启用，否则走下方循环判断） --------------*/

/**
 * @brief 判断 filter 是否可被 LIST 查表覆盖（LIST 模式 + 标准帧类型 + 至少一个 ID 在 0~0x7FF）
 * @note 与 CAN_FilterMatch 的 LIST 分支对齐：标准帧 pack->id <= 0x7FF，ID >0x7FF 视为 unused
 *       （永远匹配不到标准帧，不落槽）；frame_type 为标准数据/远程帧。
 */
static uint8_t FDCAN_ListLutCoverable(const CAN_Filter_s *f)
{
    if (f->mode != CAN_FILTER_MODE_LIST)
        return 0;
    if (f->frame_type != CAN_STANDARD_DATA_FRAME && f->frame_type != CAN_STANDARD_REMOTE_FRAME)
        return 0;
    if ((f->id0 > 0x7FF) && (f->id1 > 0x7FF))
        return 0;
    return 1;
}

/**
 * @brief 登记实例的 LIST 标准帧 filter 到查表槽位（CANConfig 调用，增量注册/重配置天然支持）
 * @note 只登记「标准帧 + LIST 模式」且 ID 合法的 filter：id0 必填（≤0x7FF）、id1 可空
 *       （CAN_ID_UNUSED 表示仅匹配 id0）；任一真实 ID >0x7FF 属配置非法，整条跳过不入表（日志告警）。
 *       同 (CAN, ID) 已被其他实例占用时后写覆盖前写，后注册实例优先。
 */
static void FDCAN_ListLutRegister(CANInstance *inst)
{
    uint8_t ci = inst->can_e;
    uint8_t j;

    if (ci >= CAN_NUM_MAX)
        return;
    for (j = 0; j < inst->filter_num; j++)
    {
        CAN_Filter_s *f = &inst->filters[j];

        if (f->callback == NULL)
            continue; /* 无回调，循环路径也跳过，不入表 */
        if (f->mode != CAN_FILTER_MODE_LIST)
            continue; /* MASK/RANGE 不入表，由循环兜底 */
        /* 扩展帧 LIST 不入表（29 位 ID 允许 >0x7FF），由循环兜底 */
        if (f->frame_type != CAN_STANDARD_DATA_FRAME && f->frame_type != CAN_STANDARD_REMOTE_FRAME)
            continue;

        /* 标准帧 LIST 校验：id0 必填、id1 可空(CAN_ID_UNUSED)；任一真实 ID >0x7FF 判非法，整条跳过 */
        if ((f->id0 > 0x7FF) || ((f->id1 != CAN_ID_UNUSED) && (f->id1 > 0x7FF)))
        {
            BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "[bsp_can] LIST 标准帧 filter ID 非法(id0=0x%lX id1=0x%lX)整条跳过，不入表",
                   (unsigned long)f->id0, (unsigned long)f->id1);
            continue;
        }
        /* 登记槽位并标记该 CAN 查表已被覆盖（调试直接 Watch s_fdcan_list_lut_used） */
        s_fdcan_list_lut_used[ci] = 1;
        if ((f->id0 == f->id1) || (f->id1 == CAN_ID_UNUSED))
            s_fdcan_list_lut[ci][(uint16_t)f->id0] = inst; /* 单 ID：只落 id0 一槽 */
        else
        {
            s_fdcan_list_lut[ci][(uint16_t)f->id0] = inst; /* 双 ID：id0/id1 各落一槽 */
            s_fdcan_list_lut[ci][(uint16_t)f->id1] = inst;
        }
    }
}
#endif

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

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_can_idx >= CAN_INSTANCE_NUM, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        if (s_can_instance[i] == instance)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Instance already registered!");
            return -1;
        }
    }

    s_can_instance[s_can_idx++] = instance;

    BSPLOG(&g_can_log, LOG_LEVEL_INFO, "[bsp_can] CAN Instance registered, idx=%d", s_can_idx - 1);
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

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    hfdcan = instance->map.handle;
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] FDCAN handle is NULL, check bsp_map mapping!"));

    // 一个 handle 允许多个实例共享（如不同 ID 分组各占一个实例），无需防重
    BSP_RETURN_IF_TRUE_LOG(config->filter_num > 0 && config->filters == NULL, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] filters is NULL but filter_num=%d!", config->filter_num));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters; // 软件过滤器数组（Config 时写入，指向 config 中的数组）
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;

#if defined(BSP_CAN_LIST_LUT_USED)
    // 标准 ID + LIST 模式 filter 直接登记查表槽位（增量注册/改 ID 天然支持；同 ID 后写覆盖前写）
    FDCAN_ListLutRegister(instance);
#endif

    // 模式与 CubeMX FrameFormat 兼容性检查：
    //  - 非 CLASSIC 实例需控制器启用 FD（CubeMX FrameFormat != CLASSIC，硬件 FDOE=1），否则 FD 帧发送失败
    //  - FD_BRS 实例还需控制器启用 BRS（CubeMX FrameFormat == FD_BRS，硬件 BRSE=1），否则数据段时序/8 Mbps 不生效
    if (instance->mode != CAN_FRAME_FORMAT_CLASSIC)
    {
        BSP_RETURN_IF_TRUE_LOG(hfdcan->Init.FrameFormat == FDCAN_FRAME_CLASSIC, -1,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] 实例模式为 FD(mode=%d) 但 hfdcan FrameFormat=CLASSIC（CubeMX 配置），控制器未启用 FD!", instance->mode));
        BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_FD_BRS && hfdcan->Init.FrameFormat != FDCAN_FRAME_FD_BRS, -1,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] 实例模式为 FD_BRS 但 hfdcan FrameFormat != FD_BRS（CubeMX 配置），控制器未启用 BRS，8 Mbps 数据段时序不会生效!"));
    }

    // 首次配置：硬过滤全通 + 启动外设 + 使能接收/发送/错误中断（全部成功后才置位 s_fdcan_started）
    // 用独立标志而非 HAL State 判断：任一步失败返回后，重试 CANConfig 会重走完整初始化（见下 Stop 归一）
    if (!s_fdcan_started[instance->can_e])
    {
        FDCAN_InitTypeDef *init = &hfdcan->Init;
        uint32_t active_it = 0;
        uint32_t line0_ints = 0;

        // 上次中途失败可能停在 BUSY（Start 已成功、后续步骤失败）：先停回 READY，
        // 否则 ConfigGlobalFilter/Start 等 READY 门控的 HAL 调用会再次失败，重试永远不成功
        if (hfdcan->State == HAL_FDCAN_STATE_BUSY)
        {
            BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_Stop(hfdcan) != HAL_OK, -1,
                                   BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] FDCAN Stop failed, can't retry init (can_e=%d)!", instance->can_e));
        }

        // 发送统一接口走 Tx FIFO（AddMessageToTxFifoQ + GetTxFifoFreeLevel），需 CubeMX 配置
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueElmtsNbr == 0, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] TxFifoQueueElmtsNbr=0! 统一接口发送走 Tx FIFO，请用 CubeMX 配置（TxFifoQueueElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueMode != FDCAN_TX_FIFO_OPERATION, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] TxFifoQueueMode 需为 FDCAN_TX_FIFO_OPERATION（只要 Tx FIFO，不要 Tx Queue）!"));

        // 硬过滤全通：全部走 FDCAN_ACCEPT_IN_RX_FIFO0（接收只保留 Rx FIFO 路径）
        BSP_RETURN_IF_TRUE_LOG(init->RxFifo0ElmtsNbr == 0, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] RxFifo0ElmtsNbr=0! 接收走 Rx FIFO0，请用 CubeMX 配置（RxFifo0ElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK,
                               -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] HAL_FDCAN_ConfigGlobalFilter failed!"));

        // 显式配置中断线：接收/发送事件/错误状态中断源全部映射到 IT0（对应 FDCANx_IT0_IRQn）
        // 注意：HAL_FDCAN_ConfigInterruptLines 是覆盖式调用，需将同一中断线的所有中断源合并
        line0_ints |= FDCAN_IT_RX_FIFO0_NEW_MESSAGE;
        // RxFIFO0 满 / 丢报文事件（HAL_FDCAN_RxFifo0Callback 内按 RxFifo0ITs 位统计）
        line0_ints |= FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST;
        // 错误状态中断：Bus-off / Error Passive / Error Warning（HAL_FDCAN_ErrorStatusCallback 上报 + 恢复）
        line0_ints |= FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
        // Message RAM 访问失败（IR.IRA，IRQHandler 只置 ErrorCode 无回调，在 Rx/错误回调里采样统计）
        line0_ints |= FDCAN_IT_RAM_ACCESS_FAILURE;
        // 发送完成溯源：Tx Event FIFO + MessageMarker（需 CubeMX 配置 TxEventsNbr>0；FULL/LOST 用于检测溯源丢失）
        if (init->TxEventsNbr > 0)
            line0_ints |= FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST;
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ConfigInterruptLines(hfdcan, line0_ints, FDCAN_INTERRUPT_LINE0) != HAL_OK,
                               -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] HAL_FDCAN_ConfigInterruptLines failed!"));

        // Rx FIFO0 新报文 + 满/丢失事件（满/丢失用于状态统计）
        active_it |= FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST;
        // 错误状态中断：Bus-off / Error Passive / Error Warning（HAL_FDCAN_ErrorStatusCallback 上报 + 恢复）
        active_it |= FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
        // Message RAM 访问失败（IR.IRA，IRQHandler 只置 ErrorCode 无回调，在 Rx/错误回调里采样统计）
        active_it |= FDCAN_IT_RAM_ACCESS_FAILURE;
        // 发送完成溯源：Tx Event FIFO + MessageMarker（需 CubeMX 配置 TxEventsNbr>0；FULL/LOST 用于检测溯源丢失）
        if (init->TxEventsNbr > 0)
            active_it |= FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST;

        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_Start(hfdcan) != HAL_OK, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] HAL_FDCAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ActivateNotification(hfdcan, active_it, 0) != HAL_OK, -1, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] HAL_FDCAN_ActivateNotification failed!"));

        // 全部初始化步骤成功后才置位：任一步失败返回，标志保持 0，下次 CANConfig 可完整重试
        s_fdcan_started[instance->can_e] = 1;
    }

    return 0;
}

/**
 * @brief CANTransmit 失败路径统一计数后返回 -1
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏内 `return (ret)` 会求值），
 *       使所有 -1 返回点都统计进 s_fdcan_status[].tx_fail，日志行为不变。
 */
static int8_t CAN_FdcanTxFailThenRet(const CANInstance *instance)
{
    if (instance != NULL && instance->can_e < CAN_NUM_MAX)
        s_fdcan_status[instance->can_e].tx_fail++;
    return -1;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param timeout_ms    发送资源等待超时（ms）：Tx FIFO 满时最多等待其空闲；传 0 表示不等待，满即失败
 * @param tx_mailbox    出参：本次发送使用的发送标记（FDCAN=MessageMarker 0~31，对应 BxCAN 邮箱索引 0~2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余可发送元素数（Tx FIFO 空闲数）；可为 NULL
 * @retval 0  发送成功
 * @retval -1 失败（参数非法 / 长度超限 / 帧类型非法 / 等待 FIFO 超时 / marker 占满 / 加入 FIFO 失败）
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level)
{
    FDCAN_HandleTypeDef *hfdcan;
    FDCAN_TxHeaderTypeDef tx_header = {0};
    uint32_t marker = 0;
    uint8_t can_idx;
    uint8_t use_tx_event;
    int32_t dlc;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, CAN_FdcanTxFailThenRet(instance), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Instance is NULL!"));
    hfdcan = instance->map.handle;
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, CAN_FdcanTxFailThenRet(instance), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] FDCAN handle is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, CAN_FdcanTxFailThenRet(instance), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节，FD 最大 64 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 64, CAN_FdcanTxFailThenRet(instance), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Length %d exceeds FD max (64)!", pack->len));
    BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_CLASSIC && pack->len > 8, CAN_FdcanTxFailThenRet(instance),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Classic mode but len=%d exceeds 8!", pack->len));

    // TxElmtSize 越界硬拒绝：len 超过外设 Tx 元素尺寸时，HAL_FDCAN_AddMessageToTxFifoQ 内部
    // FDCAN_CopyMessageToRAM 会按 len 越界写 Message RAM（内存破坏），必须硬拒绝而非仅警告
    {
        uint8_t elmt_bytes = FDCAN_ElmtSizeToBytes(hfdcan->Init.TxElmtSize);
        BSP_RETURN_IF_TRUE_LOG(pack->len > elmt_bytes, CAN_FdcanTxFailThenRet(instance),
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] len=%d exceeds FDCAN TxElmtSize=%d bytes!", pack->len, elmt_bytes));
    }

    // 帧类型与工作模式兼容性检查（FD 帧格式无 RTR 位：非经典模式拒绝远程帧）
    if (FDCAN_CheckFrameTypeCompatible(instance->mode, pack->frame_type) != 0)
        return CAN_FdcanTxFailThenRet(instance);

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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Invalid frame_type=%d!", pack->frame_type);
        return CAN_FdcanTxFailThenRet(instance);
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
    // len>64 兜底（正常流程入口已校验，此处防御非法 DLC 写入）
    dlc = FDCAN_BytesToDlc(pack->len);
    if (dlc < 0)
    {
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Length %d exceeds FD max (64)!", pack->len);
        return CAN_FdcanTxFailThenRet(instance);
    }
    tx_header.DataLength = (uint32_t)dlc;

    can_idx = instance->can_e;
    use_tx_event = (hfdcan->Init.TxEventsNbr > 0) ? 1 : 0; // Tx Event FIFO 使能才可溯源
    tx_header.TxEventFifoControl = use_tx_event ? FDCAN_STORE_TX_EVENTS : FDCAN_NO_TX_EVENTS;

    // Tx FIFO 空闲等待：满则轮询等待其释放（timeout_ms 上限，0 表示不等待、满即失败）。
    // 无总线信号/对端离线时 FIFO 持续占满（帧发不出去），不能死等，超时返回失败
    if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0)
    {
        if (timeout_ms == 0)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] TX FIFO full!");
            return CAN_FdcanTxFailThenRet(instance);
        }
        else
        {
            uint64_t start_time = DWT_GetTimeUs();
            uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

            while (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0)
            {
                if ((DWT_GetTimeUs() - start_time) > timeout_us)
                {
                    BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "[bsp_can] FDCAN Tx FIFO timeout (can_e=%d, id=0x%lX)!", instance->can_e, (unsigned long)pack->id);
                    return CAN_FdcanTxFailThenRet(instance);
                }
            }
        }
    }

    // 发送完成溯源：分配 MessageMarker 并先登记（入队后中断可能立即触发）
    if (use_tx_event)
    {
        if (FDCAN_AllocMarker(can_idx, instance, &marker) != 0)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] All %d TX markers in flight (Tx Event FIFO busy)!", FDCAN_TX_MARKER_NUM);
            return CAN_FdcanTxFailThenRet(instance);
        }
        tx_header.MessageMarker = marker;
    }

    // 加入发送 Tx FIFO
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &tx_header, pack->data) != HAL_OK)
    {
        if (use_tx_event)
            s_fdcan_tx_owner[can_idx][marker] = NULL; // 加入失败，释放 marker 槽
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] HAL_FDCAN_AddMessageToTxFifoQ failed!");
        return CAN_FdcanTxFailThenRet(instance);
    }

    // 出参：发送标记（BxCAN=邮箱索引 / FDCAN=MessageMarker）+ 剩余可发送数（顺带更新状态快照）
    if (tx_mailbox != NULL)
        *tx_mailbox = marker;
    s_fdcan_status[can_idx].tx_free = (uint8_t)HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);
    if (tx_free_level != NULL)
        *tx_free_level = s_fdcan_status[can_idx].tx_free;

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

#if defined(BSP_CAN_LIST_LUT_USED)
/**
 * @brief 分发（LUT 启用时的唯一入口）：先按标准 ID 查表回调 LIST 标准帧，再循环兜底其余模式
 * @note 查表命中直接回调（O(该实例 filter 数)，无全 CAN 线性扫描）；循环内跳过 LIST 标准帧
 *       （已由查表处理），兜底 MASK/RANGE/扩展/未登记，与未启用 LUT 的完整循环等价。
 *       frame_type 二次校验防同 ID 下 STD_DATA / STD_REMOTE 串。
 */
static void FDCAN_ListLutDispatch(uint8_t ci, const FDCAN_HandleTypeDef *hfdcan, const CAN_Pack_s *pack)
{
    uint16_t id;
    CANInstance *inst;
    uint8_t i;

    if (ci >= CAN_NUM_MAX)
        return;
    id = (uint16_t)(pack->id & 0x7FF); /* 必须 uint16_t：0~2047 */
    inst = s_fdcan_list_lut[ci][id];
    if (inst != NULL)
    {
        uint8_t j;

        for (j = 0; j < inst->filter_num; j++)
        {
            CAN_Filter_s *f = &inst->filters[j];

            if (f->callback != NULL && FDCAN_ListLutCoverable(f) &&
                f->frame_type == pack->frame_type &&
                (f->id0 == pack->id || (f->id1 != CAN_ID_UNUSED && f->id1 == pack->id)))
                f->callback(inst, pack);
        }
    }

    /* 循环兜底：LIST 标准帧已由查表分发（跳过），其余 MASK/RANGE/扩展/未登记在此匹配 */
    for (i = 0; i < s_can_idx; i++)
    {
        uint8_t j;

        inst = s_can_instance[i];
        if (inst->map.handle != hfdcan)
            continue;
        for (j = 0; j < inst->filter_num; j++)
        {
            CAN_Filter_s *f = &inst->filters[j];

            if (f->callback == NULL)
                continue;
            if (f->mode == CAN_FILTER_MODE_LIST &&
                (f->frame_type == CAN_STANDARD_DATA_FRAME || f->frame_type == CAN_STANDARD_REMOTE_FRAME))
                continue; /* LIST 标准帧已由查表分发，避免重复 */
            if (CAN_FilterMatch(f, pack))
                f->callback(inst, pack);
        }
    }
}
#else
/**
 * @brief 软件过滤循环分发（未启用 LUT 时的唯一入口）：遍历本 CAN 上已注册实例，逐个匹配过滤器数组
 */
static void FDCAN_LoopDispatch(const FDCAN_HandleTypeDef *hfdcan, const CAN_Pack_s *pack)
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
#endif

/**
 * @brief 排空一个接收 FIFO：逐个读报文并分发（中断内一次性取空，避免 IRQ 风暴）
 */
static void FDCAN_ReceiveFifo(FDCAN_HandleTypeDef *hfdcan, uint32_t fifo)
{
    uint8_t can_idx = FDCAN_HcanToIndex(hfdcan);
    uint32_t fill = HAL_FDCAN_GetRxFifoFillLevel(hfdcan, fifo);

    // 状态快照：入口填充数（反映突发深度）+ 硬件错误事件数（ECR.CEL 差值累加，CEL 饱和 255 取增量不受影响）
    if (can_idx < CAN_NUM_MAX)
    {
        FDCAN_ErrorCountersTypeDef ecr = {0};
        uint8_t cel;

        s_fdcan_status[can_idx].rx_fifo0_fill = (uint8_t)fill;
        (void)HAL_FDCAN_GetErrorCounters(hfdcan, &ecr);
        cel = (uint8_t)ecr.ErrorLogging;
        s_fdcan_status[can_idx].err_event += (uint8_t)(cel - s_fdcan_hw_err_log[can_idx]);
        s_fdcan_hw_err_log[can_idx] = cel;
    }

    while (fill > 0)
    {
        FDCAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[64];
        CAN_Pack_s pack = {0};

        if (HAL_FDCAN_GetRxMessage(hfdcan, fifo, &rx_header, rx_data) != HAL_OK)
            break;
        if (can_idx < CAN_NUM_MAX)
            s_fdcan_status[can_idx].rx_ok++;
        FDCAN_BuildPack(&rx_header, rx_data, &pack);
        // 软件过滤分发：LUT 启用时单入口（查表 + 循环兜底），未启用时纯循环
#if defined(BSP_CAN_LIST_LUT_USED)
        FDCAN_ListLutDispatch(can_idx, hfdcan, &pack); /* 查表命中直接回调 LIST 标准帧，其余循环兜底 */
#else
        FDCAN_LoopDispatch(hfdcan, &pack); /* 完整循环，全模式分发 */
#endif
        fill--;
    }
}

/*------------- 接收中断回调 --------------*/

/**
 * @brief Rx FIFO0 新报文中断回调（接收只走 Rx FIFO0）
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    uint8_t can_idx = FDCAN_HcanToIndex(hfdcan);

    // RxFIFO0 满 / 丢报文事件（可能丢包）：计数后照常排空
    if (can_idx < CAN_NUM_MAX)
    {
        if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_FULL)
            s_fdcan_status[can_idx].rx_full++;
        if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST)
            s_fdcan_status[can_idx].rx_lost++;
        FDCAN_SampleRamAccessFail(can_idx, hfdcan);
    }

    FDCAN_ReceiveFifo(hfdcan, FDCAN_RX_FIFO0);
}

/*------------- 错误状态中断回调（由 CANConfig 激活 BUS_OFF / ERROR_WARNING / ERROR_PASSIVE） --------------*/

/**
 * @brief FDCAN 错误状态中断回调：Bus-off / Error Passive / Error Warning 上报 + Bus-off 恢复
 * @note Bus-off 时清 CCCR.INIT 触发硬件自动恢复（H7 无软件手动恢复，靠硬件清错重同步）。
 *       三个状态可同时置位（如 Bus-off 同时伴随 Warning/Passive），必须用独立 if 分别记录，else-if 会漏报。
 */
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    FDCAN_ProtocolStatusTypeDef protocol_status = {0};
    FDCAN_ErrorCountersTypeDef error_counters = {0};
    uint8_t can_idx = FDCAN_HcanToIndex(hfdcan);

    (void)HAL_FDCAN_GetProtocolStatus(hfdcan, &protocol_status);
    (void)HAL_FDCAN_GetErrorCounters(hfdcan, &error_counters);

    // 状态快照 + 错误计数（三个状态独立 if，可同时置位）
    if (can_idx < CAN_NUM_MAX)
    {
        s_fdcan_status[can_idx].bus_off = (uint8_t)protocol_status.BusOff;
        s_fdcan_status[can_idx].error_passive = (uint8_t)protocol_status.ErrorPassive;
        s_fdcan_status[can_idx].error_warning = (uint8_t)protocol_status.Warning;
        s_fdcan_status[can_idx].lec = (uint8_t)protocol_status.LastErrorCode;
        s_fdcan_status[can_idx].tec = (uint8_t)error_counters.TxErrorCnt;
        s_fdcan_status[can_idx].rec = (uint8_t)error_counters.RxErrorCnt;
        // 错误事件数也在此采样：bus-off 期间无收帧，仅靠 Rx 回调采样会让 err_event 停滞
        s_fdcan_status[can_idx].err_event += (uint8_t)((uint8_t)error_counters.ErrorLogging - s_fdcan_hw_err_log[can_idx]);
        s_fdcan_hw_err_log[can_idx] = (uint8_t)error_counters.ErrorLogging;

        if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
            s_fdcan_status[can_idx].err_bus_off++;
        if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
            s_fdcan_status[can_idx].err_passive++;
        if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
            s_fdcan_status[can_idx].err_warning++;
        FDCAN_SampleRamAccessFail(can_idx, hfdcan);
    }

    // Bus-off 恢复：清 INIT 位请求离开初始化模式，硬件自动执行 Bus-off 恢复序列
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0U && protocol_status.BusOff != 0U)
    {
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
    }

    if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] FDCAN Bus-off! (can_e=%d)", can_idx);
    if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "[bsp_can] FDCAN Error Passive! (can_e=%d)", can_idx);
    if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "[bsp_can] FDCAN Error Warning! (can_e=%d)", can_idx);
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
    {
        s_fdcan_status[can_idx].tx_event_lost++;
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "[bsp_can] Tx Event FIFO full/lost! 发送完成溯源可能丢失");
    }

    // 逐个弹出事件并分发
    fill = (hfdcan->Instance->TXEFS & FDCAN_TXEFS_EFFL) >> FDCAN_TXEFS_EFFL_Pos;
    while (fill > 0)
    {
        if (HAL_FDCAN_GetTxEvent(hfdcan, &event) != HAL_OK)
            break;
        // 每弹出一个事件 = 一帧已真正发出（与 FDCAN_TxCompleteHandler 同一位置）
        s_fdcan_status[can_idx].tx_ok++;
        // MessageMarker 硬件为 8-bit，池内 0~31 用原值即可；越界值由 FDCAN_TxCompleteHandler 边界检查兜底
        FDCAN_TxCompleteHandler(can_idx, event.MessageMarker);
        fill--;
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_FDCAN */

#endif /* BSP_CAN_USED */
