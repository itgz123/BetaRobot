/**
 * @file drv_vofa_lite.c
 * @brief VOFA+ JustFloat 协议简化版驱动实现（无任务，外部调用）
 *        使用三缓冲区 + 发送完成回调实现高频发送
 *        VofaLiteSetChannel 直接写入发送缓冲区，减少一次数据复制
 */

#include "drv_vofa_lite.h"

#ifdef VOFA_LITE_USED

#include "bsp_log.h"
#include "bsp_usart.h"
#include "bsp_dwt.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/*------------- 发送缓冲区大小计算 --------------*/

// 缓冲区 = (时间戳1 + 用户通道) * 4 + 帧尾4字节
#define TX_BUFF_SIZE ((1 + VOFA_LITE_CHANNELS) * 4 + 4)

/*------------- 缓冲区状态枚举 --------------*/

typedef enum
{
    BUFF_IDLE = 0,   // 空闲，可写入或发送
    BUFF_PENDING,    // 待发送，等待 DMA
    BUFF_ACTIVE,     // DMA 正在发送
} BuffState_e;

/*------------- 静态实例定义 --------------*/

// USART 实例（静态定义）
USART_INSTANCE_DEF(s_vofa_lite_uart, VOFA_LITE_UART, 1);

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

/**
 * @brief DMA 发送完成回调
 * @param instance USART 实例
 */
static void VofaLiteTxCpltCallback(USARTInstance *instance)
{
    // 当前 active 缓冲区变为空闲
    s_buff_state[s_active_buff] = BUFF_IDLE;

    // 查找 pending 缓冲区并发送
    for (uint8_t i = 0; i < 3; i++)
    {
        if (s_buff_state[i] == BUFF_PENDING)
        {
            s_buff_state[i] = BUFF_ACTIVE;
            s_active_buff = i;
            HAL_UART_Transmit_DMA(instance->handle, s_tx_buff[i], TX_BUFF_SIZE);
            return;
        }
    }
}

/*------------- 实现函数 --------------*/

void VofaLiteInit(void)
{
    // 注册 USART（使用 DMA 模式 + 发送完成回调）
    USART_Init_Config_s usart_cfg = {
        .tx_mode = USART_DMA_MODE,
        .rx_callback = NULL,
        .tx_callback = VofaLiteTxCpltCallback,
    };
    USARTRegister(&s_vofa_lite_uart, &usart_cfg);

    LOGINFO("[VOFA Lite] Initialized, UART: %d, Channels: %d (ch0=timestamp, ch1~%d=user), Triple-buffer DMA",
            VOFA_LITE_UART, VOFA_LITE_CHANNELS, VOFA_LITE_CHANNELS);
}

void VofaLiteSetChannel(uint8_t ch, float value)
{
    // ch=0 为时间戳通道，用户不能设置
    if (ch == 0 || ch > VOFA_LITE_CHANNELS)
        return;

    // 检查写入缓冲区是否为空闲状态
    if (s_buff_state[s_write_buff] != BUFF_IDLE)
    {
        // 缓冲区正在使用，写入无效
        return;
    }

    // 直接写入当前写入缓冲区的对应位置
    // 位置计算：4(时间戳) + (ch-1)*4
    uint8_t *p = &s_tx_buff[s_write_buff][4 + (ch - 1) * 4];
    memcpy(p, &value, 4);
}

void VofaLiteSend(void)
{
    // 1. 填充时间戳到写入缓冲区
    float timestamp = (float)DWT_GetTimeUs();
    memcpy(&s_tx_buff[s_write_buff][0], &timestamp, 4);

    // 2. 填充帧尾
    uint8_t *p = &s_tx_buff[s_write_buff][4 + VOFA_LITE_CHANNELS * 4];
    *p++ = VOFA_JUST_FLOAT_FRAME_END_0;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_1;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_2;
    *p++ = VOFA_JUST_FLOAT_FRAME_END_3;

    // 3. 标记写入缓冲区状态并发送
    if (s_vofa_lite_uart.handle->gState == HAL_UART_STATE_READY)
    {
        // DMA 空闲，直接发送
        s_buff_state[s_write_buff] = BUFF_ACTIVE;
        s_active_buff = s_write_buff;
        HAL_UART_Transmit_DMA(s_vofa_lite_uart.handle, s_tx_buff[s_write_buff], TX_BUFF_SIZE);
    }
    else
    {
        // DMA 忙，标记为待发送
        s_buff_state[s_write_buff] = BUFF_PENDING;
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
    LOGWARNING("[VOFA Lite] No idle buffer, SetChannel will be ignored until buffer available!");
}

#else // !VOFA_LITE_USED

void VofaLiteInit(void)
{
}

void VofaLiteSetChannel(uint8_t ch, float value)
{
    (void)ch;
    (void)value;
}

void VofaLiteSend(void)
{
}

#endif // VOFA_LITE_USED