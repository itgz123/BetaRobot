/**
 * @file bsp_spi.c
 * @brief SPI驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置和片选控制。
 *
 * @note 本层职责（对照 bsp_usart / bsp_can）：
 *       ① 收发（三模式，模式每次调用传参）；
 *       ② 必要回调（TxRxCplt / RxCplt / TxCplt / Error）并分发到实例；
 *       ③ 纠正错误（总线卡死时强制中止并复位 HAL 状态）；
 *       ④ 计数与状态快照存 static 结构体供调试器 Watch（`s_spi_status`）。
 *       除此之外不提供任何接口：不做发送队列/缓冲（忙即 BSP_BUSY，多缓冲归上层），
 *       不对外暴露 work_mode / is_ready。卡死自恢复有一个对外入口
 *       （SPIRecoverTxIfStuck），只把"何时看一眼"交给上层，判据与动作都在这层
 *       —— 这也是本模块不需要"入口自证"的原因：判据（这个句柄非 READY 超时）与动作
 *       （中止这个句柄的传输）作用域天然相同，详见 SPIRecoverTxIfStuck 的说明。
 */

#include "bsp_spi.h"
#include "app_cfg.h"

#ifdef BSP_SPI_USED

#if SPI_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_log.h"

/*------------- 私有变量 --------------*/
static uint8_t s_spi_idx = 0;
#ifndef BSP_SPI_LOG_LIMIT
#define BSP_SPI_LOG_LIMIT 10
#endif                                                     // !BSP_SPI_LOG_LIMIT
LOG_INSTANCE_DEF(g_spi_log, "bsp_spi", BSP_SPI_LOG_LIMIT); /* SPI 日志实例 */

/* 传输"卡死"判定阈值（ms）：State 连续非 READY 超过该时长即强制复位。
 * 取值远大于最长合法帧的传输时间（8 字节 @3MHz 约 21µs，BMI088 的 DMA 传输），
 * 又要在 DRDY 中断（≥400Hz）漏掉几拍之内就把总线救回来，故取 20ms。 */
#ifndef SPI_TX_STUCK_TIMEOUT_MS
#define SPI_TX_STUCK_TIMEOUT_MS 20
#endif

static SPIInstance *s_spi_instance[SPI_INSTANCE_NUM] = {NULL};

/* 句柄索引 → 实例路由表（Config 时登记）。回调里先按索引取实例，
 * 索引本身用于状态统计——即使实例路由缺失，错误/事件也不漏计。
 * 不做"最近命中缓存"：spi_map 只有几个表项，线性查找的成本远低于缓存失效的排查成本
 * （实例重配后缓存会指向旧实例）。 */
static SPIInstance *s_spi_inst_by_spi[SPI_NUM_MAX] = {NULL};

/* 每个 SPI 最近一次观察到 State == READY 的时刻（DWT 微秒）：用于判定传输是否卡死。
 * 0 表示尚未采样过（Config 之前不判卡死）。刷新点：SPIConfig、SPI_WaitReady 的就绪路径、
 * 以及自恢复入口 SPIRecoverTxIfStuck —— 它每次发现"总线空闲"都刷新一次基准，
 * 保证基准不会陈旧到让下一次正常传输一上来就被误判成卡死。 */
static uint64_t s_spi_ready_us[SPI_NUM_MAX] = {0};

/*------------- SPI 状态/错误统计（调试用，调试器直接 Watch s_spi_status） --------------*/

/**
 * @brief SPI 外设状态与错误统计（每 SPI 一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：收发完成回调、错误回调、收发接口的失败返回，
 *       外加便于判断"当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;      /* 传输成功完成次数（BLOCK 直接成功 + IT/DMA 的 TxCplt/TxRxCplt） */
    uint32_t tx_fail;    /* 三个收发接口返回非 BSP_OK 的总次数 */
    uint32_t tx_busy;    /* 其中 BSP_BUSY（总线忙，上层应排队重试） */
    uint32_t tx_timeout; /* 其中 BSP_TIMEOUT（已尝试复位卡死状态） */
    uint32_t rx_ok;      /* 收到数据的次数（BLOCK 收满 + IT/DMA 的 RxCplt/TxRxCplt） */
    /* 错误分类计数（可同时置位，独立 if） */
    uint32_t err_total;  /* 错误回调总次数 */
    uint32_t err_modf;   /* 模式错误（片选/时序） */
    uint32_t err_crc;    /* CRC 错误 */
    uint32_t err_ovr;    /* 溢出错误（收得比取走快） */
    uint32_t err_fre;    /* TI 模式帧错误 */
    uint32_t err_dma;    /* DMA 传输错误 */
    uint32_t err_flag;   /* RXP/TXP/DXP/BSY 等标志位错误 */
    uint32_t err_other;  /* 其它错误位（两版 HAL 的私有位：UDR/TIMEOUT/NOT_SUPPORTED…） */
    uint32_t err_no_dma; /* 调用 DMA 模式但该口无对应 DMA（hdmatx/hdmarx 为 NULL） */
    uint32_t err_start;  /* HAL 启动传输失败（非忙/超时）次数 */
    /* 自恢复计数 */
    uint32_t tx_recover; /* 卡死传输被强制复位次数（复位后即可重新发起） */
    /* 实时快照 */
    uint8_t state;          /* HAL State（最近一次采样） */
    uint8_t last_mode;      /* 最近一次传输模式 */
    uint32_t error_code;    /* 最近一次 hspi->ErrorCode */
    uint64_t err_time_us;   /* 最近一次错误时间（DWT 微秒） */
    uint8_t dma_tx_capable; /* Config 时记录：该口是否有 TX DMA */
    uint8_t dma_rx_capable; /* Config 时记录：该口是否有 RX DMA */
    uint8_t dma_tx_state;   /* 最近一次启动失败时 hdmatx->State（0xFF = 该口无 TX DMA） */
    uint8_t dma_rx_state;   /* 最近一次启动失败时 hdmarx->State（0xFF = 该口无 RX DMA） */
    uint16_t tx_len;        /* 最近一次传输长度 */
    uint16_t rx_len;        /* 最近一次接收长度 */
} SPI_Status_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile SPI_Status_s s_spi_status[SPI_NUM_MAX];

