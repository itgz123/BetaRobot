/**
 * @file bsp_bxcan.c
 * @brief BxCAN驱动封装实现（F4 平台，经典 CAN）
 *
 * @note 与 bsp_fdcan.c 共用统一接口（CANRegister / CANConfig / CANTransmit / CANRecover）：
 *       这里额外负责软件过滤分发（含可选 LIST 查表加速）、发送溯源与逐帧结果收口、
 *       错误分类统计与总线级错误广播。
 *       本文件仅在 BSP_CAN_IP == BSP_CAN_IP_BXCAN 时编译。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if BSP_CAN_IP == BSP_CAN_IP_BXCAN

#include "bsp_assert.h"
#include "bsp_log.h"
#include "bsp_dwt.h"

/*------------- 私有变量 --------------*/
#ifndef BSP_BXCAN_LOG_LIMIT
#define BSP_BXCAN_LOG_LIMIT 10
#endif                                                         // !BSP_BXCAN_LOG_LIMIT
LOG_INSTANCE_DEF(g_can_log, "bsp_bxcan", BSP_BXCAN_LOG_LIMIT); // CAN 日志实例
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

#if defined(BSP_CAN_LIST_LUT_USED)
/*------------- 接收 LIST 查表加速（标准 ID 直接索引，省双重循环线性扫描） -------------
 * s_bxcan_list_lut[CAN][ID] = 该标准 ID 的 LIST filter 所属实例（CANConfig 时登记）
 * 分发时按 ID 直接索引命中实例（再扫该实例 filters 精确匹配），替代全 CAN 线性扫描。
 * 一个 (CAN, ID) 只存一个实例指针：同 ID 多实例注册时后写覆盖前写（后注册实例优先）。
 */
static CANInstance *s_bxcan_list_lut[CAN_NUM_MAX][0x800]; /* 标准 ID(0~0x7FF) → 实例指针 */

/* 调试辅助：该 CAN 是否已有 LIST 标准帧查表槽位被登记（1=被覆盖过，调试器直接 Watch） */
volatile uint8_t s_bxcan_list_lut_used[CAN_NUM_MAX] = {0};

/**
 * @brief 判断 filter 是否可被 LIST 查表覆盖（LIST 模式 + 标准帧类型 + 至少一个 ID 在 0~0x7FF）
 * @note 与 CAN_FilterMatch 的 LIST 分支对齐：标准帧 pack->id <= 0x7FF，ID >0x7FF 视为 unused
 *       （永远匹配不到标准帧，不落槽）；frame_type 为标准数据/远程帧。
 */
static uint8_t CAN_ListLutCoverable(const CAN_Filter_s *f)
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
static void CAN_ListLutRegister(CANInstance *inst)
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
        /* 登记槽位并标记该 CAN 查表已被覆盖（调试直接 Watch s_bxcan_list_lut_used） */
        s_bxcan_list_lut_used[ci] = 1;
        if ((f->id0 == f->id1) || (f->id1 == CAN_ID_UNUSED))
            s_bxcan_list_lut[ci][(uint16_t)f->id0] = inst; /* 单 ID：只落 id0 一槽 */
        else
        {
            s_bxcan_list_lut[ci][(uint16_t)f->id0] = inst; /* 双 ID：id0/id1 各落一槽 */
            s_bxcan_list_lut[ci][(uint16_t)f->id1] = inst;
        }
    }
}
#endif

/*------------- CAN 外设状态/错误统计（调试用，调试器直接 Watch s_bxcan_status） --------------*/

/**
 * @brief BxCAN 外设状态与错误统计（每 CAN 一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：错误回调（①）、RxFIFO0/1 FULL 与 overrun（②）、CANTransmit 的失败返回（③）、
 *       发送完成/失败收口，外加便于"判断当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;        /* 发送完成次数（帧真正发到总线上） */
    uint32_t tx_fail;      /* CANTransmit 返回非 BSP_OK 次数 */
    uint32_t tx_busy;      /* └ 其中"资源满且不等待"（BSP_BUSY）的次数——正常拥塞，不是故障 */
    uint32_t tx_timeout;   /* └ 其中等邮箱耗尽 timeout_ms（BSP_TIMEOUT）的次数——总线可能已瘫 */
    uint32_t tx_isr_clamp; /* 中断上下文里被钳成"不等待"的发送次数（见 CANTransmit 入口） */
    uint32_t tx_abort;     /* 被 CANAbortAllTx / CANRecover 主动取消的在途帧数 */
    uint32_t tx_drain;     /* 任务上下文同步分发的完成事件数（消除邮箱复用竞态；见 CAN_TxDrainCompletions） */
    uint32_t rx_ok;        /* 成功收帧次数 */
    uint32_t rx_full;      /* RxFIFO0/1 FULL 事件次数 */
    uint32_t rx_lost;      /* RxFIFO0/1 overrun 丢帧次数（FOV0/FOV1） */
    /* 错误计数 */
    uint32_t err_bus_off;  /* bus-off 进入次数 */
    uint32_t err_passive;  /* error passive 进入次数 */
    uint32_t err_warning;  /* error warning 进入次数 */
    uint32_t err_protocol; /* 协议错误次数（LEC: STF/FOR/ACK/BR/BD/CRC） */
    uint32_t err_tx;       /* 发送失败帧数（ALST/TERR，仲裁失败或发送错误） */
    /* 恢复计数（CANRecover） */
    uint32_t recover_ok;   /* 恢复后总线可用次数 */
    uint32_t recover_fail; /* 恢复后仍 bus-off / 邮箱仍占满的次数 */
    /* 实时状态快照（最近一次错误回调或 CANRecover 采样） */
    uint8_t bus_off;       /* ESR.BOFF */
    uint8_t error_passive; /* ESR.EPVF */
    uint8_t error_warning; /* ESR.EWGF */
    uint8_t lec;           /* ESR.LEC（上一次错误码） */
    uint8_t tec;           /* ESR.TEC bit16-23 */
    uint8_t rec;           /* ESR.REC bit24-31 */
    uint8_t tx_free;       /* 空闲邮箱数（发送后采样） */
    uint8_t tx_owner_busy; /* 已入队但结果尚未收口的帧数（正常 ≤ 3，总线安静时应归零；长期不归零=溯源槽泄漏） */
    uint8_t rx_fifo0_fill; /* RF0R.FMP */
    uint8_t rx_fifo1_fill; /* RF1R.FMP */
    uint64_t last_err_us;  /* 最近一次错误回调的 DWT 时间戳（us），用于判断错误是否已经停止 */
} CAN_BxcanStatus_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile CAN_BxcanStatus_s s_bxcan_status[CAN_NUM_MAX];

