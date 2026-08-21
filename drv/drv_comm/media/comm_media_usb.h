/**
 * @file comm_media_usb.h
 * @brief 通信框架-硬件层（Media）USB(CDC 虚拟串口) 后端
 *
 * 把 bsp_usb 包装成统一"任意长度数据单元"通道，并在 media 层做分包收发：
 *   - 发送：整帧（协议帧）按 63B/片切分，每片前加 1B 分包序号（该片在整个帧的第几包，
 *           0 起递增），每包 = [pkt_idx][数据片 ≤ 63B]（≤64B，CDC FS 单包上限）经 USBTransmit 发出；
 *           整帧 ≤ 63B 时单包（pkt_idx=0）
 *   - 接收：bsp 收包（0~64B）→ 适配钩子 → 按分包序号连续重组整帧
 *           → 错位/丢包则丢帧重同步 → CommMediaRxHook（comm 层接收入口）
 * 整帧 = 协议帧（rx/tx size + 协议开销），分包序号不进入协议内容。
 *
 * @note COMM_DEF 通过 token 拼接 COMM_##media_type_##_DEF 分发到本宏。
 * @note USB CDC 无 DMA（usbd_conf.c dma_enable=DISABLE），缓冲放普通 RAM 即可；
 *       若日后启用 USB DMA，缓冲须移入 RAM_D1/D2。
 */

#ifndef COMM_MEDIA_USB_H
#define COMM_MEDIA_USB_H

#include "comm_media.h"

#ifdef DRV_COMM_USED

#include "bsp_usb.h"

/* USB 介质派生结构体（首成员必须为 CommMedia 基类，vtable 约定） */
typedef struct
{
    CommMedia base;        /* 基类（首成员；发送不持 staging 缓冲，MediaUsbSend 直接引用 comm 打包缓冲 data） */
    uint8_t *rx_buff;      /* 接收累积缓冲（完整协议帧，不含分包序号；DEF 宏静态绑定，大小 = rx_buff_sz） */
    uint16_t rx_frame_len; /* 完整协议帧长（不含分包序号）= rx_buff_sz（DEF 宏写入；接收累积目标） */
    uint16_t tx_frame_len; /* 完整协议帧长（不含分包序号）= tx_buff_sz（DEF 宏写入；发送分包依据） */
    uint16_t rx_cnt;       /* 已累积字节数（0..rx_frame_len，上交后归零） */
    uint8_t rx_expect_pkt; /* 期望接收的下一分包序号（帧内 0 起递增；错位说明丢包，丢帧重同步） */
    uint32_t lost_frames;  /* 丢帧计数（分包错位/帧中途丢包累加） */
} CommMediaUsb;

/**
 * @brief 静态定义 USB 介质实例
 * @param name        实例名称
 * @param rx_buff_sz  协议帧长（= rx_size + 协议开销，COMM_DEF 传入；接收累积缓冲 = rx_buff_sz）
 * @param tx_buff_sz  协议帧长（= tx_size + 协议开销；发送分包依据，写入 tx_frame_len）
 *
 * @note 展开定义 name##_usb（USBInstance）、name##_rx_buff（完整协议帧接收缓冲，
 *       不含分包序号）与 name（CommMediaUsb），并绑定 base.media。发送不持
 *       staging 缓冲：MediaUsbSend 直接引用 comm 打包缓冲（data 在本函数运行期间
 *       有效）。缓冲放普通 RAM（USB 无 DMA）。
 *
 * @example
 *   COMM_MEDIA_USB_DEF(usb_comm_media, 16, 16); 协议帧 16B，帧长 > 63B 时自动分包
 */
#define COMM_MEDIA_USB_DEF(name, rx_buff_sz, tx_buff_sz) \
    USB_INSTANCE_DEF(name##_usb);                        \
    static uint8_t name##_rx_buff[(rx_buff_sz)] = {0};   \
    static CommMediaUsb name = {                         \
        .base.media = &name##_usb,                       \
        .rx_buff = name##_rx_buff,                       \
        .rx_frame_len = (rx_buff_sz),                    \
        .tx_frame_len = (tx_buff_sz)}

/**
 * @brief 注册 USB 介质后端（不可重入：仅可调用一次）
 * @param media CommMediaUsb 实例指针（COMM_MEDIA_USB_DEF 定义）
 * @retval 0 成功；-1 参数非法 / bsp 注册失败
 *
 * @note 完成 bsp USBRegister（防重复注册，USB_INSTANCE_NUM=1）+ 挂 vtable +
 *       建立 usb↔media 反向指针 + 清接收累积与序列状态。
 *       接收回调由 MediaUsbConfig 挂接。
 */
int8_t MediaUsbRegister(CommMediaUsb *media);

/**
 * @brief 配置 USB 介质后端（可重入：可反复调用改回调）
 * @param media CommMediaUsb 实例指针（须先 MediaUsbRegister）
 * @param cfg   USB 运行时配置（USB_Config_s*，可传 NULL；USB 无硬件运行期参数）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置失败
 *
 * @note 内部调 bsp USBConfig 并强制接管 rx_callback=MediaUsbRxHook、
 *       parent=media（反向指针），保证接收统一进 comm 层接收入口
 *       （CommMediaRxHook）；USBConfig 将二者写入实例。
 */
int8_t MediaUsbConfig(CommMediaUsb *media, USB_Config_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_USB_H */
