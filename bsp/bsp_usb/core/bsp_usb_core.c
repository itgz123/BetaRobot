/**
 * @file bsp_usb_core.c
 * @brief EP0 控制传输状态机 + 标准请求 + 类请求分发（移植 XRUSB dev_core.cpp）
 *
 * 状态机要点：
 *   - SET_ADDRESS 在 STATUS IN 完成后才真正生效（pending_addr 延迟）
 *   - 设备->主机 IN 传输：最后一个 IN 包发出前预装 STATUS OUT（ArmStatusOutIfNeeded）
 *   - 类请求（CDC）经 class_vtable 分发，读/写数据阶段经 EP0 DATA_OUT/DATA_IN
 */

#include "bsp_usb_core.h"

#include "string.h"

#include "bsp_usb.h"      /* USBInstance 完整定义 */
#include "bsp_usb_desc.h" /* 描述符 getter */

#define USB_EP0_MAX_PACKET 64

/* 前向声明（SendDescriptor / ProcessStandardRequest 先于反查函数定义） */
static USBClassSlot_t *USB_CoreFindClassByInterface(USBInstance *inst, uint16_t itf);
static USBClassSlot_t *USB_CoreFindDeviceDescOverrideClass(USBInstance *inst);

/*------------- 内部辅助 --------------*/

static void USB_CoreStallControlEndpoint(USBInstance *inst)
{
    USB_EPStall(inst->handle, &inst->ep[0][USB_EP_DIR_OUT]);
    USB_EPStall(inst->handle, &inst->ep[0][USB_EP_DIR_IN]);
}

static void USB_CoreResetControlTransferState(USBInstance *inst)
{
    USBEP0_t *ep0 = &inst->ep0;
    ep0->in0_state = USB_CTX_UNKNOWN;
    ep0->out0_state = USB_CTX_UNKNOWN;
    ep0->write_remain = NULL;
    ep0->write_remain_len = 0;
    ep0->read_remain = NULL;
    ep0->read_remain_len = 0;
    ep0->pending_addr = 0xFF;
    ep0->out0_buffer = NULL;
    ep0->need_write_zlp = 0;
    ep0->status_out_armed = 0;
}

void USB_CoreResetClassRequestState(USBInstance *inst)
{
    USBEP0_t *ep0 = &inst->ep0;
    ep0->cls_slot = NULL;
    ep0->cls_read = 0;
    ep0->cls_write = 0;
    ep0->cls_in_data_status_pending = 0;
    ep0->cls_bRequest = 0;
    ep0->cls_read_data = NULL;
    ep0->cls_read_len = 0;
    ep0->cls_write_data = NULL;
    ep0->cls_write_len = 0;
}

static void USB_CoreReadZLP(USBInstance *inst)
{
    inst->ep0.out0_state = USB_CTX_ZLP;
    USB_EPTransferZLP(inst->handle, &inst->ep[0][USB_EP_DIR_OUT]);
}

static void USB_CoreWriteZLP(USBInstance *inst, uint8_t context)
{
    inst->ep0.in0_state = context;
    USB_EPTransferZLP(inst->handle, &inst->ep[0][USB_EP_DIR_IN]);
}

static void USB_CoreArmStatusOutIfNeeded(USBInstance *inst)
{
    if (!inst->ep0.status_out_armed)
    {
        USB_CoreReadZLP(inst);
        inst->ep0.status_out_armed = 1;
    }
}

/* SET_ADDRESS：仅在 STATUS_IN_ARMED 阶段写硬件地址（对照 XRUSB SetAddress） */
static int8_t USB_DevSetAddress(USBInstance *inst, uint8_t address, uint8_t context)
{
    if (context == USB_CTX_STATUS_IN_ARMED)
    {
        if (HAL_PCD_SetAddress(inst->handle, address) != HAL_OK)
        {
            return -1;
        }
    }
    return 0;
}

