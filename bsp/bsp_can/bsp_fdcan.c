/**
 * @file bsp_fdcan.c
 * @brief FDCAN驱动封装实现（H7 平台，经典 CAN / CAN FD）
 *
 * @note 与 bsp_bxcan.c 共用统一接口（CANRegister / CANConfig / CANTransmit）。
 *       发送/接收机制跟随 CubeMX 配置（首次配置时读 hfdcan->Init 自动适配）：
 *         - 接收：Rx FIFO0（全局过滤全通，只走 Rx FIFO），收帧后软件过滤分发
 *         - 发送：Tx FIFO（HAL_FDCAN_AddMessageToTxFifoQ），tx_free_level = 剩余可发送元素数
 *         - 发送完成：Tx Event FIFO + MessageMarker 溯源（8-bit = 代际 3 位 | 槽位 5 位，见下方私有宏），
 *           对应 BxCAN 的邮箱索引
 *       额外负责发送溯源与逐帧结果收口、错误分类统计与总线级错误广播、Tx Event 丢失后的
 *       marker 池回收。
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
#define FDCAN_TX_MARKER_NUM 32 // 槽位数（对应 BxCAN 的 3 个邮箱索引；TxEventsNbr=32 和 TxFifoQueueElmtsNbr=32 都是 32）

/* 硬件 MessageMarker 是 8-bit，故拆成 [代际 3 位 | 槽位 5 位]：
 *   高 3 位 = 该槽位的"代际"，槽位每次被回收就 +1（模 8）
 *   低 5 位 = 槽位号 0~31（对外出参 tx_mailbox 用的就是这个编码后的值，属于不透明标记，
 *            调用方只应原样回传给回调做比对，不要解释它的数值）
 * 为什么要代际：槽位回收后会被新帧复用，而旧帧的 Tx Event 可能仍在 FIFO 里排队。
 * 事件弹出时只带 MessageMarker，若无代际就无法区分"这是我这次发的帧"还是"上一轮的旧帧"，
 * 会把上一轮帧的结果错误地记到新的发起者头上（并提前清掉它正在等结果的槽）。
 * 3 位代际意味着一帧从入队到事件弹出之间要经历 8 次回收（≈256 个丢失事件）才会撞车，
 * 而 8-bit 是硬件上限，故取满。 */
#define FDCAN_TX_MARKER_SLOT_BITS 5
#define FDCAN_TX_MARKER_SLOT_MASK ((uint32_t)FDCAN_TX_MARKER_NUM - 1u)
#define FDCAN_TX_MARKER_GEN_MASK 0x07u

