/**
 * @file bsp_i2c.h
 * @brief I2C驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（时钟/时序/地址模式/DMA等）由 CubeMX 负责，BSP 层只管理实例
 * @note 与 SPI 最大的不同：I2C 的"片选"是 7 位从机地址，它由 DRV 层在**每次调用时**
 *       传入（对应 SPI 由 DRV 用 bsp_gpio 管 CS 的分工），不放进实例/Config。
 *       传的是设备地址本身、编码由本层做，见下方「从机地址形态」。
 *       这样才能让一个 I2C 实例服务同一总线上的多个从机，且 handle→instance
 *       查表（回调分发）永远唯一。Config 里只放总线级参数。
 * @note 异步(IT/DMA)模式要求 CubeMX 使能 I2Cx event/error 全局中断并生成
 *       I2Cx_EV_IRQHandler / I2Cx_ER_IRQHandler，否则收不到完成回调。
 *
 * @note 对外接口（与 bsp_usart / bsp_spi 同一套模板）：
 *       I2CRegister（注册，不可重入）→ I2CConfig（配置，可重入）
 *       → I2CMemRead / I2CMemWrite（寄存器读写）与 I2CMasterTransmit / I2CMasterReceive
 *         （裸收发）—— 传输模式（阻塞/IT/DMA）一律**每次调用传参**
 *       → I2CIsDeviceReady（探测从机）/ I2CBusRecover（外设级总线恢复，任务上下文）。
 *       其余一切（work_mode / 函数指针表 / is_ready）都不对外暴露或不实现：
 *       忙由 BSP_BUSY 表达，队列与多缓冲归上层。
 *
 * @note 本层**只做主模式**：不封装 HAL 的 `HAL_I2C_Slave_*` / `HAL_I2C_EnableListen_IT`
 *       一族，也不实现只在从机模式下触发的回调（Addr / SlaveTx / SlaveRx / ListenCplt /
 *       AbortCplt）—— 那些位置由 HAL 的 `__weak` 空实现兜底。板卡侧 `OwnAddress1` 都是 0，
 *       没有任何总线把 MCU 当从机；要这个能力请另起模块（从机是长驻 LISTEN 状态、
 *       方向要到寻址阶段才知道，与这里"一笔一笔收发"的模型不同）。
 *
 * @note 缓冲区与生存期：
 *       - 读：结果固定写入 I2C_INSTANCE_DEF 静态分配的 rx_buff，长度上限 buff_size
 *       - 写：data 指向的缓冲生存期须覆盖到 tx_callback 返回（BLOCK 模式只需覆盖调用期）
 *
 * @note 上下文约束（三模式通用）：
 *       - BLOCK 模式内部按 HAL tick 自旋，**禁止在中断上下文调用**；
 *       - IT/DMA 可在中断中调用，但中断里**必须传 timeout_ms = 0**（忙即 BSP_BUSY）：
 *         中断里等就绪既占着 CPU、又做不了超时后的收尾（见 I2C_WaitReady）。
 *
 * @note 传输互斥是**调用方的责任**：同一实例同一时刻只允许一笔在途传输（BLOCK 亦然）。
 *       本层做到的只是"发起前先判一次总线归属，判不过就返回 BSP_BUSY 且不写实例里的
 *       任何现场"（快照里的 xfer_len 是在途那笔的完成回调填 rx_len 的唯一依据，
 *       收/发缓冲同理）；本层**没有锁**。判完到真正调用 HAL 之间始终存在一个窗口，
 *       两个上下文真并发时，HAL 的 State/Lock 只能保证不重复启动，不保证缓冲与快照
 *       不被覆盖 —— 需要并发就得在上层串行化（如 drv_ist8310 的 transfer_busy / armed）。
 */

#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "bsp_map.h"

#ifdef HAL_I2C_MODULE_ENABLED

#include "bsp_common.h"
#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief err_callback 的触发原因（与 I2CInstance.err_callback 配套）
 * @note 两者都表示"本次传输不会再有完成回调"，上层须在 handler 里复位自身传输状态
 *       （如 IST8310 的 transfer_busy），否则该从机永久失联：
 *       - I2C_ERR_HW：HAL 报硬件错（BERR/ARLO/AF/OVR/DMA…），ISR 上下文；
 *       - I2C_ERR_ABORT：传输没发起成功或已被 bsp 强制收尾（启动失败 / 等就绪超时），
 *         任务上下文（发起调用的那一边）。
 *       上层 handler 必须无阻塞、可重入且**幂等**（判自身状态再复位，重复调用无副作用）。
 */