/* 设备->主机 EP0 IN 数据（可能跨包 / 需补 ZLP） */
static void USB_CoreDevWriteEP0Data(USBInstance *inst, const uint8_t *data, uint16_t size,
                                    uint16_t request_size, uint8_t early_read_zlp)
{
    USBEP0_t *ep0 = &inst->ep0;
    USBEndpoint *ep_in = &inst->ep[0][USB_EP_DIR_IN];

    ep0->in0_state = USB_CTX_DATA_IN;

    const uint8_t first_chunk = (ep0->write_remain_len == 0);
    const uint16_t host_request = (request_size > 0) ? request_size : size;
    uint16_t transfer_total = size;

    if (first_chunk)
    {
        ep0->status_out_armed = 0;
    }

    /* 限制到主机请求长度 */
    if (request_size > 0 && request_size < size)
    {
        size = request_size;
        transfer_total = request_size;
    }

    if (first_chunk)
    {
        ep0->need_write_zlp = (host_request > transfer_total) &&
                              ((transfer_total % USB_EP0_MAX_PACKET) == 0);
    }

    if (size == 0)
    {
        USB_CoreStallControlEndpoint(inst);
        return;
    }

    if (size > USB_EP0_MAX_PACKET)
    {
        ep0->write_remain = data + USB_EP0_MAX_PACKET;
        ep0->write_remain_len = size - USB_EP0_MAX_PACKET;
        size = USB_EP0_MAX_PACKET;
    }
    else
    {
        ep0->write_remain = NULL;
        ep0->write_remain_len = 0;
        if (!ep0->need_write_zlp && !ep0->status_out_armed)
        {
            USB_CoreArmStatusOutIfNeeded(inst);
        }
    }

    memcpy(USB_EPActiveBuffer(ep_in), data, size);

    if (early_read_zlp && !ep0->status_out_armed)
    {
        USB_CoreArmStatusOutIfNeeded(inst);
    }

    USB_EPTransfer(inst->handle, ep_in, size);
}

/* 主机->设备 EP0 OUT 接收（可能跨包） */
static void USB_CoreDevReadEP0Data(USBInstance *inst, uint8_t *data, uint16_t size)
{
    USBEP0_t *ep0 = &inst->ep0;
    USBEndpoint *ep_out = &inst->ep[0][USB_EP_DIR_OUT];

    ep0->out0_state = USB_CTX_DATA_OUT;

    if (size == 0 || size > 0xFFFF)
    {
        USB_CoreStallControlEndpoint(inst);
        return;
    }

    if (size <= USB_EP0_MAX_PACKET)
    {
        ep0->read_remain = NULL;
        ep0->read_remain_len = 0;
    }
    else
    {
        ep0->read_remain = data + USB_EP0_MAX_PACKET;
        ep0->read_remain_len = size - USB_EP0_MAX_PACKET;
        size = USB_EP0_MAX_PACKET;
    }

    ep0->out0_buffer = data;
    USB_EPTransfer(inst->handle, ep_out, size);
}

/*------------- EP0 完成回调 --------------*/

void USB_CoreOnEP0InComplete(USBEndpoint *ep, uint32_t actual_len)
{
    USBInstance *inst = (USBInstance *)ep->ctx;
    if (inst == NULL || !inst->inited)
    {
        return;
    }
    (void)actual_len;

    uint8_t status = inst->ep0.in0_state;
    inst->ep0.in0_state = USB_CTX_UNKNOWN;

    switch (status)
    {
    case USB_CTX_ZLP:
        break;

    case USB_CTX_STATUS_IN_COMPLETE:
        if (inst->ep0.pending_addr != 0xFF)
        {
            USB_DevSetAddress(inst, inst->ep0.pending_addr, USB_CTX_STATUS_IN_COMPLETE);
            inst->ep0.pending_addr = 0xFF;
        }
        break;

    case USB_CTX_DATA_IN:
        if (inst->ep0.write_remain_len > 0)
        {
            USB_CoreDevWriteEP0Data(inst, inst->ep0.write_remain, inst->ep0.write_remain_len, 0, 0);
        }
        else if (inst->ep0.need_write_zlp)
        {
            inst->ep0.need_write_zlp = 0;
            USB_CoreArmStatusOutIfNeeded(inst);
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
        }
        else if (inst->ep0.cls_write)
        {
            inst->ep0.cls_write = 0;
            if (inst->ep0.cls_slot != NULL && inst->ep0.cls_slot->vtable->on_class_data != NULL)
            {
                inst->ep0.cls_slot->vtable->on_class_data(inst->ep0.cls_slot->ctx,
                                                          inst->ep0.cls_bRequest);
            }
            if (!inst->ep0.cls_in_data_status_pending)
            {
                USB_CoreResetClassRequestState(inst);
            }
        }
        break;

    default:
        USB_CoreStallControlEndpoint(inst);
        break;
    }
}

