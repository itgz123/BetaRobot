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
// #define BMI088_SENSORTIME_0_REG 0x18      // 传感器时间低字节
// #define BMI088_SENSORTIME_1_REG 0x19      // 传感器时间中字节
// #define BMI088_SENSORTIME_2_REG 0x1A      // 传感器时间高字节
#define BMI088_TEMP_L 0x23 // 温度数据低字节
#define BMI088_TEMP_M 0x22 // 温度数据高字节
// #define BMI088_ACC_FIFO_LENGTH_L 0x24     // FIFO 数据长度低字节
// #define BMI088_ACC_FIFO_LENGTH_H 0x25     // FIFO 数据长度高字节
// #define BMI088_ACC_FIFO_DATA_REG 0x26     // FIFO 数据寄存器
#define BMI088_ACC_CONF_REG 0x40  // 加速度计配置寄存器
#define BMI088_ACC_RANGE_REG 0x41 // 量程配置寄存器
// #define BMI088_ACC_FIFO_DOWNS_REG 0x45    // FIFO 降采样配置寄存器
// #define BMI088_ACC_FIFO_WTM_L 0x46        // FIFO 水印低字节
// #define BMI088_ACC_FIFO_WTM_H 0x47        // FIFO 水印高字节
// #define BMI088_ACC_FIFO_CONFIG_0_REG 0x48 // FIFO 配置寄存器 0
// #define BMI088_ACC_FIFO_CONFIG_1_REG 0x49 // FIFO 配置寄存器 1
#define BMI088_INT1_IO_CTRL_REG 0x53  // INT1 引脚配置寄存器
#define BMI088_INT2_IO_CTRL_REG 0x54  // INT2 引脚配置寄存器
#define BMI088_INT_MAP_DATA_REG 0x58  // 中断映射寄存器
#define BMI088_ACC_SELF_TEST_REG 0x6D // 自检寄存器
#define BMI088_ACC_PWR_CONF_REG 0x7C  // 电源配置寄存器
#define BMI088_ACC_PWR_CTRL_REG 0x7D  // 电源控制寄存器
#define BMI088_ACC_SOFTRESET_REG 0x7E // 软复位寄存器

/*------- 陀螺仪寄存器地址 -------*/
#define BMI088_GYRO_CHIP_ID_REG 0x00 // Who Am I 寄存器地址
#define BMI088_GYRO_X_L 0x02         // X 轴角速度低字节
#define BMI088_GYRO_X_H 0x03         // X 轴角速度高字节
#define BMI088_GYRO_Y_L 0x04         // Y 轴角速度低字节
#define BMI088_GYRO_Y_H 0x05         // Y 轴角速度高字节
#define BMI088_GYRO_Z_L 0x06         // Z 轴角速度低字节
#define BMI088_GYRO_Z_H 0x07         // Z 轴角速度高字节
#define BMI088_GYRO_INT_STAT_1 0x0A  // 中断状态寄存器 1
// #define BMI088_GYRO_FIFO_STATUS_REG 0x0E       // FIFO 状态寄存器
#define BMI088_GYRO_RANGE_REG 0x0F             // 量程配置寄存器
#define BMI088_GYRO_BANDWIDTH_REG 0x10         // 带宽配置寄存器
#define BMI088_GYRO_LPM1_REG 0x11              // 低功耗模式配置寄存器
#define BMI088_GYRO_SOFTRESET_REG 0x14         // 软复位寄存器
#define BMI088_GYRO_INT_CTRL_REG 0x15          // 中断控制寄存器
#define BMI088_GYRO_INT3_INT4_IO_CONF_REG 0x16 // INT3/INT4 引脚配置寄存器
#define BMI088_GYRO_INT3_INT4_IO_MAP_REG 0x18  // INT3/INT4 中断映射寄存器
// #define BMI088_GYRO_FIFO_WM_EN_REG 0x1E        // FIFO 水印使能寄存器
// #define BMI088_GYRO_FIFO_EXT_INT_S_REG 0x34    // FIFO 外部中断选择寄存器
#define BMI088_GYRO_SELF_TEST_REG 0x3C // 自检寄存器
// #define BMI088_GYRO_FIFO_CONFIG_0_REG 0x3D     // FIFO 配置寄存器 0
// #define BMI088_GYRO_FIFO_CONFIG_1_REG 0x3E     // FIFO 配置寄存器 1
// #define BMI088_GYRO_FIFO_DATA_REG 0x3F         // FIFO 数据寄存器