/* 两个 HAL 版本共有、且判据一致的错误位（F4 的 0x80 是 INVALID_CALLBACK，
 * H7 的 0x80 是 UDR —— 位域不同，故只能用交集做分类掩码，其余一律进 err_other） */
#define SPI_ERROR_COMMON_MASK                                               \
    (uint32_t)(HAL_SPI_ERROR_MODF | HAL_SPI_ERROR_CRC | HAL_SPI_ERROR_OVR | \
               HAL_SPI_ERROR_FRE | HAL_SPI_ERROR_DMA | HAL_SPI_ERROR_FLAG | \
               HAL_SPI_ERROR_ABORT)

/*------------- 私有函数：查表与计数 --------------*/

/**
 * @brief hspi → 板载 SPI 索引（SPI_LCD_1/SPI_BMI088/... 对应 spi_map 下标）
 * @retval SPI_NUM_MAX 未找到
 */
static uint8_t SPI_HspiToIndex(const SPI_HandleTypeDef *hspi)
{
    uint8_t i;

    if (hspi == NULL)
        return SPI_NUM_MAX;

    for (i = 0; i < SPI_NUM_MAX; i++)
    {
        if (spi_map[i].handle == hspi)
            return i;
    }
    return SPI_NUM_MAX;
}

/**
 * @brief 收发接口失败路径统一计数后返回状态码
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏内 `return (ret)` 会求值），
 *       使所有失败返回点都统计进 s_spi_status[]，日志行为不变。
 */
static BSP_Status_e SPI_FailThenRet(const SPIInstance *instance, BSP_Status_e status)
{
    uint8_t idx = (instance != NULL) ? SPI_HspiToIndex(instance->handle) : SPI_NUM_MAX;

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].tx_fail++;
        if (status == BSP_BUSY)
            s_spi_status[idx].tx_busy++;
        else if (status == BSP_TIMEOUT)
            s_spi_status[idx].tx_timeout++;
    }
    return status;
}

/**
 * @brief 启动失败的现场快照 + 日志（三个收发接口共用）
 * @param instance SPI 实例
 * @param what     动作名（"transmit" / "receive" / "transmit/receive"）
 * @param mode     本次传输模式
 * @param idx      SPI 下标（SPI_NUM_MAX = 未登记，只打日志不计数）
 *
 * @note 失败现场必须一起打出来才能定位：这类失败几乎总是"某一路 DMA 流还没释放"
 *       （见 SPI_BusIsIdle / SPI_ForceReleaseDma），只知道 `spi_e` 与 `mode`
 *       根本分不清是哪一路、卡在哪个状态。
 *       dma 状态取 `HAL_DMA_STATE_*`，**编号随 HAL 版本不同**：
 *       关键只看 `1`=READY（空闲）与 `2`=BUSY（在途）；
 *       其余为异常/终止态 —— F4 是 3=TIMEOUT / 4=ERROR / 5=ABORT，
 *       H7 是 3=ERROR / 4=ABORT（见两版 `stm32*xx_hal_dma.h` 的 StateTypeDef）。
 *       `-1` 表示该口压根没有这一路 DMA。
 * @note 只有**真失败**才走到这里（`HAL_BUSY` 是流控，见 SPI_StartFail），
 *       因此日志里必然带着 `err=0x...`：见到 `err=0x0` 就不是本函数打的。
 */
static void SPI_LogStartFail(const SPIInstance *instance, const char *what,
                             BSP_Transfer_Mode_e mode, uint8_t idx)
{
    SPI_HandleTypeDef *hspi = instance->handle;
    int tx_state = (hspi->hdmatx != NULL) ? (int)hspi->hdmatx->State : -1;
    int rx_state = (hspi->hdmarx != NULL) ? (int)hspi->hdmarx->State : -1;
    /* BLOCK 是"跑了但没成功"（HAL 返回 TIMEOUT/ERROR），IT/DMA 才是"没启动起来" */
    const char *phase = (mode == BSP_BLOCK_MODE) ? "failed" : "start failed";

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].err_start++;
        s_spi_status[idx].state = hspi->State;
        s_spi_status[idx].error_code = hspi->ErrorCode;
        s_spi_status[idx].err_time_us = DWT_GetTimeUs();
        s_spi_status[idx].dma_tx_state = (tx_state < 0) ? 0xFF : (uint8_t)tx_state;
        s_spi_status[idx].dma_rx_state = (rx_state < 0) ? 0xFF : (uint8_t)rx_state;
    }

    BSPLOG(&g_spi_log, LOG_LEVEL_WARNING,
           "SPI %s %s (spi_e=%d, mode=%d, spi=%d, tx_dma=%d, rx_dma=%d, err=0x%lX)!",
           what, phase, (int)instance->spi_e, (int)mode, (int)hspi->State, tx_state, rx_state,
           (unsigned long)hspi->ErrorCode);
}

/**
 * @brief 启动结果分类：HAL 报"忙"是流控信号，不是故障
 * @param instance SPI 实例
 * @param st       HAL 启动调用的返回（三个收发接口的 BLOCK / IT / DMA 分支）
 * @param what     动作名（透传给 SPI_LogStartFail）
 * @param mode     本次传输模式
 * @param idx      SPI 下标
 * @retval BSP_BUSY    `HAL_BUSY`：此刻总线不可用（两版 HAL 的入口判据都是
 *                     "State 非 READY 即 HAL_BUSY"，且此时**不动 State、不置 ErrorCode**）。
 *                     计入 `tx_busy`，不打日志、不计 `err_start` —— 上层下一拍/下一帧重试即可
 * @retval BSP_HW_ERR  真失败：HAL 已置 `ErrorCode`（DMA 流没释放、MODF/OVR 等）或 BLOCK 超时
 *
 * @note 为什么必须分开：BLOCK 模式**不做**就绪预判（`SPI_WaitReady` 只用于 IT/DMA，
 *       HAL 阻塞版自己会等），于是"总线此刻正被 ISR 占用时的一次 BLOCK 调用"必然拿到
 *       `HAL_BUSY`。若把它记成 `err_start` 并打 `start failed` 告警，就会在日志里复刻
 *       A.6 那条假象（`err=0x0` 的 start failed）、把排查引向错误方向；而真失败必然
 *       带着 `ErrorCode`，两者的区分度是干净的。
 * @note IT/DMA 也会走到这里：`SPI_WaitReady` 判完就绪到真正调用 HAL 之间仍有极小的窗口
 *       （此刻被更高优先级的中断抢进来发起传输）。同样按"忙"上报，让上层重试，
 *       而不是把一次可重试的争用记成硬件故障。
 */