void USB_CoreOnEP0OutComplete(USBEndpoint *ep, uint32_t actual_len)
{
    USBInstance *inst = (USBInstance *)ep->ctx;
    if (inst == NULL || !inst->inited)
    {
        return;
    }

    const uint8_t status_out_done = inst->ep0.status_out_armed &&
                                    inst->ep0.cls_in_data_status_pending &&
                                    !USB_EPIsBusy(&inst->ep[0][USB_EP_DIR_IN]);

    uint8_t status = inst->ep0.out0_state;
    inst->ep0.out0_state = USB_CTX_UNKNOWN;

    switch (status)
    {
    case USB_CTX_ZLP:
        inst->ep0.status_out_armed = 0;
        if (USB_EPIsBusy(&inst->ep[0][USB_EP_DIR_IN]))
        {
            /* 主机中断 IN：重建 EP0 IN */
            USB_EPClose(inst->handle, &inst->ep[0][USB_EP_DIR_IN]);
            USB_EPConfigure(inst->handle, &inst->ep[0][USB_EP_DIR_IN], USB_EP_TYPE_CONTROL,
                            USB_EP0_MAX_PACKET);
            inst->ep0.in0_state = USB_CTX_ZLP;
            inst->ep0.write_remain = NULL;
            inst->ep0.write_remain_len = 0;
        }
        /* fall through */
        /* FALLTHRU */
    case USB_CTX_STATUS_OUT:
        inst->ep0.status_out_armed = 0;
        if (status_out_done)
        {
            /* 类 IN 数据发出后，主机回 STATUS OUT：类数据阶段彻底结束
             * （对照 XRUSB OnClassInDataStatusComplete） */
            if (inst->ep0.cls_slot != NULL &&
                inst->ep0.cls_slot->vtable->on_class_in_data_status_complete != NULL)
            {
                inst->ep0.cls_slot->vtable->on_class_in_data_status_complete(
                    inst->ep0.cls_slot->ctx, inst->ep0.cls_bRequest);
            }
            USB_CoreResetClassRequestState(inst);
        }
        break;

    case USB_CTX_DATA_OUT:
        if (actual_len > 0 && inst->ep0.out0_buffer != NULL)
        {
            /* EP0 为单缓冲：数据在 active（=buf[0]），不能用 PendingBuffer */
            memcpy(inst->ep0.out0_buffer, USB_EPActiveBuffer(ep), actual_len);
        }

        if (inst->ep0.read_remain_len > 0)
        {
            inst->ep0.out0_buffer += actual_len;
            USB_CoreDevReadEP0Data(inst, inst->ep0.read_remain, inst->ep0.read_remain_len);
        }
        else if (inst->ep0.cls_read)
        {
            inst->ep0.cls_read = 0;
            if (inst->ep0.cls_slot != NULL && inst->ep0.cls_slot->vtable->on_class_data != NULL)
            {
                inst->ep0.cls_slot->vtable->on_class_data(inst->ep0.cls_slot->ctx,
                                                          inst->ep0.cls_bRequest);
            }
            USB_CoreResetClassRequestState(inst);
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
        }
        else
        {
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
        }
        break;

    default:
        USB_CoreStallControlEndpoint(inst);
        break;
    }
}

/*------------- 标准请求 --------------*/

static int8_t USB_CoreRespondWithStatus(USBInstance *inst, const USB_SetupPacket_t *setup,
                                        uint8_t recipient)
{
    if (setup->wLength != 2)
    {
        return -1;
    }

    uint16_t status = 0;
    switch (recipient)
    {
    case USB_RECIPIENT_DEVICE:
        status = 0; /* 总线供电，无 remote wakeup */
        break;
    case USB_RECIPIENT_INTERFACE:
        status = 0;
        break;
    case USB_RECIPIENT_ENDPOINT:
    {
        uint8_t ep_addr = setup->wIndex & 0xFF;
        uint8_t epnum = ep_addr & 0x7F;
        uint8_t dir = (ep_addr & 0x80) ? USB_EP_DIR_IN : USB_EP_DIR_OUT;
        if (epnum >= USB_EP_MAX)
        {
            return -1;
        }
        if (USB_EPIsStalled(&inst->ep[epnum][dir]))
        {
            status = 0x0001;
        }
        break;
    }
    default:
        return -1;
    }

    USB_CoreDevWriteEP0Data(inst, (const uint8_t *)&status, 2, setup->wLength, 0);
    return 0;
}

static int8_t USB_CoreClearFeature(USBInstance *inst, const USB_SetupPacket_t *setup,
                                   uint8_t recipient)
{
    switch (recipient)
    {
    case USB_RECIPIENT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_ENDPOINT_HALT)
        {
            uint8_t ep_addr = setup->wIndex & 0xFF;
            uint8_t epnum = ep_addr & 0x7F;
            uint8_t dir = (ep_addr & 0x80) ? USB_EP_DIR_IN : USB_EP_DIR_OUT;
            if (epnum >= USB_EP_MAX)
            {
                return -1;
            }
            if (USB_EPUnstall(inst->handle, &inst->ep[epnum][dir]) != 0)
            {
                return -1;
            }
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
            return 0;
        }
        return -1;

    case USB_RECIPIENT_DEVICE:
        if (setup->wValue == USB_FEATURE_DEVICE_REMOTE_WAKEUP)
        {
            /* 未实现 remote wakeup：仅 ACK */
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
            return 0;
        }
        return -1;

    default:
        return -1;
    }
}

