/**
 * @file bsp_i2c.c
 * @brief I2C驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置和从机地址管理
 *
 * @note 本层职责（对照 bsp_usart / bsp_spi / bsp_can）：
 *       ① 收发（寄存器读写与裸收发，三模式每次调用传参）；
 *       ② 必要回调（Master/Mem 的 Tx/Rx 完成 + Error）并分发到实例；
 *       ③ 纠正错误（等就绪超时/启动失败时复位句柄，总线仍 BUSY 时重建外设）；
 *       ④ 计数与状态快照存 static 结构体供调试器 Watch（`s_i2c_status`）。
 *       除此之外不提供任何接口：不做发送队列/缓冲，也不对外暴露 work_mode / is_ready。
 *       总线级恢复有对外入口 I2CBusRecover，从机级恢复（RSTN 脉冲 + 重跑初始化）归 DRV。
 *
 * @note **只做主模式**：本层不封装 HAL 的 `HAL_I2C_Slave_*` / `EnableListen_IT` 一族接口，
 *       也不实现只在从机模式下触发的回调（Addr / SlaveTx / SlaveRx / ListenCplt /
 *       AbortCplt）—— 全交回 HAL 的 `__weak` 空实现。板卡侧 `OwnAddress1` 也都是 0。
 *       真要让 MCU 当从机请另起模块，别往这里塞（从机是"方向由寻址阶段决定 + 长驻
 *       LISTEN 状态"，与这里"一笔一笔同步/异步收发"的模型不是一回事）。
 */

#include "bsp_i2c.h"
#include "app_cfg.h"

#ifdef BSP_I2C_USED

#if I2C_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_log.h"

/*------------- 私有变量 --------------*/

static uint8_t s_i2c_idx = 0;
#ifndef BSP_I2C_LOG_LIMIT
#define BSP_I2C_LOG_LIMIT 10
#endif                                                     // !BSP_I2C_LOG_LIMIT
LOG_INSTANCE_DEF(g_i2c_log, "bsp_i2c", BSP_I2C_LOG_LIMIT); /* I2C 日志实例 */

static I2CInstance *s_i2c_instance[I2C_INSTANCE_NUM] = {NULL};

/* 句柄索引 → 实例路由表（Config 时登记）。回调里先按索引取实例，
 * 索引本身用于状态统计——即使实例路由缺失，错误/事件也不漏计。
 * 不做"最近命中缓存"：i2c_map 只有几个表项，线性查找的成本远低于缓存失效的排查成本
 * （实例重配后缓存会指向旧实例）。 */
static I2CInstance *s_i2c_inst_by_i2c[I2C_NUM_MAX] = {NULL};

/*------------- I2C 状态/错误统计（调试用，调试器直接 Watch s_i2c_status） --------------*/

/**
 * @brief I2C 外设状态与错误统计（每总线一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：收发完成回调、错误回调、收发接口的失败返回，
 *       外加便于判断"当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;        /* 写完成次数（BLOCK 直接成功 + IT/DMA 的 TxCplt） */
    uint32_t rx_ok;        /* 读完成次数（BLOCK 直接成功 + IT/DMA 的 RxCplt） */
    uint32_t xfer_fail;    /* 四个收发接口返回非 BSP_OK 的总次数 */
    uint32_t xfer_busy;    /* 其中 BSP_BUSY（总线忙，上层应排队重试） */
    uint32_t xfer_timeout; /* 其中 BSP_TIMEOUT（等就绪超时 / HAL 启动即超时，句柄已复位） */
    /* 错误分类计数（HAL 的错误位可同时置位，独立 if） */
    uint32_t err_total;   /* HAL_I2C_ErrorCallback 次数 */
    uint32_t err_berr;    /* 总线错误（起始/停止时序错乱） */
    uint32_t err_arlo;    /* 仲裁丢失 */
    uint32_t err_af;      /* 从机 NACK（设备不在/地址错） */
    uint32_t err_ovr;     /* 溢出 */
    uint32_t err_dma;     /* DMA 错误 */
    uint32_t err_timeout; /* HAL 内部的超时（HAL_I2C_ERROR_TIMEOUT） */
    uint32_t err_other;   /* 其它错误位（SIZE / INVALID_CALLBACK / INVALID_PARAM…） */
    uint32_t err_no_dma;  /* 调用 DMA 模式但该口无对应方向的 DMA（hdmatx/hdmarx 为 NULL） */
    uint32_t err_start;   /* HAL 启动传输失败（非"忙"）次数，含超时类 */
    /* 收尾与恢复计数 */
    uint32_t abort_reset; /* 强制复位句柄次数（启动失败 / 等就绪超时 / 重配收尾） */
    uint32_t bus_rebuild; /* 因总线仍 BUSY 而重建外设（DeInit + Init）的次数 */
    uint32_t bus_recover; /* I2CBusRecover 真正动手重建的次数（入口自证通过） */
    uint32_t bus_recover_skip; /* I2CBusRecover 被调用但**没动手**的次数：入口自证不成立
                                * （非任务上下文 / BUSY 标志已落 / 本句柄有在途传输）。
                                * 与 bus_recover 一起看即可判断"上层是不是在白调"：这个数远大于
                                * bus_recover，说明触发者看的是实例级现象，而那些失败根本不在
                                * 总线上（见 bsp_i2c.md §2.B.1.1）。 */
    /* 探测计数（I2CIsDeviceReady）：恢复流程里那道"器件还在不在"的门禁 */
    uint32_t probe_ok;   /* 探测到从机应答（HAL_OK） */
    uint32_t probe_fail; /* 探测未通过：器件不应答，或探测本身就超时（两版 HAL 都返回
                          * HAL_ERROR/HAL_TIMEOUT，且 H7 在"试满 trials 全是 NACK"时
                          * 也会置 TIMEOUT 位，**分不开**，故不拆） */
    uint32_t probe_busy; /* 其中总线被占（HAL_BUSY，与器件在不在无关；F4 的"BUSY 标志
                          * 一直不落"也走这条，那种情况同时会置 ErrorCode 的 TIMEOUT 位） */
    /* 实时快照 */
    uint8_t state;              /* HAL State（最近一次采样） */
    uint8_t last_mode;          /* 最近一次传输模式 */
    uint8_t last_mem_addr_size; /* 最近一次寄存器地址宽度 */
    uint8_t lock;               /* 最近一次强制收尾时的 h->Lock（残留会让后续调用全 HAL_BUSY） */
    uint32_t error_code;        /* 最近一次 h->ErrorCode */
    uint64_t err_time_us;       /* 最近一次错误时间（DWT 微秒） */
    uint8_t dma_tx_capable;     /* Config 时记录：该口是否有 TX DMA */
    uint8_t dma_rx_capable;     /* Config 时记录：该口是否有 RX DMA */
    uint8_t dma_tx_state;       /* 最近一次强制收尾时 hdmatx->State（0xFF = 该口无 TX DMA） */
    uint8_t dma_rx_state;       /* 最近一次强制收尾时 hdmarx->State（0xFF = 该口无 RX DMA） */
    uint16_t dev_addr;          /* 最近一次从机地址（喂给 HAL 的线上形式：7 位左移后 / 10 位原样） */
    uint16_t mem_addr;          /* 最近一次寄存器地址 */
    uint16_t xfer_len;          /* 最近一次收发请求长度（完成回调据此填 rx_len） */
    uint16_t rx_len;            /* 最近一次读到的长度 */
} I2C_Status_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile I2C_Status_s s_i2c_status[I2C_NUM_MAX];

/* 两版 HAL（F4/H7）的 I2C 错误位定义逐位一致，故分类掩码可直接共用；
 * 掩码外的位（SIZE / DMA_PARAM / INVALID_CALLBACK / H7 的 INVALID_PARAM…）
 * 一律进 err_other，保证 err_total 与各项明细自洽。 */
#define I2C_ERROR_KNOWN_MASK                             \
    (uint32_t)(HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO | \
               HAL_I2C_ERROR_AF | HAL_I2C_ERROR_OVR |    \
               HAL_I2C_ERROR_DMA | HAL_I2C_ERROR_TIMEOUT)

