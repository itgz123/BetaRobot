/**
 * @file comm_media_usart.c
 * @brief 通信框架-硬件层（Media）UART 后端实现
 *
 * 发送：USARTTransmit，按 bsp 实例 tx_mode 执行；
 *       阻塞模式整帧发完，IT/DMA 异步发送（timeout 仅用于等待就绪）。
 * 接收：HAL_UARTEx_RxEventCallback（DMA+IDLE）→ bsp rx_callback → 适配钩子
 *       MediaUsartRxHook → MediaHandleRx → 引擎 rx_cb。
 *
 * @note bsp 在接收回调后立即 memset(rx_buff) 并重启接收，因此 rx_cb
 *       必须在回调上下文内同步消费或 memcpy 走，不能延迟引用 rx_buff。
 */

#include "comm_media_usart.h"
#include <string.h>

#ifdef DRV_COMM_USED

/* 发送超时（ms）：BLOCK 模式 = 整帧发送超时；IT/DMA 模式 = 等待就绪超时 */
#ifndef MEDIA_USART_TX_TIMEOUT_MS
#define MEDIA_USART_TX_TIMEOUT_MS 100
#endif

static int8_t MediaUsartSend(CommMedia *media, const uint8_t *data);

static const CommMediaVTable_s s_usart_vtable = {
    .send = MediaUsartSend,
};

/* vtable 发送实现：拷入自有发送缓冲再发送（UART 字节流无需分包，长度 = tx_buff_size） */
static int8_t MediaUsartSend(CommMedia *media, const uint8_t *data)
{
    CommMediaUsart *usart_media = (CommMediaUsart *)media;
    USARTInstance *usart = (USARTInstance *)media->media;
    if (usart_media == NULL || usart == NULL || data == NULL || usart_media->tx_buff_size == 0)
        return -1;

    memcpy(usart_media->tx_buff, data, usart_media->tx_buff_size);
    USARTTransmit(usart, usart_media->tx_buff, usart_media->tx_buff_size, MEDIA_USART_TX_TIMEOUT_MS);
    return 0;
}

/* bsp 接收适配钩子：收完一段数据，从 bsp 实例读数据交给 media 基类 */
static void MediaUsartRxHook(USARTInstance *usart)
{
    CommMedia *media = (CommMedia *)usart->parent; /* DRV 层设置的反向指针 */
    if (media == NULL)
        return;
    MediaHandleRx(media, usart->rx_buff);
}

int8_t MediaUsartRegister(CommMediaUsart *media)
{
    USARTInstance *usart;

    if (media == NULL || media->tx_buff == NULL)
        return -1;
    usart = (USARTInstance *)media->base.media; /* COMM_MEDIA_USART_DEF 已绑定 */
    if (usart == NULL)
        return -1;

    /* bsp 注册（防重复注册，本函数不可重入） */
    if (USARTRegister(usart) != 0)
        return -1;

    media->base.vtable = &s_usart_vtable;
    media->base.type = MEDIA_USART;
    media->base.rx_cb = NULL;  /* 引擎层挂接收分发钩子 */
    media->base.parent = NULL; /* 引擎层挂所属 CommInstance */

    usart->parent = media; /* 反向指针：适配钩子据此取回 media */
    return 0;
}

int8_t MediaUsartConfig(CommMediaUsart *media, USART_Config_s *cfg)
{
    USARTInstance *usart;

    if (media == NULL || cfg == NULL)
        return -1;
    usart = (USARTInstance *)media->base.media;
    if (usart == NULL)
        return -1;

    /* bsp 配置（DMA+IDLE 接收由 USARTConfig 启动，可反复调用） */
    if (USARTConfig(usart, cfg) != 0)
        return -1;

    /* USARTConfig 会写入 config->rx_callback；强制接管为适配钩子，
     * 保证接收统一进 MediaHandleRx → rx_cb（引擎挂接） */
    usart->rx_callback = MediaUsartRxHook;
    return 0;
}

#endif /* DRV_COMM_USED */