/* DLC 码（0~15）→ 实际字节数（与 HAL 内部 DLCtoBytes 表一致） */
static const uint8_t s_fdcan_dlc_bytes[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/*------------- 私有变量 --------------*/
#ifndef BSP_FDCAN_LOG_LIMIT
#define BSP_FDCAN_LOG_LIMIT 10
#endif                                                         // !BSP_FDCAN_LOG_LIMIT
LOG_INSTANCE_DEF(g_can_log, "bsp_fdcan", BSP_FDCAN_LOG_LIMIT); // CAN 日志实例
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

// 发送溯源表：每个 FDCAN 的每个 MessageMarker 当前属于哪个实例（发送完成回调据此回调）
static CANInstance *s_fdcan_tx_owner[CAN_NUM_MAX][FDCAN_TX_MARKER_NUM] = {{NULL}};

// 槽位代际表：见 FDCAN_TX_MARKER_SLOT_BITS 处的说明。仅在回收（Reclaim）一个槽时 +1，
// 用于让"回收前发出的旧帧"的迟到 Tx Event 能被识别出来并丢弃。
static uint8_t s_fdcan_tx_gen[CAN_NUM_MAX][FDCAN_TX_MARKER_NUM] = {{0}};

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
 *       信息来源：错误回调（①）、RxFIFO0 FULL/MESSAGE_LOST（②）、CANTransmit 的失败返回（③）、
 *       TxEventFifo 事件与 FULL/LOST（④），外加便于"判断当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;          /* 发送完成次数（TxEventFifo 弹事件，每事件 +1） */
    uint32_t tx_fail;        /* CANTransmit 返回非 BSP_OK 次数 */
    uint32_t tx_busy;        /* └ 其中"资源满且不等待"（BSP_BUSY）的次数——含 marker 池被占满 */
    uint32_t tx_timeout;     /* └ 其中等 Tx FIFO 耗尽 timeout_ms（BSP_TIMEOUT）的次数 */
    uint32_t tx_isr_clamp;   /* 中断上下文里被钳成"不等待"的发送次数（见 CANTransmit 入口） */
    uint32_t tx_abort;       /* 被 CANRecover 主动取消的在途帧数 */
    uint32_t tx_abort_late;  /* 取消请求迟到、帧仍发出去的事件数（Tx Event 的 IN_SPITE_OF_ABORT） */
    uint32_t rx_ok;          /* 成功收帧次数 */
    uint32_t rx_full;        /* RxFIFO0 FULL 事件次数 */
    uint32_t rx_lost;        /* RxFIFO0 MESSAGE_LOST 丢帧次数 */
    uint32_t tx_event_lost;  /* TxEventFifo FULL/LOST 次数（溯源表丢失） */
    uint32_t marker_reclaim; /* 因溯源丢失而回收的 marker 槽数（TxEvent FULL/LOST 或 CANRecover） */
    uint32_t tx_result_stale;/* 代际不符而丢弃的迟到 Tx Event 数（槽位已被回收并复用，结果不能归给新占用者） */
    /* 错误计数 */
    uint32_t err_bus_off;    /* bus-off 进入次数 */
    uint32_t err_passive;    /* error passive 进入次数 */
    uint32_t err_warning;    /* error warning 进入次数 */
    uint32_t err_event;      /* 硬件累计错误事件数（ECR.CEL 差值累加，CEL 饱和 255 但取增量不受影响） */
    uint32_t err_ram_access; /* Message RAM 访问失败次数（IR.IRA，配置级严重故障，正常从不触发） */
    /* 恢复计数（CANRecover） */
    uint32_t recover_ok;   /* 恢复后总线可用次数 */
    uint32_t recover_fail; /* 恢复后仍 bus-off / Tx FIFO 仍占满的次数 */
    /* 实时状态快照（最近一次采样） */
    uint8_t bus_off;       /* PSR.BO */
    uint8_t error_passive; /* PSR.EP */
    uint8_t error_warning; /* PSR.EW */
    uint8_t lec;           /* PSR.LEC 上次错误码 */
    uint8_t tec;           /* ECR.TEC */
    uint8_t rec;           /* ECR.REC */
    uint8_t tx_free;       /* TXFQS 空闲元素数（发送后采样） */
    uint8_t tx_owner_busy; /* 已入队但结果尚未收口的帧数（应 ≤ 32，总线安静时归零；长期不归零=marker 池泄漏） */
    uint8_t rx_fifo0_fill; /* RXF0S.F0FL（收帧回调入口采样，反映突发深度） */
    uint64_t last_err_us;  /* 最近一次错误回调的 DWT 时间戳（us），用于判断错误是否已经停止 */
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
 * @param can_idx can_e 索引
 * @param hfdcan  FDCAN 句柄
 * @retval 1 本次采样发现新的 RAM 访问失败；0 没有
 * @note IRQHandler 对 RAM_ACCESS_FAILURE 只置 hfdcan->ErrorCode 无回调，故在各回调里采样清零。
 *       RAM 访问失败为配置级严重故障（正常运行时从不触发）。返回值交给调用方广播
 *       CAN_ERR_HW —— 本函数在文件前部定义，拿不到后面才定义的 FDCAN_NotifyError。
 */
static uint8_t FDCAN_SampleRamAccessFail(uint8_t can_idx, FDCAN_HandleTypeDef *hfdcan)
{
    if (can_idx >= CAN_NUM_MAX)
        return 0;
    if (hfdcan->ErrorCode & HAL_FDCAN_ERROR_RAM_ACCESS)
    {
        s_fdcan_status[can_idx].err_ram_access++;
        CLEAR_BIT(hfdcan->ErrorCode, HAL_FDCAN_ERROR_RAM_ACCESS);
        return 1;
    }
    return 0;
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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "FD 模式(mode=%d)不支持远程帧(frame_type=%d)，FD 帧格式无 RTR 位!", mode, frame_type);
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
            BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "LIST 标准帧 filter ID 非法(id0=0x%lX id1=0x%lX)整条跳过，不入表",
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

/*------------- 私有函数：发送溯源与逐帧结果收口 --------------*/

/**
 * @brief 当前是否处于可阻塞等待的上下文（任务上下文且非临界区）
 * @note 判据与 USART_CanBlockingAbort / SPI_CanBlockingAbort 一致。
 *       这里挡的**不是死锁**——等待用 DWT 计时，不依赖 tick，ISR 里等不会卡死；
 *       挡的是"在中断里空转"：Tx FIFO 满时等待循环必然跑满 timeout_ms，而 comm media
 *       的续发钩子正是在 CAN 中断里以 timeout_ms=1 调 CANTransmit。
 */
static uint8_t FDCAN_CanWait(void)
{
    return ((__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U)) ? 1U : 0U;
}

/**
 * @brief 采样"已入队但结果尚未收口"的帧数（marker 池泄漏指示）
 * @note 正常应 ≤ FDCAN_TX_MARKER_NUM，总线安静时归零；长期不归零说明 marker 槽没被回收，
 *       攒满 32 个后该实例就再也发不出帧（见 FDCAN_ReclaimMarkers）。
 */
static uint8_t FDCAN_OwnerBusyCount(uint8_t can_idx)
{
    uint8_t i, n = 0;

    for (i = 0; i < FDCAN_TX_MARKER_NUM; i++)
    {
        if (s_fdcan_tx_owner[can_idx][i] != NULL)
            n++;
    }
    return n;
}

/** 槽位号 → 对外/硬件用的 MessageMarker（低 5 位槽位 + 高 3 位代际） */
static uint32_t FDCAN_MarkerEncode(uint8_t can_idx, uint8_t slot)
{
    return (((uint32_t)s_fdcan_tx_gen[can_idx][slot] & FDCAN_TX_MARKER_GEN_MASK) << FDCAN_TX_MARKER_SLOT_BITS) |
           ((uint32_t)slot & FDCAN_TX_MARKER_SLOT_MASK);
}

/** 从 MessageMarker 取槽位号 */
static uint8_t FDCAN_MarkerSlot(uint32_t marker)
{
    return (uint8_t)(marker & FDCAN_TX_MARKER_SLOT_MASK);
}

/** 从 MessageMarker 取代际 */
static uint8_t FDCAN_MarkerGen(uint32_t marker)
{
    return (uint8_t)((marker >> FDCAN_TX_MARKER_SLOT_BITS) & FDCAN_TX_MARKER_GEN_MASK);
}

/**
 * @brief 分配一个空闲 marker 槽并登记所属实例
 * @param marker 出参：**编码后**的 MessageMarker（代际<<5 | 槽位），既写进硬件也回给调用方
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
            *marker = FDCAN_MarkerEncode(can_idx, i);
            return 0;
        }
    }
    return -1;
}

/** 释放一个 marker 槽（入队失败时用；不带代际变更——该帧从未进硬件，不会有事件回来） */
static void FDCAN_FreeMarker(uint8_t can_idx, uint32_t marker)
{
    s_fdcan_tx_owner[can_idx][FDCAN_MarkerSlot(marker)] = NULL;
}

/**
 * @brief 逐帧结果收口：查溯源表 → 清槽 → 回调所属实例（成功与失败共用一条路径）
 * @param can_idx can_e 索引
 * @param marker  编码后的 MessageMarker（代际<<5 | 槽位 0~31），来自 Tx Event
 * @param result  BSP_OK=帧已真正发出；BSP_HW_ERR=被取消 / 溯源丢失
 * @note **先清槽再回调**：回调内可能立即再次发送并复用同一槽。
 *       槽为空（该帧已由另一条路径收口，或溯源信息已丢）时只计数、不回调。
 * @note **代际不符的事件丢弃**：槽位被回收（CANRecover / 事件丢失）后会被新帧复用，
 *       而旧帧的事件可能仍在 FIFO 里。若它的代际与该槽当前代际不符，说明这是上一轮
 *       的迟到事件，其结果不能记到当前占用者头上（也不能清掉它正在等结果的槽）。
 */
static void FDCAN_TxResultHandler(uint8_t can_idx, uint32_t marker, BSP_Status_e result)
{
    CANInstance *inst;
    uint8_t slot;

    if (can_idx >= CAN_NUM_MAX)
        return;

    slot = FDCAN_MarkerSlot(marker);
    if (slot >= FDCAN_TX_MARKER_NUM)
        return;

    if (FDCAN_MarkerGen(marker) != (s_fdcan_tx_gen[can_idx][slot] & FDCAN_TX_MARKER_GEN_MASK))
    {
        s_fdcan_status[can_idx].tx_result_stale++;
        return;
    }

    if (result == BSP_OK)
        s_fdcan_status[can_idx].tx_ok++;

    inst = s_fdcan_tx_owner[can_idx][slot];
    s_fdcan_tx_owner[can_idx][slot] = NULL;
    s_fdcan_status[can_idx].tx_owner_busy = FDCAN_OwnerBusyCount(can_idx);
    if (inst != NULL && inst->tx_complete_callback != NULL)
        inst->tx_complete_callback(inst, marker, result);
}

/**
 * @brief 回收全部 marker 槽并把结果通知给发起者（BSP_HW_ERR）
 * @param can_idx can_e 索引
 * @param reclaim 1=原因是溯源信息已丢失（Tx Event FIFO 满/丢），0=主动取消（CANRecover）
 * @retval 回收的槽数
 *
 * @note **为什么必须显式回收**：marker 槽只有弹出一个 Tx Event 才会被释放，而
 *       ① Tx Event FIFO 满时新事件直接丢弃（TEFL），丢失的事件永远补不回来；
 *       ② 被成功取消的帧**不产生 Tx Event**（M_CAN 只在"已发出"或"取消请求迟到"时写事件）。
 *       两种情况都会让槽位永久占用，攒满 32 个该实例就再也发不出帧（"All 32 TX markers
 *       in flight"），且没有任何回调会来告诉上层。这里把它们统一放掉。
 * @note **代价**：无法按 marker 区分"哪些帧真的丢了"，一律按失败上报，故最多有 32 帧会被
 *       上层重发（接收端若按序号去重可自动消掉重复）。这个取舍是刻意的：marker 池泄漏是
 *       **永久性**死锁，而多收几帧只是短暂冗余。
 * @note 回收时一并推进该槽的**代际**（见 FDCAN_TX_MARKER_SLOT_BITS）：被回收的帧可能
 *       仍会写出 Tx Event（取消请求迟到 = 帧其实发出去了），代际一变，那条迟到事件就会被
 *       FDCAN_TxResultHandler 识别为陈旧结果丢弃，不会算到下一个占用者头上。
 */
static uint8_t FDCAN_ReclaimMarkers(uint8_t can_idx, uint8_t reclaim)
{
    uint8_t i, n = 0;

    if (can_idx >= CAN_NUM_MAX)
        return 0;

    for (i = 0; i < FDCAN_TX_MARKER_NUM; i++)
    {
        CANInstance *owner = s_fdcan_tx_owner[can_idx][i];
        uint32_t marker;

        if (owner == NULL)
            continue;

        marker = FDCAN_MarkerEncode(can_idx, i); // 先按当前代际编码，回调要拿到与 CANTransmit 出参一致的值
        s_fdcan_tx_owner[can_idx][i] = NULL;
        s_fdcan_tx_gen[can_idx][i] = (uint8_t)((s_fdcan_tx_gen[can_idx][i] + 1u) & FDCAN_TX_MARKER_GEN_MASK);
        n++;
        if (reclaim)
            s_fdcan_status[can_idx].marker_reclaim++;
        else
            s_fdcan_status[can_idx].tx_abort++;
        if (owner->tx_complete_callback != NULL)
            owner->tx_complete_callback(owner, marker, BSP_HW_ERR);
    }
    s_fdcan_status[can_idx].tx_owner_busy = FDCAN_OwnerBusyCount(can_idx);
    return n;
}

/**
 * @brief 向该 CAN 上所有注册了 err_callback 的实例广播一个错误原因
 * @note 错误是**外设级**的（同一 handle 上可以有多个实例共享同一条总线），无法归给某一个
 *       实例，故广播给该 handle 上的所有注册者。handler 契约要求幂等，重复收到同一原因无害；
 *       没配 err_callback 的实例（如只关心收发的电机驱动）直接跳过。
 */
static void FDCAN_NotifyError(const FDCAN_HandleTypeDef *hfdcan, CAN_ErrReason_e reason)
{
    uint8_t i;

    for (i = 0; i < s_can_idx; i++)
    {
        CANInstance *inst = s_can_instance[i];

        if (inst->map.handle == hfdcan && inst->err_callback != NULL)
            inst->err_callback(inst, reason);
    }
}

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册CAN实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 CANConfig 负责）
 */
BSP_Status_e CANRegister(CANInstance *instance)
{

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    // 超实例数 / 重复注册都是**调用方用法错误**（不是硬件问题），与 USARTRegister/USBRegister 保持一致
    BSP_RETURN_IF_TRUE_LOG(s_can_idx >= CAN_INSTANCE_NUM, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        if (s_can_instance[i] == instance)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return BSP_PARAM_ERR;
        }
    }

    s_can_instance[s_can_idx++] = instance;

    BSPLOG(&g_can_log, LOG_LEVEL_INFO, "CAN Instance registered, idx=%d", s_can_idx - 1);
    return BSP_OK;
}

