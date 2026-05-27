/**
 * @file drv_vofa.c
 * @brief VOFA+ JustFloat 协议示波器驱动实现（单例模式）
 */

#include "drv_vofa.h"

#ifdef VOFA_USED

#error 不要用这个，用drv\drv_vofa\drv_vofa_lite.h

#include "bsp_log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_dwt.h"
#include "bsp_usart.h"
#include <string.h>

/*------------- 发送缓冲区大小计算 --------------*/

// 缓冲区 = (用户通道 + 时间戳通道) * 4 + 帧尾4字节
#define VOFA_TX_BUFF_SIZE ((VOFA_MAX_CHANNEL + VOFA_USE_DWT_TIME_STAMP) * 4 + 4)

/*------------- 内部类型定义 --------------*/

typedef struct
{
    float *data_ptr;  // 数据指针（指向用户变量）
    const char *name; // 通道名称（调试用，可为NULL）
} VofaChannel_t;

typedef struct
{
    USARTInstance *usart_inst;                // 串口实例
    VofaChannel_t channels[VOFA_MAX_CHANNEL]; // 用户通道数组
    uint8_t channel_count;                    // 已注册用户通道数
    uint8_t tx_buff[VOFA_TX_BUFF_SIZE];       // 发送缓冲区
} VofaInstance;

/*------------- 静态单例实例 --------------*/

// USART 实例（静态定义）
USART_INSTANCE_DEF(s_vofa_uart, VOFA_UART, 1);

// VOFA 单例实例
static VofaInstance s_vofa_instance = {
    .usart_inst = &s_vofa_uart,
    .channels = {{0}},
    .channel_count = 0,
    .tx_buff = {0},
};

/*------------- RTOS 任务相关 --------------*/

static TaskHandle_t s_vofaTaskHandle;
static StackType_t s_vofaTaskStack[VOFA_STACK_SIZE];
static StaticTask_t s_vofaTaskTCB;

/*------------- 前置声明 --------------*/

ITCM_RAM __attribute__((noreturn)) static void StartVofaTask(void *argument);

/*------------- 实现函数 --------------*/

void VofaInit(void)
{
    // 注册 USART（使用阻塞模式避免 DMA 发送未完成的问题）
    USART_Init_Config_s usart_cfg = {
        .tx_mode = USART_IT_MODE,
        .rx_callback = NULL,
    };
    USARTRegister(s_vofa_instance.usart_inst, &usart_cfg);

    LOGINFO("[VOFA] Instance initialized, UART: %d", VOFA_UART);

    // 创建任务
    s_vofaTaskHandle = xTaskCreateStatic(
        StartVofaTask,
        "vofaTask",
        VOFA_STACK_SIZE,
        NULL,
        VOFA_TASK_PRIORITY,
        s_vofaTaskStack,
        &s_vofaTaskTCB);
}

int8_t VofaAddChannel(float *data_ptr, const char *name)
{
    if (!data_ptr)
        return -1;

    if (s_vofa_instance.channel_count >= VOFA_MAX_CHANNEL)
    {
        LOGERROR("[VOFA] Channel count exceeded max: %d", VOFA_MAX_CHANNEL);
        return -1;
    }

    int8_t ch_id = (int8_t)s_vofa_instance.channel_count;
    s_vofa_instance.channels[ch_id].data_ptr = data_ptr;
    s_vofa_instance.channels[ch_id].name = name;
    s_vofa_instance.channel_count++;

    return ch_id;
}

static void VofaSend(void)
{
    if (s_vofa_instance.channel_count == 0 && VOFA_USE_DWT_TIME_STAMP == 0)
        return;

    // 检查串口状态（DMA/IT 模式需要）
    if (s_vofa_instance.usart_inst->handle->gState != HAL_UART_STATE_READY)
        return;

    uint8_t *p = s_vofa_instance.tx_buff;

#if VOFA_USE_DWT_TIME_STAMP == 1
    // 第一通道：DWT 时间戳
    float timestamp = (float)DWT_GetTimeUs();
    memcpy(p, &timestamp, 4);
    p += 4;
#endif

    // 打包所有用户通道数据（小端浮点，STM32 本身是小端，直接拷贝）
    for (uint8_t i = 0; i < s_vofa_instance.channel_count; i++)
    {
        float val = *(s_vofa_instance.channels[i].data_ptr);
        memcpy(p, &val, 4);
        p += 4;
    }

    // 添加帧尾
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0x7f;

    // 计算发送长度
    uint16_t tx_len = (s_vofa_instance.channel_count + VOFA_USE_DWT_TIME_STAMP) * 4 + 4;
    USARTTransmit(s_vofa_instance.usart_inst, s_vofa_instance.tx_buff, tx_len);
}

/*------------- RTOS 任务 --------------*/

ITCM_RAM __attribute__((noreturn)) static void StartVofaTask(void *argument)
{
    static uint64_t start;
    static uint64_t dt;

    LOGINFO("[freeRTOS] VOFA Task Start");

    for (;;)
    {
        start = DWT_GetTimeUs();
        VofaSend();
        dt = DWT_GetTimeUs() - start;

        // 执行时间超限告警
        if ((dt / 1000) > VOFA_FREQ_MS)
        {
            LOGERROR("[freeRTOS] VOFA Task is being DELAY! dt = %d(ms)", (uint32_t)(dt / 1000));
        }

        vTaskDelay(pdMS_TO_TICKS(VOFA_FREQ_MS));
    }
}

#else // !VOFA_USED

void VofaInit(void)
{
}

int8_t VofaAddChannel(float *data_ptr, const char *name)
{
    (void)data_ptr;
    (void)name;
    return -1;
}

#endif // VOFA_USED