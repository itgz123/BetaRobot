/**
 * @file bsp_usart.c
 * @brief USART驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置。
 *
 * @note 本层职责（对照 bsp_can）：
 *       ① 收发（三模式，模式每次调用传参）；
 *       ② 必要回调（Rx / TxCplt / Error）并分发到实例；
 *       ③ 纠正错误（发送卡死复位、接收续收重启）；
 *       ④ 计数与状态快照存 static 结构体供调试器 Watch（`s_usart_status`）。
 *       除此之外不提供任何接口：不做发送队列/缓冲（忙即 BSP_BUSY，多缓冲归上层），
 *       不对外暴露 is_ready。两条自恢复各有一个对外入口（USARTRecoverRxIfStalled /
 *       USARTRecoverTxIfStuck），只把"何时看一眼"交给上层，判据与动作都在这层。
 */

#include "bsp_usart.h"
#include "app_cfg.h"

#ifdef BSP_USART_USED

#if UART_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_log.h"
#include <string.h>

/*------------- 私有变量 --------------*/
static uint8_t s_usart_idx = 0;
#ifndef BSP_USART_LOG_LIMIT
#define BSP_USART_LOG_LIMIT 10
#endif // !BSP_USART_LOG_LIMIT

/* 接收续收失败重试次数（自恢复：避免一次失败后 RX 永久停摆） */
#ifndef USART_RX_RESTART_RETRY
#define USART_RX_RESTART_RETRY 3
#endif

/* 发送"卡死"判定阈值（ms）：gState 连续非 READY 超过该时长即强制复位发送状态。
 * 取值远大于最长合法帧的传输时间（115200 下 64B 约 6ms），不会误伤正常发送；
 * 又给 timeout_ms==0 的高频发送方（log/vofa/terminal）留了自恢复通道。 */
#ifndef USART_TX_STUCK_TIMEOUT_MS
#define USART_TX_STUCK_TIMEOUT_MS 200
#endif

LOG_INSTANCE_DEF(g_usart_log, "bsp_usart", BSP_USART_LOG_LIMIT); /* USART 日志实例 */

static USARTInstance *s_usart_instance[UART_INSTANCE_NUM] = {NULL};

/* 句柄索引 → 实例路由表（Config 时登记）。回调里先按索引取实例，
 * 索引本身用于状态统计——即使实例路由缺失，错误/事件也不漏计。 */
static USARTInstance *s_usart_inst_by_uart[UART_NUM_MAX] = {NULL};

/* 每个 UART 最近一次观察到 gState == READY 的时刻（DWT 微秒）：用于判定发送是否卡死。
 * 0 表示尚未采样过（首帧前不判卡死）。只被任务上下文读写。
 * 刷新点：USARTConfig、USARTTransmit 的就绪路径（含复位成功之后）、以及自恢复入口
 * USARTRecoverTxIfStuck —— 它每次发现"发送空闲"都刷新一次基准，保证基准不会陈旧到
 * 让下一次正常发送一上来就被误判成卡死。 */
static uint64_t s_tx_ready_us[UART_NUM_MAX] = {0};

/* 每个 UART 最近一次"接收停摆重启"的尝试时刻（DWT 微秒）：USARTRecoverRxIfStalled 的限频基准。
 * 0 表示从未尝试过（首次调用立即放行）。放这里而不是让每个上层各存一个：
 * 四条链路（dbus / sbus / comm-media / terminal）的自恢复都走同一个入口，限频也只该有一份。 */
static uint64_t s_rx_restart_us[UART_NUM_MAX] = {0};

/*------------- USART 状态/错误统计（调试用，调试器直接 Watch s_usart_status） --------------*/

/**
 * @brief USART 外设状态与错误统计（每 UART 一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：Rx 事件回调、Tx 完成回调、错误回调、收发接口的失败返回，
 *       外加便于判断"当前状态"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;      /* 发送成功次数（BLOCK 直接成功 + IT/DMA TxCplt） */
    uint32_t tx_fail;    /* USARTTransmit 返回非 BSP_OK 的总次数 */
    uint32_t tx_busy;    /* 其中 BSP_BUSY（外设忙，上层应排队重试） */
    uint32_t tx_timeout; /* 其中 BSP_TIMEOUT（已强制复位卡死状态） */
    uint32_t rx_ok;      /* 收帧成功次数（RxEvent 回调 / BLOCK 收满） */
    uint32_t rx_start;   /* 常开接收成功启动次数 */
    /* 错误分类计数（可同时置位，独立 if） */
    uint32_t err_total;  /* 错误回调总次数 */
    uint32_t err_pe;     /* 奇偶校验错误 */
    uint32_t err_fe;     /* 帧错误 */
    uint32_t err_ne;     /* 噪声错误 */
    uint32_t err_ore;    /* 溢出错误（RX 未及时取走） */
    uint32_t err_dma;    /* DMA 传输错误 */
    uint32_t err_other;  /* 其它错误位（如 RTO） */
    uint32_t err_no_dma; /* 调用 DMA 模式但该口无对应 DMA（hdmarx/hdmatx 为 NULL） */
    uint32_t err_start;  /* HAL 启动收发失败（非 busy/timeout）次数 */
    /* 自恢复计数 */
    uint32_t tx_recover;      /* 卡死的发送被强制复位次数（复位后即可重新发送） */
    uint32_t rx_restart;      /* 接收自动续收成功次数 */
    uint32_t rx_restart_fail; /* 续收最终失败次数（>0 表示 RX 已停摆，须上层重新 USARTReceive） */
    uint32_t rx_recover;      /* 上层重启接收时发现残留状态、被强制复位次数（见 USART_RecoverRx） */
    /* 实时快照 */
    uint8_t g_state;        /* HAL gState（最近一次采样） */
    uint8_t rx_state;       /* HAL RxState（最近一次采样） */
    uint8_t rx_armed;       /* 常开接收是否在跑 */
    uint8_t tx_mode;        /* 最近一次发送模式 */
    uint8_t rx_mode;        /* 最近一次接收模式 */
    uint32_t error_code;    /* 最近一次 huart->ErrorCode */
    uint64_t err_time_us;   /* 最近一次错误时间（DWT 微秒） */
    uint8_t dma_tx_capable; /* Config 时记录：该口是否有 TX DMA */
    uint8_t dma_rx_capable; /* Config 时记录：该口是否有 RX DMA */
    uint16_t tx_len;        /* 最近一次发送长度 */
    uint16_t rx_len;        /* 最近一次接收长度 */
} USART_Status_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile USART_Status_s s_usart_status[UART_NUM_MAX];

