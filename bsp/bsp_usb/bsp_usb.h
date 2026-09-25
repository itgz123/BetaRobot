/**
 * @file bsp_usb.h
 * @brief USB VCP 驱动封装，提供实例管理、错误上报与自恢复入口
 *
 * @note 硬件配置（CDC 枚举/缓冲）由 CubeMX 负责，BSP 层只管理实例。
 *
 * @note 对外接口：
 *       USBRegister（注册，不可重入）→ USBConfig（配置，可重入）
 *       → USBTransmit（发送）
 *       → USBRecoverTxIfStuck / USBRecoverRxIfStalled / USBReenumerate
 *         （三条自恢复入口，只把"何时看一眼"交给上层，判据/计时/纠正动作都在这层）。
 *       其余一切（is_ready / 内部发送队列 / 多缓冲）都不对外暴露或不实现：
 *       忙由 BSP_BUSY 表达，多缓冲与排队归上层。
 *
 * @warning USB 硬件初始化（MX_USB_DEVICE_Init）必须早于第一次 USBTransmit——这才是
 *          真正有效的约束。现状是四块板都在 freertos.c 的 StartDefaultTask
 *          （osPriorityIdle）里调用，与旧版注释"必须在 osKernelStart() 之前"不符但实测可用
 *          （每个周期任务都会阻塞让出，idle 能跑到）。详见 bsp_usb.c 顶部 @warning。
 */

#ifndef __BSP_USB_H
#define __BSP_USB_H

#include "bsp_map.h"

#if defined(HAL_PCD_MODULE_ENABLED)

#include "bsp_common.h"
#include <stdint.h>
#include "usbd_cdc_if.h"

/*------------- 常量定义 --------------*/

/**
 * @brief 单包发送缓冲区大小
 * @note 统一使用 FS 最大包长 64 字节：
 *   - DJI_C/A (F407/F427) — USB_OTG_FS，原生 FS
 *   - DM_MC02 (H723)      — USB_OTG_HS 内嵌 FS PHY，实际也是 FS
 */
#define USB_TX_BUF_SIZE CDC_DATA_FS_MAX_PACKET_SIZE /* 64 */

/* 自恢复默认阈值（调用方传 0 时取默认值） */
#ifndef USB_TX_STUCK_DEFAULT_MS
#define USB_TX_STUCK_DEFAULT_MS 200u /* TX 在途超过该时长仍未出队即判卡死 */
#endif
#ifndef USB_RX_STALL_DEFAULT_MS
#define USB_RX_STALL_DEFAULT_MS 100u /* 距上次收帧超过该时长即判接收停摆 */
#endif

/*------------- 错误上报 --------------*/

/**
 * @brief err_callback 的触发原因
 * @note USB 的"错误"与串口不同：端点层几乎不会主动报错（PCD 的 suspend/resume/
 *       disconnect 回调本层未接），能观测到的都是"发不出去/收不到"这类**停滞**。
 *       因此这里如实分成四类，而不是硬套串口的 HW/TX_ABORT/RX_STALLED 三分法：
 *       - TX_STUCK / RX_STALLED 是本层**已经做完纠正动作**之后的通报，handler
 *         据 reason 即可，不必自己再判断"是不是真卡了"；
 *       - HW 是"纠正动作本身失败"（唯一触发点：TX 卡死收尾时 EP_Abort 没成功），
 *         此时清 TxState 是唯一还起作用的动作；
 *       - NOT_CONFIGURED 是"主机侧串口没打开/已拔出"，软件无从恢复，只能等枚举回来。
 */