/*------------- 私有常量：邮箱位掩码与中断上下文判定 --------------*/

/* TSR 中三个邮箱的“请求完成”/“发送成功”位（bxCAN 不连续：bit0/8/16） */
static const uint32_t s_can_rqcp_mask[CAN_TX_MAILBOX_NUM] = {CAN_TSR_RQCP0, CAN_TSR_RQCP1, CAN_TSR_RQCP2};
static const uint32_t s_can_txok_mask[CAN_TX_MAILBOX_NUM] = {CAN_TSR_TXOK0, CAN_TSR_TXOK1, CAN_TSR_TXOK2};

/* 发送失败（仲裁丢失 / 发送错误）错误码位，下标即邮箱号 0/1/2。
 * HAL 在 IRQHandler 里已清掉 RQCPx，邮箱号只能从错误码位反推，
 * 见 HAL_CAN_ErrorCallback 的"逐帧结果收口"。 */
static const uint32_t s_can_tx_fail_mask[CAN_TX_MAILBOX_NUM] = {HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_TERR0,
                                                                HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_TERR1,
                                                                HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR2};

/**
 * @brief 当前是否处于可阻塞等待的上下文（任务上下文且非临界区）
 * @note 判据与 USART_CanBlockingAbort / SPI_CanBlockingAbort 一致。
 *       这里挡的**不是死锁**——本模块的等待用 DWT 计时，不依赖 tick，ISR 里等不会卡死；
 *       挡的是"在中断里空转"：资源满（总线无 ACK、帧发不出去）时等待循环必然跑满 timeout_ms，
 *       而 comm media 的续发钩子正是在 CAN 中断里以 timeout_ms=1 调 CANTransmit ——
 *       那就是每次 TxComplete 在 ISR 内空转 1ms，压住所有优先级更低的中断与任务。
 */
static uint8_t CAN_CanWait(void)
{
    return ((__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U)) ? 1U : 0U;
}

/*------------- 私有函数：发送溯源与逐帧结果收口 --------------*/

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
 * @brief 采样"已入队但结果尚未收口"的帧数（溯源槽泄漏指示）
 * @note 正常应 ≤ CAN_TX_MAILBOX_NUM，总线安静时归零；长期不归零说明有帧的溯源槽没被回收。
 */
static uint8_t CAN_OwnerBusyCount(uint8_t can_idx)
{
    uint8_t i, n = 0;

    for (i = 0; i < CAN_TX_MAILBOX_NUM; i++)
    {
        if (s_can_tx_owner[can_idx][i] != NULL)
            n++;
    }
    return n;
}

/**
 * @brief 原子摘取某个邮箱的溯源槽（"读 owner → 置 NULL"不可分）
 * @return 槽里原来的 owner；槽本就没人认领时返回 NULL
 *
 * @note 为什么必须原子：**同一个邮箱的完成事件会被多条路径同时观察到** —— HAL 的 TX 邮箱
 *       中断（TXOK/取消）、SCE 错误中断（ALST/TERR），以及任务上下文的
 *       CAN_TxDrainCompletions 与 CANAbortAllTx。它们不在同一优先级：drain 只屏蔽了
 *       TX 邮箱中断（管不住 SCE），CANAbortAllTx 什么都不屏蔽，两者又都会一次处理多个邮箱。
 *       "读→清"之间被抢占，两条路径就都会拿到同一个非空 owner 并各回调一次。
 *       头文件把 tx_complete_callback 定为幂等，重复通知不至于出错，但"同一帧的结果只通知
 *       一次"是这条溯源机制的基本承诺，不该靠上层兜底 —— 代价只有一次 4 条指令的临界区，
 *       且临界区里不做任何回调。
 */
static CANInstance *CAN_TakeOwner(uint8_t can_idx, uint8_t mailbox_idx)
{
    CANInstance *inst;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    inst = s_can_tx_owner[can_idx][mailbox_idx];
    s_can_tx_owner[can_idx][mailbox_idx] = NULL;
    if (primask == 0U)
        __enable_irq();

    return inst;
}

/**
 * @brief 逐帧结果收口：查溯源表 → 清槽 → 回调所属实例（成功与失败共用一条路径）
 * @param hcan        硬件句柄
 * @param mailbox_idx 邮箱索引（0/1/2）
 * @param result      BSP_OK=帧已真正发出；BSP_HW_ERR=仲裁失败/发送错误/被取消
 * @note **先清槽再回调**：回调内可能立即再次发送并复用同一邮箱。
 *       槽为空（该邮箱的发送不是经本层发起的，或已由另一条路径收口）时只计数、不回调。
 *       多条路径并发时由 CAN_TakeOwner 的原子摘取保证"最多一条拿到非空槽"。
 */
