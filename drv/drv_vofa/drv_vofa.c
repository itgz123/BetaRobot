/**
 * @file drv_vofa.c
 * @brief VOFA+ JustFloat 协议驱动实现（无任务，外部调用）
 *        发送缓冲/排队/补发交给 drv_tx_slot，本文件只管协议帧
 *        VofaSetChannel 直接写入当前写槽，减少一次数据复制
 */

#include "drv_vofa.h"

#if (defined(VOFA_USED)) && (defined(VOFA_UART))
#include "bsp_log.h"
#include "bsp_usart.h"
#include "bsp_dwt.h"
#include "drv_tx_slot.h"
#include <string.h>

/*------------- 发送缓冲区大小计算 --------------*/

// 缓冲区 = (时间戳1 + 用户通道) * 4 + 帧尾4字节
#define TX_BUFF_SIZE ((1 + VOFA_CHANNELS) * 4 + 4)

// 槽数（三缓冲：一个在途 + 一个排队 + 一个可写；与原实现一致）
#ifndef VOFA_TX_SLOT_COUNT
#define VOFA_TX_SLOT_COUNT 3
#endif

/*------------- 联合体：float与uint8_t/uint32_t互转 --------------*/

typedef union
{
    float f;
    uint32_t u32;
} FloatBytes_u;

/*------------- 静态实例定义 --------------*/

// USART 实例（静态定义）
USART_INSTANCE_DEF(s_vofa_uart, 1);

#ifndef DRV_VOFA_LOG_LIMIT
#define DRV_VOFA_LOG_LIMIT 10
#endif                                                        // !DRV_VOFA_LOG_LIMIT
LOG_INSTANCE_DEF(g_vofa_log, "drv_vofa", DRV_VOFA_LOG_LIMIT); // VOFA 日志实例

// 发送槽池（缓冲放在 DMA_RAM 区域；槽对象只在 CPU 侧用，不必进 DMA_RAM）
static uint8_t s_tx_buff[VOFA_TX_SLOT_COUNT][TX_BUFF_SIZE] DMA_RAM = {0};
static TxSlot_s s_tx_slots[VOFA_TX_SLOT_COUNT];
static TxSlotPool_s s_vofa_pool;

// 当前写槽（唯一写者：调用 VofaSetChannel/VofaSend 的任务）
static TxSlot_s *s_write_slot = NULL;

/* 孤儿 PENDING 槽回收计数（见 VofaSend 入口）：
 * 恒 0 = 发送链从未出现"有排队却无在途"的空档；
 * 非 0 = 该空档发生过（错误回调释放了唯一在途槽、却没带走排队的槽），
 * 值就是被回收救回的槽数，是可观测的自愈凭据，勿删。 */
volatile uint32_t g_vofa_pending_reclaim = 0;

/*------------- 协议帧尾定义 --------------*/
#define VOFA_JUST_FLOAT_FRAME_END_0 0x00
#define VOFA_JUST_FLOAT_FRAME_END_1 0x00
#define VOFA_JUST_FLOAT_FRAME_END_2 0x80
#define VOFA_JUST_FLOAT_FRAME_END_3 0x7f

/*------------- 内部函数 --------------*/

/**
 * @brief 启动一次 DMA 发送（drv_tx_slot 的传输层适配）
 * @note 非阻塞：DMA 启动即返回，完成/错误由下面的回调转交槽池。
 */
static TxSlotStartResult_e VofaTxStart(void *transport, const uint8_t *buf, uint16_t len)
{
    BSP_Status_e ret = USARTTransmit((USARTInstance *)transport, buf, len, BSP_DMA_MODE, 0);
    if (ret == BSP_OK)
    {
        return TX_SLOT_START_OK;
    }
    if (ret == BSP_BUSY)
    {
        return TX_SLOT_START_BUSY;
    }
    return TX_SLOT_START_FAIL;
}

/**
 * @brief DMA 发送完成回调
 * @param instance USART 实例
 * @note 归还 slot 并补发一个排队槽，细节全在 drv_tx_slot。
 */
static void VofaTxCpltCallback(USARTInstance *instance)
{
    (void)instance;
    TxSlotOnTxCplt(&s_vofa_pool);
}

/* UART 错误回调（ISR 或**任务上下文**；后者来自 bsp 的发送卡死自恢复 USART_RecoverTx）：
 * 若本次发送已被中止（HAL/Abort 已把状态置回 READY），不会再有待归还的完成回调，
 * 须释放槽，否则它永久停在 SEND、反复出错会耗光槽池。
 * 若发送仍在途（gState 非 READY），错在接收侧，此刻不能释放 DMA 还在读的缓冲——
 * 那种残留由下一次发送完成回调兜底归还。
 *
 * 幂等由槽池保证：同一次错误可能既走硬件错误又走卡死恢复，重复调用无副作用。
 * 释放后**不**补发排队槽（槽池同样刻意不补发：本回调可能在 USARTTransmit 调用栈内
 * 被触发，补发等于嵌套启动；积压的排队帧由下一次完成回调带出）。 */
static void VofaErrHandler(USARTInstance *instance, USART_ErrReason_e reason)
{
    /* RX_STALLED 与本模块无关（VOFA 只发不收） */
    if (reason == USART_ERR_RX_STALLED)
    {
        return;
    }

    /* TX_ABORT：bsp 已强止发送并把 gState 复位；HW：错误位都在接收侧，
     * gState 非 READY 说明发送仍在途，不能归还 DMA 正在读的缓冲 */
    bool tx_aborted = (reason == USART_ERR_TX_ABORT) || (instance != NULL && instance->handle != NULL && instance->handle->gState == HAL_UART_STATE_READY);

    TxSlotOnError(&s_vofa_pool, tx_aborted);
}

