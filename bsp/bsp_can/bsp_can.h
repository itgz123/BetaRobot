#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp_map.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "main.h"
#include "stdint.h"

#define CAN_TX_MAILBOX_FREE_BASE 50
#define CAN_TX_MAILBOX_NUM 3 // BxCAN 每个 CAN 的发送邮箱数量
#define CAN_ID_UNUSED ((uint32_t)0xFFFFFFFF)

/* 对于fdcan硬件和bxcan硬件的统一接口，以下CAN_Frame_Type_e和CAN_Mode_Type_e枚举的相关兼容性
 * bxcan:
 *     CANConfig配置CAN_Mode_Type_e时：
 *         只有CAN_Mode_Type_e==CAN_FRAME_FORMAT_CLASSIC才不报错
 *     发送的pack：
 *         CAN_Frame_Type_e的5种都支持
 *     接收过滤器：
 *         CAN_Frame_Type_e的5种都支持
 * fdcan:
 *     CANConfig配置CAN_Mode_Type_e时：
 *         只有一下3种情况允许
 *         1. (CANInstance.mode==CAN_FRAME_FORMAT_FD_BRS)&&(FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD_BRS)
 *         2. (CANInstance.mode==CAN_FRAME_FORMAT_FD)&&((FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD)||(FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD_BRS))
 *         3. CANInstance.mode==CAN_FRAME_FORMAT_CLASSIC
 *     发送的pack：
 *         CAN_FRAME_FORMAT_CLASSIC模式下CAN_Frame_Type_e的5种都支持
 *         CAN_FRAME_FORMAT_FD/FD_BRS下CAN_STANDARD_REMOTE_FRAME和CAN_EXTENDED_REMOTE_FRAME报错
 *     接收过滤器：
 *         CAN_FRAME_FORMAT_CLASSIC模式下CAN_Frame_Type_e的5种都支持
 *         CAN_FRAME_FORMAT_FD/FD_BRS下CAN_STANDARD_REMOTE_FRAME和CAN_EXTENDED_REMOTE_FRAME报错
 */

typedef enum : uint8_t
{
    CAN_STANDARD_DATA_FRAME = 0,   // 标准数据帧（11-bit ID）。Standard data frame (11-bit ID).
    CAN_EXTENDED_DATA_FRAME = 1,   // 扩展数据帧（29-bit ID）。Extended data frame (29-bit ID).
    CAN_STANDARD_REMOTE_FRAME = 2, // 标准远程帧。Standard remote frame.
    CAN_EXTENDED_REMOTE_FRAME = 3, // 扩展远程帧。Extended remote frame.
} CAN_Frame_Type_e;

typedef enum : uint8_t
{
    CAN_FILTER_MODE_MASK = 0, // 掩码匹配：(id & id0) == (id1 & id0) 命中。Mask: (id & id0) == (id1 & id0).
    CAN_FILTER_MODE_LIST = 1, // 精确ID列表：id0、id1 两个精确ID（id1=CAN_ID_UNUSED 未用，仅匹配 id0）。List: two exact IDs.
    CAN_FILTER_MODE_RANGE = 2 // 区间匹配：id0 <= id <= id1 命中。Range: id0 <= id <= id1.
} CAN_Filter_Mode_e;

typedef enum : uint8_t
{
    CAN_FRAME_FORMAT_CLASSIC = 0, // 经典 CAN（最大 8 字节）
    CAN_FRAME_FORMAT_FD = 1,      // FD 帧（无 BRS，最大 64 字节）
    CAN_FRAME_FORMAT_FD_BRS = 2   // FD 帧（带 BRS，最大 64 字节）
} CAN_Mode_Type_e;

typedef struct CANInstance CANInstance;

// 不管bxcan还是fdcan，都用这个
typedef struct
{
    uint32_t id;                 // CAN ID。CAN ID.
    CAN_Frame_Type_e frame_type; // 帧类型。Frame type.
    uint8_t len;                 // 数据长度（0~64）。Data length
    uint8_t data[64];            // 数据载荷。Data payload.
} CAN_Pack_s;

/**
 * @brief CAN软件过滤器（每个实例可配置多个，存放在 filters[] 数组，数量为 filter_num；硬过滤全通，收帧后按此匹配分发）
 * @note id0/id1 按模式取值：
 *       - MASK : id0 = 掩码，id1 = 匹配值；(id & id0) == (id1 & id0) 命中
 *       - LIST : id0、id1 = 两个精确ID（id1 = CAN_ID_UNUSED 表示未用，仅匹配 id0）
 *       - RANGE: id0 = 区间下限，id1 = 区间上限；id0 <= id <= id1 命中
 */
typedef struct CAN_Filter_s
{
    CAN_Filter_Mode_e mode;                                                 // 过滤模式。Filter mode.
    uint32_t id0;                                                           // MASK:掩码 / LIST:精确ID1 / RANGE:区间下限。Mask / exact ID1 / range lower bound.
    uint32_t id1;                                                           // MASK:匹配值 / LIST:精确ID2 / RANGE:区间上限。Match value / exact ID2 / range upper bound.
    CAN_Frame_Type_e frame_type;                                            // 帧类型过滤（仅接收该类型帧）。Frame type filter.
    void (*callback)(struct CANInstance *instance, const CAN_Pack_s *pack); // 接收回调（匹配后调用；NULL 不接收）。Callback.
} CAN_Filter_s;

typedef struct
{
    BoardCAN_e can_e;                                                                // 板载CAN枚举（用于查找硬件映射）
    CAN_Mode_Type_e mode;                                                            // 工作模式
    void *parent;                                                                    // 父实例指针（由 DRV 层设置）
    CAN_Filter_s *filters;                                                           // 软件过滤器数组（硬过滤全通后由软件匹配分发；可为 NULL）
    uint8_t filter_num;                                                              // 过滤器数量
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox); // 发送完成回调（一帧从硬件发出后调用；NULL 不启用）
} CAN_Config_s;

struct CANInstance
{
    BoardCAN_e can_e;                                                                // 板载CAN枚举（Config时查找映射）
    CAN_Map_t map;                                                                   // CAN映射（Config时自动填充）
    CAN_Mode_Type_e mode;                                                            // 工作模式
    void *parent;                                                                    // 父实例指针（由 DRV 层设置）
    CAN_Filter_s *filters;                                                           // 软件过滤器数组（Config时写入，指向 config 中的数组）
    uint8_t filter_num;                                                              // 过滤器数量（Config时写入）
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox); // 发送完成回调（一帧从硬件发出后调用；NULL 不启用）
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
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param tx_mailbox    出参：本次发送的发送标记（BxCAN=邮箱索引 0~2 / FDCAN=MessageMarker 0~31，
 *                      发送完成回调据此与发送帧对应）；可为 NULL
 * @param tx_free_level 出参：发送后剩余可发送数（BxCAN=空闲邮箱数 0~CAN_TX_MAILBOX_NUM /
 *                      FDCAN=Tx FIFO 空闲元素数）；可为 NULL
 * @retval 0  发送成功
 * @retval -1 失败（参数非法 / 长度超限 / 帧类型非法 / 发送资源全满 / 加入发送资源失败）
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint8_t *tx_mailbox, uint8_t *tx_free_level);

#endif // BSP_CAN_MODULE_ENABLED

#endif /* __BSP_CAN_H */