/**
 * @brief 配置CAN实例（填充硬件映射 + 工作模式 + 父指针 + 回调，可重复调用）
 * @note 要求先调用 CANRegister 注册实例。
 *       FDCAN 发送/接收机制跟随 CubeMX 配置：首次配置读 hfdcan->Init 自动使能对应中断，
 *       无需在软件里硬编码 FIFO/Buffer 数量。
 */
BSP_Status_e CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    FDCAN_HandleTypeDef *hfdcan;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    hfdcan = instance->map.handle;
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, BSP_HW_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "FDCAN handle is NULL, check bsp_map mapping!"));

    // 一个 handle 允许多个实例共享（如不同 ID 分组各占一个实例），无需防重
    BSP_RETURN_IF_TRUE_LOG(config->filter_num > 0 && config->filters == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "filters is NULL but filter_num=%d!", config->filter_num));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters; // 软件过滤器数组（Config 时写入，指向 config 中的数组）
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;
    instance->err_callback = config->err_callback;

#if defined(BSP_CAN_LIST_LUT_USED)
    // 标准 ID + LIST 模式 filter 直接登记查表槽位（增量注册/改 ID 天然支持；同 ID 后写覆盖前写）
    FDCAN_ListLutRegister(instance);
#endif

    // 模式与 CubeMX FrameFormat 兼容性检查：
    //  - 非 CLASSIC 实例需控制器启用 FD（CubeMX FrameFormat != CLASSIC，硬件 FDOE=1），否则 FD 帧发送失败
    //  - FD_BRS 实例还需控制器启用 BRS（CubeMX FrameFormat == FD_BRS，硬件 BRSE=1），否则数据段时序/8 Mbps 不生效
    if (instance->mode != CAN_FRAME_FORMAT_CLASSIC)
    {
        BSP_RETURN_IF_TRUE_LOG(hfdcan->Init.FrameFormat == FDCAN_FRAME_CLASSIC, BSP_PARAM_ERR,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "实例模式为 FD(mode=%d) 但 hfdcan FrameFormat=CLASSIC（CubeMX 配置），控制器未启用 FD!", instance->mode));
        BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_FD_BRS && hfdcan->Init.FrameFormat != FDCAN_FRAME_FD_BRS, BSP_PARAM_ERR,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "实例模式为 FD_BRS 但 hfdcan FrameFormat != FD_BRS（CubeMX 配置），控制器未启用 BRS，8 Mbps 数据段时序不会生效!"));
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
            BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_Stop(hfdcan) != HAL_OK, BSP_HW_ERR,
                                   BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "FDCAN Stop failed, can't retry init (can_e=%d)!", instance->can_e));
        }

        // 发送统一接口走 Tx FIFO（AddMessageToTxFifoQ + GetTxFifoFreeLevel），需 CubeMX 配置
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueElmtsNbr == 0, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "TxFifoQueueElmtsNbr=0! 统一接口发送走 Tx FIFO，请用 CubeMX 配置（TxFifoQueueElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(init->TxFifoQueueMode != FDCAN_TX_FIFO_OPERATION, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "TxFifoQueueMode 需为 FDCAN_TX_FIFO_OPERATION（只要 Tx FIFO，不要 Tx Queue）!"));

        // 硬过滤全通：全部走 FDCAN_ACCEPT_IN_RX_FIFO0（接收只保留 Rx FIFO 路径）
        BSP_RETURN_IF_TRUE_LOG(init->RxFifo0ElmtsNbr == 0, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "RxFifo0ElmtsNbr=0! 接收走 Rx FIFO0，请用 CubeMX 配置（RxFifo0ElmtsNbr>0）"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK,
                               BSP_HW_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_FDCAN_ConfigGlobalFilter failed!"));

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
                               BSP_HW_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_FDCAN_ConfigInterruptLines failed!"));

        // Rx FIFO0 新报文 + 满/丢失事件（满/丢失用于状态统计）
        active_it |= FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST;
        // 错误状态中断：Bus-off / Error Passive / Error Warning（HAL_FDCAN_ErrorStatusCallback 上报 + 恢复）
        active_it |= FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE;
        // Message RAM 访问失败（IR.IRA，IRQHandler 只置 ErrorCode 无回调，在 Rx/错误回调里采样统计）
        active_it |= FDCAN_IT_RAM_ACCESS_FAILURE;
        // 发送完成溯源：Tx Event FIFO + MessageMarker（需 CubeMX 配置 TxEventsNbr>0；FULL/LOST 用于检测溯源丢失）
        if (init->TxEventsNbr > 0)
            active_it |= FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST;

        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_Start(hfdcan) != HAL_OK, BSP_HW_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_FDCAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_FDCAN_ActivateNotification(hfdcan, active_it, 0) != HAL_OK, BSP_HW_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_FDCAN_ActivateNotification failed!"));

        // 全部初始化步骤成功后才置位：任一步失败返回，标志保持 0，下次 CANConfig 可完整重试
        s_fdcan_started[instance->can_e] = 1;
    }

    return BSP_OK;
}

