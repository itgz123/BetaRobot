/**
 * @file bsp_spi.h
 * @brief SPI驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（时钟极性/相位/波特率/DMA等）由 CubeMX 负责，BSP 层只管理实例
 * @note 片选控制由 DRV 层通过 GPIO 接口管理，BSP 层不负责
 *       （与 bsp_i2c 的"从机地址每次调用传参"是同一分工：BSP 只管总线）
 *
 * @note 对外接口：
 *       SPIRegister（注册，不可重入）→ SPIConfig（配置，可重入）
 *       → SPITransmit / SPIReceive / SPITransmitReceive（收发，模式每次调用传参）
 *       → SPIRecoverTxIfStuck（卡死自恢复入口，供 DRV 在自己任务上下文的读入口调用，
 *         判据/计时/纠正动作都在 bsp 内）。
 *       其余一切（work_mode / 函数指针表 / 内部收发缓冲 / is_ready）都不对外暴露或不实现：
 *       忙由 BSP_BUSY 表达，多缓冲与排队归上层。
 *
 * @note 缓冲区与生存期（IT/DMA 模式）：
 *       - 发送：tx_data 指向的缓冲生存期须覆盖到 tx_callback 返回；BLOCK 模式只在调用期内有效
 *       - 接收：结果固定写入 SPIConfig 登记的 rx_buff（由 SPI_INSTANCE_DEF 静态分配）
 */

#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "bsp_map.h"

#ifdef HAL_SPI_MODULE_ENABLED

#include "bsp_common.h"
#include "main.h"
#include "stdint.h"

/*------------- 实例结构体 --------------*/

/**
 * @brief err_callback 的触发原因（与 SPIInstance.err_callback 配套）
 * @note 两者都表示"本次传输不会再有完成回调"，上层须在 handler 里复位自身传输状态
 *       （如 BMI088 的 transfer_busy）并释放片选，否则该从机永久失联：
 *       - SPI_ERR_HW：HAL 报硬件错（MODF/OVR/FRE/DMA…），ISR 上下文；
 *       - SPI_ERR_ABORT：本次传输被 bsp 强制收尾（卡死自恢复），任务上下文。
 *       上层 handler 必须无阻塞、可重入且**幂等**（判自身状态再复位，重复调用无副作用）。
 */
typedef enum : uint8_t
{
    SPI_ERR_HW = 0,    //!< 硬件错误（MODF/OVR/FRE/DMA…），HAL_SPI_ErrorCallback 内，ISR 上下文
    SPI_ERR_ABORT = 1, //!< 传输被强制收尾（卡死自恢复），任务上下文，HAL State 已复位为 READY
} SPI_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct SPIInstance，而结构体本身在后面才定义。
 * 缺了它，`struct SPIInstance *` 会在**函数原型作用域**里另立一个同名 tag，
 * 与后面的正式定义是两个类型 → 赋值回调时 -Wincompatible-pointer-types。 */
struct SPIInstance;

/** 错误回调签名（reason 见 SPI_ErrReason_e） */
typedef void (*SPI_ErrCallback)(struct SPIInstance *instance, SPI_ErrReason_e reason);

/**
 * @brief SPI实例结构体
 */
typedef struct SPIInstance
{
    void *parent;                              // 父实例指针（Config 写入，DRV 不再直写）
    BoardSPI_e spi_e;                          // 板载SPI枚举（Config时查找映射）
    SPI_HandleTypeDef *handle;                 // SPI句柄（Config时自动填充）
    uint8_t *rx_buff;                          // 接收缓冲区指针
    const uint16_t buff_size;                  // 缓冲区大小（编译期固定、只读）
    volatile uint16_t rx_len;                  // 最近一次收到数据的长度（ISR 写、任务读）
                                               // 只由 Receive / TransmitReceive 更新；纯发送不动它
    void (*rx_callback)(struct SPIInstance *); // 接收/全双工完成回调（ISR 上下文）
    void (*tx_callback)(struct SPIInstance *); // 发送完成回调（IT/DMA，ISR 上下文）
    SPI_ErrCallback err_callback;              // 错误回调（ISR 或任务上下文，见 SPI_ErrReason_e）
    // 契约：err_callback 触发后不会再有本次传输的 rx_callback / tx_callback，上层须在此复位
    //       自身状态（如清 transfer_busy、释放片选）。handler 不得在里面调用
    //       SPITransmit / SPIReceive / SPIConfig。
} SPIInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义SPI实例（同时定义接收缓冲区）
 * @param name     实例名称
 * @param buff_sz  接收缓冲区大小（影响静态内存分配，必须编译期确定）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   SPI_INSTANCE_DEF(bmi088_spi, 64);
 */
