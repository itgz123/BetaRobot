/**
 * @file bsp_i2c.h
 * @brief I2C驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（时钟/时序/地址模式/DMA等）由 CubeMX 负责，BSP 层只管理实例
 * @note 与 SPI 最大的不同：I2C 的"片选"是 7 位从机地址，它由 DRV 层在**每次调用时**
 *       传入（对应 SPI 由 DRV 用 bsp_gpio 管 CS 的分工），不放进实例/Config。
 *       这样才能让一个 I2C 实例服务同一总线上的多个从机，且 handle→instance
 *       查表（回调分发）永远唯一。Config 里只放总线级参数。
 * @note 异步(IT/DMA)模式要求 CubeMX 使能 I2Cx event/error 全局中断并生成
 *       I2Cx_EV_IRQHandler / I2Cx_ER_IRQHandler，否则收不到完成回调。
 */

#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "bsp_map.h"

#ifdef HAL_I2C_MODULE_ENABLED

#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief I2C工作模式枚举
 */
typedef enum : uint8_t
{
    I2C_BLOCK_MODE = 0, // 阻塞模式
    I2C_IT_MODE,        // 中断模式
    I2C_DMA_MODE,       // DMA模式（需 CubeMX 配 DMA，且缓冲在 DMA_RAM）
} I2C_Work_Mode_e;

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
    void *parent;                               // 父实例指针（由 DRV 层设置）
    BoardI2C_e i2c_e;                           // 板载I2C枚举（Config时查找映射）
    I2C_HandleTypeDef *handle;                  // I2C句柄（Config时自动填充）
    I2C_Work_Mode_e work_mode;                  // 工作模式
    uint8_t *rx_buff;                           // 接收缓冲区指针
    const uint16_t buff_size;                   // 缓冲区大小（编译期固定、只读）
    uint16_t rx_len;                            // 最近一次接收数据长度
    uint16_t last_xfer_len;                     // 最近一次收发请求长度
    uint16_t last_dev_addr;                     // 最近一次从机地址（8bit形式，仅调试/日志）
    uint16_t last_mem_addr;                     // 最近一次寄存器地址（仅调试/日志）
    void (*rx_callback)(struct I2CInstance *);  // 传输完成回调
    void (*err_callback)(struct I2CInstance *); // 传输错误回调（启动失败/忙超时/HAL报错）
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

/*------------- 从机地址辅助宏 --------------*/

/**
 * @brief 7位从机地址 → HAL 需要的 8位形式（左移1位）
 * @example I2CMemRead(inst, I2C_DEV_ADDR(0x0E), reg, ...);
 */
#define I2C_DEV_ADDR(addr7) ((uint16_t)((uint16_t)(addr7) << 1))

/*------------- 配置结构体 --------------*/

/**
 * @brief I2C 运行时配置结构体（用于 I2CConfig）
 * @note 只含总线级参数，不含从机地址（地址每次调用传参，见文件头注释）
 */
typedef struct
{
    BoardI2C_e i2c_e;                           // 板载I2C枚举（用于查找硬件映射）
    I2C_Work_Mode_e work_mode;                  // 工作模式（阻塞/中断/DMA）
    void (*rx_callback)(struct I2CInstance *);  // 传输完成回调（可为NULL）
    void (*err_callback)(struct I2CInstance *); // 传输错误回调（可为NULL）
} I2C_Config_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册I2C实例（仅调用一次）
 * @param instance I2C实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限或参数无效）
 *
 * @note 仅检查参数、防重后加入 static 管理数组。
 *       不配置硬件参数（由 I2CConfig 负责）。
 */
int8_t I2CRegister(I2CInstance *instance);

/**
 * @brief 配置I2C实例（可重复调用）
 * @param instance   I2C实例指针
 * @param config     配置结构体指针（i2c_e/工作模式/回调）
 * @retval 0 成功
 * @retval -1 失败（参数非法）
 *
 * @note 填充硬件句柄，设置工作模式和回调，不修改 static 管理数组。
 *       可重复调用以重新配置（例如在阻塞/中断模式间切换，只改实例字段、不碰硬件）。
 *       要求在 I2CRegister 之后调用。
 */