/*------------- 私有函数：查表与计数 --------------*/

/**
 * @brief huart → uart_e 索引（UART_1/UART_SBUS/... 对应 uart_map 下标）
 * @retval UART_NUM_MAX 未找到
 */
static uint8_t USART_HuartToIndex(const UART_HandleTypeDef *huart)
{
    uint8_t i;

    if (huart == NULL)
        return UART_NUM_MAX;

    for (i = 0; i < UART_NUM_MAX; i++)
    {
        if (uart_map[i].handle == huart)
            return i;
    }
    return UART_NUM_MAX;
}

/**
 * @brief 收发接口失败路径统一计数后返回状态码
 * @note 作为 BSP_RETURN_IF_TRUE_LOG 的 ret 参数注入（宏内 `return (ret)` 会求值），
 *       使所有失败返回点都统计进 s_usart_status[]，日志行为不变。
 */
static BSP_Status_e USART_TxFailThenRet(const USARTInstance *instance, BSP_Status_e status)
{
    uint8_t idx = (instance != NULL) ? USART_HuartToIndex(instance->handle) : UART_NUM_MAX;

    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].tx_fail++;
        if (status == BSP_BUSY)
            s_usart_status[idx].tx_busy++;
        else if (status == BSP_TIMEOUT)
            s_usart_status[idx].tx_timeout++;
    }
    return status;
}

/*------------- 私有函数：上下文判定 --------------*/

/**
 * @brief 当前上下文能否安全执行"按 HAL_GetTick 自旋"的中止动作（HAL_UART_Abort* → HAL_DMA_Abort）
 * @retval 1 可以（普通任务上下文）
 * @retval 0 不可以（死等风险，只能改期）
 *
 * @note 两类不安全上下文都要查（共同后果是 `HAL_GetTick` 冻住）：
 *       ① 中断上下文（`IPSR != 0`）——HAL tick 源在本工程是 TIM 不是 SysTick
 *          （DJI_C 的 TIM14、其余板 TIM23，优先级 5/15），优先级数值不小于外设中断的 5，
 *          抢占不了本 ISR，故中断里 tick 不前进；
 *       ② 临界区——`taskENTER_CRITICAL` 抬的是 BASEPRI（FreeRTOS ARM_CM4F/CM7 端口的
 *          `portDISABLE_INTERRUPTS` = `vPortRaiseBASEPRI`），tick 源同样进不来。
 *          调用方 drv_terminal_lite 的发送提交正是整个包在 `taskENTER_CRITICAL` 里，
 *          故只查 IPSR 不够（bsp_i2c 的 `I2C_CanBlockingAbort` 判据与本函数相同）。
 * @note 判定为 0 时一律不动作、也不假复位（假复位会让下一次 HAL 启动直接失败）：
 *       判据侧不刷新时间戳，下一次任务上下文的调用会照样命中并补做。
 */
static inline uint8_t USART_CanBlockingAbort(void)
{
    return (__get_IPSR() == 0U) && (__get_PRIMASK() == 0U) && (__get_BASEPRI() == 0U);
}

/*------------- 私有函数：发送卡死纠正 --------------*/

/**
 * @brief 强制中止发送并复位状态（只在任务上下文调用）
 * @param instance USART 实例
 * @retval 1 已中止（err_callback 已按契约通知）
 * @retval 0 未动作（上下文不允许阻塞 Abort，或参数不合法）
 * @note IT/DMA 发送中途出错时 gState 可能停在 BUSY_TX 永不回 READY，此后每次发送都在
 *       "等就绪"处失败 = 发送永久静默。这里强制中止并复位状态，让下一次发送能重新发起。
 *       内部经 HAL_DMA_Abort 按 HAL_GetTick 自旋，中断上下文/临界区里 tick 不前进会死等
 *       （bsp_i2c 的 `I2C_ResetHandle` 是同一个坑），故入口先做 `USART_CanBlockingAbort()`
 *       判定，不允许时**什么都不做**并返回 0——真正的复位留给下一次任务上下文的调用。
 * @note 被中止的那次发送不会再触发 tx_callback，所以这里按 err_callback 契约通知上层
 *       复位自身状态（归还"发送中"的缓冲）。漏通知的后果不是 bsp 层而是上层堵死：
 *       bsp_log 的 s_tx_buf 会永远指向 SEND 状态的槽、vofa 停在 BUFF_ACTIVE，
 *       即便 bsp 已把 gState 复位，上层自己也再发不出东西。
 * @note 通知顺序不能颠倒：必须 Abort + 把 gState 复位到 READY **之后**再通知。上层的
 *       归还判据普遍是"gState == READY 说明 HAL 确已收尾、不会再来完成回调"，提前通知
 *       会被上层当成"发送仍在途"而拒绝归还（见 bsp_usart.md §1.6）。
 * @note 上层 handler 须能在**任务上下文**安全运行（此前契约只说 ISR）。
 */
