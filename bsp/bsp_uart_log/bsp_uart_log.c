/**
 * @file bsp_uart_log.c
 * @brief UART 日志输出模块实现
 * @note 使用三缓冲区 + DMA 发送 + 全局频率限制
 *       参考 drv_vofa_lite 设计模式，每条日志完整存储在一个缓冲区
 *       硬件配置由 CubeMX 负责，BSP 层只管理实例和缓冲区
 */

#include "bsp_uart_log.h"

#ifdef UART_LOG_USED

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_usart.h"
#include "bsp_dwt.h"
#include <string.h>

/*============= 私有宏定义 =============*/

// 单个缓冲区大小（单条日志最大长度）
#ifndef UART_LOG_BUFF_SIZE
#define UART_LOG_BUFF_SIZE 256
#endif

// 缓冲区数量
#ifndef UART_LOG_BUFF_COUNT
#define UART_LOG_BUFF_COUNT 3
#endif

/*============= 缓冲区状态枚举 =============*/

typedef enum
{
    BUFF_IDLE = 0, // 空闲，可写入或发送
    BUFF_PENDING,  // 待发送，等待 DMA
    BUFF_ACTIVE,   // DMA 正在发送
} BuffState_e;

/*============= 私有变量 =============*/

// USART 实例（静态定义，仅发送，接收缓冲区最小）
USART_INSTANCE_DEF(s_log_uart, 1);

// 三个发送缓冲区（放在 DMA_RAM 区域）
static char s_tx_buff[UART_LOG_BUFF_COUNT][UART_LOG_BUFF_SIZE] DMA_RAM = {0};

// 每个缓冲区的状态
static volatile BuffState_e s_buff_state[UART_LOG_BUFF_COUNT] = {BUFF_IDLE, BUFF_IDLE, BUFF_IDLE};

// 每个缓冲区的实际数据长度
static volatile uint16_t s_buff_len[UART_LOG_BUFF_COUNT] = {0};

// 当前 DMA 使用的缓冲区索引
static volatile uint8_t s_active_buff = 0;

// 频率限制（全局）
static volatile uint32_t s_global_count = 0;
static volatile uint32_t s_global_last_ms = 0;

// 级别标签和颜色
static const char *s_level_str[] = {
    [LOG_LEVEL_DEBUG] = "[D]",
    [LOG_LEVEL_INFO] = "[I]",
    [LOG_LEVEL_WARNING] = "[W]",
    [LOG_LEVEL_ERROR] = "[E]",
};

static const char *s_level_color[] = {
    [LOG_LEVEL_DEBUG] = LOG_COLOR_CYAN,
    [LOG_LEVEL_INFO] = LOG_COLOR_GREEN,
    [LOG_LEVEL_WARNING] = LOG_COLOR_YELLOW,
    [LOG_LEVEL_ERROR] = LOG_COLOR_RED,
};

/*============= 内部函数声明 =============*/

static void BSPLogTxCpltCallback(USARTInstance *inst);
static void BSPLogTryStartDma(void);

/*============= 内部函数实现 =============*/

/**
 * @brief 尝试启动 DMA 发送
 * @note 查找 PENDING 状态的缓冲区并发送
 */
static void BSPLogTryStartDma(void)
{
    // 检查 UART 状态
    if (!USARTIsReady(&s_log_uart))
    {
        return; // DMA 忙，等待回调
    }

    // 查找 PENDING 缓冲区
    for (uint8_t i = 0; i < UART_LOG_BUFF_COUNT; i++)
    {
        if (s_buff_state[i] == BUFF_PENDING)
        {
            // 标记为 ACTIVE 并启动发送
            s_buff_state[i] = BUFF_ACTIVE;
            s_active_buff = i;
            USARTTransmit(&s_log_uart, (uint8_t *)s_tx_buff[i], s_buff_len[i], 0);
            return;
        }
    }
}

/**
 * @brief DMA 发送完成回调
 * @param inst USART 实例
 */
