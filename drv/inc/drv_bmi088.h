/**
 * @file drv_bmi088.h
 * @brief BMI088 六轴 IMU 驱动（加速度计 + 陀螺仪）
 *
 * @note 使用 DMA 非阻塞模式进行 SPI 数据读取
 * @note 片选由 DRV 层通过 GPIO 接口控制
 */

#ifndef __DRV_BMI088_H
#define __DRV_BMI088_H

#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_tim.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include <math.h>

/*============================ 寄存器定义 ============================*/

/*------- 加速度计寄存器地址 -------*/
#define BMI088_ACC_CHIP_ID 0x00       // Who Am I 寄存器地址
#define BMI088_ACC_CHIP_ID_VALUE 0x1E // Who Am I 期望值

#define BMI088_ACC_ERR_REG 0x02    // 错误标志寄存器
#define BMI088_ACC_STATUS 0x03     // 状态寄存器
#define BMI088_ACCEL_DRDY (1 << 7) // 数据就绪标志位

#define BMI088_ACCEL_XOUT_L 0x12 // X轴加速度低字节
#define BMI088_ACCEL_XOUT_H 0x13 // X轴加速度高字节
#define BMI088_ACCEL_YOUT_L 0x14 // Y轴加速度低字节
#define BMI088_ACCEL_YOUT_H 0x15 // Y轴加速度高字节
#define BMI088_ACCEL_ZOUT_L 0x16 // Z轴加速度低字节
#define BMI088_ACCEL_ZOUT_H 0x17 // Z轴加速度高字节

#define BMI088_TEMP_M 0x22 // 温度数据高字节
#define BMI088_TEMP_L 0x23 // 温度数据低字节

#define BMI088_ACC_CONF 0x40          // 加速度计配置寄存器
#define BMI088_ACC_CONF_MUST_SET 0x80 // 配置寄存器必须设置的位
#define BMI088_ACC_NORMAL (0x02 << 4) // 正常工作模式
#define BMI088_ACC_800_HZ (0x0B << 0) // 800Hz 输出数据率

#define BMI088_ACC_RANGE 0x41     // 量程配置寄存器
#define BMI088_ACC_RANGE_3G 0x00  // ±3g 量程
#define BMI088_ACC_RANGE_6G 0x01  // ±6g 量程
#define BMI088_ACC_RANGE_12G 0x02 // ±12g 量程
#define BMI088_ACC_RANGE_24G 0x03 // ±24g 量程

#define BMI088_INT1_IO_CTRL 0x53       // INT1 引脚配置寄存器
#define BMI088_INT1_IO_ENABLE (1 << 3) // INT1 输出使能
#define BMI088_INT1_GPIO_PP (0 << 2)   // 推挽输出模式
#define BMI088_INT1_GPIO_LOW (0 << 1)  // 低电平有效

#define BMI088_INT_MAP_DATA 0x58            // 中断映射寄存器
#define BMI088_INT1_DRDY_INTERRUPT (1 << 2) // 数据就绪中断映射到 INT1

#define BMI088_ACC_PWR_CONF 0x7C    // 电源配置寄存器
#define BMI088_ACC_PWR_ACTIVE 0x00  // 激活模式
#define BMI088_ACC_PWR_SUSPEND 0x03 // 挂起模式

#define BMI088_ACC_PWR_CTRL 0x7D // 电源控制寄存器
#define BMI088_ACC_ENABLE 0x04   // 加速度计使能
#define BMI088_ACC_DISABLE 0x00  // 加速度计禁用

#define BMI088_ACC_SOFTRESET 0x7E       // 软复位寄存器
#define BMI088_ACC_SOFTRESET_VALUE 0xB6 // 软复位命令值

/*------- 陀螺仪寄存器地址 -------*/
#define BMI088_GYRO_CHIP_ID 0x00       // Who Am I 寄存器地址
#define BMI088_GYRO_CHIP_ID_VALUE 0x0F // Who Am I 期望值