static uint8_t USART_RecoverTx(USARTInstance *instance)
{
    uint8_t idx;

    if (instance == NULL || instance->handle == NULL || !USART_CanBlockingAbort())
        return 0;

    idx = USART_HuartToIndex(instance->handle);

    (void)HAL_UART_AbortTransmit(instance->handle);
    if (instance->handle->gState != HAL_UART_STATE_READY)
        instance->handle->gState = HAL_UART_STATE_READY; /* Abort 未复位状态时兜底 */

    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].tx_recover++;
        s_usart_status[idx].g_state = instance->handle->gState;
    }

    if (instance->err_callback != NULL)
        instance->err_callback(instance, USART_ERR_TX_ABORT); /* 被中止的发送不会再有 tx_callback */

    return 1;
}

/*------------- 私有函数：接收启动 --------------*/

/**
 * @brief 强制中止接收并把 RxState 复位到 READY（只在任务上下文调用）
 * @param instance USART 实例
 * @retval 1 已中止
 * @retval 0 未动作（上下文不允许阻塞 Abort，或参数不合法）
 * @note 与发送卡死同源：DMA/HAL 出错后 RxState 可能停在非 READY、DMA 流也未清干净，
 *       但软件侧已没有任何接收在跑。此时任何"重新启动接收"都会被 HAL 以 HAL_BUSY 拒绝 ——
 *       ISR 里的续收重试必然次次失败（`rx_restart_fail` 增长、RX 永久停摆），
 *       上层再怎么调 USARTReceive 也起不来。这里强制中止一次，把 DMA 流与 RxState 都清干净，
 *       让上层那次重启真正生效。
 * @note 上下文判据同 `USART_RecoverTx`：内部经 HAL_DMA_Abort 按 HAL_GetTick 自旋，
 *       中断上下文/临界区里 tick 不前进会死等，那种上下文直接返回 0 不动作。
 *       （USARTReceive 的正常调用方——daemon 回调、Config——都是任务上下文，不受影响。）
 */
static uint8_t USART_RecoverRx(USARTInstance *instance)
{
    uint8_t idx;

    if (instance == NULL || instance->handle == NULL || !USART_CanBlockingAbort())
        return 0;

    idx = USART_HuartToIndex(instance->handle);

    (void)HAL_UART_AbortReceive(instance->handle);
    if (instance->handle->RxState != HAL_UART_STATE_READY)
        instance->handle->RxState = HAL_UART_STATE_READY; /* Abort 未复位状态时兜底 */

    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].rx_recover++;
        s_usart_status[idx].rx_state = instance->handle->RxState;
    }

    return 1;
}

/**
 * @brief 按已记录的 rx_mode/rx_xfer_len 启动一次接收（不重试）
 * @retval BSP_OK / BSP_BUSY / BSP_HW_ERR / BSP_PARAM_ERR
 */
static BSP_Status_e USART_StartReceiveRaw(USARTInstance *instance)
{
    HAL_StatusTypeDef st;

    /* 干净的接收缓冲：只在自己真正启动新一次传输前清，避免清掉别人正在收的数据 */
    memset(instance->rx_buff, 0, instance->rx_xfer_len);

    switch (instance->rx_mode)
    {
    case BSP_IT_MODE:
        st = HAL_UARTEx_ReceiveToIdle_IT(instance->handle, instance->rx_buff, instance->rx_xfer_len);
        break;
    case BSP_DMA_MODE:
        st = HAL_UARTEx_ReceiveToIdle_DMA(instance->handle, instance->rx_buff, instance->rx_xfer_len);
        break;
    default:
        return BSP_PARAM_ERR;
    }

    if (st == HAL_BUSY)
        return BSP_BUSY;
    if (st != HAL_OK)
        return BSP_HW_ERR;

    /* TOIDLE 下 HAL 半传输也会进 RxEventCallback（会把半帧当一帧上报），关掉只管 IDLE/收满 */
    if (instance->rx_mode == BSP_DMA_MODE && instance->handle->hdmarx != NULL)
        __HAL_DMA_DISABLE_IT(instance->handle->hdmarx, DMA_IT_HT);

    return BSP_OK;
}

/**
 * @brief 常开接收续收（收到一帧后按原参数继续收下一帧），失败重试若干次
 * @param instance USART 实例
 * @param idx      实例对应的 uart_map 下标（UART_NUM_MAX 表示未登记）
 * @retval 1 接收在跑（含"外设已经在收"）
 * @retval 0 续收最终失败 —— **RX 已停摆**（`rx_armed` 已清、`rx_restart_fail++`）
 *
 * @note 续收最终失败意味着 RX 停摆：此后不会再触发 `rx_callback`（对端看门狗会判离线、
 *       链路永久失效），只能由上层重新 `USARTReceive` 才能恢复。
 *       返回值交给调用方决定是否通知上层，而不是在这里直接调 `err_callback`：
 *       `HAL_UART_ErrorCallback` 末尾本来就会通知一次，重复通知虽幂等但没必要；
 *       而 `HAL_UARTEx_RxEventCallback` 没有别的通知路径，必须自己补。
 */
