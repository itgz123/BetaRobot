/**
 * @file drv_bmi088.h
 * @brief BMI088 六轴 IMU 驱动（加速度计 + 陀螺仪）
 *
 * @note 使用 DMA 非阻塞模式进行 SPI 数据读取
 * @note 片选由 DRV 层通过 GPIO 接口控制
 *
 * @note 本驱动的数据传输方式：
 * @note 传感器数据就绪中断->
 * @note gpio 中断回调中启动 spi 的 dma 获取数据->
 * @note dma 完成中断拷贝数据到队列->
 * @note 任务中获取队列信息滤波反馈。
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
#define BMI088_ACC_CHIP_ID_REG 0x00    // Who Am I 寄存器地址
#define BMI088_ACC_ERR_REG 0x02        // 错误标志寄存器
#define BMI088_ACC_STATUS_REG 0x03     // 状态寄存器
#define BMI088_ACC_INT_STAT_1_REG 0x1D // 中断状态寄存器 1
#define BMI088_ACCEL_XOUT_L 0x12       // X 轴加速度低字节
#define BMI088_ACCEL_XOUT_H 0x13       // X 轴加速度高字节
#define BMI088_ACCEL_YOUT_L 0x14       // Y 轴加速度低字节
#define BMI088_ACCEL_YOUT_H 0x15       // Y 轴加速度高字节
#define BMI088_ACCEL_ZOUT_L 0x16       // Z 轴加速度低字节
#define BMI088_ACCEL_ZOUT_H 0x17       // Z 轴加速度高字节
#define BMI088_SENSORTIME_0_REG 0x18   // 传感器时间低字节
#define BMI088_SENSORTIME_1_REG 0x19   // 传感器时间中字节
#define BMI088_SENSORTIME_2_REG 0x1A   // 传感器时间高字节
#define BMI088_TEMP_L 0x23             // 温度数据低字节
#define BMI088_TEMP_M 0x22             // 温度数据高字节
#define BMI088_ACC_FIFO_LENGTH_L 0x24  // FIFO 数据长度低字节
#define BMI088_ACC_FIFO_LENGTH_H 0x25  // FIFO 数据长度高字节
#define BMI088_ACC_FIFO_DATA_REG 0x26  // FIFO 数据寄存器
#define BMI088_ACC_CONF_REG 0x40       // 加速度计配置寄存器
#define BMI088_ACC_RANGE_REG 0x41      // 量程配置寄存器
#define BMI088_ACC_FIFO_DOWNS_REG 0x45 // FIFO 降采样配置寄存器
#define BMI088_ACC_FIFO_WTM_L 0x46     // FIFO 水印低字节
#define BMI088_ACC_FIFO_WTM_H 0x47     // FIFO 水印高字节
#define BMI088_ACC_FIFO_CONFIG_0_REG 0x48 // FIFO 配置寄存器 0
#define BMI088_ACC_FIFO_CONFIG_1_REG 0x49 // FIFO 配置寄存器 1
#define BMI088_INT1_IO_CTRL_REG 0x53   // INT1 引脚配置寄存器
#define BMI088_INT2_IO_CTRL_REG 0x54   // INT2 引脚配置寄存器
#define BMI088_INT_MAP_DATA_REG 0x58   // 中断映射寄存器
#define BMI088_ACC_SELF_TEST_REG 0x6D  // 自检寄存器
#define BMI088_ACC_PWR_CONF_REG 0x7C   // 电源配置寄存器
#define BMI088_ACC_PWR_CTRL_REG 0x7D   // 电源控制寄存器
#define BMI088_ACC_SOFTRESET_REG 0x7E  // 软复位寄存器

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

/*============================ 固定值定义 ============================*/

/*------- 加速度计固定值 -------*/
#define BMI088_ACC_CHIP_ID_VALUE 0x1E   // Who Am I 期望值
#define BMI088_ACC_SOFTRESET_VALUE 0xB6 // 软复位命令值

/*------- 陀螺仪固定值 -------*/
#define BMI088_GYRO_CHIP_ID_VALUE 0x0F   // Who Am I 期望值
#define BMI088_GYRO_SOFTRESET_VALUE 0xB6 // 软复位命令值

