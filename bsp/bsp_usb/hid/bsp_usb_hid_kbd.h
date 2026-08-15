/**
 * @file bsp_usb_hid_kbd.h
 * @brief HID 键盘类（纯 C，参考 XRUSB hid/hid_keyboard）
 *
 * @todo 当前停用：Windows 对复合设备 HID 接口枚举异常（配置描述符正确——已确认接口 2
 *   拼接为 09 04 02 01 03 01 01——但 Windows 不实例化该 HID 接口，不发送 GET_DESCRIPTOR(0x22)，
 *   也不 poll interrupt IN 端点，疑似主机侧枚举缓存问题）。已从 bsp/CMakeLists.txt 移除编译；
 *   恢复时：加回 bsp_usb/hid/bsp_usb_hid_kbd.c、解开 bsp_usb.h 中 #include 与 hid_kbd 字段、
 *   解开 app_usb.c 中注册与演示代码的 #if 0 块。
 *
 * 单接口 HID Boot 键盘：
 *   - 1 个 INTERRUPT IN 端点（8B，轮询 10ms），bind 时经 USB_EPPoolGet 领取
 *   - HID report descriptor（63B）走 vtable on_get_descriptor（GET_DESCRIPTOR 0x22）
 *   - write_device_descriptor 把设备描述符类字段归零（单类无 IAD 时 core 自动调用，
 *     使"仅 HID 配置"的设备呈现为真正的 HID 设备而非复合设备）
 */

#ifndef __BSP_USB_HID_KBD_H
#define __BSP_USB_HID_KBD_H

#include "stdint.h"
#include "bsp_usb_types.h"
#include "bsp_usb_core.h"

typedef struct USBInstance USBInstance;

/*------------- HID 键盘类实例 --------------*/
typedef struct USBHIDKbd
{
    USBInstance *inst; /* 反向指针（bind 时填充，供回调访问） */
    uint8_t bound;     /* bind 幂等标志 */

    /* 协议状态（类请求 GET/SET_PROTOCOL、GET/SET_IDLE 读写） */
    uint8_t protocol;  /* 0=boot / 1=report（HID 1.11 §7.2.6；复位后默认 report） */
    uint8_t idle_rate; /* idle 速率（0=仅状态变化时发送；GET_IDLE 返回） */

    /* 端点（bind 时经 USB_EPPoolGet 领取） */
    USBEndpoint *ep_in;

    /* 描述符块：接口 9 + HID 9 + 端点 7 = 25B（bind 时填充接口号/端点地址） */
    uint8_t desc_block[25];
} USBHIDKbd;

/*------------- HID 类请求码（USB HID 1.11 §7.2） --------------*/
#define USB_HID_GET_REPORT 0x01
#define USB_HID_GET_IDLE 0x02
#define USB_HID_GET_PROTOCOL 0x03
#define USB_HID_SET_REPORT 0x09
#define USB_HID_SET_IDLE 0x0A
#define USB_HID_SET_PROTOCOL 0x0B

/*------------- API --------------*/

/** @brief 获取 HID 键盘类 vtable（USBAddClass 挂载用） */
const USBClassVTable_t *USB_HIDKbdVTable(void);

/**
 * @brief 非阻塞发送 8B 键盘 report
 * @retval 0 提交成功；-1 失败（未绑定 / 端点忙）
 * @note report 缓冲在调用返回后无需保持（内部拷贝到端点缓冲）
 */
int32_t USBHIDKbdSend(USBInstance *inst, const uint8_t report[8]);

#endif /* __BSP_USB_HID_KBD_H */
