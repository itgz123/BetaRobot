/**
 * @file bsp_can.h
 * @brief CAN驱动封装，提供实例管理和回调分发功能
 *
 * @note 本文件为 LibXR CAN 抽象（src/driver/can.hpp + driver/st/stm32_can{,fd}.cpp）的
 *       C 语言完整移植：多订阅者（ID_MASK / ID_RANGE 过滤）、异步发送队列、位时序配置、
 *       错误虚拟帧上报、错误状态查询等能力全部收敛到 CANInstance。
 *       硬件配置（波特率/过滤器容量/中断/DMA/FD模式等）由 CubeMX 负责，BSP 层只管理实例。
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

/**
 * @brief 异步发送队列容量（每个实例独立，classic 与 FD 各一份）
 * @note 须为 2 的幂（内部环形缓冲用掩码），可覆盖
 */
#ifndef CAN_TX_QUEUE_SIZE
#define CAN_TX_QUEUE_SIZE 8
#endif

/**
 * @brief 订阅者槽位数量（每个实例的静态订阅池，xrobot LockFreeList 的 C 化静态分配）
 */
#ifndef CAN_SUBSCRIBER_NUM
#define CAN_SUBSCRIBER_NUM 8
#endif

/**
 * @brief 错误虚拟 ID 前缀（与真实总线 ID 不重叠，见 CANErrorID_e）
 */
#define CAN_ERROR_ID_PREFIX 0xFFFF0000u

/*------------- 类型定义 --------------*/

/**
 * @brief CAN帧类型枚举（位标志，按位或组合）
 * @note  四个维度位可自由组合，常用组合：
 *        标准数据帧 = CAN_FRAME_STANDARD | CAN_FRAME_DATA
 *        扩展数据帧 = CAN_FRAME_EXTENDED | CAN_FRAME_DATA
 *        标准远程帧 = CAN_FRAME_STANDARD | CAN_FRAME_REMOTE
 *        扩展远程帧 = CAN_FRAME_EXTENDED | CAN_FRAME_REMOTE
 *        错误帧 = CAN_FRAME_ERROR（虚拟事件，不对应实际总线帧）
 *        未显式设置的维度默认标准帧 + 数据帧（CANConfig 归一化）。
 */
typedef enum : uint8_t
{
    CAN_FRAME_STANDARD = 0x01, // 标准帧 ID（11-bit）
    CAN_FRAME_EXTENDED = 0x02, // 扩展帧 ID（29-bit）
    CAN_FRAME_DATA = 0x04,     // 数据帧
    CAN_FRAME_REMOTE = 0x08,   // 远程帧（RTR，无数据载荷，DLC 表示请求长度）
    CAN_FRAME_ERROR = 0x10,    // 错误帧（虚拟事件，xrobot Type::ERROR，ID 为 CANErrorID_e）
} CANFrameType_e;

/*------------- 帧类型工具函数 --------------*/

/**
 * @brief 帧类型归一化：未设置的维度补默认（ID 默认标准帧，帧类型默认数据帧）
 * @note  错误帧为独立虚拟事件，不参与归一化（原样返回）。
 *        现有调用方（designated initializer 不设置该字段，值 0）零改动兼容为标准数据帧。
 */
static inline CANFrameType_e CANFrameTypeNormalize(CANFrameType_e t)
{
    if (((uint8_t)t & CAN_FRAME_ERROR) != 0U)
    {
        return t; /* 错误帧不归一化 */
    }
    if (((uint8_t)t & (CAN_FRAME_STANDARD | CAN_FRAME_EXTENDED)) == 0U)
    {
        t = (CANFrameType_e)((uint8_t)t | CAN_FRAME_STANDARD);
    }
    if (((uint8_t)t & (CAN_FRAME_DATA | CAN_FRAME_REMOTE)) == 0U)
    {
        t = (CANFrameType_e)((uint8_t)t | CAN_FRAME_DATA);
    }
    return t;
}

/**
 * @brief 帧类型合法性校验（应在归一化后调用）
 * @retval 1 合法；0 非法（ID 维度/帧类型维度恰好各一位，且无多余位；错误帧须单独出现）
 */
