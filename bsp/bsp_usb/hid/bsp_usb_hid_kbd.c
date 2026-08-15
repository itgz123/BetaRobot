/**
 * @file bsp_usb_hid_kbd.c
 * @brief HID 键盘类实现（见 bsp_usb_hid_kbd.h）
 *
 * @todo 当前停用（不编译）：见 bsp_usb_hid_kbd.h 顶部注释——Windows 复合设备 HID 接口
 *   枚举问题暂缓，恢复时把本文件加回 bsp/CMakeLists.txt。
 *
 * 描述符块布局（25B，bind 时填充接口号 + 端点地址）：
 *   [0..8]  接口描述符 9B（HID 0x03 / Boot 0x01 / Keyboard 0x01）
 *   [9..17] HID 描述符 9B（type 0x21，report desc 长度 63）
 *   [18..24]端点描述符 7B（INTERRUPT IN，8B，轮询 10ms）
 */

#include "bsp_usb_hid_kbd.h"
#include "bsp_usb.h"    /* USBInstance 完整定义 */
#include "bsp_usb_ep.h" /* USB_EPPoolGet / USB_EPConfigure / ... */
#include "string.h"

#define USB_HIDKBD_REPORT_LEN 8 /* 键盘 report：modifier, reserved, key[6] */

/* 描述符块内偏移 */
#define USB_HIDKBD_OFF_IFNUM 2   /* 接口描述符 bInterfaceNumber */
#define USB_HIDKBD_OFF_EPADDR 20 /* 端点描述符 bEndpointAddress */

/*------------- HID report descriptor（标准 63B Boot 键盘） --------------*/
static const uint8_t s_kbd_report_desc[63] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x05, 0x07, /*   Usage Page (Key Codes) */
    0x19, 0xE0, /*   Usage Minimum (224) */
    0x29, 0xE7, /*   Usage Maximum (231) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x01, /*   Logical Maximum (1) */
    0x75, 0x01, /*   Report Size (1) */
    0x95, 0x08, /*   Report Count (8) */
    0x81, 0x02, /*   Input (Data, Variable, Absolute) - modifier byte */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x08, /*   Report Size (8) */
    0x81, 0x01, /*   Input (Constant) - reserved */
    0x05, 0x07, /*   Usage Page (Key Codes) */
    0x19, 0x00, /*   Usage Minimum (0) */
    0x29, 0x65, /*   Usage Maximum (101) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x65, /*   Logical Maximum (101) */
    0x75, 0x08, /*   Report Size (8) */
    0x95, 0x06, /*   Report Count (6) */
    0x81, 0x00, /*   Input (Data, Array) - 6 个按键码 */
    0xC0        /* End Collection */
};

/*------------- 描述符块模板（接口号/端点地址 bind 时填充） --------------*/
static const uint8_t s_kbd_desc_template[25] = {
    /* 接口描述符：9B，1 端点，HID / Boot / Keyboard */
    9,
    USB_DESC_INTERFACE,
    0,
    0,
    1,
    0x03,
    0x01,
    0x01,
    0,
    /* HID 描述符：9B，bcdHID=0x0111，report desc 长度 63 */
    9,
    0x21,
    0x11,
    0x01,
    0,
    1,
    0x22,
    (sizeof(s_kbd_report_desc) & 0xFF),
    (uint8_t)((sizeof(s_kbd_report_desc) >> 8) & 0xFF),
    /* 端点描述符：7B，INTERRUPT IN，8B，轮询 10ms */
    7,
    USB_DESC_ENDPOINT,
    0,
    USB_EP_TYPE_INTERRUPT,
    8,
    0,
    0x0A,
};

/*------------- vtable：bind / unbind --------------*/
static int8_t USB_HIDKbdBind(void *ctx, USBInstance *inst, USBClassSlot_t *slot)
{
    USBHIDKbd *kbd = (USBHIDKbd *)ctx;
    if (kbd->bound)
    {
        return 0; /* 幂等 */
    }

    kbd->inst = inst;

    /* 协议状态复位（SET_PROTOCOL/SET_IDLE 到达前用默认值） */
    kbd->protocol = 1; /* report protocol（HID 1.11 复位后默认） */
    kbd->idle_rate = 0;

    /* 从端点池领取 1 个 IN 中断端点 */
    if (USB_EPPoolGet(&inst->ep_pool, inst->ep, USB_EP_NUM_AUTO, USB_EP_DIR_IN,
                      &kbd->ep_in) != 0)
    {
        kbd->ep_in = NULL;
        return -1;
    }

    /* 填描述符块：接口号 + 端点地址 */
    memcpy(kbd->desc_block, s_kbd_desc_template, sizeof(kbd->desc_block));
    kbd->desc_block[USB_HIDKBD_OFF_IFNUM] = slot->itf_start;
    kbd->desc_block[USB_HIDKBD_OFF_EPADDR] =
        USB_EP_ADDR(kbd->ep_in->number, kbd->ep_in->dir);

    (void)USB_EPConfigure(inst->handle, kbd->ep_in, USB_EP_TYPE_INTERRUPT, 8);

    kbd->bound = 1;
    return 0;
}

static void USB_HIDKbdUnbind(void *ctx, USBInstance *inst, USBClassSlot_t *slot)
{
    (void)slot;
    USBHIDKbd *kbd = (USBHIDKbd *)ctx;
    if (!kbd->bound)
    {
        return;
    }

    (void)USB_EPClose(inst->handle, kbd->ep_in);
    USB_EPPoolRelease(&inst->ep_pool, kbd->ep_in);
    kbd->ep_in = NULL;
    kbd->bound = 0;
}

