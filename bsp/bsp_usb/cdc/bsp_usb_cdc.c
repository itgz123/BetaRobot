/**
 * @file bsp_usb_cdc.c
 * @brief CDC-ACM 类实现（见 bsp_usb_cdc.h）
 *
 * RX 背压不变式：仅在 ring free >= 64 时 rearm EP1 OUT，保证完成回调 push 必然成功。
 * TX 状态机：应用 push op 入环，发送状态机一次只处理一个 op；op 不跨界，
 *   op.len % 64 == 0 且 > 0 时末尾补 ZLP（对照 XRUSB cdc 的 need_write_zlp）。
 */

#include "bsp_usb_cdc.h"
#include "bsp_usb.h"      /* USBInstance 完整定义 */
#include "bsp_usb_desc.h" /* USB_DescCdcBlockGet（描述符块模板） */
#include "bsp_usb_ep.h"
#include "string.h"

#define USB_CDC_DATA_PACKET 64          /* EP1 BULK 包长 */
#define USB_CDC_NOTIF_SERIAL_STATE 0x20 /* 串行状态通知码（CDC-ACM 必需） */

/*------------- 内部：RX 背压 rearm --------------*/
static void USB_CDCRearmRx(USBInstance *inst)
{
    USBCDC *cdc = &inst->cdc;
    if (!cdc->configured)
    {
        return;
    }

    USBEndpoint *ep = &inst->ep[1][USB_EP_DIR_OUT];
    if (USB_EPIsBusy(ep))
    {
        return;
    }
    if (USBRingByteFree(&cdc->rx_ring) < USB_CDC_DATA_PACKET)
    {
        cdc->rx_pause = 1;
        return;
    }

    cdc->rx_pause = 0;
    (void)USB_EPTransfer(inst->handle, ep, USB_CDC_DATA_PACKET);
}

/*------------- 内部：TX 发送状态机 --------------*/
static void USB_CDCKick(USBInstance *inst);

static void USB_CDCSendChunk(USBInstance *inst)
{
    USBCDC *cdc = &inst->cdc;
    USBEndpoint *ep_in = &inst->ep[1][USB_EP_DIR_IN];

    uint16_t remaining = cdc->tx_cur_len - cdc->tx_op_sent;
    if (remaining > 0)
    {
        uint16_t chunk = (remaining > USB_CDC_DATA_PACKET) ? USB_CDC_DATA_PACKET
                                                           : remaining;
        memcpy(USB_EPActiveBuffer(ep_in), cdc->tx_cur + cdc->tx_op_sent, chunk);
        (void)USB_EPTransfer(inst->handle, ep_in, chunk);
    }
    else if (cdc->tx_need_zlp)
    {
        cdc->tx_need_zlp = 0; /* ZLP 由本次发送完成（完成回调里结束 op） */
        (void)USB_EPTransferZLP(inst->handle, ep_in);
    }
    else
    {
        /* op 完成：上报一次发送完成（对照 XRUSB WritePort::Finish），再继续下一个。
         * 回调在调用前清空，避免重复；回调里应用可安全复用 data 缓冲 */
        cdc->tx_active = 0;
        void (*cb)(void *, uint16_t) = cdc->tx_done_cb;
        void *cb_ctx = cdc->tx_done_ctx;
        uint16_t done_len = cdc->tx_cur_len;
        cdc->tx_done_cb = NULL;
        cdc->tx_done_ctx = NULL;
        if (cb != NULL)
        {
            cb(cb_ctx, done_len);
        }
        USB_CDCKick(inst);
    }
}

static void USB_CDCKick(USBInstance *inst)
{
    USBCDC *cdc = &inst->cdc;
    if (cdc->tx_active)
    {
        return;
    }

    USB_TXOp_t op;
    if (USBOpRingPop(&cdc->tx_ops, &op) != 0)
    {
        return; /* op 环空 */
    }

    cdc->tx_cur = op.data;
    cdc->tx_cur_len = op.len;
    cdc->tx_op_sent = 0;
    cdc->tx_need_zlp = (op.len > 0) && ((op.len % USB_CDC_DATA_PACKET) == 0);
    cdc->tx_done_cb = op.on_done; /* 当前 op 完成回调（发完时调用） */
    cdc->tx_done_ctx = op.done_ctx;
    cdc->tx_active = 1;

    USB_CDCSendChunk(inst);
}