/* "启动即超时"的错误位（见 I2C_StartFail）：
 * - HAL_I2C_ERROR_TIMEOUT：HAL 等标志位/等 BUSY 落 超时（两版都有）；
 * - HAL_I2C_WRONG_START：F4 的 I2C_RequestMemory* 在 START 位没发出去时置位。该宏**只有
 *   F4 的 HAL 定义**（H7 头文件里没有），故按宏存在与否条件编译，而不是按 CPU_CORE。 */
#ifdef HAL_I2C_WRONG_START
#define I2C_START_TIMEOUT_MASK (uint32_t)(HAL_I2C_ERROR_TIMEOUT | HAL_I2C_WRONG_START)
#else
#define I2C_START_TIMEOUT_MASK (uint32_t)(HAL_I2C_ERROR_TIMEOUT)
#endif

/*------------- 私有函数声明 --------------*/

static uint8_t I2C_Hi2cToIndex(const I2C_HandleTypeDef *hi2c);
static BSP_Status_e I2C_FailThenRet(const I2CInstance *instance, BSP_Status_e status);
static void I2C_SnapshotReq(uint8_t idx, I2CInstance *instance, uint16_t hal_dev_addr, uint16_t mem_addr,
                            I2C_MemAddrSize_e mem_addr_size, uint16_t len, BSP_Transfer_Mode_e mode);
static void I2C_ResetHandle(I2CInstance *instance);
static void I2C_RebuildPeriph(I2CInstance *instance);
static void I2C_AbortOnError(I2CInstance *instance, const char *reason, uint8_t notify);
static BSP_Status_e I2C_StartFail(I2CInstance *instance, HAL_StatusTypeDef st, const char *what,
                                  uint8_t idx, BSP_Transfer_Mode_e mode);
static BSP_Status_e I2C_WaitReady(I2CInstance *instance, uint32_t timeout_ms, const char *reason);
static BSP_Status_e I2C_ClaimBus(I2CInstance *instance, BSP_Transfer_Mode_e mode,
                                 uint32_t timeout_ms, const char *reason);
static uint8_t I2C_CheckDmaCapability(I2CInstance *instance, BSP_Transfer_Mode_e mode,
                                      uint8_t need_tx, uint8_t need_rx);
static uint16_t I2C_ToHalMemAddrSize(I2C_MemAddrSize_e mem_addr_size);
static BSP_Status_e I2C_EncodeDevAddr(const I2CInstance *instance, uint16_t dev_addr, uint16_t *out);
static void I2C_ReportTxCplt(I2C_HandleTypeDef *hi2c);
static void I2C_ReportRxCplt(I2C_HandleTypeDef *hi2c);

/*------------- 私有函数实现：查表与计数 --------------*/

/**
 * @brief hi2c → 板载 I2C 索引（I2C_EX_2 / I2C_IST8310 / … 对应 i2c_map 下标）
 * @retval I2C_NUM_MAX 未找到
 */
static uint8_t I2C_Hi2cToIndex(const I2C_HandleTypeDef *hi2c)
{
    uint8_t i;

    if (hi2c == NULL)
        return I2C_NUM_MAX;

    for (i = 0; i < I2C_NUM_MAX; i++)
    {
        if (i2c_map[i].handle == hi2c)
            return i;
    }
    return I2C_NUM_MAX;
}

/**
 * @brief 收发接口失败路径统一计数后返回状态码
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏内 `return (ret)` 会求值），
 *       使所有失败返回点都统计进 s_i2c_status[]，日志行为不变。
 */
static BSP_Status_e I2C_FailThenRet(const I2CInstance *instance, BSP_Status_e status)
{
    uint8_t idx = (instance != NULL) ? I2C_Hi2cToIndex(instance->handle) : I2C_NUM_MAX;

    if (idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].xfer_fail++;
        if (status == BSP_BUSY)
            s_i2c_status[idx].xfer_busy++;
        else if (status == BSP_TIMEOUT)
            s_i2c_status[idx].xfer_timeout++;
    }
    return status;
}

/**
 * @brief 登记本次请求的现场（调试快照）
 * @note 必须在真正调用 HAL **之前**登记：`xfer_len` 是完成回调填 `rx_len` 的唯一依据
 *       （I2C 的 HAL 句柄里没有"本次请求长度"这种现成信息，不像 SPI 的 RxXferSize），
 *       异步传输的回调可能在登记之后随时进来。
 * @note hal_dev_addr 是**编码后**的形式（与真正喂给 HAL 的一致），不是调用方传的原始地址：
 *       看快照时能直接与 HAL 侧行为对上，不必再自己左移一遍。
 */
static void I2C_SnapshotReq(uint8_t idx, I2CInstance *instance, uint16_t hal_dev_addr, uint16_t mem_addr,
                            I2C_MemAddrSize_e mem_addr_size, uint16_t len, BSP_Transfer_Mode_e mode)
{
    if (idx >= I2C_NUM_MAX)
        return;

    s_i2c_status[idx].dev_addr = hal_dev_addr;
    s_i2c_status[idx].mem_addr = mem_addr;
    s_i2c_status[idx].last_mem_addr_size = (uint8_t)mem_addr_size;
    s_i2c_status[idx].xfer_len = len;
    s_i2c_status[idx].last_mode = (uint8_t)mode;
    s_i2c_status[idx].state = instance->handle->State;
}

/*------------- 私有函数实现：上下文判定与句柄收尾 --------------*/

/**
 * @brief 当前上下文能否安全执行收尾动作（HAL_DMA_Abort 按 `HAL_GetTick` 自旋；
 *        HAL_I2C_DeInit/Init 是外设重建，不按 tick 等，但只允许在任务上下文做）
 * @retval 1 可以（普通任务上下文）
 * @retval 0 不可以（死等风险，只能改期）
 *
 * @note 两类不安全上下文都要查（共同后果是 `HAL_GetTick` 冻住）：
 *       ① 中断上下文（`IPSR != 0`）——IST8310 的 INT 模式就是在 DRDY EXTI 里发起传输。
 *          HAL tick 源在本工程是 TIM 不是 SysTick（DJI_C 的 TIM14、其余板 TIM23，
 *          优先级 5/15），优先级数值不小于外设中断的 5，抢占不了本 ISR，故中断里 tick 不前进；
 *       ② 临界区——`taskENTER_CRITICAL` 抬的是 BASEPRI（FreeRTOS ARM_CM4F/CM7 端口的
 *          `portDISABLE_INTERRUPTS` = `vPortRaiseBASEPRI`），tick 源同样进不来。
 * @note `HAL_DMA_Abort` 内部按 `HAL_GetTick` 轮询等 DMA 流的 EN 位清零
 *       （`while(...) + HAL_TIMEOUT_DMA_ABORT`），tick 不前进就是死循环；重建外设
 *       （`HAL_I2C_DeInit/Init` → MspDeInit/MspInit，涉及时钟与 GPIO/NVIC）更是只能
 *       在任务上下文做。故判定为 0 时只跳过**这两件事**，软件状态复位照做不误
 *       （见 I2C_ResetHandle）：那几行不碰 tick，而且正是它让这笔传输作废 ——
 *       State/Mode 一旦归位，HAL 的 ISR 就不再按这两个字段把迟到的完成中断分发到
 *       本层回调。剩下"外设硬件可能还占着总线"这一项，留给任务上下文的下一次
 *       收发入口或 I2CBusRecover 用 I2C_RebuildPeriph 收尾。
 */
static inline uint8_t I2C_CanBlockingAbort(void)
{
    return (__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U);
}

/**
 * @brief 总线是否空闲可用（可以发起新传输）
 * @param h I2C 句柄
 * @retval 1 空闲
 * @retval 0 忙 / 句柄被别人锁着
 *
 * @note 只看 `State == READY` 不够：HAL 的每个 I2C 接口开头都是
 *       `if (hi2c->Lock == HAL_LOCKED) return HAL_BUSY;`，而 `Lock` 由接口自己成对
 *       开关（并非只在传输期间）。把它并进判据，是为了让"能不能发起"这件事在本层就有
 *       确定答案，而不是让 HAL 去返回一个既不置 ErrorCode、也不动 State 的 `HAL_BUSY`。
 */
