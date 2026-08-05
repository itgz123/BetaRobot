/**
 * @file comm_media_usb.c
 * @brief 硬件层 USB-CDC 后端实现
 *
 * @note 接收路径：bsp_usb_rx_handler -> MediaUsbRxHook -> media->rx_cb（写 ring）。
 *       HAL CDC 在下一轮接收前会复用 UserRxBuffer，必须立即搬运字节。
 */

#include "comm_media_usb.h"
#ifdef DRV_COMM_USED
#include "drv_comm.h"

#ifdef HAL_PCD_MODULE_ENABLED

#include "bsp_uart_log.h"
#include <string.h>

static void MediaUsbInit(CommMedia *inst)
{
    (void)inst; /* bsp 注册已在 Config 中完成 */
}

static int8_t MediaUsbSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    CommMediaUsb *mu = COMM_CONTAINER_OF(inst, CommMediaUsb);
    (void)timeout_ms;
    if (!data || len == 0)
    {
        return -1;
    }
    USBTransmit(&mu->usb, data, len);
    return 0;
}

static void MediaUsbDeinit(CommMedia *inst)
{
    (void)inst;
}

static const CommMediaVTable_s s_media_usb_vtable = {
    .init = MediaUsbInit,
    .send = MediaUsbSend,
    .deinit = MediaUsbDeinit,
};

/* bsp_usb 接收回调：ISR 上下文 */
static void MediaUsbRxHook(USBInstance *usb_inst)
{
    CommMediaUsb *mu = (CommMediaUsb *)usb_inst->parent;
    if (!mu)
    {
        return;
    }
    CommMedia *media = &mu->media;
    if (media->rx_cb)
    {
        media->rx_cb(media, MEDIA_EVT_RX_STREAM, usb_inst->rx_buff, usb_inst->rx_len);
    }
}

int8_t MediaUsbConfig(CommMedia *inst, const CommMediaUsb_Config_s *cfg)
{
    CommMediaUsb *mu = COMM_CONTAINER_OF(inst, CommMediaUsb);
    if (!inst || !cfg)
    {
        LOGERROR("[comm_media_usb] inst or cfg is NULL!");
        return -1;
    }

    inst->vtable = &s_media_usb_vtable;
    inst->type = MEDIA_USB;
    inst->kind = MEDIA_KIND_BYTESTREAM;
    inst->media_id = cfg->media_id;
    inst->unpack_in_isr = cfg->unpack_in_isr;

    USBRegister(&mu->usb); /* bsp 注册（防重复） */

    mu->usb.parent = mu;
    USB_Config_s usb_cfg = {
        .rx_callback = MediaUsbRxHook,
        .tx_callback = NULL,
    };
    if (USBConfig(&mu->usb, &usb_cfg) != 0)
    {
        LOGERROR("[comm_media_usb] USB config failed!");
        return -1;
    }
    return 0;
}

#endif /* HAL_PCD_MODULE_ENABLED */
#endif /* DRV_COMM_USED */
