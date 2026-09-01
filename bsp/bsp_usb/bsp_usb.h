/**
 * @file bsp_usb.h
 * @brief USB VCP 驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（CDC 枚举/缓冲）由 CubeMX 负责，BSP 层只管理实例。
 * @warning USB 硬件初始化（MX_USB_DEVICE_Init）须在 main() 里 osKernelStart()
 *          之前调用（USER CODE BEGIN 2 段），不要在 freertos.c 的
 *          StartDefaultTask（osPriorityIdle）里调用。详见 bsp_usb.c 顶部注释。
 */

#ifndef __BSP_USB_H
#define __BSP_USB_H

#include "bsp_map.h"

#if defined(HAL_PCD_MODULE_ENABLED)

#include <stdint.h>
#include "usbd_cdc_if.h"

/*------------- 常量定义 --------------*/

/**
 * @brief 单包发送缓冲区大小
 * @note 统一使用 FS 最大包长 64 字节：
 *   - DJI_C/A (F407/F427) — USB_OTG_FS，原生 FS
 *   - DM_MC02 (H723)      — USB_OTG_HS 内嵌 FS PHY，实际也是 FS
 */
#define USB_TX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE /* 64 */

/*------------- 类型定义 --------------*/

/**
 * @brief USB实例结构体
 * @note TX 环形缓冲为实例内嵌数组（tx_ring），随实例整体放入 DMA_RAM 区域（H7 上为 .ram_d1）；
 *       接收数据由 bsp_usb_rx_handler 填入 rx_buff/rx_len，回调内直接从实例读取。
 */
typedef struct USBInstance
{
    void *parent;                              /* 父实例指针（由 DRV 层设置）*/
    uint8_t tx_ring[APP_TX_DATA_SIZE];         /* TX 环形缓冲区*/
    volatile uint16_t tx_head;                 /* 生产者写入位置 */
    volatile uint16_t tx_tail;                 /* 消费者读出位置 */
    uint8_t tx_buf[USB_TX_BUF_SIZE];           /* 单包发送缓冲*/
    uint8_t *rx_buff;                          /* 接收缓冲指针（指向 HAL 的 UserRxBufferFS/HS）*/
    uint16_t rx_len;                           /* 本次接收数据长度 */
    void (*rx_callback)(struct USBInstance *); /* 接收完成回调 */
    void (*tx_callback)(struct USBInstance *); /* 发送完成回调 */
} USBInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief USB 运行时配置结构体（用于 USBConfig）
 */
typedef struct
{
    void (*rx_callback)(struct USBInstance *); /* 接收完成回调（可为 NULL）*/
    void (*tx_callback)(struct USBInstance *); /* 发送完成回调（可为 NULL）*/
    void *parent;                              /* 父实例指针（经 USBConfig 写入实例；可为 NULL）*/
} USB_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 USB 实例（整个结构体放入 DMA_RAM 区域）
 * @param name 实例名称
 * @example
 *   USB_INSTANCE_DEF(usb_vcp);
 */
#define USB_INSTANCE_DEF(name) static USBInstance name = {0}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 USB 实例（仅调用一次）
 * @param instance USB 实例指针（需先通过 USB_INSTANCE_DEF 定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限、参数非法、重复注册）
 *
 * @note 仅注册实例（TX 环形缓冲为实例内嵌数组），不初始化硬件。
 *       硬件初始化由 MX_USB_DEVICE_Init 负责，且必须先行完成——
 *       它在 main() 中 osKernelStart() 之前调用（USER CODE BEGIN 2 段），
 *       详见 bsp_usb.c 顶部 @warning 注释。
 */
int8_t USBRegister(USBInstance *instance);

/**
 * @brief 配置 USB 实例（可重复调用）
 * @param instance USB 实例指针
 * @param config   配置结构体指针（回调等）
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 *
 * @note 只设置回调，不初始化 USB 硬件。
 *       要求先调用 USBRegister。
 */
int8_t USBConfig(USBInstance *instance, const USB_Config_s *config);

/**
 * @brief 发送数据（异步）
 * @param instance USB 实例
 * @param data 数据指针
 * @param len  数据长度
 *
 * @note 数据写入环形缓冲后立即尝试发送；
 *       缓冲满时丢弃超出数据并输出 WARNING 日志。
 */
void USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len);

#endif /* HAL_PCD_MODULE_ENABLED */

#endif /* __BSP_USB_H */