static int8_t USB_CoreApplyFeature(USBInstance *inst, const USB_SetupPacket_t *setup,
                                   uint8_t recipient)
{
    switch (recipient)
    {
    case USB_RECIPIENT_ENDPOINT:
        if (setup->wValue == USB_FEATURE_ENDPOINT_HALT)
        {
            uint8_t ep_addr = setup->wIndex & 0xFF;
            uint8_t epnum = ep_addr & 0x7F;
            uint8_t dir = (ep_addr & 0x80) ? USB_EP_DIR_IN : USB_EP_DIR_OUT;
            if (epnum >= USB_EP_MAX)
            {
                return -1;
            }
            if (USB_EPStall(inst->handle, &inst->ep[epnum][dir]) != 0)
            {
                return -1;
            }
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
            return 0;
        }
        return -1;

    case USB_RECIPIENT_DEVICE:
        if (setup->wValue == USB_FEATURE_DEVICE_REMOTE_WAKEUP)
        {
            /* 未实现 remote wakeup：仅 ACK */
            USB_CoreWriteZLP(inst, USB_CTX_ZLP);
            return 0;
        }
        return -1;

    default:
        return -1;
    }
}

static int8_t USB_CoreSendDescriptor(USBInstance *inst, const USB_SetupPacket_t *setup,
                                     uint8_t recipient)
{
    uint8_t desc_type = (setup->wValue >> 8) & 0xFF;
    uint8_t desc_idx = setup->wValue & 0xFF;
    const uint8_t *data = NULL;
    uint16_t size = 0;
    uint8_t early_read_zlp = 0;

    switch (desc_type)
    {
    case USB_DESC_DEVICE:
    {
        data = USB_DescGetDevice(inst->vid, inst->pid, inst->bcd, &size);
        /* 单类无 IAD 时允许类覆盖设备描述符类字段（对照 XRUSB TryOverrideDeviceDescriptor） */
        USBClassSlot_t *ovr = USB_CoreFindDeviceDescOverrideClass(inst);
        if (ovr != NULL && ovr->vtable->write_device_descriptor != NULL)
        {
            (void)ovr->vtable->write_device_descriptor(ovr->ctx, (USB_DescDevice_t *)data);
        }
        early_read_zlp = 1;
        break;
    }

    case USB_DESC_CONFIGURATION:
        data = USB_DescGetConfig(inst, &size);
        break;

    case USB_DESC_STRING:
    {
        if (recipient != USB_RECIPIENT_DEVICE)
        {
            return -1;
        }
        data = USB_DescGetString(desc_idx, &size);
        if (data == NULL)
        {
            return -1;
        }
        break;
    }

    default:
    {
        /* 类特定描述符（HID report 等）：interface-recipient 反查类
         * （对照 XRUSB SendDescriptor 的 OnGetDescriptor 分支） */
        if (recipient != USB_RECIPIENT_INTERFACE)
        {
            return -1;
        }
        USBClassSlot_t *slot = USB_CoreFindClassByInterface(inst, setup->wIndex);
        if (slot == NULL || slot->vtable->on_get_descriptor == NULL)
        {
            return -1;
        }
        const uint8_t *class_desc = NULL;
        uint16_t class_len = 0;
        if (slot->vtable->on_get_descriptor(slot->ctx, setup->bRequest, setup->wValue,
                                            setup->wLength, &class_desc, &class_len) != 0 ||
            class_desc == NULL || class_len == 0)
        {
            return -1;
        }
        data = class_desc;
        size = class_len;
        break;
    }
    }

    if (data == NULL || size == 0)
    {
        return -1;
    }

    USB_CoreDevWriteEP0Data(inst, data, size, setup->wLength, early_read_zlp);
    return 0;
}

static int8_t USB_CorePrepareAddressChange(USBInstance *inst, uint16_t address)
{
    inst->ep0.pending_addr = (uint8_t)(address & 0x7F);

    /* STATUS IN 发出前预置硬件地址（F4/H7 OTG：在 status 阶段生效） */
    (void)USB_DevSetAddress(inst, inst->ep0.pending_addr, USB_CTX_SETUP_BEFORE_STATUS);
    USB_CoreWriteZLP(inst, USB_CTX_STATUS_IN_COMPLETE);
    return USB_DevSetAddress(inst, inst->ep0.pending_addr, USB_CTX_STATUS_IN_ARMED);
}