static void BSPLogTxCpltCallback(USARTInstance *inst)
{
    (void)inst;

    // 当前 ACTIVE 缓冲区变为 IDLE
    s_buff_state[s_active_buff] = BUFF_IDLE;

    // 查找下一个 PENDING 缓冲区并发送
    for (uint8_t i = 0; i < UART_LOG_BUFF_COUNT; i++)
    {
        if (s_buff_state[i] == BUFF_PENDING)
        {
            s_buff_state[i] = BUFF_ACTIVE;
            s_active_buff = i;
            USARTTransmit(&s_log_uart, (uint8_t *)s_tx_buff[i], s_buff_len[i], 0);
            return;
        }
    }
}

/*============= 外部接口实现 =============*/

void BSPLogInit(void)
{
    // 注册 USART（使用 DMA 模式 + 发送回调）
    USART_Init_Config_s usart_cfg = {
        .uart_e = UART_LOG_UART,
        .tx_mode = USART_DMA_MODE,
        .rx_callback = NULL,
        .tx_callback = BSPLogTxCpltCallback,
    };
    USARTRegister(&s_log_uart, &usart_cfg);

    // 初始化状态
    for (uint8_t i = 0; i < UART_LOG_BUFF_COUNT; i++)
    {
        s_buff_state[i] = BUFF_IDLE;
        s_buff_len[i] = 0;
    }
    s_active_buff = 0;
    s_global_count = 0;
    s_global_last_ms = 0;

    LOGINFO("[bsp_uart_log] Initialized, UART: %d, %d buffers x %d bytes",
            UART_LOG_UART, UART_LOG_BUFF_COUNT, UART_LOG_BUFF_SIZE);
}

void BSPLogOutput(LogLevel_e level, const char *format, ...)
{
    // 1. 频率限制检查（使用 DWT 时间）
    uint32_t now = (uint32_t)(DWT_GetTimeUs() / 1000);
    if (now - s_global_last_ms >= 1000)
    {
        s_global_count = 0;
        s_global_last_ms = now;
    }

#ifdef LOG_GLOBAL_LIMIT
    if (s_global_count >= LOG_GLOBAL_LIMIT)
    {
        return;
    }
#endif

    s_global_count++;

    // 2. 找到一个 IDLE 缓冲区
    uint8_t buff_idx = UART_LOG_BUFF_COUNT;
    for (uint8_t i = 0; i < UART_LOG_BUFF_COUNT; i++)
    {
        if (s_buff_state[i] == BUFF_IDLE)
        {
            buff_idx = i;
            break;
        }
    }

    if (buff_idx >= UART_LOG_BUFF_COUNT)
    {
        // 没有空闲缓冲区，丢弃日志
        return;
    }

    // 3. 格式化日志内容到该缓冲区
    char *buff = s_tx_buff[buff_idx];
    uint16_t offset = 0;

    // 颜色前缀
    offset += sprintf(buff + offset, "%s", s_level_color[level]);

    // 时间戳
#if UART_LOG_ENABLE_TIMESTAMP
    uint64_t ts = DWT_GetTimeUs();
    offset += sprintf(buff + offset, "[%lu] ", (uint32_t)(ts / 1000)); // 毫秒
#endif

    // 级别标签
    offset += sprintf(buff + offset, "%s ", s_level_str[level]);

    // 用户数据
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buff + offset, UART_LOG_BUFF_SIZE - offset - 8, format, args);
    va_end(args);

    if (ret < 0)
    {
        return;
    }
    offset += (ret < UART_LOG_BUFF_SIZE - offset - 8) ? ret : (UART_LOG_BUFF_SIZE - offset - 8);

    // 颜色重置 + 换行
    offset += sprintf(buff + offset, "%s\r\n", LOG_COLOR_RESET);

    // 4. 记录长度并标记为 PENDING
    s_buff_len[buff_idx] = offset;
    s_buff_state[buff_idx] = BUFF_PENDING;

    // 5. 尝试启动 DMA 发送
    BSPLogTryStartDma();
}

#endif // HAL_UART_MODULE_ENABLED

#else // UART_LOG_USED != 1

// 日志系统禁用时，所有函数为空实现
void BSPLogInit(void)
{
}

void BSPLogOutput(LogLevel_e level, const char *format, ...)
{
    (void)level;
    (void)format;
}

#endif // UART_LOG_USED
