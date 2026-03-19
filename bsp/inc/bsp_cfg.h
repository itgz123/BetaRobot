/**
 * @file bsp_cfg.h
 * @brief BSP层配置文件：板载资源定义、实例数量配置、硬件映射
 *
 * @note 本文件职责：
 *       1. 定义板载资源枚举（GPIO/TIM）
 *       2. 声明硬件映射数组和获取接口
 *       3. 根据开发板自动配置实例数量
 *
 * @note 切换开发板：只需修改 app_cfg.h 中的 DEVELOPMENT_BOARD 宏
 *       本文件会自动适配，无需修改
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
 * @note PWM和编码器分开枚举
 */
typedef enum
{
    // PWM定时器
    TIM_MOTOR_PWM = 0, // 电机PWM TIM1 (4通道)

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

#if DEVELOPMENT_BOARD == TELESKY_VET6
#define CPU_FREQ_MHZ 168
#define ADC_NUM 0
#define CAN_NUM 0
#define GPIO_NUM 10 // 2 LED + 8 电机方向
#define I2C_NUM 0
#define PWM_NUM 4     // TIM1的4个通道
#define ENCODER_NUM 4 // TIM2/3/4/8
#define SPI_NUM 0
#define UART_NUM 1 // UART2
#define USART_NUM 0
#define USB_NUM 0
#endif // TELESKY_VET6

#if DEVELOPMENT_BOARD == DM_MC02
#define CPU_FREQ_MHZ 550
#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_A
#define CPU_FREQ_MHZ 180
#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C
#define CPU_FREQ_MHZ 168
#endif // DJI_C

#endif // __BSP_CFG_H