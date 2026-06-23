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
    USARTInstance *usart_inst;   // BSP 实例指针
    SBUS_Data_t sbus_data;       // 解析后的通道数据（在中断回调中填充）
    DaemonInstance *daemon;      // 看门狗监控实例指针
    uint64_t lost_start_time_us; // 丢帧/失控开始时间戳 (us)，0 表示正常
    uint8_t signal_lost;         // 信号丢失确认标志（超过 lost_timeout_us 仍未恢复） (0: 正常, 1: 失控)
    uint64_t lost_timeout_us;    // 丢帧/失控确认超时 (us)，0=立即标志
} SBUSInstance;

/*------------- 配置结构体 --------------*/

typedef struct
{
    BoardUART_e uart_e;       // 板载UART枚举（注册时用于查找映射）
    uint16_t daemon_reload;   // daemon 喂狗重载值
    uint8_t daemon_fault;     // daemon 离线故障动作
    uint32_t lost_timeout_ms; // 丢帧/失控确认超时 (ms)，0=立即标志
} SBUS_Init_Config_s;

/*------------- 实例定义宏 --------------*/
/**
 * @brief 静态定义 SBUS 实例
 * @param name 实例名称
 *
 * @note 使用 BSP 层的 USART_INSTANCE_DEF 宏定义底层实例
 *       parent 指针在注册时设置，指向 SBUSInstance 自身
 *
 * @example
 *   SBUS_INSTANCE_DEF(sbus_inst);
 */
#define SBUS_INSTANCE_DEF(name)                       \
    USART_INSTANCE_DEF(name##_uart, SBUS_FRAME_SIZE); \
    DAEMON_INSTANCE_DEF(name##_daemon);               \
    static SBUSInstance name = {                      \
        .usart_inst = &name##_uart,                   \
        .daemon = &name##_daemon,                     \
    }

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 SBUS 实例
 * @param instance SBUS 实例指针（需先通过宏定义）
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t SBUSRegister(SBUSInstance *instance, const SBUS_Init_Config_s *config);

#endif /* BSP_UART_MODULE_ENABLED */

#endif /* __DRV_SBUS_H */