static uint8_t I2C_BusIsIdle(const I2C_HandleTypeDef *h)
{
    return (h->State == HAL_I2C_STATE_READY && h->Lock == HAL_UNLOCKED) ? 1 : 0;
}

/**
 * @brief 把 I2C 句柄恢复成"空闲可用"的兜底状态
 *
 * @note 与 SPI 不同，I2C 的 HAL 在正常完成/错误回调前多数分支已复位 State 并解锁，
 *       但仍有几处只置 ErrorCode 就返回（尤其等待 BUSY 标志超时的分支），
 *       会把句柄留在 HAL_I2C_STATE_BUSY_* 且 Lock 残留，使后续调用直接返回 HAL_BUSY。
 *       这里统一兜底，两处调用点（AbortOnError / BusRecover）共用。
 * @note PreviousState 的"空值"是 HAL 私有宏 I2C_STATE_NONE，其定义就等于
 *       HAL_I2C_MODE_NONE（见 F4/H7 的 stm32xxxx_hal_i2c.c），此处直接写后者。
 * @note XferISR 是 H7 独有字段（F4 句柄里没有），故按内核条件编译。
 */
static void I2C_ResetHandle(I2CInstance *instance)
{
    I2C_HandleTypeDef *h = instance->handle;

    /* 停掉异步传输与中断源：不关的话残留在途传输会在复位后又把状态改回 BUSY。
     * 但中断上下文里必须跳过 DMA 中止 —— HAL_DMA_Abort 在 State==BUSY 且 DMA 的
     * EN 位不落时用 HAL_GetTick() 死等，而 HAL tick 源（TIM14/TIM23）的 NVIC 优先级数值
     * 不小于外设中断的 5（见 I2C_CanBlockingAbort），抢占不了 → 中断里 HAL_GetTick()
     * 根本不前进 → 那个 while 就是死循环。中断里本也不该有用 DMA 在飞的
     * 传输（DMA 模式禁止从 ISR 发起），真卡住了交给任务上下文的 I2CBusRecover 收尾。 */
    if (I2C_CanBlockingAbort())
    {
        if (h->hdmarx != NULL)
        {
            (void)HAL_DMA_Abort(h->hdmarx);
        }
        if (h->hdmatx != NULL)
        {
            (void)HAL_DMA_Abort(h->hdmatx);
        }
    }

#if defined(CPU_CORE) && (CPU_CORE == CORTEX_M7)
    /* H7：中断源在 CR1，逐个关掉 */
    __HAL_I2C_DISABLE_IT(h, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI |
                                I2C_IT_ADDRI | I2C_IT_RXI | I2C_IT_TXI);
    h->XferISR = NULL;
#else
    /* F4：中断源在 CR2 */
    __HAL_I2C_DISABLE_IT(h, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
#endif

    h->State = HAL_I2C_STATE_READY;
    h->Mode = HAL_I2C_MODE_NONE;
    h->PreviousState = HAL_I2C_MODE_NONE;
    h->ErrorCode = HAL_I2C_ERROR_NONE;
    h->Lock = HAL_UNLOCKED;
}

/**
 * @brief 外设级重建：DeInit + Init，真正把卡住的 I2C 松开
 *
 * @note 为什么光复位结构体字段不够：HAL 在传输中途出错时硬件 BUSY 标志可能仍置位，
 *       此后每次调用一进 HAL 就立刻返回 HAL_BUSY（F4 的阻塞版还会先在
 *       I2C_TIMEOUT_BUSY_FLAG 上白等 25ms 才认输），形成"每次调用都失败"的死循环，
 *       外部现象是"I2C 从机永久失联"。
 *       `HAL_I2C_DeInit` 会 `__HAL_I2C_DISABLE` 掉 PE（这是 HAL 自己在
 *       I2C_ITError/I2C_DMAAbort 里用的同一手法）并调 MspDeInit 关时钟、放引脚；
 *       `HAL_I2C_Init` 再按 hi2c->Init 的原始值重建外设。这是不翻转 SCL/SDA 的前提下
 *       唯一能真正松开总线的路径，与 I2CBusRecover 是同一套动作。
 * @note 含 MspDeInit/MspInit（时钟、GPIO、NVIC 重配），**只能在任务上下文调用**
 *       （由 I2C_CanBlockingAbort 保证）。
 */
static void I2C_RebuildPeriph(I2CInstance *instance)
{
    I2C_HandleTypeDef *h = instance->handle;

    if (HAL_I2C_DeInit(h) != HAL_OK)
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "HAL_I2C_DeInit failed");
    }
    if (HAL_I2C_Init(h) != HAL_OK)
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "HAL_I2C_Init failed");
    }
}

/**
 * @brief 传输失败收尾：留现场 → 复位 HAL 状态 → （必要时）重建外设 → 通知上层
 * @param instance I2C实例
 * @param reason   失败原因（仅日志）
 *
 * @note 必须做这一步的原因：本文件"等待 State==READY"的循环超时后，若不复位，
 *       句柄永久停在 HAL_I2C_STATE_BUSY_*，而 DRV 层等不到完成回调、其传输标志
 *       也永远清不掉 → 整个从机永久失联。
 * @note 不在这里调 HAL_I2C_Master_Abort_IT：它是异步的、依赖 I2C 中断，
 *       总线卡死时永远不会完成（等于二次卡死）；且本函数可能运行在任务上下文
 *       的普通调用里（也只在任务上下文里做重建）。
 * @note 硬件 BUSY 兜底：只复位软件字段救不了"外设还占着总线"的情况（见 I2C_RebuildPeriph）。
 *       仅在对总线真的还 busy 时才重建 —— 常见的 NACK / 从机不在 之类错误之后总线是空闲的，
 *       不做无谓的时钟与 GPIO 重建。
 * @note 被收尾的那次传输不会再有完成回调，所以按 err_callback 契约通知上层复位自身状态。
 *       顺序必须是"复位句柄之后"再通知：上层判据普遍是"State == READY 才说明 HAL 已收尾"。
 * @note notify 决定这次要不要通知，由调用方按**本次传输模式**给：
 *       异步（IT/DMA）为 1，BLOCK 为 0。理由（bsp_i2c.h 的 I2CInstance 契约：
 *       "BLOCK 模式不产生任何回调"，drv_ist8310 的初始化序列正是照这条写的）：
 *       - BLOCK 的成败在调用点就由返回值给出了（BSP_TIMEOUT / BSP_HW_ERR），
 *         再回一次 err_callback 是同一个失败说两遍；
 *       - 更要紧的是语义污染：上层的 err_callback 是**异步采集链**的故障入口
 *         （drv_ist8310 在其中累积 fail_count / 置恢复请求），而 BLOCK 传输大量出现在
 *         初始化与恢复序列里 —— 那里的失败是**被容忍**的探针（如 WAI 重试本就要试几次
 *         才知道器件在不在），混进去会让"器件不在"直接变成"采集链故障"，在恢复流程内部
 *         再提出恢复请求。
 */
