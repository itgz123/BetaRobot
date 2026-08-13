/**
 * @file comm_media_usart.c
 * @brief 通信框架-硬件层（Media）UART 后端实现
 *
 * 发送：USARTTransmit，按 bsp 实例 tx_mode 执行；
 *       阻塞模式整帧发完，IT/DMA 异步发送（timeout 仅用于等待就绪）。
 * 接收：HAL_UARTEx_RxEventCallback（DMA+IDLE）→ bsp rx_callback → 适配钩子
 *       MediaUsartRxHook → CommMediaRxHook（comm 层接收入口）。
 *
 * @note bsp 在接收回调后立即 memset(rx_buff) 并重启接收，因此接收处理
 *       必须在回调上下文内同步消费或 memcpy 走，不能延迟引用 rx_buff。
 */

#include "comm_media_usart.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
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

/* vtable 发送实现：UART 无分包，一帧 = 整个缓冲。
 * 拷贝 comm 打包缓冲 → media->tx_buff（DMA 异步发送期间须常驻）后整帧发出。
 * 分包后端（CAN/USB）需用状态机标志 + 发送完成回调续发，此处不需要。 */
static int8_t MediaUsartSend(CommMedia *media, const uint8_t *data)
{
    USARTInstance *usart = (USARTInstance *)media->media;
    if (media == NULL || usart == NULL || data == NULL || media->tx_buff_size == 0)
        return -1;
    memcpy(media->tx_buff, data, media->tx_buff_size);
    return USARTTransmit(usart, media->tx_buff, media->tx_buff_size, MEDIA_USART_TX_TIMEOUT_MS);
}

/* bsp 接收适配钩子：收完一段数据，长度校验后直接交给 comm 层接收入口 */
static void MediaUsartRxHook(USARTInstance *usart)
{
    CommMedia *media = (CommMedia *)usart->parent; /* DRV 层设置的反向指针 */
    if (media == NULL)
        return;
    /* 固定长度模型：UART 无分包，一帧 = 整个接收缓冲（rx_buff_size）。
     * DMA+IDLE 下 rx_len 运行时才知，可能 < rx_buff_size（IDLE 提前触发）；
     * 长度不对直接丢，不回调上层——协议层拿到的永远是完整一帧 */
    if (usart->rx_len != usart->rx_buff_size)
        return;
    CommMediaRxHook(media, usart->rx_buff); /* 跳过 media 基类，直连 comm */
}

int8_t MediaUsartRegister(CommMediaUsart *media)
{
    USARTInstance *usart;

    if (media == NULL || media->base.tx_buff == NULL)
        return -1;
    usart = (USARTInstance *)media->base.media; /* COMM_MEDIA_USART_DEF 已绑定 */
    if (usart == NULL)
        return -1;

    /* bsp 注册（防重复注册，本函数不可重入） */
    if (USARTRegister(usart) != 0)
        return -1;

    media->base.vtable = &s_usart_vtable;
    media->base.type = MEDIA_USART;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

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
     * 保证接收统一进 comm 层接收入口（CommMediaRxHook） */
    usart->rx_callback = MediaUsartRxHook;
    return 0;
}

#endif /* DRV_COMM_USED */
