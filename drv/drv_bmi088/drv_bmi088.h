/**
 * @file drv_bmi088.h
 * @brief BMI088 六轴 IMU 驱动（加速度计 + 陀螺仪）
 *
 * @note 片选由 DRV 层通过 GPIO 接口控制
 * @note 中断模式使用循环缓冲 + 线性插值对齐 acc/gyro 时间戳
 *
 */

#ifndef __DRV_BMI088_H
#define __DRV_BMI088_H

#include "main.h"
#include "bsp_map.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "bsp_tim.h"
#include "bmi088_reg_def.h"

/**
 * @brief BMI088 工作模式枚举
 * @note 中断模式如下优点：
 * @note 1. 精确时间戳
 * @note 2. 避免数据覆盖
 * @note 3. BMI088ReadInt比BMI088ReadBlocking少了spi时间
 */
typedef enum : uint8_t
{
    BMI088_MODE_POLLING = 0, // 阻塞模式：主动轮询读取6轴数据
    BMI088_MODE_INT,         // 中断模式：数据就绪后实例自动读取
} BMI088_WorkMode_e;

/*============================ 缓冲区大小定义 ============================*/

#define BMI088_BUFF_SIZE 10 // SPI缓冲区大小：1地址 + 1虚拟 + 6数据 + 2温度（最大）

/*============================ 循环缓冲大小 ============================*/
// 用复杂状态机可以用3+3实现
// f_acc=12.5-1600hz
// f_gyro=100-2000hz
#define BMI088_ACC_BUF_SIZE 17
#define BMI088_GYRO_BUF_SIZE 17

/*============================ 数据尺寸常量 ============================*/
#define BMI088_AXIS_NUM 3      // 轴数
#define BMI088_RAW_DATA_SIZE 6 // 原始数据字节数 (3轴 × 2字节)

/*============================ 配置结构体 ============================*/

/**
 * @brief BMI088 初始化配置结构体
 */
typedef struct
{
    BoardSPI_e spi_e;              // 板载SPI枚举
    BoardGPIO_e cs_acc_e;          // 加速度计片选GPIO枚举
    BoardGPIO_e cs_gyro_e;         // 陀螺仪片选GPIO枚举
    BoardGPIO_e int_acc_e;         // 加速度计中断GPIO枚举
    BoardGPIO_e int_gyro_e;        // 陀螺仪中断GPIO枚举
    BoardTIM_e heater_e;           // 加热PWM枚举
    BMI088_AccRange_e acc_range;   // 加速度计量程
    uint8_t acc_bwp;               // 加速度计低通滤波器带宽
    uint8_t acc_odr;               // 加速度计输出数据速率
    BMI088_GyroRange_e gyro_range; // 陀螺仪量程
    uint8_t gyro_odr;              // 陀螺仪输出数据速率
    uint8_t gyro_bw;               // 陀螺仪滤波器带宽
    BMI088_WorkMode_e work_mode;   // 工作模式（轮询/中断）
} BMI088_Init_Config_s;
/**
 * @brief IMU 数据结构体
 */
typedef struct
{
    float gyro[BMI088_AXIS_NUM]; // 陀螺仪数据 (rad/s)
    float acc[BMI088_AXIS_NUM];  // 加速度计数据 (m/s²)
    uint64_t time_stamp;         // 时间戳 (μs)
} BMI088_Data_t;

/**
 * @brief IMU 多速率数据结构体（用于 Kalman 等需要独立时间戳的融合算法）
 */
