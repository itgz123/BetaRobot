/**
 * @file drv_vofa.c
 * @brief VOFA+ JustFloat 协议驱动实现（无任务，外部调用）
 *        使用三缓冲区 + 发送完成回调实现高频发送
 *        VofaSetChannel 直接写入发送缓冲区，减少一次数据复制
 */

#include "drv_vofa.h"

#if (defined(VOFA_USED)) && (defined(VOFA_UART))
#include "bsp_log.h"
#include "bsp_usart.h"
#include "bsp_dwt.h"
#include <string.h>

/*------------- 发送缓冲区大小计算 --------------*/

// 缓冲区 = (时间戳1 + 用户通道) * 4 + 帧尾4字节
#define TX_BUFF_SIZE ((1 + VOFA_CHANNELS) * 4 + 4)

/*------------- 联合体：float与uint8_t/uint32_t互转 --------------*/

typedef union
{
    float f;
    uint32_t u32;
} FloatBytes_u;

/*------------- 缓冲区状态枚举 --------------*/

typedef enum : uint8_t
{
    BUFF_IDLE = 0, // 空闲，可写入或发送
    BUFF_PENDING,  // 待发送，等待 DMA
    BUFF_ACTIVE,   // DMA 正在发送
} BuffState_e;

/*------------- 静态实例定义 --------------*/

// USART 实例（静态定义）
USART_INSTANCE_DEF(s_vofa_uart, 1);

#ifndef DRV_VOFA_LOG_LIMIT
#define DRV_VOFA_LOG_LIMIT 10
#endif                                                        // !DRV_VOFA_LOG_LIMIT
LOG_INSTANCE_DEF(g_vofa_log, "drv_vofa", DRV_VOFA_LOG_LIMIT); // VOFA 日志实例

// 三套发送缓冲区（放在 DMA_RAM 区域）
static uint8_t s_tx_buff[3][TX_BUFF_SIZE] DMA_RAM = {0};

// 缓冲区状态
static volatile BuffState_e s_buff_state[3] = {BUFF_IDLE, BUFF_IDLE, BUFF_IDLE};

// 当前写入缓冲区索引（必须为 IDLE 状态）
static volatile uint8_t s_write_buff = 0;

// 当前 DMA 使用的缓冲区索引
static volatile uint8_t s_active_buff = 0;

/* 孤儿 PENDING 槽回收计数（见 VofaSend 入口）：
 * 恒 0 = 发送链从未出现"有 PENDING 却无 ACTIVE"的空档；
 * 非 0 = 该空档发生过（错误回调释放了唯一在途槽、却没带走排队的 PENDING），
 * 值就是被回收救回的槽数，是可观测的自愈凭据，勿删。 */
volatile uint32_t g_vofa_pending_reclaim = 0;

/*------------- 协议帧尾定义 --------------*/
#define VOFA_JUST_FLOAT_FRAME_END_0 0x00
#define VOFA_JUST_FLOAT_FRAME_END_1 0x00
#define VOFA_JUST_FLOAT_FRAME_END_2 0x80
#define VOFA_JUST_FLOAT_FRAME_END_3 0x7f

/*------------- 内部函数 --------------*/

/* 是否已有缓冲区在 DMA 发送中：同一时刻至多一个，用作"能否直接发送"的就绪判据
 * （问自己的状态即可，无需查 uart 就绪；且无 ACTIVE 时必然没有传输在途，不会有完成回调抢跑） */
static uint8_t VofaHasActive(void)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        if (s_buff_state[i] == BUFF_ACTIVE)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief DMA 发送完成回调
 * @param instance USART 实例
 */
static void VofaTxCpltCallback(USARTInstance *instance)
{
    // 当前 active 缓冲区变为空闲
    s_buff_state[s_active_buff] = BUFF_IDLE;

    // 查找 pending 缓冲区并发送
    for (uint8_t i = 0; i < 3; i++)
    {
        if (s_buff_state[i] == BUFF_PENDING)
        {
            /* 与 VofaSend 同款：启动成功才置 ACTIVE/认领。提前置 ACTIVE 的话，一个早先挂起、
             * 此刻才被响应的错误中断会看到"本槽在途"，把尚未交给 DMA 的它释放成 IDLE，
             * 而外层照旧把 DMA 启动了（缓冲随即可能被 WriteBuff 复用）。 */
            if (USARTTransmit(instance, s_tx_buff[i], TX_BUFF_SIZE, BSP_DMA_MODE, 0) != BSP_OK)
            {
                // 启动失败：归还本槽（下一次完成回调或新的 VofaSend 会重新调度）
                s_buff_state[i] = BUFF_IDLE;
            }
            else
            {
                s_buff_state[i] = BUFF_ACTIVE;
                s_active_buff = i;
            }
            return;
        }
    }
}