static int8_t USB_CoreSendConfiguration(USBInstance *inst)
{
    uint8_t cfg = inst->config_value;
    USB_CoreDevWriteEP0Data(inst, &cfg, 1, 1, 0);
    return 0;
}

static int8_t USB_CoreSwitchConfiguration(USBInstance *inst, uint16_t value)
{
    if (value > USB_CONFIG_MAX)
    {
        return -1; /* 超出可用配置数 */
    }

    if (inst->config_value != value)
    {
        /* 解绑旧配置 → 更新配置值 → 绑定新配置（对照 XRUSB SwitchConfig：先 Unbind 再 Bind） */
        USB_CoreUnbindClasses(inst);
        inst->config_value = (uint8_t)value;
        inst->configured = (value != 0);
        if (value != 0)
        {
            USB_CoreBindClasses(inst);
        }
    }

    USB_CoreWriteZLP(inst, USB_CTX_ZLP);
    return 0;
}

static int8_t USB_CoreProcessStandardRequest(USBInstance *inst, const USB_SetupPacket_t *setup,
                                             uint8_t recipient)
{
    switch (setup->bRequest)
    {
    case USB_STD_GET_STATUS:
        return USB_CoreRespondWithStatus(inst, setup, recipient);

    case USB_STD_CLEAR_FEATURE:
        return USB_CoreClearFeature(inst, setup, recipient);

    case USB_STD_SET_FEATURE:
        return USB_CoreApplyFeature(inst, setup, recipient);

    case USB_STD_SET_ADDRESS:
        return USB_CorePrepareAddressChange(inst, setup->wValue);

    case USB_STD_GET_DESCRIPTOR:
        return USB_CoreSendDescriptor(inst, setup, recipient);

    case USB_STD_SET_DESCRIPTOR:
        return -1; /* 极少使用 */

    case USB_STD_GET_CONFIGURATION:
        return USB_CoreSendConfiguration(inst);

    case USB_STD_SET_CONFIGURATION:
        return USB_CoreSwitchConfiguration(inst, setup->wValue);

    case USB_STD_GET_INTERFACE:
    {
        if (recipient != USB_RECIPIENT_INTERFACE)
        {
            return -1;
        }
        uint8_t alt = 0;
        USBClassSlot_t *slot = USB_CoreFindClassByInterface(inst, setup->wIndex);
        if (slot == NULL)
        {
            return -1; /* 接口号不存在 */
        }
        if (slot->vtable->get_alt_setting != NULL)
        {
            (void)slot->vtable->get_alt_setting(slot->ctx, setup->wIndex, &alt);
        }
        USB_CoreDevWriteEP0Data(inst, &alt, 1, 1, 0);
        return 0;
    }

    case USB_STD_SET_INTERFACE:
    {
        if (recipient != USB_RECIPIENT_INTERFACE)
        {
            return -1;
        }
        USBClassSlot_t *slot = USB_CoreFindClassByInterface(inst, setup->wIndex);
        if (slot == NULL)
        {
            return -1;
        }
        int8_t alt_ret;
        if (slot->vtable->set_alt_setting != NULL)
        {
            alt_ret = slot->vtable->set_alt_setting(slot->ctx, setup->wIndex, setup->wValue);
        }
        else
        {
            /* 默认：只接受 alt=0（对照 XRUSB 基类默认 SetAltSetting） */
            alt_ret = (setup->wValue == 0) ? 0 : -1;
        }
        if (alt_ret != 0)
        {
            return -1;
        }
        USB_CoreWriteZLP(inst, USB_CTX_ZLP);
        return 0;
    }

    default:
        return -1;
    }
}

/* 当前激活配置槽：未配置/值0 时按槽0（对照 XRUSB current_cfg_） */
static USBConfig_t *USB_CoreActiveConfig(USBInstance *inst)
{
    uint8_t slot = (inst->config_value == 0) ? 0 : (uint8_t)(inst->config_value - 1);
    if (slot >= USB_CONFIG_MAX)
    {
        slot = 0;
    }
    return &inst->configs[slot];
}

/* 按接口号反查所属类（对照 XRUSB FindClassByInterfaceNumber） */
static USBClassSlot_t *USB_CoreFindClassByInterface(USBInstance *inst, uint16_t itf)
{
    USBConfig_t *cfg = USB_CoreActiveConfig(inst);
    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot = &cfg->classes[i];
        if (slot->vtable == NULL)
        {
            continue;
        }
        if (itf >= slot->itf_start && itf < (uint16_t)(slot->itf_start + slot->itf_count))
        {
            return slot;
        }
    }
    return NULL;
}

