/**
 * @file bsp_usart.h
 * @brief USART驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（波特率/校验位/中断/DMA等）由 CubeMX 负责，BSP 层只管理实例
 *
 * @note 对外接口：
 *       USARTRegister（注册，不可重入）→ USARTConfig（配置，可重入）
 *       → USARTTransmit / USARTReceive（收发，模式每次调用传参）
 *       → USARTRecoverRxIfStalled / USARTRecoverTxIfStuck（两条自恢复入口，
 *         供各链路在自己的任务上下文时基上调用，判据/计时/纠正动作都在 bsp 内）。
 *       其余一切（is_ready / 函数指针表 / 内部发送队列 / 缓冲池）都不对外暴露或不实现：
 *       忙由 BSP_BUSY 表达，多缓冲与排队归上层。
 *
 * @note 缓冲区与生存期（IT/DMA 模式）：
 *       - 发送：data 指向的缓冲生存期须覆盖到 tx_callback 返回；BLOCK 模式只在调用期内有效
 *       - 接收：USARTConfig 写入的 rx_buff 由 USART_INSTANCE_DEF 静态分配，常驻
 *       - 本层不做发送队列/缓冲：忙即返回 BSP_BUSY，多缓冲由上层（bsp_log 缓冲池、
 *         drv_vofa 三缓冲、drv_terminal_lite 槽池）自行管理
 */

#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "bsp_map.h"

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_common.h"
#include "main.h"
#include "stdint.h"

/*------------- 实例结构体 --------------*/

/**
 * @brief err_callback 的触发原因（与 USARTInstance.err_callback 配套）
 * @note 上层 handler 的归还判据各不相同，需据此分流，避免"靠 gState 反推是哪种情况"：
 *       - TX 侧三种（log/vofa/terminal/media）：只关心 TX_ABORT（缓冲必被归还）与 HW
 *         （要看 gState 判断发送是否仍在途）；
 *       - RX 侧（dbus/sbus）：只关心 RX_STALLED（失控保护必须覆盖"完全没有数据流"）。
 */
typedef enum : uint8_t
{
    USART_ERR_HW = 0,         //!< ① 硬件错误（PE/FE/NE/ORE/DMA…），HAL_UART_ErrorCallback 内，ISR 上下文
    USART_ERR_TX_ABORT = 1,   //!< ② 在途发送被强制中止（卡死自恢复），任务上下文，gState 已复位为 READY
    USART_ERR_RX_STALLED = 2, //!< ③ 接收停摆（续收最终失败、rx_armed 已清），ISR 上下文，此后不再有 rx_callback
} USART_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct USARTInstance，而结构体本身在后面才定义。
 * 缺了它，`struct USARTInstance *` 会在**函数原型作用域**里另立一个同名 tag，
 * 与后面的正式定义是两个类型 → 赋值回调时 -Wincompatible-pointer-types。 */
struct USARTInstance;

/** 错误回调签名（reason 见 USART_ErrReason_e） */
typedef void (*USART_ErrCallback)(struct USARTInstance *instance, USART_ErrReason_e reason);

/**
 * @brief USART实例结构体
 */
typedef struct USARTInstance
{
    void *parent;                                // 父实例指针（Config 写入，DRV 不再直写）
    BoardUART_e uart_e;                          // 板载UART枚举（Config时查找映射）
    UART_HandleTypeDef *handle;                  // UART句柄（Config时自动填充）
    uint8_t *rx_buff;                            // 接收缓冲区指针
    const uint16_t rx_buff_size;                 // 接收缓冲区大小（编译期固定、只读）
    volatile uint16_t rx_len;                    // 最近一次收到的数据长度（ISR 写、任务读）
    uint16_t rx_xfer_len;                        // 当前接收请求长度（IT/DMA 自动续收用）
    BSP_Transfer_Mode_e rx_mode;                 // 当前接收模式（IT/DMA 自动续收用）
    volatile uint8_t rx_armed;                   // 1 = IT/DMA 接收常开流已启动（收到一帧会自动续收）
    void (*rx_callback)(struct USARTInstance *); // 接收完成回调（ISR 上下文）
    void (*tx_callback)(struct USARTInstance *); // 发送完成回调（IT/DMA，ISR 上下文）
    USART_ErrCallback err_callback;              // 错误回调（ISR 或任务上下文，原因见 USART_ErrReason_e）
    // 契约：err_callback 触发后不会再有**本次传输**的完成回调，上层须在此复位自身状态
    //       （如归还正在"发送中"的缓冲，否则缓冲池会永久泄漏）。何种传输受影响由 reason 指明：
    //         USART_ERR_TX_ABORT   → 在途发送已被中止，tx_callback 不会再来；
    //         USART_ERR_RX_STALLED → 接收常开流已断，rx_callback 不会再来；
    //         USART_ERR_HW         → 硬件错误，发送可能仍在途（HAL 的错误位都在接收侧），
    //                                须自行判 gState 后再决定是否归还。
    // 因此 handler 必须无阻塞、可重入、且**幂等**（判自身状态再归还，重复调用无副作用），
    // 且不得在里面调用 USARTTransmit / USARTConfig / USARTReceive。
} USARTInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义USART实例（同时定义缓冲区）
 * @param name     实例名称
 * @param buff_sz  接收缓冲区大小（影响静态内存分配，必须编译期确定）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   USART_INSTANCE_DEF(sbus_uart, 64);
 */
