/**
 * @file bsp_spi.c
 * @brief SPI驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置和片选控制
 */

#include "bsp_spi.h"
#include "bsp_log.h"

/*------------- 私有类型定义 --------------*/

/**
 * @brief 收发函数指针类型（统一4参数，IT/DMA忽略timeout）
 * @note HAL_SPI_TransmitReceive 的 tx_data 参数是 const uint8_t*
 */
typedef HAL_StatusTypeDef (*SPI_TransmitReceiveFunc)(SPI_HandleTypeDef *, const uint8_t *, uint8_t *, uint16_t, uint32_t);

/**
 * @brief 发送函数指针类型
 * @note HAL_SPI_Transmit 的 data 参数是 const uint8_t*
 */
typedef HAL_StatusTypeDef (*SPI_TransmitFunc)(SPI_HandleTypeDef *, const uint8_t *, uint16_t, uint32_t);

/**
 * @brief 接收函数指针类型
 */
typedef HAL_StatusTypeDef (*SPI_ReceiveFunc)(SPI_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
#if SPI_INSTANCE_NUM > 0
static SPIInstance *s_spi_instance[SPI_INSTANCE_NUM] = {NULL};
#else
static SPIInstance **s_spi_instance = NULL;
#endif

/**
 * @brief 收发函数指针数组
 * @note IT/DMA模式的timeout参数被忽略
 */
static const SPI_TransmitReceiveFunc transmit_receive_funcs[] = {
    [SPI_BLOCK_MODE] = HAL_SPI_TransmitReceive,
    [SPI_IT_MODE] = (SPI_TransmitReceiveFunc)HAL_SPI_TransmitReceive_IT,
    [SPI_DMA_MODE] = (SPI_TransmitReceiveFunc)HAL_SPI_TransmitReceive_DMA,
};

/**
 * @brief 发送函数指针数组
 */
static const SPI_TransmitFunc transmit_funcs[] = {
    [SPI_BLOCK_MODE] = HAL_SPI_Transmit,
    [SPI_IT_MODE] = (SPI_TransmitFunc)HAL_SPI_Transmit_IT,
    [SPI_DMA_MODE] = (SPI_TransmitFunc)HAL_SPI_Transmit_DMA,
};

/**
 * @brief 接收函数指针数组
 */
static const SPI_ReceiveFunc receive_funcs[] = {
    [SPI_BLOCK_MODE] = HAL_SPI_Receive,
    [SPI_IT_MODE] = (SPI_ReceiveFunc)HAL_SPI_Receive_IT,
    [SPI_DMA_MODE] = (SPI_ReceiveFunc)HAL_SPI_Receive_DMA,
};

/*------------- 私有函数 --------------*/

/**
 * @brief 检查SPI是否就绪
 * @param instance SPI实例
 * @retval 1 就绪
 * @retval 0 忙碌
 */
static uint8_t SPIIsReady(SPIInstance *instance)
{
    if (instance == NULL || instance->handle == NULL)
    {
        return 0;
    }
    return (instance->handle->State == HAL_SPI_STATE_READY);
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief SPI收发完成回调函数
 * @param hspi 发生中断的SPI句柄
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hspi == s_spi_instance[i]->handle)
        {
            // 保存接收长度
            s_spi_instance[i]->rx_len = s_spi_instance[i]->buff_size;

            // 调用接收回调
            if (s_spi_instance[i]->rx_callback != NULL)
            {
                s_spi_instance[i]->rx_callback(s_spi_instance[i]);
            }
            return;
        }
    }
}

