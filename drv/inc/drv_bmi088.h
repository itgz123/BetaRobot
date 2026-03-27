/**
 * @file drv_bmi088.h
 * @brief BMI088 六轴 IMU 驱动（加速度计 + 陀螺仪）
 *
 * @note 使用 DMA 非阻塞模式进行 SPI 数据读取
 * @note 片选由 DRV 层通过 GPIO 接口控制
 *
 */

#ifndef __DRV_BMI088_H
#define __DRV_BMI088_H

#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_tim.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include <math.h>

/*============================ 寄存器地址定义 ============================*/

/*------- 加速度计寄存器地址 -------*/
#define BMI088_ACC_CHIP_ID_REG 0x00       // Who Am I 寄存器地址
#define BMI088_ACC_ERR_REG 0x02           // 错误标志寄存器
#define BMI088_ACC_STATUS_REG 0x03        // 状态寄存器
#define BMI088_ACC_INT_STAT_1_REG 0x1D    // 中断状态寄存器 1
#define BMI088_ACCEL_XOUT_L 0x12          // X 轴加速度低字节
#define BMI088_ACCEL_XOUT_H 0x13          // X 轴加速度高字节
#define BMI088_ACCEL_YOUT_L 0x14          // Y 轴加速度低字节
#define BMI088_ACCEL_YOUT_H 0x15          // Y 轴加速度高字节
#define BMI088_ACCEL_ZOUT_L 0x16          // Z 轴加速度低字节
#define BMI088_ACCEL_ZOUT_H 0x17          // Z 轴加速度高字节
#define BMI088_SENSORTIME_0_REG 0x18      // 传感器时间低字节
#define BMI088_SENSORTIME_1_REG 0x19      // 传感器时间中字节
#define BMI088_SENSORTIME_2_REG 0x1A      // 传感器时间高字节
#define BMI088_TEMP_L 0x23                // 温度数据低字节
#define BMI088_TEMP_M 0x22                // 温度数据高字节
#define BMI088_ACC_FIFO_LENGTH_L 0x24     // FIFO 数据长度低字节
#define BMI088_ACC_FIFO_LENGTH_H 0x25     // FIFO 数据长度高字节
#define BMI088_ACC_FIFO_DATA_REG 0x26     // FIFO 数据寄存器
#define BMI088_ACC_CONF_REG 0x40          // 加速度计配置寄存器
#define BMI088_ACC_RANGE_REG 0x41         // 量程配置寄存器
#define BMI088_ACC_FIFO_DOWNS_REG 0x45    // FIFO 降采样配置寄存器
#define BMI088_ACC_FIFO_WTM_L 0x46        // FIFO 水印低字节
#define BMI088_ACC_FIFO_WTM_H 0x47        // FIFO 水印高字节
#define BMI088_ACC_FIFO_CONFIG_0_REG 0x48 // FIFO 配置寄存器 0
#define BMI088_ACC_FIFO_CONFIG_1_REG 0x49 // FIFO 配置寄存器 1
#define BMI088_INT1_IO_CTRL_REG 0x53      // INT1 引脚配置寄存器
#define BMI088_INT2_IO_CTRL_REG 0x54      // INT2 引脚配置寄存器
#define BMI088_INT_MAP_DATA_REG 0x58      // 中断映射寄存器
#define BMI088_ACC_SELF_TEST_REG 0x6D     // 自检寄存器
#define BMI088_ACC_PWR_CONF_REG 0x7C      // 电源配置寄存器
#define BMI088_ACC_PWR_CTRL_REG 0x7D      // 电源控制寄存器
#define BMI088_ACC_SOFTRESET_REG 0x7E     // 软复位寄存器

/*------- 陀螺仪寄存器地址 -------*/
#define BMI088_GYRO_CHIP_ID_REG 0x00           // Who Am I 寄存器地址
#define BMI088_GYRO_X_L 0x02                   // X 轴角速度低字节
#define BMI088_GYRO_X_H 0x03                   // X 轴角速度高字节
#define BMI088_GYRO_Y_L 0x04                   // Y 轴角速度低字节
#define BMI088_GYRO_Y_H 0x05                   // Y 轴角速度高字节
#define BMI088_GYRO_Z_L 0x06                   // Z 轴角速度低字节
#define BMI088_GYRO_Z_H 0x07                   // Z 轴角速度高字节
#define BMI088_GYRO_INT_STAT_1 0x0A            // 中断状态寄存器 1
#define BMI088_GYRO_FIFO_STATUS_REG 0x0E       // FIFO 状态寄存器
#define BMI088_GYRO_RANGE_REG 0x0F             // 量程配置寄存器
#define BMI088_GYRO_BANDWIDTH_REG 0x10         // 带宽配置寄存器
#define BMI088_GYRO_LPM1_REG 0x11              // 低功耗模式配置寄存器
#define BMI088_GYRO_SOFTRESET_REG 0x14         // 软复位寄存器
#define BMI088_GYRO_INT_CTRL_REG 0x15          // 中断控制寄存器
#define BMI088_GYRO_INT3_INT4_IO_CONF_REG 0x16 // INT3/INT4 引脚配置寄存器
#define BMI088_GYRO_INT3_INT4_IO_MAP_REG 0x18  // INT3/INT4 中断映射寄存器
#define BMI088_GYRO_FIFO_WM_EN_REG 0x1E        // FIFO 水印使能寄存器
#define BMI088_GYRO_FIFO_EXT_INT_S_REG 0x34    // FIFO 外部中断选择寄存器
#define BMI088_GYRO_SELF_TEST_REG 0x3C         // 自检寄存器
#define BMI088_GYRO_FIFO_CONFIG_0_REG 0x3D     // FIFO 配置寄存器 0
#define BMI088_GYRO_FIFO_CONFIG_1_REG 0x3E     // FIFO 配置寄存器 1
#define BMI088_GYRO_FIFO_DATA_REG 0x3F         // FIFO 数据寄存器

/**
 * @brief IMU 数据结构体
 */
typedef struct
{
    float gyro[3]; // 陀螺仪数据 (rad/s)
    float acc[3];  // 加速度计数据 (m/s²)
    float temp;    // 温度 (°C)
    time
} BMI088_Data_t;

/**
 * @brief BMI088 实例结构体
 * @note 内嵌 BSP 实例，使用 container_of 获取父指针
 */
typedef struct BMI088Instance
{
    /* BSP 实例 */
    SPIInstance spi_inst;   // SPI 实例
    GPIOInstance cs_acc;    // 加速度计片选
    GPIOInstance cs_gyro;   // 陀螺仪片选
    GPIOInstance int_acc;   // 加速度计中断
    GPIOInstance int_gyro;  // 陀螺仪中断（可选）
    PWMInstance heater_pwm; // 加热 PWM（可选）

} BMI088Instance;

/*============================ 公开接口声明 ============================*/

#endif /* __DRV_BMI088_H */