/**
 * @brief CANTransmit 失败路径统一计数后返回状态码
 * @param instance CAN实例（可为 NULL：仅返回状态码）
 * @param status   要返回的状态码（BSP_BUSY / BSP_TIMEOUT / BSP_PARAM_ERR / BSP_HW_ERR）
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏只在条件成立时求值，
 *       故 NULL 的 instance 不会走到解引用），使所有失败返回点都统计进
 *       s_fdcan_status[].tx_fail 并分因计数，日志行为不变。
 */
static BSP_Status_e CAN_FdcanTxFail(const CANInstance *instance, BSP_Status_e status)
{
    if (instance != NULL && instance->can_e < CAN_NUM_MAX)
    {
        s_fdcan_status[instance->can_e].tx_fail++;
        if (status == BSP_BUSY)
            s_fdcan_status[instance->can_e].tx_busy++;
        else if (status == BSP_TIMEOUT)
            s_fdcan_status[instance->can_e].tx_timeout++;
    }
    return status;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param timeout_ms    发送资源等待超时（ms）：Tx FIFO 满时最多等待其空闲；传 0 表示不等待，满即失败。
 *                      中断上下文内恒按 0 处理（见 FDCAN_CanWait 判据）
 * @param tx_mailbox    出参：本次发送使用的发送标记（FDCAN=编码后的 MessageMarker，**不透明标记**，
 *                      只应与 tx_complete_callback 回传的值比对；对应 BxCAN 邮箱索引 0~2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余可发送元素数（Tx FIFO 空闲数）；可为 NULL
 * @retval BSP_OK        已加入 Tx FIFO（**不代表已发出**，逐帧结果看 tx_complete_callback）
 * @retval BSP_PARAM_ERR / BSP_BUSY / BSP_TIMEOUT / BSP_HW_ERR 见 bsp_can.h
 */
BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level)
{
    FDCAN_HandleTypeDef *hfdcan;
    FDCAN_TxHeaderTypeDef tx_header = {0};
    uint32_t marker = 0;
    uint8_t can_idx;
    uint8_t use_tx_event;
    int32_t dlc;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, CAN_FdcanTxFail(instance, BSP_PARAM_ERR), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    hfdcan = instance->map.handle;
    // handle 为 NULL = 该实例从未成功 CANConfig（映射由 CANConfig 填充），属调用方用错而非参数问题
    BSP_RETURN_IF_TRUE_LOG(hfdcan == NULL, CAN_FdcanTxFail(instance, BSP_HW_ERR), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "FDCAN handle is NULL (CANConfig not called or failed)!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, CAN_FdcanTxFail(instance, BSP_PARAM_ERR), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节，FD 最大 64 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 64, CAN_FdcanTxFail(instance, BSP_PARAM_ERR), BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Length %d exceeds FD max (64)!", pack->len));
    BSP_RETURN_IF_TRUE_LOG(instance->mode == CAN_FRAME_FORMAT_CLASSIC && pack->len > 8, CAN_FdcanTxFail(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Classic mode but len=%d exceeds 8!", pack->len));

    // TxElmtSize 越界硬拒绝：len 超过外设 Tx 元素尺寸时，HAL_FDCAN_AddMessageToTxFifoQ 内部
    // FDCAN_CopyMessageToRAM 会按 len 越界写 Message RAM（内存破坏），必须硬拒绝而非仅警告
    {
        uint8_t elmt_bytes = FDCAN_ElmtSizeToBytes(hfdcan->Init.TxElmtSize);
        BSP_RETURN_IF_TRUE_LOG(pack->len > elmt_bytes, CAN_FdcanTxFail(instance, BSP_PARAM_ERR),
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "len=%d exceeds FDCAN TxElmtSize=%d bytes!", pack->len, elmt_bytes));
    }

    // 帧类型与工作模式兼容性检查（FD 帧格式无 RTR 位：非经典模式拒绝远程帧）
    if (FDCAN_CheckFrameTypeCompatible(instance->mode, pack->frame_type) != 0)
        return CAN_FdcanTxFail(instance, BSP_PARAM_ERR);

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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Invalid frame_type=%d!", pack->frame_type);
        return CAN_FdcanTxFail(instance, BSP_PARAM_ERR);
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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Length %d exceeds FD max (64)!", pack->len);
        return CAN_FdcanTxFail(instance, BSP_PARAM_ERR);
    }
    tx_header.DataLength = (uint32_t)dlc;

    can_idx = instance->can_e;
    use_tx_event = (hfdcan->Init.TxEventsNbr > 0) ? 1 : 0; // Tx Event FIFO 使能才可溯源
    tx_header.TxEventFifoControl = use_tx_event ? FDCAN_STORE_TX_EVENTS : FDCAN_NO_TX_EVENTS;

    // 中断上下文里不允许等待：钳成"只试一次"。这不是防死锁（等待用 DWT 计时），
    // 而是防在 ISR 里空转——总线上其他实例的续发钩子就是在 CAN 中断里调本函数的。
    if (!FDCAN_CanWait())
    {
        if (timeout_ms != 0)
            s_fdcan_status[can_idx].tx_isr_clamp++;
        timeout_ms = 0;
    }

    // Tx FIFO 空闲等待：满则轮询等待其释放（timeout_ms 上限，0 表示不等待、满即失败）。
    // 无总线信号/对端离线时 FIFO 持续占满（帧发不出去），不能死等，超时返回失败
    if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0)
    {
        if (timeout_ms == 0)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "TX FIFO full!");
            return CAN_FdcanTxFail(instance, BSP_BUSY);
        }
        else
        {
            uint64_t start_time = DWT_GetTimeUs();
            uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

            while (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0)
            {
                if ((DWT_GetTimeUs() - start_time) > timeout_us)
                {
                    // 超时说明 FIFO 被"发不出去"的帧长期占着（无 ACK 时硬件会一直在重传），
                    // 那些帧既不会完成也不会报错。若只是返回 BSP_TIMEOUT，本次调用者还能重试，
                    // 但那几帧的发起者永远等不到结果、该 CAN 的发送资源也被永久占死。
                    // 故在此取消全部在途发送并逐帧通知各自的发起者（含回收 marker 槽）。
                    BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "FDCAN Tx FIFO timeout (can_e=%d, id=0x%lX), abort queued frames!",
                           instance->can_e, (unsigned long)pack->id);
                    (void)HAL_FDCAN_AbortTxRequest(hfdcan, 0xFFFFFFFFU);
                    if (use_tx_event)
                        (void)FDCAN_ReclaimMarkers(can_idx, 0);
                    return CAN_FdcanTxFail(instance, BSP_TIMEOUT);
                }
            }
        }
    }

    // 发送完成溯源：分配 MessageMarker 并先登记（入队后中断可能立即触发）
    if (use_tx_event)
    {
        if (FDCAN_AllocMarker(can_idx, instance, &marker) != 0)
        {
            // 32 个 marker 全被占 = 已入队但结果未收口的帧堆到上限（正常最多同时 32 帧在途）。
            // 这是"资源满、可重试"，不是硬件错误；但若长期如此说明槽位没被回收
            // （Tx Event 丢失是已知来源，见 FDCAN_ReclaimMarkers），值得看 s_fdcan_status[].tx_owner_busy。
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "All %d TX markers in flight (Tx Event FIFO busy)!", FDCAN_TX_MARKER_NUM);
            return CAN_FdcanTxFail(instance, BSP_BUSY);
        }
        tx_header.MessageMarker = marker;
    }

    // 加入发送 Tx FIFO
    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &tx_header, pack->data) != HAL_OK)
    {
        if (use_tx_event)
            FDCAN_FreeMarker(can_idx, marker); // 加入失败，释放 marker 槽（该帧没进硬件，不会有事件回来）
        // HAL 在这里有三种失败：① Tx FIFO 满（TXFQS.TFQF）；② Tx 缓冲/队列区没配（TXBC.TFQS=0）；
        // ③ 外设不在 BUSY（没 Start）。①是"资源满、可重试"，②③是配置/状态异常，语义不同。
        // 上面第 697 行的判空与本次入队之间可能被同总线的其他上下文（ISR 里的续发钩子）抢走
        // 最后一个元素，故不能直接按本函数的入参判 —— 再读一次空闲量来区分。
        if (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0U)
            return CAN_FdcanTxFail(instance, BSP_BUSY);
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_FDCAN_AddMessageToTxFifoQ failed!");
        return CAN_FdcanTxFail(instance, BSP_HW_ERR);
    }

    // 出参：发送标记（BxCAN=邮箱索引 / FDCAN=编码后的 MessageMarker）+ 剩余可发送数（顺带更新状态快照）
    if (tx_mailbox != NULL)
        *tx_mailbox = marker;
    s_fdcan_status[can_idx].tx_free = (uint8_t)HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);
    s_fdcan_status[can_idx].tx_owner_busy = FDCAN_OwnerBusyCount(can_idx);
    if (tx_free_level != NULL)
        *tx_free_level = s_fdcan_status[can_idx].tx_free;

    return BSP_OK;
}