/**
 * @brief SPI发送完成回调函数
 * @param hspi 发生中断的SPI句柄
 * @note 发送完成时也调用 rx_callback，保持接口统一
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hspi == s_spi_instance[i]->handle)
        {
            // 发送完成也调用接收回调（统一接口）
            if (s_spi_instance[i]->rx_callback != NULL)
            {
                s_spi_instance[i]->rx_callback(s_spi_instance[i]);
            }
            return;
        }
    }
}

/**
 * @brief SPI接收完成回调函数
 * @param hspi 发生中断的SPI句柄
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hspi == s_spi_instance[i]->handle)
        {
            // 保存接收长度
            s_spi_instance[i]->rx_len = s_spi_instance[i]->buff_size;

            // 调用接收回调
            if (s_spi_instance[i]->rx_callback != NULL)
            {
                s_spi_instance[i]->rx_callback(s_spi_instance[i]);
            }
            return;
        }
    }
}

/**
 * @brief SPI错误回调函数
 * @param hspi 发生错误的SPI句柄
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hspi == s_spi_instance[i]->handle)
        {
            uint32_t error_code = hspi->ErrorCode;
            LOGWARNING("[spi] Error detected, idx[%d], code=0x%lX (MODF:%d OVR:%d FRE:%d DMA:%d)", i, error_code,
                       (error_code & HAL_SPI_ERROR_MODF) ? 1 : 0, // 模式错误
                       (error_code & HAL_SPI_ERROR_OVR) ? 1 : 0,  // 溢出错误
                       (error_code & HAL_SPI_ERROR_FRE) ? 1 : 0,  // 帧错误
                       (error_code & HAL_SPI_ERROR_DMA) ? 1 : 0); // DMA错误

            return;
        }
    }
}

/*------------- 外部接口实现 --------------*/

int8_t SPIRegister(SPIInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[bsp_spi] Instance is NULL!");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= SPI_INSTANCE_NUM)
    {
        LOGERROR("[bsp_spi] Exceeded max instance count!");
        return -1;
    }

    // 根据枚举自动填充硬件句柄
    instance->handle = spi_map[instance->spi_e].handle;

    if (instance->handle == NULL)
    {
        LOGERROR("[bsp_spi] SPI handle is NULL, check CubeMX configuration!");
        return -1;
    }

    s_spi_instance[s_idx++] = instance;

    LOGINFO("[bsp_spi] SPI instance registered, idx=%d", s_idx - 1);
    return 0;
}

void SPITransmit(SPIInstance *instance, uint8_t *data, uint16_t len)
{
    if (instance == NULL || data == NULL || len == 0)
    {
        LOGWARNING("[bsp_spi] Invalid transmit parameters!");
        return;
    }

    // IT/DMA模式需要检查状态
    if (instance->work_mode != SPI_BLOCK_MODE)
    {
        if (!SPIIsReady(instance))
        {
            LOGWARNING("[bsp_spi] SPI busy, transmit skipped!");
            return;
        }
    }

    transmit_funcs[instance->work_mode](instance->handle, data, len, SPI_BLOCK_TIMEOUT_MS);
}

void SPIReceive(SPIInstance *instance, uint16_t len)
{
    if (instance == NULL || len == 0)
    {
        LOGWARNING("[bsp_spi] Invalid receive parameters!");
        return;
    }

    if (len > instance->buff_size)
    {
        LOGWARNING("[bsp_spi] len > buff_size, truncated!");
        len = instance->buff_size;
    }

    // IT/DMA模式需要检查状态
    if (instance->work_mode != SPI_BLOCK_MODE)
    {
        if (!SPIIsReady(instance))
        {
            LOGWARNING("[bsp_spi] SPI busy, receive skipped!");
            return;
        }
    }

    instance->rx_len = 0;
    receive_funcs[instance->work_mode](instance->handle, instance->rx_buff, len, SPI_BLOCK_TIMEOUT_MS);
}

void SPITransmitReceive(SPIInstance *instance, uint8_t *tx_data, uint16_t len)
{
    if (instance == NULL || tx_data == NULL || len == 0)
    {
        LOGWARNING("[bsp_spi] Invalid transmit/receive parameters!");
        return;
    }

    if (len > instance->buff_size)
    {
        LOGWARNING("[bsp_spi] len > buff_size, truncated!");
        len = instance->buff_size;
    }

    // IT/DMA模式需要检查状态
    if (instance->work_mode != SPI_BLOCK_MODE)
    {
        if (!SPIIsReady(instance))
        {
            LOGWARNING("[bsp_spi] SPI busy, transmit/receive skipped!");
            return;
        }
    }

    instance->rx_len = 0;
    transmit_receive_funcs[instance->work_mode](instance->handle, tx_data, instance->rx_buff, len, SPI_BLOCK_TIMEOUT_MS);
}