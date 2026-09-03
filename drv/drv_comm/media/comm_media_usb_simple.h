/**
 * @file comm_media_usb_simple.h
 * @brief 通信框架-硬件层（Media）USB(CDC 虚拟串口) 后端·短帧免序号版
 *
 * 与 comm_media_usb 的唯一差别：整帧 ≤ 64B（CDC FS 单包上限）时**免分包序号**，
 * 直接把完整协议帧经 USBTransmit 一包发出；接收侧按固定帧长 rx_frame_len 判定，
 * ≤ 64B 时整包透传（不剥序号）后上交 comm 层。
 * 长帧（> 64B）行为与 comm_media_usb 完全一致：分包发送（每包 = [pkt_idx][数据片 ≤ 63B]）、
 * 接收按分包序号连续重组。
 *
 * 适用场景：兼容视觉 PC 端"固定长度、单包无序号"的原始线协议（如 50B 收 / 57B 发
 * 的视觉包），host 侧无需感知分包序号元数据。
 *
 * @note 收发判定分别依据 tx_frame_len / rx_frame_len：短帧对话（两侧 ≤ 64B）双向
 *       免序号、长帧对话（两侧 > 64B）双向带序号。建议收发协议帧长保持一致，
 *       否则链路不对称（一侧免序号一侧带序号），接收长度/序号校验会错位。
 * @note USB 为单例（USB_INSTANCE_NUM=1），usb 与 usb_simple 后端不可同时注册，
 *       只能启用其一。
 */

#ifndef COMM_MEDIA_USB_SIMPLE_H
#define COMM_MEDIA_USB_SIMPLE_H

#include "comm_media.h"

#ifdef DRV_COMM_USED

#include "bsp_usb.h"

/* USB 介质派生结构体（首成员必须为 CommMedia 基类，vtable 约定；字段同 CommMediaUsb） */
typedef struct
{
    CommMedia base;        /* 基类（首成员；发送不持 staging 缓冲，MediaUsbSimpleSend 直接引用 comm 打包缓冲 data） */
    uint8_t *rx_buff;      /* 接收累积缓冲（完整协议帧，不含分包序号；DEF 宏静态绑定，大小 = rx_buff_sz） */
    uint16_t rx_frame_len; /* 完整协议帧长（不含分包序号）= rx_buff_sz（DEF 宏写入；接收判定依据 + 短帧透传目标长度） */
    uint16_t tx_frame_len; /* 完整协议帧长（不含分包序号）= tx_buff_sz（DEF 宏写入；发送分包/透传依据） */
    uint16_t rx_cnt;       /* 已累积字节数（0..rx_frame_len，上交后归零；长帧重组用） */
    uint8_t rx_expect_pkt; /* 期望接收的下一分包序号（帧内 0 起递增；长帧重组用） */
    uint32_t lost_frames;  /* 丢帧计数（短帧长度不符 / 长帧分包错位累加） */
} CommMediaUsbSimple;

/**
 * @brief 静态定义 USB-simple 介质实例
 * @param name        实例名称
 * @param rx_buff_sz  协议帧长（= rx_size + 协议开销，COMM_DEF 传入；接收累积缓冲 = rx_buff_sz）
 * @param tx_buff_sz  协议帧长（= tx_size + 协议开销；发送透传/分包依据，写入 tx_frame_len）
 *
 * @note 展开定义 name##_usb（USBInstance）、name##_daemon（链路对端看门狗，绑定到
 *       name.base.daemon）、name##_rx_buff（完整协议帧接收缓冲，不含分包序号）与
 *       name（CommMediaUsbSimple），并绑定 base.media/base.daemon。
 *       缓冲放普通 RAM（USB 无 DMA）。
 *
 * @example
 *   COMM_MEDIA_USB_SIMPLE_DEF(vis_comm_media, 57, 57); 57B ≤ 64B → 免序号整包透传
 *   COMM_MEDIA_USB_SIMPLE_DEF(vis_comm_media, 100, 100); 100B > 64B → 分包带序号（同 usb）
 */
#define COMM_MEDIA_USB_SIMPLE_DEF(name, rx_buff_sz, tx_buff_sz) \
    USB_INSTANCE_DEF(name##_usb);                               \
    DAEMON_INSTANCE_DEF(name##_daemon);                         \
    static uint8_t name##_rx_buff[(rx_buff_sz)] = {0};          \
    static CommMediaUsbSimple name = {                          \
        .base.media = &name##_usb,                              \
        .base.daemon = &name##_daemon,                          \
        .rx_buff = name##_rx_buff,                              \
        .rx_frame_len = (rx_buff_sz),                           \
        .tx_frame_len = (tx_buff_sz)}

/**
 * @brief 注册 USB-simple 介质后端（不可重入：仅可调用一次）
 * @param media CommMediaUsbSimple 实例指针（COMM_MEDIA_USB_SIMPLE_DEF 定义）
 * @retval 0 成功；-1 参数非法 / bsp 注册失败
 *
 * @note 完成 bsp USBRegister（防重复注册，USB_INSTANCE_NUM=1）+ 挂 vtable +
 *       建立 usb↔media 反向指针 + 清接收累积与序列状态。
 *       接收回调由 MediaUsbSimpleConfig 挂接。
 */
int8_t MediaUsbSimpleRegister(CommMediaUsbSimple *media);

/**
 * @brief 配置 USB-simple 介质后端（可重入：可反复调用改回调）
 * @param media CommMediaUsbSimple 实例指针（须先 MediaUsbSimpleRegister）
 * @param cfg   USB 运行时配置（USB_Config_s*，可传 NULL；USB 无硬件运行期参数）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置失败
 *
 * @note 内部调 bsp USBConfig 并强制接管 rx_callback=MediaUsbSimpleRxHook、
 *       parent=media（反向指针），保证接收统一进 comm 层接收入口
 *       （CommMediaRxHook）；USBConfig 将二者写入实例。
 */
int8_t MediaUsbSimpleConfig(CommMediaUsbSimple *media, USB_Config_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_USB_SIMPLE_H */
