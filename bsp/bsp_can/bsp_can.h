/**
 * @file bsp_can.h
 * @brief CAN驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/滤波器容量/中断/DMA/FD模式等）由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp_cfg.h"

#ifdef BSP_CAN_MODULE_ENABLED

#include "main.h"
#include "stdint.h"

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
    uint32_t tx_id;                            // 发送标准ID
    CANFilterMode_e filter_mode;               // 过滤器模式（掩码/列表）
    uint8_t rx_id_count;                       // 列表模式下实际ID数量（1-4）
    uint32_t rx_id_list[4];                    // 接收ID列表（列表模式：精确ID；掩码模式：rx_id_list[0]为基准ID）
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

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义CAN实例（掩码模式）
 * @param name    实例名称
 * @param can_idx 板载CAN枚举（BoardCAN_e）
 * @param txid    发送标准ID
 * @param rxid    接收基准ID
 * @param mask    掩码值（用于范围匹配）
 * @param cb      接收回调函数（可为NULL）
 *
 * @example 匹配 0x201-0x208: rxid=0x200, mask=0x7F8
 */
#define CAN_INSTANCE_DEF_MASK(name, can_idx, txid, rxid, mask, cb) \
    static CANInstance name = {                                    \
        .parent = NULL,                                            \
        .can_e = can_idx,                                          \
        .map = {0},                                                \
        .tx_id = txid,                                             \
        .filter_mode = CAN_FILTER_MODE_MASK,                       \
        .rx_id_count = 1,                                          \
        .rx_id_list = {rxid, 0, 0, 0},                             \
        .rx_mask = mask,                                           \
        .rx_id_matched = 0,                                        \
        .tx_buff = {0},                                            \
        .rx_buff = {0},                                            \
        .rx_len = 0,                                               \
        .rx_callback = cb}

/**
 * @brief 静态定义CAN实例（列表模式）
 * @param name    实例名称
 * @param can_idx 板载CAN枚举（BoardCAN_e）
 * @param txid    发送标准ID
 * @param id0-id3 接收标准ID列表（最多4个，未使用的填0）
 * @param cb      接收回调函数（可为NULL）
 *
 * @note id0 用于 FIFO 分配（奇数→FIFO0，偶数→FIFO1）
 */
#define CAN_INSTANCE_DEF_LIST(name, can_idx, txid, id0, id1, id2, id3, cb) \
    static CANInstance name = {                                            \
        .parent = NULL,                                                    \
        .can_e = can_idx,                                                  \
        .map = {0},                                                        \
        .tx_id = txid,                                                     \
        .filter_mode = CAN_FILTER_MODE_LIST,                               \
        .rx_id_count = 4,                                                  \
        .rx_id_list = {id0, id1, id2, id3},                                \
        .rx_mask = 0,                                                      \
        .rx_id_matched = 0,                                                \
        .tx_buff = {0},                                                    \
        .rx_buff = {0},                                                    \
        .rx_len = 0,                                                       \
        .rx_callback = cb}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册CAN实例
 * @param instance CAN实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限、参数非法或重复注册）
 */
int8_t CANRegister(CANInstance *instance);

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
