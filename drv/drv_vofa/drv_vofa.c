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
        /* 无在途发送：直接启动（无 ACTIVE 时不会有完成回调抢跑——PENDING 只能在有 ACTIVE
         * 时产生，而完成回调必把它升为 ACTIVE，故 HasActive()==0 蕴含无 PENDING）。
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
    // 没有空闲缓冲区，保持当前索引，下次 SetChannel 会因状态检查而失效
    BSPLOG(&g_vofa_log, LOG_LEVEL_WARNING, "No idle buffer, SetChannel will be ignored until buffer available!");
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