/* UART 错误回调（ISR 或**任务上下文**；后者来自 bsp 的发送卡死自恢复 USART_RecoverTx）：
 * 若本次发送已被中止（HAL/Abort 已把状态置回 READY），不会再有待归还的完成回调，
 * 须释放缓冲，否则它永久停在 ACTIVE、反复出错会耗光三缓冲。
 * 若发送仍在途（gState 非 READY），错在接收侧，此刻不能释放 DMA 还在读的缓冲——
 * 那种残留由下一次发送完成回调兜底归还。
 *
 * 幂等：重复调用须无副作用（同一次错误可能既走硬件错误又走卡死恢复）。
 *
 * 这里刻意**不**补发 PENDING：本回调可能在 USARTTransmit 调用栈内被触发（卡死恢复），
 * 补发等于嵌套调用 USARTTransmit，外层失败后还会写 s_buff_state / s_active_buff，
 * 会把内层刚启动的槽状态覆盖掉。积压的 PENDING 由下一次发送完成回调（TxCplt）带出；
 * 期间若再没有一次 VofaSend，这些槽就一直挂在 PENDING，直到下一帧触发发送。 */
static void VofaErrHandler(USARTInstance *instance, USART_ErrReason_e reason)
{
    /* RX_STALLED 与本模块无关（VOFA 只发不收） */
    if (reason == USART_ERR_RX_STALLED)
        return;

    if (s_buff_state[s_active_buff] != BUFF_ACTIVE)
        return;

    /* TX_ABORT：bsp 已强止发送并把 gState 复位；HW：错误位都在接收侧，
     * gState 非 READY 说明发送仍在途，不能归还 DMA 正在读的缓冲 */
    if (reason == USART_ERR_TX_ABORT ||
        (instance != NULL && instance->handle != NULL &&
         instance->handle->gState == HAL_UART_STATE_READY))
    {
        s_buff_state[s_active_buff] = BUFF_IDLE;
    }
}

/*------------- 实现函数 --------------*/

void VofaInit(void)
{

    USART_Config_s usart_cfg = {
        .uart_e = VOFA_UART,
        .parent = NULL,
        .rx_callback = NULL,
        .tx_callback = VofaTxCpltCallback,
        .err_callback = VofaErrHandler,
    };

    // 注册 USART（使用 DMA 模式 + 发送完成回调）
    if (USARTRegister(&s_vofa_uart) != BSP_OK)
    {
        return;
    }
    if (USARTConfig(&s_vofa_uart, &usart_cfg) != BSP_OK)
    {
        return;
    }

    BSPLOG(&g_vofa_log, LOG_LEVEL_INFO, "Initialized, UART: %d, Channels: %d (ch0=timestamp, ch1~%d=user), Triple-buffer DMA",
           VOFA_UART, VOFA_CHANNELS, VOFA_CHANNELS);
}

void VofaSetChannel(uint8_t ch, float value)
{
    // ch=0 为时间戳通道，用户不能设置
    if (ch == 0 || ch > VOFA_CHANNELS)
        return;

    // 检查写入缓冲区是否为空闲状态
    if (s_buff_state[s_write_buff] != BUFF_IDLE)
    {
        // 缓冲区正在使用，写入无效
        return;
    }

    // 使用联合体直接写入（单次字写入，避免 memcpy 和逐字节赋值）
    FloatBytes_u fb = {.f = value};
    *(uint32_t *)&s_tx_buff[s_write_buff][4 + (ch - 1) * 4] = fb.u32;
}