static BSP_Status_e SPI_StartFail(SPIInstance *instance, HAL_StatusTypeDef st, const char *what,
                                  BSP_Transfer_Mode_e mode, uint8_t idx)
{
    if (st == HAL_BUSY)
        return SPI_FailThenRet(instance, BSP_BUSY);

    SPI_LogStartFail(instance, what, mode, idx);
    return SPI_FailThenRet(instance, BSP_HW_ERR);
}

/*------------- 私有函数：上下文判定 --------------*/

/**
 * @brief 当前上下文能否安全执行"按 HAL_GetTick 自旋"的中止动作（HAL_SPI_Abort → HAL_DMA_Abort）
 * @retval 1 可以（普通任务上下文）
 * @retval 0 不可以（死等风险，只能改期）
 *
 * @note 两类不安全上下文都要查（共同后果是 `HAL_GetTick` 冻住）：
 *       ① 中断上下文（`IPSR != 0`）——BMI088 的 INT 模式就是在 DRDY EXTI 里发起传输。
 *          HAL tick 源在本工程是 TIM 不是 SysTick（DJI_C 的 TIM14、其余板 TIM23，
 *          优先级 5/15），优先级数值不小于外设中断的 5，抢占不了本 ISR，故中断里 tick 不前进；
 *       ② 临界区——`taskENTER_CRITICAL` 抬的是 BASEPRI（FreeRTOS ARM_CM4F/CM7 端口的
 *          `portDISABLE_INTERRUPTS` = `vPortRaiseBASEPRI`），tick 源同样进不来。
 * @note HAL_SPI_Abort 内部对 DMA 流的收尾调的是阻塞版 HAL_DMA_Abort：它按 HAL_GetTick
 *       等"流真的停下来"，tick 不前进就是死等（F4 版 HAL_SPI_Abort 自带的计数器轮询也一样
 *       要等 DMA 停）。故判定为 0 时一律不动作、也不假复位（假复位会让下一次 HAL 启动
 *       直接错过真实的 BUSY）：判据侧不刷新时间戳，下一次任务上下文的调用会照样命中并补做。
 */
static inline uint8_t SPI_CanBlockingAbort(void)
{
    return (__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U);
}

/*------------- 私有函数：DMA 流状态判定 --------------*/

/**
 * @brief 判定一个 DMA 流是否真正空闲（可以重新启动）
 * @param hdma DMA 流句柄
 * @retval 1 空闲
 * @retval 0 仍在收尾 / 状态未知
 *
 * @note 为什么还要查 Lock：HAL 的 DMA 启动用的是"启动时上锁、完成中断里解锁"的成对约定
 *       （`HAL_DMA_Start_IT` 的成功路径不 UNLOCK，由 `HAL_DMA_IRQHandler` 在完成中断里
 *       `__HAL_UNLOCK`）。因此"State 已 READY 但 Lock 仍是 LOCKED"是真实存在的中间态：
 *       下一次 `HAL_DMA_Start_IT` 会在函数开头的 `__HAL_LOCK` 上直接返回 `HAL_BUSY`。
 * @note RESET 也算空闲：CubeMX 生成的流在 `HAL_DMA_Init` 后是 READY，但没被用过的流
 *       可能是 RESET（初值 0），把它当"忙"会让整条链路再也发不出去。
 */
static uint8_t SPI_DmaIdle(const DMA_HandleTypeDef *hdma)
{
    if (hdma->Lock != HAL_UNLOCKED)
        return 0;

    return (hdma->State == HAL_DMA_STATE_READY || hdma->State == HAL_DMA_STATE_RESET) ? 1 : 0;
}

/**
 * @brief 判定这条 SPI 当前能否发起新传输（外设 + 两路 DMA 流都空闲）
 * @param hspi SPI 句柄
 * @retval 1 可以发起
 * @retval 0 忙
 *
 * @note 只判 `hspi->State` 是不够的，实机踩过：`HAL_SPI_TransmitReceive_DMA` 是**先启动
 *       RX 流、再启动 TX 流**，且把整个收尾放在 **RX 完成回调**里（TX 流的 XferCpltCallback
 *       被 HAL 显式置为 NULL，源码注释："the communication closing is performed in DMA
 *       reception complete callback"）。于是"上层收到 rx_callback、以为总线空了"与
 *       "TX 流的完成中断真正跑完"之间有一个窗口 —— 两个 DMA 流中断同优先级（DJI_C 上都是
 *       5,0）不能互相抢占，NVIC 号小的 RX(DMA2_Stream0, 56) 先被服务，TX(DMA2_Stream3, 59)
 *       的收尾可能排在它后面。
 *       窗口内发起新传输，HAL 会在 `HAL_DMA_Start_IT` 上拿到 `HAL_BUSY`，然后
 *       `SET_BIT(ErrorCode, HAL_SPI_ERROR_DMA)` 直接返回 HAL_ERROR，**不复位 `hspi->State`**
 *       —— 总线被自己锁死到下一次 `SPIRecoverTxIfStuck`（20ms 后）才复位，中间每一拍采样
 *       都白白丢掉。这里提前判掉，改成 `BSP_BUSY` 让上层下一拍重试：状态始终干净，也不丢帧。
 */
static uint8_t SPI_BusIsIdle(const SPI_HandleTypeDef *hspi)
{
    if (hspi->State != HAL_SPI_STATE_READY)
        return 0;

    if (hspi->hdmatx != NULL && !SPI_DmaIdle(hspi->hdmatx))
        return 0;

    if (hspi->hdmarx != NULL && !SPI_DmaIdle(hspi->hdmarx))
        return 0;

    return 1;
}