#define BMI088_GYRO_X_L 0x02 // X轴角速度低字节
#define BMI088_GYRO_X_H 0x03 // X轴角速度高字节
#define BMI088_GYRO_Y_L 0x04 // Y轴角速度低字节
#define BMI088_GYRO_Y_H 0x05 // Y轴角速度高字节
#define BMI088_GYRO_Z_L 0x06 // Z轴角速度低字节
#define BMI088_GYRO_Z_H 0x07 // Z轴角速度高字节

#define BMI088_GYRO_INT_STAT_1 0x0A // 中断状态寄存器
#define BMI088_GYRO_DRDY (1 << 7)   // 数据就绪标志位

#define BMI088_GYRO_RANGE 0x0F // 量程配置寄存器
#define BMI088_GYRO_2000 0x00  // ±2000°/s 量程
#define BMI088_GYRO_1000 0x01  // ±1000°/s 量程
#define BMI088_GYRO_500 0x02   // ±500°/s 量程
#define BMI088_GYRO_250 0x03   // ±250°/s 量程
#define BMI088_GYRO_125 0x04   // ±125°/s 量程

#define BMI088_GYRO_BANDWIDTH 0x10   // 带宽配置寄存器
#define BMI088_GYRO_BW_MUST_SET 0x80 // 带宽寄存器必须设置的位
#define BMI088_GYRO_2000_230HZ 0x01  // 2000Hz ODR, 230Hz 带宽
#define BMI088_GYRO_1000_116HZ 0x02  // 1000Hz ODR, 116Hz 带宽

#define BMI088_GYRO_LPM1 0x11         // 低功耗模式配置寄存器
#define BMI088_GYRO_NORMAL_MODE 0x00  // 正常工作模式
#define BMI088_GYRO_SUSPEND_MODE 0x80 // 挂起模式

#define BMI088_GYRO_SOFTRESET 0x14       // 软复位寄存器
#define BMI088_GYRO_SOFTRESET_VALUE 0xB6 // 软复位命令值

#define BMI088_GYRO_CTRL 0x15 // 控制寄存器
#define BMI088_DRDY_ON 0x80   // 数据就绪中断使能

#define BMI088_GYRO_INT3_INT4_IO_CONF 0x16 // INT3/INT4 引脚配置寄存器
#define BMI088_GYRO_INT3_PP (0 << 1)       // INT3 推挽输出模式
#define BMI088_GYRO_INT3_LOW (0 << 0)      // INT3 低电平有效

#define BMI088_GYRO_INT3_INT4_IO_MAP 0x18 // 中断映射寄存器
#define BMI088_DRDY_IO_INT3 0x01          // 数据就绪中断映射到 INT3

/*------- 灵敏度系数 -------*/
/* 加速度计灵敏度：将原始值转换为 g，单位 g/LSB */
#define BMI088_ACCEL_3G_SEN 0.0008974358974f  // ±3g 量程灵敏度
#define BMI088_ACCEL_6G_SEN 0.00179443359375f // ±6g 量程灵敏度
#define BMI088_ACCEL_12G_SEN 0.0035888671875f // ±12g 量程灵敏度
#define BMI088_ACCEL_24G_SEN 0.007177734375f  // ±24g 量程灵敏度

/* 陀螺仪灵敏度：将原始值转换为 rad/s，单位 rad/s/LSB */
#define BMI088_GYRO_2000_SEN 0.00106526443603169529841533860381f    // ±2000°/s 量程灵敏度
#define BMI088_GYRO_1000_SEN 0.00053263221801584764920766930190693f // ±1000°/s 量程灵敏度
#define BMI088_GYRO_500_SEN 0.00026631610900792382460383465095346f  // ±500°/s 量程灵敏度
#define BMI088_GYRO_250_SEN 0.00013315805450396191230191732547673f  // ±250°/s 量程灵敏度
#define BMI088_GYRO_125_SEN 0.000066579027251980956150958662738366f // ±125°/s 量程灵敏度

/* 温度转换系数 */
#define BMI088_TEMP_FACTOR 0.125f // 温度分辨率 °C/LSB
#define BMI088_TEMP_OFFSET 23.0f  // 温度偏移量 °C