typedef enum : uint8_t
{
    USB_ERR_HW = 0,             //!< 端点/PCD 层收尾失败（EP_Abort 未成功）；任务上下文
    USB_ERR_TX_STUCK = 1,       //!< 在途 IN 传输卡死，已强制收尾并重试；任务上下文，hcdc->TxState 已清
    USB_ERR_RX_STALLED = 2,     //!< 接收长期无进展，且重新武装最终失败；任务上下文
    USB_ERR_NOT_CONFIGURED = 3, //!< 枚举状态下降沿（未枚举/已拔出）：发送被拒，软件无从恢复
} USB_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct USBInstance，而结构体本身在后面才定义。
 * 缺了它，`struct USBInstance *` 会在**函数原型作用域**里另立一个同名 tag，
 * 与后面的正式定义是两个类型 → 赋值回调时 -Wincompatible-pointer-types。 */
struct USBInstance;

/** 错误回调签名（reason 见 USB_ErrReason_e） */
typedef void (*USB_ErrCallback)(struct USBInstance *instance, USB_ErrReason_e reason);

/*------------- 类型定义 --------------*/

/**
 * @brief USB实例结构体
 * @note TX 环形缓冲为实例内嵌数组（tx_ring），与实例一同放在普通 RAM：
 *       **USB 不走 DMA（CubeMX 里 PCD 的 dma_enable 为 DISABLE），无需 DMA_RAM**。
 *       接收数据由 bsp_usb_rx_handler 填入 rx_buff/rx_len（指向 CubeMX 侧
 *       UserRxBufferFS/HS，每包由 CDC 回调重新挂同一缓冲），回调内直接从实例读取。
 */
typedef struct USBInstance
{
    void *parent;                              /* 父实例指针（由 DRV 层设置）*/
    uint8_t tx_ring[APP_TX_DATA_SIZE];         /* TX 环形缓冲区*/
    volatile uint16_t tx_head;                 /* 生产者写入位置（任务上下文写）*/
    volatile uint16_t tx_tail;                 /* 消费者读出位置（ISR 与任务上下文都可能推进）*/
    uint8_t tx_buf[USB_TX_BUF_SIZE];           /* 单包发送缓冲（仅被持 tx_claim 的上下文使用）*/
    volatile uint8_t tx_claim;                 /* 1 = 某上下文正在提交这一包（保护 tx_buf 不被并发覆写）*/
    volatile uint64_t tx_last_ok_us;           /* 最近一次成功提交给 CDC 的时刻（DWT 微秒；卡死判据基准）*/
    volatile uint8_t dev_state;                /* 最近一次采样的 USB 枚举状态（USBD_STATE_*）*/
    uint8_t *rx_buff;                          /* 接收缓冲指针（指向 HAL 的 UserRxBufferFS/HS）*/
    uint16_t rx_len;                           /* 本次接收数据长度 */
    volatile uint64_t rx_last_us;              /* 最近一次收帧时刻（DWT 微秒；接收停摆判据基准）*/
    void (*rx_callback)(struct USBInstance *); /* 接收完成回调（ISR 上下文）*/
    void (*tx_callback)(struct USBInstance *); /* 发送完成回调（ISR 上下文）*/
    USB_ErrCallback err_callback;              /* 错误回调（任务上下文，原因见 USB_ErrReason_e）*/
    // 契约：err_callback 全部在**纠正动作之后**调用，且只在任务上下文触发（本模块的
    //       恢复入口与 USBTransmit 都是任务上下文；USB 中断里不产生 err_callback）。
    //       handler 必须无阻塞、可重入、且**幂等**，且不得在里面调用
    //       USBTransmit / USBConfig / 三条恢复入口（那会在恢复路径上递归）。
} USBInstance;

/*------------- 配置结构体 --------------*/

/**
 * @brief USB 运行时配置结构体（用于 USBConfig）
 */
typedef struct
{
    void (*rx_callback)(struct USBInstance *); /* 接收完成回调（可为 NULL）*/
    void (*tx_callback)(struct USBInstance *); /* 发送完成回调（可为 NULL）*/
    USB_ErrCallback err_callback;              /* 错误回调（可为 NULL，签名见 USB_ErrReason_e）*/
    void *parent;                              /* 父实例指针（经 USBConfig 写入实例；可为 NULL）*/
} USB_Config_s;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义 USB 实例（放普通 RAM：USB 无 DMA）
 * @param name 实例名称
 * @example
 *   USB_INSTANCE_DEF(usb_vcp);
 */
