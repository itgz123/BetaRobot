#ifndef __DM_MC02_BSP_MAP_H
#define __DM_MC02_BSP_MAP_H

#include "app_cfg.h"
#include "tim.h"
#include "usart.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"
#include "fdcan.h"

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
#define CPU_CORE CORTEX_M7
#define BSP_CAN_IP BSP_CAN_IP_FDCAN
#define HAS_FPU 1
#define HAS_DSP 1
#define HAS_CORDIC 1
#define HAS_CRC 1
#define HAS_FMAC 1
#define HAS_MPU 1
#define HAS_RAMECC 1

#if CPU_CORE == CORTEX_M7
#define DMA_RAM __attribute__((section(".ram_d1")))
#define ITCM_RAM __attribute__((section(".itcmram")))
#else
#define DMA_RAM
#define ITCM_RAM
#endif

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
typedef struct
{
    FDCAN_HandleTypeDef *handle;
} CAN_Map_t;
typedef struct
{
    SPI_HandleTypeDef *handle;
} SPI_Map_t;
typedef struct
{
    I2C_HandleTypeDef *handle;
} I2C_Map_t;
typedef struct
{
    ADC_HandleTypeDef *handle;
    uint32_t channel;
} ADC_Map_t;

/*============================================
 *              板载资源枚举
 *============================================*/
typedef enum
{
    GPIO_BMI088_CS_ACCEL = 0,
    GPIO_BMI088_CS_GYRO,
    GPIO_BMI088_INT_ACCEL,
    GPIO_BMI088_INT_GYRO,
    GPIO_POWER_24V_EN1,
    GPIO_POWER_24V_EN2,
    GPIO_POWER_5V_EN,
    GPIO_LCD_KEY1,
    GPIO_LCD_KEY2,
    GPIO_EX_KEY,
    GPIO_USER_KEY,
    GPIO_NUM_MAX
} BoardGPIO_e;

typedef enum
{
    TIM_PWM_1 = 0,
    TIM_PWM_2,
    TIM_PWM_3,
    TIM_PWM_4,
    TIM_HEATER,
    TIM_BUZZER,
    TIM_NUM_MAX
} BoardTIM_e;

typedef enum
{
    UART_1 = 0,
    UART_SBUS,
    UART_RS485_2,
    UART_RS485_3,
    UART_7,
    UART_EX_8,
    UART_EX_9,
    UART_10,
    UART_NUM_MAX
} BoardUART_e;

typedef enum
{
    CAN_1 = 0,
    CAN_2,
    CAN_3,
    CAN_NUM_MAX
} BoardCAN_e;
typedef enum
{
    SPI_LCD_1 = 0,
    SPI_BMI088,
    SPI_NUM_MAX
} BoardSPI_e;
typedef enum
{
    I2C_LCD_2 = 0,
    I2C_NUM_MAX
} BoardI2C_e;
typedef enum
{
    ADC_BAT = 0,
    ADC_NUM_MAX
} BoardADC_e;

#define VOLTAGE_TRANFER 0.00055389404296875f

/*============================================
 *              逻辑实例数量配置
 *============================================*/
#define CAN_INSTANCE_NUM 3
#define I2C_INSTANCE_NUM 1
#define SPI_INSTANCE_NUM 2
#define GPIO_INSTANCE_NUM 11
#define UART_INSTANCE_NUM 8
#define PWM_INSTANCE_NUM 6
#define ENCODER_INSTANCE_NUM 0
#define ADC_INSTANCE_NUM 1

/*============================================
 *              extern 声明
 *============================================*/
extern const GPIO_Map_t gpio_map[];
extern const TIM_Map_t tim_map[];
extern const UART_Map_t uart_map[];
extern const CAN_Map_t can_map[];
extern const SPI_Map_t spi_map[];
extern const I2C_Map_t i2c_map[];
extern const ADC_Map_t adc_map[];

#endif /* __DM_MC02_BSP_MAP_H */