/*============================ 寄存器位定义 ============================*/

/**
 * @brief ACC_ERR_REG (0x02) 错误标志寄存器
 * @note bit[0] - fatal_err: 致命错误，芯片未处于可操作状态
 * @note bit[4:2] - error_code: 持续错误的错误状态
 * @note 访问: RO
 */
typedef enum
{
    // bit[0] - fatal_err: 致命错误标志
    BMI088_ACC_ERR_FATAL = 1 << 0, // 存在致命错误

    // bit[4:2] - error_code: 错误码
    BMI088_ACC_ERR_CODE_NO_ERROR = 0x00 << 2,   // 无错误
    BMI088_ACC_ERR_CODE_CONF_ERROR = 0x01 << 2, // ACC_CONF 寄存器存在无效数据
} BMI088_AccErr_e;

/**
 * @brief ACC_STATUS_REG (0x03) 状态寄存器
 * @note bit[7] - acc_drdy: 加速度计数据就绪标志
 * @note 访问: RO
 */
typedef enum
{
    BMI088_ACC_STATUS_DRDY = 1 << 7, // 加速度数据已就绪
} BMI088_AccStatus_e;

/**
 * @brief ACC_INT_STAT_1_REG (0x1D) 中断状态寄存器 1
 * @note bit[7] - acc_drdy: 加速度数据就绪中断标志
 * @note 访问: RO
 */
typedef enum
{
    BMI088_ACC_INT_STAT_DRDY = 1 << 7, // 数据就绪中断已触发
} BMI088_AccIntStat_e;

/**
 * @brief ACC_CONF_REG (0x40) 加速度计配置寄存器
 * @note bit[7:4] - acc_bwp: 低通滤波器带宽设置
 * @note bit[3:0] - acc_odr: 输出数据速率
 * @note 访问: RW
 */
typedef enum
{
    // bit[7:4] - acc_bwp: 滤波器带宽设置
    BMI088_ACC_BWP_OSR4 = 0x08 << 4,   // 4倍过采样
    BMI088_ACC_BWP_OSR2 = 0x09 << 4,   // 2倍过采样
    BMI088_ACC_BWP_NORMAL = 0x0A << 4, // 正常模式

    // bit[3:0] - acc_odr: 输出数据速率
    BMI088_ACC_ODR_12_5 = 0x05, // 12.5 Hz
    BMI088_ACC_ODR_25 = 0x06,   // 25 Hz
    BMI088_ACC_ODR_50 = 0x07,   // 50 Hz
    BMI088_ACC_ODR_100 = 0x08,  // 100 Hz
    BMI088_ACC_ODR_200 = 0x09,  // 200 Hz
    BMI088_ACC_ODR_400 = 0x0A,  // 400 Hz
    BMI088_ACC_ODR_800 = 0x0B,  // 800 Hz
    BMI088_ACC_ODR_1600 = 0x0C, // 1600 Hz
} BMI088_AccConf_e;

/**
 * @brief ACC_RANGE_REG (0x41) 加速度计量程设置寄存器
 * @note bit[1:0] - acc_range: 测量量程
 * @note 访问: RW
 */
typedef enum
{
    BMI088_ACC_RANGE_3G = 0x00,  // ±3g
    BMI088_ACC_RANGE_6G = 0x01,  // ±6g
    BMI088_ACC_RANGE_12G = 0x02, // ±12g
    BMI088_ACC_RANGE_24G = 0x03, // ±24g
    BMI088_ACC_RANGE_NUM = 4,    // 加速度计量程数量
} BMI088_AccRange_e;

/**
 * @brief INT1_IO_CTRL_REG (0x53) INT1 引脚配置寄存器
 * @note bit[4] - int1_in: 启用 INT1 作为输入引脚
 * @note bit[3] - int1_out: 启用 INT1 作为输出引脚
 * @note bit[2] - int1_od: 引脚行为 (0=推挽, 1=开漏)
 * @note bit[1] - int1_lvl: 有效状态 (0=低电平有效, 1=高电平有效)
 * @note 访问: RW
 */
