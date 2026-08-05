/**
 * @file comm_media_usart.h
 * @brief 硬件层 USART 后端：包装 bsp_usart（DMA+IDLE 接收）
 */

#ifndef DRV_COMM_MEDIA_USART_H
#define DRV_COMM_MEDIA_USART_H

#include "drv_comm.h"
#include "bsp_usart.h"

typedef struct
{
    CommMedia media; /* 首成员：内嵌基类 */
    USARTInstance usart;
    USART_Work_Mode_e tx_mode;
    uint8_t tx_buff[COMM_TX_BUF_SIZE]; /* 发送缓冲（DMA 需要稳定内存） */
} CommMediaUsart;

typedef struct
{
    BoardUART_e uart_e;           /* 板载 UART 枚举（bsp_map） */
    USART_Work_Mode_e tx_mode;    /* 发送模式（阻塞/中断/DMA） */
    uint8_t media_id;             /* 引擎路由用 */
    uint8_t unpack_in_isr;        /* 1=ISR 直通解包 */
} CommMediaUsart_Config_s;

int8_t MediaUsartConfig(CommMedia *inst, const CommMediaUsart_Config_s *cfg);

#endif /* DRV_COMM_MEDIA_USART_H */
