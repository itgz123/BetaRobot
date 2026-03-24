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
#include "bsp_cfg.h"
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
 * @brief SBUS 实例结构体
 * @note 内嵌 USARTInstance，直接管理 BSP 层实例
 */
typedef struct SBUSInstance
{
    USARTInstance usart_inst;                    // 内嵌 BSP 实例
    void (*app_callback)(struct SBUSInstance *); // APP 层回调
} SBUSInstance;

/**
 * @brief SBUS 原始帧结构体
 * @note 用于中断到任务的队列传递
 */
typedef struct
{
    uint8_t data[25]; // SBUS 原始数据（25字节）
    uint16_t len;     // 数据长度
} SBUS_RawFrame_t;

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

/*------------- 实例定义宏 --------------*/
/**
 * @brief 静态定义 SBUS 实例（同时定义缓冲区）
 * @param name 实例名称
 *
 * @note handle 和 callback 在注册时设置
 *       Cortex-M7 缓冲区放入 RAM_D1 以支持 DMA 访问
 *
 * @example
 *   SBUS_INSTANCE_DEF(sbus_inst);
 */
#if CPU_CORE == CORTEX_M7
#define SBUS_INSTANCE_DEF(name)                                                  \
    static uint8_t name##_rx_buff[25] __attribute__((section(".ram_d1"))) = {0}; \
    SBUSInstance name = {                                                        \
        .usart_inst = {                                                          \
            .handle = NULL,                                                      \
            .tx_mode = USART_DMA_MODE,                                           \
            .rx_buff = name##_rx_buff,                                           \
            .rx_buff_size = 25,                                                  \
            .rx_len = 0,                                                         \
            .rx_callback = NULL},                                                \
        .app_callback = NULL}
#else
#define SBUS_INSTANCE_DEF(name)              \
    static uint8_t name##_rx_buff[25] = {0}; \
    SBUSInstance name = {                    \
        .usart_inst = {                      \
            .handle = NULL,                  \
            .tx_mode = USART_DMA_MODE,       \
            .rx_buff = name##_rx_buff,       \
            .rx_buff_size = 25,              \
            .rx_len = 0,                     \
            .rx_callback = NULL},            \
        .app_callback = NULL}
#endif

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 SBUS 实例
 * @param instance SBUS 实例指针（需先通过宏定义）
 * @param uart_e 板载 UART 枚举（BoardUART_e）
 * @param app_callback APP 层回调函数
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t SBUSRegister(SBUSInstance *instance, BoardUART_e uart_e, void (*app_callback)(SBUSInstance *));

/**
 * @brief 解析 SBUS 数据帧
 * @param data 原始数据指针（25字节）
 * @return 解析后的 SBUS 数据
 * @note 在任务上下文中调用
 */
SBUS_Data_t SBUSDecodeFrame(const uint8_t *data, uint16_t len);

#endif /* __DRV_SBUS_H */