/*------------- vtable：bind / unbind --------------*/
static int8_t USB_CDCBind(void *ctx, USBInstance *inst, USBClassSlot_t *slot)
{
    USBCDC *cdc = (USBCDC *)ctx;
    if (cdc->bound)
    {
        return 0; /* 幂等 */
    }

    cdc->inst = inst;
    cdc->configured = 1;
    cdc->rx_pause = 0;
    cdc->tx_active = 0;
    cdc->rx_ring.head = cdc->rx_ring.tail = 0;
    cdc->tx_ops.head = cdc->tx_ops.tail = 0;

    /* 从端点池领取数据/通信端点（EP_AUTO 自动分配号） */
    if (USB_EPPoolGet(&inst->ep_pool, inst->ep, USB_EP_NUM_AUTO, USB_EP_DIR_IN, &cdc->ep_data_in) != 0 ||
        USB_EPPoolGet(&inst->ep_pool, inst->ep, USB_EP_NUM_AUTO, USB_EP_DIR_OUT, &cdc->ep_data_out) != 0 ||
        USB_EPPoolGet(&inst->ep_pool, inst->ep, USB_EP_NUM_AUTO, USB_EP_DIR_IN, &cdc->ep_comm_in) != 0)
    {
        /* 领取失败：回滚已领取的端点 */
        if (cdc->ep_data_in != NULL)
        {
            USB_EPPoolRelease(&inst->ep_pool, cdc->ep_data_in);
            cdc->ep_data_in = NULL;
        }
        if (cdc->ep_data_out != NULL)
        {
            USB_EPPoolRelease(&inst->ep_pool, cdc->ep_data_out);
            cdc->ep_data_out = NULL;
        }
        cdc->ep_comm_in = NULL;
        return -1;
    }

    /* 填充描述符块：接口号平移 + 端点地址（对照 XRUSB cdc_base::BindEndpoints） */
    memcpy(&cdc->desc_block, USB_DescCdcBlockGet(), sizeof(cdc->desc_block));
    {
        USB_CDCDescBlock_t *d = &cdc->desc_block;
        uint8_t itf = slot->itf_start;
        d->iad.bFirstInterface = itf;
        d->comm_intf.bInterfaceNumber = itf;
        d->cdc_callmgmt.bDataInterface = (uint8_t)(itf + 1);
        d->cdc_union.bMasterInterface = itf;
        d->cdc_union.bSlaveInterface0 = (uint8_t)(itf + 1);
        d->data_intf.bInterfaceNumber = (uint8_t)(itf + 1);
        d->comm_ep.bEndpointAddress =
            USB_EP_ADDR(cdc->ep_comm_in->number, cdc->ep_comm_in->dir);
        d->data_ep_out.bEndpointAddress =
            USB_EP_ADDR(cdc->ep_data_out->number, cdc->ep_data_out->dir);
        d->data_ep_in.bEndpointAddress =
            USB_EP_ADDR(cdc->ep_data_in->number, cdc->ep_data_in->dir);
    }

    /* 绑定完成回调 */
    cdc->ep_data_out->ctx = inst;
    cdc->ep_data_out->on_complete = USB_CDCOnEPOut;
    cdc->ep_data_in->ctx = inst;
    cdc->ep_data_in->on_complete = USB_CDCOnEPIn;
    cdc->ep_comm_in->ctx = inst;

    /* open 硬件端点 */
    (void)USB_EPConfigure(inst->handle, cdc->ep_data_out, USB_EP_TYPE_BULK, USB_CDC_DATA_PACKET);
    (void)USB_EPConfigure(inst->handle, cdc->ep_data_in, USB_EP_TYPE_BULK, USB_CDC_DATA_PACKET);
    (void)USB_EPConfigure(inst->handle, cdc->ep_comm_in, USB_EP_TYPE_INTERRUPT, 16);

    /* 初始 arm EP OUT，开始接收 */
    (void)USB_EPTransfer(inst->handle, cdc->ep_data_out, USB_CDC_DATA_PACKET);

    cdc->bound = 1;
    return 0;
}