static void CAN_TxResultHandler(CAN_HandleTypeDef *hcan, uint8_t mailbox_idx, BSP_Status_e result)
{
    uint8_t can_idx = CAN_HcanToIndex(hcan);
    CANInstance *inst;

    if (can_idx >= CAN_NUM_MAX || mailbox_idx >= CAN_TX_MAILBOX_NUM)
        return;

    if (result == BSP_OK)
        s_bxcan_status[can_idx].tx_ok++;
    else
        s_bxcan_status[can_idx].err_tx++; /* 帧没能发出去 */

    inst = CAN_TakeOwner(can_idx, mailbox_idx);
    s_bxcan_status[can_idx].tx_owner_busy = CAN_OwnerBusyCount(can_idx);
    if (inst != NULL && inst->tx_complete_callback != NULL)
        inst->tx_complete_callback(inst, mailbox_idx, result);
}

/**
 * @brief 取消全部在途发送，并逐帧把结果通知给发起者（BSP_HW_ERR）
 * @param instance CAN实例（作用于其所在的整个 CAN 外设）
 * @retval HAL_CAN_AbortTxRequest 的返回（HAL_OK = 取消请求已受理）
 *
 * @note **必须显式通知**：被成功取消的帧（ABRQ 生效、帧未发出）在 HAL 里走
 *       TxMailbox*AbortCallback，但那条路径依赖 TX 邮箱中断使能、且只覆盖
 *       "RQCP 置位而 TXOK/ALST/TERR 均未置"这一种情形，不能把 owner 的收尾押在它上面。
 *       这里先清槽再回调，故与 HAL 那条路径不会重复通知（谁先到谁通知，后到的查到空槽跳过）。
 * @note 代价：在途帧被放弃，上层会看到它们以 BSP_HW_ERR 失败并按各自协议重发，
 *       即"用丢若干帧换回发送能力"。**不得在 ISR 里调用**（会丢帧且回调可能很长）。
 */
static HAL_StatusTypeDef CANAbortAllTx(CANInstance *instance)
{
    CAN_HandleTypeDef *hcan = instance->map.handle;
    uint8_t can_idx = instance->can_e;
    HAL_StatusTypeDef ret;
    uint8_t i;

    if (hcan == NULL || can_idx >= CAN_NUM_MAX)
        return HAL_ERROR;

    /* HAL_CAN_AbortTxRequest 只置 TSR 的 ABRQ 位，无任何 tick 依赖的自旋，故不受
     * CAN_CanWait 门禁约束；但它要求外设处于 READY/LISTENING，否则返回 HAL_ERROR */
    ret = HAL_CAN_AbortTxRequest(hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);

    for (i = 0; i < CAN_TX_MAILBOX_NUM; i++)
    {
        /* 原子摘取：ACRQ 生效后 HAL 的 TxMailbox*AbortCallback / SCE 错误中断可能同时
         * 观察到同一个邮箱，非原子摘取会让同一帧被回调两次（见 CAN_TakeOwner） */
        CANInstance *owner = CAN_TakeOwner(can_idx, i);

        if (owner != NULL)
        {
            s_bxcan_status[can_idx].tx_abort++;
            if (owner->tx_complete_callback != NULL)
                owner->tx_complete_callback(owner, i, BSP_HW_ERR);
        }
    }
    s_bxcan_status[can_idx].tx_owner_busy = 0;

    return ret;
}

/**
 * @brief 向该 CAN 上所有注册了 err_callback 的实例广播一个错误原因
 * @note 错误是**外设级**的（同一 handle 上可以有多个实例共享同一条总线），无法归给某一个
 *       实例，故广播给该 handle 上的所有注册者。handler 契约要求幂等，重复收到同一原因无害；
 *       没配 err_callback 的实例直接跳过（本仓库的 drv 层都已接上，见各 drvs_* 的 ErrHook）。
 */
static void CAN_NotifyError(const CAN_HandleTypeDef *hcan, CAN_ErrReason_e reason)
{
    uint8_t i;

    for (i = 0; i < s_can_idx; i++)
    {
        CANInstance *inst = s_can_instance[i];

        if (inst->map.handle == hcan && inst->err_callback != NULL)
            inst->err_callback(inst, reason);
    }
}

/* 分发进行中标志：分发会回调上层（可能再触发 CANTransmit）；嵌套调用直接跳过，
 * 避免同一 media 的回调被重入（外层未处理完时内层又推进 tx_sent 造成分包错乱）。
 * ISR 可能抢占任务上下文看到 1，故 volatile。 */
static volatile uint8_t s_can_tx_draining = 0;

/**
 * @brief 同步分发 TSR 中尚未处理的发送完成事件（消除邮箱复用竞态）
 * @param instance 即将发送的 CAN 实例
 * @note bxCAN 发送完成本由 HAL 在 CAN 中断内依 TSR.RQCPx 分发；但向邮箱写 TXRQ 会清掉
 *       RQCPx，而 HAL 进中断时只读一次 TSR。若任务上下文（总线上其他实例，如共用 CAN1 的
 *       电机）在中断处理前复用了刚发完的邮箱，该完成事件即被吞掉——依赖它续发的异步分包
 *       （comm media idseq/pkt0）会永久卡在 tx_active 造成单向静默。
 *       故入队前先屏蔽 TX 邮箱中断、把未处理的完成事件同步分发掉再恢复中断，从根上消除竞态。
 *       屏蔽期间新到的完成事件因 bxCAN 中断是 TSR 电平触发，恢复 IER 后会补触发，不会丢。
 * @note 屏蔽的是 IER.TX_MAILBOX_EMPTY 这一位，而 HAL 的整个发送分支（完成 / 中止 /
 *       ALST·TERR 错误码）都受该位门控（CANx_TX 与 CANx_SCE 两个向量进的是同一个
 *       HAL_CAN_IRQHandler），所以屏蔽期间不会与中断并发处理同一邮箱。
 */