/*------------- 私有函数：卡死纠正 --------------*/

/**
 * @brief 兜底：把仍处于忙态的 DMA 流强制停掉（只在任务上下文调用）
 * @param hspi SPI 句柄
 *
 * @note 为什么 `HAL_SPI_Abort` 之外还需要它：`HAL_SPI_Abort` 只 abort **当前 CR2 里
 *       DMA 请求已置位**的那几路流。而 `HAL_SPI_TransmitReceive_DMA` 是"先启动 RX 流、
 *       置 RXDMAEN，再启动 TX 流"，`TXDMAEN` 要到函数最末尾才置位 —— 一旦 **TX 流启动
 *       失败**（正是实机那种 `HAL_SPI_ERROR_DMA`），TXDMAEN 根本没置位，Abort 就会跳过
 *       TX 流。结果是：句柄表面上复位成功（State 写回 READY），下一笔仍然在
 *       `HAL_DMA_Start_IT(hdmatx)` 上失败，形成"每 20ms 自恢复一次、永远发不出去"的循环。
 * @note `HAL_DMA_Abort` 会 disable 流并等 EN 清零，是任务上下文安全的阻塞调用
 *       （由 SPI_CanBlockingAbort 保证只在任务上下文进入）。
 */
static void SPI_ForceReleaseDma(SPI_HandleTypeDef *hspi)
{
    if (hspi->hdmatx != NULL && !SPI_DmaIdle(hspi->hdmatx))
    {
        (void)HAL_DMA_Abort(hspi->hdmatx);
        __HAL_DMA_DISABLE(hspi->hdmatx); /* Abort 未等到 EN 落时的兜底（写 EN=0 总是安全的） */
        __HAL_UNLOCK(hspi->hdmatx);
        hspi->hdmatx->State = HAL_DMA_STATE_READY;
        hspi->hdmatx->ErrorCode = HAL_DMA_ERROR_NONE;
    }

    if (hspi->hdmarx != NULL && !SPI_DmaIdle(hspi->hdmarx))
    {
        (void)HAL_DMA_Abort(hspi->hdmarx);
        __HAL_DMA_DISABLE(hspi->hdmarx);
        __HAL_UNLOCK(hspi->hdmarx);
        hspi->hdmarx->State = HAL_DMA_STATE_READY;
        hspi->hdmarx->ErrorCode = HAL_DMA_ERROR_NONE;
    }
}

/**
 * @brief 强制中止传输并复位 HAL 状态（只在任务上下文调用）
 * @param instance SPI 实例
 * @retval 1 已中止（err_callback 已按契约通知）
 * @retval 0 未动作（上下文不允许阻塞 Abort，或参数不合法）
 *
 * @note 为什么需要：HAL 的 DMA 启动失败分支只置 ErrorCode、不复位 hspi->State。
 *       典型如 HAL_SPI_TransmitReceive_DMA 中 HAL_DMA_Start_IT 返回 HAL_BUSY（另一路
 *       DMA 流的完成中断尚未执行，State 还是 BUSY）时，直接
 *       `SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_DMA); return HAL_ERROR;`，句柄被永久
 *       留在 HAL_SPI_STATE_BUSY_TX_RX。此后每次传输都拿不到总线，DRV 等不到完成回调、
 *       自己的 transfer_busy 永远清不掉 → 整个从机失联且看不到任何错误。
 *
 * @note 中止必须用 HAL_SPI_Abort，**不能用 HAL_SPI_DMAStop**：H7 版的 DMAStop 是空实现，
 *       只置 HAL_SPI_ERROR_NOT_SUPPORTED 就返回 HAL_ERROR（见 stm32h7xx_hal_spi.c），
 *       调它等于没调（旧版正是因此只能靠手写 `State = READY` 兜底）。HAL_SPI_Abort 两版
 *       都实现且都会把 DMA 流与状态一起收拾干净。
 * @note 后面的强制写 State 只是兜底：HAL_SPI_Abort 失败（如句柄被 __HAL_LOCK 锁住返回
 *       HAL_BUSY）或走了不复位 State 的分支时，不写回 READY 就等于什么都没修。
 * @note 被中止的那次传输不会再有完成回调，所以按 err_callback 契约通知上层复位自身状态。
 *       提前通知会出问题：上层的归还/复位判据普遍是"State == READY 才说明 HAL 已收尾"，
 *       故顺序必须是 Abort + 复位 State **之后**再通知（见 bsp_usart 的同款约定）。
 */
static uint8_t SPI_RecoverTx(SPIInstance *instance)
{
    uint8_t idx;

    if (instance == NULL || instance->handle == NULL || !SPI_CanBlockingAbort())
        return 0;

    idx = SPI_HspiToIndex(instance->handle);

    /* 先留存现场再中止：HAL_SPI_Abort 成功时会把 ErrorCode 清零，那是判断"为什么卡住"
     * 的唯一直观依据（例如 HAL_SPI_ERROR_DMA = DMA 流没释放） */
    if (idx < SPI_NUM_MAX)
        s_spi_status[idx].error_code = instance->handle->ErrorCode;

    (void)HAL_SPI_Abort(instance->handle);
    if (instance->handle->State != HAL_SPI_STATE_READY)
        instance->handle->State = HAL_SPI_STATE_READY; /* Abort 未复位状态时兜底 */

    /* Abort 够不到的 DMA 流（典型：TxRx 模式里 TX 流启动失败，TXDMAEN 从未置位） */
    SPI_ForceReleaseDma(instance->handle);

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].tx_recover++;
        s_spi_status[idx].state = instance->handle->State;
    }

    if (instance->err_callback != NULL)
        instance->err_callback(instance, SPI_ERR_ABORT); /* 被中止的传输不会再有完成回调 */

    return 1;
}

/*------------- 私有函数：就绪等待 --------------*/

