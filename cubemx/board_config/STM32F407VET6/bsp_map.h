#ifndef __STM32F407VET6_BSP_MAP_H
#define __STM32F407VET6_BSP_MAP_H

#include "app_cfg.h"
#include "tim.h"
#include "usart.h"

/*============================================
 *              内核与 CAN 类型常量
 *============================================*/
#define CORTEX_M4 0
#define CORTEX_M7 1
#define BSP_CAN_IP_BXCAN 0
#define BSP_CAN_IP_FDCAN 1

/*============================================
 *              硬件特性配置
 *============================================*/
#define CPU_CORE CORTEX_M4
#define BSP_CAN_IP BSP_CAN_IP_BXCAN
#define HAS_FPU 1
#define HAS_DSP 1
#define HAS_CORDIC 0
#define HAS_CRC 1
#define HAS_FMAC 0
#define HAS_MPU 1
#define HAS_RAMECC 0

#define DMA_RAM
#define ITCM_RAM

/*============================================
 *              映射结构体类型
 *============================================*/
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} GPIO_Map_t;
typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} TIM_Map_t;
typedef struct
{
    UART_HandleTypeDef *handle;
} UART_Map_t;

/*============================================
 *              板载资源枚举
 *============================================*/
typedef enum
{
    GPIO_LED_G = 0,
    GPIO_LED_R,
    GPIO_MOTOR1_IN1,
    GPIO_MOTOR1_IN2,
    GPIO_MOTOR2_IN1,
    GPIO_MOTOR2_IN2,
    GPIO_MOTOR3_IN1,
    GPIO_MOTOR3_IN2,
    GPIO_MOTOR4_IN1,
    GPIO_MOTOR4_IN2,
    GPIO_NUM_MAX
} BoardGPIO_e;

typedef enum
{
    TIM_PWM_1 = 0,
    TIM_PWM_2,
    TIM_PWM_3,
    TIM_PWM_4,
    TIM_ENCODER_1,
    TIM_ENCODER_2,
    TIM_ENCODER_3,
    TIM_ENCODER_4,
    TIM_NUM_MAX
} BoardTIM_e;

typedef enum
{
    UART_SBUS = 0,
    UART_NUM_MAX
} BoardUART_e;

/*============================================
 *              逻辑实例数量配置
 *============================================*/
#define CAN_INSTANCE_NUM 0
#define I2C_INSTANCE_NUM 0
#define SPI_INSTANCE_NUM 0
#define GPIO_INSTANCE_NUM 10
#define UART_INSTANCE_NUM 1
#define PWM_INSTANCE_NUM 4
#define ENCODER_INSTANCE_NUM 4
#define ADC_INSTANCE_NUM 0

/*============================================
 *              extern 声明
 *============================================*/
extern const GPIO_Map_t gpio_map[];
extern const TIM_Map_t tim_map[];
extern const UART_Map_t uart_map[];

#endif /* __STM32F407VET6_BSP_MAP_H */