/**
 * @brief 有界等待 Tx FIFO 腾出空间（任务上下文，只给 CANRecover 用）
 * @param hfdcan FDCAN 句柄
 * @return 最后一次读到的空闲元素数（0 = 等到超时仍满，由调用方判失败）
 * @note 取消在途帧（TXBCR）是硬件异步执行的，写完立刻回读 TXFQS 可能还是旧值 → 这里
 *       按 CAN_RECOVER_DRAIN_US 轮询，把这个窗口等掉。等待用 DWT 计时（不依赖 tick，
 *       与 CANTransmit 的等待循环同一套）。
 */
static uint32_t FDCAN_WaitTxFree(FDCAN_HandleTypeDef *hfdcan)
{
    uint64_t start = DWT_GetTimeUs();
    uint32_t free_level = HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);

    while (free_level == 0U && (DWT_GetTimeUs() - start) < (uint64_t)CAN_RECOVER_DRAIN_US)
        free_level = HAL_FDCAN_GetTxFifoFreeLevel(hfdcan);

    return free_level;
}

/**
 * @brief CAN 发送资源自恢复（任务上下文；五步顺序与理由见 bsp_can.h）
 * @note FDCAN 的 bus-off 恢复：ISR 里已清 CCCR.INIT 让硬件走恢复序列；这里做任务侧兜底 ——
 *       若外设仍停在 bus-off（外部总线条件不满足，如线没接回），停止并按原配置重启外设。
 *       HAL_FDCAN_Stop/Start 会清掉中断线映射与已激活的中断，必须一并重新配置。
 */