static inline uint8_t CANFrameTypeIsValid(CANFrameType_e t)
{
    uint8_t v = (uint8_t)t;
    if ((v & ~(CAN_FRAME_STANDARD | CAN_FRAME_EXTENDED | CAN_FRAME_DATA | CAN_FRAME_REMOTE | CAN_FRAME_ERROR)) != 0U)
    {
        return 0;
    }
    if ((v & CAN_FRAME_ERROR) != 0U)
    {
        /* 错误帧为虚拟事件帧，不与其它维度组合 */
        return (v == CAN_FRAME_ERROR) ? 1U : 0U;
    }
    uint8_t id_bits = v & (CAN_FRAME_STANDARD | CAN_FRAME_EXTENDED);
    uint8_t frame_bits = v & (CAN_FRAME_DATA | CAN_FRAME_REMOTE);
    return (id_bits == CAN_FRAME_STANDARD || id_bits == CAN_FRAME_EXTENDED) &&
           (frame_bits == CAN_FRAME_DATA || frame_bits == CAN_FRAME_REMOTE);
}

/**
 * @brief 错误虚拟帧 ID 枚举（xrobot CAN::ErrorID 移植）
 * @note  ClassicPack::type == CAN_FRAME_ERROR 时，pack.id 落入本空间。
 *        CAN FD 协议不支持远程帧；错误事件统一走经典帧通道分发。
 */
typedef enum : uint32_t
{
    CAN_ERROR_ID_GENERIC = CAN_ERROR_ID_PREFIX,           // 通用错误
    CAN_ERROR_ID_BUS_OFF = CAN_ERROR_ID_PREFIX + 1,       // BUS-OFF
    CAN_ERROR_ID_ERROR_PASSIVE = CAN_ERROR_ID_PREFIX + 2, // Error Passive
    CAN_ERROR_ID_ERROR_WARNING = CAN_ERROR_ID_PREFIX + 3, // Error Warning
    CAN_ERROR_ID_PROTOCOL = CAN_ERROR_ID_PREFIX + 4,      // 协议错误
    CAN_ERROR_ID_ACK = CAN_ERROR_ID_PREFIX + 5,           // ACK 错误
    CAN_ERROR_ID_STUFF = CAN_ERROR_ID_PREFIX + 6,         // 位填充错误
    CAN_ERROR_ID_FORM = CAN_ERROR_ID_PREFIX + 7,          // 格式错误
    CAN_ERROR_ID_BIT0 = CAN_ERROR_ID_PREFIX + 8,          // 发送位错误（显性）
    CAN_ERROR_ID_BIT1 = CAN_ERROR_ID_PREFIX + 9,          // 发送位错误（隐性）
    CAN_ERROR_ID_CRC = CAN_ERROR_ID_PREFIX + 10,          // CRC 错误
    CAN_ERROR_ID_OTHER = CAN_ERROR_ID_PREFIX + 11,        // 其它
} CANErrorID_e;

/** 将错误枚举转为可写入 pack.id 的虚拟 ID */
static inline uint32_t CANFromErrorID(CANErrorID_e e)
{
    return (uint32_t)e;
}

/** 判断 id 是否处于错误虚拟 ID 空间 */
static inline uint8_t CANIsErrorId(uint32_t id)
{
    return ((id & 0xFFFF0000u) == CAN_ERROR_ID_PREFIX) ? 1U : 0U;
}

/** 将 id 解释为错误枚举 */
static inline CANErrorID_e CANToErrorID(uint32_t id)
{
    return (CANErrorID_e)id;
}

/**
 * @brief 经典 CAN 帧结构（xrobot CAN::ClassicPack 移植）
 */
#pragma pack(push, 1)
typedef struct
{
    uint32_t id;         // CAN ID（11/29 bit 或错误虚拟 ID）
    CANFrameType_e type; // 帧类型（位组合，含 CAN_FRAME_ERROR）
    uint8_t dlc;         // 数据长度（0~8）；远程帧为请求的数据长度，data 无效
    uint8_t data[8];     // 数据载荷
} CAN_ClassicPack_s;
#pragma pack(pop)