/**
 * @brief IT/DMA 模式等总线就绪（BLOCK 模式不做预判，HAL 阻塞版自己会等）
 * @param instance   SPI 实例
 * @param timeout_ms 等待超时（ms）；0 = 只判一次，忙即返回
 * @retval BSP_OK      总线就绪（顺带刷新卡死计时基准）
 * @retval BSP_BUSY    忙（timeout_ms == 0）
 * @retval BSP_TIMEOUT 等待超时（任务上下文下已强制中止卡死的上一次传输）
 *
 * @note 旧版这里在 timeout_ms == 0 时会**死循环**（`while (State != READY)` 里的超时判据
 *       被 `timeout_ms > 0` 短路，永远不成立），而 INT 模式下这个调用恰恰来自 DRDY 中断 ——
 *       一旦总线卡死就是中断里永久死等。现在统一为"0 即不等待"：立刻 BSP_BUSY。
 * @note "就绪"的判据是 SPI_BusIsIdle（外设 + 两路 DMA 流），不是单看 `hspi->State`：
 *       上一笔的 DMA 收尾可能比完成回调晚一拍，此时让 HAL 去启动只会拿到 ErrorCode 与
 *       一个被锁死的句柄（见 SPI_BusIsIdle 的说明）。
 */
static BSP_Status_e SPI_WaitReady(SPIInstance *instance, uint32_t timeout_ms)
{
    uint8_t idx = SPI_HspiToIndex(instance->handle);

    if (SPI_BusIsIdle(instance->handle))
    {
        if (idx < SPI_NUM_MAX)
            s_spi_ready_us[idx] = DWT_GetTimeUs(); /* 采样：总线空闲时刻 */
        return BSP_OK;
    }

    if (timeout_ms == 0)
        return BSP_BUSY;

    {
        BSP_Timeout_s t;

        BSP_TimeoutStart(&t, timeout_ms);
        while (!SPI_BusIsIdle(instance->handle))
        {
            if (BSP_TimeoutExpired(&t))
            {
                /* 上层愿意等却等不到 = 传输卡死；能复位就复位（任务上下文），
                 * 上下文不允许 Abort（中断/临界区）时如实报出"没复位"，不静默 */
                if (SPI_RecoverTx(instance))
                    BSPLOG(&g_spi_log, LOG_LEVEL_WARNING, "SPI busy timeout, state reset (spi_e=%d)!",
                           (int)instance->spi_e);
                else
                    BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "SPI busy timeout, reset skipped (ISR/critical) (spi_e=%d)!",
                           (int)instance->spi_e);
                return BSP_TIMEOUT;
            }
        }
    }

    if (idx < SPI_NUM_MAX)
        s_spi_ready_us[idx] = DWT_GetTimeUs();
    return BSP_OK;
}

/**
 * @brief DMA 能力校验（该口没配对应方向的 DMA 就明确拒绝，而不是让 HAL 静默失败）
 * @param instance SPI 实例
 * @param need_tx  本次传输是否需要 TX DMA
 * @param need_rx  本次传输是否需要 RX DMA
 * @retval 1 可用（或不是 DMA 模式，无需校验）
 * @retval 0 不可用（已计数并打日志）
 */
static uint8_t SPI_CheckDmaCapability(SPIInstance *instance, BSP_Transfer_Mode_e mode,
                                      uint8_t need_tx, uint8_t need_rx)
{
    if (mode != BSP_DMA_MODE)
        return 1;

    if ((need_tx && instance->handle->hdmatx == NULL) ||
        (need_rx && instance->handle->hdmarx == NULL))
    {
        uint8_t idx = SPI_HspiToIndex(instance->handle);

        if (idx < SPI_NUM_MAX)
            s_spi_status[idx].err_no_dma++;
        BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "SPI DMA not available (spi_e=%d, tx=%d, rx=%d)!",
               (int)instance->spi_e, need_tx, need_rx);
        return 0;
    }
    return 1;
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief SPI全双工收发完成回调（IT/DMA）
 * @param hspi 发生中断的SPI句柄
 * @note 长度取 hspi->RxXferSize：HAL 在启动时写入、传输期间只递减 RxXferCount，
 *       因此回调里它仍是本次请求的长度（复用 handle 的现成信息，不必在实例里再存一份）。
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    uint8_t idx = SPI_HspiToIndex(hspi);
    SPIInstance *instance;

    if (idx >= SPI_NUM_MAX)
        return;

    s_spi_status[idx].tx_ok++;
    s_spi_status[idx].rx_ok++;
    s_spi_status[idx].state = hspi->State;
    s_spi_status[idx].rx_len = hspi->RxXferSize;

    instance = s_spi_inst_by_spi[idx];
    if (instance == NULL)
        return;

    instance->rx_len = hspi->RxXferSize;

    if (instance->rx_callback != NULL)
        instance->rx_callback(instance);
}