static void CAN_TxDrainCompletions(CANInstance *instance)
{
    CAN_HandleTypeDef *hcan = instance->map.handle;
    uint32_t ier, tsr;
    uint8_t i;

    if (hcan == NULL || s_can_tx_draining)
        return; /* 嵌套调用：外层正在分发，跳过（否则将重入同一 media 回调） */

    s_can_tx_draining = 1;
    ier = hcan->Instance->IER;
    hcan->Instance->IER = ier & ~CAN_IT_TX_MAILBOX_EMPTY; /* 屏蔽，避免 HAL 中断并发处理同一事件 */
    tsr = hcan->Instance->TSR;

    for (i = 0; i < CAN_TX_MAILBOX_NUM; i++)
    {
        if ((tsr & s_can_rqcp_mask[i]) == 0U)
            continue;

        hcan->Instance->TSR = s_can_rqcp_mask[i]; /* 写 1 清 RQCPx（同时清 TXOK/ALST/TERR） */
        if ((tsr & s_can_txok_mask[i]) != 0U)
        {
            CAN_TxResultHandler(hcan, i, BSP_OK); /* 发送成功 */
            s_bxcan_status[instance->can_e].tx_drain++;
        }
        else
        {
            /* 仲裁失败/发送错误：与 HAL 走 AbortCallback / ErrorCallback 是同一件事
             * （RQCP 置位而 TXOK 未置）。也必须把逐帧结果通知到发起者，否则在这个
             * 屏蔽窗口内失败的帧会永远等不到结果。 */
            CAN_TxResultHandler(hcan, i, BSP_HW_ERR);
        }
    }

    hcan->Instance->IER = ier; /* 恢复中断使能 */
    s_can_tx_draining = 0;
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
    BSP_RETURN_IF_TRUE_LOG(s_can_idx >= CAN_INSTANCE_NUM, BSP_PARAM_ERR,
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

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
 * @note 要求先调用 CANRegister 注册实例
 * @note **配置项一律覆盖写入，包括两个回调**：`config->tx_complete_callback`/`err_callback`
 *       传 NULL 的含义是"清空"，不是"保持原样"。所以**同一个实例别配两遍**：第二次若不带
 *       回调，第一次注册的就被静默清掉，异步分包会卡在"永远等不到发送完成"。
 *       多个实例共享同一 handle（见上面 CANRegister 的说明）则互不影响 —— 两个回调都
 *       按实例存放：发送完成回调只发给**发起那帧的实例**（CAN_TxResultHandler 按
 *       CAN_TakeOwner 查到谁发的那帧），错误回调才按 handle **广播**给该总线上的每个实例
 *       （CAN_NotifyError 逐个调各自那份，某个实例没挂就跳过它）。
 */
BSP_Status_e CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR, BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, BSP_PARAM_ERR,
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, BSP_HW_ERR,
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "CAN handle is NULL, check bsp_map mapping!"));

    // 一个 handle 允许多个实例共享（如不同 ID 分组各占一个实例），无需防重
    BSP_RETURN_IF_TRUE_LOG(
        config->filter_num > 0 && config->filters == NULL, BSP_PARAM_ERR,
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "filters is NULL but filter_num=%d!", config->filter_num));

    // F4 BxCAN 仅支持经典 CAN（FD 帧需 H7 FDCAN），非 CLASSIC 一律拒绝
    BSP_RETURN_IF_TRUE_LOG(
        config->mode != CAN_FRAME_FORMAT_CLASSIC, BSP_PARAM_ERR,
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "BxCAN only supports CLASSIC frame format (mode=%d)!", config->mode));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters; // 软件过滤器数组（Config 时写入，指向 config 中的数组）
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;
    instance->err_callback = config->err_callback;

#if defined(BSP_CAN_LIST_LUT_USED)
    // 标准 ID + LIST 模式 filter 直接登记查表槽位（增量注册/改 ID 天然支持；同 ID 后写覆盖前写）
    CAN_ListLutRegister(instance);
#endif

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
            BSP_RETURN_IF_TRUE_LOG(
                HAL_CAN_Stop(instance->map.handle) != HAL_OK, BSP_HW_ERR,
                BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "CAN Stop failed, can't retry init (can_e=%d)!", instance->can_e));
        }

        hw_filter.FilterIdHigh = 0;
        hw_filter.FilterIdLow = 0;
        hw_filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 掩码模式
        hw_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 32位
        hw_filter.FilterMaskIdHigh = 0;
        hw_filter.FilterMaskIdLow = 0; // 掩码全 0 = 全通过
        hw_filter.FilterActivation = ENABLE;
        hw_filter.SlaveStartFilterBank = 14; // F4 双 CAN 共享 28 filter bank：CAN1(主)占 0..13，CAN2(从)占 14..27
        // 漏设此字段会被 HAL_CAN_ConfigFilter 写成 0，使 CAN2 失去从 bank 的过滤能力 → CAN2 收不到任何帧

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
        it_mask |= CAN_IT_RX_FIFO0_FULL | CAN_IT_RX_FIFO0_OVERRUN | CAN_IT_RX_FIFO1_FULL | CAN_IT_RX_FIFO1_OVERRUN;
        // 错误状态中断：Error Warning / Error Passive / Bus-off / Last Error Code（HAL_CAN_ErrorCallback 上报）
        // 注意：必须同时使能 CAN_IT_ERROR（IER.ERRIE 主开关），否则 IRQHandler 的错误分支不执行
        it_mask |= CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE | CAN_IT_ERROR;

        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ConfigFilter(instance->map.handle, &hw_filter) != HAL_OK, BSP_HW_ERR,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_CAN_ConfigFilter failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_Start(instance->map.handle) != HAL_OK, BSP_HW_ERR,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_CAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ActivateNotification(instance->map.handle, it_mask) != HAL_OK, BSP_HW_ERR,
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_CAN_ActivateNotification failed!"));

        // 全部初始化步骤成功后才置位：任一步失败返回，标志保持 0，下次 CANConfig 可完整重试
        s_can_started[instance->can_e] = 1;
    }

    return BSP_OK;
}