static void I2C_AbortOnError(I2CInstance *instance, const char *reason, uint8_t notify)
{
    I2C_HandleTypeDef *h = instance->handle;
    uint8_t idx = I2C_Hi2cToIndex(h);
    int tx_state = (h->hdmatx != NULL) ? (int)h->hdmatx->State : -1;
    int rx_state = (h->hdmarx != NULL) ? (int)h->hdmarx->State : -1;

    /* 先留现场再复位：I2C_ResetHandle 会把 State / ErrorCode / Lock 全清掉，
     * 而那三个正是判断"为什么卡住"的唯一直观依据。
     * dma 状态取 `HAL_DMA_STATE_*`，**编号随 HAL 版本不同**：关键只看 1=READY（空闲）
     * 与 2=BUSY（在途），其余为异常/终止态（F4 是 3=TIMEOUT/4=ERROR/5=ABORT，
     * H7 是 3=ERROR/4=ABORT）；-1 表示该口压根没有这一路 DMA。 */
    if (idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].abort_reset++;
        s_i2c_status[idx].state = h->State;
        s_i2c_status[idx].error_code = h->ErrorCode;
        s_i2c_status[idx].lock = h->Lock;
        s_i2c_status[idx].err_time_us = DWT_GetTimeUs();
        s_i2c_status[idx].dma_tx_state = (tx_state < 0) ? 0xFF : (uint8_t)tx_state;
        s_i2c_status[idx].dma_rx_state = (rx_state < 0) ? 0xFF : (uint8_t)rx_state;
    }

    BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR,
           "I2C %s (i2c_e=%d, state=0x%02X, err=0x%lX, lock=%d, tx_dma=%d, rx_dma=%d)",
           reason, (int)instance->i2c_e, (unsigned)h->State, (unsigned long)h->ErrorCode,
           (int)h->Lock, tx_state, rx_state);

    I2C_ResetHandle(instance);

    /* 中断上下文里不重建（含 MspDeInit/MspInit），留给任务上下文：drv_ist8310 攒够失败
     * 次数后会调 I2CBusRecover，走的正是同一条重建路径。 */
    if (I2C_CanBlockingAbort() && (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET))
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Bus still busy after abort, rebuilding peripheral");
        if (idx < I2C_NUM_MAX)
        {
            s_i2c_status[idx].bus_rebuild++;
        }
        I2C_RebuildPeriph(instance);
        I2C_ResetHandle(instance);
    }

    if (notify && instance->err_callback != NULL)
    {
        instance->err_callback(instance, I2C_ERR_ABORT); /* 被收尾的传输不会再有完成回调 */
    }
}

/**
 * @brief 启动结果分类：HAL 报"忙"是流控信号，不是故障；超时与真故障也要分开
 * @param instance I2C实例
 * @param st       本次调用的 HAL 返回
 * @param what     动作名（仅日志）
 * @param idx      总线下标
 * @param mode     本次传输模式：只用来决定收尾时要不要按 err_callback 契约通知上层
 *                 （BLOCK 不通知，见 I2C_AbortOnError 的 notify）
 * @retval BSP_BUSY    `HAL_BUSY`：此刻总线被占用（HAL 的每个接口入口都判 `Lock`/`State`，
 *                     且此时**不动 State、不置 ErrorCode**）。计入 `xfer_busy`，
 *                     不打日志、不计 `err_start` —— 上层下一拍/下一帧重试即可
 * @retval BSP_TIMEOUT 超时类失败：`ErrorCode` 带 TIMEOUT（等标志位/等 BUSY 超时）或
 *                     F4 的 WRONG_START（START 位没发出去）。计入 `err_start`/`xfer_timeout`，
 *                     已复位句柄；**IT/DMA 才**按契约回调 `err_callback`，BLOCK 不回调
 *                     （它的失败已由本次调用的返回值给出，见 @param mode）
 * @retval BSP_HW_ERR  其余真失败：`ErrorCode` 是 NACK/BERR/ARLO/DMA…（从机不在/时序错乱）。
 *                     同样计入 `err_start` 并走 I2C_AbortOnError 收尾（通知与否同上）
 *
 * @note 为什么"忙"必须和真失败分开：三种模式都会在这里拿到 `HAL_BUSY` ——
 *       `I2C_ClaimBus` 判完就绪到真正调用 HAL 之间始终有一个窗口（此刻被更高优先级的
 *       中断抢进来发起传输），BLOCK 还会撞上"State 是 READY、但硬件 BUSY 标志没过期"
 *       的另一条 `HAL_BUSY`。若把它们记成 `err_start` 并打错误日志，就会在日志里复刻
 *       bsp_spi 那条假象（`err=0x0` 的启动失败），把排查引向错误方向；
 *       而真失败必然带着 `ErrorCode`，两者的区分度是干净的。
 * @note 为什么超时也要单独分出来：`HAL_I2C_ERROR_TIMEOUT` / `WRONG_START` 都表示
 *       "这一笔压根没发出去、总线本身多半还是好的"（从机没应答则是 AF，不在这里），
 *       上层据此可以立刻重试一次，而不是当硬件故障进恢复流程。注意 H7 的超时是
 *       以 `HAL_ERROR` + `ErrorCode=TIMEOUT` 返回的（不是 `HAL_TIMEOUT`），
 *       所以判据只能取 `ErrorCode`，不能看 `st`。
 * @note `ErrorCode` 必须在 I2C_AbortOnError **之前**采样：它会经 I2C_ResetHandle
 *       把 ErrorCode 清零，之后再读就只剩 0（旧版把它记成 `err_start` 时，
 *       现场信息就是这样丢掉的）。
 */
static BSP_Status_e I2C_StartFail(I2CInstance *instance, HAL_StatusTypeDef st, const char *what,
                                  uint8_t idx, BSP_Transfer_Mode_e mode)
{
    uint32_t err = instance->handle->ErrorCode; /* 复位前采样，见上 */

    if (st == HAL_BUSY)
    {
        /* 除常规争用外，F4 的 IT/DMA 入口还有一条 HAL_BUSY：State 已是 READY、硬件 BUSY
         * 标志却在 I2C_TIMEOUT_BUSY_FLAG 内不落，这时**会**置 TIMEOUT（H7 的对应分支
         * 不置位）。那是总线真卡死而非争用，落到下面按超时上报并收尾
         * （AbortOnError 里会因 BUSY 标志仍置而重建外设），否则上层只会一直重试。 */
        if ((err & HAL_I2C_ERROR_TIMEOUT) == 0U)
        {
            return I2C_FailThenRet(instance, BSP_BUSY);
        }
    }

    if (idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].err_start++;
    }
    /* BLOCK 的失败已由本次调用的返回值给出，不再回 err_callback（见 I2C_AbortOnError） */
    I2C_AbortOnError(instance, what, (uint8_t)(mode != BSP_BLOCK_MODE));

    if ((err & I2C_START_TIMEOUT_MASK) != 0U)
    {
        return I2C_FailThenRet(instance, BSP_TIMEOUT);
    }
    return I2C_FailThenRet(instance, BSP_HW_ERR);
}

/**
 * @brief 等待总线就绪（自旋判据用 DWT 计时，不是 HAL tick；只由 I2C_ClaimBus 在 IT/DMA 下调用）
 * @param instance   I2C实例
 * @param timeout_ms 等待超时（ms）；0 = 只判一次，忙即返回
 * @param reason     超时收尾时的日志用词
 * @retval BSP_OK      总线就绪
 * @retval BSP_BUSY    忙（timeout_ms == 0）
 * @retval BSP_TIMEOUT 等待超时（已在任务上下文复位句柄并回调了 err_callback）
 *
 * @note 旧版这里在 timeout_ms == 0 时会**死循环**（`while (State != READY)` 里的超时判据
 *       被 `timeout_ms > 0` 短路，永远不成立），于是不得不显式拒绝 0 超时并打错误日志。
 *       现在统一为"0 即不等待"：立刻 BSP_BUSY，中断里也能安全使用。
 */
static BSP_Status_e I2C_WaitReady(I2CInstance *instance, uint32_t timeout_ms, const char *reason)
{
    BSP_Timeout_s t;

    if (I2C_BusIsIdle(instance->handle))
    {
        return BSP_OK;
    }

    if (timeout_ms == 0)
    {
        return BSP_BUSY;
    }

    BSP_TimeoutStart(&t, timeout_ms);
    while (!I2C_BusIsIdle(instance->handle))
    {
        if (BSP_TimeoutExpired(&t))
        {
            /* 等不到就绪同样是"传输没发出去"，必须通知上层，否则其传输标志永久卡死。
             * 本路径只被 IT/DMA 走到（BLOCK 由 I2C_ClaimBus 短路，从不进 I2C_WaitReady），
             * 故恒 notify=1。 */
            I2C_AbortOnError(instance, reason, 1);
            return BSP_TIMEOUT;
        }
    }

    return BSP_OK;
}

