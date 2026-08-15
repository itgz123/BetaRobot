/**
 * @file bsp_usb_desc_dev.c
 * @brief 设备描述符（18 字节），vid/pid/bcd 运行时填充
 *
 * 字段对照 XRUSB DeviceDescriptor：
 *   bcdUSB=0x0200, bDeviceClass=0xEF(杂项)/SubClass 0x02/Protocol 0x01（IAD 组合标准）
 *   bMaxPacketSize0=64, iManufacturer=1, iProduct=2, iSerialNumber=3, bNumConfigurations=1
 */

#include "bsp_usb_desc.h"
#include "bsp_usb_types.h"
#include "stddef.h" /* NULL */

static uint8_t s_desc_buf[18];

const uint8_t *USB_DescGetDevice(uint16_t vid, uint16_t pid, uint16_t bcd, uint16_t *len)
{
    USB_DescDevice_t *desc = (USB_DescDevice_t *)s_desc_buf;

    desc->bLength = 18;
    desc->bDescriptorType = USB_DESC_DEVICE;
    desc->bcdUSB = 0x0200;
    desc->bDeviceClass = USB_CLASS_MISC;
    desc->bDeviceSubClass = 0x02;
    desc->bDeviceProtocol = 0x01;
    desc->bMaxPacketSize0 = 64;
    desc->idVendor = vid;
    desc->idProduct = pid;
    desc->bcdDevice = bcd;
    desc->iManufacturer = USB_STR_MANUFACTURER;
    desc->iProduct = USB_STR_PRODUCT;
    desc->iSerialNumber = USB_STR_SERIAL;
    desc->bNumConfigurations = USB_CONFIG_MAX; /* 多配置（对照 XRUSB GetConfigNum） */

    if (len != NULL)
    {
        *len = 18;
    }
    return s_desc_buf;
}
