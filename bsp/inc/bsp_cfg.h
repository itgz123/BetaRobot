/**
 * @file bsp_cfg.h
 * @brief BSP层配置文件：板载资源定义、实例数量配置、硬件映射
 *
 * @note 本文件职责：
 *       1. 定义板载资源枚举（GPIO/TIM/UART等）
 *       2. 声明硬件映射数组和获取接口
 *       3. 根据开发板自动配置实例数量
 *
 * @note 切换开发板：只需修改 app_cfg.h 中的 DEVELOPMENT_BOARD 宏
 *       本文件会自动适配，无需修改
 *
 * @section 设计原则
 *
 * @subsection 枚举命名统一
 * - 板载枚举使用**功能命名**，而非物理引脚命名
 * - 示例：GPIO_LED_GREEN（功能）而非 GPIO_PE14（引脚）
 * - 目的：切换开发板后 APP/DRV 层代码无需修改
 *
 * @subsection 竞争问题处理
 * - 总线类外设（CAN/I2C/SPI）可能被多个逻辑实例共用同一物理硬件
 * - 发送前必须检查硬件状态：if (XXX_IsReady(instance)) { XXX_Transmit(...); }
 * - BSP 层提供状态查询接口，避免发送冲突
 *
 * @subsection 数量限制设计
 * - 物理硬件数量：由枚举的 _NUM_MAX 自动统计（如 GPIO_NUM_MAX、UART_NUM_MAX）
 * - 逻辑实例数量：用宏 XXX_INSTANCE_NUM 手动配置（如 GPIO_INSTANCE_NUM）
 * - 一个物理硬件可承载多个逻辑实例（如多个 CAN 订阅者共用一个 CAN 总线）
 */

#ifndef __BSP_CFG_H
#define __BSP_CFG_H

#include "app_cfg.h"
#include "main.h"

/*============================================
 *              板载资源枚举
 *============================================*/

/**
 * @brief 板载GPIO枚举
 * @note 用于索引板级映射数组，APP层通过此枚举访问GPIO
 */
