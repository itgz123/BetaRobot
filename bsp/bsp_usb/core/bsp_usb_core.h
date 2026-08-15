/**
 * @file bsp_usb_core.h
 * @brief USB 控制传输核心：EP0 状态机 + 标准请求 + 类请求分发（纯 C，移植 XRUSB dev_core）
 *
 * 依赖方向：bsp_usb.c → core → (ep / desc / cdc)。
 * 本头不包含 USBInstance 完整定义（core.c include bsp_usb.h 取用），只前向声明。
 */

#ifndef __BSP_USB_CORE_H
#define __BSP_USB_CORE_H

#include "stdint.h"
#include "bsp_usb_types.h"
#include "bsp_usb_ep.h"

/*------------- EP0 状态机上下文（Context） --------------*/
typedef enum : uint8_t
{
    USB_CTX_UNKNOWN = 0,
    USB_CTX_SETUP_BEFORE_STATUS = 1,
    USB_CTX_STATUS_IN_ARMED = 2,
    USB_CTX_DATA_OUT = 3,
    USB_CTX_STATUS_OUT = 4,
    USB_CTX_DATA_IN = 5,
    USB_CTX_STATUS_IN_COMPLETE = 6,
    USB_CTX_ZLP = 7,
} USBEP0Ctx_e;

struct USBClassSlot; /* 前向声明（vtable / EP0 上下文使用） */

/*------------- EP0 控制传输上下文 --------------*/
typedef struct
{
    /* state_.in0 / state_.out0（Context 枚举） */
    USBEP0Ctx_e in0_state;
    USBEP0Ctx_e out0_state;

    /* state_.write_remain（IN 剩余待发） */
    const uint8_t *write_remain;
    uint16_t write_remain_len;
    /* state_.read_remain（OUT 剩余待收） */
    uint8_t *read_remain;
    uint16_t read_remain_len;
    /* state_.out0_buffer（数据阶段接收目标指针） */
    uint8_t *out0_buffer;
    /* state_.pending_addr（SET_ADDRESS 延迟生效；0xFF=无） */
    uint8_t pending_addr;
    uint8_t need_write_zlp;
    uint8_t status_out_armed;

    /* class_req_ 上下文 */
    struct USBClassSlot *cls_slot; /* 当前处理类槽（分发时记录，回调时取 vtable） */
    uint8_t cls_read;
    uint8_t cls_write;
    uint8_t cls_in_data_status_pending;
    uint8_t cls_bRequest;
    uint8_t *cls_read_data;
    uint16_t cls_read_len;
    const uint8_t *cls_write_data;
    uint16_t cls_write_len;
} USBEP0_t;

/*------------- 类回调 vtable（对照 XRUSB DeviceClass / ConfigDescriptorItem） --------------
 * 类实现同时声明"请求处理"与"描述符/端点元数据"，由 core 组合分发。
 */
typedef struct USBInstance USBInstance;
typedef struct USBClassSlot USBClassSlot_t;