/**
 * @brief CAN FD 帧结构（xrobot FDCAN::FDPack 移植）
 * @note FD 帧 type 仅使用 STANDARD|DATA / EXTENDED|DATA
 */
#pragma pack(push, 1)
typedef struct
{
    uint32_t id;         // CAN ID
    CANFrameType_e type; // 帧类型（STANDARD|DATA / EXTENDED|DATA）
    uint8_t len;         // 数据长度（0~64）
    uint8_t data[64];    // 数据载荷
} CAN_FDPack_s;
#pragma pack(pop)

/**
 * @brief CAN帧格式枚举（仅 FDCAN 支持 FD 帧）
 * @note  设计限制：FD/FD_BRS 帧的实际收发长度受外设 Rx/Tx 元素尺寸限制
 *        （bsp_map 的 can_cfg_map：rx_fifo0/1_elmt_size、tx_elmt_size，当前均为
 *         FDCAN_DATA_BYTES_8 = 经典 CAN）。若未相应加大元素尺寸，
 *        >8 字节的 FD 帧无法正确收发，CANConfig 会对超 TxElmtSize 的 tx_len 直接报错。
 */
typedef enum : uint8_t
{
    CAN_FRAME_FORMAT_CLASSIC = 0, // 经典 CAN（最大 8 字节）
    CAN_FRAME_FORMAT_FD = 1,      // FD 帧（无 BRS，最大 64 字节）
    CAN_FRAME_FORMAT_FD_BRS = 2   // FD 帧（带 BRS，最大 64 字节）
} CANFrameFormat_e;

/**
 * @brief CAN过滤器模式枚举（硬件过滤器：掩码/列表）
 */
typedef enum : uint8_t
{
    CAN_FILTER_MODE_MASK = 0, // 掩码模式（支持范围匹配）
    CAN_FILTER_MODE_LIST = 1  // 列表模式（最多4个精确ID）
} CANFilterMode_e;

/**
 * @brief 位时序配置（xrobot CAN::BitTiming 移植，仲裁相位）
 * @note brp/prop_seg/phase_seg1/phase_seg2/sjw 为 0 表示"保持原值"（部分更新）
 */
typedef struct
{
    uint32_t brp;        // 预分频（实际值，非寄存器值）
    uint32_t prop_seg;   // 传播段
    uint32_t phase_seg1; // 相位段 1
    uint32_t phase_seg2; // 相位段 2
    uint32_t sjw;        // 同步跳宽
} CAN_BitTiming_s;

/**
 * @brief CAN 工作模式（xrobot CAN::Mode 移植）
 */
typedef struct
{
    uint8_t loopback;        // 内部回环（不经物理总线）
    uint8_t listen_only;     // 只听（静默）模式
    uint8_t triple_sampling; // 三采样（FDCAN 忽略）
    uint8_t one_shot;        // 单次发送（失败不自动重发）
} CAN_Mode_s;

/**
 * @brief FDCAN FD 模式配置（xrobot FDCAN::FDMode 移植）
 * @note brs/esi 当前在每帧 TxHeader 中由驱动决定，fd_mode 保留给上层语义
 */
typedef struct
{
    uint8_t fd_enabled; // 启用 CAN FD
    uint8_t brs;        // 启用 Bit Rate Switch
    uint8_t esi;        // ESI 标志
} CAN_FDMode_s;

/**
 * @brief CAN 当前错误状态快照（xrobot CAN::ErrorState 移植）
 */
typedef struct
{
    uint8_t tx_error_counter; // 发送错误计数 TEC
    uint8_t rx_error_counter; // 接收错误计数 REC
    uint8_t bus_off;          // BUS-OFF
    uint8_t error_passive;    // Error Passive
    uint8_t error_warning;    // Error Warning
} CAN_ErrorState_s;

/**
 * @brief 订阅过滤器匹配模式（软件过滤，xrobot CAN::FilterMode 移植）
 * @note 区别于 CANFilterMode_e（硬件过滤器模式）
 */
