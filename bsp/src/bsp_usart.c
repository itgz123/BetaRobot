/**
 * @file bsp_usart.c
 * @brief USART驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置
 */

#include "bsp_usart.h"

#if UART_INSTANCE_NUM > 0

#include "bsp_log.h"
#include "string.h"

/*------------- 私有类型定义 --------------*/

/**
 * @brief 发送函数指针类型（统一4参数，IT/DMA忽略timeout）
 */
typedef void (*UART_TransmitFunc)(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
#if UART_INSTANCE_NUM > 0
static USARTInstance *s_usart_instance[UART_INSTANCE_NUM] = {NULL};
#else
static USARTInstance **s_usart_instance = NULL;
#endif

/**
 * @brief 发送函数指针数组
 * @note IT/DMA模式的timeout参数被忽略
 */
static const UART_TransmitFunc transmit_funcs[] = {
    [USART_BLOCK_MODE] = (UART_TransmitFunc)HAL_UART_Transmit,
    [USART_IT_MODE] = (UART_TransmitFunc)HAL_UART_Transmit_IT,
    [USART_DMA_MODE] = (UART_TransmitFunc)HAL_UART_Transmit_DMA,
};

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief DMA/IDLE中断回调函数
 * @param huart 发生中断的串口句柄
 * @param Size 此次接收到的数据量
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (huart == s_usart_instance[i]->handle)
        {
            // 保存接收长度
            s_usart_instance[i]->rx_len = Size;

            // 调用回调函数
            if (s_usart_instance[i]->rx_callback != NULL)
            {
                s_usart_instance[i]->rx_callback(s_usart_instance[i]);
            }

            // 清空缓冲区并重新启动接收
            memset(s_usart_instance[i]->rx_buff, 0, Size);
            USARTRestartReceive(s_usart_instance[i]);
            return;
        }
    }
}

/**
 * @brief 串口错误回调函数
 * @param huart 发生错误的串口句柄
 * @note 最常见的错误：奇偶校验/溢出/帧错误
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (huart == s_usart_instance[i]->handle)
        {
            uint32_t error_code = huart->ErrorCode;
            LOGWARNING("[bsp_usart] Error detected, idx[%d], code=0x%lX (PE:%d FE:%d NE:%d ORE:%d DMA:%d)", i, error_code,
                       (error_code & HAL_UART_ERROR_PE) ? 1 : 0,   // 奇偶校验错误
                       (error_code & HAL_UART_ERROR_FE) ? 1 : 0,   // 帧错误
                       (error_code & HAL_UART_ERROR_NE) ? 1 : 0,   // 噪声错误
                       (error_code & HAL_UART_ERROR_ORE) ? 1 : 0,  // 溢出错误
                       (error_code & HAL_UART_ERROR_DMA) ? 1 : 0); // DMA错误
            USARTRestartReceive(s_usart_instance[i]);
            return;
        }
    }
}

/*------------- 外部接口实现 --------------*/

int8_t USARTRegister(USARTInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[bsp_usart] Instance is NULL!");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= UART_INSTANCE_NUM)
    {
        LOGERROR("[bsp_usart] Exceeded max instance count!");
        return -1;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_usart_instance[i]->handle == instance->handle)
        {
            LOGERROR("[bsp_usart] Instance already registered!");
            return -1;
        }
    }

    // 根据枚举自动填充硬件句柄
    instance->handle = uart_map[instance->uart_e].handle;

    s_usart_instance[s_idx++] = instance;

    // 启动接收
    USARTRestartReceive(instance);

    LOGINFO("[bsp_usart] USART Instance registered, idx=%d", s_idx - 1);
    return 0;
}

void USARTTransmit(USARTInstance *instance, uint8_t *data, uint16_t len)
{
    if (instance == NULL || data == NULL || len == 0)
    {
        LOGWARNING("[bsp_usart] Invalid transmit parameters!");
        return;
    }

    // IT/DMA模式需要检查发送状态
    if (instance->tx_mode != USART_BLOCK_MODE)
    {
        if (instance->handle->gState != HAL_UART_STATE_READY)
        {
            LOGWARNING("[bsp_usart] UART busy, transmit skipped!");
            return;
        }
    }

    transmit_funcs[instance->tx_mode](instance->handle, data, len, USART_BLOCK_TIMEOUT_MS);
}

void USARTRestartReceive(USARTInstance *instance)
{
    if (instance == NULL)
    {
        return;
    }

    HAL_UARTEx_ReceiveToIdle_DMA(instance->handle, instance->rx_buff, instance->rx_buff_size);
    // 关闭DMA半传输中断，防止两次进入HAL_UARTEx_RxEventCallback()
    __HAL_DMA_DISABLE_IT(instance->handle->hdmarx, DMA_IT_HT);
}

#endif