/* 按端点地址反查所属类（对照 XRUSB FindClassByEndpointAddress） */
static USBClassSlot_t *USB_CoreFindClassByEndpoint(USBInstance *inst, uint8_t ep_addr)
{
    USBConfig_t *cfg = USB_CoreActiveConfig(inst);
    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot = &cfg->classes[i];
        if (slot->vtable == NULL || slot->vtable->owns_ep == NULL)
        {
            continue;
        }
        if (slot->vtable->owns_ep(slot->ctx, ep_addr))
        {
            return slot;
        }
    }
    return NULL;
}

/* 单类、无 IAD、单接口 时允许覆盖设备描述符类字段（对照 XRUSB
 * is_device_descriptor_override_eligible + TryOverrideDeviceDescriptor） */
static USBClassSlot_t *USB_CoreFindDeviceDescOverrideClass(USBInstance *inst)
{
    /* 判定基于当前激活配置（对照 XRUSB TryOverrideDeviceDescriptor 用 items_[current_cfg_]） */
    USBConfig_t *cfg = USB_CoreActiveConfig(inst);
    USBClassSlot_t *only = NULL;
    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot = &cfg->classes[i];
        if (slot->vtable == NULL)
        {
            continue;
        }
        if (only != NULL)
        {
            return NULL; /* 多类：不覆盖 */
        }
        only = slot;
    }
    if (only == NULL || only->vtable->has_iad == NULL || only->vtable->get_itf_count == NULL ||
        only->vtable->has_iad(only->ctx) || only->vtable->get_itf_count(only->ctx) != 1)
    {
        return NULL;
    }
    return only;
}

/* 把类/vendor 请求结果武装成 EP0 数据阶段（OUT 数据 / IN 数据 / ZLP） */
static int8_t USB_CoreArmDataStage(USBInstance *inst, USBClassSlot_t *slot, uint8_t bRequest,
                                   const USB_SetupPacket_t *setup,
                                   const USB_ClassReqResult_t *result)
{
    if (result->read_len > 0 && result->write_len > 0)
    {
        return -1; /* 读写缓冲不能同时 */
    }

    /* Host->Device：OUT 数据阶段 */
    if (result->read_len > 0)
    {
        if (setup->wLength == 0 || result->read_len < setup->wLength)
        {
            return -1;
        }
        inst->ep0.cls_slot = slot;
        inst->ep0.cls_read = 1;
        inst->ep0.cls_write = 0;
        inst->ep0.cls_in_data_status_pending = 0;
        inst->ep0.cls_bRequest = bRequest;
        inst->ep0.cls_read_data = result->read_data;
        inst->ep0.cls_read_len = result->read_len;
        USB_CoreDevReadEP0Data(inst, result->read_data, result->read_len);
        return 0;
    }

    /* Device->Host：IN 数据阶段 */
    if (result->write_len > 0)
    {
        if (setup->wLength == 0)
        {
            return -1;
        }
        inst->ep0.cls_slot = slot;
        inst->ep0.cls_write = 1;
        inst->ep0.cls_read = 0;
        inst->ep0.cls_in_data_status_pending = 1;
        inst->ep0.cls_bRequest = bRequest;
        inst->ep0.cls_write_data = result->write_data;
        inst->ep0.cls_write_len = result->write_len;
        USB_CoreDevWriteEP0Data(inst, result->write_data, result->write_len, setup->wLength, 0);
        return 0;
    }

    if (result->read_zlp)
    {
        USB_CoreReadZLP(inst);
        return 0;
    }
    if (result->write_zlp)
    {
        USB_CoreWriteZLP(inst, USB_CTX_ZLP);
        return 0;
    }

    return 0; /* 无数据阶段，默认 ACK */
}

