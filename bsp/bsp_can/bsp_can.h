#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "bsp_map.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_common.h"
#include "main.h"
#include "stdint.h"

#define CAN_TX_MAILBOX_NUM 3 // BxCAN 每个 CAN 的发送邮箱数量（FDCAN 不用它，见 bsp_fdcan.c 的 FDCAN_TX_MARKER_NUM）
#define CAN_ID_UNUSED ((uint32_t)0xFFFFFFFF)

/* CANRecover 里"等取消请求落地"的上限 (us)。
 * 取消在途帧（ABRQ / TXBCR）是**硬件异步执行**的：写下去立刻回读占用量，可能还是旧值，
 * 于是把一次正常的恢复误判成"邮箱/FIFO 仍满"。任务上下文里有界轮询一下就够了。 */
#ifndef CAN_RECOVER_DRAIN_US
#define CAN_RECOVER_DRAIN_US 1000u
#endif

/* 对于fdcan硬件和bxcan硬件的统一接口，以下CAN_Frame_Type_e和CAN_Mode_Type_e枚举的相关兼容性
 * bxcan:
 *     CANConfig配置CAN_Mode_Type_e时：
 *         只有CAN_Mode_Type_e==CAN_FRAME_FORMAT_CLASSIC才不报错
 *     发送的pack：
 *         CAN_Frame_Type_e的4种都支持
 *     接收过滤器：
 *         CAN_Frame_Type_e的4种都支持
 * fdcan:
 *     CANConfig配置CAN_Mode_Type_e时：
 *         只有一下3种情况允许
 *         1. (CANInstance.mode==CAN_FRAME_FORMAT_FD_BRS)&&(FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD_BRS)
 *         2. (CANInstance.mode==CAN_FRAME_FORMAT_FD)&&((FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD)||(FDCAN_HandleTypeDef.Init.FrameFormat==CAN_FRAME_FORMAT_FD_BRS))
 *         3. CANInstance.mode==CAN_FRAME_FORMAT_CLASSIC
 *     发送的pack：
 *         CAN_FRAME_FORMAT_CLASSIC模式下CAN_Frame_Type_e的4种都支持
 *         CAN_FRAME_FORMAT_FD/FD_BRS下CAN_STANDARD_REMOTE_FRAME和CAN_EXTENDED_REMOTE_FRAME报错
 *     接收过滤器：
 *         CAN_FRAME_FORMAT_CLASSIC模式下CAN_Frame_Type_e的4种都支持
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

/*------------- 错误回调 --------------*/

/**
 * @brief err_callback 的触发原因（与 CANInstance.err_callback 配套）
 * @note 只上报「总线/硬件级」事件；**发送资源类失败**（邮箱/FIFO 满、等待超时、加入失败）
 *       不走 err_callback，由 CANTransmit 的返回值（BSP_BUSY / BSP_TIMEOUT / BSP_HW_ERR）
 *       和 tx_complete_callback 的 result 表达，避免同一件事两个上报通道。
 *       与 USART/BSP 的差异：CAN 的发送失败是"逐帧"的（有明确 owner），
 *       所以它能直达发起者；串口的发送失败只能靠返回值。
 */
typedef enum : uint8_t
{
    CAN_ERR_BUS_OFF = 0,       //!< 进入 bus-off（ISR）——纠正动作已执行（见各 .c 的恢复说明），上层应停发并等恢复
    CAN_ERR_ERROR_PASSIVE = 1, //!< 进入 error passive（ISR）——尚未停摆，但错误计数已过半，值得上层降速/告警
    CAN_ERR_ERROR_WARNING = 2, //!< 进入 error warning（ISR）——最轻的一级，仅告警
    CAN_ERR_PROTOCOL = 3,      //!< 协议错误（bxCAN: LEC 位集 STF/FOR/ACK/BR/BD/CRC）或发送失败（ALST/TERR）
                               //!< 可能高频触发（总线断开时每帧一次），handler 必须廉价且幂等。
                               //!< FDCAN 侧无 LEC 中断源，不单独上报此原因，靠上面三级表达。
    CAN_ERR_RX_OVERFLOW = 4,   //!< 接收溢出丢帧（bxCAN: FOV0/FOV1；FDCAN: Rx FIFO MESSAGE_LOST）
    CAN_ERR_HW = 5,            //!< 外设级硬件错误：Message RAM 访问失败 / Tx Event FIFO 满·丢失
                               //!< （FDCAN 专有；bxCAN 无对应故障源，该原因不会被触发）
} CAN_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct CANInstance，而结构体本身在后面才定义。
 * 缺了它，`struct CANInstance *` 会在**函数原型作用域**里另立一个同名 tag，
 * 与后面的正式定义是两个类型 → 赋值回调时 -Wincompatible-pointer-types。 */