/**
 * @brief CANTransmit 失败路径统一计数后返回状态码
 * @param instance CAN实例（可为 NULL：仅返回状态码）
 * @param status   要返回的状态码（BSP_BUSY / BSP_TIMEOUT / BSP_PARAM_ERR / BSP_HW_ERR）
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏只在条件成立时求值，
 *       故 NULL 的 instance 不会走到解引用），使所有失败返回点都统计进
 *       s_bxcan_status[].tx_fail 并分因计数，日志行为不变。
 */
static BSP_Status_e CAN_BxcanTxFail(const CANInstance *instance, BSP_Status_e status)
{
    if (instance != NULL && instance->can_e < CAN_NUM_MAX)
    {
        s_bxcan_status[instance->can_e].tx_fail++;
        if (status == BSP_BUSY)
            s_bxcan_status[instance->can_e].tx_busy++;
        else if (status == BSP_TIMEOUT)
            s_bxcan_status[instance->can_e].tx_timeout++;
    }
    return status;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param timeout_ms    发送资源等待超时（ms）：三个邮箱全满时最多等待其空闲；传 0 表示不等待，满即失败。
 *                      中断上下文内恒按 0 处理（见下方 CAN_CanWait 判据）
 * @param tx_mailbox    出参：本次发送使用的邮箱索引（0/1/2，对应 HAL CAN_TX_MAILBOX0/1/2）；可为 NULL
 * @param tx_free_level 出参：发送后剩余空闲邮箱数（0~CAN_TX_MAILBOX_NUM）；可为 NULL
 * @retval BSP_OK        已加入邮箱（**不代表已发出**，逐帧结果看 tx_complete_callback）
 * @retval BSP_PARAM_ERR / BSP_BUSY / BSP_TIMEOUT / BSP_HW_ERR 见 bsp_can.h
 */
BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox,
                         uint8_t *tx_free_level)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t mailbox;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, CAN_BxcanTxFail(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    // handle 为 NULL = 该实例从未成功 CANConfig（映射由 CANConfig 填充），属调用方用错而非参数问题
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, CAN_BxcanTxFail(instance, BSP_HW_ERR),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "CAN handle is NULL (CANConfig not called or failed)!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, CAN_BxcanTxFail(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 8, CAN_BxcanTxFail(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Length %d exceeds classic CAN max (8)!", pack->len));

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
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Invalid frame_type=%d!", pack->frame_type);
        return CAN_BxcanTxFail(instance, BSP_PARAM_ERR);
    }
    /* ID 范围校验（按声明的帧类型）：HAL 只在 USE_FULL_ASSERT 下才查 ID（本工程各板都没开
     * USE_FULL_ASSERT，见 stm32f4xx_hal_conf.h），而 TIR 里标准 ID 只占 11 位
     * （`StdId << CAN_TI0R_STID_Pos`）—— 超范围的标准帧 ID 会被静默截断（0x800 移出 32 位，
     * 还可能与 EXID 字段互相污染），发出去的是**另一个 ID** 的帧。现场表现最坑：发送路径
     * 一路成功（`tx_ok` 照涨、没有错误位），接收端却因过滤器不匹配一帧都收不到。
     * 宁可在这里硬拒成参数错。 */
    if (tx_header.IDE == CAN_ID_STD)
    {
        BSP_RETURN_IF_TRUE_LOG(
            pack->id > 0x7FFu, CAN_BxcanTxFail(instance, BSP_PARAM_ERR),
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Standard frame id=0x%lX exceeds 0x7FF!", (unsigned long)pack->id));
    }
    else
    {
        BSP_RETURN_IF_TRUE_LOG(pack->id > 0x1FFFFFFFu, CAN_BxcanTxFail(instance, BSP_PARAM_ERR),
                               BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "Extended frame id=0x%lX exceeds 0x1FFFFFFF!",
                                      (unsigned long)pack->id));
    }

    tx_header.DLC = pack->len;

    // 入队前先同步分发未处理的发送完成事件：防止本函数复用刚发完的邮箱时写 TXRQ 清掉 RQCPx，
    // 把 HAL 本该在中断里分发的完成回调吞掉（异步分包发送会因此永久卡死）。必须在下面的
    // 空闲邮箱判断之前做——分发可能触发续发、改变邮箱占用情况。
    CAN_TxDrainCompletions(instance);

    // 中断上下文里不允许等待：钳成"只试一次"。这不是防死锁（等待用 DWT 计时），
    // 而是防在 ISR 里空转——总线上其他实例的续发钩子就是在 CAN 中断里调本函数的。
    if (!CAN_CanWait())
    {
        if (timeout_ms != 0)
            s_bxcan_status[instance->can_e].tx_isr_clamp++;
        timeout_ms = 0;
    }

    // 邮箱空闲等待：三个发送邮箱全满则轮询等待其释放（timeout_ms 上限，0 表示不等待、满即失败）。
    // 无总线信号/对端离线时邮箱持续被占满（帧发不出去），不能死等，超时返回失败
    if (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0)
    {
        if (timeout_ms == 0)
        {
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "TX mailboxes full!");
            return CAN_BxcanTxFail(instance, BSP_BUSY);
        }
        else
        {
            uint64_t start_time = DWT_GetTimeUs();
            uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

            while (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0)
            {
                if ((DWT_GetTimeUs() - start_time) > timeout_us)
                {
                    // 超时说明邮箱被"发不出去"的帧长期占着：bxCAN 在无 ACK 时会无限重传，
                    // 那些帧既不会完成也不会报错，没有任何回调会来。若只是返回 BSP_TIMEOUT：
                    //   ① 本次调用者还能重试，但那几帧的发起者永远等不到结果（异步分包卡死在 tx_active）；
                    //   ② 该 CAN 的发送资源被永久占死，任何实例都再也发不出帧。
                    // 故在此主动取消它们并逐帧通知各自的发起者。代价是放弃若干帧（上层按各自协议重发），
                    // 即"用丢帧换回发送能力"；超时设得过短时，健康但拥塞的总线也可能命中。
                    BSPLOG(&g_can_log, LOG_LEVEL_WARNING,
                           "CAN TX mailbox timeout (can_e=%d, id=0x%lX), abort queued frames!", instance->can_e,
                           (unsigned long)pack->id);
                    (void)CANAbortAllTx(instance);
                    return CAN_BxcanTxFail(instance, BSP_TIMEOUT);
                }
            }
        }
    }

    // 加入发送邮箱
    if (HAL_CAN_AddTxMessage(instance->map.handle, &tx_header, pack->data, &mailbox) != HAL_OK)
    {
        // HAL 在这里只有两种失败：① 三个邮箱全满（HAL_CAN_ERROR_PARAM）；② 外设不在
        // READY/LISTENING（HAL_CAN_ERROR_NOT_INITIALIZED）。①是"资源满、可重试"，②是状态异常。
        // 上面第 592 行的判空与本次入队之间可能被同总线的其他上下文（ISR 里的续发钩子）抢走
        // 最后一个邮箱，故不能直接按前面那次判断下结论 —— 再读一次空闲量来区分。
        if (HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle) == 0)
            return CAN_BxcanTxFail(instance, BSP_BUSY);
        BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "HAL_CAN_AddTxMessage failed!");
        return CAN_BxcanTxFail(instance, BSP_HW_ERR);
    }

    // 溯源：记录该邮箱当前属于哪个实例（发送完成回调据此调用其回调）。
    // 必须紧跟在 AddTxMessage 之后——帧可能在下一行之前就被发出并触发完成中断
    s_can_tx_owner[instance->can_e][CAN_MailboxIndex(mailbox)] = instance;

    // 出参：使用的邮箱索引 + 发送后剩余空闲邮箱数（顺带更新状态快照）
    if (tx_mailbox != NULL)
        *tx_mailbox = CAN_MailboxIndex(mailbox);
    s_bxcan_status[instance->can_e].tx_free = (uint8_t)HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);
    s_bxcan_status[instance->can_e].tx_owner_busy = CAN_OwnerBusyCount(instance->can_e);
    if (tx_free_level != NULL)
        *tx_free_level = s_bxcan_status[instance->can_e].tx_free;

    return BSP_OK;
}