typedef enum : uint8_t
{
    CAN_FILTER_MATCH_MASK = 0, // 掩码匹配：(id & start_id_mask) == end_id_mask
    CAN_FILTER_MATCH_RANGE = 1 // 区间匹配：start_id_mask <= id <= end_id_mask
} CANFilterMatchMode_e;

/* 前向声明（订阅者回调引用实例） */
struct CANInstance;

/**
 * @brief 经典帧订阅者回调类型（xrobot CAN::Callback 移植）
 * @param inst   触发回调的 CAN 实例
 * @param pack   接收到的经典帧（只读；回调返回后失效，不可保存指针异步使用）
 * @param in_isr 是否在中断上下文中调用（1=ISR）
 */
typedef void (*CANSubscriberCb_t)(struct CANInstance *inst, const CAN_ClassicPack_s *pack, uint8_t in_isr);

/**
 * @brief FD 帧订阅者回调类型（xrobot FDCAN::CallbackFD 移植）
 */
typedef void (*CANSubscriberFDCb_t)(struct CANInstance *inst, const CAN_FDPack_s *pack, uint8_t in_isr);

/**
 * @brief 经典帧订阅过滤器节点（xrobot CAN::Filter 移植，静态池分配）
 */
typedef struct CANSubscriber
{
    struct CANSubscriber *next; // 链表下一节点（NULL=尾）
    uint8_t in_use;             // 槽位占用标记
    CANFilterMatchMode_e mode;  // 过滤模式
    uint32_t start_id_mask;     // 掩码模式：掩码；区间模式：起始 ID
    uint32_t end_id_mask;       // 掩码模式：匹配值；区间模式：结束 ID
    CANFrameType_e type;        // 订阅帧类型（位组合，含 CAN_FRAME_ERROR）
    CANSubscriberCb_t cb;       // 回调函数
} CANSubscriber_s;

/**
 * @brief FD 帧订阅过滤器节点（xrobot FDCAN::Filter 移植）
 */
typedef struct CANSubscriberFD
{
    struct CANSubscriberFD *next;
    uint8_t in_use;
    CANFilterMatchMode_e mode;
    uint32_t start_id_mask;
    uint32_t end_id_mask;
    CANFrameType_e type; // 订阅帧类型（仅 STANDARD|DATA / EXTENDED|DATA）
    CANSubscriberFDCb_t cb;
} CANSubscriberFD_s;

/**
 * @brief CAN实例结构体
 * @note 一个实例 = 一个 CAN 外设驱动（xrobot 语义）：
 *       - 多订阅者（经典 + FD 各一份静态池，软件过滤分发）
 *       - 异步发送队列（经典 + FD 双环形缓冲，TxService 轮询填硬件）
 *       - 错误虚拟帧上报（订阅 CAN_FRAME_ERROR 的订阅者接收）
 *       同时保留旧式单接收 ID 兼容字段（rx_id_list/rx_mask/rx_frame_type/rx_callback）。
 */