struct CANInstance;

/** 错误回调签名（reason 见 CAN_ErrReason_e） */
typedef void (*CAN_ErrCallback)(struct CANInstance *instance, CAN_ErrReason_e reason);

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

/*------------- 发送完成回调契约（tx_complete_callback） --------------
 * 签名：void (*)(struct CANInstance *, uint32_t tx_mailbox, BSP_Status_e result)
 *   tx_mailbox: 该帧的发送标记（BxCAN=邮箱索引 0~2 / FDCAN=编码后的 MessageMarker），
 *               与 CANTransmit 出参 tx_mailbox 一致，用于与发送帧对应。
 *               **FDCAN 侧是不透明标记**（内部为"代际<<5 | 槽位"，用于识别迟到的旧事件），
 *               只应原样回传比对，不要解释其数值或假定范围
 *   result:     逐帧结果，成功与失败都会回调
 *               - BSP_OK     帧已真正发到总线上（含 FDCAN 的"取消请求迟到、仍发出"）
 *               - BSP_HW_ERR 帧**没有**发出去：仲裁失败 / 发送错误 / 被取消，
 *                            或该帧的溯源信息丢失（FDCAN Tx Event FIFO 满·丢失）
 *
 * **为什么失败也回调**：帧发不出去时若只有计数、没人通知发起者，异步分包
 * （comm media idseq/pkt0）就只能靠"连续 N 次发送仍忙"的兜底判据自愈 ——
 * N 帧被拒才恢复，且无法区分"真卡死"与"总线一直在错"。带上 result 后发起者可当场复位。
 *
 * 上下文：ISR（正常完成/失败）或任务上下文（CANRecover 强制收口、CANTransmit 入口的
 * 同步分发）。与 err_callback 同一套契约：无阻塞、可重入、幂等，且不得在其中调用
 * CANTransmit / CANConfig（会重入发送分发逻辑）。
 * 溯源槽先清后回调，故回调内可立即再发送并复用同一 tx_mailbox。
 */

/**
 * @brief CAN 运行时配置结构体（用于 CANConfig）
 * @note 只放总线级参数；单次发送的超时按调用传参（见 CANTransmit 的 timeout_ms）。
 */
typedef struct
{
    BoardCAN_e can_e;                                                                         // 板载CAN枚举（用于查找硬件映射）
    CAN_Mode_Type_e mode;                                                                     // 工作模式
    void *parent;                                                                             // 父实例指针（由 DRV 层设置）
    CAN_Filter_s *filters;                                                                    // 软件过滤器数组（硬过滤全通后由软件匹配分发；可为 NULL）
    uint8_t filter_num;                                                                       // 过滤器数量
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result); // 发送完成回调（见上方契约；可为 NULL）
    CAN_ErrCallback err_callback;                                                             // 错误回调（总线/硬件级事件，见 CAN_ErrReason_e；可为 NULL）
} CAN_Config_s;