/**
 * @brief 确保手上有一个写槽（池满时先回收孤儿再借）
 * @return true=有写槽可写 false=池满
 * @note 单写者调用（SetChannel/VofaSend 所在任务）。孤儿 = 排队却无在途的槽：
 *       没有在途就不会再有完成回调去补发它们，占着槽只会让后面借不到。
 */
static bool VofaEnsureWriteSlot(void)
{
    if (s_write_slot != NULL)
    {
        return true;
    }

    s_write_slot = TxSlotClaim(&s_vofa_pool);
    if (s_write_slot == NULL)
    {
        g_vofa_pending_reclaim += TxSlotReclaimOrphans(&s_vofa_pool);
        s_write_slot = TxSlotClaim(&s_vofa_pool);
    }

    return s_write_slot != NULL;
}

/*------------- 实现函数 --------------*/

void VofaInit(void)
{
    /* 槽池必须先就绪再注册串口：回调一旦挂上就可能进来 */
    TxSlotTransport_s transport = {
        .start = VofaTxStart,
        .transport = &s_vofa_uart,
    };
    TxSlotPoolInit(&s_vofa_pool, &transport, s_tx_slots, VOFA_TX_SLOT_COUNT, &s_tx_buff[0][0], TX_BUFF_SIZE);

    /* 预借第一帧的写槽，使上电后的 VofaSetChannel 立刻有效 */
    s_write_slot = TxSlotClaim(&s_vofa_pool);

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

    BSPLOG(&g_vofa_log, LOG_LEVEL_INFO, "Initialized, UART: %d, Channels: %d (ch0=timestamp, ch1~%d=user), %d-slot DMA", VOFA_UART, VOFA_CHANNELS,
           VOFA_CHANNELS, VOFA_TX_SLOT_COUNT);
}

void VofaSetChannel(uint8_t ch, float value)
{
    // ch=0 为时间戳通道，用户不能设置
    if (ch == 0 || ch > VOFA_CHANNELS)
    {
        return;
    }

    /* 本周期第一个通道写入时就地借写槽：池满则本次丢弃，整帧都不会发（见 VofaSend 入口）。
     * 放在这里而不是 Send 里，是为了让"池刚腾出空位"的那一帧能从第一个通道开始写完整。 */
    if (!VofaEnsureWriteSlot())
    {
        return;
    }

    // 使用联合体直接写入（单次字写入，避免 memcpy 和逐字节赋值）
    FloatBytes_u fb = {.f = value};
    *(uint32_t *)&s_write_slot->buf[4 + (ch - 1) * 4] = fb.u32;
}

void VofaSend(void)
{
    /* 0. 发送卡死自检（必须早于下面任何状态判断）：
     * 本模块的发送链一旦卡住就再也调用不到 USARTTransmit —— 在途槽永远停在 SEND、
     * 新帧全部转 WAIT_SEND、槽池占满后连借都借不到，整条链静默。
     * 只有每帧必经的这里能把状态复位、并借 err_callback 释放那个 SEND 槽。
     * ISR 里调用安全（bsp 入口直接返回 BSP_BUSY）；空闲时只做一次 DWT 读。 */
    (void)USARTRecoverTxIfStuck(&s_vofa_uart, 0);

    /* 写槽必须真是本周期借到的那块：上一帧提交后预借失败（池满）时它是 NULL，
     * 本周期 VofaSetChannel 也全都丢了 —— 通道数据不完整，本帧必须丢弃，
     * 否则会发出"旧通道数据配新时间戳"的帧，从外面完全看不出来。
     * 丢弃归丢弃，写槽必须在这里补借回来：入口只认 s_write_slot，别处没有谁会去借，
     * 不补的话一次池满就会让 VofaSend 永久早退（后续 SetChannel 也永远无效）。 */
    if (s_write_slot == NULL)
    {
        (void)VofaEnsureWriteSlot();
        return;
    }

    // 1. 填充时间戳到写入缓冲区（单次字写入）
    FloatBytes_u ts = {.f = (float)DWT_GetTimeUs()};
    *(uint32_t *)s_write_slot->buf = ts.u32;

    // 2. 填充帧尾
    uint8_t *p = &s_write_slot->buf[4 + VOFA_CHANNELS * 4];
    *p++ = VOFA_JUST_FLOAT_FRAME_END_0;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_1;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_2;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_3;

    // 3. 提交：传输层空闲则直接启动，忙则排队（槽池内部判在途 + 启动，与完成回调互斥）
    s_write_slot->len = TX_BUFF_SIZE;
    if (TxSlotSubmit(s_write_slot) == TX_SLOT_SUBMIT_FAILED)
    {
        BSPLOG(&g_vofa_log, LOG_LEVEL_WARNING, "DMA send start failed, frame dropped!");
    }

    // 4. 预借下一帧的写槽；池满就留 NULL，下次 VofaSend 的入口会丢弃并尝试回收
    s_write_slot = TxSlotClaim(&s_vofa_pool);
    if (s_write_slot == NULL)
    {
        BSPLOG(&g_vofa_log, LOG_LEVEL_WARNING, "No idle buffer, VofaSend/SetChannel will be ignored until buffer available!");
    }
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
