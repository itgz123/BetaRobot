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
#define HAS_FMAC 1         // 滤波数学加速器
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
 *              内存属性宏
 *============================================*/
/**
 * @brief 内存区域属性宏定义
 * @note Cortex-M7 特有内存区域：
 *       - ITCM_RAM：指令紧耦合内存，CPU取指最快，用于高频任务函数
 *       - DMA_RAM：D1域RAM，DMA可访问，用于DMA缓冲区
 *       Cortex-M4 无这些区域，定义为空
 */
#if CPU_CORE == CORTEX_M7
#define ITCM_RAM __attribute__((section(".itcmram")))
#define DMA_RAM __attribute__((section(".ram_d1")))
#else
#define ITCM_RAM
#define DMA_RAM
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
#define ADC_INSTANCE_NUM 0     // ADC 实例数量
#endif

#if DEVELOPMENT_BOARD == DM_MC02
#define CAN_INSTANCE_NUM 3     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 1     // I2C 设备数量
#define SPI_INSTANCE_NUM 2     // SPI 设备数量（BMI088）
#define GPIO_INSTANCE_NUM 11   // GPIO 实例数量
#define UART_INSTANCE_NUM 8    // UART 实例数量
#define PWM_INSTANCE_NUM 6     // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量
#define ADC_INSTANCE_NUM 1     // ADC 实例数量
#endif

#if DEVELOPMENT_BOARD == DJI_A
#define CAN_INSTANCE_NUM 2     // CAN 订阅者数量
#define I2C_INSTANCE_NUM 1     // I2C 设备数量
#define SPI_INSTANCE_NUM 3     // SPI 设备数量
#define GPIO_INSTANCE_NUM 16   // GPIO 实例数量
#define UART_INSTANCE_NUM 6    // UART 实例数量
#define PWM_INSTANCE_NUM 22    // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量
#define ADC_INSTANCE_NUM 11    // ADC 实例数量
#define DAC_INSTANCE_NUM 2     // DAC 实例数量
#endif

#if DEVELOPMENT_BOARD == DJI_C
#define CAN_INSTANCE_NUM 2     // CAN 订阅者数量（无CAN）
#define I2C_INSTANCE_NUM 2     // I2C 设备数量（IST8310磁力计）
#define SPI_INSTANCE_NUM 2     // SPI 设备数量（BMI088 IMU）
#define GPIO_INSTANCE_NUM 7    // GPIO 实例数量
#define UART_INSTANCE_NUM 3    // UART 实例数量
#define PWM_INSTANCE_NUM 13    // PWM 实例数量
#define ENCODER_INSTANCE_NUM 0 // 编码器实例数量（无编码器）
#define ADC_INSTANCE_NUM 1     // ADC 实例数量（电压检测）
#endif

/*============================================
 *              BSP 模块使能宏
 *============================================*/
/**
 * @note 根据 HAL 配置自动定义 BSP 模块使能（只要硬件存在就定义）
 *       供所有 BSP 和 DRV 层文件进行条件编译
 *       xxx_INSTANCE_NUM 仅用于 bsp_xxx.c 中定义实例池大小
 */

#ifdef HAL_GPIO_MODULE_ENABLED
#define BSP_GPIO_MODULE_ENABLED
#endif

#ifdef HAL_TIM_MODULE_ENABLED
#define BSP_TIM_MODULE_ENABLED
#endif

#ifdef HAL_UART_MODULE_ENABLED
#define BSP_UART_MODULE_ENABLED
#endif

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)
#define BSP_CAN_MODULE_ENABLED
#endif

#ifdef HAL_SPI_MODULE_ENABLED
#define BSP_SPI_MODULE_ENABLED
#endif

#ifdef HAL_I2C_MODULE_ENABLED
#define BSP_I2C_MODULE_ENABLED
#endif

#ifdef HAL_ADC_MODULE_ENABLED
#define BSP_ADC_MODULE_ENABLED
#endif

#ifdef HAL_DAC_MODULE_ENABLED
#define BSP_DAC_MODULE_ENABLED
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
    GPIO_LED_G = 0, // LED绿 PE14
    GPIO_LED_R,     // LED红 PA1

    // 电机方向控制
    GPIO_MOTOR1_IN1, // 电机1 IN1 PB12
    GPIO_MOTOR1_IN2, // 电机1 IN2 PB13
    GPIO_MOTOR2_IN1, // 电机2 IN1 PB14
    GPIO_MOTOR2_IN2, // 电机2 IN2 PB15
    GPIO_MOTOR3_IN1, // 电机3 IN1 PC9
    GPIO_MOTOR3_IN2, // 电机3 IN2 PC10
    GPIO_MOTOR4_IN1, // 电机4 IN1 PC11
    GPIO_MOTOR4_IN2, // 电机4 IN2 PC12

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
    UART_SBUS = 0, // SBUS/DBUS遥控接收 UART2

    UART_NUM_MAX // UART数量上限
} BoardUART_e;