static void USB_CDCUnbind(void *ctx, USBInstance *inst, USBClassSlot_t *slot)
{
    USBCDC *cdc = (USBCDC *)ctx;
    (void)slot;
    if (!cdc->bound)
    {
        return;
    }

    (void)USB_EPClose(inst->handle, cdc->ep_data_in);
    (void)USB_EPClose(inst->handle, cdc->ep_data_out);
    (void)USB_EPClose(inst->handle, cdc->ep_comm_in);

    USB_EPPoolRelease(&inst->ep_pool, cdc->ep_data_in);
    USB_EPPoolRelease(&inst->ep_pool, cdc->ep_data_out);
    USB_EPPoolRelease(&inst->ep_pool, cdc->ep_comm_in);

    cdc->ep_data_in = NULL;
    cdc->ep_data_out = NULL;
    cdc->ep_comm_in = NULL;
    cdc->configured = 0;
    cdc->bound = 0;
    cdc->tx_active = 0;
    cdc->tx_done_cb = NULL; /* 未完成 op 的回调不补发：USB 断开即发送失败 */
    cdc->tx_done_ctx = NULL;
    cdc->rx_pause = 0;
}

/*------------- vtable：类请求 / 数据阶段 --------------*/
static int8_t USB_CDCOnClassRequest(void *ctx, USBCDCRequest_e bRequest, uint16_t wValue,
                                    uint16_t wIndex, uint16_t wLength,
                                    USB_ClassReqResult_t *result)
{
    USBCDC *cdc = (USBCDC *)ctx;
    (void)wIndex;

    switch (bRequest)
    {
    case USB_CDC_SET_LINE_CODING: /* Host->Device：收 7B 线路编码 */
        if (wLength != sizeof(cdc->line_coding))
        {
            return -1; /* wLength 非 7：STALL（对照 XRUSB ARG_ERR） */
        }
        result->read_data = (uint8_t *)&cdc->line_coding;
        result->read_len = sizeof(cdc->line_coding);
        return 0;

    case USB_CDC_GET_LINE_CODING: /* Device->Host：发 7B 线路编码 */
        if (wLength != sizeof(cdc->line_coding))
        {
            return -1; /* wLength 非 7：STALL（对照 XRUSB ARG_ERR） */
        }
        result->write_data = (const uint8_t *)&cdc->line_coding;
        result->write_len = sizeof(cdc->line_coding);
        return 0;

    case USB_CDC_SET_CONTROL_LINE_STATE: /* 无数据阶段 */
        cdc->ctrl_line_state = wValue;
        if (cdc->ctrl_line_cb != NULL)
        {
            cdc->ctrl_line_cb(cdc->inst, wValue);
        }
        (void)USB_CDCSendSerialState(cdc->inst); /* 自动经 EP2 IN 上报串行状态（对照 XRUSB OnClassRequest） */
        result->write_zlp = 1;
        return 0;

    case USB_CDC_SEND_BREAK: /* 无数据阶段 */
        result->write_zlp = 1;
        return 0;

    default:
        return -1; /* 未知类请求：STALL */
    }
}

static void USB_CDCOnClassData(void *ctx, USBCDCRequest_e bRequest)
{
    USBCDC *cdc = (USBCDC *)ctx;

    if (bRequest == USB_CDC_SET_LINE_CODING)
    {
        if (cdc->line_coding_cb != NULL)
        {
            cdc->line_coding_cb(cdc->inst, &cdc->line_coding);
        }
    }
}

/*------------- vtable：接口字符串（对照 XRUSB cdc_base 的
 * control_interface_string_ / data_interface_string_） --------------*/
