/**
 * @file comm_media_usart.c
 * @brief 硬件层 USART 后端实现
 *
 * @note 接收路径（ISR 上下文）：bsp_usart DMA+IDLE 中断 -> MediaUsartRxHook
 *       -> media->rx_cb（引擎钩子写 ring）。
 *       bsp_usart 在回调返回后会 memset 清空接收缓冲并重启接收，
 *       因此必须在此回调内立即把字节交给上层（写 ring），不得延迟引用指针。
 */

#include "comm_media_usart.h"
#include "drv_comm.h"

#include "bsp_uart_log.h"
#include <string.h>

static void MediaUsartInit(CommMedia *inst)
{
    (void)inst; /* bsp 注册已在 Config 中完成，此处可选额外初始化 */
}

static int8_t MediaUsartSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    CommMediaUsart *us = COMM_CONTAINER_OF(inst, CommMediaUsart);
    if (!data || len == 0 || len > sizeof(us->tx_buff))
    {
        LOGWARNING("[comm_media_usart] invalid send params, len=%d", len);
        return -1;
    }
    memcpy(us->tx_buff, data, len);
    USARTTransmit(&us->usart, us->tx_buff, len, timeout_ms);
    return 0;
}

static void MediaUsartDeinit(CommMedia *inst)
{
    (void)inst;
}

static const CommMediaVTable_s s_media_usart_vtable = {
    .init = MediaUsartInit,
    .send = MediaUsartSend,
    .deinit = MediaUsartDeinit,
};

/* bsp_usart 接收回调：ISR 上下文，立即搬运字节 */
static void MediaUsartRxHook(USARTInstance *usart_inst)
{
    CommMediaUsart *us = (CommMediaUsart *)usart_inst->parent;
    if (!us)
    {
        return;
    }
    CommMedia *media = &us->media;
    if (media->rx_cb)
    {
        media->rx_cb(media, MEDIA_EVT_RX_STREAM, usart_inst->rx_buff, usart_inst->rx_len);
    }
}

int8_t MediaUsartConfig(CommMedia *inst, const CommMediaUsart_Config_s *cfg)
{
    CommMediaUsart *us = COMM_CONTAINER_OF(inst, CommMediaUsart);
    if (!inst || !cfg)
    {
        LOGERROR("[comm_media_usart] inst or cfg is NULL!");
        return -1;
    }

    inst->vtable = &s_media_usart_vtable;
    inst->type = MEDIA_USART;
    inst->kind = MEDIA_KIND_BYTESTREAM;
    inst->media_id = cfg->media_id;
    inst->unpack_in_isr = cfg->unpack_in_isr;

    /* bsp 注册（防重复，重调用返回 -1 忽略）在 Config 内完成，保证中断分发可用 */
    USARTRegister(&us->usart);

    us->usart.parent = us; /* bsp 回调通过 parent 找回派生实例 */
    USART_Config_s usart_cfg = {
        .uart_e = cfg->uart_e,
        .tx_mode = cfg->tx_mode,
        .rx_callback = MediaUsartRxHook,
        .tx_callback = NULL,
    };
    if (USARTConfig(&us->usart, &usart_cfg) != 0)
    {
        LOGERROR("[comm_media_usart] USART config failed!");
        return -1;
    }
    us->tx_mode = cfg->tx_mode;
    return 0;
}
