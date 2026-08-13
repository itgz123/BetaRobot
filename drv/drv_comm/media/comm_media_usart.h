/**
 * @file comm_media_usart.h
 * @brief 通信框架-硬件层（Media）UART 后端
 *
 * 把 bsp_usart 包装成统一"任意长度数据单元"通道：
 *   - 发送：vtable->send → 拷贝 comm 打包缓冲到 media->tx_buff → USARTTransmit
 *           （按 bsp 实例 tx_mode：BLOCK/IT/DMA）
 *   - 接收：bsp DMA+IDLE 收完一段 → 适配钩子 → MediaHandleRx → comm 层 rx_cb
 *
 * @note COMM_DEF 通过 token 拼接 COMM_##media_type_##_DEF 分发到本宏。
 */

#ifndef COMM_MEDIA_USART_H
#define COMM_MEDIA_USART_H

#include "comm_media.h"

#ifdef DRV_COMM_USED

#include "bsp_usart.h"

/* UART 介质派生结构体（首成员必须为 CommMedia 基类，vtable 约定） */
typedef struct
{
    CommMedia base; /* 基类（首成员；发送缓冲 tx_buff/tx_buff_size 在基类，供协议分包直接写入） */
} CommMediaUsart;

/**
 * @brief 静态定义 UART 介质实例（内部用 USART_INSTANCE_DEF 定义 bsp 实例并绑定）
 * @param name        实例名称
 * @param rx_buff_sz  bsp 接收缓冲区大小（传给 USART_INSTANCE_DEF）
 * @param tx_buff_sz  media 发送缓冲区大小（app payload + 协议开销，编译期确定）
 *
 * @note 展开定义 name##_usart（USARTInstance，含接收缓冲）与 name（CommMediaUsart，
 *       含发送缓冲），且 name.base.media 指向 name##_usart——运行时无需另传 bsp 实例。
 *       DMA_RAM 在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA，M4 上为空（同 bsp_usart）。
 *
 * @example
 *   COMM_MEDIA_USART_DEF(uart_comm, 64, 32);
 */
#define COMM_MEDIA_USART_DEF(name, rx_buff_sz, tx_buff_sz)   \
    USART_INSTANCE_DEF(name##_usart, rx_buff_sz);            \
    static uint8_t name##_tx_buff[tx_buff_sz] DMA_RAM = {0}; \
    static CommMediaUsart name = {                           \
        .base.media = &name##_usart,                         \
        .base.tx_buff = name##_tx_buff,                      \
        .base.tx_buff_size = tx_buff_sz}

/**
 * @brief 注册 UART 介质后端（不可重入：仅可调用一次）
 * @param media CommMediaUsart 实例指针（COMM_MEDIA_USART_DEF 定义）
 * @retval 0 成功；-1 参数非法 / bsp 注册失败
 *
 * @note 完成 bsp USARTRegister（防重复注册）+ 挂 vtable/type + 建立
 *       usart↔media 反向指针。介质参数配置由 MediaUsartConfig 负责。
 */
int8_t MediaUsartRegister(CommMediaUsart *media);

/**
 * @brief 配置 UART 介质后端（可重入：可反复调用改参数）
 * @param media CommMediaUsart 实例指针（须先 MediaUsartRegister）
 * @param cfg   UART 运行时配置（uart_e/发送模式）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置失败
 *
 * @note 内部调 bsp USARTConfig（启动 DMA 接收）；USARTConfig 会写入
 *       config->rx_callback，本函数随后强制接管为适配钩子 MediaUsartRxHook，
 *       保证接收统一进 MediaHandleRx → rx_cb（引擎挂接），业务不直接走 bsp 回调。
 */
int8_t MediaUsartConfig(CommMediaUsart *media, USART_Config_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_USART_H */