static uint8_t USART_ContinueReceive(USARTInstance *instance, uint8_t idx)
{
    uint8_t attempt;
    BSP_Status_e ret = BSP_HW_ERR; /* 重试次数被配成 0 时的兜底（按失败处理） */

    for (attempt = 0; attempt < USART_RX_RESTART_RETRY; attempt++)
    {
        ret = USART_StartReceiveRaw(instance);
        if (ret == BSP_OK)
            break;
        /* BSP_BUSY = 外设已经在收（例如错误回调与收发路径前后脚都要求续收），
         * 结果同样是"接收在跑"，不算失败——否则会误清 rx_armed 让常开流停摆 */
        if (ret == BSP_BUSY)
            break;
    }

    if (ret == BSP_OK || ret == BSP_BUSY)
    {
        instance->rx_armed = 1;
        if (idx < UART_NUM_MAX)
        {
            s_usart_status[idx].rx_restart++;
            s_usart_status[idx].rx_armed = 1;
        }
        return 1;
    }

    instance->rx_armed = 0;
    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].rx_armed = 0;
        s_usart_status[idx].rx_restart_fail++;
    }
    BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Restart receive failed after %d retries (uart_e=%d)!",
           USART_RX_RESTART_RETRY, (int)instance->uart_e);
    return 0;
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief 接收事件回调（IDLE 或收满触发）
 * @param huart 发生中断的串口句柄
 * @param Size  此次收到的数据量
 *
 * @note HAL 在进入本回调前已完成收尾：DMA_NORMAL 下 IDLE 与 TC 两条路径都会
 *       关 DMAR、置 RxState=READY、清 IDLEIE 并中止 DMA（见 stm32h7xx_hal_uart.c
 *       的 IDLE 分支与 UART_DMAReceiveCplt），IT 模式同理置 RxState=READY。
 *       因此本回调内不需要再 HAL_UART_DMAStop（旧版是为"循环 DMA 回卷"加的，
 *       而工程里 CubeMX 配的都是 DMA_NORMAL），避免多调一次会自旋的 Abort。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uint8_t idx = USART_HuartToIndex(huart);
    USARTInstance *instance;

    if (idx >= UART_NUM_MAX)
        return;

    /* 状态统计与快照：即使实例路由缺失，事件也不漏计 */
    s_usart_status[idx].rx_ok++;
    s_usart_status[idx].rx_state = huart->RxState;
    s_usart_status[idx].rx_len = Size;

    instance = s_usart_inst_by_uart[idx];
    if (instance == NULL)
        return;

    if (Size > instance->rx_buff_size)
        Size = instance->rx_buff_size; /* 纠正：防御性截断，防越界读 */

    instance->rx_len = Size;

    if (instance->rx_callback != NULL)
        instance->rx_callback(instance);

    /* BLOCK 接收或常开流已停：不续收 */
    if (!instance->rx_armed)
        return;

    /* 缓冲清理由 StartReceiveRaw 在真正启动前做。
     * 续收最终失败 = 接收链路已断，此后不会再触发本回调，必须通知上层：
     * 上层靠 rx_callback 判"数据还在流"（如 dbus/sbus 的失控检测），
     * 回调一旦静默停掉，那种判据会永远冻结在"正常"上。 */
    if (!USART_ContinueReceive(instance, idx) && instance->err_callback != NULL)
        instance->err_callback(instance, USART_ERR_RX_STALLED);
}

/**
 * @brief 串口错误回调（ISR 上下文）
 * @note 本回调内完成三件事：分类计数 + 状态快照、纠正错误（发送卡死复位 / 接收续收）、
 *       通知上层（err_callback）。上层若此刻正有 IT/DMA 发送在途，该发送的完成回调
 *       不会再触发，须在 err_callback 里归还对应缓冲（见头文件契约）。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uint8_t idx = USART_HuartToIndex(huart);
    USARTInstance *instance;
    uint32_t error_code;

    if (idx >= UART_NUM_MAX)
        return;

    error_code = huart->ErrorCode;
    instance = s_usart_inst_by_uart[idx];

    /* ① 分类计数（多种错误可同时置位，必须独立 if，else-if 会漏计） */
    s_usart_status[idx].err_total++;
    if (error_code & HAL_UART_ERROR_PE)
        s_usart_status[idx].err_pe++;
    if (error_code & HAL_UART_ERROR_FE)
        s_usart_status[idx].err_fe++;
    if (error_code & HAL_UART_ERROR_NE)
        s_usart_status[idx].err_ne++;
    if (error_code & HAL_UART_ERROR_ORE)
        s_usart_status[idx].err_ore++;
    if (error_code & HAL_UART_ERROR_DMA)
        s_usart_status[idx].err_dma++;
    if (error_code & ~(uint32_t)(HAL_UART_ERROR_PE | HAL_UART_ERROR_FE | HAL_UART_ERROR_NE |
                                 HAL_UART_ERROR_ORE | HAL_UART_ERROR_DMA))
        s_usart_status[idx].err_other++;

    /* 状态快照 */
    s_usart_status[idx].error_code = error_code;
    s_usart_status[idx].g_state = huart->gState;
    s_usart_status[idx].rx_state = huart->RxState;
    s_usart_status[idx].err_time_us = DWT_GetTimeUs();

    /* ② 纠正错误 */
    /* 接收断流：错误会中止本次接收且不会再自动续收，此处按原参数重启 */
    /* 返回值在此忽略：本函数末尾无论如何都会调 err_callback 通知上层 */
    if (instance != NULL && instance->rx_armed && huart->RxState == HAL_UART_STATE_READY)
        (void)USART_ContinueReceive(instance, idx);
    /* 注意：这里不动发送状态。HAL 的错误位都在接收侧，此刻 gState != READY 通常表示
     * "有一次健康的发送正在途"，误判成卡死会中止正常发送。真正卡死由 USARTTransmit 的
     * 就绪失败路径按 DWT 时长（USART_TX_STUCK_TIMEOUT_MS）判定并复位，且不在 ISR 里做。 */

    /* ③ 上报（保留解码日志，便于直接看出是哪种错） */
    BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "Error detected, code=0x%lX (PE:%d FE:%d NE:%d ORE:%d DMA:%d)",
           (unsigned long)error_code,
           (error_code & HAL_UART_ERROR_PE) ? 1 : 0,
           (error_code & HAL_UART_ERROR_FE) ? 1 : 0,
           (error_code & HAL_UART_ERROR_NE) ? 1 : 0,
           (error_code & HAL_UART_ERROR_ORE) ? 1 : 0,
           (error_code & HAL_UART_ERROR_DMA) ? 1 : 0);

    if (instance != NULL && instance->err_callback != NULL)
        instance->err_callback(instance, USART_ERR_HW);
}