/*============================ 位域/掩码定义 ============================*/

/*------- 加速度计状态位 -------*/
#define BMI088_ACCEL_DRDY (1U << 7) // 数据就绪标志位

/*------- 加速度计配置位 -------*/
#define BMI088_ACC_CONF_MUST_SET (1U << 7)  // 配置寄存器必须设置的位
#define BMI088_ACC_NORMAL_MODE (0x02U << 4) // 正常工作模式

/*------- 加速度计电源控制位 -------*/
#define BMI088_ACC_ENABLE 0x04U  // 加速度计使能
#define BMI088_ACC_DISABLE 0x00U // 加速度计禁用

/*------- 加速度计电源配置位 -------*/
#define BMI088_ACC_PWR_ACTIVE 0x00U  // 激活模式
#define BMI088_ACC_PWR_SUSPEND 0x03U // 挂起模式

/*------- 加速度计中断配置位 -------*/
#define BMI088_INT1_IO_ENABLE (1U << 3)      // INT1 输出使能
#define BMI088_INT1_GPIO_PP (0U << 2)        // 推挽输出模式
#define BMI088_INT1_GPIO_LOW (0U << 1)       // 低电平有效
#define BMI088_INT1_DRDY_INTERRUPT (1U << 2) // 数据就绪中断映射到 INT1

/*------- 陀螺仪状态位 -------*/
#define BMI088_GYRO_DRDY (1U << 7) // 数据就绪标志位

/*------- 陀螺仪配置位 -------*/
#define BMI088_GYRO_BW_MUST_SET (1U << 7) // 带宽寄存器必须设置的位
#define BMI088_GYRO_NORMAL_MODE 0x00U     // 正常工作模式
#define BMI088_GYRO_SUSPEND_MODE 0x80U    // 挂起模式

/*------- 陀螺仪控制位 -------*/
#define BMI088_DRDY_ON 0x80U // 数据就绪中断使能

/*------- 陀螺仪中断配置位 -------*/
#define BMI088_GYRO_INT3_PP (0U << 1)  // INT3 推挽输出模式
#define BMI088_GYRO_INT3_LOW (0U << 0) // INT3 低电平有效
#define BMI088_DRDY_IO_INT3 0x01U      // 数据就绪中断映射到 INT3

/*============================ 配置枚举定义 ============================*/

/**
 * @brief 加速度计 ODR（输出数据率）枚举
 * @note 写入 ACC_CONF 寄存器 bits[3:0]
 */
typedef enum
{
    BMI088_ACC_ODR_12_5_HZ = 0x05, // 12.5Hz  输出数据率
    BMI088_ACC_ODR_25_HZ = 0x06,   // 25Hz    输出数据率
    BMI088_ACC_ODR_50_HZ = 0x07,   // 50Hz    输出数据率
    BMI088_ACC_ODR_100_HZ = 0x08,  // 100Hz   输出数据率
    BMI088_ACC_ODR_200_HZ = 0x09,  // 200Hz   输出数据率
    BMI088_ACC_ODR_400_HZ = 0x0A,  // 400Hz   输出数据率
    BMI088_ACC_ODR_800_HZ = 0x0B,  // 800Hz   输出数据率
    BMI088_ACC_ODR_1600_HZ = 0x0C, // 1600Hz  输出数据率
} BMI088_AccODR_e;

/**
 * @brief 加速度计滤波器带宽配置枚举
 * @note 写入 ACC_CONF 寄存器 bits[7:4]
 */
typedef enum
{
    BMI088_ACC_BW_OSR4 = 0x08,   // OSR4 (4 倍过采样)
    BMI088_ACC_BW_OSR2 = 0x09,   // OSR2 (2 倍过采样)
    BMI088_ACC_BW_NORMAL = 0x0A, // 正常模式
} BMI088_AccBW_e;

/**
 * @brief 加速度计量程枚举
 * @note 写入 ACC_RANGE 寄存器 bits[1:0]
 *       灵敏度通过 BMI088_AccSenTable[range] 查表获取
 */
