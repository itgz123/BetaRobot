#include "tim.h"
#include "usart.h"
#include "can.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"
#include "dac.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // OLED
    [GPIO_OLED_KEY] = {GPIOA, GPIO_PIN_6},  // OLED按键 PA6
    [GPIO_OLED_DC] = {GPIOB, GPIO_PIN_9},   // OLED DC PB9
    [GPIO_OLED_RST] = {GPIOB, GPIO_PIN_10}, // OLED复位 PB10

    // 按键
    [GPIO_USER_KEY] = {GPIOB, GPIO_PIN_2}, // 按键 PB2

    // MPU6500
    [GPIO_MPU6500_INT] = {GPIOB, GPIO_PIN_8}, // MPU6500中断 PB8

    // IST8310
    [GPIO_IST8310_RSTN] = {GPIOE, GPIO_PIN_2}, // IST8310复位 PE2
    [GPIO_IST8310_DRDY] = {GPIOE, GPIO_PIN_3}, // IST8310数据就绪 PE3

    // LED
    [GPIO_LED_R] = {GPIOE, GPIO_PIN_11}, // LED红 PE11
    [GPIO_LED_G] = {GPIOF, GPIO_PIN_14}, // LED绿 PF14

    // SD卡
    [GPIO_SD_EXTI] = {GPIOE, GPIO_PIN_15}, // SD卡检测 PE15

    // 激光
    [GPIO_LASER] = {GPIOG, GPIO_PIN_13}, // 激光 PG13

    // 电源控制
    [GPIO_POWER_EN1] = {GPIOH, GPIO_PIN_2}, // 电源控制1 PH2
    [GPIO_POWER_EN2] = {GPIOH, GPIO_PIN_3}, // 电源控制2 PH3
    [GPIO_POWER_EN3] = {GPIOH, GPIO_PIN_4}, // 电源控制3 PH4
    [GPIO_POWER_EN4] = {GPIOH, GPIO_PIN_5}, // 电源控制4 PH5

    // DAC外部中断
    [GPIO_DAC_EXTI] = {GPIOI, GPIO_PIN_9}, // DAC外部中断 PI9
};

/*============================================
 *              TIM 映射数组
 *============================================*/
const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    // 特殊功能定时器
    [TIM_HEATER] = {&htim3, TIM_CHANNEL_2},  // MPU6500加热 TIM3_CH4 PB1
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_1}, // 蜂鸣器 TIM12_CH1 PH6

    [TIM_PWM_3Pin1] = {&htim1, TIM_CHANNEL_1}, // PWM CH1 PA8
    [TIM_PWM_3Pin2] = {&htim1, TIM_CHANNEL_2}, // PWM CH2 PA9
    [TIM_PWM_3Pin3] = {&htim1, TIM_CHANNEL_3}, // PWM CH3 PE13
    [TIM_PWM_3Pin4] = {&htim1, TIM_CHANNEL_4}, // PWM CH4 PE14

    [TIM_PWM_1] = {&htim2, TIM_CHANNEL_1}, // PWM CH5 PA0
    [TIM_PWM_2] = {&htim2, TIM_CHANNEL_2}, // PWM CH6 PA1
    [TIM_PWM_3] = {&htim2, TIM_CHANNEL_3}, // PWM CH7 PA2
    [TIM_PWM_4] = {&htim2, TIM_CHANNEL_4}, // PWM CH8 PA3

    [TIM_PWM_5] = {&htim4, TIM_CHANNEL_1}, // PWM CH10 PD12
    [TIM_PWM_6] = {&htim4, TIM_CHANNEL_2}, // PWM CH11 PD13
    [TIM_PWM_7] = {&htim4, TIM_CHANNEL_3}, // PWM CH12 PD14
    [TIM_PWM_8] = {&htim4, TIM_CHANNEL_4}, // PWM CH13 PD15

    [TIM_PWM_9] = {&htim5, TIM_CHANNEL_1},  // PWM CH14 PH10
    [TIM_PWM_10] = {&htim5, TIM_CHANNEL_2}, // PWM CH15 PH11
    [TIM_PWM_11] = {&htim5, TIM_CHANNEL_3}, // PWM CH16 PH12
    [TIM_PWM_12] = {&htim5, TIM_CHANNEL_4}, // PWM CH17 PI0

    [TIM_PWM_13] = {&htim8, TIM_CHANNEL_1}, // PWM CH18 PI5
    [TIM_PWM_14] = {&htim8, TIM_CHANNEL_2}, // PWM CH19 PI6
    [TIM_PWM_15] = {&htim8, TIM_CHANNEL_3}, // PWM CH20 PI7
    [TIM_PWM_16] = {&htim8, TIM_CHANNEL_4}, // PWM CH21 PI2
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart1},   // SBUS遥控接收 USART1 PB6/PB7
    [UART_2] = {&huart2},      // 扩展串口 USART2 PD5/PD6
    [UART_3] = {&huart3},      // 扩展串口 USART3 PD8/PD9
    [UART_7] = {&huart7},      // 扩展串口 UART7 PE7/PE8
    [UART_8] = {&huart8},      // 扩展串口 UART8 PE0/PE1
    [UART_4Pin_6] = {&huart6}, // 扩展串口 USART6 PG9/PG14
};