/**
 * @brief SPI接收完成回调（IT/DMA）
 * @param hspi 发生中断的SPI句柄
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    uint8_t idx = SPI_HspiToIndex(hspi);
    SPIInstance *instance;

    if (idx >= SPI_NUM_MAX)
        return;

    s_spi_status[idx].rx_ok++;
    s_spi_status[idx].state = hspi->State;
    s_spi_status[idx].rx_len = hspi->RxXferSize;

    instance = s_spi_inst_by_spi[idx];
    if (instance == NULL)
        return;

    instance->rx_len = hspi->RxXferSize;

    if (instance->rx_callback != NULL)
        instance->rx_callback(instance);
}

/**
 * @brief SPI发送完成回调（IT/DMA，只发不收）
 * @param hspi 发生中断的SPI句柄
 * @note 与旧版不同：不再把它并入 rx_callback。纯发送没有接收结果，混在一起会让上层
 *       分不清"收完了"和"发完了"（旧版还顺带把 rx_len 置 0，语义更乱）。本工程只发不收的
 *       SPI 实例目前不存在，此改动不影响 BMI088。
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    uint8_t idx = SPI_HspiToIndex(hspi);
    SPIInstance *instance;

    if (idx >= SPI_NUM_MAX)
        return;

    s_spi_status[idx].tx_ok++;
    s_spi_status[idx].state = hspi->State;

    instance = s_spi_inst_by_spi[idx];
    if (instance != NULL && instance->tx_callback != NULL)
        instance->tx_callback(instance);
}

/**
 * @brief SPI错误回调（ISR 上下文）
 * @param hspi 发生错误的SPI句柄
 * @note 本回调内完成分类计数 + 状态快照 + 通知上层。**不在这里做中止收尾**：
 *       HAL 的几条错误路径（SPI_ITError / SPI_DMAError / EndRxTxTransaction 失败）自身
 *       都已停掉传输并把 State 复位为 READY，而在 ISR 里再调 HAL_SPI_Abort 会经
 *       HAL_DMA_Abort 等 HAL tick —— 中断里 tick 不前进，直接死等。
 *       万一 HAL 没复位干净，下一次收发会卡在"等就绪"并触发 SPIRecoverTxIfStuck（任务上下文）。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    uint8_t idx = SPI_HspiToIndex(hspi);
    SPIInstance *instance;
    uint32_t error_code;

    if (idx >= SPI_NUM_MAX)
        return;

    error_code = hspi->ErrorCode;
    instance = s_spi_inst_by_spi[idx];

    /* ① 分类计数（多种错误可同时置位，必须独立 if，else-if 会漏计） */
    s_spi_status[idx].err_total++;
    if (error_code & HAL_SPI_ERROR_MODF)
        s_spi_status[idx].err_modf++;
    if (error_code & HAL_SPI_ERROR_CRC)
        s_spi_status[idx].err_crc++;
    if (error_code & HAL_SPI_ERROR_OVR)
        s_spi_status[idx].err_ovr++;
    if (error_code & HAL_SPI_ERROR_FRE)
        s_spi_status[idx].err_fre++;
    if (error_code & HAL_SPI_ERROR_DMA)
        s_spi_status[idx].err_dma++;
    if (error_code & HAL_SPI_ERROR_FLAG)
        s_spi_status[idx].err_flag++;
    if (error_code & ~SPI_ERROR_COMMON_MASK)
        s_spi_status[idx].err_other++;

    /* ② 状态快照 */
    s_spi_status[idx].error_code = error_code;
    s_spi_status[idx].state = hspi->State;
    s_spi_status[idx].err_time_us = DWT_GetTimeUs();

    /* ③ 上报（保留解码日志，便于直接看出是哪种错） */
    BSPLOG(&g_spi_log, LOG_LEVEL_WARNING, "Error detected, code=0x%lX (MODF:%d OVR:%d FRE:%d DMA:%d)",
           (unsigned long)error_code,
           (error_code & HAL_SPI_ERROR_MODF) ? 1 : 0,
           (error_code & HAL_SPI_ERROR_OVR) ? 1 : 0,
           (error_code & HAL_SPI_ERROR_FRE) ? 1 : 0,
           (error_code & HAL_SPI_ERROR_DMA) ? 1 : 0);

    if (instance != NULL && instance->err_callback != NULL)
        instance->err_callback(instance, SPI_ERR_HW);
}

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册SPI实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 SPIConfig 负责）
 */
BSP_Status_e SPIRegister(SPIInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_spi_idx >= SPI_INSTANCE_NUM, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_spi_idx; i++)
    {
        if (s_spi_instance[i] == instance)
        {
            BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return BSP_PARAM_ERR;
        }
    }

    s_spi_instance[s_spi_idx++] = instance;

    BSPLOG(&g_spi_log, LOG_LEVEL_INFO, "SPI instance registered, idx=%d", s_spi_idx - 1);
    return BSP_OK;
}

/**
 * @brief 配置SPI实例（填充硬件映射 + 父指针 + 回调，可重复调用）
 * @note 要求先调用 SPIRegister 注册实例
 */
BSP_Status_e SPIConfig(SPIInstance *instance, const SPI_Config_s *config)
{
    uint8_t old_idx = SPI_NUM_MAX;
    uint8_t new_idx;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->spi_e >= SPI_NUM_MAX, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "spi_e out of range!"));

    new_idx = (uint8_t)config->spi_e;

    BSP_RETURN_IF_TRUE_LOG(spi_map[new_idx].handle == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "SPI handle is NULL, check bsp_map mapping!"));

    /* 同一 handle 不允许被两个实例占用：回调靠句柄反查实例，冲突则分发错乱 */
    for (uint8_t i = 0; i < s_spi_idx; i++)
    {
        if (s_spi_instance[i] == instance)
            continue;
        if (s_spi_instance[i]->handle == spi_map[new_idx].handle)
        {
            BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Same SPI handle already registered!");
            return BSP_PARAM_ERR;
        }
    }

    /* 重入收尾：把上一次可能还挂着的传输中止掉（否则旧传输会继续往缓冲里写、还会
     * 占着总线让新配置一次都发不出去）+ 清旧路由槽 */
    if (instance->handle != NULL)
    {
        /* 只在真有东西要收尾时动手：SPI_RecoverTx 一律计一次 `tx_recover` 并按契约回调
         * `SPI_ERR_ABORT`（"本次传输已被强制收尾、不会再有完成回调"）。总线本来空闲时
         * 它什么都没中止，却把这句话说给了上层 —— 上层（如 BMI088）据此清 transfer_busy、
         * 记一次失败，而 Config 是可重入的（切模式、重挂回调都会重调），每次重配都白报一笔，
         * `tx_recover` 这个诊断量也跟着失真（分不清"真卡死恢复了几次"与"配了几次"）。
         * 判据用 SPI_BusIsIdle（外设 + 两路 DMA 流），与 SPIRecoverTxIfStuck 同源；
         * 上下文不允许 Abort 时 SPI_RecoverTx 自己返回 0，交由下一次收发兜底。 */
        if (!SPI_BusIsIdle(instance->handle))
            (void)SPI_RecoverTx(instance);
        old_idx = SPI_HspiToIndex(instance->handle);
        if (old_idx < SPI_NUM_MAX && s_spi_inst_by_spi[old_idx] == instance)
            s_spi_inst_by_spi[old_idx] = NULL;
    }

    /* 填充枚举、硬件句柄、父指针与回调 */
    instance->spi_e = config->spi_e;
    instance->handle = spi_map[new_idx].handle;
    instance->parent = config->parent;
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->err_callback = config->err_callback;
    instance->rx_len = 0;

    s_spi_inst_by_spi[new_idx] = instance;

    /* DMA 能力快照（供调试：该口有哪些 DMA，DMA 模式才可能用上） */
    s_spi_status[new_idx].dma_tx_capable = (instance->handle->hdmatx != NULL) ? 1 : 0;
    s_spi_status[new_idx].dma_rx_capable = (instance->handle->hdmarx != NULL) ? 1 : 0;
    s_spi_status[new_idx].state = instance->handle->State;

    /* 空闲计时起点：Config 前不判卡死（见 s_spi_ready_us 的说明） */
    s_spi_ready_us[new_idx] = DWT_GetTimeUs();

    return BSP_OK;
}

