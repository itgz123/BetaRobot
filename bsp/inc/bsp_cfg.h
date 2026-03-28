/**
 * @file bsp_cfg.h
 * @brief BSP层配置文件：板载资源定义、实例数量配置、硬件映射
 *
 * @note 本文件职责：
 *       1. 定义板载资源枚举（GPIO/TIM/UART/CAN/SPI/I2C等）
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
 *              通用配置
 *============================================*/

#define USART_BLOCK_TIMEOUT_MS 100 // 阻塞模式发送超时时间（毫秒）
#define SPI_BLOCK_TIMEOUT_MS 100   // 阻塞模式发送超时时间（毫秒）

/*============================================
 *              硬件特性配置
 *============================================*/
/**
 * @note CPU频率和硬件加速特性由MCU型号决定，不可修改
 */
#define CORTEX_M4 0
#define CORTEX_M7 1

#if DEVELOPMENT_BOARD == STM32F407VET6
#define CPU_FREQ_MHZ 168   // CPU频率 MHz
#define CPU_CORE CORTEX_M4 // Cortex-M4 内核
#define HAS_FPU 1          // Cortex-M4F 单精度浮点
#define HAS_DSP 1          // DSP指令集
#define HAS_CORDIC 0       // 无CORDIC协处理器
#define HAS_CRC 1          // 硬件CRC
#define HAS_FMAC 0         // 无滤波数学加速器
#define HAS_MPU 1          // 内存保护单元（可选）
#define HAS_RAMECC 0       // 无RAM错误校正
#endif

#if DEVELOPMENT_BOARD == DM_MC02
#define CPU_FREQ_MHZ 480   // CPU频率 MHz
#define CPU_CORE CORTEX_M7 // Cortex-M7 内核
#define HAS_FPU 1          // Cortex-M7 双精度浮点
#define HAS_DSP 1          // DSP指令集
#define HAS_CORDIC 1       // CORDIC协处理器
#define HAS_CRC 1          // 硬件CRC
#define HAS_FMAC 0         // 滤波数学加速器（暂时禁用）
#define HAS_MPU 1          // 内存保护单元
#define HAS_RAMECC 1       // RAM错误校正
#endif

#if DEVELOPMENT_BOARD == DJI_A
#define CPU_FREQ_MHZ 180   // CPU频率 MHz
#define CPU_CORE CORTEX_M4 // Cortex-M4 内核
#define HAS_FPU 1          // Cortex-M4F 单精度浮点
#define HAS_DSP 1          // DSP指令集
#define HAS_CORDIC 0       // 无CORDIC协处理器
#define HAS_CRC 1          // 硬件CRC
#define HAS_FMAC 0         // 无滤波数学加速器
#define HAS_MPU 1          // 内存保护单元（可选）
#define HAS_RAMECC 0       // 无RAM错误校正
#endif

#if DEVELOPMENT_BOARD == DJI_C
#define CPU_FREQ_MHZ 168   // CPU频率 MHz
#define CPU_CORE CORTEX_M4 // Cortex-M4 内核
#define HAS_FPU 1          // Cortex-M4F 单精度浮点
#define HAS_DSP 1          // DSP指令集
#define HAS_CORDIC 0       // 无CORDIC协处理器
#define HAS_CRC 1          // 硬件CRC
#define HAS_FMAC 0         // 无滤波数学加速器
#define HAS_MPU 1          // 内存保护单元（可选）
#define HAS_RAMECC 0       // 无RAM错误校正
#endif

/*============================================
 *              逻辑实例数量配置
 *============================================*/
/**
 * @note 定义APP/DRV层可注册的最大实例数，根据实际使用调整
 *       总线类外设：多个逻辑实例可共用同一物理硬件
 *       独占类外设：逻辑实例与物理硬件一一对应
 */

#if DEVELOPMENT_BOARD == STM32F407VET6
#define CAN_INSTANCE_NUM 0     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0     // I2C 设备数量
#define SPI_INSTANCE_NUM 0     // SPI 设备数量
#define GPIO_INSTANCE_NUM 10   // GPIO 实例数量
#define UART_INSTANCE_NUM 1    // UART 实例数量
#define PWM_INSTANCE_NUM 4     // PWM 实例数量
#define ENCODER_INSTANCE_NUM 4 // 编码器实例数量
#endif

#if DEVELOPMENT_BOARD == DM_MC02
#define CAN_INSTANCE_NUM 0     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0     // I2C 设备数量
#define SPI_INSTANCE_NUM 1     // SPI 设备数量（BMI088）
#define GPIO_INSTANCE_NUM 11   // GPIO 实例数量
#define UART_INSTANCE_NUM 8    // UART 实例数量
#define PWM_INSTANCE_NUM 6     // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量
#define ADC_INSTANCE_NUM 1     // ADC 实例数量
#endif

#if DEVELOPMENT_BOARD == DJI_A
#define CAN_INSTANCE_NUM 0     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0     // I2C 设备数量
#define SPI_INSTANCE_NUM 0     // SPI 设备数量
#define GPIO_INSTANCE_NUM 0    // GPIO 实例数量
#define UART_INSTANCE_NUM 0    // UART 实例数量
#define PWM_INSTANCE_NUM 0     // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量
#endif