typedef enum : uint8_t
{
    I2C_ERR_HW = 0,    //!< 硬件错误（BERR/ARLO/AF/OVR/DMA…），HAL_I2C_ErrorCallback 内，ISR 上下文
    I2C_ERR_ABORT = 1, //!< 传输未发起或已被强制收尾，任务上下文，HAL State 已复位为 READY
} I2C_ErrReason_e;

/* 前向声明：下面的回调签名要用到 struct I2CInstance，而结构体本身在后面才定义。
 * 缺了它，`struct I2CInstance *` 会在**函数原型作用域**里另立一个同名 tag，
 * 与后面的正式定义是两个类型 → 赋值回调时 -Wincompatible-pointer-types。 */
struct I2CInstance;

/** 错误回调签名（reason 见 I2C_ErrReason_e） */
typedef void (*I2C_ErrCallback)(struct I2CInstance *instance, I2C_ErrReason_e reason);

/**
 * @brief 从机寄存器地址宽度（对应 HAL 的 MemAddSize）
 * @note 自行定义，避免把 HAL 的 I2C_MEMADD_SIZE_* 直接漏给 DRV
 */
typedef enum : uint8_t
{
    I2C_MEM_ADDR_SIZE_8BIT = 0, // 8 位寄存器地址
    I2C_MEM_ADDR_SIZE_16BIT,    // 16 位寄存器地址
} I2C_MemAddrSize_e;

/**
 * @brief I2C实例结构体
 */
typedef struct I2CInstance
{
    void *parent;                              // 父实例指针（Config 写入，DRV 不再直写）
    BoardI2C_e i2c_e;                          // 板载I2C枚举（Config时查找映射）
    I2C_HandleTypeDef *handle;                 // I2C句柄（Config时自动填充）
    uint8_t *rx_buff;                          // 接收缓冲区指针
    const uint16_t buff_size;                  // 缓冲区大小（编译期固定、只读）
    volatile uint16_t rx_len;                  // 最近一次收到的数据长度（ISR 写、任务读）
                                               // 只由读传输更新；写传输把它清 0
    void (*rx_callback)(struct I2CInstance *); // 读传输完成回调（ISR 上下文）
    void (*tx_callback)(struct I2CInstance *); // 写传输完成回调（ISR 上下文）
    I2C_ErrCallback err_callback;              // 错误回调（ISR 或任务上下文，见 I2C_ErrReason_e）
    // 契约：err_callback 触发后不会再有本次传输的 rx_callback / tx_callback，上层须在此复位
    //       自身状态（如清 transfer_busy）。handler 不得在里面调用 I2CConfig 或发起新传输。
    //       BLOCK 模式不产生任何回调（传输在函数内同步完成），回调只与 IT/DMA 有关。
} I2CInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义I2C实例（同时定义接收缓冲区）
 * @param name     实例名称
 * @param buff_sz  接收缓冲区大小（影响静态内存分配，必须编译期确定）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   I2C_INSTANCE_DEF(ist8310_i2c, IST8310_BUFF_SIZE);
 */
#define I2C_INSTANCE_DEF(name, buff_sz)                   \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0}; \
    static I2CInstance name = {                           \
        .rx_buff = name##_rx_buff,                        \
        .buff_size = buff_sz}

/*------------- 从机地址形态 --------------*/

/**
 * @brief 所有接口的 `dev_addr` 传的是**设备地址本身**，不是 HAL 的 8 位形式
 * @note 编码由本层按句柄的 `Init.AddressingMode` 自己做（与 xrobot/LibXR 的
 *       `EncodeHalDevAddress` 同一约定）：
 *       - 7 位寻址（当前所有板卡）：传 0x00~0x7F，**不含 R/W 位**，如 IST8310 的 0x0E；
 *       - 10 位寻址：传 0x000~0x3FF。
 *       越界一律 `BSP_PARAM_ERR` 拒绝，不做静默截断（7 位寻址下 0x0E 与 0x1C 都能
 *       发出去，只是打到不同从机 —— 这种错必须当场拦住）。
 * @note 旧版要求调用方用 `I2C_DEV_ADDR(addr7)` 宏自己左移成 8 位形式，**那个宏已删除**：
 *       10 位寻址下 HAL 要的恰恰是**未左移**的裸地址，宏会静默寻址错；而忘用宏
 *       （直接传 0x0E）在 7 位寻址下会静默寻址到 0x07 —— 现象只是 NACK，很难反推。
 */

/*------------- 配置结构体 --------------*/

/**
 * @brief I2C 运行时配置结构体（用于 I2CConfig）
 * @note 只含总线级参数：不含从机地址（地址每次调用传参，见文件头注释），
 *       也不含传输模式（模式每次调用传参）。
 */