static const char *USB_CDCGetInterfaceString(void *ctx, uint8_t itf)
{
    (void)ctx;
    switch (itf)
    {
    case 0:
        return "BetaRobot CDC"; /* 通信接口 */
    case 1:
        return "BetaRobot CDC Data"; /* 数据接口 */
    default:
        return NULL;
    }
}

static void USB_CDCSetInterfaceStringIndex(void *ctx, uint8_t itf, uint8_t idx)
{
    USBCDC *cdc = (USBCDC *)ctx;
    if (itf == 0)
    {
        cdc->desc_block.comm_intf.iInterface = idx;
    }
    else if (itf == 1)
    {
        cdc->desc_block.data_intf.iInterface = idx;
    }
}

/*------------- vtable：描述符元数据 --------------*/
static const uint8_t *USB_CDCGetDesc(void *ctx, uint16_t *len)
{
    USBCDC *cdc = (USBCDC *)ctx;
    if (len != NULL)
    {
        *len = sizeof(cdc->desc_block);
    }
    return (const uint8_t *)&cdc->desc_block;
}

static uint8_t USB_CDCGetItfCount(void *ctx)
{
    (void)ctx;
    return 2; /* 通信接口 + 数据接口 */
}

static uint8_t USB_CDCHasIAD(void *ctx)
{
    (void)ctx;
    return 1;
}

static uint8_t USB_CDCOwnsEP(void *ctx, uint8_t ep_addr)
{
    USBCDC *cdc = (USBCDC *)ctx;
    if (!cdc->bound || cdc->ep_data_in == NULL)
    {
        return 0;
    }
    return (USB_EP_ADDR(cdc->ep_data_in->number, cdc->ep_data_in->dir) == ep_addr ||
            USB_EP_ADDR(cdc->ep_data_out->number, cdc->ep_data_out->dir) == ep_addr ||
            USB_EP_ADDR(cdc->ep_comm_in->number, cdc->ep_comm_in->dir) == ep_addr);
}

/*------------- vtable getter --------------*/
const USBClassVTable_t *USB_CDCVTable(void)
{
    static const USBClassVTable_t s_vtable = {
        .on_class_request = USB_CDCOnClassRequest,
        /* .on_vendor_request = NULL（CDC 不处理厂商请求） */
        .on_class_data = USB_CDCOnClassData,
        /* .on_class_in_data_status_complete = NULL（CDC 数据阶段无需收尾） */
        .bind = USB_CDCBind,
        .unbind = USB_CDCUnbind,
        .get_desc = USB_CDCGetDesc,
        .get_itf_count = USB_CDCGetItfCount,
        .has_iad = USB_CDCHasIAD,
        .owns_ep = USB_CDCOwnsEP,
        .get_interface_string = USB_CDCGetInterfaceString,
        .set_interface_string_index = USB_CDCSetInterfaceStringIndex,
        /* .on_get_descriptor = NULL（CDC 无类特定描述符） */
        /* .set_alt_setting / .get_alt_setting = NULL（CDC 仅 alt=0，core 默认处理） */
        /* .write_device_descriptor = NULL（CDC 用 IAD，不覆盖设备描述符） */
    };
    return &s_vtable;
}

/*------------- Reset --------------*/
void USB_CDCReset(USBInstance *inst)
{
    USBCDC *cdc = &inst->cdc;
    cdc->configured = 0;
    cdc->rx_pause = 0;
    cdc->tx_active = 0;
    cdc->tx_done_cb = NULL;
    cdc->tx_done_ctx = NULL;
    cdc->rx_ring.head = cdc->rx_ring.tail = 0;
    cdc->tx_ops.head = cdc->tx_ops.tail = 0;
    cdc->ctrl_line_state = 0;

    /* 防御性清理：端点解绑由 CoreUnbindClasses 负责，这里兜底 */
    cdc->bound = 0;
    cdc->ep_data_in = NULL;
    cdc->ep_data_out = NULL;
    cdc->ep_comm_in = NULL;
}