typedef struct
{
    /* 类特定请求（返回 0=已处理；非 0=STALL） */
    int8_t (*on_class_request)(void *ctx, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                               uint16_t wLength, USB_ClassReqResult_t *result);
    /* 厂商请求（返回 0=已处理；非 0=STALL） */
    int8_t (*on_vendor_request)(void *ctx, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                uint16_t wLength, USB_ClassReqResult_t *result);
    /* 类请求数据阶段完成（SET_LINE_CODING 收完 / GET_LINE_CODING 发完） */
    void (*on_class_data)(void *ctx, uint8_t bRequest);
    /* 类 IN 数据阶段后 STATUS OUT 完成（对照 XRUSB OnClassInDataStatusComplete） */
    void (*on_class_in_data_status_complete)(void *ctx, uint8_t bRequest);

    /* 配置激活：从端点池领取端点 + open + 填充描述符块（Init / SET_CONFIGURATION 时调用，需幂等） */
    int8_t (*bind)(void *ctx, USBInstance *inst, USBClassSlot_t *slot);
    /* 配置去激活：close + 释放端点 */
    void (*unbind)(void *ctx, USBInstance *inst, USBClassSlot_t *slot);

    /* 描述符元数据（bind 后 get_desc 才有效） */
    const uint8_t *(*get_desc)(void *ctx, uint16_t *len);
    uint8_t (*get_itf_count)(void *ctx);
    uint8_t (*has_iad)(void *ctx);
    uint8_t (*owns_ep)(void *ctx, uint8_t ep_addr);

    /* 接口字符串（可选；对照 XRUSB DeviceClass::GetInterfaceString）：
     * 返回本地接口 itf 的字符串，NULL/空串 = 无字符串。core 在
     * USB_CoreRegisterInterfaceStrings 遍历分配字符串索引（从 base 起）后，
     * 回调 set_interface_string_index 让类把索引写回描述符块 iInterface 字段。 */
    const char *(*get_interface_string)(void *ctx, uint8_t itf);
    void (*set_interface_string_index)(void *ctx, uint8_t itf, uint8_t idx);

    /* 类特定描述符（GET_DESCRIPTOR 非 DEVICE/CONFIG/STRING 类型，interface-recipient；
     * HID report descriptor 等。返回 0=已处理并填充 out；非 0=STALL） */
    int8_t (*on_get_descriptor)(void *ctx, uint8_t bRequest, uint16_t wValue,
                                uint16_t wLength, const uint8_t **out_data, uint16_t *out_len);
    /* 备选设置（对照 XRUSB SetAltSetting/GetAltSetting；返回 0=已处理，非 0=STALL/不支持） */
    int8_t (*set_alt_setting)(void *ctx, uint8_t itf, uint8_t alt);
    int8_t (*get_alt_setting)(void *ctx, uint8_t itf, uint8_t *alt);
    /* 可选：覆盖设备描述符字段（仅单类无 IAD 时 core 调用；返回 0=已覆盖，非 0=不支持） */
    int8_t (*write_device_descriptor)(void *ctx, USB_DescDevice_t *desc);
} USBClassVTable_t;

/*------------- 类槽：一个槽 = 一个已注册类的上下文 + 分配到的接口号 --------------*/
struct USBClassSlot
{
    const USBClassVTable_t *vtable;
    void *ctx;
    uint8_t itf_start;    /* 本类起始接口号（bind 时由 core 分配） */
    uint8_t itf_count;    /* 本类占用的接口数 */
    uint8_t itf_str_base; /* 本类接口字符串起始索引（0=无；RegisterInterfaceStrings 分配） */
};

/*------------- core API（由 bsp_usb.c 的 HAL 回调调用） --------------*/

void USB_CoreControlInit(USBInstance *inst); /* 不与 LL USB_CoreInit 撞名 */
void USB_CoreDeinit(USBInstance *inst);
void USB_CoreOnSetupPacket(USBInstance *inst, const USB_SetupPacket_t *setup);
void USB_CoreOnEP0InComplete(USBEndpoint *ep, uint32_t actual_len);
void USB_CoreOnEP0OutComplete(USBEndpoint *ep, uint32_t actual_len);

/* 类绑定/解绑（分配接口号 + 端点 + 填充描述符），Init/Reset 时调用 */
void USB_CoreBindClasses(USBInstance *inst);
void USB_CoreUnbindClasses(USBInstance *inst);

/* 接口字符串注册（BindClasses 末尾调用）：遍历全部配置的类，按 get_interface_string
 * 分配字符串索引（从 USB_STR_INTERFACE_BASE 起），经 set_interface_string_index
 * 写回描述符块 iInterface，源字符串入扁平表供运行时生成 */
void USB_CoreRegisterInterfaceStrings(USBInstance *inst);

/* 供 bsp_usb.c 复位用 */
void USB_CoreResetClassRequestState(USBInstance *inst);

#endif /* __BSP_USB_CORE_H */