/**
 * @brief 发送完成回调函数（IT/DMA）
 * @param huart 发送完成的串口句柄
 * @note HAL 在调用本回调前已把 gState 置回 READY，因此在 tx_callback 内直接续发是合法的
 *       （bsp_log / drv_vofa / drv_terminal_lite 的发送链依赖此契约）。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    uint8_t idx = USART_HuartToIndex(huart);
    USARTInstance *instance;

    if (idx >= UART_NUM_MAX)
        return;

    s_usart_status[idx].tx_ok++;
    s_usart_status[idx].g_state = huart->gState;

    instance = s_usart_inst_by_uart[idx];
    if (instance != NULL && instance->tx_callback != NULL)
        instance->tx_callback(instance);
}

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册USART实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 USARTConfig 负责）
 */
BSP_Status_e USARTRegister(USARTInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_usart_idx >= UART_INSTANCE_NUM, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_usart_idx; i++)
    {
        if (s_usart_instance[i] == instance)
        {
            BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return BSP_PARAM_ERR;
        }
    }

    s_usart_instance[s_usart_idx++] = instance;

    BSPLOG(&g_usart_log, LOG_LEVEL_INFO, "USART Instance registered, idx=%d", s_usart_idx - 1);
    return BSP_OK;
}

/**
 * @brief 配置USART实例（填充硬件映射 + 父指针 + 回调，可重复调用）
 * @note 要求先调用 USARTRegister 注册实例
 * @note 不再自动启动接收：需接收的实例请在 Config 之后显式 USARTReceive
 */
BSP_Status_e USARTConfig(USARTInstance *instance, const USART_Config_s *config)
{
    uint8_t old_idx = UART_NUM_MAX;
    uint8_t new_idx;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->uart_e >= UART_NUM_MAX, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "uart_e out of range!"));

    new_idx = (uint8_t)config->uart_e;

    BSP_RETURN_IF_TRUE_LOG(uart_map[new_idx].handle == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "UART handle is NULL, check bsp_map mapping!"));

    /* 同一 handle 不允许被两个实例占用：回调靠句柄反查实例，冲突则分发错乱 */
    for (uint8_t i = 0; i < s_usart_idx; i++)
    {
        if (s_usart_instance[i] == instance)
            continue;
        if (s_usart_instance[i]->handle == uart_map[new_idx].handle)
        {
            BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Same UART handle already registered!");
            return BSP_PARAM_ERR;
        }
    }

    /* 重入收尾：中止未完成的接收（旧句柄，避免旧传输继续写缓冲）+ 清旧路由槽 */
    if (instance->handle != NULL)
    {
        if (instance->rx_armed)
        {
            (void)HAL_UART_AbortReceive(instance->handle);
            instance->rx_armed = 0;
        }
        old_idx = USART_HuartToIndex(instance->handle);
        if (old_idx < UART_NUM_MAX && s_usart_inst_by_uart[old_idx] == instance)
        {
            s_usart_inst_by_uart[old_idx] = NULL;
            s_usart_status[old_idx].rx_armed = 0;
        }
    }

    /* 填充枚举、硬件句柄、父指针与回调 */
    instance->uart_e = config->uart_e;
    instance->handle = uart_map[new_idx].handle;
    instance->parent = config->parent;
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->err_callback = config->err_callback;
    instance->rx_len = 0;
    instance->rx_mode = BSP_BLOCK_MODE;
    instance->rx_xfer_len = 0;

    s_usart_inst_by_uart[new_idx] = instance;

    /* DMA 能力快照（供调试：该口有哪些 DMA，DMA 模式才可能用上） */
    s_usart_status[new_idx].dma_tx_capable = (instance->handle->hdmatx != NULL) ? 1 : 0;
    s_usart_status[new_idx].dma_rx_capable = (instance->handle->hdmarx != NULL) ? 1 : 0;
    s_usart_status[new_idx].rx_armed = 0;

    /* 发送空闲计时起点：Config 前不判卡死（见 USARTTransmit 的 s_tx_ready_us != 0 判断） */
    s_tx_ready_us[new_idx] = DWT_GetTimeUs();

    return BSP_OK;
}

