/**
 * @file bsp_can.h
 * @brief CAN驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/滤波器容量/中断/DMA/FD模式等）由 CubeMX 负责，BSP 层只管理实例。
 *       需要覆盖 CubeMX 硬件参数时，由 hal_can 层在 bsp_map 的 can_cfg_map 中逐路配置。
 */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp_map.h"

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
 * @brief CAN帧ID类型枚举
 */
typedef enum : uint8_t
{
    CAN_FRAME_ID_STD = 0, // 标准帧 ID（11-bit）
    CAN_FRAME_ID_EXT = 1  // 扩展帧 ID（29-bit）
} CANFrameIdType_e;

/**
 * @brief CAN帧格式枚举（仅 FDCAN 支持 FD 帧）
 * @note  设计限制：FD/FD_BRS 帧的实际收发长度受外设 Rx/Tx 元素尺寸限制
 *        （bsp_map 的 can_cfg_map：rx_fifo0/1_elmt_size、tx_elmt_size，当前均为
 *         FDCAN_DATA_BYTES_8 = 经典 CAN）。若未相应加大元素尺寸，
 *         >8 字节的 FD 帧无法正确收发，CANConfig 会对超 TxElmtSize 的 tx_len 直接报错。
 */
typedef enum : uint8_t
{
    CAN_FRAME_FORMAT_CLASSIC = 0, // 经典 CAN（最大 8 字节）
    CAN_FRAME_FORMAT_FD = 1,      // FD 帧（无 BRS，最大 64 字节）
    CAN_FRAME_FORMAT_FD_BRS = 2   // FD 帧（带 BRS，最大 64 字节）
} CANFrameFormat_e;

/**
 * @brief CAN过滤器模式枚举
 */
typedef enum : uint8_t
{
    CAN_FILTER_MODE_MASK = 0, // 掩码模式（支持范围匹配）
    CAN_FILTER_MODE_LIST = 1  // 列表模式（最多4个精确ID）
} CANFilterMode_e;

/**
 * @brief CAN实例结构体
 */
typedef struct CANInstance
{
    void *parent;                                       // 父实例指针（由 DRV 层设置）
    BoardCAN_e can_e;                                   // 板载CAN枚举（Config时查找映射）
    CAN_Map_t map;                                      // CAN映射（Config时自动填充）
    uint32_t tx_id;                                     // 发送ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFrameIdType_e tx_id_type;                        // 发送ID类型（标准/扩展）
    CANFrameFormat_e tx_frame_format;                   // 发送帧格式（经典/FD/FD_BRS）
    CANFilterMode_e filter_mode;                        // 过滤器模式（掩码/列表）
    uint8_t rx_id_count;                                // 列表模式下有效接收ID数量（Config时自动计算）
    uint32_t rx_id_list[4];                             // 接收ID列表；CAN_ID_UNUSED(-1) 表示该槽位无效
    uint32_t rx_mask;                                   // 掩码模式：掩码值（列表模式不使用）
    CANFrameIdType_e rx_id_type;                        // 接收ID类型（标准/扩展）
    uint32_t rx_id_matched;                             // 实际匹配到的ID（回调中使用）
    uint8_t tx_buff[64];                                // 发送缓存（最大64字节，FD帧）
    uint8_t rx_buff[64];                                // 接收缓存（最大64字节，FD帧）
    uint8_t rx_len;                                     // 接收长度（字节）
    void (*rx_callback)(struct CANInstance *);          // 接收完成回调
    void (*tx_complete_callback)(struct CANInstance *); // 发送完成回调（一帧从硬件发出后调用；NULL 不启用）
#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    FDCAN_TxHeaderTypeDef tx_header; // FDCAN发送头
#else
    CAN_TxHeaderTypeDef tx_header; // CAN发送头
    uint32_t tx_mailbox;           // BxCAN发送邮箱索引
#endif
} CANInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief CAN 运行时配置结构体（仅用于 CANConfig）
 * @note 统一掩码模式和列表模式，通过 filter_mode 区分
 * @note rx_id_count 会根据 rx_id_list 自动计算，无需用户填写
 * @note 新增字段 tx_id_type/tx_frame_format/rx_id_type 默认值 0 = 标准帧/经典 CAN，
 *       现有 drv_motor 调用（designated initializer）零改动兼容
 */
typedef struct
{
    BoardCAN_e can_e;                                   // 板载CAN枚举（用于查找硬件映射）
    uint32_t tx_id;                                     // 发送ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFrameIdType_e tx_id_type;                        // 发送ID类型（标准/扩展），0=标准
    CANFrameFormat_e tx_frame_format;                   // 发送帧格式（经典/FD/FD_BRS），0=经典
    uint8_t tx_len;                                     // 发送数据长度（1~64，仅合法尺寸）；0 表示默认8
    CANFilterMode_e filter_mode;                        // 过滤器模式（掩码/列表）
    uint32_t rx_id_list[4];                             // 接收ID列表；CAN_ID_UNUSED(-1) 表示该槽位无效
    uint32_t rx_mask;                                   // 掩码模式：掩码值（列表模式不使用）
    CANFrameIdType_e rx_id_type;                        // 接收ID类型（标准/扩展），0=标准
    void (*rx_callback)(struct CANInstance *);          // 接收完成回调（可为NULL）
    void (*tx_complete_callback)(struct CANInstance *); // 发送完成回调（一帧发出后调用；可为NULL）
} CAN_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义CAN实例（仅身份绑定）
 * @param name    实例名称
 * @param can_idx 板载CAN枚举（BoardCAN_e）
 */
#define CAN_INSTANCE_DEF(name) static CANInstance name

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册CAN实例（仅调用一次）
 * @param instance CAN实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限、参数非法、重复注册）
 *
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不配置硬件参数（由 CANConfig 负责）。
 */
int8_t CANRegister(CANInstance *instance);

/**
 * @brief 配置CAN实例（可重复调用）
 * @param instance CAN实例指针
 * @param config   运行时配置结构体指针（can_e/tx_id/tx_id_type/tx_frame_format/tx_len/filter/rx/rx_id_type/callback）
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 *
 * @note 填充硬件映射，设置运行时参数并配置硬件滤波器/启动CAN。
 *       发送长度由 config->tx_len 指定（0 表示默认 8 字节）。
 *       tx_id_type/rx_id_type 选择标准(0)/扩展(1)帧；tx_frame_format 选择经典(0)/FD(1)/FD_BRS(2)。
 *       tx_len>8 必须配 FD/FD_BRS；BxCAN 不支持 FD 帧。
 *       不修改 static 管理数组。
 *       注意：每个实例仅应调用一次。过滤器索引（s_can*_filter_idx / s_fdcan*_filter_idx）
 *       只增不重置，重复调用会重复消耗过滤器 bank/索引，最终报 "filter index overflow" 错误。
 *       要求在 CANRegister 之后调用。
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config);

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
