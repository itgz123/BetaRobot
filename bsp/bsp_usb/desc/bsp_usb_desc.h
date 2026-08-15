/**
 * @file bsp_usb_desc.h
 * @brief USB 描述符 getter 统一入口（设备 / 配置 / 字符串）
 *
 * 每个描述符一个实现文件：
 *   - bsp_usb_desc_dev.c  设备描述符 18B（vid/pid/bcd 动态填充）
 *   - bsp_usb_desc_cfg.c  配置描述符 9B 头 + CDC 块 = 75B
 *   - bsp_usb_desc_cdc.c  CDC 类特定描述符块 66B（供 cfg 组装）
 *   - bsp_usb_desc_str.c  字符串描述符（UTF-16LE 按需生成）
 */

#ifndef __BSP_USB_DESC_H
#define __BSP_USB_DESC_H

#include "stdint.h"
#include "bsp_usb_types.h"

/*------------- 字符串索引约定 --------------*/
typedef enum : uint8_t
{
    USB_STR_LANGID = 0,
    USB_STR_MANUFACTURER = 1,
    USB_STR_PRODUCT = 2,
    USB_STR_SERIAL = 3,
} USBStrIdx_e;

/* 接口字符串索引起点 = SERIAL + 1 = 4（索引 1..3 保留给厂商/产品/序列号，
 * 与 XRUSB DescriptorStrings::Index 一致） */
#define USB_STR_INTERFACE_BASE ((uint8_t)(USB_STR_SERIAL + 1))

/* 接口字符串表容量（索引 4 起顺序分配；可在 app_cfg.h 或编译选项覆盖） */
#ifndef USB_IF_STR_MAX
#define USB_IF_STR_MAX 8
#endif

/*------------- getter --------------*/

/**
 * @brief 注册一个接口字符串到扁平表（core 在类 bind 后按 get_interface_string 遍历调用）
 * @param str 源字符串（UTF-8 编码，运行时转 UTF-16LE；4 字节码点跳过）
 * @return 分配的字符串索引（>= USB_STR_INTERFACE_BASE）；表满/参数非法返回 0
 */
uint8_t USB_DescAddInterfaceString(const char *str);

/**
 * @brief 清空接口字符串表（类重新 bind 时调用，索引重新从 base 分配）
 */
void USB_DescClearInterfaceStrings(void);

/**
 * @brief 获取 CDC-ACM 描述符块（66B，供配置描述符组装）
 */
const USB_CDCDescBlock_t *USB_DescCdcBlockGet(void);

/**
 * @brief 获取设备描述符（每次重建到内部静态缓冲）
 * @param vid / pid / bcd  运行时的厂商/产品/版本号（USBConfig 传入）
 * @param len  输出：长度（固定 18）
 * @return 描述符指针
 */
const uint8_t *USB_DescGetDevice(uint16_t vid, uint16_t pid, uint16_t bcd, uint16_t *len);

/**
 * @brief 获取配置描述符（9B 头 + 各已绑定类描述符块，运行时拼接）
 * @param inst 实例（遍历 class 槽取描述符块）
 * @param len 输出：长度
 * @return 描述符指针
 */
const uint8_t *USB_DescGetConfig(USBInstance *inst, uint16_t *len);

/**
 * @brief 获取字符串描述符（UTF-16LE，按需生成到内部静态缓冲）
 * @param idx 字符串索引（USB_STR_*）
 * @param len 输出：长度
 * @return 描述符指针；不支持返回 NULL
 */
const uint8_t *USB_DescGetString(USBStrIdx_e idx, uint16_t *len);

#endif /* __BSP_USB_DESC_H */
