/**
 * @file comm_media_usart.h
 * @brief 通信框架-硬件层（Media）UART 后端
 *
 * 把 bsp_usart 包装成统一"任意长度数据单元"通道：
 *   - 发送：vtable->send → 拷贝 comm 打包缓冲到 media->tx_buff → USARTTransmit
 *           （按 bsp 实例 tx_mode：BLOCK/IT/DMA）
 *   - 接收：bsp DMA+IDLE 收完一段 → 适配钩子 → CommMediaRxHook（comm 层接收入口）
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
    CommMedia base;              /* 基类（首成员） */
    uint8_t *tx_buff;            /* 发送 staging 缓冲（协议分包写入；DMA 异步发送期间须常驻） */
    const uint16_t tx_buff_size; /* 发送缓冲大小（= tx payload + 协议开销，编译期固定、DEF 宏写入） */
    BSP_Transfer_Mode_e tx_mode; /* 发送模式（Config 写入，每次发送透传给 bsp） */
    uint32_t timeout_ms;         /* 发送等待就绪超时（Config 写入，0 = 不等待） */
    uint32_t tx_fail;            /* 发送失败计数（含重试后仍失败；只增不清，调试用） */
    /* 本链路"一帧多长"：接收钩子判帧长用它（见 MediaUsartRxHook），Config 时写入。
     * 注：重启接收用的模式/长度不在这里存——停摆自恢复统一走 bsp 的
     * USARTRecoverRxIfStalled，参数取 bsp 实例的 rx_mode/rx_xfer_len（bsp 保证在所有
     * 失败分支之前记录），限频基准也在 bsp 内，上层不必再各存一份。 */
    uint16_t rx_len; /* 单次接收长度（Config 写入，0 → 用 bsp 实例的 rx_buff_size） */
} CommMediaUsart;

/* UART 介质运行期配置（CommConfig 的 media_cfg 指向本结构体） */
typedef struct
{
    USART_Config_s usart;        /* bsp 总线级配置（uart_e / parent / 回调；parent 会被本后端覆盖为自身） */
    BSP_Transfer_Mode_e tx_mode; /* 发送模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE） */
    BSP_Transfer_Mode_e rx_mode; /* 接收模式（同上） */
    uint32_t timeout_ms;         /* 发送等待就绪超时（毫秒），0 = 不等待、忙即失败 */
    uint16_t rx_len;             /* 单次接收长度，0 = 用 bsp 实例的 rx_buff_size（常用） */
} CommMediaUsartConfig_s;

/**
 * @brief 静态定义 UART 介质实例（内部用 USART_INSTANCE_DEF 定义 bsp 实例并绑定）
 * @param name        实例名称
 * @param rx_buff_sz  bsp 接收缓冲区大小（传给 USART_INSTANCE_DEF）
 * @param tx_buff_sz  media 发送缓冲区大小（app payload + 协议开销，编译期确定）
 *
 * @note 展开定义 name##_usart（USARTInstance，含接收缓冲）、name##_daemon（链路对端
 *       看门狗，绑定到 name.base.daemon）与 name（CommMediaUsart，含发送缓冲），且
 *       name.base.media 指向 name##_usart——运行时无需另传 bsp 实例。
 *       DMA_RAM 在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA，M4 上为空（同 bsp_usart）。
 *
 * @example
 *   COMM_MEDIA_USART_DEF(uart_comm, 64, 32);
 */
#define COMM_MEDIA_USART_DEF(name, rx_buff_sz, tx_buff_sz)                                                             \
    USART_INSTANCE_DEF(name##_usart, rx_buff_sz);                                                                      \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                \
    static uint8_t name##_tx_buff[tx_buff_sz] DMA_RAM = {0};                                                           \
    static CommMediaUsart name = {.base.media = &name##_usart,                                                         \
                                  .base.daemon = &name##_daemon,                                                       \
                                  .tx_buff = name##_tx_buff,                                                           \
                                  .tx_buff_size = tx_buff_sz}

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
 * @param cfg   UART 运行期配置（总线参数 + 收发模式与超时）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置或启动接收失败
 *
 * @note 内部调 bsp USARTConfig 并使用 cfg->usart.rx_callback；本函数把 parent
 *       与 rx_callback 覆盖为自身适配钩子 MediaUsartRxHook，保证接收统一进
 *       comm 层接收入口（CommMediaRxHook），业务不直接走 bsp 回调。
 *       随后按 rx_mode/rx_len 启动接收常开流（bsp 的 Config 不再自动启动接收）。
 * @note 参数在动硬件**之前**全部校验（模式、长度），失败只返回 -1、不留半应用状态；
 *       但"USARTConfig 成功而启动接收失败"这种残留态无法预先排除 —— 该失败由接收停摆
 *       自恢复兜住（bsp 的 USARTRecoverRxIfStalled 会按已记录的模式/长度反复重试）。
 * @note 接收停摆（bsp 续收最终失败、rx_armed 被清）后，由本后端的 vtable.offline 钩子在
 *       DaemonTask（任务上下文）里调 bsp 的 USARTRecoverRxIfStalled 重启；
 *       该钩子经 CommConfig 挂到介质 daemon 上；daemon_reload 配 0 会被 CommConfig
 *       提升为默认值（本后端有 offline 钩子，提升条件成立），故该通道始终有效。
 * @note rx_mode **不接受 BSP_BLOCK_MODE**：阻塞接收不触发 rx_callback，配错等于本链路
 *       静默收不到任何数据；而 CommMediaUsartConfig_s 零初始化出来正好是 BLOCK，
 *       "少配一个字段"就中招，故明确拒绝而不是放行。
 */
int8_t MediaUsartConfig(CommMediaUsart *media, CommMediaUsartConfig_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_USART_H */