typedef enum
{
    // bit[4] - int1_in: 启用输入引脚
    BMI088_INT1_IN = 1 << 4, // 启用 INT1 作为输入引脚

    // bit[3] - int1_out: 启用输出引脚
    BMI088_INT1_OUT = 1 << 3, // 启用 INT1 作为输出引脚

    // bit[2] - int1_od: 引脚行为
    BMI088_INT1_OD_OPEN_DRAIN = 1 << 2, // 开漏输出 (默认推挽)

    // bit[1] - int1_lvl: 有效状态
    BMI088_INT1_LVL_HIGH = 1 << 1, // 高电平有效 (默认低电平)
} BMI088_Int1IoCtrl_e;

/**
 * @brief INT2_IO_CTRL_REG (0x54) INT2 引脚配置寄存器
 * @note bit[4] - int2_in: 启用 INT2 作为输入引脚
 * @note bit[3] - int2_out: 启用 INT2 作为输出引脚
 * @note bit[2] - int2_od: 引脚行为 (0=推挽, 1=开漏)
 * @note bit[1] - int2_lvl: 有效状态 (0=低电平有效, 1=高电平有效)
 * @note 访问: RW
 */
typedef enum
{
    // bit[4] - int2_in: 启用输入引脚
    BMI088_INT2_IN = 1 << 4, // 启用 INT2 作为输入引脚

    // bit[3] - int2_out: 启用输出引脚
    BMI088_INT2_OUT = 1 << 3, // 启用 INT2 作为输出引脚

    // bit[2] - int2_od: 引脚行为
    BMI088_INT2_OD_OPEN_DRAIN = 1 << 2, // 开漏输出 (默认推挽)

    // bit[1] - int2_lvl: 有效状态
    BMI088_INT2_LVL_HIGH = 1 << 1, // 高电平有效 (默认低电平)
} BMI088_Int2IoCtrl_e;

/**
 * @brief INT_MAP_DATA_REG (0x58) 中断映射寄存器
 * @note bit[6] - int2_drdy: 数据就绪中断映射到 INT2
 * @note bit[5] - int2_fwm: FIFO 水印中断映射到 INT2
 * @note bit[4] - int2_full: FIFO 满中断映射到 INT2
 * @note bit[2] - int1_drdy: 数据就绪中断映射到 INT1
 * @note bit[1] - int1_fwm: FIFO 水印中断映射到 INT1
 * @note bit[0] - int1_full: FIFO 满中断映射到 INT1
 * @note 访问: RW
 */
typedef enum
{
    // INT2 中断映射
    BMI088_INT2_DRDY = 1 << 6, // 数据就绪中断映射到 INT2
    BMI088_INT2_FWM = 1 << 5,  // FIFO 水印中断映射到 INT2
    BMI088_INT2_FULL = 1 << 4, // FIFO 满中断映射到 INT2

    // INT1 中断映射
    BMI088_INT1_DRDY = 1 << 2, // 数据就绪中断映射到 INT1
    BMI088_INT1_FWM = 1 << 1,  // FIFO 水印中断映射到 INT1
    BMI088_INT1_FULL = 1 << 0, // FIFO 满中断映射到 INT1
} BMI088_IntMapData_e;

/**
 * @brief ACC_SELF_TEST_REG (0x6D) 自检寄存器
 * @note 启用传感器自检信号，自检需要由用户主动关闭
 * @note 访问: RW
 */
typedef enum
{
    BMI088_ACC_SELF_TEST_OFF = 0x00, // 自检关闭
    BMI088_ACC_SELF_TEST_NEG = 0x09, // 启用负极性自检信号
    BMI088_ACC_SELF_TEST_POS = 0x0D, // 启用正极性自检信号
} BMI088_AccSelfTest_e;

/**
 * @brief ACC_PWR_CONF_REG (0x7C) 电源配置寄存器
 * @note bit[7:0] - acc_pwr_save: 电源保存模式
 * @note 访问: RW
 */
typedef enum
{
    BMI088_ACC_PWR_SAVE_ACTIVE = 0x00,  // 活动模式
    BMI088_ACC_PWR_SAVE_SUSPEND = 0x03, // 挂起模式
} BMI088_AccPwrConf_e;