int8_t I2CConfig(I2CInstance *instance, const I2C_Config_s *config);

/**
 * @brief 读从机寄存器（Mem 模式，HAL 内部完成"写地址→重复起始→读"两段式）
 * @param instance      I2C实例
 * @param dev_addr      从机地址（8位形式，用 I2C_DEV_ADDR() 从7位地址换算）
 * @param mem_addr      寄存器地址
 * @param mem_addr_size 寄存器地址宽度
 * @param len           读取长度，不得超过实例 buff_size；结果写入 instance->rx_buff
 * @param timeout_ms    超时时间（毫秒）
 * @retval 0 已受理（阻塞模式=已完成；IT/DMA 模式=已启动，完成看 rx_callback）
 * @retval -1 参数非法、等待就绪超时或 HAL 启动失败（此时 err_callback 已被调用）
 */
int8_t I2CMemRead(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                  I2C_MemAddrSize_e mem_addr_size, uint16_t len, uint32_t timeout_ms);

/**
 * @brief 写从机寄存器（Mem 模式）
 * @param instance      I2C实例
 * @param dev_addr      从机地址（8位形式）
 * @param mem_addr      寄存器地址
 * @param mem_addr_size 寄存器地址宽度
 * @param data          待写数据指针
 * @param len           数据长度
 * @param timeout_ms    超时时间（毫秒）
 * @retval 0 已受理；-1 失败（同 I2CMemRead）
 */
int8_t I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                   I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                   uint32_t timeout_ms);

/**
 * @brief 原始发送（不带寄存器地址，对应 SPI 的 SPITransmit）
 * @param instance   I2C实例
 * @param dev_addr   从机地址（8位形式）
 * @param data       发送数据指针（外部缓冲区）
 * @param len        数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval 0 已受理；-1 失败
 */
int8_t I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                         uint16_t len, uint32_t timeout_ms);

/**
 * @brief 原始接收（对应 SPI 的 SPIReceive）
 * @param instance   I2C实例
 * @param dev_addr   从机地址（8位形式）
 * @param len        数据长度，不超过 buff_size；结果写入 instance->rx_buff
 * @param timeout_ms 超时时间（毫秒）
 * @retval 0 已受理；-1 失败
 */
int8_t I2CMasterReceive(I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                        uint32_t timeout_ms);

/**
 * @brief 探测从机是否应答
 * @param instance   I2C实例
 * @param dev_addr   从机地址（8位形式）
 * @param trials     重试次数
 * @param timeout_ms 单次尝试超时（毫秒）
 * @retval 0 从机应答
 * @retval -1 无应答/总线忙/参数非法
 *
 * @note 内部为阻塞轮询且用 HAL_GetTick 计时，只能在任务上下文调用，不能在中断里用。
 * @note 入口要求 State==READY 且总线 BUSY 标志为 0，因此可用来判断"总线是否真被拉死"。
 */
int8_t I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                        uint32_t timeout_ms);

/**
 * @brief 外设级总线恢复：重建 I2C 外设 + 复位 State/ErrorCode/Lock
 * @param instance I2C实例
 * @retval 0 重建后总线 BUSY 标志为 0（可用）
 * @retval -1 仍 BUSY（多半 SCL/SDA 被从机拉死，需从机侧复位；本层不翻转引脚）
 *
 * @note 实现 = HAL_I2C_DeInit + HAL_I2C_Init（Init 复用 hi2c->Init 原值重配，
 *       F4 的 ClockSpeed/DutyCycle 与 H7 的 Timing 差异自动抹平）+ 强制复位句柄字段。
 *       含 MspDeInit/MspInit，涉及时钟与 GPIO/NVIC 重配，只能在任务上下文调用。
 */
int8_t I2CBusRecover(I2CInstance *instance);

#endif // HAL_I2C_MODULE_ENABLED

#endif /* __BSP_I2C_H */