/**
 * @brief 发起传输前的"总线归属"检查：不是空闲的，就别碰实例里的任何现场
 * @param instance   I2C实例
 * @param mode       本次传输模式
 * @param timeout_ms 等就绪超时（**仅 IT/DMA 有效**；BLOCK 不等待，见下）
 * @param reason     超时收尾时的日志用词
 * @retval BSP_OK      总线空闲，可发起
 * @retval BSP_BUSY    已被占用（BLOCK 也归这里，上层下一拍重试）
 * @retval BSP_TIMEOUT IT/DMA 等就绪超时（已复位句柄并回调 err_callback）
 *
 * @note 四个收发接口都必须**先过这一关再写实例/快照状态**。原因：
 *       `I2C_SnapshotReq` 记的 `xfer_len` 是在途那笔传输的完成回调填 `rx_len` 的唯一依据，
 *       一旦被一笔"反正要失败"的调用覆盖，在途那笔的 `rx_len` 就再也对不上；
 *       收/发缓冲（`rx_buff`、DRV 传入的 tx 缓冲）同理 —— HAL 的 `Lock` 只管它自己的句柄，
 *       不管我们的缓冲。
 * @note BLOCK 模式**不等待**（`timeout_ms` 对 BLOCK 是 HAL 的传输超时，语义不同），
 *       只判一次：忙就 `BSP_BUSY`。这与"放行让 HAL 自己返回 `HAL_BUSY`"结果完全相同
 *       （I2C_StartFail 把不带 ErrorCode 的 `HAL_BUSY` 也归为 `BSP_BUSY`），
 *       区别只在本次不会先污染现场。也正因为判据是 `State/Lock` 而不是硬件 BUSY 标志，
 *       "总线真卡死"（State 已 READY、卡的是 BUSY 标志）这一关照样放行，
 *       仍由 HAL 在 `I2C_TIMEOUT_BUSY_FLAG` 后带 TIMEOUT 报错、进 I2C_AbortOnError 收尾。
 * @note BLOCK 这一支与 `I2C_WaitReady` 里 `timeout_ms == 0` 那一支是同一个判据的两种
 *       写法（都只看 `I2C_BusIsIdle`），统一到这里之后，四个收发接口就不必各写一遍
 *       `if (mode != BSP_BLOCK_MODE)`。
 */
static BSP_Status_e I2C_ClaimBus(I2CInstance *instance, BSP_Transfer_Mode_e mode,
                                 uint32_t timeout_ms, const char *reason)
{
    if (mode == BSP_BLOCK_MODE)
    {
        return I2C_BusIsIdle(instance->handle) ? BSP_OK : BSP_BUSY;
    }

    return I2C_WaitReady(instance, timeout_ms, reason);
}

/**
 * @brief DMA 能力校验（该口没配对应方向的 DMA 就明确拒绝，而不是让 HAL 静默失败）
 * @param instance I2C实例
 * @param mode     本次传输模式
 * @param need_tx  本次传输是否需要 TX DMA
 * @param need_rx  本次传输是否需要 RX DMA
 * @retval 1 可用（或不是 DMA 模式，无需校验）
 * @retval 0 不可用（已计数并打日志）
 */
static uint8_t I2C_CheckDmaCapability(I2CInstance *instance, BSP_Transfer_Mode_e mode,
                                      uint8_t need_tx, uint8_t need_rx)
{
    if (mode != BSP_DMA_MODE)
    {
        return 1;
    }

    if ((need_tx && instance->handle->hdmatx == NULL) ||
        (need_rx && instance->handle->hdmarx == NULL))
    {
        uint8_t idx = I2C_Hi2cToIndex(instance->handle);

        if (idx < I2C_NUM_MAX)
        {
            s_i2c_status[idx].err_no_dma++;
        }
        BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "I2C DMA not available (i2c_e=%d, tx=%d, rx=%d)!",
               (int)instance->i2c_e, need_tx, need_rx);
        return 0;
    }
    return 1;
}

/**
 * @brief 设备地址 → HAL 需要的"线上形式"（7 位左移腾出 R/W 位，10 位原样）
 * @param instance I2C实例（取句柄的 Init.AddressingMode）
 * @param dev_addr 设备地址本身（7 位寻址 0x00~0x7F / 10 位寻址 0x000~0x3FF）
 * @param out      输出：可直接喂给 HAL 的 DevAddress
 * @retval BSP_OK        成功
 * @retval BSP_PARAM_ERR 越界（不做静默截断）
 *
 * @note 见头文件「从机地址形态」：约定与 xrobot/LibXR 的 EncodeHalDevAddress 一致，
 *       由本层统一做，调用方只传器件手册上的地址。
 * @note 两版 HAL 的 7 位分支都用 `I2C_7BIT_ADD_WRITE(DevAddress)`（F4）/ 直接写 SADD
 *       （H7），**都不做左移**，故左移必须由调用方完成；10 位分支 F4 用
 *       `I2C_10BIT_HEADER_WRITE`（取 addr[9:8]）、H7 用 `I2C_CR2_ADD10` + SADD[9:0]，
 *       两者要的都是**未左移**的裸地址 —— 这就是不能用统一宏 `addr << 1` 的原因。
 * @note 已知限制：F4 的 Mem 接口（`I2C_RequestMemoryWrite/Read`）只用 7 位宏，
 *       10 位寻址下 Mem 读写是坏的（HAL 的限制）；要用 10 位从机请走
 *       Master 收发自行拼寄存器地址。
 */
static BSP_Status_e I2C_EncodeDevAddr(const I2CInstance *instance, uint16_t dev_addr, uint16_t *out)
{
    if (out == NULL)
    {
        return BSP_PARAM_ERR;
    }

    if (instance->handle->Init.AddressingMode == I2C_ADDRESSINGMODE_10BIT)
    {
        if (dev_addr > 0x3FFU)
        {
            return BSP_PARAM_ERR;
        }
        *out = dev_addr;
        return BSP_OK;
    }

    if (dev_addr > 0x7FU)
    {
        return BSP_PARAM_ERR;
    }
    *out = (uint16_t)(dev_addr << 1);
    return BSP_OK;
}

/**
 * @brief 寄存器地址宽度枚举 → HAL 常量
 */
static uint16_t I2C_ToHalMemAddrSize(I2C_MemAddrSize_e mem_addr_size)
{
    return (mem_addr_size == I2C_MEM_ADDR_SIZE_16BIT) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT;
}

/*------------- 完成回调的公共处理（四个 HAL 回调共用两份实现） --------------*/

/**
 * @brief 写传输完成的公共处理（Master/Mem 两路的 TxCplt 都走这里）
 * @note 写完成**不碰 rx_len**：写没有接收结果，把它清 0 是旧版"写也派发 rx_callback"的
 *       产物；现在读写各走各的回调，上层不必再从 rx_len 里猜这笔是什么。
 */
static void I2C_ReportTxCplt(I2C_HandleTypeDef *hi2c)
{
    uint8_t idx = I2C_Hi2cToIndex(hi2c);
    I2CInstance *instance;

    if (idx >= I2C_NUM_MAX)
    {
        return;
    }

    s_i2c_status[idx].tx_ok++;
    s_i2c_status[idx].state = hi2c->State;

    instance = s_i2c_inst_by_i2c[idx];
    if (instance != NULL && instance->tx_callback != NULL)
    {
        instance->tx_callback(instance);
    }
}

/**
 * @brief 读传输完成的公共处理（Master/Mem 两路的 RxCplt 都走这里）
 * @note 长度取本次请求时登记的 `xfer_len`：I2C 的 HAL 句柄里没有"本次请求长度"这种
 *       现成信息（不像 SPI 的 RxXferSize），只能由本层在发起时记下。
 */