/**
 * @brief ACC_PWR_CTRL_REG (0x7D) 电源控制寄存器
 * @note bit[7:0] - acc_enable: 加速度计使能
 * @note 访问: RW
 */
typedef enum
{
    BMI088_ACC_ENABLE_OFF = 0x00, // 加速度计关闭
    BMI088_ACC_ENABLE_ON = 0x04,  // 加速度计开启
} BMI088_AccPwrCtrl_e;

/**
 * @brief ACC_SOFTRESET_REG (0x7E) 软复位寄存器
 * @note 写入 0xB6 触发软复位，1ms 后所有配置恢复默认值
 * @note 访问: W (只写寄存器)
 */
#define BMI088_ACC_SOFTRESET_CMD 0xB6 // 软复位命令

/*------- 陀螺仪寄存器位定义 -------*/

/**
 * @brief GYRO_INT_STAT_1 (0x0A) 中断状态寄存器 1
 * @note bit[7] - gyro_drdy: 数据就绪中断状态
 * @note bit[4] - fifo_int: FIFO 中断状态
 * @note 访问: RO
 */
typedef enum
{
    // bit[7] - gyro_drdy: 数据就绪中断
    BMI088_GYRO_INT_STAT_DRDY = 1 << 7, // 数据就绪中断已触发

    // bit[4] - fifo_int: FIFO 中断
    BMI088_GYRO_INT_STAT_FIFO = 1 << 4, // FIFO 中断已触发
} BMI088_GyroIntStat_e;

/**
 * @brief GYRO_RANGE (0x0F) 陀螺仪量程设置寄存器
 * @note bit[7:0] - gyro_range: 测量量程
 * @note 访问: RW
 */
typedef enum
{
    BMI088_GYRO_RANGE_2000 = 0x00, // ±2000°/s
    BMI088_GYRO_RANGE_1000 = 0x01, // ±1000°/s
    BMI088_GYRO_RANGE_500 = 0x02,  // ±500°/s
    BMI088_GYRO_RANGE_250 = 0x03,  // ±250°/s
    BMI088_GYRO_RANGE_125 = 0x04,  // ±125°/s
    BMI088_GYRO_RANGE_NUM = 5,     // 陀螺仪量程数量
} BMI088_GyroRange_e;

/**
 * @brief GYRO_BANDWIDTH (0x10) 陀螺仪带宽配置寄存器
 * @note bit[7:0] - gyro_bw: 输出数据速率和滤波器带宽
 * @note 访问: RW
 */
typedef enum
{
    BMI088_GYRO_BW_2000_532 = 0x00, // ODR=2000Hz, BW=532Hz
    BMI088_GYRO_BW_2000_230 = 0x01, // ODR=2000Hz, BW=230Hz
    BMI088_GYRO_BW_1000_116 = 0x02, // ODR=1000Hz, BW=116Hz
    BMI088_GYRO_BW_400_47 = 0x03,   // ODR=400Hz, BW=47Hz
    BMI088_GYRO_BW_200_23 = 0x04,   // ODR=200Hz, BW=23Hz
    BMI088_GYRO_BW_100_12 = 0x05,   // ODR=100Hz, BW=12Hz
    BMI088_GYRO_BW_200_64 = 0x06,   // ODR=200Hz, BW=64Hz
    BMI088_GYRO_BW_100_32 = 0x07,   // ODR=100Hz, BW=32Hz
} BMI088_GyroBandwidth_e;

/**
 * @brief GYRO_LPM1 (0x11) 低功耗模式配置寄存器
 * @note bit[7:0] - gyro_pm: 电源模式
 * @note 访问: RW
 */
typedef enum
{
    BMI088_GYRO_PM_NORMAL = 0x00,       // 正常模式
    BMI088_GYRO_PM_SUSPEND = 0x80,      // 挂起模式
    BMI088_GYRO_PM_DEEP_SUSPEND = 0x20, // 深度挂起模式
} BMI088_GyroPm_e;

/**
 * @brief GYRO_SOFTRESET (0x14) 软复位寄存器
 * @note 写入 0xB6 触发软复位，30ms 后所有配置恢复默认值
 * @note 访问: W (只写寄存器)
 */