/*------- 延时参数 -------*/
#define BMI088_COM_WAIT_TIME_MS 80 // 通信等待时间
#define BMI088_INIT_DELAY_MS 1     // 初始化延时

/*============================ 类型定义 ============================*/

/**
 * @brief BMI088 工作模式枚举
 */
typedef enum
{
    BMI088_BLOCK_MODE, // 阻塞模式（用于初始化）
    BMI088_DMA_MODE,   // DMA 非阻塞模式（运行时）
} BMI088_Work_Mode_e;

/**
 * @brief BMI088 标定模式枚举
 */
typedef enum
{
    BMI088_CALI_ONLINE, // 在线标定
    BMI088_CALI_PRESET, // 使用预标定参数
} BMI088_Cali_Mode_e;

/**
 * @brief BMI088 读取状态机枚举
 */
typedef enum
{
    BMI088_STATE_IDLE,      // 空闲
    BMI088_STATE_ACC_READ,  // 正在读取加速度计
    BMI088_STATE_GYRO_READ, // 正在读取陀螺仪
    BMI088_STATE_TEMP_READ, // 正在读取温度
} BMI088_ReadState_e;

/**
 * @brief BMI088 错误码枚举
 */
typedef enum
{
    BMI088_NO_ERROR = 0x00,             // 无错误
    BMI088_ACC_PWR_CTRL_ERROR = 0x01,   // 加速度计电源控制寄存器配置错误
    BMI088_ACC_PWR_CONF_ERROR = 0x02,   // 加速度计电源配置寄存器配置错误
    BMI088_ACC_CONF_ERROR = 0x03,       // 加速度计配置寄存器配置错误
    BMI088_ACC_RANGE_ERROR = 0x04,      // 加速度计量程配置错误
    BMI088_INT1_IO_CTRL_ERROR = 0x05,   // INT1 引脚配置错误
    BMI088_INT_MAP_DATA_ERROR = 0x06,   // 中断映射配置错误
    BMI088_GYRO_RANGE_ERROR = 0x07,     // 陀螺仪量程配置错误
    BMI088_GYRO_BANDWIDTH_ERROR = 0x08, // 陀螺仪带宽配置错误
    BMI088_GYRO_LPM1_ERROR = 0x09,      // 陀螺仪低功耗模式配置错误
    BMI088_GYRO_CTRL_ERROR = 0x0A,      // 陀螺仪控制寄存器配置错误
    BMI088_GYRO_INT_CONF_ERROR = 0x0B,  // 陀螺仪中断引脚配置错误
    BMI088_GYRO_INT_MAP_ERROR = 0x0C,   // 陀螺仪中断映射配置错误
    BMI088_NO_SENSOR = 0xFF,            // 未检测到传感器（Who Am I 失败）
} BMI088_Error_e;

/**
 * @brief IMU 数据结构体
 */
typedef struct
{
    float gyro[3]; // 陀螺仪数据 (rad/s)
    float acc[3];  // 加速度计数据 (m/s²)
    float temp;    // 温度 (°C)
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

    /* 配置 */
    BMI088_Work_Mode_e work_mode; // 工作模式
    BMI088_Cali_Mode_e cali_mode; // 标定模式

    /* 状态机 */
    BMI088_ReadState_e read_state; // 读取状态
    uint8_t temp_read_cnt;         // 温度读取计数器

    /* 数据 */
    BMI088_Data_t data;   // IMU 数据
    float gyro_offset[3]; // 陀螺仪零偏
    float acc_scale;      // 加速度计缩放系数
    float g_norm;         // 重力加速度模长

    /* 灵敏度 */
    float acc_sen;  // 加速度计灵敏度
    float gyro_sen; // 陀螺仪灵敏度

    /* 标志位 */
    volatile uint8_t imu_ready;    // IMU 数据就绪标志
    volatile uint8_t acc_updated;  // 加速度计数据更新标志
    volatile uint8_t gyro_updated; // 陀螺仪数据更新标志

    /* APP 回调 */
    void (*app_callback)(struct BMI088Instance *); // 数据就绪回调

    /* 温度控制 */
    float target_temp;      // 目标温度
    uint8_t heater_enabled; // 加热使能
} BMI088Instance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief 静态定义 BMI088 实例
 * @param name      实例名称
 * @param spi_idx   SPI 枚举 (BoardSPI_e)
 * @param cs_acc    加速度计 CS 枚举 (BoardGPIO_e)
 * @param cs_gyro   陀螺仪 CS 枚举 (BoardGPIO_e)
 * @param int_acc   加速度计 INT 枚举 (BoardGPIO_e)
 * @param int_gyro  陀螺仪 INT 枚举 (BoardGPIO_e，可为 GPIO_NUM_MAX 表示不使用)
 * @param heater    加热 TIM 枚举 (BoardTIM_e，可为 TIM_NUM_MAX 表示不使用)
 * @param app_cb    APP 回调函数
 */