static void I2C_ReportRxCplt(I2C_HandleTypeDef *hi2c)
{
    uint8_t idx = I2C_Hi2cToIndex(hi2c);
    I2CInstance *instance;
    uint16_t len;

    if (idx >= I2C_NUM_MAX)
    {
        return;
    }

    len = s_i2c_status[idx].xfer_len;
    s_i2c_status[idx].rx_ok++;
    s_i2c_status[idx].state = hi2c->State;
    s_i2c_status[idx].rx_len = len;

    instance = s_i2c_inst_by_i2c[idx];
    if (instance == NULL)
    {
        return;
    }

    instance->rx_len = len;

    if (instance->rx_callback != NULL)
    {
        instance->rx_callback(instance);
    }
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief 主机发送完成回调（IT/DMA）
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_ReportTxCplt(hi2c);
}

/**
 * @brief 主机接收完成回调（IT/DMA）
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_ReportRxCplt(hi2c);
}

/**
 * @brief 寄存器写完成回调（IT/DMA）
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_ReportTxCplt(hi2c);
}

/**
 * @brief 寄存器读完成回调（IT/DMA）
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_ReportRxCplt(hi2c);
}

/**
 * @brief I2C错误回调（ISR 上下文）
 * @param hi2c 发生错误的句柄
 * @note 本回调内完成分类计数 + 状态快照 + 通知上层。**不在这里做中止收尾**：
 *       HAL 的错误路径在调用本回调前已复位 State 并解锁，而在 ISR 里再调
 *       I2C_AbortOnError 会走到 DMA 中止（HAL_DMA_Abort 按 HAL_GetTick 自旋等 EN 位清零）
 *       与外设重建两条收尾路径 —— 中断里 tick 不前进、外设重建也做不了，
 *       所以那里被 I2C_CanBlockingAbort 拦下（真没复位干净，下一次收发会在"等就绪"
 *       超时路径里补做）。
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    uint8_t idx = I2C_Hi2cToIndex(hi2c);
    I2CInstance *instance;
    uint32_t error_code;

    if (idx >= I2C_NUM_MAX)
    {
        return;
    }

    error_code = hi2c->ErrorCode;
    instance = s_i2c_inst_by_i2c[idx];

    /* ① 分类计数（多种错误可同时置位，必须独立 if，else-if 会漏计） */
    s_i2c_status[idx].err_total++;
    if (error_code & HAL_I2C_ERROR_BERR)
        s_i2c_status[idx].err_berr++;
    if (error_code & HAL_I2C_ERROR_ARLO)
        s_i2c_status[idx].err_arlo++;
    if (error_code & HAL_I2C_ERROR_AF)
        s_i2c_status[idx].err_af++;
    if (error_code & HAL_I2C_ERROR_OVR)
        s_i2c_status[idx].err_ovr++;
    if (error_code & HAL_I2C_ERROR_DMA)
        s_i2c_status[idx].err_dma++;
    if (error_code & HAL_I2C_ERROR_TIMEOUT)
        s_i2c_status[idx].err_timeout++;
    if (error_code & ~I2C_ERROR_KNOWN_MASK)
        s_i2c_status[idx].err_other++;

    /* ② 状态快照 */
    s_i2c_status[idx].error_code = error_code;
    s_i2c_status[idx].state = hi2c->State;
    s_i2c_status[idx].lock = hi2c->Lock;
    s_i2c_status[idx].err_time_us = DWT_GetTimeUs();

    /* ③ 上报（保留解码日志，便于直接看出是哪种错） */
    BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Error detected, code=0x%lX (BERR:%d ARLO:%d AF:%d OVR:%d DMA:%d TIMEOUT:%d)",
           error_code,
           (error_code & HAL_I2C_ERROR_BERR) ? 1 : 0,     // 总线错误（起始/停止时序错乱）
           (error_code & HAL_I2C_ERROR_ARLO) ? 1 : 0,     // 仲裁丢失
           (error_code & HAL_I2C_ERROR_AF) ? 1 : 0,       // 从机 NACK（设备不在/地址错）
           (error_code & HAL_I2C_ERROR_OVR) ? 1 : 0,      // 溢出
           (error_code & HAL_I2C_ERROR_DMA) ? 1 : 0,      // DMA错误
           (error_code & HAL_I2C_ERROR_TIMEOUT) ? 1 : 0); // 超时

    if (instance != NULL && instance->err_callback != NULL)
    {
        instance->err_callback(instance, I2C_ERR_HW);
    }
}

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册I2C实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 I2CConfig 负责）
 */
BSP_Status_e I2CRegister(I2CInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_i2c_idx >= I2C_INSTANCE_NUM, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_i2c_idx; i++)
    {
        if (s_i2c_instance[i] == instance)
        {
            BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return BSP_PARAM_ERR;
        }
    }

    s_i2c_instance[s_i2c_idx++] = instance;

    BSPLOG(&g_i2c_log, LOG_LEVEL_INFO, "I2C instance registered, idx=%d", s_i2c_idx - 1);
    return BSP_OK;
}

/**
 * @brief 配置I2C实例（填充硬件映射 + 父指针 + 回调，可重复调用）
 * @note 要求先调用 I2CRegister 注册实例
 */
BSP_Status_e I2CConfig(I2CInstance *instance, const I2C_Config_s *config)
{
    uint8_t new_idx;
    uint8_t old_idx;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_e >= I2C_NUM_MAX, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "i2c_e out of range!"));

    new_idx = (uint8_t)config->i2c_e;

    BSP_RETURN_IF_TRUE_LOG(i2c_map[new_idx].handle == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "I2C handle is NULL, check bsp_map mapping!"));

    /* 同一 handle 不允许被两个实例占用：回调靠句柄反查实例，冲突则分发错乱
     * （同一总线上的多个从机共用一个实例，靠每次调用传 dev_addr 区分，见头文件） */
    for (uint8_t i = 0; i < s_i2c_idx; i++)
    {
        if (s_i2c_instance[i] == instance)
            continue;
        if (s_i2c_instance[i]->handle == i2c_map[new_idx].handle)
        {
            BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Same I2C handle already registered!");
            return BSP_PARAM_ERR;
        }
    }

    /* 重入收尾：清旧路由槽，并把上一次可能还挂着的传输收尾（否则旧传输会继续往缓冲里写、
     * 还会占着总线让新配置一笔都发不出去）。总线空闲时不做任何动作。 */
    if (instance->handle != NULL)
    {
        old_idx = I2C_Hi2cToIndex(instance->handle);
        if (old_idx < I2C_NUM_MAX && s_i2c_inst_by_i2c[old_idx] == instance)
        {
            s_i2c_inst_by_i2c[old_idx] = NULL;
        }
        if (!I2C_BusIsIdle(instance->handle))
        {
            /* 在途传输只能是 IT/DMA（BLOCK 在调用内同步跑完，跨不过一次 Config），
             * 故恒 notify=1：那笔的发起方正在等完成回调，不通知就永远等不到。 */
            I2C_AbortOnError(instance, "reconfig with in-flight transfer", 1);
        }
    }

    /* 填充枚举、硬件句柄、父指针与回调 */
    instance->i2c_e = config->i2c_e;
    instance->handle = i2c_map[new_idx].handle;
    instance->parent = config->parent;
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->err_callback = config->err_callback;
    instance->rx_len = 0;

    s_i2c_inst_by_i2c[new_idx] = instance;

    /* DMA 能力快照（供调试：该口有哪些 DMA，DMA 模式才可能用上） */
    s_i2c_status[new_idx].dma_tx_capable = (instance->handle->hdmatx != NULL) ? 1 : 0;
    s_i2c_status[new_idx].dma_rx_capable = (instance->handle->hdmarx != NULL) ? 1 : 0;
    s_i2c_status[new_idx].state = instance->handle->State;

    return BSP_OK;
}