BSP_Status_e CANRecover(CANInstance *instance)
{
    FDCAN_HandleTypeDef *hfdcan;
    FDCAN_ProtocolStatusTypeDef protocol_status = {0};
    uint32_t tx_free;
    uint8_t can_idx;

    if (instance == NULL || instance->map.handle == NULL)
        return BSP_PARAM_ERR;
    can_idx = instance->can_e;
    hfdcan = instance->map.handle;
    if (can_idx >= CAN_NUM_MAX)
        return BSP_PARAM_ERR;

    if (!s_fdcan_started[can_idx])
    {
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CANRecover: can_e=%d not started, nothing to recover", can_idx);
        return BSP_BUSY;
    }

    // ①② 取消全部在途发送（TXBCR 整掩码 = 逐元素取消）并逐帧通知发起者。
    //     被成功取消的帧**不产生 Tx Event**，marker 槽不会被回调回收，故必须显式回收。
    (void)HAL_FDCAN_AbortTxRequest(hfdcan, 0xFFFFFFFFU);
    if (hfdcan->Init.TxEventsNbr > 0)
        (void)FDCAN_ReclaimMarkers(can_idx, 0);

    // ③ FDCAN 没有"清软件错误标志"的对应动作（HAL_FDCAN 的错误位是粘滞的路由标志），
    //    总线恢复靠硬件状态机：ISR 已清 CCCR.INIT，这里只回读状态确认
    (void)HAL_FDCAN_GetProtocolStatus(hfdcan, &protocol_status);

    // ④ 回读总线状态（tx_free 不在这里采 —— 它必须等重启分支走完，见 ⑤）
    s_fdcan_status[can_idx].bus_off = (uint8_t)protocol_status.BusOff;
    s_fdcan_status[can_idx].error_passive = (uint8_t)protocol_status.ErrorPassive;
    s_fdcan_status[can_idx].error_warning = (uint8_t)protocol_status.Warning;
    s_fdcan_status[can_idx].lec = (uint8_t)protocol_status.LastErrorCode;

    // 仍在 bus-off：总线物理层没恢复（线没接回 / 终端电阻 / 对端没上电）。
    // 兜底动作：停止并按原配置重启外设，让状态机从头走一遍（Stop 会清掉中断线映射与已激活的
    // 中断，所以必须重新 ConfigInterruptLines + ActivateNotification，否则恢复后一个中断都收不到）
    if (s_fdcan_status[can_idx].bus_off != 0U)
    {
        uint32_t active_it;
        uint32_t line_it;

        // 与 CANConfig 的首次配置保持一致：Tx Event 系列中断只在 TxEventsNbr>0 时才使能
        // （TxEventsNbr=0 时没有事件可弹，使能了不起作用，但保持两处一致便于对照排查）
        active_it = FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_RX_FIFO0_FULL | FDCAN_IT_RX_FIFO0_MESSAGE_LOST |
                    FDCAN_IT_BUS_OFF | FDCAN_IT_ERROR_WARNING | FDCAN_IT_ERROR_PASSIVE | FDCAN_IT_RAM_ACCESS_FAILURE;
        if (hfdcan->Init.TxEventsNbr > 0)
            active_it |= FDCAN_IT_TX_EVT_FIFO_NEW_DATA | FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST;
        line_it = active_it;

        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "FDCAN still bus-off (can_e=%d), restart peripheral", can_idx);
        if (HAL_FDCAN_Stop(hfdcan) != HAL_OK ||
            HAL_FDCAN_ConfigInterruptLines(hfdcan, line_it, FDCAN_INTERRUPT_LINE0) != HAL_OK ||
            HAL_FDCAN_Start(hfdcan) != HAL_OK ||
            HAL_FDCAN_ActivateNotification(hfdcan, active_it, 0) != HAL_OK)
        {
            s_fdcan_status[can_idx].recover_fail++;
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "CANRecover failed (can_e=%d): peripheral restart error", can_idx);
            return BSP_HW_ERR;
        }

        // 重启后总线仍未同步（BO 位是硬件在总线空闲足够久后才清），仍算本次未恢复
        (void)HAL_FDCAN_GetProtocolStatus(hfdcan, &protocol_status);
        if (protocol_status.BusOff != 0U)
        {
            s_fdcan_status[can_idx].recover_fail++;
            s_fdcan_status[can_idx].bus_off = 1;
            return BSP_HW_ERR;
        }
        s_fdcan_status[can_idx].bus_off = 0;
    }

    // ⑤ Tx FIFO 是否真的腾出来了。采样点必须在上面重启分支**之后**：
    //    HAL_FDCAN_Stop/Start 会把 TX FIFO 整个清掉，拿重启前的值判"仍满"会把一次成功的
    //    恢复误报成 recover_fail。取消请求本身是硬件异步执行的 → 由 FDCAN_WaitTxFree
    //    做一次有界轮询，等它落地再判。
    tx_free = FDCAN_WaitTxFree(hfdcan);
    s_fdcan_status[can_idx].tx_free = (uint8_t)tx_free;
    if (tx_free == 0U)
    {
        s_fdcan_status[can_idx].recover_fail++;
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CANRecover failed (can_e=%d): Tx FIFO still full", can_idx);
        return BSP_HW_ERR;
    }

    s_fdcan_status[can_idx].recover_ok++;
    BSPLOG(&g_can_log, LOG_LEVEL_INFO, "CANRecover ok (can_e=%d)", can_idx);
    return BSP_OK;
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
    uint8_t overflow = 0;
    uint8_t hw_err = 0;

    // RxFIFO0 满 / 丢报文事件（可能丢包）：先计数，FIFO 照常排空
    if (can_idx < CAN_NUM_MAX)
    {
        if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_FULL)
            s_fdcan_status[can_idx].rx_full++;
        if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_MESSAGE_LOST)
        {
            s_fdcan_status[can_idx].rx_lost++;
            overflow = 1;
        }
        hw_err = FDCAN_SampleRamAccessFail(can_idx, hfdcan);
    }

    // 纠正动作在先：先把 FIFO 里能救的帧全部读出来分发掉，再广播（err_callback 契约）
    FDCAN_ReceiveFifo(hfdcan, FDCAN_RX_FIFO0);

    if (overflow)
        FDCAN_NotifyError(hfdcan, CAN_ERR_RX_OVERFLOW);
    if (hw_err)
        FDCAN_NotifyError(hfdcan, CAN_ERR_HW);
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
        s_fdcan_status[can_idx].last_err_us = DWT_GetTimeUs();
        // 错误事件数也在此采样：bus-off 期间无收帧，仅靠 Rx 回调采样会让 err_event 停滞
        s_fdcan_status[can_idx].err_event += (uint8_t)((uint8_t)error_counters.ErrorLogging - s_fdcan_hw_err_log[can_idx]);
        s_fdcan_hw_err_log[can_idx] = (uint8_t)error_counters.ErrorLogging;

        if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
            s_fdcan_status[can_idx].err_bus_off++;
        if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
            s_fdcan_status[can_idx].err_passive++;
        if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
            s_fdcan_status[can_idx].err_warning++;
    }

    // Bus-off 恢复：清 INIT 位请求离开初始化模式，硬件自动执行 Bus-off 恢复序列。
    // 这里**只做这一件寄存器级动作**（ISR 里不丢帧、不重启外设）；若总线条件一直不满足
    // （线没接回），任务侧的 CANRecover 会再兜一层。
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0U && protocol_status.BusOff != 0U)
    {
        CLEAR_BIT(hfdcan->Instance->CCCR, FDCAN_CCCR_INIT);
    }

    if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "FDCAN Bus-off! (can_e=%d)", can_idx);
    if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "FDCAN Error Passive! (can_e=%d)", can_idx);
    if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "FDCAN Error Warning! (can_e=%d)", can_idx);

    // 纠正动作做完后再广播给上层（err_callback 契约：分类计数 → 快照 → 纠正 → 通知）。
    // FDCAN 没有 LEC 中断源，协议错误不进 err_callback，靠上面三级表达。
    if (ErrorStatusITs & FDCAN_IT_BUS_OFF)
        FDCAN_NotifyError(hfdcan, CAN_ERR_BUS_OFF);
    if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE)
        FDCAN_NotifyError(hfdcan, CAN_ERR_ERROR_PASSIVE);
    if (ErrorStatusITs & FDCAN_IT_ERROR_WARNING)
        FDCAN_NotifyError(hfdcan, CAN_ERR_ERROR_WARNING);
    if (FDCAN_SampleRamAccessFail(can_idx, hfdcan))
        FDCAN_NotifyError(hfdcan, CAN_ERR_HW);
}