#define SPI_INSTANCE_DEF(name, buff_sz)                                                                                \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};                                                              \
    static SPIInstance name = {.rx_buff = name##_rx_buff, .buff_size = buff_sz}

/*------------- 配置结构体 --------------*/

/**
 * @brief SPI 运行时配置结构体（用于 SPIConfig）
 * @note 只放总线级参数：传输模式改为每次收发调用传参，不在此配置。
 */
typedef struct
{
    BoardSPI_e spi_e;                          // 板载SPI枚举（用于查找硬件映射）
    void *parent;                              // 父实例指针（可为NULL，写入 instance->parent）
    void (*rx_callback)(struct SPIInstance *); // 接收/全双工完成回调（可为NULL）
    void (*tx_callback)(struct SPIInstance *); // 发送完成回调（IT/DMA，可为NULL）
    SPI_ErrCallback err_callback;              // 错误回调（可为NULL，签名见 SPI_ErrReason_e）
} SPI_Config_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册SPI实例（仅调用一次）
 * @param instance SPI实例指针（需先通过宏定义）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 实例为空 / 重复注册 / 超过实例数上限
 *
 * @note 仅检查参数、防重后加入 static 管理数组。不配置硬件参数（由 SPIConfig 负责）。
 */
BSP_Status_e SPIRegister(SPIInstance *instance);

/**
 * @brief 配置SPI实例（可重复调用）
 * @param instance SPI实例指针
 * @param config   配置结构体指针（spi_e / parent / 三个回调）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 参数非法 / spi_e 越界 / 该 spi_e 未映射句柄 / 同一句柄已属其它实例
 *
 * @note 填充硬件句柄与回调，并登记句柄→实例路由表。可重复调用以重新配置。
 *       要求在 SPIRegister 之后调用。
 * @note 重入时会先把上一次传输收尾（中止在途的 IT/DMA 传输，避免旧传输继续写缓冲），
 *       因此不要在该实例的回调内调用本函数。
 */
BSP_Status_e SPIConfig(SPIInstance *instance, const SPI_Config_s *config);

/**
 * @brief SPI发送（只发不收，收进来的字节丢弃）
 * @param instance   SPI实例
 * @param data       发送数据指针（IT/DMA 模式须生存到 tx_callback 返回；BLOCK 只需覆盖调用期）
 * @param len        数据长度
 * @param mode       传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms 等待"总线就绪"的超时（毫秒）；0 = 不等待，忙即返回 BSP_BUSY
 * @retval BSP_OK        已受理（BLOCK=已发完；IT/DMA=已启动，完成看 tx_callback）
 * @retval BSP_BUSY      总线忙（timeout_ms==0 时的常态，上层应自行排队重试）。含两类窗口：
 *                       ① "上一笔的 DMA 流尚未完全释放"——由本层拦下，**不会**让 HAL 报错
 *                          并把句柄锁死（见 bsp_spi.md §2 A.6）；
 *                       ② HAL 启动时判"State 非 READY"而返回的 `HAL_BUSY`——此时 HAL 不动
 *                          State、不置 ErrorCode，是**可重试的争用而不是故障**，故归 BSP_BUSY
 *                          （BLOCK 模式不做就绪预判，这类返回尤其常见）
 * @retval BSP_TIMEOUT   等待就绪超时（此时 bsp 已在任务上下文强制中止卡死的传输并复位状态）
 * @retval BSP_PARAM_ERR 参数非法 / len 为 0 / mode 非法 / 该口没有 TX DMA
 * @retval BSP_HW_ERR    HAL 启动发送失败（真失败：HAL 已置 `ErrorCode` 的 DMA/标志错误，
 *                       或 BLOCK 超时）
 *
 * @note timeout_ms 只在 IT/DMA 模式用于等待上一次传输结束；BLOCK 模式该值直接透传给
 *       HAL_SPI_Transmit（0 表示 HAL 侧立即超时，语义一致）。
 * @note BLOCK 模式内部按 tick 自旋，**禁止在中断上下文调用**；IT/DMA 可在中断中调用，
 *       但中断里必须传 timeout_ms = 0（见 bsp_spi.md）——卡死复位要等 HAL tick，
 *       中断里做不了，改期给 SPIRecoverTxIfStuck。
 * @note tx_callback 内续发是允许的（HAL 已把 State 置回 READY），但不得在回调内调
 *       SPIConfig。
 */
BSP_Status_e SPITransmit(SPIInstance *instance, const uint8_t *data, uint16_t len, BSP_Transfer_Mode_e mode,
                         uint32_t timeout_ms);

/**
 * @brief SPI接收（只收不发，发出去的字节为 0xFF/0x00 由 HAL 决定）
 * @param instance   SPI实例
 * @param len        接收长度（1 ~ buff_size），结果写入 instance->rx_buff
 * @param mode       传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms 等待"总线就绪"的超时（毫秒）；0 = 不等待，忙即返回 BSP_BUSY
 * @retval BSP_OK        已受理（BLOCK=已收到；IT/DMA=已启动，完成看 rx_callback）
 * @retval BSP_BUSY      总线忙（含义同 SPITransmit）/ BLOCK 模式下 HAL 报忙
 * @retval BSP_TIMEOUT   BLOCK 模式等待接收超时；IT/DMA 模式等待就绪超时（已强制复位状态）
 * @retval BSP_PARAM_ERR 参数非法 / len 越界 / mode 非法 / 该口没有 RX DMA
 * @retval BSP_HW_ERR    HAL 启动接收失败（真失败，判定同 SPITransmit）
 *
 * @note len > buff_size 一律**拒绝**（BSP_PARAM_ERR），不做静默截断：截断会让上层拿到的
 *       数据长度与请求不符却毫无察觉。缓冲不够请把 SPI_INSTANCE_DEF 开大。
 * @note 上下文限制同 SPITransmit（BLOCK 禁止在中断里调用；IT/DMA 在中断里传 timeout_ms=0）。
 */
BSP_Status_e SPIReceive(SPIInstance *instance, uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief SPI全双工收发（同一时钟下同时收与发，结果写入 instance->rx_buff）
 * @param instance   SPI实例
 * @param tx_data    发送数据指针（生存期要求同 SPITransmit）
 * @param len        收发长度（1 ~ buff_size，收发同长）
 * @param mode       传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms 等待"总线就绪"的超时（毫秒）；0 = 不等待，忙即返回 BSP_BUSY
 * @retval BSP_OK        已受理（BLOCK=已完成；IT/DMA=已启动，完成看 rx_callback）
 * @retval BSP_BUSY      总线忙（含义同 SPITransmit）
 * @retval BSP_TIMEOUT   BLOCK 模式等待超时；IT/DMA 模式等待就绪超时（已强制复位状态）
 * @retval BSP_PARAM_ERR 参数非法 / len 为 0 或越界 / mode 非法 / 该口缺 TX 或 RX DMA
 * @retval BSP_HW_ERR    HAL 启动收发失败（真失败，判定同 SPITransmit）
 *
 * @note 这是 BMI088 这类"先发寄存器地址再读数据"器件的唯一必需接口。
 * @note 上下文限制同 SPITransmit（BLOCK 禁止在中断里调用；IT/DMA 在中断里传 timeout_ms=0）。
 */
BSP_Status_e SPITransmitReceive(SPIInstance *instance, const uint8_t *tx_data, uint16_t len, BSP_Transfer_Mode_e mode,
                                uint32_t timeout_ms);

/**
 * @brief 传输卡死自恢复（幂等；DRV 在自己任务上下文的读入口调用）
 * @param instance 待检查的 SPI 实例
 * @param stuck_ms 判定"卡死"的时长阈值（ms）：State 连续非 READY 超过它即强制复位；0 = 用默认值
 * @retval BSP_OK        本次刚复位了一次卡死的传输（err_callback 已按契约以 SPI_ERR_ABORT 通知过）
 * @retval BSP_BUSY      未动作：总线空闲（顺带刷新计时基准）/ 还没到阈值 / 上下文不允许 Abort
 * @retval BSP_PARAM_ERR 实例或句柄为空
 *
 * @note 为什么要有这个入口（而不是只靠收发接口内部的复位）：INT 模式下 SPI 传输由 EXTI 发起
 *       （BMI088 的 DRDY 中断），那里**不能**等 HAL tick、也就不能真正 Abort；而 HAL 的
 *       DMA 启动失败分支会把句柄永久留在 BUSY_TX_RX（见 bsp_spi.md），此后每次收发都拿不到
 *       总线、DRV 的 transfer_busy 也清不掉 —— 驱动永久静默且看不到任何错误。
 *       本入口把复位搬到任务上下文：DRV 在自己的读入口（每控制周期必经之处）调一次即可。
 *
 * @note 必须在任务上下文调用（内部经 HAL_DMA_Abort 自旋等 HAL tick，见 SPI_CanBlockingAbort）：
 *       ISR / 临界区里直接返回 BSP_BUSY 且不刷新计时基准，下一次任务上下文的调用照常补上。
 * @note 幂等且开销小：总线空闲时只做一次 DWT 读并刷新基准后返回；只有真的超过阈值才动硬件。
 *       计时基准在 bsp 内按 SPI 维护，上层不必自存时间戳。
 *
 * @note **判据不需要入口自证，而 `I2CBusRecover` / `CANRecover` 需要**：本入口的故障对象与
 *       动作对象是同一个 —— 判据读 `hspi->State` 与本句柄的两路 DMA 流（外设自己的话，
 *       不是上层"我这笔没成"的转述），动作也只作用于这一个句柄，中止的正是判据认定的那笔
 *       卡死传输（正常传输 6~64µs vs 阈值 20ms）。另一类入口则是"判据来自实例、动作落到
 *       整条总线"（重建整个外设 / 取消总线上所有实例的在途帧），作用域不重合，才必须在
 *       bsp 入口自证后早退 `BSP_BUSY`（见 `bsp_i2c.h` 的 `I2CBusRecover` /
 *       `bsp_can.h` 的 `CANRecover`）。
 *       因此 `stuck_ms` 只能收紧时长，改不了判据，也改不了作用范围。
 */
BSP_Status_e SPIRecoverTxIfStuck(SPIInstance *instance, uint32_t stuck_ms);

#endif // HAL_SPI_MODULE_ENABLED

#endif /* __BSP_SPI_H */
