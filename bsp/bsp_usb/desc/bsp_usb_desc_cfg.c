/**
 * @file bsp_usb_desc_cfg.c
 * @brief 配置描述符：9B 头 + 各已绑定类描述符块（运行时拼接）
 *
 *   遍历实例 class 槽，把每个类 bind 时填充好的描述符块顺序拼接到
 *   配置描述符，并自动累计 bNumInterfaces / wTotalLength。
 *   （对照 XRUSB ConfigDescriptor::BuildConfigDescriptor）
 *   当前默认配置：总线供电，bMaxPower=50（100mA），bConfigurationValue=1。
 */

#include "bsp_usb.h" /* USBInstance 完整定义 */
#include "bsp_usb_desc.h"
#include "bsp_usb_types.h"
#include "string.h"

static uint8_t s_cfg_buf[USB_DESC_CFG_BUFF_SIZE];

const uint8_t *USB_DescGetConfig(USBInstance *inst, uint16_t *len)
{
    /* 当前激活配置槽：未配置/值0 时按槽0（对照 XRUSB BuildConfigDescriptor 用 current_cfg_） */
    uint8_t slot = (inst->config_value == 0) ? 0 : (uint8_t)(inst->config_value - 1);
    if (slot >= USB_CONFIG_MAX)
    {
        slot = 0;
    }
    USBConfig_t *cfg = &inst->configs[slot];

    USB_DescConfigHeader_t hdr;
    hdr.bLength = 9;
    hdr.bDescriptorType = USB_DESC_CONFIGURATION;
    hdr.bNumInterfaces = 0;
    hdr.bConfigurationValue = (uint8_t)(slot + 1);
    hdr.iConfiguration = 0;
    hdr.bmAttributes = 0x80; /* 总线供电 */
    hdr.bMaxPower = 50;      /* 100mA */

    uint16_t off = sizeof(hdr);
    uint8_t itf_total = 0;

    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot_cls = &cfg->classes[i];
        if (slot_cls->vtable == NULL || slot_cls->vtable->get_desc == NULL)
        {
            continue;
        }

        uint16_t blk_len = 0;
        const uint8_t *blk = slot_cls->vtable->get_desc(slot_cls->ctx, &blk_len);
        if (blk == NULL || blk_len == 0 || (off + blk_len) > USB_DESC_CFG_BUFF_SIZE)
        {
            continue; /* 溢出或空块：跳过 */
        }

        memcpy(s_cfg_buf + off, blk, blk_len);
        off += blk_len;
        itf_total += slot_cls->vtable->get_itf_count(slot_cls->ctx);
    }

    hdr.bNumInterfaces = itf_total;
    hdr.wTotalLength = off;
    memcpy(s_cfg_buf, &hdr, sizeof(hdr));

    if (len != NULL)
    {
        *len = off;
    }
    return s_cfg_buf;
}
