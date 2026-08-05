/**
 * @file comm_media_usb.h
 * @brief 硬件层 USB-CDC 后端：包装 bsp_usb（虚拟串口）
 *
 * @note USB 硬件初始化须在 osKernelStart() 之前（CubeMX USER CODE 段），
 *       见 bsp_usb.c 顶部 @warning。仅当板级启用 USB（HAL_PCD_MODULE_ENABLED）
 *       时本类型才存在。
 */

#ifndef DRV_COMM_MEDIA_USB_H
#define DRV_COMM_MEDIA_USB_H

#include "drv_comm.h"

#ifdef HAL_PCD_MODULE_ENABLED
#include "bsp_usb.h"

typedef struct
{
    CommMedia media; /* 首成员：内嵌基类 */
    USBInstance usb;
} CommMediaUsb;

typedef struct
{
    uint8_t media_id;      /* 引擎路由用 */
    uint8_t unpack_in_isr; /* 1=ISR 直通解包 */
} CommMediaUsb_Config_s;

int8_t MediaUsbConfig(CommMedia *inst, const CommMediaUsb_Config_s *cfg);

#endif /* HAL_PCD_MODULE_ENABLED */

#endif /* DRV_COMM_MEDIA_USB_H */