#define BMI088_GYRO_SOFTRESET_CMD 0xB6 // 软复位命令

/**
 * @brief GYRO_INT_CTRL (0x15) 中断控制寄存器
 * @note bit[7] - data_en: 启用新数据中断
 * @note bit[6] - fifo_en: 启用 FIFO 中断
 * @note 访问: RW
 */
typedef enum
{
    // bit[7] - data_en: 新数据中断使能
    BMI088_GYRO_INT_DATA_EN = 1 << 7, // 启用新数据中断

    // bit[6] - fifo_en: FIFO 中断使能
    BMI088_GYRO_INT_FIFO_EN = 1 << 6, // 启用 FIFO 中断
} BMI088_GyroIntCtrl_e;

/**
 * @brief INT3_INT4_IO_CONF (0x16) INT3/INT4 引脚配置寄存器
 * @note bit[3] - int4_od: INT4 引脚行为 (0=推挽, 1=开漏)
 * @note bit[2] - int4_lvl: INT4 有效状态 (0=低电平有效, 1=高电平有效)
 * @note bit[1] - int3_od: INT3 引脚行为 (0=推挽, 1=开漏)
 * @note bit[0] - int3_lvl: INT3 有效状态 (0=低电平有效, 1=高电平有效)
 * @note 访问: RW
 */
typedef enum
{
    // bit[3] - int4_od: INT4 引脚行为
    BMI088_INT4_OD_OPEN_DRAIN = 1 << 3, // INT4 开漏输出 (默认推挽)

    // bit[2] - int4_lvl: INT4 有效状态
    BMI088_INT4_LVL_HIGH = 1 << 2, // INT4 高电平有效 (默认低电平)

    // bit[1] - int3_od: INT3 引脚行为
    BMI088_INT3_OD_OPEN_DRAIN = 1 << 1, // INT3 开漏输出 (默认推挽)

    // bit[0] - int3_lvl: INT3 有效状态
    BMI088_INT3_LVL_HIGH = 1 << 0, // INT3 高电平有效 (默认低电平)
} BMI088_Int3Int4IoConf_e;

/**
 * @brief INT3_INT4_IO_MAP (0x18) INT3/INT4 中断映射寄存器
 * @note bit[7] - int4_data: 数据就绪中断映射到 INT4
 * @note bit[5] - int4_fifo: FIFO 中断映射到 INT4
 * @note bit[2] - int3_fifo: FIFO 中断映射到 INT3
 * @note bit[0] - int3_data: 数据就绪中断映射到 INT3
 * @note 访问: RW
 */
typedef enum
{
    // INT4 中断映射
    BMI088_INT4_DATA = 1 << 7, // 数据就绪中断映射到 INT4
    BMI088_INT4_FIFO = 1 << 5, // FIFO 中断映射到 INT4

    // INT3 中断映射
    BMI088_INT3_FIFO = 1 << 2, // FIFO 中断映射到 INT3
    BMI088_INT3_DATA = 1 << 0, // 数据就绪中断映射到 INT3
} BMI088_Int3Int4IoMap_e;

/**
 * @brief GYRO_SELF_TEST (0x3C) 自检寄存器
 * @note bit[4] - rate_ok: 传感器功能正常标志 (RO)
 * @note bit[2] - bist_fail: 自检失败标志 (RO)
 * @note bit[1] - bist_rdy: 自检完成标志 (RO)
 * @note bit[0] - trig_bist: 触发自检 (W)
 * @note 访问: RW (部分位只读/只写)
 */
typedef enum
{
    // bit[4] - rate_ok: 传感器功能正常 (RO)
    BMI088_GYRO_SELF_TEST_RATE_OK = 1 << 4, // 传感器功能正常

    // bit[2] - bist_fail: 自检失败 (RO)
    BMI088_GYRO_SELF_TEST_BIST_FAIL = 1 << 2, // 自检失败

    // bit[1] - bist_rdy: 自检完成 (RO)
    BMI088_GYRO_SELF_TEST_BIST_RDY = 1 << 1, // 自检完成

    // bit[0] - trig_bist: 触发自检 (W)
    BMI088_GYRO_SELF_TEST_TRIG = 1 << 0, // 触发内置自检
} BMI088_GyroSelfTest_e;

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