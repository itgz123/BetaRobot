/**
 * @file bsp_usb_desc_cdc.c
 * @brief CDC-ACM 类特定描述符块（66 字节），供配置描述符组装
 *
 * 端点布局（与 cdc 类一致）：
 *   EP1 IN (0x81) BULK 64、EP1 OUT (0x01) BULK 64、EP2 IN (0x82) INTERRUPT 16
 * 字符串索引取 0（无接口字符串），与 XRUSB cdc_base 的默认块一致。
 */

#include "bsp_usb_desc.h"
#include "bsp_usb_types.h"

static const USB_CDCDescBlock_t s_cdc_block = {
    /* IAD：通信接口 0 + 数据接口 1，功能类 CDC-ACM */
    .iad = {8, USB_DESC_IAD, 0, 2, USB_CLASS_CDC_COMM, 0x02, 0x00, 0},
    /* 通信接口：1 个端点（EP2 IN） */
    .comm_intf = {9, USB_DESC_INTERFACE, 0, 0, 1, USB_CLASS_CDC_COMM, 0x02, 0x00, 0},
    /* Header：CDC 1.10 */
    .cdc_header = {5, USB_DESC_CS_INTERFACE, USB_CDC_SUBTYPE_HEADER, 0x0110},
    /* Call Management：无呼叫管理能力，数据接口 = 1 */
    .cdc_callmgmt = {5, USB_DESC_CS_INTERFACE, USB_CDC_SUBTYPE_CALL_MGMT, 0x00, 1},
    /* ACM：支持 Set/Get LineCoding、SetControlLineState */
    .cdc_acm = {4, USB_DESC_CS_INTERFACE, USB_CDC_SUBTYPE_ACM, 0x02},
    /* Union：主接口 0，从接口 1 */
    .cdc_union = {5, USB_DESC_CS_INTERFACE, USB_CDC_SUBTYPE_UNION, 0, 1},
    /* 通信 EP：EP2 IN，INTERRUPT，16B，FS 4ms */
    .comm_ep = {7, USB_DESC_ENDPOINT, 0x82, USB_EP_TYPE_INTERRUPT, 16, 0x04},
    /* 数据接口：2 个端点 */
    .data_intf = {9, USB_DESC_INTERFACE, 1, 0, 2, USB_CLASS_CDC_DATA, 0x00, 0x00, 0},
    /* 数据 EP：EP1 OUT，BULK，64B */
    .data_ep_out = {7, USB_DESC_ENDPOINT, 0x01, USB_EP_TYPE_BULK, 64, 0},
    /* 数据 EP：EP1 IN，BULK，64B */
    .data_ep_in = {7, USB_DESC_ENDPOINT, 0x81, USB_EP_TYPE_BULK, 64, 0},
};

const USB_CDCDescBlock_t *USB_DescCdcBlockGet(void)
{
    return &s_cdc_block;
}