/*============================================
 *              CAN 映射数组
 *============================================*/
const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hcan1}, // CAN1 PD0/PD1
    [CAN_2] = {&hcan2}, // CAN2 PB12/PB13
};

/*============================================
 *              SPI 映射数组
 *============================================*/
const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_OLED] = {&hspi1},    // SPI1 TX Only PB3/PA7
    [SPI_EX_4] = {&hspi4},    // SPI4 Full Duplex PE4/PE5/PE6/PE12
    [SPI_MPU6500] = {&hspi5}, // SPI5 Full Duplex PF6/PF7/PF8/PF9
};

/*============================================
 *              I2C 映射数组
 *============================================*/
const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_EX_2] = {&hi2c2}, // I2C2 PF0/PF1
};

/*============================================
 *              ADC 映射数组
 *============================================*/
const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    // ADC1 通道
    [ADC1_EX_8] = {&hadc1, ADC_CHANNEL_8},   // ADC1_IN8 PB0
    [ADC1_EX_9] = {&hadc1, ADC_CHANNEL_9},   // ADC1_IN9 PB1
    [ADC1_EX_10] = {&hadc1, ADC_CHANNEL_10}, // ADC1_IN10 PC0
    [ADC1_EX_11] = {&hadc1, ADC_CHANNEL_11}, // ADC1_IN11 PC1
    [ADC1_EX_12] = {&hadc1, ADC_CHANNEL_12}, // ADC1_IN12 PC2
    [ADC1_EX_13] = {&hadc1, ADC_CHANNEL_13}, // ADC1_IN13 PC3
    [ADC1_EX_14] = {&hadc1, ADC_CHANNEL_14}, // ADC1_IN14 PC4
    [ADC1_EX_15] = {&hadc1, ADC_CHANNEL_15}, // ADC1_IN15 PC5

    // ADC3 通道
    [ADC3_4] = {&hadc3, ADC_CHANNEL_4},    // ADC3_IN4 PF4
    [ADC3_5] = {&hadc3, ADC_CHANNEL_5},    // ADC3_IN5 PF5
    [ADC3_EX_8] = {&hadc3, ADC_CHANNEL_8}, // ADC3_IN8 PF10
};

/*============================================
 *              DAC 映射数组
 *============================================*/
const DAC_Map_t dac_map[DAC_NUM_MAX] = {
    [DAC_1] = {&hdac, DAC_CHANNEL_1}, // DAC通道1 PA4
    [DAC_2] = {&hdac, DAC_CHANNEL_2}, // DAC通道2 PA5
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(dac_map, DAC_NUM_MAX);