#define USART_INSTANCE_DEF(name, buff_sz)                                                                              \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};                                                              \
    static USARTInstance name = {.rx_buff = name##_rx_buff, .rx_buff_size = buff_sz}

/*------------- 配置结构体 --------------*/

/**
 * @brief USART 运行时配置结构体（用于 USARTConfig）
 * @note 只放总线级参数：模式与超时改为每次收发调用传参，不在此配置。
 */
typedef struct
{
    BoardUART_e uart_e;                          // 板载UART枚举（用于查找硬件映射）
    void *parent;                                // 父实例指针（可为NULL，写入 instance->parent）
    void (*rx_callback)(struct USARTInstance *); // 接收完成回调（可为NULL）
    void (*tx_callback)(struct USARTInstance *); // 发送完成回调（IT/DMA，可为NULL）
    USART_ErrCallback err_callback;              // 错误回调（可为NULL，签名见 USART_ErrReason_e）
} USART_Config_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册USART实例（仅调用一次）
 * @param instance USART实例指针（需先通过宏定义）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 实例为空 / 重复注册 / 超过实例数上限
 *
 * @note 仅检查参数、防重后加入 static 管理数组。不配置硬件参数（由 USARTConfig 负责）。
 */
BSP_Status_e USARTRegister(USARTInstance *instance);

/**
 * @brief 配置USART实例（可重复调用）
 * @param instance USART实例指针
 * @param config   配置结构体指针（uart_e / parent / 三个回调）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 参数非法 / uart_e 越界 / 该 uart_e 未映射句柄 / 同一句柄已属其它实例
 *
 * @note 填充硬件句柄与回调，并登记句柄→实例路由表。可重复调用以重新配置。
 *       要求先调用 USARTRegister。
 * @note 本函数不再自动启动接收（旧版在此启动 DMA+IDLE）：接收模式改为调用期决定，
 *       需接收的实例请在 Config 之后显式调用 USARTReceive 启动常开流。
 * @note 重入时会先中止未完成的 IT/DMA 接收（避免旧传输继续写新缓冲），
 *       因此不要在该实例的 rx_callback 内调用本函数。
 */
BSP_Status_e USARTConfig(USARTInstance *instance, const USART_Config_s *config);

/**
 * @brief 发送数据（模式每次调用传参）
 * @param instance   USART实例
 * @param data       发送数据指针（IT/DMA 模式须生存到 tx_callback 返回；BLOCK 只需覆盖调用期）
 * @param len        数据长度
 * @param mode       传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms 等待"发送就绪"的超时（毫秒）；0 = 不等待，忙即返回 BSP_BUSY
 * @retval BSP_OK        已受理（BLOCK=已发完；IT/DMA=已启动，完成看 tx_callback）
 * @retval BSP_BUSY      外设忙（timeout_ms==0 时的常态，上层应自行排队重试）
 * @retval BSP_TIMEOUT   等待就绪超时（此时 BSP 已强制中止卡死的发送并复位状态，可立即重试）
 * @retval BSP_PARAM_ERR 参数非法 / len 为 0 / mode 非法 / 该口没有 TX DMA
 * @retval BSP_HW_ERR    HAL 启动发送失败
 *
 * @note timeout_ms 只在 IT/DMA 模式用于等待上一次发送结束；BLOCK 模式该值直接
 *       透传给 HAL_UART_Transmit（0 表示 HAL 侧立即超时，语义一致）。
 * @note BLOCK 模式内部按 tick 自旋，禁止在中断上下文调用；IT/DMA 可在中断中调用，
 *       卡死复位（HAL_UART_AbortTransmit）只在就绪等待失败路径触发，也属于该调用上下文。
 * @note tx_callback 内续发是允许的（HAL 已把 gState 置回 READY），但不得在回调内调
 *       USARTConfig / USARTReceive。
 */
BSP_Status_e USARTTransmit(USARTInstance *instance, const uint8_t *data, uint16_t len, BSP_Transfer_Mode_e mode,
                           uint32_t timeout_ms);

/**
 * @brief 启动接收（模式每次调用传参）
 * @param instance   USART实例
 * @param len        本次接收长度（1 ~ rx_buff_size）
 * @param mode       传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms BLOCK 模式的接收超时（毫秒）；IT/DMA 模式忽略
 * @retval BSP_OK        已受理（BLOCK=已收到；IT/DMA=已启动常开流）
 * @retval BSP_BUSY      IT/DMA 接收已在运行或外设忙 / BLOCK 模式外设忙
 * @retval BSP_TIMEOUT   BLOCK 模式等待接收超时
 * @retval BSP_PARAM_ERR 参数非法 / len 越界 / mode 非法 / 该口没有 RX DMA
 * @retval BSP_HW_ERR    HAL 启动接收失败
 *
 * @note IT/DMA 为"常开流"：每收到一帧（IDLE 或收满）自动按同一 len/mode 续收，
 *       期间 rx_armed 为 1；重复调用返回 BSP_BUSY。收到数据触发 rx_callback。
 * @note BLOCK 模式只收一帧，收完即返回，不占用常开流。
 * @note 常开流与 BLOCK 接收互斥（同实例只允许一种接收在跑）。
 * @note **必须在任务上下文调用**（内部可能触发 HAL_UART_AbortReceive 清残留状态，
 *       它会按 HAL_GetTick 自旋，ISR 里会死等）。
 * @note 这是底层"启动"接口：停摆后的自恢复请用 USARTRecoverRxIfStalled —— 它按 bsp 记着的
 *       rx_xfer_len/rx_mode 重启、自带限频与停摆判据，上层不必再各写一份。
 */
BSP_Status_e USARTReceive(USARTInstance *instance, uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief 接收停摆自恢复（幂等；各链路的 daemon 回调 / 空闲超时里调用）
 * @param instance  待检查的 USART 实例
 * @param period_ms 重启尝试的限频周期（ms）；0 = 不限频
 * @retval BSP_OK       本次刚把接收重启起来（据此打"已恢复"日志；链路健康时不返回它）
 * @retval BSP_BUSY     未动作：接收在跑 / 该实例没有常开流 / 限频期内 / 上下文不允许 Abort
 * @retval BSP_PARAM_ERR 实例 / 句柄 / 接收缓冲为空（未 Register 或未 Config）
 * @retval 其它         尝试过但失败（透传 USARTReceive 的结果，bsp 内部已打 ERROR 日志）
 *
 * @note 判据是内部的 `rx_armed == 0`（真停摆），不是"对端没发帧"：对端没开机时接收本身
 *       是好的，不动它。重启参数用 bsp 实例里的 rx_xfer_len/rx_mode（USARTReceive 在
 *       所有失败分支**之前**记录，故失败过也仍可重试）。
 * @note **必须在任务上下文调用**：内部经 USARTReceive → HAL_UART_AbortReceive 清残留，
 *       按 HAL_GetTick 自旋，ISR / 临界区里会死等（那里直接返回 BSP_BUSY，不消费限频窗口）。
 * @note 限频基准（s_rx_restart_us）在 bsp 内按 UART 维护，上层不必自存时间戳。
 */
BSP_Status_e USARTRecoverRxIfStalled(USARTInstance *instance, uint32_t period_ms);

/**
 * @brief 发送卡死自恢复（幂等；各发送链在自己的"发送入口"调用）
 * @param instance 待检查的 USART 实例
 * @param stuck_ms 判定"卡死"的时长阈值（ms）：gState 连续非 READY 超过它即强制复位；0 = 用默认值
 * @retval BSP_OK   本次刚复位了一次卡死的发送：err_callback 已按契约以 TX_ABORT 通知过，
 *                  上层"发送中"的缓冲已被归还，可以立刻重新发送
 * @retval BSP_BUSY 未动作：发送空闲（顺带刷新计时基准）/ 还没到阈值（可能只是一次正常在途发送）
 *                  / 上下文不允许 Abort
 * @retval BSP_PARAM_ERR 实例或句柄为空
 *
 * @note 为什么要有这个入口（而不是只靠 USARTTransmit 内部的复位）：IT/DMA 发送卡死后
 *       gState 停在 BUSY_TX，该次发送的 tx_callback 永远不会来。而各发送链的"发送中就绪判据"
 *       用的是自己的缓冲状态（bsp_log 找 SEND 槽、vofa 找 ACTIVE、terminal 找 s_tx_now、
 *       media 直接看 gState），一旦有帧卡在"发送中"，它们**再也不会调用 USARTTransmit**
 *       （新帧只会被排进 WAIT_SEND/PENDING 队列，池满后连排都不排），于是 USARTTransmit
 *       里那段卡死复位永远没有机会执行 —— 链接永久静默，且看不到任何错误。
 *       本入口把那段判定单独暴露出来，让各发送链在**自己的发送入口**（每帧必经之处、
 *       与"是否真的发出去了"无关）就能做一次检查。
 *
 * @note 必须在任务上下文调用（内部经 HAL_UART_AbortTransmit 自旋等 HAL tick，见 USART_CanBlockingAbort）：
 *       ISR / 临界区里直接返回 BSP_BUSY 且不刷新计时基准，下一次任务上下文的调用照常补上。
 *       因此各发送入口原样调用即可 —— 从 ISR 打日志（BSPLOG）也是安全的。
 * @note 幂等且开销小：发送空闲时只做一次 DWT 读并刷新基准后返回；只有真的超过阈值才动硬件。
 * @note 计时基准（s_tx_ready_us）在 bsp 内按 UART 维护：它记"最近一次确认 gState == READY"的时刻，
 *       由本入口与 USARTTransmit 的就绪路径共同刷新，上层不必自存时间戳。
 */
BSP_Status_e USARTRecoverTxIfStuck(USARTInstance *instance, uint32_t stuck_ms);

#endif // HAL_UART_MODULE_ENABLED

#endif /* __BSP_USART_H */
