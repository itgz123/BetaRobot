/**
 * @file bsp_usb.h
 * @brief USB 设备协议栈对外 API（纯 C，静态内存，参考 XRUSB 多类组合架构）
 *
 * 用法（对齐 bsp_usart 的 Register/Config 三段式）：
 *   USB_INSTANCE_DEF(usb_vcp, 256, 4);        // 静态实例 + 缓冲
 *   USBRegister(&usb_vcp);                     // 入注册表（一次）
 *   USBConfig(&usb_vcp, &cfg);                 // 填句柄/VID/PID/回调（默认挂 CDC 类）
 *   USBAddClass(&usb_vcp, 0, &hid_vtable, &hid); // 可选：配置 1 追加 HID/DFU 等第二类
 *   USBAddClass(&usb_vcp, 1, &hid_vtable, &hid); // 配置 2 的可选类集合（多配置）
 *   USBStart(&usb_vcp);                        // 配 FIFO + 连接
 *
 * 多类组合 + 多配置（对照 XRUSB DeviceComposition）：
 *   每个配置是一份类槽表（USBConfig_t configs[]），每个类是一个 USBClassSlot_t，
 *   提供请求处理 + 描述符/端点元数据；core 在 Init/Reset 时统一分配接口号 +
 *   端点池领取端点号 + 填充描述符块，配置描述符运行时拼接（bNumInterfaces /
 *   wTotalLength 自动累计），类/厂商请求按接口号 / 端点地址反查所属类分发。
 *   SET_CONFIGURATION(n) 切换激活配置：解绑旧配置类 → 绑定新配置类（重新分配接口号/端点）。
 *
 * 三块板差异仅句柄名（usb_map[] 映射 + BoardUSB_e 枚举）由 bsp_map 吸收，
 * 协议栈配额（实例数/类数/端点/FIFO）用 bsp_usb_types.h 默认值（可 app_cfg.h 覆盖），
 * 本模块源码零 #if defined(STM32...) 分支。
 * 缓冲统一 __attribute__((aligned(4)))，不依赖板级 ALIGN_4/DMA_RAM。
 */

#ifndef __BSP_USB_H
#define __BSP_USB_H

#include "bsp_map.h"

#ifdef HAL_PCD_MODULE_ENABLED

#include "stdint.h"
#include "main.h"
#include "bsp_usb_types.h"
#include "bsp_usb_core.h"
#include "bsp_usb_cdc.h"
/* TODO[HID停用]：Windows 复合设备 HID 枚举问题暂缓，HID 类不编译；恢复时取消注释 */
/* #include "bsp_usb_hid_kbd.h" */

/*------------- 配置类槽表（对照 XRUSB DeviceComposition 的每配置 item 表） --------------
 * 每个配置是一份独立的类槽表；SET_CONFIGURATION(n) 切换激活配置时，
 * core 解绑旧配置类、绑定新配置类（接口号/端点各自重新分配）。
 */
typedef struct
{
    USBClassSlot_t classes[USB_CLASS_MAX]; /* 类槽（0 为默认 CDC，USBAddClass 追加） */
    uint8_t class_count;                   /* 该配置已注册类数 */
} USBConfig_t;

/*------------- 实例结构 --------------*/
typedef struct USBInstance
{
    void *parent;              /* 父实例指针（DRV 层设置） */
    BoardUSB_e usb_e;          /* 板载 USB 枚举（Config 时查 usb_map） */
    PCD_HandleTypeDef *handle; /* PCD 句柄（Config 时填充） */

    uint8_t inited;       /* Register 后置 1 */
    uint8_t configured;   /* 主机下发 SetConfiguration=1 后置 1 */
    uint8_t config_value; /* 当前配置值 0/1 */

    uint16_t vid; /* 厂商 ID（Config） */
    uint16_t pid; /* 产品 ID（Config） */
    uint16_t bcd; /* 设备版本 bcdDevice（Config） */

    /* 静态缓冲（USB_INSTANCE_DEF 宏生成并赋值，USB_EPDefaults 填入端点对象） */
    uint8_t (*ep_buff)[2][2][64]; /* 数据端点缓冲池 [ep][dir][block][64B] */
    uint8_t *rx_buff;             /* CDC RX 环形缓冲 */
    uint32_t rx_size;             /* CDC RX 环容量 */
    USB_TXOp_t *tx_ops;           /* CDC TX op 环 */
    uint32_t tx_ops_num;          /* CDC TX op 环容量 */

    USBEP0_t ep0;                  /* EP0 控制传输状态机 */
    USBEndpoint ep[USB_EP_MAX][2]; /* 数据端点对象（[n][dir]） */
    USB_EPPool_t ep_pool;          /* 端点池（类 bind 时领取端点号） */

    USBConfig_t configs[USB_CONFIG_MAX]; /* 多配置类槽表（0=配置1，1=配置2...；对照 XRUSB configs） */
    uint8_t class_binded;                /* 当前激活配置的类已绑定（接口号/端点已分配） */

    USBCDC cdc; /* CDC-ACM 类 */
    /* TODO[HID停用]：恢复时取消注释 */
    /* USBHIDKbd hid_kbd; */
} USBInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 USB 实例（同时定义全部缓冲）
 * @param name     实例名称
 * @param rx_size  RX 环形队列缓冲字节数（环实际容量 = rx_size - 1）
 * @param tx_ops   TX op 环容量（op 数；实际可用 = tx_ops - 1）
 *
 * 缓冲布局：数据端点池 [USB_EP_MAX][2][2][64]（每端点每方向双缓冲两块 64B，
 * EP0 只用第一块）、RX 环、TX op 数组。全部 4 字节对齐。
 * 端点实际使用由 USB_EPPool 在类 bind 时分配。
 *
 * @example
 *   USB_INSTANCE_DEF(usb_vcp, 256, 4);
 */