/**
 * @brief 有界等待发送邮箱腾出来（任务上下文，只给 CANRecover 用）
 * @param hcan CAN 句柄
 * @return 最后一次读到的空闲邮箱数（0 = 等到超时仍满，由调用方判失败）
 * @note 取消在途帧（TSR.ABRQx）是硬件异步执行的，写完立刻回读 TSR.TMEx 可能还是旧值 →
 *       这里按 CAN_RECOVER_DRAIN_US 轮询，把这个窗口等掉。等待用 DWT 计时（不依赖 tick）。
 */
static uint32_t CAN_BxcanWaitTxFree(CAN_HandleTypeDef *hcan)
{
    uint64_t start = DWT_GetTimeUs();
    uint32_t free_level = HAL_CAN_GetTxMailboxesFreeLevel(hcan);

    while (free_level == 0U && (DWT_GetTimeUs() - start) < (uint64_t)CAN_RECOVER_DRAIN_US)
        free_level = HAL_CAN_GetTxMailboxesFreeLevel(hcan);

    return free_level;
}

/**
 * @brief CAN 发送资源自恢复（任务上下文；五步顺序与理由见 bsp_can.h）
 * @note bxCAN 的 bus-off 总线恢复由 CubeMX 的 AutoBusOff=ENABLE 交给硬件（自动在检测到
 *       128 次连续 11 隐性位后重同步），这里只清软件错误标志并回读确认。
 */