BSP_Status_e USARTTransmit(USARTInstance *instance, const uint8_t *data, uint16_t len,
                           BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    uint8_t idx;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, USART_TxFailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, USART_TxFailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Handle is NULL, call USARTConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, USART_TxFailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, USART_TxFailThenRet(instance, BSP_PARAM_ERR),
                           BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "Invalid transmit parameters!"));

    idx = USART_HuartToIndex(instance->handle);

    /* DMA 能力校验：该口没配 TX DMA 就明确拒绝，而不是让 HAL 静默失败 */
    if (mode == BSP_DMA_MODE && instance->handle->hdmatx == NULL)
    {
        if (idx < UART_NUM_MAX)
            s_usart_status[idx].err_no_dma++;
        BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "TX DMA not available (uart_e=%d)!", (int)instance->uart_e);
        return USART_TxFailThenRet(instance, BSP_PARAM_ERR);
    }

    /* IT/DMA 需要先拿到发送权；BLOCK 不做预判（HAL 阻塞版自己会等） */
    if (mode != BSP_BLOCK_MODE)
    {
        if (instance->handle->gState == HAL_UART_STATE_READY)
        {
            if (idx < UART_NUM_MAX)
                s_tx_ready_us[idx] = DWT_GetTimeUs(); /* 采样：发送空闲时刻 */
        }
        else if (timeout_ms == 0)
        {
            /* 不等待：忙即返回。但若已连续"忙"很久（远超最长合法帧），说明状态卡死，
             * 复位一次让后续发送能恢复 —— 与对外暴露的 USARTRecoverTxIfStuck 是同一份
             * 判定与动作（这里只是"顺手再判一次"，因为调用方此刻确实摸到了 USARTTransmit）。
             * @note 发送链一旦卡住就再也不会走到这里（各链的就绪判据会让新帧排队而不是重发，
             *       见 USARTRecoverTxIfStuck 注释），所以这条路只是兜底，主力是上层在自己的
             *       发送入口调那个入口。 */
            (void)USARTRecoverTxIfStuck(instance, 0);
            return USART_TxFailThenRet(instance, BSP_BUSY);
        }
        else
        {
            BSP_Timeout_s t;

            BSP_TimeoutStart(&t, timeout_ms);
            while (instance->handle->gState != HAL_UART_STATE_READY)
            {
                if (BSP_TimeoutExpired(&t))
                {
                    /* 上层愿意等却等不到 = 发送卡死，能复位就复位（任务上下文）；
                     * 上下文不允许 Abort 时如实报出"没复位"，不静默 */
                    if (USART_RecoverTx(instance))
                        BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "UART TX ready timeout, state reset (uart_e=%d)!",
                               (int)instance->uart_e);
                    else
                        BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "UART TX ready timeout, reset skipped (ISR/critical) (uart_e=%d)!",
                               (int)instance->uart_e);
                    return USART_TxFailThenRet(instance, BSP_TIMEOUT);
                }
            }
            if (idx < UART_NUM_MAX)
                s_tx_ready_us[idx] = DWT_GetTimeUs();
        }
    }

    switch (mode)
    {
    case BSP_BLOCK_MODE:
        if (HAL_UART_Transmit(instance->handle, data, len, timeout_ms) != HAL_OK)
        {
            BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "HAL_UART_Transmit failed (uart_e=%d)!", (int)instance->uart_e);
            return USART_TxFailThenRet(instance, BSP_HW_ERR);
        }
        break;
    case BSP_IT_MODE:
        if (HAL_UART_Transmit_IT(instance->handle, data, len) != HAL_OK)
        {
            BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "HAL_UART_Transmit_IT failed (uart_e=%d)!", (int)instance->uart_e);
            return USART_TxFailThenRet(instance, BSP_HW_ERR);
        }
        break;
    case BSP_DMA_MODE:
        if (HAL_UART_Transmit_DMA(instance->handle, data, len) != HAL_OK)
        {
            BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "HAL_UART_Transmit_DMA failed (uart_e=%d)!", (int)instance->uart_e);
            return USART_TxFailThenRet(instance, BSP_HW_ERR);
        }
        break;
    default:
        return USART_TxFailThenRet(instance, BSP_PARAM_ERR);
    }

    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].tx_mode = (uint8_t)mode;
        s_usart_status[idx].tx_len = len;
        s_usart_status[idx].g_state = instance->handle->gState;
        if (mode == BSP_BLOCK_MODE)
            s_usart_status[idx].tx_ok++; /* BLOCK 返回即发完 */
    }

    return BSP_OK;
}

BSP_Status_e USARTReceive(USARTInstance *instance, uint16_t len,
                          BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    uint8_t idx;
    BSP_Status_e ret;
    HAL_StatusTypeDef st;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL || instance->rx_buff == NULL, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Handle is NULL, call USARTConfig first!"));
    BSP_RETURN_IF_TRUE_LOG(mode > BSP_DMA_MODE, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Invalid mode=%d!", (int)mode));
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->rx_buff_size, BSP_PARAM_ERR,
                           BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Invalid receive len=%d (buff=%d)!",
                                  len, instance->rx_buff_size));

    idx = USART_HuartToIndex(instance->handle);

    /* BLOCK：一次一帧，与常开流互斥 */
    if (mode == BSP_BLOCK_MODE)
    {
        if (instance->rx_armed)
            return BSP_BUSY;

        st = HAL_UART_Receive(instance->handle, instance->rx_buff, len, timeout_ms);
        if (st == HAL_BUSY)
            return BSP_BUSY;
        if (st == HAL_TIMEOUT)
            return BSP_TIMEOUT;
        if (st != HAL_OK)
        {
            if (idx < UART_NUM_MAX)
                s_usart_status[idx].err_start++;
            BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "HAL_UART_Receive failed (uart_e=%d)!", (int)instance->uart_e);
            return BSP_HW_ERR;
        }

        instance->rx_len = len;
        /* 刻意**不写** instance->rx_mode / rx_xfer_len：那两个字段是"常开流的重启参数"
         * （USARTRecoverRxIfStalled 靠它们重启接收），只由下面的 IT/DMA 分支写。
         * 若在这里覆盖成 BLOCK，一个"跑过常开流、之后偶发用 BLOCK 收一帧"的实例，其停摆
         * 的流就再也重启不起来了（自恢复静默失效）。只跑 BLOCK 的实例靠 Config 写入的初值
         * （BLOCK / 0）就已经被该函数判为"没有可重启的流"，无需在这里再标记一次。 */
        if (idx < UART_NUM_MAX)
        {
            s_usart_status[idx].rx_mode = (uint8_t)mode; /* 状态快照 = 最近一次调用的模式 */
            s_usart_status[idx].rx_ok++;
            s_usart_status[idx].rx_len = len;
            s_usart_status[idx].rx_state = instance->handle->RxState;
        }
        return BSP_OK;
    }

    /* IT/DMA：启动常开流，收到一帧自动续收 */
    if (instance->rx_armed)
        return BSP_BUSY; /* 常开流已在跑，重复启动属调用方错误 */

    /* 先记参数、再逐项判失败：USARTRecoverRxIfStalled 靠这两个字段重启接收，
     * 若把它们放在下面的失败分支之后，任何一次启动失败都会让参数停在 Config 时的
     * 0/BLOCK，自恢复拿着它永远重启不了（且是静默的）。 */
    instance->rx_mode = mode;
    instance->rx_xfer_len = len;

    if (mode == BSP_DMA_MODE && instance->handle->hdmarx == NULL)
    {
        if (idx < UART_NUM_MAX)
            s_usart_status[idx].err_no_dma++;
        BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "RX DMA not available (uart_e=%d)!", (int)instance->uart_e);
        return BSP_PARAM_ERR;
    }

    instance->rx_armed = 1;

    ret = USART_StartReceiveRaw(instance);

    /* 启动没成功，但上面已确认没有常开流在跑（rx_armed==0）= 上一次停摆留下的残留状态
     * （DMA 流 / RxState 未清干净）。不强行复位的话，上层每次"重启接收"都会拿到同样的失败，
     * 重启永远不生效。
     * @note 不能只认 BSP_BUSY：残留态也可能让 HAL 报 HAL_ERROR（DMA 流 EN 位不自清那种，
     *       正是 ST 在 HAL_DMA_Abort 里留 Errata 2.22 补丁的状态），映射过来是 BSP_HW_ERR，
     *       只认 BUSY 会漏掉这条**唯一没有恢复路径**的失败。BSP_PARAM_ERR 出现在这里不可能
     *       （mode 已在入口校验过），一并当失败收尾也无副作用。 */
    if (ret != BSP_OK)
    {
        USART_RecoverRx(instance); /* 上下文不允许 Abort 时返回 0，下面的重试会照旧失败并如实上报 */
        ret = USART_StartReceiveRaw(instance);
    }

    if (ret != BSP_OK)
    {
        instance->rx_armed = 0;
        if (ret != BSP_BUSY && idx < UART_NUM_MAX)
            s_usart_status[idx].err_start++;
        BSPLOG(&g_usart_log, LOG_LEVEL_ERROR, "Start receive failed (uart_e=%d, mode=%d, ret=%d)!",
               (int)instance->uart_e, (int)mode, (int)ret);
        return ret;
    }

    if (idx < UART_NUM_MAX)
    {
        s_usart_status[idx].rx_mode = (uint8_t)mode;
        s_usart_status[idx].rx_armed = 1;
        s_usart_status[idx].rx_state = instance->handle->RxState;
        s_usart_status[idx].rx_start++;
    }
    return BSP_OK;
}