#define USB_INSTANCE_DEF(name, _rx_size, _tx_ops_num)                                                 \
    static uint8_t name##_ep_buff[USB_EP_MAX][2][2][64] __attribute__((aligned(4))) = {0};            \
    static uint8_t name##_rx_buff[_rx_size] __attribute__((aligned(4))) = {0};                        \
    static USB_TXOp_t name##_tx_ops[_tx_ops_num] = {0};                                               \
    static USBInstance name = {                                                                       \
        .ep_buff = name##_ep_buff,                                                                    \
        .rx_buff = name##_rx_buff,                                                                    \
        .rx_size = _rx_size,                                                                          \
        .tx_ops = name##_tx_ops,                                                                      \
        .tx_ops_num = _tx_ops_num,                                                                    \
        .cdc = {                                                                                      \
            .line_coding = {.dwDTERate = 115200, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8}, \
            .rx_ring = {.buff = name##_rx_buff, .size = _rx_size, .head = 0, .tail = 0},              \
            .tx_ops = {.ops = name##_tx_ops, .capacity = _tx_ops_num, .head = 0, .tail = 0},          \
        },                                                                                            \
    }

/*------------- 配置结构体 --------------*/

/**
 * @brief USB 运行时配置（用于 USBConfig）
 */
typedef struct
{
    BoardUSB_e usb_e; /* 板载 USB 枚举（查 usb_map 拿 PCD 句柄） */
    uint16_t vid;     /* 厂商 ID */
    uint16_t pid;     /* 产品 ID */
    uint16_t bcd;     /* 设备版本 bcdDevice */

    /* 类回调（可为 NULL） */
    void (*line_coding_cb)(USBInstance *inst, const USB_LineCoding_t *lc);
    void (*ctrl_line_cb)(USBInstance *inst, USBCDCControlBit_e state);
} USB_Config_s;

/*------------- 对外 API --------------*/

/**
 * @brief 注册 USB 实例（仅调用一次）
 * @retval 0 成功；-1 失败（参数非法 / 重复 / 超过 USB_INSTANCE_NUM）
 */
int8_t USBRegister(USBInstance *instance);

/**
 * @brief 配置 USB 实例（可重复调用）
 * @param instance USB 实例指针
 * @param config   配置（usb_e/vid/pid/bcd/回调）
 * @retval 0 成功；-1 失败（参数非法）
 */
int8_t USBConfig(USBInstance *instance, const USB_Config_s *config);

/**
 * @brief 追加一个设备类到指定配置的类槽（需在 USBStart 前调用）
 * @param instance  实例指针
 * @param cfg_index 配置索引（0 = 配置 1；< USB_CONFIG_MAX）
 * @param vtable    类 vtable（提供请求处理 + 描述符/端点元数据）
 * @param ctx       类实例上下文（CDC 传 &cdc）
 * @retval 0 成功；-1 失败（参数非法 / 配置索引越界 / 类槽满）
 */
int8_t USBAddClass(USBInstance *instance, uint8_t cfg_index, const USBClassVTable_t *vtable,
                   void *ctx);

/**
 * @brief 启动：配 FIFO + HAL_PCD_Start + DevConnect
 * @retval 0 成功；-1 失败
 */
int8_t USBStart(USBInstance *instance);

/**
 * @brief 停止：DevDisconnect + HAL_PCD_Stop
 * @retval 0 成功；-1 失败
 */
int8_t USBStop(USBInstance *instance);

/**
 * @brief 发送（非阻塞）
 * @retval 0 入队成功；-1 失败（未配置 / op 环满）
 * @note data 缓冲须在发送完成前保持有效
 */
int32_t USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len);

/**
 * @brief 带完成回调的发送（非阻塞；对照 XRUSB WritePort::Finish）
 * @param on_done  发送完成回调（可为 NULL）：该次发送全部数据（含 ZLP）发完后调用一次，
 *                 参数 = done_ctx + 已发字节数；此时 data 缓冲可安全复用
 * @param done_ctx 回调上下文（通常传 instance）
 * @retval 0 入队成功；-1 失败（未配置 / op 环满）
 */
int32_t USBTransmitEx(USBInstance *instance, const uint8_t *data, uint16_t len,
                      void (*on_done)(void *ctx, uint16_t len), void *done_ctx);

/**
 * @brief 接收：从 RX 环取数据
 * @retval 实际取到字节数
 */
int32_t USBReceive(USBInstance *instance, uint8_t *data, uint16_t len);

/**
 * @brief 是否已连接（配置完成且未断开）
 * @retval 1 已连接；0 未连接/参数非法
 */
uint8_t USBIsConnected(USBInstance *instance);

#endif /* HAL_PCD_MODULE_ENABLED */

#endif /* __BSP_USB_H */
