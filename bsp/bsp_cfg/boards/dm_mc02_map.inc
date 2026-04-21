#include "tim.h"
#include "usart.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"
#include "fdcan.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // BMI088 控制
    [GPIO_BMI088_CS_ACCEL] = {GPIOC, GPIO_PIN_0},   // BMI088 CS_Accel PC0
    [GPIO_BMI088_CS_GYRO] = {GPIOC, GPIO_PIN_3},    // BMI088 CS_Gyro PC3
    [GPIO_BMI088_INT_ACCEL] = {GPIOE, GPIO_PIN_10}, // BMI088 INT1_Accel PE10
    [GPIO_BMI088_INT_GYRO] = {GPIOE, GPIO_PIN_12},  // BMI088 INT3_Gyro PE12

    // 电源控制
    [GPIO_POWER_24V_EN1] = {GPIOC, GPIO_PIN_14}, // 24V输出1 PC14
    [GPIO_POWER_24V_EN2] = {GPIOC, GPIO_PIN_13}, // 24V输出2 PC13
    [GPIO_POWER_5V_EN] = {GPIOC, GPIO_PIN_15},   // 5V使能 PC15

    // 按键输入
    [GPIO_LCD_KEY1] = {GPIOA, GPIO_PIN_5},  // LCD按键1 PA5
    [GPIO_LCD_KEY2] = {GPIOD, GPIO_PIN_10}, // LCD按键2 PD10
    [GPIO_EX_KEY] = {GPIOA, GPIO_PIN_15},   // 扩展按键 PA15
    [GPIO_USER_KEY] = {GPIOE, GPIO_PIN_14}, // 用户按键 PE14
};

/*============================================
 *              TIM 映射数组
 *============================================*/
const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    // PWM定时器
    [TIM_PWM_1] = {&htim2, TIM_CHANNEL_1}, // PWM输出1 TIM2_CH1 PA0
    [TIM_PWM_2] = {&htim2, TIM_CHANNEL_3}, // PWM输出2 TIM2_CH3 PA2
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_1}, // PWM输出3 TIM1_CH1 PE9
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_3}, // PWM输出4 TIM1_CH3 PE13

    // 特殊功能定时器
    [TIM_HEATER] = {&htim3, TIM_CHANNEL_4},  // BMI088加热 TIM3_CH4 PB1
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_2}, // 蜂鸣器 TIM12_CH2 PB15
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_1] = {&huart1},       // 调试串口 USART1 PA9/PA10
    [UART_SBUS] = {&huart5},    // 遥控接收 UART5 PD2
    [UART_RS485_2] = {&huart2}, // RS485-1 USART2 PD4/PD5/PD6
    [UART_RS485_3] = {&huart3}, // RS485-2 USART3 PB14/PD8/PD9
    [UART_7] = {&huart7},       // 扩展串口1 UART7 PE7/PE8
    [UART_EX_8] = {&huart8},    // 扩展串口2 UART8 PE0/PE1
    [UART_EX_9] = {&huart9},    // 扩展串口3 UART9 PD14/PD15
    [UART_10] = {&huart10},     // 扩展串口4 USART10 PE2/PE3
};

/*============================================
 *              CAN 映射数组
 *============================================*/
const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hfdcan1}, // CAN1 PD0/PD1
    [CAN_2] = {&hfdcan2}, // CAN2 PB5/PB6
    [CAN_3] = {&hfdcan3}, // CAN3 PD12/PD13
};

/*============================================
 *              SPI 映射数组
 *============================================*/
const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_LCD_1] = {&hspi1},  // SPI1 LCD PB3/PB4/PD7/PE15
    [SPI_BMI088] = {&hspi2}, // SPI2 BMI088 PB13/PC1/PC2
};

/*============================================
 *              I2C 映射数组
 *============================================*/
const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_LCD_2] = {&hi2c2}, // I2C2 LCD PB10/PB11
};

/*============================================
 *              ADC 映射数组
 *============================================*/
const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT] = {&hadc1, ADC_CHANNEL_4}, // 电池电压 PC04 -> ADC1_INP4
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