#endif // STM32F407VET6

#if DEVELOPMENT_BOARD == DM_MC02

/**
 * @brief 板载GPIO枚举
 */
typedef enum
{
    // BMI088 控制
    GPIO_BMI088_CS_ACCEL = 0, // BMI088 CS_Accel PC0
    GPIO_BMI088_CS_GYRO,      // BMI088 CS_Gyro PC3
    GPIO_BMI088_INT_ACCEL,    // BMI088 INT1_Accel PE10
    GPIO_BMI088_INT_GYRO,     // BMI088 INT3_Gyro PE12

    // 电源控制
    GPIO_POWER_24V_EN1, // 24V输出1 PC14
    GPIO_POWER_24V_EN2, // 24V输出2 PC13
    GPIO_POWER_5V_EN,   // 5V使能 PC15

    // 按键输入
    GPIO_LCD_KEY1, // LCD按键1 PA5
    GPIO_LCD_KEY2, // LCD按键2 PD10
    GPIO_EX_KEY,   // 扩展按键 PA15
    GPIO_USER_KEY, // 用户按键 PE14

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
    TIM_HEATER, // BMI088加热 TIM3_CH4 PB1
    TIM_BUZZER, // 蜂鸣器 TIM12_CH2 PB15

    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_1 = 0,   // 调试串口 USART1 PA9/PA10
    UART_SBUS,    // SBUS/DBUS遥控接收 UART5 PD2
    UART_RS485_2, // RS485-1 USART2 PD4/PD5/PD6
    UART_RS485_3, // RS485-2 USART3 PB14/PD8/PD9
    UART_7,       // 扩展串口1 UART7 PE7/PE8
    UART_EX_8,    // 扩展串口2 UART8 PE0/PE1
    UART_EX_9,    // 扩展串口3 UART9 PD14/PD15
    UART_10,      // 扩展串口4 USART10 PE2/PE3

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
    SPI_LCD_1 = 0, // SPI1 LCD PB3/PB4/PD7/PE15
    SPI_BMI088,    // SPI2 BMI088 PB13/PC1/PC2

    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_LCD_2 = 0, // I2C2 LCD PB10/PB11

    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

/**
 * @brief 板载ADC枚举
 */
typedef enum
{
    ADC_BAT = 0, // 电池电压 PC04 -> ADC1_INP4

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
    // OLED
    GPIO_OLED_KEY = 0, // OLED按键 PA6
    GPIO_OLED_DC,      // OLED DC PB9
    GPIO_OLED_RST,     // OLED复位 PB10

    // 按键
    GPIO_USER_KEY, // 按键 PB2

    // MPU6500
    GPIO_MPU6500_INT, // MPU6500中断 PB8

    // IST8310
    GPIO_IST8310_RSTN, // IST8310复位 PE2
    GPIO_IST8310_DRDY, // IST8310数据就绪 PE3

    // LED
    GPIO_LED_R, // LED红 PE11
    GPIO_LED_G, // LED绿 PF14

    // SD卡
    GPIO_SD_EXTI, // SD卡检测 PE15

    // 激光
    GPIO_LASER, // 激光 PG13

    // 电源控制
    GPIO_POWER_EN1, // 电源控制1 PH2
    GPIO_POWER_EN2, // 电源控制2 PH3
    GPIO_POWER_EN3, // 电源控制3 PH4
    GPIO_POWER_EN4, // 电源控制4 PH5

    // DAC外部中断
    GPIO_DAC_EXTI, // DAC外部中断 PI9

    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 */
typedef enum
{
    // 特殊功能定时器
    TIM_HEATER = 0, // MPU6500加热 TIM3_CH4 PB1
    TIM_BUZZER,     // 蜂鸣器 TIM12_CH1 PH6

    TIM_PWM_3Pin1, // PWM CH1 PA8
    TIM_PWM_3Pin2, // PWM CH2 PA9
    TIM_PWM_3Pin3, // PWM CH3 PE13
    TIM_PWM_3Pin4, // PWM CH4 PE14

    TIM_PWM_1, // PWM CH5 PA0
    TIM_PWM_2, // PWM CH6 PA1
    TIM_PWM_3, // PWM CH7 PA2
    TIM_PWM_4, // PWM CH8 PA3

    TIM_PWM_5, // PWM CH10 PD12
    TIM_PWM_6, // PWM CH11 PD13
    TIM_PWM_7, // PWM CH12 PD14
    TIM_PWM_8, // PWM CH13 PD15

    TIM_PWM_9,  // PWM CH14 PH10
    TIM_PWM_10, // PWM CH15 PH11
    TIM_PWM_11, // PWM CH16 PH12
    TIM_PWM_12, // PWM CH17 PI0

    TIM_PWM_13, // PWM CH18 PI5
    TIM_PWM_14, // PWM CH19 PI6
    TIM_PWM_15, // PWM CH20 PI7
    TIM_PWM_16, // PWM CH21 PI2

    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_SBUS = 0, // SBUS遥控接收 USART1 PB6/PB7
    UART_2,        // 扩展串口 USART2 PD5/PD6
    UART_3,        // 扩展串口 USART3 PD8/PD9
    UART_7,        // 扩展串口 UART7 PE7/PE8
    UART_8,        // 扩展串口 UART8 PE0/PE1
    UART_4Pin_6,   // 扩展串口 USART6 PG9/PG14

    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_1 = 0, // CAN1 PD0/PD1
    CAN_2,     // CAN2 PB12/PB13

    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_OLED = 0, // SPI1 TX Only PB3/PA7
    SPI_EX_4,     // SPI4 Full Duplex PE4/PE5/PE6/PE12
    SPI_MPU6500,  // SPI5 Full Duplex PF6/PF7/PF8/PF9

    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_EX_2 = 0, // I2C2 PF0/PF1

    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

/**
 * @brief 板载ADC枚举
 */
typedef enum
{
    // ADC1 通道
    ADC1_EX_8 = 0, // ADC1_IN8 PB0
    ADC1_EX_9,     // ADC1_IN9 PB1
    ADC1_EX_10,    // ADC1_IN10 PC0
    ADC1_EX_11,    // ADC1_IN11 PC1
    ADC1_EX_12,    // ADC1_IN12 PC2
    ADC1_EX_13,    // ADC1_IN13 PC3
    ADC1_EX_14,    // ADC1_IN14 PC4
    ADC1_EX_15,    // ADC1_IN15 PC5

    // ADC3 通道
    ADC3_4,    // ADC3_IN4 PF4
    ADC3_5,    // ADC3_IN5 PF5
    ADC3_EX_8, // ADC3_IN8 PF10

    ADC_NUM_MAX // ADC数量上限
} BoardADC_e;

/**
 * @brief 板载DAC枚举
 */
typedef enum
{
    DAC_1 = 0, // DAC通道1 PA4
    DAC_2,     // DAC通道2 PA5

    DAC_NUM_MAX // DAC数量上限
} BoardDAC_e;

#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C

/**
 * @brief 板载GPIO枚举
 */
typedef enum
{
    // 用户按键
    GPIO_USER_KEY = 0, // 用户按键 PA0

    // BMI088 控制
    GPIO_BMI088_CS_ACCEL,  // BMI088 CS_Accel PA4
    GPIO_BMI088_CS_GYRO,   // BMI088 CS_Gyro PB0
    GPIO_BMI088_INT_ACCEL, // BMI088 INT_Accel PC4
    GPIO_BMI088_INT_GYRO,  // BMI088 INT_Gyro PC5

    // IST8310 控制
    GPIO_IST8310_DRDY, // IST8310 DRDY PG3
    GPIO_IST8310_RSTN, // IST8310 RSTN PG6

    GPIO_NUM_MAX // GPIO数量上限
} BoardGPIO_e;

/**
 * @brief 板载TIM枚举
 */
typedef enum
{
    // PWM定时器（TIM1 4通道）
    TIM_PWM_1 = 0, // 电机PWM CH1 PE9
    TIM_PWM_2,     // 电机PWM CH2 PE11
    TIM_PWM_3,     // 电机PWM CH3 PE13
    TIM_PWM_4,     // 电机PWM CH4 PE14

    // PWM定时器（TIM5 3通道 LED）
    TIM_LED_B, // LED蓝 PH10
    TIM_LED_G, // LED绿 PH11
    TIM_LED_R, // LED红 PH12

    // PWM定时器（TIM8 3通道）
    TIM_PWM_5, // PWM CH1 PC6
    TIM_PWM_6, // PWM CH2 PI6
    TIM_PWM_7, // PWM CH3 PI7

    // 特殊功能定时器
    TIM_LASER,  // 激光 TIM3_CH3 PC8
    TIM_BUZZER, // 蜂鸣器 TIM4_CH3 PD14
    TIM_HEATER, // BMI088加热 TIM10_CH1 PF6

    TIM_NUM_MAX // TIM数量上限
} BoardTIM_e;

/**
 * @brief 板载UART枚举
 */
typedef enum
{
    UART_SBUS = 0, // SBUS/DBUS遥控接收 USART3 PC11
    UART_1,        // USART1 PA9/PB7
    UART_6,        // USART6 PG9/PG14

    UART_NUM_MAX // UART数量上限
} BoardUART_e;

/**
 * @brief 板载CAN枚举
 */
typedef enum
{
    CAN_1 = 0, // CAN1 PD0/PD1
    CAN_2,     // CAN2 PB5/PB6

    CAN_NUM_MAX // CAN数量上限
} BoardCAN_e;

/**
 * @brief 板载SPI枚举
 */
typedef enum
{
    SPI_BMI088 = 0, // SPI1 BMI088 PA7/PB3/PB4
    SPI_EX_2,       // SPI2 PB12/PB13/PB14/PB15

    SPI_NUM_MAX // SPI数量上限
} BoardSPI_e;

/**
 * @brief 板载I2C枚举
 */
typedef enum
{
    I2C_EX_2 = 0, // I2C2 PF0/PF1
    I2C_IST8310,  // I2C3 IST8310磁力计 PA8/PC9

    I2C_NUM_MAX // I2C数量上限
} BoardI2C_e;

/**
 * @brief 板载ADC枚举
 */
typedef enum
{
    ADC_BAT = 0, // 电池电压 ADC3_IN8 PF10

    ADC_NUM_MAX // ADC数量上限
} BoardADC_e;

#endif // DJI_C

#ifdef BSP_GPIO_MODULE_ENABLED
/**
 * @brief GPIO映射结构体
 */
typedef struct
{
    GPIO_TypeDef *port; // GPIO端口
    uint16_t pin;       // 引脚号
} GPIO_Map_t;

extern const GPIO_Map_t gpio_map[];
#endif

#ifdef BSP_TIM_MODULE_ENABLED
/**
 * @brief TIM映射结构体
 */
typedef struct
{
    TIM_HandleTypeDef *htim; // TIM句柄
    uint32_t channel;        // 通道（仅PWM使用，编码器填0）
} TIM_Map_t;

extern const TIM_Map_t tim_map[];
#endif

#ifdef BSP_UART_MODULE_ENABLED
/**
 * @brief UART映射结构体
 */
typedef struct
{
    UART_HandleTypeDef *handle; // UART句柄
} UART_Map_t;

extern const UART_Map_t uart_map[];
#endif

#ifdef BSP_CAN_MODULE_ENABLED
/**
 * @brief CAN映射结构体
 */
typedef struct
{
#if DEVELOPMENT_BOARD == DM_MC02
    FDCAN_HandleTypeDef *handle; // CAN句柄
#else
    CAN_HandleTypeDef *handle; // CAN句柄
#endif
} CAN_Map_t;

extern const CAN_Map_t can_map[];
#endif

#ifdef BSP_SPI_MODULE_ENABLED
/**
 * @brief SPI映射结构体
 */
typedef struct
{
    SPI_HandleTypeDef *handle; // SPI句柄
} SPI_Map_t;

extern const SPI_Map_t spi_map[];
#endif

#ifdef BSP_I2C_MODULE_ENABLED
/**
 * @brief I2C映射结构体
 */
typedef struct
{
    I2C_HandleTypeDef *handle; // I2C句柄
} I2C_Map_t;

extern const I2C_Map_t i2c_map[];
#endif

#ifdef BSP_ADC_MODULE_ENABLED
/**
 * @brief ADC映射结构体
 */
typedef struct
{
    ADC_HandleTypeDef *handle; // ADC句柄
    uint32_t channel;          // ADC通道
} ADC_Map_t;

extern const ADC_Map_t adc_map[];
#endif

#ifdef BSP_DAC_MODULE_ENABLED
/**
 * @brief DAC映射结构体
 */
typedef struct
{
    DAC_HandleTypeDef *handle; // DAC句柄
    uint32_t channel;          // DAC通道
} DAC_Map_t;

extern const DAC_Map_t dac_map[];
#endif

#endif // __BSP_CFG_H