BSP_Status_e SPITransmit(SPIInstance *instance, const uint8_t *data, uint16_t len,
                         BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Handle is NULL, call SPIConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_WARNING, "Invalid transmit parameters!"));

    idx = SPI_HspiToIndex(instance->handle);

    if (!SPI_CheckDmaCapability(instance, mode, 1, 0))
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);

    if (mode != BSP_BLOCK_MODE)
    {
        ret = SPI_WaitReady(instance, timeout_ms);
        if (ret != BSP_OK)
            return SPI_FailThenRet(instance, ret);
    }

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_SPI_Transmit(instance->handle, data, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_SPI_Transmit_IT(instance->handle, data, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_SPI_Transmit_DMA(instance->handle, data, len);
        break;
    default:
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
        return SPI_StartFail(instance, hal_st, "transmit", mode, idx);

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].last_mode = (uint8_t)mode;
        s_spi_status[idx].tx_len = len;
        s_spi_status[idx].state = instance->handle->State;
        if (mode == BSP_BLOCK_MODE)
            s_spi_status[idx].tx_ok++; /* BLOCK 返回即发完 */
    }

    return BSP_OK;
}

BSP_Status_e SPIReceive(SPIInstance *instance, uint16_t len,
                        BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL || instance->rx_buff == NULL,
                           SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Handle is NULL, call SPIConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    /* 静默截断会让上层拿到"比请求短"的数据却毫无察觉，直接拒绝 */
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->buff_size,
                           SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Invalid receive len=%d (buff=%d)!",
                                  len, instance->buff_size));

    idx = SPI_HspiToIndex(instance->handle);

    if (!SPI_CheckDmaCapability(instance, mode, 0, 1))
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);

    if (mode != BSP_BLOCK_MODE)
    {
        ret = SPI_WaitReady(instance, timeout_ms);
        if (ret != BSP_OK)
            return SPI_FailThenRet(instance, ret);
    }

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_SPI_Receive(instance->handle, instance->rx_buff, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_SPI_Receive_IT(instance->handle, instance->rx_buff, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_SPI_Receive_DMA(instance->handle, instance->rx_buff, len);
        break;
    default:
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
        return SPI_StartFail(instance, hal_st, "receive", mode, idx);

    /* BLOCK 返回即收满，长度在这里落定；IT/DMA 由完成回调写（那时 hspi->RxXferSize 就是本次请求长度） */
    if (mode == BSP_BLOCK_MODE)
        instance->rx_len = len;

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].last_mode = (uint8_t)mode;
        s_spi_status[idx].rx_len = len;
        s_spi_status[idx].state = instance->handle->State;
        if (mode == BSP_BLOCK_MODE)
            s_spi_status[idx].rx_ok++; /* BLOCK 返回即收满；只收不发，不计 tx_ok */
    }

    return BSP_OK;
}

BSP_Status_e SPITransmitReceive(SPIInstance *instance, const uint8_t *tx_data, uint16_t len,
                                BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    BSP_Status_e ret;
    uint8_t idx;
    HAL_StatusTypeDef hal_st;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL || instance->rx_buff == NULL,
                           SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Handle is NULL, call SPIConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(tx_data == NULL || len == 0 || len > instance->buff_size,
                           SPI_FailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_spi_log, LOG_LEVEL_ERROR, "Invalid transmit/receive parameters (len=%d, buff=%d)!",
                                  len, instance->buff_size));

    idx = SPI_HspiToIndex(instance->handle);

    if (!SPI_CheckDmaCapability(instance, mode, 1, 1))
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);

    if (mode != BSP_BLOCK_MODE)
    {
        ret = SPI_WaitReady(instance, timeout_ms);
        if (ret != BSP_OK)
            return SPI_FailThenRet(instance, ret);
    }

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        hal_st = HAL_SPI_TransmitReceive(instance->handle, tx_data, instance->rx_buff, len, timeout_ms);
        break;
    case BSP_IT_MODE:
        hal_st = HAL_SPI_TransmitReceive_IT(instance->handle, tx_data, instance->rx_buff, len);
        break;
    case BSP_DMA_MODE:
        hal_st = HAL_SPI_TransmitReceive_DMA(instance->handle, tx_data, instance->rx_buff, len);
        break;
    default:
        return SPI_FailThenRet(instance, BSP_PARAM_ERR);
    }

    if (hal_st != HAL_OK)
    {
        /* 不做 SPI_RecoverTx：本调用者已通过返回码知道失败（且 SPI_WaitReady 已把
         * "上一笔 DMA 流未释放"挡在 HAL 之外，HAL_BUSY 又归 BSP_BUSY），真卡死由
         * SPIRecoverTxIfStuck 在任务上下文收拾（ISR 里也 Abort 不了）。 */
        return SPI_StartFail(instance, hal_st, "transmit/receive", mode, idx);
    }

    if (mode == BSP_BLOCK_MODE)
        instance->rx_len = len;

    if (idx < SPI_NUM_MAX)
    {
        s_spi_status[idx].last_mode = (uint8_t)mode;
        s_spi_status[idx].tx_len = len;
        s_spi_status[idx].rx_len = len;
        s_spi_status[idx].state = instance->handle->State;
        if (mode == BSP_BLOCK_MODE)
        {
            s_spi_status[idx].tx_ok++;
            s_spi_status[idx].rx_ok++;
        }
    }

    return BSP_OK;
}

