/**
 * @file bsp_usart.c
 * @brief USART驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置
 */

#include "bsp_usart.h"

#if UART_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_uart_log.h"
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
static USARTInstance *s_last_route_instance = NULL;
#else
static USARTInstance **s_usart_instance = NULL;
#endif

/**
 * @brief 根据 UART 句柄查找实例（带最近命中缓存）
 */
static USARTInstance *USARTFindInstanceByHandle(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return NULL;
    }

    if (s_last_route_instance != NULL && s_last_route_instance->handle == huart)
    {
        return s_last_route_instance;
    }

    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_usart_instance[i]->handle == huart)
        {
            s_last_route_instance = s_usart_instance[i];
            return s_usart_instance[i];
        }
    }

    return NULL;
}

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
    USARTInstance *instance = USARTFindInstanceByHandle(huart);
    if (instance != NULL)
    {
        // 保存接收长度
        instance->rx_len = Size;

        // 调用回调函数
        if (instance->rx_callback != NULL)
        {
            instance->rx_callback(instance);
        }

        // 清空缓冲区并重新启动接收
        memset(instance->rx_buff, 0, Size);
        USARTRestartReceive(instance);
    }
}

/**
 * @brief 串口错误回调函数
 * @param huart 发生错误的串口句柄
 * @note 最常见的错误：奇偶校验/溢出/帧错误
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    USARTInstance *instance = USARTFindInstanceByHandle(huart);
    if (instance != NULL)
    {
        uint32_t error_code = huart->ErrorCode;
        (void)error_code;
        LOGWARNING("[bsp_usart] Error detected, code=0x%lX (PE:%d FE:%d NE:%d ORE:%d DMA:%d)", error_code,
                   (error_code & HAL_UART_ERROR_PE) ? 1 : 0,   // 奇偶校验错误
                   (error_code & HAL_UART_ERROR_FE) ? 1 : 0,   // 帧错误
                   (error_code & HAL_UART_ERROR_NE) ? 1 : 0,   // 噪声错误
                   (error_code & HAL_UART_ERROR_ORE) ? 1 : 0,  // 溢出错误
                   (error_code & HAL_UART_ERROR_DMA) ? 1 : 0); // DMA错误
        USARTRestartReceive(instance);
    }
}

/**
 * @brief DMA发送完成回调函数
 * @param huart 发送完成的串口句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    USARTInstance *instance = USARTFindInstanceByHandle(huart);
    if (instance != NULL && instance->tx_callback != NULL)
    {
        instance->tx_callback(instance);
    }
}

/*------------- 外部接口实现 --------------*/

int8_t USARTRegister(USARTInstance *instance, const USART_Init_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_usart] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_usart] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_idx >= UART_INSTANCE_NUM, -1, LOGERROR("[bsp_usart] Exceeded max instance count!"));
    BSP_RETURN_IF_TRUE_LOG(config->uart_e >= UART_NUM_MAX, -1, LOGERROR("[bsp_usart] uart_e out of range!"));

    // 将配置拷贝到实例
    instance->uart_e = config->uart_e;
    instance->tx_mode = config->tx_mode;
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;

    // 根据枚举自动填充硬件句柄
    instance->handle = uart_map[instance->uart_e].handle;
    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, -1, LOGERROR("[bsp_usart] UART handle is NULL, check bsp_cfg mapping!"));

    // RX 使用 ReceiveToIdle DMA，必须配置 RX DMA
    BSP_RETURN_IF_TRUE_LOG(instance->handle->hdmarx == NULL, -1, LOGERROR("[bsp_usart] RX DMA is required but hdmarx is NULL!"));

    // 重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_usart_instance[i]->handle == instance->handle)
        {
            LOGERROR("[bsp_usart] Instance already registered!");
            return -1;
        }
    }

    s_usart_instance[s_idx++] = instance;

    // 启动接收
    USARTRestartReceive(instance);

    LOGINFO("[bsp_usart] USART Instance registered, idx=%d", s_idx - 1);
    return 0;
}

void USARTTransmit(USARTInstance *instance, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (instance == NULL || data == NULL || len == 0)
    {
        LOGWARNING("[bsp_usart] Invalid transmit parameters!");
        return;
    }

    // IT/DMA模式需要等待发送状态就绪
    if (instance->tx_mode != USART_BLOCK_MODE)
    {
        uint64_t start_time = DWT_GetTimeUs();
        uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

        while (instance->handle->gState != HAL_UART_STATE_READY)
        {
            if ((DWT_GetTimeUs() - start_time) > timeout_us)
            {
                LOGWARNING("[bsp_usart] UART busy timeout (uart_e=%d)", instance->uart_e);
                return;
            }
        }
    }

    transmit_funcs[instance->tx_mode](instance->handle, data, len, timeout_ms);
}

void USARTRestartReceive(USARTInstance *instance)
{
    if (instance == NULL || instance->handle == NULL || instance->handle->hdmarx == NULL)
    {
        return;
    }

    if (HAL_UARTEx_ReceiveToIdle_DMA(instance->handle, instance->rx_buff, instance->rx_buff_size) != HAL_OK)
    {
        LOGWARNING("[bsp_usart] Restart receive failed!");
        return;
    }

    // 关闭DMA半传输中断，防止两次进入HAL_UARTEx_RxEventCallback()
    __HAL_DMA_DISABLE_IT(instance->handle->hdmarx, DMA_IT_HT);
}

uint8_t USARTIsReady(USARTInstance *instance)
{
    if (instance == NULL || instance->handle == NULL)
    {
        return 0;
    }
    return (instance->handle->gState == HAL_UART_STATE_READY) ? 1 : 0;
}

#endif