/*------------- 发送完成回调（Tx Event FIFO + MessageMarker 溯源，由 CANConfig 激活 IT） --------------*/

/**
 * @brief Tx Event FIFO 中断回调：弹出事件并按 MessageMarker 溯源到实例
 * @note 事件类型（ET）两种取值：
 *       - FDCAN_TX_EVENT              正常发出
 *       - FDCAN_TX_IN_SPITE_OF_ABORT  取消请求迟到、帧仍然发出（等于取消失败）
 *       两者都算"已发出"→ BSP_OK；只把后者单独计数，它是取消动作没抢过硬件的直接证据。
 */
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxEventFifoITs)
{
    uint8_t can_idx = FDCAN_HcanToIndex(hfdcan);
    FDCAN_TxEventFifoTypeDef event;
    uint32_t fill;

    if (can_idx >= CAN_NUM_MAX)
        return;

    // 逐个弹出事件并分发（先把还能溯源的都收口掉，剩下的才需要回收）
    fill = (hfdcan->Instance->TXEFS & FDCAN_TXEFS_EFFL) >> FDCAN_TXEFS_EFFL_Pos;
    while (fill > 0)
    {
        if (HAL_FDCAN_GetTxEvent(hfdcan, &event) != HAL_OK)
            break;
        if (event.EventType == FDCAN_TX_IN_SPITE_OF_ABORT)
            s_fdcan_status[can_idx].tx_abort_late++;
        // Marker 是"代际<<5 | 槽位"，代际不符的（回收后迟到的旧事件）由 FDCAN_TxResultHandler 丢弃。
        // 这里不再无条件 tx_ok++：计数统一在收口函数里（回收路径要以 BSP_HW_ERR 计）
        FDCAN_TxResultHandler(can_idx, event.MessageMarker, BSP_OK);
        fill--;
    }

    // 事件 FIFO 满/丢失：有事件被硬件丢掉，对应的 marker 槽永远不会被弹出的事件回收，
    // 会永久占用 —— 攒够 32 个该实例就再也发不出帧，且没有任何回调通知上层，是永久性死锁。
    // 故把"弹完事件后仍占着的槽"当作溯源丢失、统一按失败上报并放掉（无法分辨具体是哪些帧丢的，
    // 最多 32 帧由上层重发；接收端若按序号去重可自动消掉重复）。
    if (TxEventFifoITs & (FDCAN_IT_TX_EVT_FIFO_FULL | FDCAN_IT_TX_EVT_FIFO_ELT_LOST))
    {
        uint8_t n;
        uint8_t hw_err;

        s_fdcan_status[can_idx].tx_event_lost++;
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Tx Event FIFO full/lost! 发送完成溯源丢失，回收 marker 槽");
        n = FDCAN_ReclaimMarkers(can_idx, 1);
        hw_err = FDCAN_SampleRamAccessFail(can_idx, hfdcan);
        if (n > 0 || hw_err)
            FDCAN_NotifyError(hfdcan, CAN_ERR_HW);
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_FDCAN */

#endif /* BSP_CAN_USED */