typedef struct CANInstance
{
    void *parent;                                       // 父实例指针（由 DRV 层设置）
    BoardCAN_e can_e;                                   // 板载CAN枚举（Config时查找映射）
    CAN_Map_t map;                                      // CAN映射（Config时自动填充）
    uint32_t tx_id;                                     // 默认发送ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFrameType_e tx_frame_type;                       // 默认发送帧类型（位组合：ID类型|数据/远程）
    CANFrameFormat_e tx_frame_format;                   // 默认发送帧格式（经典/FD/FD_BRS）
    CANFilterMode_e filter_mode;                        // 硬件过滤器模式（掩码/列表，旧式兼容）
    uint8_t rx_id_count;                                // 列表模式下有效接收ID数量（Config时自动计算）
    uint32_t rx_id_list[4];                             // 接收ID列表（旧式兼容）；CAN_ID_UNUSED(-1) 表示无效
    uint32_t rx_mask;                                   // 掩码模式：掩码值（旧式兼容）
    CANFrameType_e rx_frame_type;                       // 默认接收订阅帧类型（旧式兼容）
    uint32_t rx_id_matched;                             // 实际匹配到的ID（回调中使用）
    CANFrameType_e rx_frame_type_matched;               // 实际接收到的帧类型（位组合，分发时填充）
    uint8_t tx_buff[64];                                // 发送缓存（默认发送 CANTransmit 用）
    uint8_t rx_buff[64];                                // 接收缓存（旧式 rx_callback 用）
    uint8_t rx_len;                                     // 接收长度（字节）
    void (*rx_callback)(struct CANInstance *);          // 旧式兼容：接收完成回调
    void (*tx_complete_callback)(struct CANInstance *); // 发送完成回调（一帧从硬件发出后调用；NULL 不启用）

    /* ---- xrobot 移植：多订阅者（经典 + FD） ---- */
    CANSubscriber_s subscriber[CAN_SUBSCRIBER_NUM];      // 经典订阅槽位池
    CANSubscriber_s *sub_head;                           // 经典订阅链表头
    uint8_t sub_count;                                   // 经典已用订阅槽位
    CANSubscriberFD_s subscriber_fd[CAN_SUBSCRIBER_NUM]; // FD 订阅槽位池
    CANSubscriberFD_s *sub_fd_head;                      // FD 订阅链表头
    uint8_t sub_fd_count;                                // FD 已用订阅槽位

    /* ---- xrobot 移植：异步发送队列（环形缓冲，2 的幂） ---- */
    CAN_ClassicPack_s tx_queue[CAN_TX_QUEUE_SIZE]; // 经典发送队列
    CAN_FDPack_s tx_queue_fd[CAN_TX_QUEUE_SIZE];   // FD 发送队列
    volatile uint8_t tx_q_r;                       // 经典读指针
    volatile uint8_t tx_q_w;                       // 经典写指针
    volatile uint8_t tx_fd_q_r;                    // FD 读指针
    volatile uint8_t tx_fd_q_w;                    // FD 写指针
    volatile uint8_t tx_lock;                      // TxService 服务锁（1=被占用）
    volatile uint8_t tx_pend;                      // 有待处理发送请求标记（xrobot tx_pend_）

    /* ---- 接收/工作状态 ---- */
    CAN_ClassicPack_s rx_pack; // 最近接收的经典帧（分发时填充）
    CAN_FDPack_s rx_pack_fd;   // 最近接收的 FD 帧（分发时填充）
    CAN_Mode_s mode;           // 当前工作模式
    CAN_FDMode_s fd_mode;      // 当前 FD 模式
    uint32_t fifo;             // 本实例接收 FIFO（Init 时确定）

#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    FDCAN_TxHeaderTypeDef tx_header; // FDCAN发送头
#else
    CAN_TxHeaderTypeDef tx_header; // CAN发送头
    uint32_t tx_mailbox;           // BxCAN发送邮箱索引
#endif
} CANInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief CAN 运行时配置结构体（仅用于 CANConfig / CANSetConfig）
 * @note 统一掩码模式和列表模式，通过 filter_mode 区分
 * @note rx_id_count 会根据 rx_id_list 自动计算，无需用户填写
 * @note tx_frame_type/rx_frame_type 为位组合（见 CANFrameType_e），0 = 标准数据帧（归一化）
 * @note bitrate/sample_point/bit_timing/mode 为 xrobot SetConfig 移植：
 *       由 CANSetConfig 下发（停外设→写位时序/模式→重启）；CANConfig 初始化时忽略。
 *       位时序字段全 0 表示保持原值（部分更新）。
 */