static int8_t USB_CoreProcessVendorRequest(USBInstance *inst, const USB_SetupPacket_t *setup,
                                           uint8_t recipient)
{
    if ((setup->bmRequestType & USB_REQ_TYPE_MASK) != USB_TYPE_VENDOR)
    {
        return -1;
    }

    USBClassSlot_t *slot = NULL;
    if (recipient == USB_RECIPIENT_INTERFACE)
    {
        slot = USB_CoreFindClassByInterface(inst, setup->wIndex);
    }
    else if (recipient == USB_RECIPIENT_ENDPOINT)
    {
        slot = USB_CoreFindClassByEndpoint(inst, setup->wIndex & 0xFF);
    }
    else if (recipient == USB_RECIPIENT_DEVICE)
    {
        /* 设备级 vendor 请求：遍历当前激活配置的所有类，第一个接管者处理 */
        USBConfig_t *cfg = USB_CoreActiveConfig(inst);
        for (uint8_t i = 0; i < cfg->class_count; i++)
        {
            USBClassSlot_t *s = &cfg->classes[i];
            if (s->vtable == NULL || s->vtable->on_vendor_request == NULL)
            {
                continue;
            }
            USB_ClassReqResult_t result;
            memset(&result, 0, sizeof(result));
            if (s->vtable->on_vendor_request(s->ctx, setup->bRequest, setup->wValue,
                                             setup->wIndex, setup->wLength, &result) == 0)
            {
                return USB_CoreArmDataStage(inst, s, setup->bRequest, setup, &result);
            }
        }
        return -1;
    }

    if (slot == NULL || slot->vtable->on_vendor_request == NULL)
    {
        return -1;
    }

    USB_ClassReqResult_t result;
    memset(&result, 0, sizeof(result));
    int8_t ans = slot->vtable->on_vendor_request(slot->ctx, setup->bRequest, setup->wValue,
                                                 setup->wIndex, setup->wLength, &result);
    if (ans != 0)
    {
        return ans;
    }
    return USB_CoreArmDataStage(inst, slot, setup->bRequest, setup, &result);
}

static int8_t USB_CoreProcessClassRequest(USBInstance *inst, const USB_SetupPacket_t *setup,
                                          uint8_t recipient)
{
    if ((setup->bmRequestType & USB_REQ_TYPE_MASK) != USB_TYPE_CLASS)
    {
        return -1;
    }

    USBClassSlot_t *slot;
    if (recipient == USB_RECIPIENT_INTERFACE)
    {
        slot = USB_CoreFindClassByInterface(inst, setup->wIndex);
    }
    else if (recipient == USB_RECIPIENT_ENDPOINT)
    {
        slot = USB_CoreFindClassByEndpoint(inst, setup->wIndex & 0xFF);
    }
    else
    {
        return -1;
    }

    if (slot == NULL || slot->vtable->on_class_request == NULL)
    {
        return -1;
    }

    USB_ClassReqResult_t result;
    memset(&result, 0, sizeof(result));

    int8_t ans = slot->vtable->on_class_request(slot->ctx, setup->bRequest, setup->wValue,
                                                setup->wIndex, setup->wLength, &result);
    if (ans != 0)
    {
        return ans;
    }

    return USB_CoreArmDataStage(inst, slot, setup->bRequest, setup, &result);
}

/*------------- 对外入口 --------------*/

void USB_CoreOnSetupPacket(USBInstance *inst, const USB_SetupPacket_t *setup)
{
    if (inst == NULL || !inst->inited)
    {
        return;
    }

    USB_CoreResetClassRequestState(inst);

    uint8_t type = setup->bmRequestType & USB_REQ_TYPE_MASK;
    uint8_t recipient = setup->bmRequestType & USB_REQ_RECIPIENT_MASK;

    if (USB_EPIsStalled(&inst->ep[0][USB_EP_DIR_IN]))
    {
        USB_EPUnstall(inst->handle, &inst->ep[0][USB_EP_DIR_IN]);
    }
    if (USB_EPIsStalled(&inst->ep[0][USB_EP_DIR_OUT]))
    {
        USB_EPUnstall(inst->handle, &inst->ep[0][USB_EP_DIR_OUT]);
    }

    int8_t ans = 0;
    switch (type)
    {
    case USB_TYPE_STANDARD:
        ans = USB_CoreProcessStandardRequest(inst, setup, recipient);
        break;
    case USB_TYPE_CLASS:
        ans = USB_CoreProcessClassRequest(inst, setup, recipient);
        break;
    case USB_TYPE_VENDOR:
        ans = USB_CoreProcessVendorRequest(inst, setup, recipient);
        break;
    default:
        ans = -1;
        break;
    }

    if (ans != 0)
    {
        USB_CoreStallControlEndpoint(inst);
    }
}