#define BMI088_INSTANCE_DEF(name, spi_idx, cs_acc, cs_gyro, int_acc, int_gyro, heater, app_cb) \
    static uint8_t name##_spi_rx_buff[16] __attribute__((section(".ram_d1"))) = {0};           \
    static BMI088Instance name = {                                                             \
        .spi_inst = {                                                                          \
            .spi_e = spi_idx,                                                                  \
            .handle = NULL,                                                                    \
            .work_mode = SPI_DMA_MODE,                                                         \
            .rx_buff = name##_spi_rx_buff,                                                     \
            .buff_size = 16,                                                                   \
            .rx_len = 0,                                                                       \
            .rx_callback = NULL,                                                               \
        },                                                                                     \
        .cs_acc = {.gpio_e = cs_acc, .callback = NULL},                                        \
        .cs_gyro = {.gpio_e = cs_gyro, .callback = NULL},                                      \
        .int_acc = {.gpio_e = int_acc, .callback = NULL},                                      \
        .int_gyro = {.gpio_e = int_gyro, .callback = NULL},                                    \
        .heater_pwm = {.tim_e = heater, .dutyratio = 0.0f},                                    \
        .work_mode = BMI088_DMA_MODE,                                                          \
        .cali_mode = BMI088_CALI_ONLINE,                                                       \
        .read_state = BMI088_STATE_IDLE,                                                       \
        .temp_read_cnt = 0,                                                                    \
        .data = {.gyro = {0}, .acc = {0}, .temp = 0},                                          \
        .gyro_offset = {0},                                                                    \
        .acc_scale = 1.0f,                                                                     \
        .g_norm = 9.81f,                                                                       \
        .acc_sen = BMI088_ACCEL_6G_SEN,                                                        \
        .gyro_sen = BMI088_GYRO_2000_SEN,                                                      \
        .imu_ready = 0,                                                                        \
        .acc_updated = 0,                                                                      \
        .gyro_updated = 0,                                                                     \
        .app_callback = app_cb,                                                                \
        .target_temp = 45.0f,                                                                  \
        .heater_enabled = 0,                                                                   \
    }

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册并初始化 BMI088
 * @param inst BMI088 实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t BMI088Register(BMI088Instance *inst);

/**
 * @brief 获取 BMI088 数据
 * @param inst BMI088 实例指针
 * @param data 数据存储指针
 * @retval 1 数据已更新
 * @retval 0 数据未更新
 */
uint8_t BMI088GetData(BMI088Instance *inst, BMI088_Data_t *data);

/**
 * @brief 在线标定 BMI088
 * @param inst BMI088 实例指针
 * @note 标定期间使用阻塞模式，标定完成后恢复 DMA 模式
 */
void BMI088Calibrate(BMI088Instance *inst);

/**
 * @brief 温度控制
 * @param inst BMI088 实例指针
 * @note 简单阈值控制，温度低于目标温度-1°C 时开启加热
 */
void BMI088TempControl(BMI088Instance *inst);

#endif /* __DRV_BMI088_H */