/**
 * @brief 接收停摆自恢复的统一入口（**任务上下文**，各链路的 daemon 回调 / 空闲超时都调它）
 * @param instance  待检查的 USART 实例
 * @param period_ms 重启尝试的限频周期（ms）；0 = 不限频
 * @retval BSP_OK   **本次刚把接收重启起来**（调用方据此打"已恢复"日志）
 * @retval BSP_BUSY 未动作：接收在跑 / 该实例没有常开流 / 限频期内 / 上下文不允许 Abort
 * @retval BSP_PARAM_ERR 实例 / 句柄 / 接收缓冲为空（未 Register 或未 Config）
 * @retval 其它    尝试过但失败（透传 USARTReceive 的结果，bsp 内部已打 ERROR 日志）
 *
 * @note 为什么要有这个入口：链路停摆后 rx_callback 不再触发，四条链路（dbus / sbus /
 *       comm-media / terminal）各自挂在自己的任务上下文时基上（daemon 回调或任务空闲超时）
 *       做同一件事——判 rx_armed、限频、按参数重启。四份实现不仅重复，参数来源还不一致
 *       （读 bsp 运行期字段的那两份，遇到"配置失败"就会拿着 0/BLOCK 去重启而永远失败，且静默）。
 *       统一到这里后：判据、限频、参数、计数只有一份，上层各留一行调用。
 *
 * @note 判据是 `rx_armed == 0` 而不是"对端没发帧"：对端没开机时接收本身是好的（rx_armed==1），
 *       不该去动它——误重启会打断正在进行的接收、还会丢掉半帧。
 *
 * @note **同一实例不要混用 BLOCK 接收与常开流**：本函数判出"曾有常开流"（rx_mode 是
 *       IT/DMA）后，重启路径会先 `USART_RecoverRx` 清残留状态，若此刻另有一次 BLOCK 接收
 *       正挂起（RxState 非 READY），那一次会被一并中止。两者本来就共用同一个 RxState，
 *       互斥关系见 USARTReceive；本工程的四条链路都只用常开流，不触发这条。
 *
 * @note 上下文限制同 USART_RecoverRx：重启前要中止残留的 DMA/状态，HAL 的 Abort 按
 *       HAL_GetTick 自旋，ISR / 临界区里 tick 不前进会死等，故那里直接返回 BSP_BUSY 不动作
 *       （不消费限频窗口，任务上下文的下一次调用照常补上）。
 */