typedef enum
{
    BMI088_ACC_RANGE_3G = 0x00,  // ±3g   量程，灵敏度 0.000897 g/LSB
    BMI088_ACC_RANGE_6G = 0x01,  // ±6g   量程，灵敏度 0.001794 g/LSB
    BMI088_ACC_RANGE_12G = 0x02, // ±12g  量程，灵敏度 0.003589 g/LSB
    BMI088_ACC_RANGE_24G = 0x03, // ±24g  量程，灵敏度 0.007178 g/LSB
} BMI088_AccRange_e;

/**
 * @brief 加速度计量程数量
 */
#define BMI088_ACC_RANGE_NUM 4

/**
 * @brief 陀螺仪量程枚举
 * @note 写入 GYRO_RANGE 寄存器 bits[2:0]
 *       灵敏度通过 BMI088_GyroSenTable[range] 查表获取
 */
typedef enum
{
    BMI088_GYRO_RANGE_2000 = 0x00, // ±2000°/s  量程，灵敏度 0.001065 rad/s/LSB
    BMI088_GYRO_RANGE_1000 = 0x01, // ±1000°/s  量程，灵敏度 0.000533 rad/s/LSB
    BMI088_GYRO_RANGE_500 = 0x02,  // ±500°/s   量程，灵敏度 0.000266 rad/s/LSB
    BMI088_GYRO_RANGE_250 = 0x03,  // ±250°/s   量程，灵敏度 0.000133 rad/s/LSB
    BMI088_GYRO_RANGE_125 = 0x04,  // ±125°/s   量程，灵敏度 0.000067 rad/s/LSB
} BMI088_GyroRange_e;

/**
 * @brief 陀螺仪量程数量
 */
#define BMI088_GYRO_RANGE_NUM 5

/**
 * @brief 陀螺仪带宽（ODR / Bandwidth）枚举
 * @note 写入 GYRO_BANDWIDTH 寄存器 bits[2:0]
 */
typedef enum
{
    BMI088_GYRO_BW_2000_532HZ = 0x00, // ODR 2000Hz, 带宽 532Hz
    BMI088_GYRO_BW_2000_230HZ = 0x01, // ODR 2000Hz, 带宽 230Hz
    BMI088_GYRO_BW_1000_116HZ = 0x02, // ODR 1000Hz, 带宽 116Hz
    BMI088_GYRO_BW_400_47HZ = 0x03,   // ODR 400Hz,  带宽 47Hz
    BMI088_GYRO_BW_200_23HZ = 0x04,   // ODR 200Hz,  带宽 23Hz
    BMI088_GYRO_BW_100_12HZ = 0x05,   // ODR 100Hz,  带宽 12Hz
    BMI088_GYRO_BW_200_64HZ = 0x06,   // ODR 200Hz,  带宽 64Hz
    BMI088_GYRO_BW_100_32HZ = 0x07,   // ODR 100Hz,  带宽 32Hz
} BMI088_GyroBW_e;

/**
 * @brief 陀螺仪电源模式枚举
 * @note 写入 GYRO_LPM1 寄存器
 */
typedef enum
{
    BMI088_GYRO_MODE_NORMAL = 0x00,       // 正常模式
    BMI088_GYRO_MODE_SUSPEND = 0x80,      // 挂起模式
    BMI088_GYRO_MODE_DEEP_SUSPEND = 0x20, // 深度挂起模式
} BMI088_GyroMode_e;

/*============================ 灵敏度查找表 ============================*/

/**
 * @brief 加速度计灵敏度查找表
 * @note 单位：g/LSB，通过量程枚举值索引
 */
extern const float BMI088_AccSenTable[BMI088_ACC_RANGE_NUM];

/**
 * @brief 陀螺仪灵敏度查找表
 * @note 单位：rad/s/LSB，通过量程枚举值索引
 */
extern const float BMI088_GyroSenTable[BMI088_GYRO_RANGE_NUM];

/*============================ 其他常量定义 ============================*/

/*------- SPI 通信参数 -------*/
#define BMI088_SPI_READ_FLAG 0x80 // SPI 读命令标志位（或上寄存器地址）
#define BMI088_DUMMY_BYTE 0x55    // SPI 空闲字节（用于发送时接收数据）