BSP_Status_e I2CMemRead(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                        I2C_MemAddrSize_e mem_addr_size, uint16_t len,
                        BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;
    uint16_t hal_addr = 0; /* 编码后的从机地址（喂 HAL 用，见 I2C_EncodeDevAddr） */

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL || instance->rx_buff == NULL,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Handle is NULL, call I2CConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    /* 静默截断会让上层拿到"比请求短"的数据却毫无察觉，直接拒绝 */
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->buff_size,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid read len=%d (buff=%d)!",
                                  (int)len, (int)instance->buff_size));
    BSP_RETURN_IF_TRUE_LOG(mem_addr_size != I2C_MEM_ADDR_SIZE_8BIT && mem_addr_size != I2C_MEM_ADDR_SIZE_16BIT,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mem_addr_size=%d!", (int)mem_addr_size));
    BSP_RETURN_IF_TRUE_LOG(I2C_EncodeDevAddr(instance, dev_addr, &hal_addr) != BSP_OK,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid dev_addr=0x%X (addressing mode=0x%lX)!",
                                  (unsigned)dev_addr, (unsigned long)instance->handle->Init.AddressingMode));

    idx = I2C_Hi2cToIndex(instance->handle);

    if (!I2C_CheckDmaCapability(instance, mode, 0, 1))
    {
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    /* 先确认总线归我们，再登记现场：被拒的调用不碰在途那笔的 xfer_len / rx_buff */
    ret = I2C_ClaimBus(instance, mode, timeout_ms, "mem read busy timeout");
    if (ret != BSP_OK)
    {
        return I2C_FailThenRet(instance, ret);
    }

    I2C_SnapshotReq(idx, instance, hal_addr, mem_addr, mem_addr_size, len, mode);
    instance->rx_len = 0;

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_I2C_Mem_Read(instance->handle, hal_addr, mem_addr,
                                  I2C_ToHalMemAddrSize(mem_addr_size), instance->rx_buff, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_I2C_Mem_Read_IT(instance->handle, hal_addr, mem_addr,
                                     I2C_ToHalMemAddrSize(mem_addr_size), instance->rx_buff, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_I2C_Mem_Read_DMA(instance->handle, hal_addr, mem_addr,
                                      I2C_ToHalMemAddrSize(mem_addr_size), instance->rx_buff, len);
        break;
    default:
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
    {
        /* 启动失败不会有完成回调，必须由 I2C_StartFail 复位句柄并把失败抛给上层 */
        return I2C_StartFail(instance, hal_st, "mem read start failed", idx, mode);
    }

    if (mode == BSP_BLOCK_MODE)
    {
        instance->rx_len = len; /* 阻塞模式：返回即收满 */
        if (idx < I2C_NUM_MAX)
        {
            s_i2c_status[idx].rx_len = len;
            s_i2c_status[idx].rx_ok++;
        }
    }

    return BSP_OK;
}

BSP_Status_e I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                         I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                         BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;
    uint16_t hal_addr = 0;         /* 编码后的从机地址（喂 HAL 用，见 I2C_EncodeDevAddr） */
    uint8_t *tx = (uint8_t *)data; /* HAL 的 I2C 写接口形参是非 const 的 uint8_t*（两版头文件都一样），
                                    * 实现并不写这块内存；只在这里去一次 const，BSP 对外仍保持 const */

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Handle is NULL, call I2CConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Invalid write data/len!"));
    BSP_RETURN_IF_TRUE_LOG(mem_addr_size != I2C_MEM_ADDR_SIZE_8BIT && mem_addr_size != I2C_MEM_ADDR_SIZE_16BIT,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mem_addr_size=%d!", (int)mem_addr_size));
    BSP_RETURN_IF_TRUE_LOG(I2C_EncodeDevAddr(instance, dev_addr, &hal_addr) != BSP_OK,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid dev_addr=0x%X (addressing mode=0x%lX)!",
                                  (unsigned)dev_addr, (unsigned long)instance->handle->Init.AddressingMode));

    idx = I2C_Hi2cToIndex(instance->handle);

    if (!I2C_CheckDmaCapability(instance, mode, 1, 0))
    {
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    /* 先确认总线归我们，再登记现场：被拒的调用不碰在途那笔的 xfer_len / tx 缓冲 */
    ret = I2C_ClaimBus(instance, mode, timeout_ms, "mem write busy timeout");
    if (ret != BSP_OK)
    {
        return I2C_FailThenRet(instance, ret);
    }

    I2C_SnapshotReq(idx, instance, hal_addr, mem_addr, mem_addr_size, len, mode);
    instance->rx_len = 0; /* 写传输没有接收结果 */

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_I2C_Mem_Write(instance->handle, hal_addr, mem_addr,
                                   I2C_ToHalMemAddrSize(mem_addr_size), tx, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_I2C_Mem_Write_IT(instance->handle, hal_addr, mem_addr,
                                      I2C_ToHalMemAddrSize(mem_addr_size), tx, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_I2C_Mem_Write_DMA(instance->handle, hal_addr, mem_addr,
                                       I2C_ToHalMemAddrSize(mem_addr_size), tx, len);
        break;
    default:
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
    {
        return I2C_StartFail(instance, hal_st, "mem write start failed", idx, mode);
    }

    if (mode == BSP_BLOCK_MODE && idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].tx_ok++; /* 阻塞模式：返回即写完 */
    }

    return BSP_OK;
}

BSP_Status_e I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                               uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;
    uint16_t hal_addr = 0;         /* 编码后的从机地址（喂 HAL 用，见 I2C_EncodeDevAddr） */
    uint8_t *tx = (uint8_t *)data; /* 同 I2CMemWrite：HAL 写接口形参非 const，只在这里去一次 */

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Handle is NULL, call I2CConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Invalid transmit parameters!"));
    BSP_RETURN_IF_TRUE_LOG(I2C_EncodeDevAddr(instance, dev_addr, &hal_addr) != BSP_OK,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid dev_addr=0x%X (addressing mode=0x%lX)!",
                                  (unsigned)dev_addr, (unsigned long)instance->handle->Init.AddressingMode));

    idx = I2C_Hi2cToIndex(instance->handle);

    if (!I2C_CheckDmaCapability(instance, mode, 1, 0))
    {
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    /* 先确认总线归我们，再登记现场：被拒的调用不碰在途那笔的 xfer_len / tx 缓冲 */
    ret = I2C_ClaimBus(instance, mode, timeout_ms, "transmit busy timeout");
    if (ret != BSP_OK)
    {
        return I2C_FailThenRet(instance, ret);
    }

    I2C_SnapshotReq(idx, instance, hal_addr, 0, I2C_MEM_ADDR_SIZE_8BIT, len, mode);
    instance->rx_len = 0;

    /* 注意：从机地址自身占一笔"数据"，故 HAL 的 Size 就是 len（不含地址字节） */
    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_I2C_Master_Transmit(instance->handle, hal_addr, tx, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_I2C_Master_Transmit_IT(instance->handle, hal_addr, tx, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_I2C_Master_Transmit_DMA(instance->handle, hal_addr, tx, len);
        break;
    default:
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
    {
        return I2C_StartFail(instance, hal_st, "transmit start failed", idx, mode);
    }

    if (mode == BSP_BLOCK_MODE && idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].tx_ok++;
    }

    return BSP_OK;
}

BSP_Status_e I2CMasterReceive(I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                              BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;
    uint16_t hal_addr = 0; /* 编码后的从机地址（喂 HAL 用，见 I2C_EncodeDevAddr） */

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL || instance->rx_buff == NULL,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Handle is NULL, call I2CConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->buff_size,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid receive len=%d (buff=%d)!",
                                  (int)len, (int)instance->buff_size));
    BSP_RETURN_IF_TRUE_LOG(I2C_EncodeDevAddr(instance, dev_addr, &hal_addr) != BSP_OK,
                           I2C_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid dev_addr=0x%X (addressing mode=0x%lX)!",
                                  (unsigned)dev_addr, (unsigned long)instance->handle->Init.AddressingMode));

    idx = I2C_Hi2cToIndex(instance->handle);

    if (!I2C_CheckDmaCapability(instance, mode, 0, 1))
    {
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    /* 先确认总线归我们，再登记现场：被拒的调用不碰在途那笔的 xfer_len / rx_buff */
    ret = I2C_ClaimBus(instance, mode, timeout_ms, "receive busy timeout");
    if (ret != BSP_OK)
    {
        return I2C_FailThenRet(instance, ret);
    }

    I2C_SnapshotReq(idx, instance, hal_addr, 0, I2C_MEM_ADDR_SIZE_8BIT, len, mode);
    instance->rx_len = 0;

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_I2C_Master_Receive(instance->handle, hal_addr, instance->rx_buff, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_I2C_Master_Receive_IT(instance->handle, hal_addr, instance->rx_buff, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_I2C_Master_Receive_DMA(instance->handle, hal_addr, instance->rx_buff, len);
        break;
    default:
        return I2C_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
    {
        return I2C_StartFail(instance, hal_st, "receive start failed", idx, mode);
    }

    if (mode == BSP_BLOCK_MODE)
    {
        instance->rx_len = len; /* 阻塞模式：返回即收满 */
        if (idx < I2C_NUM_MAX)
        {
            s_i2c_status[idx].rx_len = len;
            s_i2c_status[idx].rx_ok++;
        }
    }

    return BSP_OK;
}

BSP_Status_e I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                              uint32_t timeout_ms)
{
    HAL_StatusTypeDef st;
    uint16_t hal_addr = 0; /* 编码后的从机地址（喂 HAL 用，见 I2C_EncodeDevAddr） */
    uint8_t idx;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "IsDeviceReady: invalid instance!"));
    BSP_RETURN_IF_TRUE_LOG(I2C_EncodeDevAddr(instance, dev_addr, &hal_addr) != BSP_OK, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "IsDeviceReady: invalid dev_addr=0x%X!",
                                  (unsigned)dev_addr));

    idx = I2C_Hi2cToIndex(instance->handle);

    /* HAL 内部是阻塞轮询（按 HAL_GetTick 计时），只能在任务上下文调用 */
    st = HAL_I2C_IsDeviceReady(instance->handle, hal_addr, trials, timeout_ms);

    /* 结论必须留痕：本函数的唯一调用者是恢复流程里那道"器件还在不在"的门禁
     * （drv_ist8310 的 IST8310_Recover），而现场排障最需要区分的恰恰是
     * "器件不应答"与"总线根本没让出来"这两条分支 —— 只看 recover_count 是看不出来的。
     * 参数错误不进计数：那是调用方的编程错误，已经打了错误日志。 */
    if (idx < I2C_NUM_MAX)
    {
        if (st == HAL_OK)
        {
            s_i2c_status[idx].probe_ok++;
        }
        else
        {
            s_i2c_status[idx].probe_fail++;
            if (st == HAL_BUSY)
            {
                s_i2c_status[idx].probe_busy++;
            }
        }
    }

    if (st == HAL_OK)
    {
        return BSP_OK;
    }
    /* HAL_BUSY = 总线被占用（入口判 State，或 BUSY 标志迟迟不落）：
     * 与"从机不应答"是两回事，上层据此可以区分"再等等"和"改去恢复" */
    return (st == HAL_BUSY) ? BSP_BUSY : BSP_HW_ERR;
}