void USB_CoreControlInit(USBInstance *inst)
{
    USBEndpoint *ep_in = &inst->ep[0][USB_EP_DIR_IN];
    USBEndpoint *ep_out = &inst->ep[0][USB_EP_DIR_OUT];

    ep_in->ctx = inst;
    ep_out->ctx = inst;
    ep_in->on_complete = USB_CoreOnEP0InComplete;
    ep_out->on_complete = USB_CoreOnEP0OutComplete;

    USB_EPConfigure(inst->handle, ep_in, USB_EP_TYPE_CONTROL, USB_EP0_MAX_PACKET);
    USB_EPConfigure(inst->handle, ep_out, USB_EP_TYPE_CONTROL, USB_EP0_MAX_PACKET);

    USB_CoreResetControlTransferState(inst);
    USB_CoreResetClassRequestState(inst);

    inst->config_value = 0;
    inst->configured = 0;
    inst->inited = 1;

    /* 绑定类：分配接口号/端点 + open + 填充描述符块（确保枚举早期
     * GET_DESCRIPTOR(CONFIGURATION) 拿到正确描述符，对照 XRUSB Init 时 BindEndpoints） */
    USB_CoreBindClasses(inst);
}

/*------------- 类绑定 / 解绑 --------------*/

void USB_CoreBindClasses(USBInstance *inst)
{
    if (inst == NULL || !inst->inited || inst->class_binded)
    {
        return;
    }

    USBConfig_t *cfg = USB_CoreActiveConfig(inst);
    uint8_t itf = 0;
    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot = &cfg->classes[i];
        if (slot->vtable == NULL)
        {
            continue;
        }
        slot->itf_count = slot->vtable->get_itf_count(slot->ctx);
        slot->itf_start = itf;
        if (slot->vtable->bind != NULL)
        {
            (void)slot->vtable->bind(slot->ctx, inst, slot);
        }
        itf += slot->itf_count;
    }

    /* 类全部 bind 后分配接口字符串索引（需描述符块已填充，bind 完成后才有效） */
    USB_CoreRegisterInterfaceStrings(inst);

    inst->class_binded = 1;
}

/*------------- 接口字符串注册 --------------*/

void USB_CoreRegisterInterfaceStrings(USBInstance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    USB_DescClearInterfaceStrings();

    /* ctx 去重：同一类上下文出现在多配置时只分配一次字符串索引
     * （对照 XRUSB classes_ 列表的去重语义） */
    const void *handled[USB_CONFIG_MAX * USB_CLASS_MAX];
    uint8_t handled_count = 0;

    for (uint8_t cfg_i = 0; cfg_i < USB_CONFIG_MAX; cfg_i++)
    {
        USBConfig_t *cfg = &inst->configs[cfg_i];
        for (uint8_t i = 0; i < cfg->class_count; i++)
        {
            USBClassSlot_t *slot = &cfg->classes[i];
            if (slot->vtable == NULL || slot->vtable->get_interface_string == NULL ||
                slot->vtable->get_itf_count == NULL)
            {
                continue;
            }

            /* 该 ctx 已在其它配置分配过（同一类实例）：跳过 */
            uint8_t seen = 0;
            for (uint8_t h = 0; h < handled_count; h++)
            {
                if (handled[h] == slot->ctx)
                {
                    seen = 1;
                    break;
                }
            }
            if (seen)
            {
                continue;
            }
            handled[handled_count++] = slot->ctx;

            slot->itf_str_base = 0;
            for (uint8_t itf = 0; itf < slot->itf_count; itf++)
            {
                const char *s = slot->vtable->get_interface_string(slot->ctx, itf);
                if (s == NULL || s[0] == '\0')
                {
                    continue;
                }

                uint8_t idx = USB_DescAddInterfaceString(s);
                if (idx == 0)
                {
                    continue; /* 表满：该接口不分配 */
                }
                if (slot->itf_str_base == 0)
                {
                    slot->itf_str_base = idx;
                }
                if (slot->vtable->set_interface_string_index != NULL)
                {
                    slot->vtable->set_interface_string_index(slot->ctx, itf, idx);
                }
            }
        }
    }
}

void USB_CoreUnbindClasses(USBInstance *inst)
{
    if (inst == NULL || !inst->class_binded)
    {
        return;
    }
    USBConfig_t *cfg = USB_CoreActiveConfig(inst);
    for (uint8_t i = 0; i < cfg->class_count; i++)
    {
        USBClassSlot_t *slot = &cfg->classes[i];
        if (slot->vtable == NULL || slot->vtable->unbind == NULL)
        {
            continue;
        }
        slot->vtable->unbind(slot->ctx, inst, slot);
    }
    inst->class_binded = 0;
}

void USB_CoreDeinit(USBInstance *inst)
{
    inst->inited = 0;
    USB_EPClose(inst->handle, &inst->ep[0][USB_EP_DIR_IN]);
    USB_EPClose(inst->handle, &inst->ep[0][USB_EP_DIR_OUT]);
    USB_CoreResetControlTransferState(inst);
    USB_CoreResetClassRequestState(inst);
}