#if DEVELOPMENT_BOARD == DJI_C
#define CAN_INSTANCE_NUM 0     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 0     // I2C 设备数量
#define SPI_INSTANCE_NUM 0     // SPI 设备数量
#define GPIO_INSTANCE_NUM 0    // GPIO 实例数量
#define UART_INSTANCE_NUM 0    // UART 实例数量
#define PWM_INSTANCE_NUM 0     // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量
#endif

/*============================================
 *              板载资源枚举
 *============================================*/

#if DEVELOPMENT_BOARD == STM32F407VET6

/**
 * @brief 板载GPIO枚举
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

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

#endif // STM32F407VET6

#if DEVELOPMENT_BOARD == DM_MC02

/**
 * @brief 板载GPIO枚举
 */
typedef enum
{
    // BMI088 控制
    GPIO_BMI088_CS1 = 0, // BMI088 CS1 PC0
    GPIO_BMI088_CS2,     // BMI088 CS2 PC3
    GPIO_BMI088_INT1,    // BMI088 INT1 PE10
    GPIO_BMI088_INT3,    // BMI088 INT3 PE12

    // 电源控制
    GPIO_POWER_24V_OUT1, // 24V输出1 PC14
    GPIO_POWER_24V_OUT2, // 24V输出2 PC13
    GPIO_POWER_5V_EN,    // 5V使能 PC15

    // 按键输入
    GPIO_KEY_LCD_1, // LCD按键1 PA5
    GPIO_KEY_LCD_2, // LCD按键2 PD10
    GPIO_KEY_EX,    // 扩展按键 PA15
    GPIO_KEY_USER,  // 用户按键 PE14

    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 */
typedef enum
{
    // PWM定时器
    TIM_PWM_1 = 0, // PWM输出1 TIM2_CH1 PA0
    TIM_PWM_2,     // PWM输出2 TIM2_CH3 PA2
    TIM_PWM_3,     // PWM输出3 TIM1_CH1 PE9
    TIM_PWM_4,     // PWM输出4 TIM1_CH3 PE13

    // 特殊功能定时器
    TIM_BUZZER, // 蜂鸣器 TIM12_CH2 PB15
    TIM_HEATER, // BMI088加热 TIM3_CH4 PB1

    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_DEBUG = 0, // 调试串口 USART1 PA9/PA10
    UART_DBUS,      // 遥控接收 UART5 PD2
    UART_RS485_1,   // RS485-1 USART2 PD4/PD5/PD6
    UART_RS485_2,   // RS485-2 USART3 PB14/PD8/PD9
    UART_EX_1,      // 扩展串口1 UART7 PE7/PE8
    UART_EX_2,      // 扩展串口2 UART8 PE0/PE1
    UART_EX_3,      // 扩展串口3 UART9 PD14/PD15
    UART_EX_4,      // 扩展串口4 USART10 PE2/PE3

    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_1 = 0, // CAN1 PD0/PD1
    CAN_2,     // CAN2 PB5/PB6
    CAN_3,     // CAN3 PD12/PD13

    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_LCD = 0, // SPI1 LCD PB3/PB4/PD7/PE15
    SPI_BMI088,  // SPI2 BMI088 PB13/PC1/PC2

    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_LCD = 0, // I2C2 LCD PB10/PB11

    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

/**
 * @brief 板载ADC枚举
 */
typedef enum
{
    ADC_BAT_VOLTAGE = 0, // 电池电压 PC04 -> ADC1_INP4

    ADC_NUM_MAX // ADC数量上限
} BoardADC_e;
// 电压值转换(3.3*11/65536=0.00055389404296875)
#define VOLTAGE_TRANFER 0.00055389404296875f

#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_A

/**
 * @brief 板载GPIO枚举
 */
typedef enum
{
    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 */
typedef enum
{
    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C

/**
 * @brief 板载GPIO枚举
 */
typedef enum
{
    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 */
typedef enum
{
    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

#endif // DJI_C

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

/**
 * @brief CAN映射结构体
 */
// typedef struct
// {
//     CAN_HandleTypeDef *handle; // CAN句柄
// } CAN_Map_t;

/**
 * @brief SPI映射结构体
 */
typedef struct
{
    SPI_HandleTypeDef *handle; // SPI句柄
} SPI_Map_t;

/**
 * @brief I2C映射结构体
 */
typedef struct
{
    I2C_HandleTypeDef *handle; // I2C句柄
} I2C_Map_t;

/**
 * @brief ADC映射结构体
 */
typedef struct
{
    ADC_HandleTypeDef *handle; // ADC句柄
    uint32_t channel;          // ADC通道
} ADC_Map_t;

/*============================================
 *              硬件映射数组声明
 *============================================*/

extern const GPIO_Map_t gpio_map[];
extern const TIM_Map_t tim_map[];
extern const UART_Map_t uart_map[];
// extern const CAN_Map_t can_map[];
extern const SPI_Map_t spi_map[];
extern const I2C_Map_t i2c_map[];
extern const ADC_Map_t adc_map[];

#endif // __BSP_CFG_H
