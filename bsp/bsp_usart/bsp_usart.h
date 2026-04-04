/**
 * @file bsp_usart.h
 * @brief USART驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/校验位/中断/DMA等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "bsp_cfg.h"

#ifdef HAL_UART_MODULE_ENABLED
#if UART_INSTANCE_NUM > 0

#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief USART发送模式枚举
 */
typedef enum
{
    USART_BLOCK_MODE = 0, // 阻塞模式
    USART_IT_MODE,        // 中断模式
    USART_DMA_MODE,       // DMA模式
} USART_Work_Mode_e;

/**
 * @brief USART实例结构体
 */
typedef struct USARTInstance
{
    void *parent;                                // 父实例指针（由 DRV 层设置）
    BoardUART_e uart_e;                          // 板载UART枚举（注册时用于查找映射）
    UART_HandleTypeDef *handle;                  // UART句柄（注册时自动填充）
    USART_Work_Mode_e tx_mode;                   // 发送模式
    uint8_t *rx_buff;                            // 接收缓冲区指针
    uint16_t rx_buff_size;                       // 接收缓冲区大小
    uint16_t rx_len;                             // 接收数据长度
    void (*rx_callback)(struct USARTInstance *); // 接收完成回调
} USARTInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义USART实例（同时定义缓冲区）
 * @param name     实例名称
 * @param uart_idx 板载UART枚举（BoardUART_e）
 * @param mode     发送模式（USART_Work_Mode_e）
 * @param buff_sz  接收缓冲区大小
 * @param cb       接收回调函数（可为NULL）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   USART_INSTANCE_DEF(sbus_uart, UART_SBUS, USART_DMA_MODE, 64, sbus_callback);
 */
#define USART_INSTANCE_DEF(name, uart_idx, mode, buff_sz, cb) \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};     \
    static USARTInstance name = {                             \
        .parent = NULL,                                       \
        .uart_e = uart_idx,                                   \
        .handle = NULL,                                       \
        .tx_mode = mode,                                      \
        .rx_buff = name##_rx_buff,                            \
        .rx_buff_size = buff_sz,                              \
        .rx_len = 0,                                          \
        .rx_callback = cb}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册USART实例
 * @param instance USART实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限或重复注册）
 */
int8_t USARTRegister(USARTInstance *instance);

/**
 * @brief 发送数据
 * @param instance USART实例
 * @param data 发送数据指针
 * @param len 数据长度
 * @note 短时间内连续调用此接口，若采用IT/DMA会导致上一次发送未完成而新的发送取消
 *       若希望连续使用DMA/IT发送，请检查 instance->handle->gState 是否为 HAL_UART_STATE_READY
 */
void USARTTransmit(USARTInstance *instance, uint8_t *data, uint16_t len);

/**
 * @brief 重新启动接收
 * @param instance USART实例
 * @note 用于错误恢复或手动重启接收
 */
void USARTRestartReceive(USARTInstance *instance);

#endif
#endif // HAL_UART_MODULE_ENABLED

#endif /* __BSP_USART_H */
