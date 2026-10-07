/**
 * @file bsp_usart.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 类型与签名逐字抄自 bsp/bsp_usart/bsp_usart.h（只省掉注释），**实现**在
 * test_referee2026.c 里，是"记录调用"的桩：不碰任何硬件，只记下"谁被调了、参数是什么"，
 * 并可按测试需要返回指定的状态码。收发行为本身一概没有 —— 本测试要验的是驱动，
 * 不是 bsp。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __BSP_USART_PC_STUB_H
#define __BSP_USART_PC_STUB_H

#include "main.h"
#include <stdint.h>

/** err_callback 的触发原因（真值见 bsp_usart.h） */
typedef enum : uint8_t
{
    USART_ERR_HW = 0,
    USART_ERR_TX_ABORT = 1,
    USART_ERR_RX_STALLED = 2,
} USART_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct USARTInstance，而结构体本身在后面才定义 */
struct USARTInstance;

/** 错误回调签名 */
typedef void (*USART_ErrCallback)(struct USARTInstance *instance, USART_ErrReason_e reason);

/** USART 实例结构体（字段与真的一致，本测试只用到 parent / rx_buff / rx_len） */
typedef struct USARTInstance
{
    void *parent;
    BoardUART_e uart_e;
    UART_HandleTypeDef *handle;
    uint8_t *rx_buff;
    const uint16_t rx_buff_size;
    volatile uint16_t rx_len;
    uint16_t rx_xfer_len;
    BSP_Transfer_Mode_e rx_mode;
    volatile uint8_t rx_armed;
    void (*rx_callback)(struct USARTInstance *);
    void (*tx_callback)(struct USARTInstance *);
    USART_ErrCallback err_callback;
} USARTInstance;

/** 静态定义 USART 实例（同时定义缓冲区） */
#define USART_INSTANCE_DEF(name, buff_sz)                                                                              \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};                                                              \
    static USARTInstance name = {.rx_buff = name##_rx_buff, .rx_buff_size = buff_sz}

/** USART 运行时配置结构体 */
typedef struct
{
    BoardUART_e uart_e;
    void *parent;
    void (*rx_callback)(struct USARTInstance *);
    void (*tx_callback)(struct USARTInstance *);
    USART_ErrCallback err_callback;
} USART_Config_s;

BSP_Status_e USARTRegister(USARTInstance *instance);
BSP_Status_e USARTConfig(USARTInstance *instance, const USART_Config_s *config);
BSP_Status_e USARTTransmit(USARTInstance *instance, const uint8_t *data, uint16_t len, BSP_Transfer_Mode_e mode,
                           uint32_t timeout_ms);
BSP_Status_e USARTReceive(USARTInstance *instance, uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e USARTRecoverRxIfStalled(USARTInstance *instance, uint32_t period_ms);
BSP_Status_e USARTRecoverTxIfStuck(USARTInstance *instance, uint32_t stuck_ms);

#endif /* __BSP_USART_PC_STUB_H */