typedef struct
{
    BoardCAN_e can_e;                                   // 板载CAN枚举（用于查找硬件映射）
    uint32_t tx_id;                                     // 发送ID；CAN_ID_UNUSED(-1) 表示不发送
    CANFrameType_e tx_frame_type;                       // 发送帧类型（位组合：ID类型|数据/远程），0=标准数据帧
    CANFrameFormat_e tx_frame_format;                   // 发送帧格式（经典/FD/FD_BRS），0=经典
    uint8_t tx_len;                                     // 发送数据长度（1~64，仅合法尺寸）；0 表示默认8
    CANFilterMode_e filter_mode;                        // 过滤器模式（掩码/列表）
    uint32_t rx_id_list[4];                             // 接收ID列表；CAN_ID_UNUSED(-1) 表示该槽位无效
    uint32_t rx_mask;                                   // 掩码模式：掩码值（列表模式不使用）
    CANFrameType_e rx_frame_type;                       // 接收订阅帧类型（位组合：ID类型|数据/远程），0=标准数据帧
    void (*rx_callback)(struct CANInstance *);          // 接收完成回调（可为NULL）
    void (*tx_complete_callback)(struct CANInstance *); // 发送完成回调（一帧发出后调用；可为NULL）

    /* xrobot SetConfig 字段（CANSetConfig 使用） */
    uint32_t bitrate;           // 仲裁相位目标波特率（Hz，宏观参数）
    float sample_point;         // 仲裁相位采样点（0~1，宏观参数）
    CAN_BitTiming_s bit_timing; // 仲裁相位位时序（寄存器级，全 0=保持原值）
    CAN_Mode_s mode;            // 工作模式（loopback/listen_only/triple_sampling/one_shot）
} CAN_Config_s;

/**
 * @brief FDCAN 全配置结构体（xrobot FDCAN::Configuration 移植，仅 CANSetFDConfig 使用）
 * @note 仲裁相位字段与 CAN_Config_s 一致；data_* 为数据相位（CAN FD）
 */
typedef struct
{
    /* 仲裁相位（继承 CAN::Configuration） */
    uint32_t bitrate;           // 仲裁相位目标波特率
    float sample_point;         // 仲裁相位采样点（0~1）
    CAN_BitTiming_s bit_timing; // 仲裁相位位时序
    CAN_Mode_s mode;            // 工作模式
    /* 数据相位（CAN FD） */
    uint32_t data_bitrate;       // 数据相位目标波特率
    float data_sample_point;     // 数据相位采样点（0~1）
    CAN_BitTiming_s data_timing; // 数据相位位时序
    CAN_FDMode_s fd_mode;        // FD 模式
} CAN_FDConfig_s;

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
 * @param config   运行时配置结构体指针（can_e/tx_id/tx_frame_type/tx_frame_format/tx_len/filter/rx/rx_frame_type/callback）
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 *
 * @note 填充硬件映射，设置运行时参数并配置硬件滤波器/启动CAN。
 *       发送长度由 config->tx_len 指定（0 表示默认 8 字节）。
 *       tx_frame_type/rx_frame_type 为位组合（标准|数据 等，见 CANFrameType_e）；tx_frame_format 选择经典(0)/FD(1)/FD_BRS(2)。
 *       tx_len>8 必须配 FD/FD_BRS；BxCAN 不支持 FD 帧。
 *       不修改 static 管理数组。
 *       注意：每个实例仅应调用一次。过滤器索引（s_can*_filter_idx / s_fdcan*_filter_idx）
 *       只增不重置，重复调用会重复消耗过滤器 bank/索引，最终报 "filter index overflow" 错误。
 *       要求在 CANRegister 之后调用。
 *       位时序/模式不在本函数下发，如需运行时改时序请调用 CANSetConfig / CANSetFDConfig。
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

/*------------- xrobot 移植接口 --------------*/

/**
 * @brief 设置经典 CAN 位时序与工作模式（xrobot CAN::SetConfig 移植）
 * @param instance CAN实例（须已 CANConfig，外设已启动）
 * @param config   配置（仅取 bit_timing / mode / bitrate / sample_point）
 * @retval 0 成功
 * @retval -1 失败（参数非法或硬件操作失败）
 *
 * @note 实现为：停外设 → 校验并写位时序/模式寄存器（0 表示保持原值）→ 重启并恢复中断通知。
 *       会短暂打断该外设上所有实例的通信（多实例共享外设时慎用）。
 */
int8_t CANSetConfig(CANInstance *instance, const CAN_Config_s *config);