BSP_Status_e CANRecover(CANInstance *instance)
{
    CAN_HandleTypeDef *hcan;
    uint32_t esr, tx_free;
    uint8_t can_idx;

    if (instance == NULL || instance->map.handle == NULL)
        return BSP_PARAM_ERR;
    can_idx = instance->can_e;
    hcan = instance->map.handle;
    if (can_idx >= CAN_NUM_MAX)
        return BSP_PARAM_ERR;

    // 外设没启动（CANConfig 未成功）时既没有邮箱占用也没有可读的寄存器状态，无从恢复
    if (!s_can_started[can_idx])
    {
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CANRecover: can_e=%d not started, nothing to recover", can_idx);
        return BSP_BUSY;
    }

    // ===== 入口自证：没有"总线级"证据就什么都不做（**必须在任何动作之前**）=====
    // 完整理由（为什么门槛不放调用点、为什么要搭发送错误计数器、为什么不按"资源占满"单独判）
    // 见 bsp_fdcan.c 的同名函数；bxCAN 这边是同一原则的同一份实现，只有判据的读法不同：
    //   ① ESR.BOFF：停在 bus-off。CubeMX 给 bxCAN 开了 AutoBusOff（硬件自己重同步），
    //      所以这里看到 BOFF 更多是"总线物理层还没回来"的信号，本函数末尾会再回读确认。
    //   ② ESR.TEC ≥ 128 且三个邮箱全占：发送错误计数已越界（bxCAN 每失败一次 +8、成功一次 -1），
    //      说明帧确实一直发不出去而不是单纯赶上了忙时 → 取消在途帧把邮箱腾出来。
    //      @note 用 TEC 而不是 EPVF：后者是"TEC ≥128 **或** REC ≥128"，收帧受干扰而发送正常的
    //            总线也会被算进去。ESR 里 TEC 只有低 8 位，TEC=256（bus-off 上限）时读回 0，
    //            但那一刻 BOFF 已置位、判据① 会接住，不影响。
    // @note bxCAN 没有 marker 池（只有 3 个邮箱），不存在 FDCAN 那条"池泄漏"判据。
    esr = hcan->Instance->ESR;
    tx_free = HAL_CAN_GetTxMailboxesFreeLevel(hcan);
    s_bxcan_status[can_idx].tx_free = (uint8_t)tx_free;
    if ((esr & CAN_ESR_BOFF) == 0U && !(tx_free == 0U && ((esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos) >= 128U))
        return BSP_BUSY;

    // ①② 取消全部在途发送并逐帧通知发起者（被成功取消的帧不会产生完成回调，必须显式收口）
    (void)CANAbortAllTx(instance);

    // ③ 清软件错误标志（清的是 HAL 的 ErrorCode 快照；硬件错误位由 ESR 只读反映）
    HAL_CAN_ResetError(hcan);

    // ④ 回读总线状态，判断恢复是否生效
    esr = hcan->Instance->ESR;
    // 邮箱空闲量要有界等到"取消落地"再读：ABRQ 是硬件异步执行的，写完立刻回读 TSR.TMEx
    // 可能还是旧值，会把一次正常的恢复误判成"邮箱仍占满"（见 bsp_can.h 的 CAN_RECOVER_DRAIN_US）
    tx_free = CAN_BxcanWaitTxFree(hcan);
    s_bxcan_status[can_idx].bus_off = (esr >> 2) & 0x1;
    s_bxcan_status[can_idx].error_passive = (esr >> 1) & 0x1;
    s_bxcan_status[can_idx].error_warning = esr & 0x1;
    s_bxcan_status[can_idx].lec = (esr >> 4) & 0x7;
    s_bxcan_status[can_idx].tec = (esr >> 16) & 0xFF;
    s_bxcan_status[can_idx].rec = (esr >> 24) & 0xFF;
    s_bxcan_status[can_idx].tx_free = (uint8_t)tx_free;

    // 仍在 bus-off：总线物理层没恢复（线没接回 / 终端电阻 / 对端没上电），只能等下次再试；
    // 邮箱仍占满：取消请求还没生效（ABRQ 是硬件异步执行的），同样算没恢复
    if (s_bxcan_status[can_idx].bus_off != 0U || tx_free == 0U)
    {
        s_bxcan_status[can_idx].recover_fail++;
        BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CANRecover failed (can_e=%d): still bus-off or mailboxes full", can_idx);
        return BSP_HW_ERR;
    }

    s_bxcan_status[can_idx].recover_ok++;
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

#if defined(BSP_CAN_LIST_LUT_USED)
/**
 * @brief 分发（LUT 启用时的唯一入口）：先按标准 ID 查表回调 LIST 标准帧，再循环兜底其余模式
 * @note 查表命中直接回调（O(该实例 filter 数)，无全 CAN 线性扫描）；循环内跳过 LIST 标准帧
 *       （已由查表处理），兜底 MASK/RANGE/扩展/未登记，与未启用 LUT 的完整循环等价。
 *       frame_type 二次校验防同 ID 下 STD_DATA / STD_REMOTE 串。
 */
static void CAN_ListLutDispatch(uint8_t ci, const CAN_HandleTypeDef *hcan, const CAN_Pack_s *pack)
{
    uint16_t id;
    CANInstance *inst;
    uint8_t i;

    if (ci >= CAN_NUM_MAX)
        return;
    id = (uint16_t)(pack->id & 0x7FF); /* 必须 uint16_t：0~2047 */
    inst = s_bxcan_list_lut[ci][id];
    if (inst != NULL)
    {
        uint8_t j;

        for (j = 0; j < inst->filter_num; j++)
        {
            CAN_Filter_s *f = &inst->filters[j];

            if (f->callback != NULL && CAN_ListLutCoverable(f) && f->frame_type == pack->frame_type &&
                (f->id0 == pack->id || (f->id1 != CAN_ID_UNUSED && f->id1 == pack->id)))
                f->callback(inst, pack);
        }
    }

    /* 循环兜底：LIST 标准帧已由查表分发（跳过），其余 MASK/RANGE/扩展/未登记在此匹配 */
    for (i = 0; i < s_can_idx; i++)
    {
        uint8_t j;

        inst = s_can_instance[i];
        if (inst->map.handle != hcan)
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
static void CAN_LoopDispatch(const CAN_HandleTypeDef *hcan, const CAN_Pack_s *pack)
{
    uint8_t i;

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
            if (CAN_FilterMatch(f, pack))
                f->callback(inst, pack);
        }
    }
}
#endif

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

    // 软件过滤分发：LUT 启用时单入口（查表 + 循环兜底），未启用时纯循环
#if defined(BSP_CAN_LIST_LUT_USED)
    CAN_ListLutDispatch(can_idx, hcan, &pack); /* 查表命中直接回调 LIST 标准帧，其余循环兜底 */
#else
    CAN_LoopDispatch(hcan, &pack); /* 完整循环，全模式分发 */
#endif
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

/*------------- 发送完成/中止回调（邮箱释放时触发，由 CANConfig 激活 CAN_IT_TX_MAILBOX_EMPTY） --------------
 * HAL 按 TSR.RQCPx 置位时 TXOKx 的情况二选一：
 *   TXOK=1            → TxMailbox*CompleteCallback（帧已发出）
 *   TXOK=0 且 ALST/TERR 置位 → 只置 ErrorCode，走 ErrorCallback（仲裁失败 / 发送错误）
 *   TXOK=0 且两者都不置     → TxMailbox*AbortCallback（取消请求生效，帧没发出去）
 * 三种情况都通过 CAN_TxResultHandler 把逐帧结果通知到发起者。
 */

/**
 * @brief 发送邮箱0完成回调
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 0, BSP_OK);
}

/**
 * @brief 发送邮箱1完成回调
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 1, BSP_OK);
}

/**
 * @brief 发送邮箱2完成回调
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 2, BSP_OK);
}

/**
 * @brief 发送邮箱0中止回调（取消请求生效、帧未发出）
 */
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 0, BSP_HW_ERR);
}

