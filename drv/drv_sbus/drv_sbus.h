/**
 * @file drv_sbus.h
 * @brief SBUS 遥控器驱动封装
 *
 * @note DRV 层职责：
 *       1. 封装 SBUS 协议解析
 *       2. 提供 BSP 回调注册
 *       3. 不使用 FreeRTOS（队列由 APP 层管理）
 */

#ifndef __DRV_SBUS_H
#define __DRV_SBUS_H

#include "main.h"
#include "bsp.h"
#include "drv_daemon.h"

#ifdef HAL_UART_MODULE_ENABLED
#include "bsp_usart.h"
#include "stdint.h"

/*------------- 宏定义 --------------*/

#define SBUS_FRAME_SIZE 25      // SBUS 帧长度
#define SBUS_CHANNEL_COUNT 16   // 模拟通道数量
#define SBUS_DIGITAL_CH_COUNT 2 // 数字通道数量

// SBUS 通道值范围
#define SBUS_CH_MIN 353     // 通道最小值172
#define SBUS_CH_MAX 1695    // 通道最大值1811
#define SBUS_CH_CENTER 1024 // 通道中间值992

// SBUS 帧头和帧尾
#define SBUS_HEADER 0x0F            // 帧头
#define SBUS_FOOTER 0x00            // 帧尾（正常帧）
#define SBUS_FOOTER_FRAME_LOST 0x04 // 帧丢失标志
#define SBUS_FOOTER_FAILSAFE 0x08   // 失控保护标志

/*------------- 类型定义 --------------*/

/**
 * @brief SBUS 通道数据结构体
 */
typedef struct
{
    float ch[SBUS_CHANNEL_COUNT];              // 16 个模拟通道值 (-1.0 ~ 1.0)
    uint8_t digital_ch[SBUS_DIGITAL_CH_COUNT]; // 2 个数字通道 (0 或 1)
    uint8_t frame_lost;                        // 帧丢失标志 (0: 正常, 1: 丢失)
    uint8_t failsafe;                          // 失控保护标志 (0: 正常, 1: 失控)
} SBUS_Data_t;

/**
 * @brief SBUS 实例结构体
 * @note 使用指针指向 BSP 实例，在注册时设置 parent
 */
typedef struct SBUSInstance
{
    USARTInstance *usart_inst;                   // BSP 实例指针
    void (*app_callback)(struct SBUSInstance *); // APP 层回调
    SBUS_Data_t sbus_data;                       // 解析后的通道数据（在中断回调中填充）
    DaemonInstance *daemon;                      // 看门狗监控实例指针
} SBUSInstance;

/*------------- 外部函数声明（供 DEF 宏使用）--------------*/
void SBUSUARTRxCallback(USARTInstance *usart_inst);

/*------------- 实例定义宏 --------------*/
/**
 * @brief 静态定义 SBUS 实例（同时定义缓冲区）
 * @param name     实例名称
 * @param uart_idx 板载 UART 枚举（BoardUART_e）
 * @param app_cb   APP 层回调函数
 *
 * @note 使用 BSP 层的 USART_INSTANCE_DEF 宏定义底层实例
 *       parent 指针在注册时设置，指向 SBUSInstance 自身
 *
 * @example
 *   SBUS_INSTANCE_DEF(sbus_inst, UART_SBUS_2, AppCallback, 30);
 */
#define SBUS_INSTANCE_DEF(name, uart_idx, app_cb, reload)                                    \
    USART_INSTANCE_DEF(name##_uart, uart_idx, USART_DMA_MODE, 25, SBUSUARTRxCallback, NULL); \
    DAEMON_INSTANCE_DEF(name##_daemon, reload);                                              \
    static SBUSInstance name = {                                                             \
        .usart_inst = &name##_uart,                                                          \
        .app_callback = app_cb,                                                              \
        .daemon = &name##_daemon,                                                            \
    }

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 SBUS 实例
 * @param instance SBUS 实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 所有参数通过 SBUS_INSTANCE_DEF 宏预设，注册时只需传入实例指针
 */
int8_t SBUSRegister(SBUSInstance *instance);

/**
 * @brief 解析 SBUS 数据帧
 * @param data 原始数据指针（25字节）
 * @return 解析后的 SBUS 数据
 * @note 在中断上下文中调用（由 SBUSUARTRxCallback 触发）
 */
SBUS_Data_t SBUSDecodeFrame(const uint8_t *data, uint16_t len);

#endif /* BSP_UART_MODULE_ENABLED */

#endif /* __DRV_SBUS_H */