#define USB_INSTANCE_DEF(name) static USBInstance name = {0}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册 USB 实例（仅调用一次）
 * @param instance USB 实例指针（需先通过 USB_INSTANCE_DEF 定义）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 实例为空 / 重复注册 / 超过实例数上限
 *
 * @note 仅注册实例（TX 环形缓冲为实例内嵌数组），不初始化硬件。
 *       硬件初始化由 MX_USB_DEVICE_Init 负责，须早于第一次 USBTransmit。
 * @note USB_INSTANCE_NUM 目前四块板都是 1（bsp_map.h），本模块**按单活动实例设计**
 *       （s_active_inst + CubeMX 侧唯一的 UserRxBuffer/HCDC 句柄）。多实例是待办事项，
 *       不是本模块已支持的用法。
 */
BSP_Status_e USBRegister(USBInstance *instance);

/**
 * @brief 配置 USB 实例（可重复调用）
 * @param instance USB 实例指针
 * @param config   配置结构体指针（回调 / parent）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 参数非法 / 实例未注册
 *
 * @note 只设置回调，不初始化 USB 硬件。要求先调用 USBRegister。
 */
BSP_Status_e USBConfig(USBInstance *instance, const USB_Config_s *config);

/**
 * @brief 发送数据（异步）
 * @param instance USB 实例
 * @param data 数据指针
 * @param len  数据长度
 * @retval BSP_OK        整帧已写入环形缓冲，并已尝试立即发出（**不代表已发出**）
 * @retval BSP_BUSY      环形缓冲放不下整帧——**一个字节都没写**，等上层退避后重发整帧
 * @retval BSP_PARAM_ERR instance/data 为空或 len == 0
 * @retval BSP_HW_ERR    未枚举（主机侧串口未打开/已拔出），整帧被拒
 *
 * @note **环满时零字节写入**：旧版是"写多少算多少、剩下的丢掉"，对端会收到残帧。
 *       现在要么整帧入队、要么不动 ring 直接报 BSP_BUSY，由上层退避重试——
 *       残帧本身就是可避免的。
 * @note 数据写入 ring 后立即尝试发送；主机恢复读取后由 TX 完成中断自动续发。
 * @note 未枚举（BSP_HW_ERR）与缓冲满（BSP_BUSY）语义不同：前者要等枚举回来，
 *       后者退避一下就能成，上层应分开处理（见 USB_ErrReason_e）。
 */
BSP_Status_e USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len);

/**
 * @brief TX 卡死自恢复：判"在途 IN 传输卡死"，是则强制收尾并重试（任务上下文）
 * @param instance USB 实例
 * @param stuck_ms 卡死判定阈值(ms)；传 0 取 USB_TX_STUCK_DEFAULT_MS
 * @retval BSP_OK       本次刚判定卡死并强制收尾（err_callback 已以 TX_STUCK 通知过）
 * @retval BSP_BUSY     未动作：ring 为空 / 未超阈值 / 首次建立基准 / 未枚举
 * @retval BSP_HW_ERR   收尾动作执行了但没能恢复出队；或设备句柄未就绪
 * @retval BSP_PARAM_ERR 无实例
 *
 * @note 返回值约定与 USARTRecoverTxIfStuck / USARTRecoverRxIfStalled 一致：
 *       **BSP_OK = 本次刚做了恢复动作**（据此打"已恢复"日志），链路健康时返回 BSP_BUSY。
 *
 * @note **判据是"ring 有货且距上次成功入队已超 stuck_ms"**，不是"TxState != 0"——
 *       主机不读时 TxState 长期为 1 是正常现象（USB 全速 1ms 帧、主机轮询间隔），
 *       只有"明明有数据要发、却迟迟一包都提交不出去"才是卡死。
 * @note 纠正动作 = 对 CDC IN 端点 HAL_PCD_EP_Abort + 清 hcdc->TxState，然后重试一次
 *       bsp_usb_process_tx()。触碰 TxState 是本仓库已有做法（usbd_cdc_if.c 自己就这么读），
 *       HAL/中间件没有公开的"清 TxState"接口；代价是主机侧可能看到被截断的一包，
 *       由 media 的分包序号重组丢帧重同步——这是"卡死 vs 丢一帧"的取舍。
 * @warning 本函数只在**任务上下文**调用（内部会回调 err_callback、动端点）；
 *          不要写在 USB 中断里——链路一旦卡住中断也不会再来，写在里面的复位永远够不到。
 * @note 幂等：未超阈值/ring 为空时只刷新基准，不动硬件。
 */