/**
 * @brief 发送邮箱1中止回调（取消请求生效、帧未发出）
 */
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 1, BSP_HW_ERR);
}

/**
 * @brief 发送邮箱2中止回调（取消请求生效、帧未发出）
 */
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan)
{
    CAN_TxResultHandler(hcan, 2, BSP_HW_ERR);
}

/*------------- 错误状态中断回调（由 CANConfig 激活 ERROR_WARNING / ERROR_PASSIVE / BUSOFF / LAST_ERROR_CODE）
 * --------------*/

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
        uint8_t tx_fail = 0;
        uint8_t protocol = ((error & (HAL_CAN_ERROR_STF | HAL_CAN_ERROR_FOR | HAL_CAN_ERROR_ACK | HAL_CAN_ERROR_BR |
                                      HAL_CAN_ERROR_BD | HAL_CAN_ERROR_CRC)) != 0U)
                               ? 1U
                               : 0U;
        uint8_t i;

        // 状态快照 + 错误计数（三个错误状态独立 if，可同时置位）
        if (can_idx < CAN_NUM_MAX)
        {
            s_bxcan_status[can_idx].tec = tec;
            s_bxcan_status[can_idx].rec = rec;
            s_bxcan_status[can_idx].lec = (esr >> 4) & 0x7;
            s_bxcan_status[can_idx].bus_off = (esr >> 2) & 0x1;
            s_bxcan_status[can_idx].error_passive = (esr >> 1) & 0x1;
            s_bxcan_status[can_idx].error_warning = esr & 0x1;
            s_bxcan_status[can_idx].last_err_us = DWT_GetTimeUs();

            if (error & HAL_CAN_ERROR_BOF)
                s_bxcan_status[can_idx].err_bus_off++;
            if (error & HAL_CAN_ERROR_EPV)
                s_bxcan_status[can_idx].err_passive++;
            if (error & HAL_CAN_ERROR_EWG)
                s_bxcan_status[can_idx].err_warning++;
            if ((error & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) != 0U)
                s_bxcan_status[can_idx].rx_lost++;
            if (protocol)
                s_bxcan_status[can_idx].err_protocol++;
        }

        // 逐帧结果收口：ALST/TERR = 那一帧没发出去（仲裁失败 / 发送错误）。
        // HAL 已在 IRQHandler 里清掉 RQCPx，邮箱号只能从错误码位反推（见 s_can_tx_fail_mask）。
        // 不加这步，这种帧就只有计数、没有回调，异步分包的发起者会永远卡在"发送中"。
        for (i = 0; i < CAN_TX_MAILBOX_NUM; i++)
        {
            if ((error & s_can_tx_fail_mask[i]) != 0U)
            {
                CAN_TxResultHandler(hcan, i, BSP_HW_ERR);
                tx_fail = 1;
            }
        }

        if (error & HAL_CAN_ERROR_BOF)
            BSPLOG(&g_can_log, LOG_LEVEL_ERROR, "CAN Bus-off! TEC=%d, REC=%d", tec, rec);
        if (error & HAL_CAN_ERROR_EPV)
            BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CAN Error Passive! TEC=%d, REC=%d", tec, rec);
        if (error & HAL_CAN_ERROR_EWG)
            BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CAN Error Warning! TEC=%d, REC=%d", tec, rec);
        // 其余为 Last Error Code 位集（协议错误）或发送失败标志，汇总上报
        if (protocol)
            BSPLOG(&g_can_log, LOG_LEVEL_WARNING, "CAN protocol error: 0x%08lX, TEC=%d, REC=%d", (unsigned long)error,
                   tec, rec);

        // 清软件错误标志（硬件 AutoBusOff=ENABLE 自动完成总线恢复）
        HAL_CAN_ResetError(hcan);

        // 纠正动作做完后再广播给上层（err_callback 契约：分类计数 → 快照 → 纠正 → 通知）。
        // BOF/EPV/EWG 是"总线健康度"的分级，ALST/TERR 与 LEC 位集都归到协议错误。
        // 注意 CAN_ERR_PROTOCOL 可能每帧触发一次（总线断开时），handler 必须廉价且幂等。
        if (error & HAL_CAN_ERROR_BOF)
            CAN_NotifyError(hcan, CAN_ERR_BUS_OFF);
        if (error & HAL_CAN_ERROR_EPV)
            CAN_NotifyError(hcan, CAN_ERR_ERROR_PASSIVE);
        if (error & HAL_CAN_ERROR_EWG)
            CAN_NotifyError(hcan, CAN_ERR_ERROR_WARNING);
        if ((error & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) != 0U)
            CAN_NotifyError(hcan, CAN_ERR_RX_OVERFLOW);
        if (protocol || tx_fail)
            CAN_NotifyError(hcan, CAN_ERR_PROTOCOL);
    }
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */

#endif /* BSP_CAN_USED */