typedef struct
{
    BoardI2C_e i2c_e;                          // 板载I2C枚举（用于查找硬件映射）
    void *parent;                              // 父实例指针（可为NULL，写入 instance->parent）
    void (*rx_callback)(struct I2CInstance *); // 读传输完成回调（可为NULL）
    void (*tx_callback)(struct I2CInstance *); // 写传输完成回调（可为NULL）
    I2C_ErrCallback err_callback;              // 错误回调（可为NULL，签名见 I2C_ErrReason_e）
} I2C_Config_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册I2C实例（仅调用一次）
 * @param instance I2C实例指针（需先通过宏定义）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 实例为空 / 重复注册 / 超过实例数上限
 *
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不配置硬件参数（由 I2CConfig 负责）。
 */
BSP_Status_e I2CRegister(I2CInstance *instance);

/**
 * @brief 配置I2C实例（可重复调用）
 * @param instance   I2C实例指针
 * @param config     配置结构体指针（i2c_e / parent / 三个回调）
 * @retval BSP_OK 成功
 * @retval BSP_PARAM_ERR 参数非法 / i2c_e 越界 / 该 i2c_e 未映射句柄 / 同一句柄已属其它实例
 *
 * @note 填充硬件句柄与回调，并登记句柄→实例路由表。可重复调用以重新配置。
 *       要求在 I2CRegister 之后调用。
 * @note 重入时会先把上一次可能还挂着的传输收尾（中止在途的 IT/DMA 传输，避免旧传输
 *       继续写缓冲），因此不要在该实例的回调内调用本函数。
 * @note 传输模式不在这里配置：每次收发调用传参（见文件头注释）。
 */
BSP_Status_e I2CConfig(I2CInstance *instance, const I2C_Config_s *config);

/**
 * @brief 读从机寄存器（Mem 模式，HAL 内部完成"写地址→重复起始→读"两段式）
 * @param instance      I2C实例
 * @param dev_addr      从机地址**本身**（7位寻址 0x00~0x7F，如 IST8310 的 0x0E；
 *                      10位寻址 0x000~0x3FF）。编码由本层按句柄的 AddressingMode 做，
 *                      越界 → BSP_PARAM_ERR（见「从机地址形态」）
 * @param mem_addr      寄存器地址
 * @param mem_addr_size 寄存器地址宽度
 * @param len           读取长度（1 ~ buff_size），结果写入 instance->rx_buff
 * @param mode          传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms    超时（毫秒）：BLOCK 模式是 HAL 的传输超时；IT/DMA 模式是等"总线
 *                      就绪"的超时，0 = 不等待（忙即 BSP_BUSY）
 * @retval BSP_OK        已受理（BLOCK=已完成；IT/DMA=已启动，完成看 rx_callback）
 * @retval BSP_BUSY      总线忙：发起前就判出被占用（**BLOCK 模式也会在此返回**，只是它
 *                       不等待、只判一次），或 HAL 启动时判 State 非 READY 而返回
 *                       HAL_BUSY。都是可重试的争用，上层下一拍重试即可
 * @retval BSP_TIMEOUT   超时：等就绪超时，或 HAL 启动后即报超时类错误（`ErrorCode` 带
 *                       TIMEOUT / F4 的 WRONG_START）。此时 bsp 已复位句柄并回调了
 *                       err_callback，上层可直接重试
 * @retval BSP_PARAM_ERR 参数非法 / dev_addr 越界 / len 越界 / mode 非法 / 该口没有 RX DMA
 * @retval BSP_HW_ERR    HAL 启动读取失败（真失败：`ErrorCode` 是 NACK/BERR/DMA…）
 */
