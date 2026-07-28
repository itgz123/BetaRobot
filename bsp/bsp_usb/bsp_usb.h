/**
 * @file bsp_usb.h
 * @brief USB VCP BSP 驱动 — 实例管理模式，纯收发封装
 *
 * @note 1. 硬件配置由 CubeMX 负责（freertos.c 默认任务初始化 USB），BSP 层只管理实例
 *       2. TX 带环形缓冲，大小跟随 CubeMX 配置（APP_TX_DATA_SIZE）
 *       3. RX 通过回调通知用户（回调接收实例指针，与 USART/CAN 风格一致）
 *       4. 单例硬件（只有一个 USB 外设），但 API 风格保持与项目一致
 */

#ifndef __BSP_USB_H
#define __BSP_USB_H

#include <stdint.h>
#include "usbd_cdc_if.h"

/*============================================
 *     单包发送缓冲区大小
 *============================================
 * 统一使用 FS 最大包长 64 字节：
 *   - DJI_C/A (F407/F427) — USB_OTG_FS，原生 FS
 *   - DM_MC02 (H723)      — USB_OTG_HS 内嵌 FS PHY，实际也是 FS
 * 若后续某板使用外部 ULPI PHY（真 HS），再重新区分。
 */
#define BSP_USB_TX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE /* 64 */

/*============================================
 *              USB 实例结构体
 *============================================*/
// 向前声明
typedef struct BSP_USB_Instance BSP_USB_Instance;

/** 接收完成回调 */
typedef void (*BSP_USB_RxCallback)(struct BSP_USB_Instance *inst, uint8_t *data, uint16_t len);
/** 发送完成回调 */
typedef void (*BSP_USB_TxCompleteCallback)(struct BSP_USB_Instance *inst);

struct BSP_USB_Instance
{
    void *parent; /**< 父实例指针（由 DRV 层设置）*/

    /* TX 环形缓冲区（直接嵌入实例） */
    uint8_t tx_ring[APP_TX_DATA_SIZE]; /**< TX 环形缓冲区 */
    volatile uint16_t tx_head;         /**< 生产者写入位置 */
    volatile uint16_t tx_tail;         /**< 消费者读出位置 */

    /* 单包发送缓冲区（ring → 平坦 buffer → USB 栈） */
    uint8_t tx_buf[BSP_USB_TX_BUF_SIZE];

    // 不需要接收缓冲区
    // uart是dma从外设写入系统内存的缓冲区。usb是外设写入usb的缓冲区。
    // 所以uart要分配系统内存给USARTInstance，但是usb不需要

    /* 回调函数 */
    BSP_USB_RxCallback rx_cb;         /**< 接收完成回调 */
    BSP_USB_TxCompleteCallback tx_cb; /**< 发送完成回调 */
};

/*============================================
 *              实例定义宏
 *============================================*/

/**
 * @brief 静态定义 USB 实例（所有缓冲区嵌入结构体）
 * @param name 实例名称
 *
 * @note TX 环形缓冲区大小自动跟随 CubeMX 配置的 APP_TX_DATA_SIZE
 *
 * @example
 *   BSP_USB_INSTANCE_DEF(usb_vcp);
 *
 *   void init(void) {
 *       BSP_USB_Register(&usb_vcp);
 *       BSP_USB_Config(&usb_vcp, &(BSP_USB_Config_s){ .rx_cb = my_rx });
 *   }
 */
#define BSP_USB_INSTANCE_DEF(name) \
    static BSP_USB_Instance name = {0}

/*============================================
 *              配置结构体
 *============================================*/

/**
 * @brief USB 运行时配置结构体（用于 BSP_USB_Config）
 */
typedef struct
{
    BSP_USB_RxCallback rx_cb;         /**< 接收完成回调（可为 NULL）*/
    BSP_USB_TxCompleteCallback tx_cb; /**< 发送完成回调（可为 NULL）*/
} BSP_USB_Config_s;

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 注册 USB 实例（仅调用一次）
 * @param inst USB 实例指针（需先通过 BSP_USB_INSTANCE_DEF 定义）
 * @retval 0 成功
 * @retval -1 失败（参数空、重复注册）
 *
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不初始化硬件（由 MX_USB_DEVICE_Init 负责）。
 */
int8_t BSP_USB_Register(BSP_USB_Instance *inst);

/**
 * @brief 配置 USB 实例（可重复调用）
 * @param inst   USB 实例指针
 * @param config 配置结构体指针（回调函数等）
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 *
 * @note 只设置回调，不初始化 USB 硬件。
 *       USB 硬件初始化由 CubeMX 生成的 freertos.c 默认任务负责：
 *         StartDefaultTask → MX_USB_DEVICE_Init()
 *       不修改 static 管理数组。
 *       要求在 BSP_USB_Register 之后调用。
 */
int8_t BSP_USB_Config(BSP_USB_Instance *inst, const BSP_USB_Config_s *config);

/**
 * @brief 发送数据（异步）
 * @param inst USB 实例指针
 * @param data 数据指针
 * @param len  数据长度
 *
 * @note 数据写入环形缓冲区后立即尝试发送。
 *       缓冲区满时丢弃超出的数据并 LOGWARNING。
 */
void BSP_USB_Transmit(BSP_USB_Instance *inst, const uint8_t *data, uint16_t len);

#endif /* __BSP_USB_H */