/*------- 数据缓冲区索引 -------*/
/* 加速度计 DMA 读取：1 字节读命令 + 1 字节 dummy + 6 字节数据 */
#define BMI088_ACC_RX_DUMMY_LEN 2 // 加速度计接收数据起始索引（跳过 dummy）
/* 陀螺仪 DMA 读取：1 字节读命令 + 1 字节 CHIP_ID + 6 字节数据 */
#define BMI088_GYRO_RX_CHIPID_IDX 0 // 陀螺仪接收 CHIP_ID 索引
#define BMI088_GYRO_RX_DATA_IDX 1   // 陀螺仪接收数据起始索引
/* 温度 DMA 读取：1 字节读命令 + 1 字节 dummy + 2 字节数据 */
#define BMI088_TEMP_RX_DUMMY_LEN 1 // 温度接收数据起始索引（跳过第1个dummy字节）

/*------- 温度转换参数 -------*/
#define BMI088_TEMP_FACTOR 0.125f // 温度分辨率 °C/LSB
#define BMI088_TEMP_OFFSET 23.0f  // 温度偏移量 °C

/*------- 延时参数 -------*/
#define BMI088_COM_WAIT_TIME_MS 80 // 通信等待时间 ms（软复位后）
#define BMI088_INIT_DELAY_MS 1     // 初始化延时 ms（寄存器写入后）
#define BMI088_PWR_CTRL_DELAY_MS 5 // ACC_PWR_CTRL 写入后延时 ms（必须至少 5ms）

/*------- 温度控制参数 -------*/
#define BMI088_HEATER_HYSTERESIS 1.0f // 温度控制迟滞值 °C
#define BMI088_HEATER_DUTY_RATIO 0.5f // 加热 PWM 默认占空比

/*------- 标定参数 -------*/
#define BMI088_CALI_SAMPLES 6000         // 标定采样次数
#define BMI088_CALI_ACC_THRESHOLD 0.5f   // 加速度标定运动阈值 g
#define BMI088_CALI_GYRO_THRESHOLD 0.15f // 陀螺仪标定运动阈值 rad/s
#define BMI088_CALI_INTERVAL_MS 0.5f     // 标定采样间隔 ms

/*------- 错误恢复参数 -------*/
#define BMI088_STATE_TIMEOUT_MS 10   // 状态机超时时间 ms
#define BMI088_STATE_TIMEOUT_MAX 100 // 状态机超时最大计数

/*============================ 类型定义 ============================*/

/**
 * @brief BMI088 工作模式枚举
 */
typedef enum
{
    BMI088_BLOCK_MODE, // 阻塞模式（用于初始化）
    BMI088_DMA_MODE,   // DMA 非阻塞模式（运行时）
} BMI088_WorkMode_e;

/**
 * @brief BMI088 标定模式枚举
 */
typedef enum
{
    BMI088_CALI_ONLINE, // 在线标定
    BMI088_CALI_PRESET, // 使用预标定参数
} BMI088_CaliMode_e;

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
    BMI088_WorkMode_e work_mode;   // 工作模式
    BMI088_CaliMode_e cali_mode;   // 标定模式
    BMI088_AccODR_e acc_odr;       // 加速度计输出数据率
    BMI088_AccBW_e acc_bw;         // 加速度计带宽配置
    BMI088_AccRange_e acc_range;   // 加速度计量程
    BMI088_GyroRange_e gyro_range; // 陀螺仪量程
    BMI088_GyroBW_e gyro_bw;       // 陀螺仪带宽

    /* 状态机 */
    BMI088_ReadState_e read_state; // 读取状态
    uint8_t temp_read_cnt;         // 温度读取计数器
    uint32_t state_timeout_cnt;    // 状态机超时计数器

    /* DMA 缓冲区 */
    uint8_t tx_buff[8]; // DMA 发送缓冲区（实例级，避免数据竞争）

    /* 数据 */
    BMI088_Data_t data;   // IMU 数据
    float gyro_offset[3]; // 陀螺仪零偏
    float acc_scale;      // 加速度计缩放系数
    float g_norm;         // 重力加速度模长

    /* 灵敏度 */
    float acc_sen;  // 加速度计灵敏度 (g/LSB)
    float gyro_sen; // 陀螺仪灵敏度 (rad/s/LSB)

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
 * @param cs_acc_pin    加速度计 CS 枚举 (BoardGPIO_e)
 * @param cs_gyro_pin   陀螺仪 CS 枚举 (BoardGPIO_e)
 * @param int_acc_pin   加速度计 INT 枚举 (BoardGPIO_e)
 * @param int_gyro_pin  陀螺仪 INT 枚举 (BoardGPIO_e，可为 GPIO_NUM_MAX 表示不使用)
 * @param heater_pin    加热 TIM 枚举 (BoardTIM_e，可为 TIM_NUM_MAX 表示不使用)
 * @param app_cb    APP 回调函数
 *
 * @note 灵敏度在 BMI088Register() 中根据配置枚举自动设置
 */