struct CANInstance
{
    BoardCAN_e can_e;                                                                         // 板载CAN枚举（Config时查找映射）
    CAN_Map_t map;                                                                            // CAN映射（Config时自动填充）
    CAN_Mode_Type_e mode;                                                                     // 工作模式
    void *parent;                                                                             // 父实例指针（由 DRV 层设置）
    CAN_Filter_s *filters;                                                                    // 软件过滤器数组（Config时写入，指向 config 中的数组）
    uint8_t filter_num;                                                                       // 过滤器数量（Config时写入）
    void (*tx_complete_callback)(struct CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result); // 发送完成回调（ISR 或任务上下文；可为 NULL）
    CAN_ErrCallback err_callback;                                                             // 错误回调（ISR 上下文；可为 NULL）
    // 契约（同 USARTInstance.err_callback）：在错误分类计数、状态快照、纠正动作**之后**调用；
    // handler 必须无阻塞、可重入、幂等（可能连续高频触发），且不得在其中调用
    // CANTransmit / CANConfig（会重入发送分发与状态机）。
};

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义CAN实例（仅身份绑定；板载CAN枚举由 CANConfig 写入）
 * @param name 实例名称
 */
#define CAN_INSTANCE_DEF(name) static CANInstance name

/**
 * @brief 注册CAN实例（仅调用一次）
 * @param instance CAN实例指针（需先通过宏定义）
 * @retval BSP_OK        成功
 * @retval BSP_PARAM_ERR 参数非法
 * @retval BSP_HW_ERR    实例数超过上限 / 重复注册
 *
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不配置硬件参数（由 CANConfig 负责）。
 */
BSP_Status_e CANRegister(CANInstance *instance);

/**
 * @brief 配置CAN实例（填充硬件映射 + 工作模式 + 父指针 + 回调，可重复调用）
 * @retval BSP_OK        成功
 * @retval BSP_PARAM_ERR 参数非法（空指针 / 枚举越界 / 模式不兼容 / filter_num 与 filters 不匹配）
 * @retval BSP_HW_ERR    板级映射缺失（handle 为 NULL）或 HAL 初始化（Stop/ConfigFilter/Start/
 *                       ActivateNotification）失败
 */
BSP_Status_e CANConfig(CANInstance *instance, const CAN_Config_s *config);

/**
 * @brief 发送一帧CAN数据
 * @param instance      CAN实例
 * @param pack          数据包（id / frame_type / len / data）
 * @param timeout_ms    发送资源等待超时（ms）：BxCAN 邮箱 / FDCAN Tx FIFO 满时最多等待其空闲，
 *                      等待期间不抢占；传 0 表示不等待，资源满立即返回 BSP_BUSY。
 *                      **中断上下文内恒按 0 处理**（ISR 里空转等邮箱等于卡住整个系统，
 *                      等待时长还不可控），见各 .c 入口的上下文判定。
 * @param tx_mailbox    出参：本次发送的发送标记（BxCAN=邮箱索引 0~2 / FDCAN=不透明的 MessageMarker，
 *                      发送完成回调据此与发送帧对应）；可为 NULL
 * @param tx_free_level 出参：发送后剩余可发送数（BxCAN=空闲邮箱数 0~CAN_TX_MAILBOX_NUM /
 *                      FDCAN=Tx FIFO 空闲元素数）；可为 NULL
 * @retval BSP_OK        已加入发送资源（**不代表已发出**，逐帧结果看 tx_complete_callback）
 * @retval BSP_PARAM_ERR 参数非法 / 长度超限 / 帧类型与工作模式不兼容
 * @retval BSP_BUSY      发送资源满且 timeout_ms == 0（含"中断上下文被钳为 0"）
 * @retval BSP_TIMEOUT   等发送资源耗尽 timeout_ms
 * @retval BSP_HW_ERR    加入发送资源失败（HAL 返回错误）或实例尚未成功 CANConfig
 */
BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level);