typedef enum
{
    // LED
    GPIO_LED_GREEN = 0, // LED绿 PE14
    GPIO_LED_RED,       // LED红 PA1

    // 电机方向控制
    GPIO_MOTOR_1_IN1, // 电机1 IN1 PB12
    GPIO_MOTOR_1_IN2, // 电机1 IN2 PB13
    GPIO_MOTOR_2_IN1, // 电机2 IN1 PB14
    GPIO_MOTOR_2_IN2, // 电机2 IN2 PB15
    GPIO_MOTOR_3_IN1, // 电机3 IN1 PC9
    GPIO_MOTOR_3_IN2, // 电机3 IN2 PC10
    GPIO_MOTOR_4_IN1, // 电机4 IN1 PC11
    GPIO_MOTOR_4_IN2, // 电机4 IN2 PC12

    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 * @note PWM每个通道单独枚举，编码器单独枚举
 */
typedef enum
{
    // PWM定时器（TIM1 4通道）
    TIM_PWM_1 = 0, // 电机PWM CH1
    TIM_PWM_2,     // 电机PWM CH2
    TIM_PWM_3,     // 电机PWM CH3
    TIM_PWM_4,     // 电机PWM CH4

    // 编码器定时器
    TIM_ENCODER_1, // 编码器1 TIM2
    TIM_ENCODER_2, // 编码器2 TIM3
    TIM_ENCODER_3, // 编码器3 TIM4
    TIM_ENCODER_4, // 编码器4 TIM8

    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_SBUS = 0, // SBUS遥控接收 UART2

    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/*============================================
 *              硬件映射结构体
 *============================================*/

/**
 * @brief GPIO映射结构体
 */
typedef struct
{
    GPIO_TypeDef *port; // GPIO端口
    uint16_t pin;       // 引脚号
} GPIO_Map_t;

/**
 * @brief TIM映射结构体
 */
typedef struct
{
    TIM_HandleTypeDef *htim; // TIM句柄
    uint32_t channel;        // 通道（仅PWM使用，编码器填0）
} TIM_Map_t;

/**
 * @brief UART映射结构体
 */
typedef struct
{
    UART_HandleTypeDef *handle; // UART句柄
} UART_Map_t;

/*============================================
 *              硬件映射数组声明
 *============================================*/

extern const GPIO_Map_t gpio_map[];
extern const TIM_Map_t tim_map[];
extern const UART_Map_t uart_map[];

/*============================================
 *              BSP通用配置
 *============================================*/

#define USART_BLOCK_TIMEOUT_MS 100 // 阻塞模式发送超时时间（毫秒）

/*============================================
 *              开发板硬件配置
 *============================================*/

#if DEVELOPMENT_BOARD == STM32F407VET6
/*---------- CPU频率 ----------*/
#define CPU_FREQ_MHZ 168

/*---------- 逻辑实例数量（APP/DRV 层可注册的最大实例数）----------*/
/* 总线类外设：多个逻辑实例可共用同一物理硬件 */
#define CAN_INSTANCE_NUM 0 // CAN 订阅者数量（如电机驱动、IMU 等）
#define I2C_INSTANCE_NUM 0 // I2C 设备数量（如传感器、EEPROM 等）
#define SPI_INSTANCE_NUM 0 // SPI 设备数量（如 Flash、屏幕等）

/* 独占类外设：逻辑实例与物理硬件一一对应 */
#define GPIO_INSTANCE_NUM 10 // GPIO 实例数量
#define UART_INSTANCE_NUM 1  // UART 实例数量
#define TIM_INSTANCE_NUM 8   // TIM 实例数量（4 PWM + 4 编码器）

/*---------- 硬件加速特性 ----------*/
#define HAS_FPU 1    // Cortex-M4F 单精度浮点
#define HAS_DSP 1    // DSP指令集
#define HAS_CORDIC 0 // 无CORDIC协处理器
#define HAS_CRC 1    // 硬件CRC
#define HAS_FMAC 0   // 无滤波数学加速器
#define HAS_MPU 1    // 内存保护单元（可选）
#define HAS_RAMECC 0 // 无RAM错误校正

/*---------- CMSIS-DSP 库支持 ----------*/
#if HAS_DSP
#define HAS_CMSIS_DSP 1 // 有DSP指令集时可使用CMSIS-DSP加速
#else
#define HAS_CMSIS_DSP 0
#endif

#endif // STM32F407VET6

#if DEVELOPMENT_BOARD == DM_MC02
/*---------- CPU频率 ----------*/
#define CPU_FREQ_MHZ 550

/*---------- 逻辑实例数量（APP/DRV 层可注册的最大实例数）----------*/
/* 总线类外设：多个逻辑实例可共用同一物理硬件 */
#define CAN_INSTANCE_NUM 0 // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0 // I2C 设备数量
#define SPI_INSTANCE_NUM 0 // SPI 设备数量

/* 独占类外设：逻辑实例与物理硬件一一对应 */
#define GPIO_INSTANCE_NUM 0 // GPIO 实例数量
#define UART_INSTANCE_NUM 0 // UART 实例数量
#define TIM_INSTANCE_NUM 0  // TIM 实例数量（根据实际配置）

/*---------- 硬件加速特性 ----------*/
#define HAS_FPU 1    // Cortex-M7 双精度浮点
#define HAS_DSP 1    // DSP指令集
#define HAS_CORDIC 1 // CORDIC协处理器
#define HAS_CRC 1    // 硬件CRC
#define HAS_FMAC 1   // 滤波数学加速器
#define HAS_MPU 1    // 内存保护单元
#define HAS_RAMECC 1 // RAM错误校正

/*---------- CMSIS-DSP 库支持 ----------*/
#if HAS_DSP
#define HAS_CMSIS_DSP 1
#else
#define HAS_CMSIS_DSP 0
#endif

#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_A
/*---------- CPU频率 ----------*/
#define CPU_FREQ_MHZ 180

/*---------- 逻辑实例数量（APP/DRV 层可注册的最大实例数）----------*/
/* 总线类外设：多个逻辑实例可共用同一物理硬件 */
#define CAN_INSTANCE_NUM 0 // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0 // I2C 设备数量
#define SPI_INSTANCE_NUM 0 // SPI 设备数量

/* 独占类外设：逻辑实例与物理硬件一一对应 */
#define GPIO_INSTANCE_NUM 0 // GPIO 实例数量
#define UART_INSTANCE_NUM 0 // UART 实例数量
#define TIM_INSTANCE_NUM 0  // TIM 实例数量（根据实际配置）

/*---------- 硬件加速特性 ----------*/
#define HAS_FPU 1    // Cortex-M4F 单精度浮点
#define HAS_DSP 1    // DSP指令集
#define HAS_CORDIC 0 // 无CORDIC协处理器
#define HAS_CRC 1    // 硬件CRC
#define HAS_FMAC 0   // 无滤波数学加速器
#define HAS_MPU 1    // 内存保护单元（可选）
#define HAS_RAMECC 0 // 无RAM错误校正

/*---------- CMSIS-DSP 库支持 ----------*/
#if HAS_DSP
#define HAS_CMSIS_DSP 1
#else
#define HAS_CMSIS_DSP 0
#endif

#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C
/*---------- CPU频率 ----------*/
#define CPU_FREQ_MHZ 168

/*---------- 逻辑实例数量（APP/DRV 层可注册的最大实例数）----------*/
/* 总线类外设：多个逻辑实例可共用同一物理硬件 */
#define CAN_INSTANCE_NUM 0 // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0 // I2C 设备数量
#define SPI_INSTANCE_NUM 0 // SPI 设备数量

/* 独占类外设：逻辑实例与物理硬件一一对应 */
#define GPIO_INSTANCE_NUM 00 // GPIO 实例数量
#define UART_INSTANCE_NUM 0  // UART 实例数量
#define TIM_INSTANCE_NUM 0   // TIM 实例数量（根据实际配置）

/*---------- 硬件加速特性 ----------*/
#define HAS_FPU 1    // Cortex-M4F 单精度浮点
#define HAS_DSP 1    // DSP指令集
#define HAS_CORDIC 0 // 无CORDIC协处理器
#define HAS_CRC 1    // 硬件CRC
#define HAS_FMAC 0   // 无滤波数学加速器
#define HAS_MPU 1    // 内存保护单元（可选）
#define HAS_RAMECC 0 // 无RAM错误校正

/*---------- CMSIS-DSP 库支持 ----------*/
#if HAS_DSP
#define HAS_CMSIS_DSP 1
#else
#define HAS_CMSIS_DSP 0
#endif

#endif // DJI_C

#endif // __BSP_CFG_H