BSP_Status_e USARTRecoverRxIfStalled(USARTInstance *instance, uint32_t period_ms)
{
    uint8_t idx;
    BSP_Status_e ret;

    if (instance == NULL || instance->handle == NULL || instance->rx_buff == NULL)
        return BSP_PARAM_ERR;

    /* 接收在跑：没有停摆（对端没开机/没发帧也走这条） */
    if (instance->rx_armed)
        return BSP_BUSY;

    /* 本实例没有常开流可重启：BLOCK 一次一帧，或从未成功启动过（rx_mode 仍是 Config 的初值）。
     * 重启参数由 USARTReceive 在**所有失败分支之前**写入 rx_mode/rx_xfer_len（见那里），
     * 故这里可以直接信它 —— 启动失败（如该口没配 RX DMA）也会留下"可重试的参数"，
     * 每次重试都会走 USARTReceive 的失败路径并留下日志，而不是静默停摆。 */
    if (instance->rx_mode != BSP_IT_MODE && instance->rx_mode != BSP_DMA_MODE)
        return BSP_BUSY;

    /* 上下文不允许阻塞 Abort：本次不动作，也不消费限频窗口 */
    if (!USART_CanBlockingAbort())
        return BSP_BUSY;

    idx = USART_HuartToIndex(instance->handle);

    /* 限频：调用方多为 1ms 周期（DaemonTask），不限频就是 1000 次/秒的重启尝试 */
    if (period_ms != 0 && idx < UART_NUM_MAX && s_rx_restart_us[idx] != 0 &&
        (DWT_GetTimeUs() - s_rx_restart_us[idx]) < ((uint64_t)period_ms * 1000u))
        return BSP_BUSY;

    if (idx < UART_NUM_MAX)
        s_rx_restart_us[idx] = DWT_GetTimeUs();

    /* USARTReceive 内部在启动失败时会先 USART_RecoverRx 清掉残留（DMA 流 / RxState）再重试一次，
     * 正好覆盖"上一次停摆留下残态"这一最常见的原因 */
    ret = USARTReceive(instance, instance->rx_xfer_len, instance->rx_mode, 0);
    if (ret != BSP_OK)
        return ret;

    return BSP_OK;
}

/**
 * @brief 发送卡死自恢复的统一入口（**任务上下文**，各发送链在自己的发送入口调用）
 * @param instance 待检查的 USART 实例
 * @param stuck_ms 判定阈值（ms），0 = 用 USART_TX_STUCK_TIMEOUT_MS
 * @retval BSP_OK        本次刚复位了一次卡死的发送（err_callback 已通知过，上层缓冲已归还）
 * @retval BSP_BUSY      未动作：发送空闲 / 未到阈值 / 上下文不允许 Abort / 该句柄未登记
 * @retval BSP_PARAM_ERR 实例或句柄为空
 *
 * @note 与 USARTRecoverRxIfStalled 对称：上层只提供"什么时候看一眼"，判据（卡死多久）、
 *       计时基准（s_tx_ready_us）、纠正动作（USART_RecoverTx）与计数（tx_recover）都在 bsp 内。
 *
 * @note 判据为什么看"gState 连续非 READY 的时长"而不是别的：IT/DMA 发送中途出错时 HAL 会
 *       把 gState 停在 BUSY_TX 且不再回来，此后任何发送都被"等就绪"挡住 = 永久静默；
 *       而正常的在途发送也是非 READY，只能靠时长区分 —— 阈值取 200ms，远大于最长合法帧的
 *       传输时间（115200 下 64B 约 6ms），不会误伤。
 *
 * @note 发送空闲时顺便刷新基准，而不是只在配置/发送时刷新：各发送链是"有帧才调"，
 *       空闲期可能很长；不刷新的话，基准会停在很久以前，下一次正常发送一开始就被判成卡死
 *       （假的 tx_recover + 一次无谓的 Abort 中止掉刚启动的发送）。
 */
BSP_Status_e USARTRecoverTxIfStuck(USARTInstance *instance, uint32_t stuck_ms)
{
    uint8_t idx;
    uint64_t now_us;

    if (instance == NULL || instance->handle == NULL)
        return BSP_PARAM_ERR;

    if (stuck_ms == 0)
        stuck_ms = USART_TX_STUCK_TIMEOUT_MS;

    /* 上下文不允许阻塞 Abort（ISR / 临界区）：本次不动作，也不刷新基准，
     * 下一次任务上下文的调用照样命中并补做（见 USART_CanBlockingAbort）。
     * 放在最前面是为了让 ISR 里的调用尽快返回 —— BSPLogV 允许从 ISR 调用。 */
    if (!USART_CanBlockingAbort())
        return BSP_BUSY;

    idx = USART_HuartToIndex(instance->handle);
    if (idx >= UART_NUM_MAX)
        return BSP_BUSY; /* 未登记句柄：没有计时基准，无法判卡死（也不该去动硬件） */

    /* 发送空闲 = 链路健康：刷新基准后返回 */
    if (instance->handle->gState == HAL_UART_STATE_READY)
    {
        s_tx_ready_us[idx] = DWT_GetTimeUs();
        return BSP_BUSY;
    }

    /* 非 READY 但没超过阈值：可能只是一次正常的在途发送 */
    now_us = DWT_GetTimeUs();
    if (s_tx_ready_us[idx] == 0 ||
        (now_us - s_tx_ready_us[idx]) <= ((uint64_t)stuck_ms * 1000u))
        return BSP_BUSY;

    /* 超过阈值仍非 READY = 卡死：该次发送的完成回调（DMA TC 中断）丢了、或在错误路径上
     * 被 HAL 停在了 BUSY_TX。中止本次、把 gState 放回 READY，让发送链重新跑起来。
     * 被中止的那次不会再有 tx_callback，USART_RecoverTx 会按契约调 err_callback 通知上层。
     * 返回值恒为 1：instance/handle 非空与上下文允许都在入口判过，这里没有第二种可能，
     * 所以不再写一条永远走不到的分支（旧版 `if (!USART_RecoverTx(...)) return BSP_HW_ERR;`）。 */
    (void)USART_RecoverTx(instance);

    s_tx_ready_us[idx] = DWT_GetTimeUs(); /* 刚复位，重新计时 */
    BSPLOG(&g_usart_log, LOG_LEVEL_WARNING, "UART TX stuck >%dms, state reset (uart_e=%d)!",
           (int)stuck_ms, (int)instance->uart_e);
    return BSP_OK;
}

#endif /* UART_INSTANCE_NUM > 0 */

#endif /* BSP_USART_USED */