BSP_Status_e USBRecoverTxIfStuck(USBInstance *instance, uint32_t stuck_ms);

/**
 * @brief RX 停摆自恢复：判"接收长期无进展"，是则重新武装 OUT 端点（任务上下文）
 * @param instance USB 实例
 * @param period_ms 停摆判定阈值(ms)；传 0 取 USB_RX_STALL_DEFAULT_MS
 * @retval BSP_OK       本次刚判定停摆并重新武装了 OUT 端点（中间件已接受）
 * @retval BSP_BUSY     未动作：未超阈值 / 首次建立基准 / 未枚举
 * @retval BSP_HW_ERR   重新武装被拒（已计 rx_rearm_fail 并回调 err_callback）；或句柄未就绪
 * @retval BSP_PARAM_ERR 无实例
 *
 * @note 背景：CubeMX 的 CDC_Receive_HS 把 USBD_CDC_ReceivePacket 的返回值丢掉了
 *       （usbd_cdc_if.c 生成代码），一旦那次重挂失败，RX **永久静默且没有任何中断可依**。
 *       这里用"距上次收帧超过 period_ms"作为可观测判据，重新挂同一缓冲
 *       （Buf 恒为 CubeMX 侧的 UserRxBufferFS/HS），对已武装的端点重复
 *       HAL_PCD_EP_Receive 不改变数据通路。
 * @note rx_last_us 在检测到停摆时也被刷新，故本函数是限频的：每 period_ms 最多动作一次。
 * @note "重新武装被接受"只说明中间件收下了这次请求，真正是否恢复要等下一帧到来
 *       （看 s_usb_status[].rx_ok 是否继续增长）。
 * @warning 只在**任务上下文**调用。
 */
BSP_Status_e USBRecoverRxIfStalled(USBInstance *instance, uint32_t period_ms);

/**
 * @brief 显式重新枚举（断开再上拉 D+；只在任务上下文、由上层主动决定是否调用）
 * @param instance USB 实例（传 NULL 时用当前活动实例）
 * @retval BSP_OK       已重新连接
 * @retval BSP_HW_ERR   句柄未就绪 / HAL_PCD_DevDisconnect|DevConnect 失败
 * @retval BSP_PARAM_ERR 无活动实例
 *
 * @warning **本模块内部绝不自动调用它**：它会让 PC 端 COM 口消失再出现，
 *          上层若没处理会以为链路彻底断了（打开中的串口会掉线，需要重新打开）。
 *          是否用、什么时候用，必须由 app 决定；本函数只把它做成一个可调用的动作。
 * @warning 阻塞约 20ms（断开后等主机察觉再上拉；DWT 忙等，不依赖 RTOS）。
 *          不要把本函数放进控制周期里跑。
 */
BSP_Status_e USBReenumerate(USBInstance *instance);

#endif /* HAL_PCD_MODULE_ENABLED */

#endif /* __BSP_USB_H */
