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
#include "bsp_map.h"
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

/*------------- SBUS 原始帧联合体 --------------*/

/**
 * @brief SBUS 原始帧联合体
 * @note 映射 25 字节 SBUS 帧：头(1) + 通道数据(22) + 标志位(1) + 帧尾(1)
 */
typedef union
{
    uint8_t raw[25];
    struct
    {
        uint8_t header;      // [0]  帧头 0x0F
        uint8_t ch_data[22]; // [1-22] 16通道 × 11位
        struct               // [23] 标志位
        {
            uint8_t ch17 : 1;       // bit 0: 数字通道 1
            uint8_t ch18 : 1;       // bit 1: 数字通道 2
            uint8_t frame_lost : 1; // bit 2: 帧丢失
            uint8_t failsafe : 1;   // bit 3: 失控保护
            uint8_t reserved : 4;   // bits 4-7
        } flags;
        uint8_t footer; // [24] 帧尾
    } frame;
} SBUS_RawFrame_u;

/**
 * @brief 从 SBUS 原始帧中提取 11 位通道原始值
 * @param frame SBUS 原始帧指针
 * @param ch    通道索引 (0-15)
 * @return uint16_t 11 位原始值 (0-2047)
 * @note 第 3 个字节读取后会溢出到 flags 字节，但被 0x07FF 掩码清除
 */
static inline uint16_t SBUS_GetChannelRaw(const SBUS_RawFrame_u *frame, uint8_t ch)
{
    uint16_t bit_off = (uint16_t)ch * 11; // 从 ch_data 起始的位偏移
    uint8_t byte_off = bit_off / 8;       // 从 raw[1] 起的字节偏移
    uint8_t bit_rem = bit_off % 8;        // 字节内位偏移
    // 从 raw[1 + byte_off] 读取最多 3 字节，超出的位被 0x07FF 清除
    return (uint16_t)(((uint32_t)frame->raw[1 + byte_off] >> bit_rem) |
                      ((uint32_t)frame->raw[1 + byte_off + 1] << (8 - bit_rem)) |
                      ((uint32_t)frame->raw[1 + byte_off + 2] << (16 - bit_rem))) &
           0x07FF;
}

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

/**
 * @brief SBUS 配置结构体（Config 函数使用）
 *
 * @note 可重复调用 SBUSConfig 运行时修改超时参数。
 */
typedef struct
{
    uint32_t lost_timeout_ms; // 丢帧/失控确认超时 (ms)，0=立即标志
} SBUS_Config_s;

/**
 * @brief SBUS 注册配置结构体（Register 函数使用）
 *
 * @note 仅调用一次。内嵌 SBUS_Config_s + 硬件/daemon 相关字段。
 */
typedef struct
{
    SBUS_Config_s sbus_config; // SBUS 配置（传入 Config）
    BoardUART_e uart_e;        // 板载UART枚举（注册时用于查找映射）
    uint16_t daemon_reload;    // daemon 喂狗重载值
    uint8_t daemon_fault;      // daemon 离线故障动作
} SBUS_Register_Config_s;

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
 * @brief 配置 SBUS 实例（可重复调用，不修改 static 变量）
 * @param instance SBUS 实例指针
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 仅配置 instance 字段（parent 指针、超时参数等），不调用子模块 Register。
 *       可重复调用以更新运行时参数。
 */
int8_t SBUSConfig(SBUSInstance *instance, const SBUS_Config_s *config);

/**
 * @brief 注册 SBUS 实例（仅调用一次）
 * @param instance SBUS 实例指针（需先通过宏定义）
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 内部调用 SBUSConfig 后，注册 USART 和 Daemon 子模块。
 */
int8_t SBUSRegister(SBUSInstance *instance, const SBUS_Register_Config_s *reg_cfg);

#endif /* BSP_UART_MODULE_ENABLED */

#endif /* __DRV_SBUS_H */
