#ifndef __DJI_A_BSP_MAP_H
#define __DJI_A_BSP_MAP_H

#include "tim.h"
#include "usart.h"
#include "can.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"
#include "dac.h"
#include "bsp.h"

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
typedef struct
{
    CAN_HandleTypeDef *handle;
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
typedef struct
{
    DAC_HandleTypeDef *handle;
    uint32_t channel;
} DAC_Map_t;

/*============================================
 *              板载资源枚举
 *============================================*/
typedef enum
{
    GPIO_OLED_KEY = 0,
    GPIO_OLED_DC,
    GPIO_OLED_RST,
    GPIO_USER_KEY,
    GPIO_MPU6500_INT,
    GPIO_IST8310_RSTN,
    GPIO_IST8310_DRDY,
    GPIO_LED_R,
    GPIO_LED_G,
    GPIO_SD_EXTI,
    GPIO_LASER,
    GPIO_POWER_EN1,
    GPIO_POWER_EN2,
    GPIO_POWER_EN3,
    GPIO_POWER_EN4,
    GPIO_DAC_EXTI,
    GPIO_NUM_MAX
} BoardGPIO_e;

typedef enum
{
    TIM_HEATER = 0,
    TIM_BUZZER,
    TIM_PWM_3Pin1,
    TIM_PWM_3Pin2,
    TIM_PWM_3Pin3,
    TIM_PWM_3Pin4,
    TIM_PWM_1,
    TIM_PWM_2,
    TIM_PWM_3,
    TIM_PWM_4,
    TIM_PWM_5,
    TIM_PWM_6,
    TIM_PWM_7,
    TIM_PWM_8,
    TIM_PWM_9,
    TIM_PWM_10,
    TIM_PWM_11,
    TIM_PWM_12,
    TIM_PWM_13,
    TIM_PWM_14,
    TIM_PWM_15,
    TIM_PWM_16,
    TIM_NUM_MAX
} BoardTIM_e;

typedef enum
{
    UART_SBUS = 0,
    UART_2,
    UART_3,
    UART_7,
    UART_8,
    UART_4Pin_6,
    UART_NUM_MAX
} BoardUART_e;

typedef enum
{
    CAN_1 = 0,
    CAN_2,
    CAN_NUM_MAX
} BoardCAN_e;
typedef enum
{
    SPI_OLED = 0,
    SPI_EX_4,
    SPI_MPU6500,
    SPI_NUM_MAX
} BoardSPI_e;
typedef enum
{
    I2C_EX_2 = 0,
    I2C_NUM_MAX
} BoardI2C_e;

typedef enum
{
    ADC1_EX_8 = 0,
    ADC1_EX_9,
    ADC1_EX_10,
    ADC1_EX_11,
    ADC1_EX_12,
    ADC1_EX_13,
    ADC1_EX_14,
    ADC1_EX_15,
    ADC3_4,
    ADC3_5,
    ADC3_EX_8,
    ADC_NUM_MAX
} BoardADC_e;

typedef enum
{
    DAC_1 = 0,
    DAC_2,
    DAC_NUM_MAX
} BoardDAC_e;

/*============================================
 *              逻辑实例数量配置
 *============================================*/
#define CAN_INSTANCE_NUM 16
#define I2C_INSTANCE_NUM 1
#define SPI_INSTANCE_NUM 3
#define GPIO_INSTANCE_NUM 16
#define UART_INSTANCE_NUM 6
#define PWM_INSTANCE_NUM 22
#define ENCODER_INSTANCE_NUM 0
#define ADC_INSTANCE_NUM 11
#define DAC_INSTANCE_NUM 2

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
extern const DAC_Map_t dac_map[];

#endif /* __DJI_A_BSP_MAP_H */