/*------------- EP 完成回调 --------------*/
void USB_CDCOnEPOut(USBEndpoint *ep, uint32_t actual_len)
{
    USBInstance *inst = (USBInstance *)ep->ctx;
    if (inst == NULL || !inst->inited)
    {
        return;
    }

    USBCDC *cdc = &inst->cdc;
    if (!cdc->configured)
    {
        return;
    }

    if (actual_len > 0)
    {
        if (USBRingBytePush(&cdc->rx_ring, USB_EPPendingBuffer(ep), actual_len) != 0)
        {
            cdc->rx_pause = 1; /* ring 满：暂停 rearm，等应用取走 */
        }
    }

    USB_CDCRearmRx(inst);
}

void USB_CDCOnEPIn(USBEndpoint *ep, uint32_t actual_len)
{
    USBInstance *inst = (USBInstance *)ep->ctx;
    if (inst == NULL || !inst->inited)
    {
        return;
    }

    USBCDC *cdc = &inst->cdc;
    if (!cdc->configured || !cdc->tx_active)
    {
        return;
    }

    cdc->tx_op_sent += (uint16_t)actual_len;
    USB_CDCSendChunk(inst);
}

/*------------- 对外数据通路 --------------*/
int32_t USB_CDCSendSerialState(USBInstance *inst)
{
    if (inst == NULL || !inst->inited)
    {
        return -1;
    }
    USBCDC *cdc = &inst->cdc;
    if (!cdc->configured || cdc->ep_comm_in == NULL)
    {
        return -1;
    }
    if (USB_EPIsBusy(cdc->ep_comm_in))
    {
        return -1; /* 上一通知未发完 */
    }

    /* 固定 10B 通知：bmRequestType=0xA1, bNotification=SERIAL_STATE, wLength=2 */
    USB_SerialStateNotif_t notif;
    memset(&notif, 0, sizeof(notif));
    notif.bmRequestType = 0xA1; /* 设备->主机，类，接口 */
    notif.bNotification = USB_CDC_NOTIF_SERIAL_STATE;
    notif.wIndex = cdc->desc_block.comm_intf.bInterfaceNumber; /* 通信接口号 */
    notif.wLength = 2;
    /* DTR 有效时报告载波检测（DCD）+ 数据设备就绪（DSR）（对照 XRUSB SendSerialState） */
    notif.serialState = (cdc->ctrl_line_state & USB_CDC_DTR) ? 0x03 : 0x00;

    memcpy(USB_EPActiveBuffer(cdc->ep_comm_in), &notif, sizeof(notif));
    (void)USB_EPTransfer(inst->handle, cdc->ep_comm_in, sizeof(notif));
    return 0;
}

int32_t USB_CDCTransmitEx(USBInstance *inst, const uint8_t *data, uint16_t len,
                          void (*on_done)(void *ctx, uint16_t len), void *done_ctx)
{
    if (inst == NULL || !inst->inited || data == NULL)
    {
        return -1;
    }
    USBCDC *cdc = &inst->cdc;
    if (!cdc->configured)
    {
        return -1;
    }
    if (len == 0)
    {
        if (on_done != NULL)
        {
            on_done(done_ctx, 0); /* 空 op：立即上报完成 */
        }
        return 0;
    }

    USB_TXOp_t op = {data, len, on_done, done_ctx};
    if (USBOpRingPush(&cdc->tx_ops, &op) != 0)
    {
        return -1; /* op 环满 */
    }

    if (!cdc->tx_active)
    {
        USB_CDCKick(inst);
    }
    return 0;
}

int32_t USB_CDCTransmit(USBInstance *inst, const uint8_t *data, uint16_t len)
{
    return USB_CDCTransmitEx(inst, data, len, NULL, NULL);
}

int32_t USB_CDCReceive(USBInstance *inst, uint8_t *data, uint16_t len)
{
    if (inst == NULL || data == NULL)
    {
        return -1;
    }
    USBCDC *cdc = &inst->cdc;

    uint32_t got = USBRingBytePop(&cdc->rx_ring, data, len);
    if (got > 0)
    {
        USB_CDCRearmRx(inst); /* 取走后恢复背压 */
    }
    return (int32_t)got;
}
