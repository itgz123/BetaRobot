/**
 * @file bsp_can.h
 * @brief CAN驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/滤波器容量/中断/DMA/FD模式等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "main.h"
#include "stdint.h"

/*------------- 常量定义 --------------*/

/**
 * @brief CAN ID 未使用标记（-1 = 0xFFFFFFFF）
 * @note 当 tx_id 或 rx_id_list 中的条目设置为 CAN_ID_UNUSED 时，
 *       表示该实例不使用发送功能或该接收ID槽位无效
 */
#define CAN_ID_UNUSED ((uint32_t)0xFFFFFFFF)

/*------------- 类型定义 --------------*/

/**
 * @brief CAN过滤器模式枚举
 */
typedef enum
{
    CAN_FILTER_MODE_MASK = 0, // 掩码模式（支持范围匹配）
    CAN_FILTER_MODE_LIST = 1  // 列表模式（最多4个精确ID）
} CANFilterMode_e;

/**
 * @brief CAN实例结构体
 */
typedef struct CANInstance
{
    void *parent;                              // 父实例指针（由 DRV 层设置）
    BoardCAN_e can_e;                          // 板载CAN枚举（注册时用于查找映射）
    CAN_Map_t map;                             // CAN映射（注册时自动填充）
    uint32_t tx_id;                            // 发送标准ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFilterMode_e filter_mode;               // 过滤器模式（掩码/列表）
    uint8_t rx_id_count;                       // 列表模式下有效接收ID数量（注册时自动计算）
    uint32_t rx_id_list[4];                    // 接收ID列表；CAN_ID_UNUSED(-1) 表示该槽位无效
    uint32_t rx_mask;                          // 掩码模式：掩码值（列表模式不使用）
    uint32_t rx_id_matched;                    // 实际匹配到的ID（回调中使用）
    uint8_t tx_buff[8];                        // 发送缓存
    uint8_t rx_buff[8];                        // 接收缓存
    uint8_t rx_len;                            // 接收长度（字节）
    void (*rx_callback)(struct CANInstance *); // 接收完成回调
#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    FDCAN_TxHeaderTypeDef tx_header; // FDCAN发送头
#else
    CAN_TxHeaderTypeDef tx_header; // CAN发送头
    uint32_t tx_mailbox;           // BxCAN发送邮箱索引
#endif
} CANInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief CAN 初始化配置结构体
 * @note 统一掩码模式和列表模式，通过 filter_mode 区分
 * @note rx_id_count 会根据 rx_id_list 自动计算，无需用户填写
 */
typedef struct
{
    uint32_t tx_id;                            // 发送标准ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFilterMode_e filter_mode;               // 过滤器模式（掩码/列表）
    uint32_t rx_id_list[4];                    // 接收ID列表；CAN_ID_UNUSED(-1) 表示该槽位无效
    uint32_t rx_mask;                          // 掩码模式：掩码值（列表模式不使用）
    void (*rx_callback)(struct CANInstance *); // 接收完成回调（可为NULL）
} CAN_Init_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义CAN实例（仅身份绑定）
 * @param name    实例名称
 * @param can_idx 板载CAN枚举（BoardCAN_e）
 */
#define CAN_INSTANCE_DEF(name, can_idx) \
    static CANInstance name = {         \
        .parent = NULL,                 \
        .can_e = can_idx,               \
        .map = {0},                     \
    }

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册CAN实例
 * @param instance CAN实例指针（需先通过宏定义）
 * @param config   初始化配置结构体指针
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限、参数非法或重复注册）
 *
 * @note 注册时将 config 中的配置拷贝到 instance，然后配置硬件滤波器并启动CAN
 */
int8_t CANRegister(CANInstance *instance, const CAN_Init_Config_s *config);

/**
 * @brief 设置发送DLC长度（1~8）
 * @param instance CAN实例
 * @param length 数据长度（字节）
 */
void CANSetDLC(CANInstance *instance, uint8_t length);

/**
 * @brief 发送CAN消息（发送数据来自 instance->tx_buff）
 * @param instance CAN实例
 * @param timeout_ms 超时时间（毫秒）
 * @retval 1 发送成功
 * @retval 0 发送失败或超时
 */
uint8_t CANTransmit(CANInstance *instance, uint32_t timeout_ms);

#endif // BSP_CAN_MODULE_ENABLED

#endif /* __BSP_CAN_H */
