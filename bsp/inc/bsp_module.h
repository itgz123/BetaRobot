/**
 * @file bsp_module.h
 * @brief BSP层板载模块硬件资源索引
 *
 * @note 本文件职责：
 *       1. 定义板载模块的硬件资源索引结构体
 *       2. 声明模块资源索引数组
 *       3. 提供模块与硬件资源的映射关系
 *
 * @note 设计原则：
 *       - 仅定义资源索引，不包含初始化/操作函数
 *       - DRV层通过索引获取硬件资源，实现模块驱动
 *       - 切换开发板只需修改 bsp_module.c 中的映射
 */

#ifndef __BSP_MODULE_H
#define __BSP_MODULE_H

#include "bsp_cfg.h"

/*============================================
 *              BMI088 IMU 模块
 *============================================*/

/**
 * @brief BMI088 硬件资源索引结构体
 * @note BMI088 包含加速度计和陀螺仪两个芯片，分别通过 SPI 访问
 */
typedef struct
{
    BoardSPI_e spi_accel;     // 加速度计 SPI 索引
    BoardSPI_e spi_gyro;      // 陀螺仪 SPI 索引
    BoardGPIO_e cs_accel;     // 加速度计片选引脚
    BoardGPIO_e cs_gyro;      // 陀螺仪片选引脚
    BoardGPIO_e int_accel;    // 加速度计中断引脚
    BoardGPIO_e int_gyro;     // 陀螺仪中断引脚
} BMI088_Resource_t;

/*============================================
 *              QFlash 外部Flash模块
 *============================================*/

/**
 * @brief QFlash 硬件资源索引结构体
 * @note QFlash 使用 OCTOSPI 接口（部分开发板）
 */
typedef struct
{
    uint8_t octospi_instance; // OCTOSPI 实例号 (1 或 2)
    BoardGPIO_e cs_pin;       // 片选引脚
    BoardGPIO_e clk_pin;      // 时钟引脚
    BoardGPIO_e io0_pin;      // 数据线 IO0
    BoardGPIO_e io1_pin;      // 数据线 IO1
    BoardGPIO_e io2_pin;      // 数据线 IO2（可选，用于四线模式）
    BoardGPIO_e io3_pin;      // 数据线 IO3（可选，用于四线模式）
} QFlash_Resource_t;

/*============================================
 *              模块资源索引数组声明
 *============================================*/

/**
 * @brief BMI088 硬件资源索引
 * @note 索引0为第一个BMI088实例
 */
extern const BMI088_Resource_t bmi088_resource[];

/**
 * @brief QFlash 硬件资源索引
 * @note 索引0为第一个QFlash实例
 */
extern const QFlash_Resource_t qflash_resource[];

/*============================================
 *              模块实例数量
 *============================================*/

#if DEVELOPMENT_BOARD == STM32F407VET6
#define BMI088_INSTANCE_NUM 0
#define QFLASH_INSTANCE_NUM 0
#endif

#if DEVELOPMENT_BOARD == DM_MC02
#define BMI088_INSTANCE_NUM 1
#define QFLASH_INSTANCE_NUM 1
#endif

#if DEVELOPMENT_BOARD == DJI_A
#define BMI088_INSTANCE_NUM 0
#define QFLASH_INSTANCE_NUM 0
#endif

#if DEVELOPMENT_BOARD == DJI_C
#define BMI088_INSTANCE_NUM 0
#define QFLASH_INSTANCE_NUM 0
#endif

#endif // __BSP_MODULE_H
