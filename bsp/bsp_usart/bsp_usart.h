/**
 * @file bsp_usart.h
 * @brief USART驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/校验位/中断/DMA等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "bsp.h"

#ifdef HAL_UART_MODULE_ENABLED

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
    void (*tx_callback)(struct USARTInstance *); // 发送完成回调（DMA模式）
} USARTInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief USART 初始化配置结构体
 */
typedef struct
{
    BoardUART_e uart_e;                          // 板载UART枚举（注册时用于查找映射）
    USART_Work_Mode_e tx_mode;                   // 发送模式（阻塞/中断/DMA）
    void (*rx_callback)(struct USARTInstance *); // 接收完成回调（可为NULL）
    void (*tx_callback)(struct USARTInstance *); // 发送完成回调（DMA模式，可为NULL）
} USART_Init_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义USART实例（同时定义缓冲区）
 * @param name     实例名称
 * @param buff_sz  接收缓冲区大小（影响静态内存分配，必须编译期确定）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   USART_INSTANCE_DEF(sbus_uart, 64);
 */
#define USART_INSTANCE_DEF(name, buff_sz)                    \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};   \
    static USARTInstance name = {                            \
        .rx_buff = name##_rx_buff,                          \
        .rx_buff_size = buff_sz}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册USART实例
 * @param instance USART实例指针（需先通过宏定义）
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限或重复注册）
 */
int8_t USARTRegister(USARTInstance *instance, const USART_Init_Config_s *config);

/**
 * @brief 发送数据
 * @param instance USART实例
 * @param data 发送数据指针
 * @param len 数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @note 阻塞模式：timeout_ms 传递给 HAL_UART_Transmit
 *       IT/DMA模式：
 *         - timeout_ms == 0: 只检查一次状态，忙碌时立即返回
 *         - timeout_ms > 0:  while 循环等待，直到就绪或超时
 */
void USARTTransmit(USARTInstance *instance, uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 重新启动接收
 * @param instance USART实例
 * @note 用于错误恢复或手动重启接收
 */
void USARTRestartReceive(USARTInstance *instance);

/**
 * @brief 检查UART是否就绪（非阻塞）
 * @param instance USART实例
 * @retval 1 就绪（可以发送）
 * @retval 0 忙碌（正在发送）或参数无效
 */
uint8_t USARTIsReady(USARTInstance *instance);

#endif // BSP_UART_MODULE_ENABLED

#endif /* __BSP_USART_H */