/**
 * @brief 设置 FDCAN 完整配置（仲裁相位 + 数据相位 + FD 模式，xrobot FDCAN::SetConfig 移植）
 * @param instance CAN实例（须已 CANConfig，外设已启动）
 * @param config   FD 全配置
 * @retval 0 成功
 * @retval -1 失败（BxCAN 平台或参数非法）
 */
int8_t CANSetFDConfig(CANInstance *instance, const CAN_FDConfig_s *config);

/**
 * @brief 注册经典帧订阅者（多订阅者，xrobot CAN::Register 移植）
 * @param instance      CAN实例
 * @param type          订阅帧类型（位组合，含 CAN_FRAME_ERROR）
 * @param mode          过滤模式（CAN_FILTER_MATCH_MASK / CAN_FILTER_MATCH_RANGE）
 * @param start_id_mask 掩码模式的掩码 / 区间模式的起始 ID
 * @param end_id_mask   掩码模式的匹配值 / 区间模式的结束 ID
 * @param cb            回调（接收帧匹配时调用；可为 NULL 则仅匹配不回调）
 * @retval 0 成功
 * @retval -1 失败（参数非法 / 订阅槽位耗尽 / 帧类型非法）
 * @note 同一实例可注册多个订阅者，所有匹配的订阅者回调都会被调用。
 *       默认全放行区间：mode=RANGE, start=0, end=UINT32_MAX。
 */
int8_t CANSubscribe(CANInstance *instance, CANFrameType_e type, CANFilterMatchMode_e mode,
                    uint32_t start_id_mask, uint32_t end_id_mask, CANSubscriberCb_t cb);

/**
 * @brief 注销经典帧订阅者（按回调函数匹配，xrobot 无对应——C 静态池需要释放）
 */
int8_t CANUnsubscribe(CANInstance *instance, CANSubscriberCb_t cb);

/**
 * @brief 注册 FD 帧订阅者（xrobot FDCAN::Register 移植）
 * @note 仅支持 type 为 STANDARD|DATA / EXTENDED|DATA；FD 协议无远程帧
 */
int8_t CANSubscribeFD(CANInstance *instance, CANFrameType_e type, CANFilterMatchMode_e mode,
                      uint32_t start_id_mask, uint32_t end_id_mask, CANSubscriberFDCb_t cb);

/**
 * @brief 注销 FD 帧订阅者
 */
int8_t CANUnsubscribeFD(CANInstance *instance, CANSubscriberFDCb_t cb);

/**
 * @brief 异步发送经典 CAN 帧（xrobot CAN::AddMessage 移植）
 * @param instance CAN实例
 * @param pack     待发送帧（type 不得为 CAN_FRAME_ERROR）
 * @retval 0 成功（已入队，由 TxService 异步发出）
 * @retval -1 失败（参数非法 / 发送队列满）
 * @note 经内部发送队列异步发出；调用后 pack 即可复用。
 */
int8_t CANAddMessage(CANInstance *instance, const CAN_ClassicPack_s *pack);

/**
 * @brief 异步发送 CAN FD 帧（xrobot FDCAN::AddMessage(FDPack) 移植）
 * @param instance CAN实例（FDCAN 平台）
 * @param pack     待发送帧（type 须为 STANDARD|DATA / EXTENDED|DATA，len<=64）
 * @retval 0 成功（已入队）
 * @retval -1 失败（参数非法 / BxCAN 平台 / 发送队列满）
 */
int8_t CANAddMessageFD(CANInstance *instance, const CAN_FDPack_s *pack);

/**
 * @brief 查询当前错误状态（xrobot CAN::GetErrorState 移植）
 * @param instance CAN实例
 * @param state    输出：错误状态快照
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 */
int8_t CANGetErrorState(CANInstance *instance, CAN_ErrorState_s *state);

/**
 * @brief 获取 CAN 外设输入时钟频率（Hz，xrobot CAN::GetClockFreq 移植）
 * @note 可用于上层位时序计算
 */
uint32_t CANGetClockFreq(CANInstance *instance);

#endif // BSP_CAN_MODULE_ENABLED

#endif /* __BSP_CAN_H */
