/**
 * @file bsp_cfg.c
 * @brief 板级硬件映射实现
 *
 * @note 不同开发板需要修改本文件中的映射数组
 *       CubeMX重新生成后需确认映射是否正确
 */

#include "bsp_cfg.h"

#if DEVELOPMENT_BOARD == STM32F407VET6

#include "tim.h"
#include "usart.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // LED
    [GPIO_LED_G] = {GPIOE, GPIO_PIN_14}, // LED绿 PE14
    [GPIO_LED_R] = {GPIOA, GPIO_PIN_1},  // LED红 PA1

    // 电机方向控制
    [GPIO_MOTOR1_IN1] = {GPIOB, GPIO_PIN_12}, // 电机1 IN1 PB12
    [GPIO_MOTOR1_IN2] = {GPIOB, GPIO_PIN_13}, // 电机1 IN2 PB13
    [GPIO_MOTOR2_IN1] = {GPIOB, GPIO_PIN_14}, // 电机2 IN1 PB14
    [GPIO_MOTOR2_IN2] = {GPIOB, GPIO_PIN_15}, // 电机2 IN2 PB15
    [GPIO_MOTOR3_IN1] = {GPIOC, GPIO_PIN_9},  // 电机3 IN1 PC9
    [GPIO_MOTOR3_IN2] = {GPIOC, GPIO_PIN_10}, // 电机3 IN2 PC10
    [GPIO_MOTOR4_IN1] = {GPIOC, GPIO_PIN_11}, // 电机4 IN1 PC11
    [GPIO_MOTOR4_IN2] = {GPIOC, GPIO_PIN_12}, // 电机4 IN2 PC12
};

/*============================================
 *              TIM 映射数组
 *============================================*/
const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    // PWM定时器（TIM1 4通道）
    [TIM_PWM_1] = {&htim1, TIM_CHANNEL_1}, // 电机PWM CH1
    [TIM_PWM_2] = {&htim1, TIM_CHANNEL_2}, // 电机PWM CH2
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_3}, // 电机PWM CH3
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_4}, // 电机PWM CH4

    // 编码器定时器
    [TIM_ENCODER_1] = {&htim2, 0}, // 编码器1 TIM2
    [TIM_ENCODER_2] = {&htim3, 0}, // 编码器2 TIM3
    [TIM_ENCODER_3] = {&htim4, 0}, // 编码器3 TIM4
    [TIM_ENCODER_4] = {&htim8, 0}, // 编码器4 TIM8
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart2}, // SBUS遥控接收 UART2
};

#endif // STM32F407VET6

#if DEVELOPMENT_BOARD == DM_MC02

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

#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_A

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

#endif // DJI_A

#if DEVELOPMENT_BOARD == DJI_C

#include "tim.h"
#include "usart.h"
#include "can.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // 用户按键
    [GPIO_USER_KEY] = {GPIOA, GPIO_PIN_0}, // 用户按键 PA0

    // BMI088 控制
    [GPIO_BMI088_CS_ACCEL] = {GPIOA, GPIO_PIN_4},  // BMI088 CS_Accel PA4
    [GPIO_BMI088_CS_GYRO] = {GPIOB, GPIO_PIN_0},   // BMI088 CS_Gyro PB0
    [GPIO_BMI088_INT_ACCEL] = {GPIOC, GPIO_PIN_4}, // BMI088 INT1_Accel PC4
    [GPIO_BMI088_INT_GYRO] = {GPIOC, GPIO_PIN_5},  // BMI088 INT1_Gyro PC5

    // IST8310 控制
    [GPIO_IST8310_DRDY] = {GPIOG, GPIO_PIN_3}, // IST8310 DRDY PG3
    [GPIO_IST8310_RSTN] = {GPIOG, GPIO_PIN_6}, // IST8310 RSTN PG6
};

/*============================================
 *              TIM 映射数组
 *============================================*/
const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    // PWM定时器（TIM1 4通道）
    [TIM_PWM_1] = {&htim1, TIM_CHANNEL_1}, // 电机PWM CH1 PE9
    [TIM_PWM_2] = {&htim1, TIM_CHANNEL_2}, // 电机PWM CH2 PE11
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_3}, // 电机PWM CH3 PE13
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_4}, // 电机PWM CH4 PE14

    // PWM定时器（TIM8 2通道）
    [TIM_PWM_5] = {&htim8, TIM_CHANNEL_1}, // PWM CH1 PC6
    [TIM_PWM_6] = {&htim8, TIM_CHANNEL_2}, // PWM CH2 PI6
    [TIM_PWM_7] = {&htim8, TIM_CHANNEL_3}, // PWM CH3 PI7

    // PWM定时器（TIM5 3通道 LED）
    [TIM_LED_B] = {&htim5, TIM_CHANNEL_1}, // LED蓝 PH10
    [TIM_LED_G] = {&htim5, TIM_CHANNEL_2}, // LED绿 PH11
    [TIM_LED_R] = {&htim5, TIM_CHANNEL_3}, // LED红 PH12

    // 特殊功能定时器
    [TIM_LASER] = {&htim3, TIM_CHANNEL_3},   // 激光 TIM3_CH3 PC8
    [TIM_BUZZER] = {&htim4, TIM_CHANNEL_3},  // 蜂鸣器 TIM4_CH3 PD14
    [TIM_HEATER] = {&htim10, TIM_CHANNEL_1}, // BMI088加热 TIM10_CH1 PF6
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart3}, // SBUS/DBUS遥控接收 USART3 PC11
    [UART_1] = {&huart1},    // USART1 PA9/PB7
    [UART_6] = {&huart6},    // USART6 PG9/PG14
};

/*============================================
 *              CAN 映射数组
 *============================================*/
const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hcan1}, // CAN1 PD0/PD1
    [CAN_2] = {&hcan2}, // CAN2 PB5/PB6
};

/*============================================
 *              SPI 映射数组
 *============================================*/
const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_BMI088] = {&hspi1}, // SPI1 BMI088 PA7/PB3/PB4
    [SPI_EX_2] = {&hspi2},   // SPI2 PB12/PB13/PB14/PB15
};

/*============================================
 *              I2C 映射数组
 *============================================*/
const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_EX_2] = {&hi2c2},    // I2C2 PF0/PF1
    [I2C_IST8310] = {&hi2c3}, // I2C3 IST8310磁力计 PA8/PC9
};

/*============================================
 *              ADC 映射数组
 *============================================*/
const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT] = {&hadc3, ADC_CHANNEL_8}, // 电池电压 ADC3_IN8 PF10
};

#endif // DJI_C