/**
 * @brief CAN 发送资源自恢复入口（**任务上下文**，由调用方在自己的时基上周期调用或按需调用）
 * @param instance CAN实例。**注意它是"哪条总线"的选择符，不是"作用对象"**——本函数作用于
 *                 该实例所在的整个 CAN 外设，同一条总线上挂的其它实例一样受影响
 * @retval BSP_OK      本次刚做过恢复动作，且做完后总线可用
 * @retval BSP_BUSY    未动作：外设未启动（尚未成功 CANConfig），**或入口自证判据不成立**
 *                     （总线没有"恢复能治好的"证据，见下面 @note）
 * @retval BSP_HW_ERR  做了动作但外设仍不可用（仍在 bus-off / 邮箱仍占满），已计数
 *
 * @note **入口自证（先判、后动）**：这是本入口与 `USARTRecoverTxIfStuck` 那一类最大的不同点。
 *       本函数作用于**整条总线**（取消该总线上**所有实例**的在途帧、必要时重停外设），
 *       而调用方只看得到**自己这一条链路**。"我这条链路没收到帧"不是总线级证据——对端不发、
 *       滤波器不匹配、流量被同总线别的实例挤掉都能造成它，且这三样本函数一个都治不好。
 *       所以：**调用点只负责"何时看一眼"（限频），"值不值得动手"的判据在 bsp 内部**，
 *       判据不成立时返回 `BSP_BUSY` 且**不碰任何在途帧**（健康链路下调用它是零代价的）。
 *       判据与逐条理由写在两个实现文件里（`bsp_fdcan.c` / `bsp_bxcan.c` 的 `CANRecover`
 *       函数体开头），改判据前先读那一段；这里只给结论：
 *       ① bus-off（硬件自己走不出恢复序列）→ 动手；
 *       ② 发送资源占满 **且** 发送错误计数器 ≥ 128 → 动手（帧确实一直发不出去，不是正常忙时）；
 *       ③ 其余一律 `BSP_BUSY`（含"marker 池占满"，那是合法突发也会有的状态，且真泄漏已在
 *          Tx Event FIFO 满/丢的分支里就地回收）。
 *
 * @note 做五件事，顺序固定（不能换）：
 *       ① 取消全部在途发送（BxCAN: 三个邮箱 / FDCAN: 全部 Tx Buffer），把"发不出去还占着
 *          资源"的帧放掉；
 *       ② 逐个通知它们的发起者"这帧失败了"—— **必须显式做**：被成功取消的帧可能不产生
 *          任何完成回调（FDCAN 尤其确定：取消成功不写 Tx Event），不通知就会让上层永远
 *          等不到结果；
 *       ③ 清软件错误标志（bxCAN `HAL_CAN_ResetError` / FDCAN 无对应动作，靠硬件状态机）；
 *       ④ 回读总线状态判断是否恢复：BOFF 仍置 / 邮箱·Tx FIFO 仍占满 ⇒ 没恢复。FDCAN 的
 *          bus-off 兜底重启（Stop→ConfigInterruptLines→Start→ActivateNotification）在这一步；
 *       ⑤ 计 recover_ok / recover_fail，刷新状态快照。
 *
 * @note **④ 的"仍占满"判据要等取消落地**：ABRQ / TXBCR 是硬件异步执行的，写完立刻回读
 *       可能还是旧值（把一次成功的恢复判成失败）。实现里按 `CAN_RECOVER_DRAIN_US` 有界轮询，
 *       且采样点必须在可能的重启动作**之后**（FDCAN 的 Stop/Start 会清空 Tx FIFO）。
 *
 * @note **为什么不在 ISR 里做**：④ 的回读要等取消落地（有界轮询 ≤ `CAN_RECOVER_DRAIN_US`），
 *       且 FDCAN 的兜底要 Stop/Start 外设——都会丢在途帧、也不该在中断里阻塞。恢复必须是
 *       上层显式调用，BSP 不自动触发。
 * @note **现状（谁在调）**：`drv_comm` 的两个 CAN 介质后端（`comm_media_can_pkt0` /
 *       `comm_media_can_idseq`）在**自己的发送入口**里调它，按 `can_e` 限频
 *       （`DRV_COMM_MEDIA_CAN_RECOVER_PERIOD_MS`，默认 100ms）。选发送入口的理由：它是
 *       任务上下文、每帧必经、且"发不出去"正是本函数要治的病。**刻意不再搭在 daemon 的
 *       离线钩子上**——那个钩子的触发条件是"这条链路没收到帧"，是实例级证据，与总线级动作
 *       不匹配（见上面的入口自证）。
 * @note **代价**：判据成立时在途帧会被取消，上层会看到它们以 BSP_HW_ERR 失败并（按各自
 *       协议）重发，即"放弃若干帧换回发送能力"；判据不成立时零代价（只多两次寄存器读）。
 */
BSP_Status_e CANRecover(CANInstance *instance);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* __BSP_CAN_H */
