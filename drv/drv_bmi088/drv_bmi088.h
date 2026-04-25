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

#include "main.h"
#include "bsp_cfg.h"

#if defined(BSP_SPI_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED)

#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_tim.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "bmi088_reg_def.h"

/**
 * @brief BMI088 工作模式枚举
 */
typedef enum
{
    BMI088_MODE_POLLING = 0, // 阻塞模式：主动轮询读取6轴数据
    BMI088_MODE_INT,         // 中断模式：数据就绪后中断通知主控
} BMI088_WorkMode_e;

/*============================ 缓冲区大小定义 ============================*/

#define BMI088_BUFF_SIZE 8 // SPI缓冲区大小：1地址 + 1虚拟 + 6数据（最大）

/**
 * @brief IMU 数据结构体
 */
typedef struct
{
    float gyro[3];    // 陀螺仪数据 (rad/s)
    float acc[3];     // 加速度计数据 (m/s²)
    float temp;       // 温度 (°C)
    float time_stamp; // 时间戳 (ms)
} BMI088_Data_t;

/**
 * @brief BMI088 实例结构体
 * @note 使用指针指向 BSP 实例，在注册时设置 parent
 */
typedef struct BMI088Instance
{
    /* BSP 实例指针 */
    SPIInstance *spi_inst;   // SPI 实例
    GPIOInstance *cs_acc;    // 加速度计片选
    GPIOInstance *cs_gyro;   // 陀螺仪片选
    GPIOInstance *int_acc;   // 加速度计中断
    GPIOInstance *int_gyro;  // 陀螺仪中断（可选）
    PWMInstance *heater_pwm; // 加热 PWM（可选）

    /* 发送缓冲区 */
    uint8_t *tx_buff; // 发送缓冲区指针
    uint8_t tx_len;   // 发送数据长度

    /* 加速度计配置 */
    BMI088_AccRange_e acc_range; // 量程
    uint8_t acc_bwp;             // 低通滤波器带宽
    uint8_t acc_odr;             // 输出数据速率

    /* 陀螺仪配置 */
    BMI088_GyroRange_e gyro_range; // 量程
    uint8_t gyro_odr;              // 输出数据速率
    uint8_t gyro_bw;               // 滤波器带宽

    /* 工作模式 */
    BMI088_WorkMode_e work_mode; // 工作模式

    /* 标定偏移量 */
    float acc_offset[3];  // 加速度计零偏 (m/s²)
    float gyro_offset[3]; // 陀螺仪零偏 (rad/s)

} BMI088Instance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief BMI088实例静态定义宏
 * @param name        实例名称
 * @param spi_idx     板载SPI枚举
 * @param cs_acc_idx  加速度计片选GPIO枚举
 * @param cs_gyro_idx 陀螺仪片选GPIO枚举
 * @param int_acc_idx 加速度计中断GPIO枚举
 * @param int_gyro_idx 陀螺仪中断GPIO枚举（无则填0）
 * @param heater_idx  加热PWM枚举（无则填0）
 *
 * @note 使用 BSP 层的实例定义宏，parent 在注册时设置
 *       初始化时默认使用阻塞模式，初始化完成后可切换到DMA模式
 *
 * @example
 *   BMI088_INSTANCE_DEF(bmi088, SPI_BMI088_2, GPIO_BMI088_CS_ACCEL, GPIO_BMI088_CS_GYRO,
 *                       GPIO_BMI088_INT_ACCEL, GPIO_BMI088_INT_GYRO, TIM_HEATER);
 */
#define BMI088_INSTANCE_DEF(name, spi_idx, cs_acc_idx, cs_gyro_idx, \
                            int_acc_idx, int_gyro_idx, heater_idx)  \
    static uint8_t name##_tx_buff[BMI088_BUFF_SIZE] DMA_RAM = {0};  \
    SPI_INSTANCE_DEF(name##_spi, spi_idx, SPI_BLOCK_MODE, BMI088_BUFF_SIZE, NULL, NULL); \
    GPIO_INSTANCE_DEF(name##_cs_acc, cs_acc_idx, NULL, NULL);       \
    GPIO_INSTANCE_DEF(name##_cs_gyro, cs_gyro_idx, NULL, NULL);     \
    GPIO_INSTANCE_DEF(name##_int_acc, int_acc_idx, NULL, NULL);     \
    GPIO_INSTANCE_DEF(name##_int_gyro, int_gyro_idx, NULL, NULL);   \
    PWM_INSTANCE_DEF(name##_heater, heater_idx);                    \
    static BMI088Instance name = {                                  \
        .spi_inst = &name##_spi,                                    \
        .cs_acc = &name##_cs_acc,                                   \
        .cs_gyro = &name##_cs_gyro,                                 \
        .int_acc = &name##_int_acc,                                 \
        .int_gyro = &name##_int_gyro,                               \
        .heater_pwm = &name##_heater,                               \
        .tx_buff = name##_tx_buff,                                  \
        .tx_len = 0,                                                \
        .acc_offset = {0.0f, 0.0f, 0.0f},                           \
        .gyro_offset = {0.0f, 0.0f, 0.0f},                          \
    }

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册BMI088实例
 * @param inst BMI088实例指针
 * @return 0成功，-1失败
 */
int8_t BMI088Register(BMI088Instance *inst);

/**
 * @brief 设置BMI088配置并写入寄存器
 * @param inst       BMI088实例指针
 * @param acc_range  加速度计量程
 * @param acc_bwp    加速度计低通滤波器带宽
 * @param acc_odr    加速度计输出数据速率
 * @param gyro_range 陀螺仪量程
 * @param gyro_odr   陀螺仪输出数据速率
 * @param gyro_bw    陀螺仪滤波器带宽
 * @param work_mode  工作模式
 * @return 0成功，-1失败
 */
int8_t BMI088SetConfig(BMI088Instance *inst, BMI088_AccRange_e acc_range, uint8_t acc_bwp, uint8_t acc_odr, BMI088_GyroRange_e gyro_range, uint8_t gyro_odr, uint8_t gyro_bw, BMI088_WorkMode_e work_mode);

/**
 * @brief 阻塞读取BMI088数据
 * @param inst BMI088实例指针
 * @return BMI088_Data_t 结构体，包含加速度、陀螺仪、温度和时间戳
 * @note 此函数会阻塞等待数据读取完成
 */
BMI088_Data_t BMI088ReadBlocking(BMI088Instance *inst);

/**
 * @brief 标定BMI088零偏
 * @param inst    BMI088实例指针
 * @param samples 采样次数（建议100~200）
 * @return 0成功，-1失败
 * @note 调用时传感器应处于静止状态
 */
int8_t BMI088Calibrate(BMI088Instance *inst, uint16_t samples);

/**
 * @brief 设置加热PWM占空比
 * @param inst        BMI088实例指针
 * @param duty_ratio  占空比（0~1）
 * @note 阻塞模式直接设置，中断模式预留PID控制接口
 */
void BMI088SetHeater(BMI088Instance *inst, float duty_ratio);

#endif /* defined(BSP_SPI_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED) */

#endif /* __DRV_BMI088_H */