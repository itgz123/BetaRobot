#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp_map.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "main.h"
#include "stdint.h"

#define CAN_TX_MAILBOX_FREE_BASE 50

typedef struct CANInstance CANInstance;
typedef enum : uint8_t
{
    CAN_STANDARD_DATA_FRAME = 0,   // 标准数据帧（11-bit ID）。Standard data frame (11-bit ID).
    CAN_EXTENDED_DATA_FRAME = 1,   // 扩展数据帧（29-bit ID）。Extended data frame (29-bit ID).
    CAN_STANDARD_REMOTE_FRAME = 2, // 标准远程帧。Standard remote frame.
    CAN_EXTENDED_REMOTE_FRAME = 3, // 扩展远程帧。Extended remote frame.
    CAN_ERROR_FRAME = 4,           // 错误帧（虚拟事件）。Error frame (virtual event).
} CAN_Frame_Type_e;

typedef enum : uint8_t
{
    CAN_FILTER_MODE_MASK = 0, // 掩码匹配：(id & start_id_mask) == end_id_mask。
    CAN_FILTER_MODE_LIST = 1, // 列表模式（最多4个精确ID）
    CAN_FILTER_MODE_RANGE = 2 // 区间匹配：start_id_mask <= id <= end_id_mask。
} CAN_Filter_Mode_e;

typedef enum : uint8_t
{
    CAN_FRAME_FORMAT_CLASSIC = 0, // 经典 CAN（最大 8 字节）
    CAN_FRAME_FORMAT_FD = 1,      // FD 帧（无 BRS，最大 64 字节）
    CAN_FRAME_FORMAT_FD_BRS = 2   // FD 帧（带 BRS，最大 64 字节）
} CAN_Mode_Type_e;

// 不管bxcan还是fdcan，都用这个
typedef struct
{
    uint32_t id;                 // CAN ID。CAN ID.
    CAN_Frame_Type_e frame_type; // 帧类型。Frame type.
    uint8_t len;                 // 数据长度（0~64）。Data length
    uint8_t data[64];            // 数据载荷。Data payload.
} CAN_Pack_s;

/**
 * @brief CAN软件过滤器（每个实例一个；硬过滤全通，收帧后按此匹配分发）
 */
typedef struct CAN_Filter_s
{
    CAN_Filter_Mode_e mode;                                                 // 过滤模式。Filter mode.
    uint32_t start_id_mask;                                                 // 起始 ID 或掩码（MASK:掩码 / RANGE:区间下限）。Start ID or mask.
    uint32_t end_id_mask;                                                   // 结束 ID 或匹配值（MASK:匹配值 / RANGE:区间上限）。End ID or match value.
    CAN_Frame_Type_e frame_type;                                            // 帧类型。Frame type.
    void (*callback)(struct CANInstance *instance, const CAN_Pack_s *pack); // 接收回调（帧匹配后调用；可为 NULL）。Callback function.
} CAN_Filter_s;

typedef struct
{
    BoardCAN_e can_e;     // 板载CAN枚举（用于查找硬件映射）
    CAN_Mode_Type_e mode; // 工作模式
    void *parent;         // 父实例指针（由 DRV 层设置）
    CAN_Filter_s filter;  // 软件过滤器（硬过滤全通后由软件匹配分发；回调可为 NULL）
} CAN_Config_s;

struct CANInstance
{
    BoardCAN_e can_e;     // 板载CAN枚举（Config时查找映射）
    CAN_Map_t map;        // CAN映射（Config时自动填充）
    CAN_Mode_Type_e mode; // 工作模式
    void *parent;         // 父实例指针（由 DRV 层设置）
    CAN_Filter_s filter;  // 软件过滤器（Config时写入）
    // void (*tx_complete_callback)(struct CANInstance *); // 发送完成回调（一帧从硬件发出后调用；NULL 不启用）

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    FDCAN_TxHeaderTypeDef tx_header; // FDCAN发送头
#else
    CAN_TxHeaderTypeDef tx_header; // CAN发送头
    uint32_t tx_mailbox;           // BxCAN发送邮箱索引
#endif
};

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义CAN实例（仅身份绑定）
 * @param name    实例名称
 * @param can_idx 板载CAN枚举（BoardCAN_e）
 */
#define CAN_INSTANCE_DEF(name) static CANInstance name

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
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config);

/**
 * @brief CANTransmit 成功返回基值
 * @note 发送成功时返回 基值 + 剩余空闲邮箱数（0/1/2/3 → 50/51/52/53）；
 *       失败返回 -1。
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack);

#endif // BSP_CAN_MODULE_ENABLED

#endif /* __BSP_CAN_H */