typedef struct
{
    float gyro[BMI088_AXIS_NUM]; // 陀螺仪数据 (rad/s)
    float acc[BMI088_AXIS_NUM];  // 加速度计数据 (m/s²)
    uint64_t time_stamp_a;       // 加速度计时间戳 (μs)
    uint64_t time_stamp_g;       // 陀螺仪时间戳 (μs)
} BMI088_MultiRateData_t;

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
    float acc_offset[BMI088_AXIS_NUM];  // 加速度计零偏 (m/s²)
    float gyro_offset[BMI088_AXIS_NUM]; // 陀螺仪零偏 (rad/s)

    /*============================ 中断模式字段 ============================*/

    /* --- EXTI/SPI 同步 --- */
    volatile uint8_t transfer_busy;  // SPI IT 传输进行中
    volatile uint8_t current_sensor; // 当前 SPI 读取的传感器 (0=acc, 1=gyro)
    uint8_t pending_mask;            // 待读取传感器掩码 (bit0=acc, bit1=gyro)

    /* --- 中断时间戳缓存 --- */
    uint64_t int_timestamp;  // 当前 SPI 读取对应的 INT 触发时间
    uint64_t pending_t_acc;  // 暂存的 acc INT 时间（pending 用）
    uint64_t pending_t_gyro; // 暂存的 gyro INT 时间（pending 用）

    /* --- 循环缓冲（写入 ISR，读出配对） --- */
    uint8_t acc_raw[BMI088_ACC_BUF_SIZE][6];   // 加速度计原始环形缓冲
    uint8_t gyro_raw[BMI088_GYRO_BUF_SIZE][6]; // 陀螺仪原始环形缓冲
    uint64_t t_acc[BMI088_ACC_BUF_SIZE];       // 加速度计时间戳 (us)
    uint64_t t_gyro[BMI088_GYRO_BUF_SIZE];     // 陀螺仪时间戳 (us)
    volatile uint16_t acc_wr_idx;              // 加速度计写入索引（永远递增）
    volatile uint16_t gyro_wr_idx;             // 陀螺仪写入索引（永远递增）
    volatile uint8_t acc_cnt;                  // 已收到的 acc 样本数
    volatile uint8_t gyro_cnt;                 // 已收到的 gyro 样本数
    volatile int16_t gyro_temp_raw;            // 陀螺仪侧原始温度值（SPI扩展读出）
} BMI088Instance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief BMI088实例静态定义宏
 * @param name 实例名称
 *
 * @note 使用 BSP 层的实例定义宏，parent 在注册时设置
 *       中断模式字段由 BMI088Register 初始化
 *
 * @example
 *   BMI088_INSTANCE_DEF(bmi088);
 */
#define BMI088_INSTANCE_DEF(name)                                  \
    static uint8_t name##_tx_buff[BMI088_BUFF_SIZE] DMA_RAM = {0}; \
    SPI_INSTANCE_DEF(name##_spi, BMI088_BUFF_SIZE);                \
    GPIO_INSTANCE_DEF(name##_cs_acc);                              \
    GPIO_INSTANCE_DEF(name##_cs_gyro);                             \
    GPIO_INSTANCE_DEF(name##_int_acc);                             \
    GPIO_INSTANCE_DEF(name##_int_gyro);                            \
    PWM_INSTANCE_DEF(name##_heater);                               \
    static BMI088Instance name = {                                 \
        .spi_inst = &name##_spi,                                   \
        .cs_acc = &name##_cs_acc,                                  \
        .cs_gyro = &name##_cs_gyro,                                \
        .int_acc = &name##_int_acc,                                \
        .int_gyro = &name##_int_gyro,                              \
        .heater_pwm = &name##_heater,                              \
        .tx_buff = name##_tx_buff}

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册BMI088实例（包含BSP子实例注册和传感器配置）
 * @param inst   BMI088实例指针
 * @param config 初始化配置结构体指针
 * @return 0成功，-1失败
 */
int8_t BMI088Register(BMI088Instance *inst, const BMI088_Init_Config_s *config);

/**
 * @brief 阻塞读取BMI088数据（轮询模式专用）
 * @param inst BMI088实例指针
 * @return BMI088_Data_t 结构体，包含加速度、陀螺仪、温度和时间戳
 * @note 此函数会阻塞等待数据读取完成
 * @note 中断模式下调用会返回空数据并记录警告
 */
BMI088_Data_t BMI088ReadBlocking(BMI088Instance *inst);

/**
 * @brief 非阻塞读取中断模式下对齐后的 IMU 数据
 * @param inst BMI088实例指针
 * @return BMI088_Data_t
 * @note 内部使用 16+16 循环缓冲 + 线性插值将 acc 和 gyro 对齐到最新数据的时间戳
 * @retval 数据成员全 0 表示尚无可用配对（启动瞬态）
 * @retval 数据有效则 time_stamp > 0
 */
BMI088_Data_t BMI088ReadInt(BMI088Instance *inst);

/**
 * @brief 读取最新一帧 Acc 和 Gyro（各自带独立时间戳，无插值对齐）
 * @param inst BMI088实例指针
 * @return BMI088_MultiRateData_t
 * @note 专为 Kalman 等需要多速率融合的算法设计，保留各自的原始时间戳
 * @retval time_stamp_a / time_stamp_g 为 0 表示数据尚未就绪
 */
BMI088_MultiRateData_t BMI088ReadLatest(BMI088Instance *inst);

#endif /* defined(BSP_SPI_MODULE_ENABLED) && defined(BSP_GPIO_MODULE_ENABLED) */

#endif /* __DRV_BMI088_H */