void VofaSend(void)
{
    /* 0. 发送卡死自检（必须早于下面那行"时间戳写入"和任何状态判断）：
     * 本模块的发送链一旦卡住就再也调用不到 USARTTransmit —— 在途槽永远停在 ACTIVE、
     * 新帧全部转 PENDING、三缓冲占满后连 PENDING 都不再产生，整条链静默。
     * 只有每帧必经的这里能把状态复位、并借 err_callback 释放那个 ACTIVE 槽。
     * ISR 里调用安全（bsp 入口直接返回 BSP_BUSY）；空闲时只做一次 DWT 读。 */
    (void)USARTRecoverTxIfStuck(&s_vofa_uart, 0);

    /* 写缓冲必须真是空闲的：第 4 步在三缓冲全非 IDLE 时会**保留原索引**，而那个槽可能
     * 正被 DMA 读（ACTIVE）或已排队等发送（PENDING）。往在途 DMA 缓冲里写，或者改写一个
     * 已排队帧的时间戳（通道数据还是旧周期的、时间戳却是新的），都会让遥测流自相矛盾、
     * 而且从外面完全看不出来。这里直接丢弃本帧：控制周期（1ms）远快于遥测一帧（~4ms@115200），
     * 少发一帧不影响判读，冒险写坏一帧才要命。VofaSetChannel 一直就有同样的检查。 */
    if (s_buff_state[s_write_buff] != BUFF_IDLE)
    {
        /* 唯一的例外：写槽停在 PENDING、而模块里没有任何 ACTIVE —— 没有在途发送，也就
         * 没有 TxCplt 再来把它升成 ACTIVE（VofaErrHandler 只释放 ACTIVE 槽，搁死的 PENDING
         * 没人管；PENDING 的来源见下面第 3 步的 BUSY 分支）。这种槽就是孤儿，当 IDLE 用；
         * 有 ACTIVE 时绝不能动 —— 它可能正排在那笔完成后的补发队列里，且这个槽不会被 DMA 读
         * （PENDING 从未交给传输层），回收到 IDLE 是安全的。 */
        if (s_buff_state[s_write_buff] == BUFF_PENDING && !VofaHasActive())
        {
            s_buff_state[s_write_buff] = BUFF_IDLE;
            g_vofa_pending_reclaim++;
        }

        /* 回收与否都**丢弃本帧**，别接着往下写：回收出来的槽里还是上一周期（甚至更早）的
         * 通道值 —— 本周期的 VofaSetChannel 见它非 IDLE 已经全部丢弃 —— 继续写就会发出
         * 上面刚否掉的那种帧：旧通道数据配新时间戳。留着当 IDLE，下个周期 SetChannel 会
         * 填进全新一组值，那时才是自洽的一帧。 */
        return;
    }

    // 1. 填充时间戳到写入缓冲区（单次字写入）
    FloatBytes_u ts = {.f = (float)DWT_GetTimeUs()};
    *(uint32_t *)s_tx_buff[s_write_buff] = ts.u32;

    // 2. 填充帧尾
    uint8_t *p = &s_tx_buff[s_write_buff][4 + VOFA_CHANNELS * 4];
    *p++ = VOFA_JUST_FLOAT_FRAME_END_0;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_1;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_2;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_3;

    // 3. 标记写入缓冲区状态并发送
    if (VofaHasActive())
    {
        // DMA 忙（本帧完成回调未到），标记为待发送，由完成回调补发
        s_buff_state[s_write_buff] = BUFF_PENDING;
    }
    else
    {
        /* 无在途发送：直接启动 —— 无 ACTIVE 就不会有完成回调抢跑（TxCplt 只由在途传输产生）。
         * 但无 ACTIVE **不**蕴含无 PENDING：下面的 BSP_BUSY 分支正会在无 ACTIVE 时留下
         * PENDING（bsp 的发送契约就是"忙即 BSP_BUSY、上层排队重试"，故这条是正常路径而非兜底）。
         * 那种槽不会再有 TxCplt 去把它升成 ACTIVE，属孤儿：由 VofaSend 入口回收到 IDLE
         * （那一帧顺带丢弃，理由见入口注释），链路由下一个周期的直发重新接上，本分支不负责它。
         * 顺序刻意是"先启动、成功才标 ACTIVE"：USARTTransmit 内部可能触发 err_callback
         * （发送卡死自恢复），若提前标 ACTIVE，那个回调会把**尚未交给 DMA** 的本帧当成
         * 被中止的那次一并释放，与下面的成功分支打架；不标则它只认 ACTIVE、碰不到本帧。
         * 反方向没有窗口：DMA 完成中断至少要等一帧的传输时间，抢不到这两行之前。 */
        BSP_Status_e ret = USARTTransmit(&s_vofa_uart, s_tx_buff[s_write_buff], TX_BUFF_SIZE, BSP_DMA_MODE, 0);

        if (ret == BSP_OK)
        {
            s_buff_state[s_write_buff] = BUFF_ACTIVE;
            s_active_buff = s_write_buff;
        }
        else if (ret == BSP_BUSY)
        {
            s_buff_state[s_write_buff] = BUFF_PENDING; // 外设仍忙：排队
        }
        else
        {
            s_buff_state[s_write_buff] = BUFF_IDLE; // 启动失败：归还本帧缓冲（丢弃本帧）
            BSPLOG(&g_vofa_log, LOG_LEVEL_WARNING, "DMA send start failed (%d), frame dropped!", (int)ret);
        }
    }

    // 4. 切换到下一个空闲缓冲区作为写入缓冲区
    for (uint8_t i = 0; i < 3; i++)
    {
        if (s_buff_state[i] == BUFF_IDLE)
        {
            s_write_buff = i;
            return;
        }
    }
    /* 没有空闲缓冲区，保持当前索引 —— 该槽当前非 IDLE（PENDING/ACTIVE），
     * 下次 VofaSend 的入口检查会丢弃当帧、VofaSetChannel 也会因状态检查失效，
     * 直到某个槽被完成回调释放。这两个丢弃都是有意的（见 VofaSend 入口注释）。 */
    BSPLOG(&g_vofa_log, LOG_LEVEL_WARNING, "No idle buffer, VofaSend/SetChannel will be ignored until buffer available!");
}

#else // !(defined(VOFA_USED)) && (defined(VOFA_UART))

void VofaInit(void)
{
}

void VofaSetChannel(uint8_t ch, float value)
{
    (void)ch;
    (void)value;
}

void VofaSend(void)
{
}

#endif // !(defined(VOFA_USED)) && (defined(VOFA_UART))