BSP_Status_e I2CMemRead(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                        I2C_MemAddrSize_e mem_addr_size, uint16_t len,
                        BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief 写从机寄存器（Mem 模式）
 * @param instance      I2C实例
 * @param dev_addr      从机地址本身（7位寻址 0x00~0x7F；10位寻址 0x000~0x3FF），编码见文件头
 * @param mem_addr      寄存器地址
 * @param mem_addr_size 寄存器地址宽度
 * @param data          待写数据指针（IT/DMA 模式须生存到 tx_callback 返回）
 * @param len           数据长度
 * @param mode          传输模式
 * @param timeout_ms    超时（毫秒），语义同 I2CMemRead
 * @retval BSP_OK        已受理（BLOCK=已完成；IT/DMA=已启动，完成看 tx_callback）
 * @retval 其余          同 I2CMemRead（该口没有 TX DMA 时 BSP_PARAM_ERR）
 */
BSP_Status_e I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                         I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                         BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief 原始发送（不带寄存器地址，对应 SPI 的 SPITransmit）
 * @param instance   I2C实例
 * @param dev_addr   从机地址本身（7位寻址 0x00~0x7F；10位寻址 0x000~0x3FF），编码见文件头
 * @param data       发送数据指针（生存期要求同 I2CMemWrite）
 * @param len        数据长度
 * @param mode       传输模式
 * @param timeout_ms 超时（毫秒），语义同 I2CMemRead
 * @retval BSP_OK 已受理；其余同 I2CMemRead
 *
 * @note 从机地址自身占一笔"数据"，故传给 HAL 的 Size 就是 len（不含地址字节）。
 */
BSP_Status_e I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                               uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief 原始接收（对应 SPI 的 SPIReceive）
 * @param instance   I2C实例
 * @param dev_addr   从机地址本身（7位寻址 0x00~0x7F；10位寻址 0x000~0x3FF），编码见文件头
 * @param len        接收长度（1 ~ buff_size），结果写入 instance->rx_buff
 * @param mode       传输模式
 * @param timeout_ms 超时（毫秒），语义同 I2CMemRead
 * @retval BSP_OK 已受理；其余同 I2CMemRead
 */
BSP_Status_e I2CMasterReceive(I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                              BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/**
 * @brief 探测从机是否应答
 * @param instance   I2C实例
 * @param dev_addr   从机地址本身（7位寻址 0x00~0x7F；10位寻址 0x000~0x3FF），编码见文件头
 * @param trials     重试次数
 * @param timeout_ms 单次尝试超时（毫秒）
 * @retval BSP_OK        从机应答
 * @retval BSP_BUSY      总线被占用（HAL 判 State 非 READY 或 BUSY 标志未落）
 * @retval BSP_HW_ERR    从机无应答（NACK）/ 其它 HAL 错误（含 HAL 自己报的 HAL_TIMEOUT：
 *                       探测本身超时，此时还没探出从机在不在）
 * @retval BSP_PARAM_ERR 参数非法 / dev_addr 越界
 *
 * @note 内部为阻塞轮询且用 HAL_GetTick 计时，**只能在任务上下文调用**，不能在中断里用。
 * @note 入口要求 State==READY 且总线 BUSY 标志为 0，因此可用来判断"总线是否真被拉死"。
 * @note 每次调用的结论都计入 `s_i2c_status[]` 的 `probe_ok` / `probe_fail` / `probe_busy`
 *       （见 bsp_i2c.md §1.5）：恢复流程里"器件不应答"与"总线没让出来"要靠它们区分。
 */
BSP_Status_e I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                              uint32_t timeout_ms);

/**
 * @brief 外设级总线恢复：重建 I2C 外设 + 复位 State/ErrorCode/Lock
 * @param instance I2C实例
 * @retval BSP_OK        本次刚做完全套重建，且总线 BUSY 标志已落（可用）
 * @retval BSP_BUSY      未做任何事：入口自证不成立（非任务上下文 / 总线 BUSY 标志已落 /
 *                       本句柄有在途传输）。计入 `s_i2c_status[].bus_recover_skip`
 * @retval BSP_HW_ERR    重建后仍 BUSY（多半 SCL/SDA 被从机拉死，需从机侧复位；本层不翻转引脚）
 * @retval BSP_PARAM_ERR 实例或句柄为空
 *
 * @note 实现 = HAL_I2C_DeInit + HAL_I2C_Init（Init 复用 hi2c->Init 原值重配，
 *       F4 的 ClockSpeed/DutyCycle 与 H7 的 Timing 差异自动抹平）+ 强制复位句柄字段。
 *       含 MspDeInit/MspInit，涉及时钟与 GPIO/NVIC 重配，只能在任务上下文调用。
 *
 * @note **入口自证（先判、后动）**：本函数是**总线级**动作（重建整个外设），而触发者看到的是
 *       **实例级**现象（"我这个从机没应答"）。证据的作用域必须与动作的作用域对齐，故判据在
 *       本函数内部、且在任何动作之前读：
 *         ① `I2C_FLAG_BUSY` 置位（外设说总线被占）
 *         ② 本句柄空闲（`State == READY && Lock` 未锁）—— 即"占着总线的那一位"不是任何一笔
 *            在途传输（正常传输同样让 BUSY 置位，单独看①会在传输途中误重建）
 *       两条同时成立才动手，否则 `BSP_BUSY` 返回、什么都不做：总线已放开时的失败（NACK / 从机挂了）
 *       重建外设治不了，该由 DRV 的探测 + RSTN 脉冲 + 整段重初始化处理。
 *       **调用点只管"何时看一眼"（自己的失败计数与冷却），bsp 只管"值不值得动手"** ——
 *       与 `SPIRecoverTxIfStuck`（bsp_spi.h）同一条原则，逐条取舍见 bsp_i2c.md §2.B.1.1。
 */
BSP_Status_e I2CBusRecover(I2CInstance *instance);

#endif // HAL_I2C_MODULE_ENABLED

#endif /* __BSP_I2C_H */