/*------------- vtable：描述符元数据 --------------*/
static const uint8_t *USB_HIDKbdGetDesc(void *ctx, uint16_t *len)
{
    USBHIDKbd *kbd = (USBHIDKbd *)ctx;
    if (len != NULL)
    {
        *len = sizeof(kbd->desc_block);
    }
    return kbd->desc_block;
}

static uint8_t USB_HIDKbdGetItfCount(void *ctx)
{
    (void)ctx;
    return 1;
}

static uint8_t USB_HIDKbdHasIAD(void *ctx)
{
    (void)ctx;
    return 0;
}

static uint8_t USB_HIDKbdOwnsEP(void *ctx, uint8_t ep_addr)
{
    USBHIDKbd *kbd = (USBHIDKbd *)ctx;
    if (!kbd->bound || kbd->ep_in == NULL)
    {
        return 0;
    }
    return (USB_EP_ADDR(kbd->ep_in->number, kbd->ep_in->dir) == ep_addr);
}

/*------------- vtable：类请求（SET/GET_PROTOCOL、SET/GET_IDLE） --------------
 * Windows kbdhid 驱动枚举 HID 键盘时必发 SET_PROTOCOL / SET_IDLE，
 * Boot 设备（接口 subclass=1）必须应答，STALL 会导致键盘驱动加载失败。
 * GET_REPORT / SET_REPORT 键盘无需支持，保持 STALL（对照 XRUSB hid_keyboard）。
 */
static int8_t USB_HIDKbdOnClassRequest(void *ctx, uint8_t bRequest, uint16_t wValue,
                                       uint16_t wIndex, uint16_t wLength,
                                       USB_ClassReqResult_t *result)
{
    (void)wIndex;
    (void)wLength;
    USBHIDKbd *kbd = (USBHIDKbd *)ctx;

    switch (bRequest)
    {
    case USB_HID_SET_IDLE: /* 无数据阶段；wValue 低字节 = idle rate（0=仅变化时发送） */
        kbd->idle_rate = (uint8_t)(wValue & 0xFF);
        result->write_zlp = 1;
        return 0;

    case USB_HID_GET_IDLE: /* IN 数据阶段：返回 1 字节 idle rate */
        result->write_data = &kbd->idle_rate;
        result->write_len = 1;
        return 0;

    case USB_HID_SET_PROTOCOL: /* 无数据阶段；wValue 低字节 = 0(boot) / 1(report) */
        kbd->protocol = (uint8_t)(wValue & 0xFF);
        result->write_zlp = 1;
        return 0;

    case USB_HID_GET_PROTOCOL: /* IN 数据阶段：返回 1 字节协议 */
        result->write_data = &kbd->protocol;
        result->write_len = 1;
        return 0;

    default:
        return -1; /* GET_REPORT/SET_REPORT 等：STALL */
    }
}

/*------------- vtable：类特定描述符 / 设备描述符覆盖 --------------*/
static int8_t USB_HIDKbdOnGetDescriptor(void *ctx, uint8_t bRequest, uint16_t wValue,
                                        uint16_t wLength, const uint8_t **out_data,
                                        uint16_t *out_len)
{
    (void)ctx;
    (void)bRequest;
    (void)wLength;
    uint8_t desc_type = (uint8_t)(wValue >> 8);
    if (desc_type == 0x22) /* HID report descriptor */
    {
        *out_data = s_kbd_report_desc;
        *out_len = sizeof(s_kbd_report_desc);
        return 0;
    }
    return -1; /* 其它类型 STALL */
}

static int8_t USB_HIDKbdWriteDeviceDescriptor(void *ctx, USB_DescDevice_t *desc)
{
    (void)ctx;
    /* 单类无 IAD 时 core 调用：设备级类字段归零，接口级报 HID
     * （对照 XRUSB hid 的 WriteDeviceDescriptor） */
    desc->bDeviceClass = 0x00;
    desc->bDeviceSubClass = 0x00;
    desc->bDeviceProtocol = 0x00;
    return 0;
}

/*------------- vtable getter --------------*/
const USBClassVTable_t *USB_HIDKbdVTable(void)
{
    static const USBClassVTable_t s_vtable = {
        .on_class_request = USB_HIDKbdOnClassRequest,
        /* .on_vendor_request = NULL（HID 不处理厂商请求） */
        /* .on_class_data = NULL（类请求无数据阶段收尾） */
        /* .on_class_in_data_status_complete = NULL */
        .bind = USB_HIDKbdBind,
        .unbind = USB_HIDKbdUnbind,
        .get_desc = USB_HIDKbdGetDesc,
        .get_itf_count = USB_HIDKbdGetItfCount,
        .has_iad = USB_HIDKbdHasIAD,
        .owns_ep = USB_HIDKbdOwnsEP,
        .on_get_descriptor = USB_HIDKbdOnGetDescriptor,
        /* .set_alt_setting / .get_alt_setting = NULL（HID 仅 alt=0，core 默认处理） */
        .write_device_descriptor = USB_HIDKbdWriteDeviceDescriptor,
    };
    return &s_vtable;
}

/*------------- 对外数据通路 --------------*/
int32_t USBHIDKbdSend(USBInstance *inst, const uint8_t report[8])
{
    if (inst == NULL || report == NULL)
    {
        return -1;
    }
    USBHIDKbd *kbd = &inst->hid_kbd;
    if (!kbd->bound || kbd->ep_in == NULL)
    {
        return -1;
    }
    if (USB_EPIsBusy(kbd->ep_in))
    {
        return -1; /* 端点忙：上一拍未发完 */
    }

    memcpy(USB_EPActiveBuffer(kbd->ep_in), report, USB_HIDKBD_REPORT_LEN);
    (void)USB_EPTransfer(inst->handle, kbd->ep_in, USB_HIDKBD_REPORT_LEN);
    return 0;
}