#define BMI088_INSTANCE_DEF(name, spi_idx, cs_acc_pin, cs_gyro_pin, int_acc_pin, int_gyro_pin, heater_pin, app_cb) \
    static uint8_t name##_spi_rx_buff[16] __attribute__((section(".ram_d1"))) = {0};                               \
    static BMI088Instance name = {                                                                                 \
        .spi_inst = {                                                                                              \
            .spi_e = spi_idx,                                                                                      \
            .handle = NULL,                                                                                        \
            .work_mode = SPI_DMA_MODE,                                                                             \
            .rx_buff = name##_spi_rx_buff,                                                                         \
            .buff_size = 16,                                                                                       \
            .rx_len = 0,                                                                                           \
            .rx_callback = NULL,                                                                                   \
        },                                                                                                         \
        .cs_acc = {.gpio_e = cs_acc_pin, .callback = NULL},                                                        \
        .cs_gyro = {.gpio_e = cs_gyro_pin, .callback = NULL},                                                      \
        .int_acc = {.gpio_e = int_acc_pin, .callback = NULL},                                                      \
        .int_gyro = {.gpio_e = int_gyro_pin, .callback = NULL},                                                    \
        .heater_pwm = {.tim_e = heater_pin, .dutyratio = 0.0f},                                                    \
        .work_mode = BMI088_DMA_MODE,                                                                              \
        .cali_mode = BMI088_CALI_ONLINE,                                                                           \
        .acc_odr = BMI088_ACC_ODR_800_HZ,                                                                          \
        .acc_bw = BMI088_ACC_BW_NORMAL,                                                                            \
        .acc_range = BMI088_ACC_RANGE_6G,                                                                          \
        .gyro_range = BMI088_GYRO_RANGE_2000,                                                                      \
        .gyro_bw = BMI088_GYRO_BW_2000_230HZ,                                                                      \
        .read_state = BMI088_STATE_IDLE,                                                                           \
        .temp_read_cnt = 0,                                                                                        \
        .state_timeout_cnt = 0,                                                                                    \
        .tx_buff = {0},                                                                                            \
        .data = {.gyro = {0}, .acc = {0}, .temp = 0},                                                              \
        .gyro_offset = {0},                                                                                        \
        .acc_scale = 1.0f,                                                                                         \
        .g_norm = 9.81f,                                                                                           \
        .acc_sen = 0.0f,                                                                                           \
        .gyro_sen = 0.0f,                                                                                          \
        .imu_ready = 0,                                                                                            \
        .acc_updated = 0,                                                                                          \
        .gyro_updated = 0,                                                                                         \
        .app_callback = app_cb,                                                                                    \
        .target_temp = 45.0f,                                                                                      \
        .heater_enabled = 0,                                                                                       \
    }

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册并初始化 BMI088
 * @param inst BMI088 实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 *
 * @note 灵敏度根据实例中的 acc_range 和 gyro_range 自动设置
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
 * @note 简单阈值控制，温度低于目标温度 -1°C 时开启加热
 */
void BMI088TempControl(BMI088Instance *inst);

#endif /* __DRV_BMI088_H */
