/**
 * @file bsp_spi.h
 * @brief SPI驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置（时钟极性/相位/波特率/DMA等）由 CubeMX 负责，BSP 层只管理实例
 * @note 片选控制由 DRV 层通过 GPIO 接口管理，BSP 层不负责
 */

#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "bsp_cfg.h"

#if SPI_INSTANCE_NUM > 0

#include "main.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief SPI工作模式枚举
 */
typedef enum
{
    SPI_BLOCK_MODE = 0, // 阻塞模式
    SPI_IT_MODE,        // 中断模式
    SPI_DMA_MODE,       // DMA模式
} SPI_Work_Mode_e;

/**
 * @brief SPI实例结构体
 */
typedef struct SPIInstance
{
    void *parent;                              // 父实例指针（由 DRV 层设置）
    BoardSPI_e spi_e;                          // 板载SPI枚举（注册时用于查找映射）
    SPI_HandleTypeDef *handle;                 // SPI句柄（注册时自动填充）
    SPI_Work_Mode_e work_mode;                 // 工作模式
    uint8_t *rx_buff;                          // 接收缓冲区指针
    uint16_t buff_size;                        // 缓冲区大小
    uint16_t rx_len;                           // 接收数据长度
    void (*rx_callback)(struct SPIInstance *); // DMA接收完成回调
} SPIInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义SPI实例（同时定义接收缓冲区）
 * @param name    实例名称
 * @param spi_idx 板载SPI枚举（BoardSPI_e）
 * @param mode    工作模式（SPI_Work_Mode_e）
 * @param buff_sz 接收缓冲区大小
 * @param rx_cb   接收完成回调函数（可为NULL，仅DMA/IT模式有效）
 *
 * @note DMA_RAM 宏在 Cortex-M7 上将缓冲区放入 RAM_D1 以支持 DMA 访问
 *       在 Cortex-M4 上定义为空
 *
 * @example
 *   SPI_INSTANCE_DEF(bmi088_spi, SPI_BMI088, SPI_DMA_MODE, 64, rx_callback);
 */
#define SPI_INSTANCE_DEF(name, spi_idx, mode, buff_sz, rx_cb) \
    static uint8_t name##_rx_buff[buff_sz] DMA_RAM = {0};     \
    static SPIInstance name = {                               \
        .parent = NULL,                                       \
        .spi_e = spi_idx,                                     \
        .handle = NULL,                                       \
        .work_mode = mode,                                    \
        .rx_buff = name##_rx_buff,                            \
        .buff_size = buff_sz,                                 \
        .rx_len = 0,                                          \
        .rx_callback = rx_cb}

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册SPI实例
 * @param instance SPI实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败（实例数超过上限或参数无效）
 */
int8_t SPIRegister(SPIInstance *instance);

/**
 * @brief SPI发送数据
 * @param instance SPI实例
 * @param data     发送数据指针（外部缓冲区）
 * @param len      数据长度
 * @note 阻塞模式：函数返回后数据传输完成
 *       IT/DMA模式：通过回调通知传输完成
 */
void SPITransmit(SPIInstance *instance, uint8_t *data, uint16_t len);

/**
 * @brief SPI接收数据
 * @param instance SPI实例
 * @param len      数据长度（不超过buff_size）
 * @note 使用实例中的 rx_buff 接收数据
 *       接收完成后 rx_len 保存实际接收长度
 */
void SPIReceive(SPIInstance *instance, uint16_t len);

/**
 * @brief SPI全双工收发
 * @param instance SPI实例
 * @param tx_data  发送数据指针（外部缓冲区）
 * @param len      数据长度（不超过buff_size）
 * @note 发送使用外部缓冲区 tx_data，接收使用内部缓冲区 rx_buff
 *       DMA/IT模式：通过回调通知传输完成
 */
void SPITransmitReceive(SPIInstance *instance, uint8_t *tx_data, uint16_t len);

#endif

#endif /* __BSP_SPI_H */