/**
 * @brief 传输卡死自恢复的统一入口（**任务上下文**，DRV 在自己的读入口调用）
 * @param instance 待检查的 SPI 实例
 * @param stuck_ms 判定阈值（ms），0 = 用 SPI_TX_STUCK_TIMEOUT_MS
 * @retval BSP_OK        本次刚复位了一次卡死的传输（err_callback 已按 SPI_ERR_ABORT 通知过）
 * @retval BSP_BUSY      未动作：总线空闲 / 未到阈值 / 上下文不允许 Abort / 该句柄未登记
 * @retval BSP_PARAM_ERR 实例或句柄为空
 *
 * @note 为什么必须有它：INT 模式的传输由 DRDY EXTI 发起，那里既不能等 tick 也就无法真正
 *       Abort（见 SPI_CanBlockingAbort）；HAL 的 DMA 启动失败分支又会把句柄留在 BUSY_TX_RX。
 *       两句合起来 = 中断里发起的传输一旦启动失败就再也起不来，且 DRV 的 transfer_busy
 *       也永远清不掉（现象：采样计数不再增长、姿态恒为初值、日志一片安静）。
 *       本入口把复位搬到任务上下文，判据（卡死多久）、计时基准（s_spi_ready_us）、
 *       纠正动作（SPI_RecoverTx）与计数（tx_recover）都在 bsp 内，上层只留一行调用。
 *
 * @note 判据为什么看"State 连续非 READY 的时长"而不是别的：正常的在途传输也是非 READY
 *       （约 20µs），只能靠时长区分；阈值 20ms 既是它的千倍余量，又能在 DRDY 漏几拍之内
 *       恢复（SPI_TX_STUCK_TIMEOUT_MS）。
 *
 * @note 总线空闲时顺便刷新基准，而不是只在配置/收发时刷新：上层是"每个控制周期调一次"，
 *       中间可能隔很久没有传输；不刷新的话基准会停在很久以前，下一次正常传输一开始就被
 *       判成卡死（假的 tx_recover + 一次无谓的 Abort 中止掉刚启动的传输）。
 *
 * @note **为什么本入口不需要"作用域自证"**（对比 `CANRecover` / `I2CBusRecover`）：
 *       **故障对象与动作对象在这里是同一个** —— 判据读的是 `hspi->State` 与本句柄的两路
 *       DMA 流（外设自己的话，不是上层"我这笔没成"的转述），动作（`HAL_SPI_Abort` +
 *       强制放流）也只作用于这一个句柄，被中止的正是判据认定的那一笔卡死传输
 *       （正常传输只有 6~64µs，阈值 20ms，量级差三个数量级，不会误伤在途的那一笔）。
 *       故障与动作天生同域，判据放在这里就已经是自证。
 *       需要额外自证的是另一类入口：**判据来自实例、动作却落到整条总线**
 *       （重建整个外设 / 取消总线上所有实例的在途帧），见 `bsp_i2c.h` 的 `I2CBusRecover`
 *       与 `bsp_can.h` 的 `CANRecover`。也正因为判据在这层，调用点传进来的 `stuck_ms`
 *       只能**收紧时长**，既改不了判据、也改不了作用范围（`drv_bmi088` 传 1ms 是
 *       "这条链路把判据卡严些"，不是"改按实例判"）。
 */
BSP_Status_e SPIRecoverTxIfStuck(SPIInstance *instance, uint32_t stuck_ms)
{
    uint8_t idx;
    uint64_t now_us;

    if (instance == NULL || instance->handle == NULL)
        return BSP_PARAM_ERR;

    if (stuck_ms == 0)
        stuck_ms = SPI_TX_STUCK_TIMEOUT_MS;

    /* 上下文不允许阻塞 Abort（ISR / 临界区）：本次不动作，也不刷新基准，
     * 下一次任务上下文的调用照样命中并补做（见 SPI_CanBlockingAbort） */
    if (!SPI_CanBlockingAbort())
        return BSP_BUSY;

    idx = SPI_HspiToIndex(instance->handle);
    if (idx >= SPI_NUM_MAX)
        return BSP_BUSY; /* 未登记句柄：没有计时基准，无法判卡死（也不该去动硬件） */

    /* 总线空闲 = 链路健康：刷新基准后返回。
     * 判据必须与 SPI_WaitReady 一致（含两路 DMA 流）：只判 `hspi->State` 的话，
     * "外设 READY 但 DMA 流卡在忙态"会同时造成 —— 收发那边永远 BSP_BUSY（发不出去），
     * 这边却以为健康而刷新基准（永不判卡死）→ 永久静默，比不修还糟。 */
    if (SPI_BusIsIdle(instance->handle))
    {
        s_spi_ready_us[idx] = DWT_GetTimeUs();
        return BSP_BUSY;
    }

    /* 非 READY 但没超过阈值：可能只是一次正常的在途传输 */
    now_us = DWT_GetTimeUs();
    if (s_spi_ready_us[idx] == 0 ||
        (now_us - s_spi_ready_us[idx]) <= ((uint64_t)stuck_ms * 1000u))
        return BSP_BUSY;

    /* 超过阈值仍非 READY = 卡死：中止本次、把 State 放回 READY，让传输链重新跑起来。
     * 被中止的那次不会再有完成回调，SPI_RecoverTx 会按契约调 err_callback 通知上层。
     * 返回值恒为 1：SPI_RecoverTx 的 0 分支只有 instance/handle 为空与上下文不允许，
     * 这三个条件在本函数入口都判过了，所以不再写一条永远走不到的分支
     * （旧版 `if (!SPI_RecoverTx(...)) return BSP_HW_ERR;`）。 */
    (void)SPI_RecoverTx(instance);

    s_spi_ready_us[idx] = DWT_GetTimeUs(); /* 刚复位，重新计时 */
    BSPLOG(&g_spi_log, LOG_LEVEL_WARNING, "SPI stuck >%dms, state reset (spi_e=%d)!",
           (int)stuck_ms, (int)instance->spi_e);
    return BSP_OK;
}

#endif /* SPI_INSTANCE_NUM > 0 */

#endif /* BSP_SPI_USED */