BSP_Status_e I2CBusRecover(I2CInstance *instance)
{
    uint8_t idx;
    uint8_t busy_flag;
    uint8_t idle;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "BusRecover: invalid instance!"));

    I2C_HandleTypeDef *h = instance->handle;

    idx = I2C_Hi2cToIndex(h);

    /* ===== 入口自证：没有"总线级"证据就什么都不做（**必须在任何动作之前**）=====
     * 本函数是**总线级**动作（重建整个外设：时钟、GPIO、NVIC 全放掉再重配），
     * 而它的触发者（drv_ist8310）看到的是**实例级**现象——"我这个从机没应答"。
     * 证据的作用域必须与动作的作用域对齐：对端不发 / 从机挂了 / 只是赶上一次总线噪声，
     * 这三样都不在总线上，重建外设一个都治不好，代价却是把本总线上**别的实例**的在途传输
     * 一起打断。故判据必须在这里、由 bsp 自己读总线状态，而不是交给调用点。
     * 调用点只负责"何时看一眼"（drv_ist8310 的失败计数 + 冷却），bsp 负责"值不值得动手"。
     *
     * 判据（两条同时成立才动手）：
     *   ① `I2C_FLAG_BUSY` 置位 —— 外设说"总线还被占着"。这是唯一的总线级观测量；
     *      总线已经放开（NACK / 从机挂了）时它是 0，那种故障该由 DRV 的
     *      探测 + RSTN 脉冲 + 重初始化去救（见 bsp_i2c.md §2.B.1.1）。
     *      它也正是本函数末尾用来判"总线是否真被拉死"的那一位，前后读的是同一个事实。
     *   ② 本句柄空闲（`State == READY && Lock` 未锁）—— 说明"占着总线"的那一位**没有
     *      任何一笔在途传输在负责**：要么标志被闩住，要么从机把 SCL/SDA 拉着。
     *      反过来若句柄非空闲，那是**有一笔正常传输正在途**（正常传输同样让 BUSY 置位），
     *      此刻重建就是误伤——这一条不能省，也不能倒过来先看。
     *
     * 为什么不用别的判据：
     * - 只看"器件不应答"（调用点的现象）：实例级证据 → 总线健康时也会成立，
     *   等于把上面那些误伤全放进来。
     * - 加"卡住超过 N ms"的时长阈值：这里没有可靠的计时起点（`State` 由 HAL 改，
     *   本层看不到它何时被置忙），而且 `I2C_ResetHandle` 每次失败都会把 `State` 复位，
     *   真要计时就得再引入一份按外设的时间戳 —— 为一条没有观测支撑的判据增状态，不值。
     *   现有两条判据已能把"总线真卡住"与"NACK 类失败"分开（前者 BUSY 置位且无人负责）。
     * - 用 `ErrorCode` 判：它是**上一笔**传输的结果，而上一笔早被 `I2C_AbortOnError`
     *   复位过了，读到的多半是 0，不代表当前总线状态。
     */
    busy_flag = (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET) ? 1 : 0;
    idle = I2C_BusIsIdle(h);
    if (!busy_flag || !idle || !I2C_CanBlockingAbort())
    {
        /* 不做任何事。上下文与两条判据共用这一个出口：对调用方而言返回值语义相同
         * （BSP_BUSY = 未做任何事），分开只会让调用点多出几个永远不看的返回值。 */
        if (idx < I2C_NUM_MAX)
        {
            s_i2c_status[idx].bus_recover_skip++;
            s_i2c_status[idx].state = h->State;
            s_i2c_status[idx].error_code = h->ErrorCode;
            s_i2c_status[idx].lock = h->Lock;
        }
        return BSP_BUSY;
    }

    if (idx < I2C_NUM_MAX)
    {
        s_i2c_status[idx].bus_recover++;
    }

    BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING,
           "Bus recover start (i2c_e=%d), busy_flag=%d, state=0x%02X, err=0x%lX",
           (int)instance->i2c_e, (int)busy_flag, (unsigned)h->State, (unsigned long)h->ErrorCode);

    /* 1) 先让外设静默 + 解锁 + 清状态（Lock 残留会让后续 HAL 调用直接 HAL_BUSY） */
    I2C_ResetHandle(instance);

    /* 2) 外设级重建（与 I2C_AbortOnError 的兜底共用一份实现，见 I2C_RebuildPeriph） */
    I2C_RebuildPeriph(instance);

    /* 3) 兜底复位（Init 正常时已复位，防 Init 失败留下脏状态） */
    I2C_ResetHandle(instance);

    /* 4) 再读一次**同一个** BUSY 标志：入口是"它置位才动手"，这里是"动完手它还置位吗"。
     *    仍置位 = 重建没能让总线松手，多半是从机真把 SCL/SDA 拉着（本层不翻转引脚，
     *    见 §2.B.1 的取舍），由 DRV 记 ERROR 并走 RSTN 脉冲 / 冷却重试。 */
    busy_flag = (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET) ? 1 : 0;
    BSPLOG(&g_i2c_log, busy_flag ? LOG_LEVEL_ERROR : LOG_LEVEL_INFO,
           "Bus recover done, busy=%d (1 = 引脚被从机拉死，需从机侧复位)", (int)busy_flag);
    return busy_flag ? BSP_HW_ERR : BSP_OK;
}

#endif /* I2C_INSTANCE_NUM > 0 */

#endif /* BSP_I2C_USED */
