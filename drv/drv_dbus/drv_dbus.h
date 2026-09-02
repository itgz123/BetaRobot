/**
 * @file drv_dbus.h
 * @brief DBUS 遥控器驱动封装
 *
 * @note DRV 层职责：
 *       1. 封装 DBUS 协议解析
 *       2. 提供 BSP 回调注册
 *       3. 不使用 FreeRTOS（队列由 APP 层管理）
 *
 * @note 协议参考 RoboMaster 遥控器（接收机）用户手册"字节控制帧结构"：
 *       帧长 18 字节，位域从字节 0 起按 LSB 优先打包。
 */

#ifndef __DRV_DBUS_H
#define __DRV_DBUS_H

#include "main.h"
#include "bsp_map.h"
#include "drv_daemon.h"

#ifdef HAL_UART_MODULE_ENABLED
#include "bsp_usart.h"
#include "stdint.h"

/*------------- 宏定义 --------------*/

#define DBUS_FRAME_SIZE 18   // DBUS 帧长度
#define DBUS_CHANNEL_COUNT 4 // 摇杆通道数量

// DBUS 通道值范围
#define DBUS_CH_MIN 364     // 通道最小值
#define DBUS_CH_MAX 1684    // 通道最大值
#define DBUS_CH_CENTER 1024 // 通道中间值

/*------------- 类型定义 --------------*/

/**
 * @brief S1/S2 开关位置枚举
 */
typedef enum
{
    DBUS_SW_UP = 1,   // 开关向上
    DBUS_SW_DOWN = 2, // 开关向下
    DBUS_SW_MID = 3,  // 开关中间
} DBUS_SW_e;

/**
 * @brief 键盘按键位枚举（官网文档：Bit0-7 = W/S/A/D/Q/E/Shift/Ctrl）
 * @note 可作为按位操作的位号索引（如 key_count[i]）
 */
typedef enum
{
    DBUS_KEY_W = 0, // Bit0: W 键
    DBUS_KEY_S,     // Bit1: S 键
    DBUS_KEY_A,     // Bit2: A 键
    DBUS_KEY_D,     // Bit3: D 键
    DBUS_KEY_Q,     // Bit4: Q 键
    DBUS_KEY_E,     // Bit5: E 键
    DBUS_KEY_SHIFT, // Bit6: Shift 键
    DBUS_KEY_CTRL,  // Bit7: Ctrl 键
    DBUS_KEY_NUM,   // 按键数量
} DBUS_KeyBit_e;

/**
 * @brief 键盘按键位域联合体（与 16 位原始值共享内存）
 * @note 位域成员便于直接访问按键状态，value 用于整体赋值/拷贝
 */
typedef union
{
#pragma pack(push, 1)
    struct
    {
        uint16_t w : 1;     // Bit0: W 键
        uint16_t s : 1;     // Bit1: S 键
        uint16_t a : 1;     // Bit2: A 键
        uint16_t d : 1;     // Bit3: D 键
        uint16_t q : 1;     // Bit4: Q 键
        uint16_t e : 1;     // Bit5: E 键
        uint16_t shift : 1; // Bit6: Shift 键
        uint16_t ctrl : 1;  // Bit7: Ctrl 键
        uint16_t reserved : 8;
    };
#pragma pack(pop)
    uint16_t value; // 16 位原始键值
} DBUS_Key_t;

/**
 * @brief DBUS 通道数据结构体
 */
typedef struct
{
    float ch[DBUS_CHANNEL_COUNT]; // 4 个摇杆通道值 (-1.0 ~ 1.0)
    DBUS_SW_e s1;                 // S1 开关位置 (1上/2下/3中)
    DBUS_SW_e s2;                 // S2 开关位置 (1上/2下/3中)
    int16_t mouse_x;              // 鼠标 X 轴移动速度
    int16_t mouse_y;              // 鼠标 Y 轴移动速度
    int16_t mouse_z;              // 鼠标 Z 轴移动速度
    uint8_t mouse_press_l;        // 鼠标左键 (0: 松开, 1: 按下)
    uint8_t mouse_press_r;        // 鼠标右键 (0: 松开, 1: 按下)
    DBUS_Key_t key;               // 键盘按键位域 (W/S/A/D/Q/E/Shift/Ctrl)
    float dial;                   // 侧边拨轮 (-1.0 ~ 1.0)
    uint8_t frame_lost;           // 帧丢失标志 (0: 正常, 1: 丢失)
    uint8_t failsafe;             // 失控保护标志 (0: 正常, 1: 失控)
} DBUS_Data_t;

/**
 * @brief DBUS 实例结构体
 * @note 使用指针指向 BSP 实例，在注册时设置 parent
 */
typedef struct DBUSInstance
{
    USARTInstance *usart_inst;   // BSP 实例指针
    DBUS_Data_t dbus_data;       // 解析后的通道数据（在中断回调中填充）
    DaemonInstance *daemon;      // 看门狗监控实例指针
    uint64_t lost_start_time_us; // 丢帧/失控开始时间戳 (us)，0 表示正常
    uint8_t signal_lost;         // 信号丢失确认标志（0: 正常, 1: 失控）
    uint64_t lost_timeout_us;    // 丢帧/失控确认超时 (us)
} DBUSInstance;

typedef struct
{
    BoardUART_e uart_e;               // 板载UART枚举（用于查找硬件映射）
    uint16_t daemon_reload;           // daemon 喂狗重载值
    DaemonFaultAction_e daemon_fault; // daemon 离线故障动作
    uint32_t lost_timeout_ms;         // 丢帧/失控确认超时 (ms)，0=立即标志
} DBUS_Config_s;

/*------------- 实例定义宏 --------------*/
/**
 * @brief 静态定义 DBUS 实例
 * @param name 实例名称
 *
 * @note 使用 BSP 层的 USART_INSTANCE_DEF 宏定义底层实例
 *       parent 指针在注册时设置，指向 DBUSInstance 自身
 *
 * @example
 *   DBUS_INSTANCE_DEF(dbus_inst);
 */
#define DBUS_INSTANCE_DEF(name)                       \
    USART_INSTANCE_DEF(name##_uart, DBUS_FRAME_SIZE); \
    DAEMON_INSTANCE_DEF(name##_daemon);               \
    static DBUSInstance name = {                      \
        .usart_inst = &name##_uart,                   \
        .daemon = &name##_daemon,                     \
    }

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 DBUS 实例（仅调用一次）
 * @param instance DBUS 实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 内部调用 USARTRegister 注册 BSP 层 USART 实例。
 *       不配置硬件参数和运行参数（由 DBUSConfig 负责）。
 */
int8_t DBUSRegister(DBUSInstance *instance);

/**
 * @brief 配置 DBUS 实例（可重复调用）
 * @param instance DBUS 实例指针
 * @param config   配置结构体指针（含 uart_e/daemon/超时）
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 填充 USART 硬件映射，设置 DMA 模式和回调、daemon 看门狗。
 *       可重复调用以更新运行时参数。
 *       要求在 DBUSRegister 之后调用。
 */
int8_t DBUSConfig(DBUSInstance *instance, const DBUS_Config_s *config);

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __DRV_DBUS_H */
