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
    [GPIO_LED_GREEN] = {GPIOE, GPIO_PIN_14}, // LED绿 PE14
    [GPIO_LED_RED] = {GPIOA, GPIO_PIN_1},    // LED红 PA1

    // 电机方向控制
    [GPIO_MOTOR_1_IN1] = {GPIOB, GPIO_PIN_12}, // 电机1 IN1 PB12
    [GPIO_MOTOR_1_IN2] = {GPIOB, GPIO_PIN_13}, // 电机1 IN2 PB13
    [GPIO_MOTOR_2_IN1] = {GPIOB, GPIO_PIN_14}, // 电机2 IN1 PB14
    [GPIO_MOTOR_2_IN2] = {GPIOB, GPIO_PIN_15}, // 电机2 IN2 PB15
    [GPIO_MOTOR_3_IN1] = {GPIOC, GPIO_PIN_9},  // 电机3 IN1 PC9
    [GPIO_MOTOR_3_IN2] = {GPIOC, GPIO_PIN_10}, // 电机3 IN2 PC10
    [GPIO_MOTOR_4_IN1] = {GPIOC, GPIO_PIN_11}, // 电机4 IN1 PC11
    [GPIO_MOTOR_4_IN2] = {GPIOC, GPIO_PIN_12}, // 电机4 IN2 PC12
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
    // PE9     ------> TIM1_CH1
    // PE11     ------> TIM1_CH2
    // PE13     ------> TIM1_CH3
    // PA11     ------> TIM1_CH4

    // PA0-WKUP     ------> TIM2_CH1
    // PB3     ------> TIM2_CH2

    // PB4     ------> TIM3_CH1
    // PB5     ------> TIM3_CH2

    // PD12     ------> TIM4_CH1
    // PD13     ------> TIM4_CH2

    // PC6     ------> TIM8_CH1
    // PC7     ------> TIM8_CH2
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
#include "adc.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // BMI088 控制
    [GPIO_BMI088_CS1] = {GPIOC, GPIO_PIN_0},   // BMI088 CS1 PC0
    [GPIO_BMI088_CS2] = {GPIOC, GPIO_PIN_3},   // BMI088 CS2 PC3
    [GPIO_BMI088_INT1] = {GPIOE, GPIO_PIN_10}, // BMI088 INT1 PE10
    [GPIO_BMI088_INT3] = {GPIOE, GPIO_PIN_12}, // BMI088 INT3 PE12

    // 电源控制
    [GPIO_POWER_24V_OUT1] = {GPIOC, GPIO_PIN_14}, // 24V输出1 PC14
    [GPIO_POWER_24V_OUT2] = {GPIOC, GPIO_PIN_13}, // 24V输出2 PC13
    [GPIO_POWER_5V_EN] = {GPIOC, GPIO_PIN_15},    // 5V使能 PC15

    // 按键输入
    [GPIO_KEY_LCD_1] = {GPIOA, GPIO_PIN_5},  // LCD按键1 PA5
    [GPIO_KEY_LCD_2] = {GPIOD, GPIO_PIN_10}, // LCD按键2 PD10
    [GPIO_KEY_EX] = {GPIOA, GPIO_PIN_15},    // 扩展按键 PA15
    [GPIO_KEY_USER] = {GPIOE, GPIO_PIN_14},  // 用户按键 PE14
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
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_2}, // 蜂鸣器 TIM12_CH2 PB15
    [TIM_HEATER] = {&htim3, TIM_CHANNEL_4},  // BMI088加热 TIM3_CH4 PB1
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    // 注意：以下 UART 需要在 CubeMX 配置后才能使用
    [UART_DEBUG] = {NULL},   // 调试串口 USART1 PA9/PA10
    [UART_DBUS] = {&huart5}, // 遥控接收 UART5 PD2
    [UART_RS485_1] = {NULL}, // RS485-1 USART2 PD4/PD5/PD6
    [UART_RS485_2] = {NULL}, // RS485-2 USART3 PB14/PD8/PD9
    [UART_EX_1] = {NULL},    // 扩展串口1 UART7 PE7/PE8
    [UART_EX_2] = {NULL},    // 扩展串口2 UART8 PE0/PE1
    [UART_EX_3] = {NULL},    // 扩展串口3 UART9 PD14/PD15
    [UART_EX_4] = {NULL},    // 扩展串口4 USART10 PE2/PE3
};

/*============================================
 *              CAN 映射数组
 *============================================*/
// const CAN_Map_t can_map[CAN_NUM_MAX] = {
//     // 注意：CAN 需要在 CubeMX 配置后才能使用
//     // [CAN_1] = {&hcan1}, // CAN1 PD0/PD1
//     // [CAN_2] = {&hcan2}, // CAN2 PB5/PB6
//     // [CAN_3] = {&hcan3}, // CAN3 PD12/PD13
// };

/*============================================
 *              SPI 映射数组
 *============================================*/
const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    // 注意：SPI1 需要在 CubeMX 配置后才能使用
    // [SPI_LCD] = {&hspi1},    // SPI1 LCD PB3/PB4/PD7/PE15
    [SPI_BMI088] = {&hspi2}, // SPI2 BMI088 PB13/PC1/PC2
};

/*============================================
 *              I2C 映射数组
 *============================================*/
const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    // 注意：I2C 需要在 CubeMX 配置后才能使用
    // [I2C_LCD] = {&hi2c2}, // I2C2 LCD PB10/PB11
};

/*============================================
 *              ADC 映射数组
 *============================================*/
const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT_VOLTAGE] = {&hadc1, ADC_CHANNEL_4}, // 电池电压 PC04 -> ADC1_INP4
};

#endif // DM_MC02

#if DEVELOPMENT_BOARD == DJI_C

#include "tim.h"
#include "usart.h"
#include "spi.h"
#include "i2c.h"
#include "adc.h"

/*============================================
 *              GPIO 映射数组
 *============================================*/
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    // 用户按键
    [GPIO_KEY_USER] = {GPIOA, GPIO_PIN_0}, // 用户按键 PA0

    // BMI088 控制
    [GPIO_BMI088_CS1_ACCEL] = {GPIOA, GPIO_PIN_4},  // BMI088 CS1_Accel PA4
    [GPIO_BMI088_CS1_GYRO] = {GPIOB, GPIO_PIN_0},   // BMI088 CS1_Gyro PB0
    [GPIO_BMI088_INT1_ACCEL] = {GPIOC, GPIO_PIN_4}, // BMI088 INT1_Accel PC4
    [GPIO_BMI088_INT1_GYRO] = {GPIOC, GPIO_PIN_5},  // BMI088 INT1_Gyro PC5

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

    // PWM定时器（TIM5 3通道 LED）
    [TIM_LED_B] = {&htim5, TIM_CHANNEL_1}, // LED蓝 PH10
    [TIM_LED_G] = {&htim5, TIM_CHANNEL_2}, // LED绿 PH11
    [TIM_LED_R] = {&htim5, TIM_CHANNEL_3}, // LED红 PH12

    // PWM定时器（TIM8 2通道）
    [TIM_PWM_5] = {&htim8, TIM_CHANNEL_1}, // PWM CH1 PC6
    [TIM_PWM_6] = {&htim8, TIM_CHANNEL_2}, // PWM CH2 PI6

    // 特殊功能定时器
    [TIM_LASER] = {&htim3, TIM_CHANNEL_3},  // 激光 TIM3_CH3 PC8
    [TIM_BUZZER] = {&htim4, TIM_CHANNEL_3}, // 蜂鸣器 TIM4_CH3 PD14
    [TIM_HEATER] = {&htim10, TIM_CHANNEL_1}, // BMI088加热 TIM10_CH1 PF6
};

/*============================================
 *              UART 映射数组
 *============================================*/
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart3}, // SBUS/DBUS遥控接收 USART3 PC11
};

/*============================================
 *              SPI 映射数组
 *============================================*/
const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_BMI088] = {&hspi1}, // SPI1 BMI088 PA7/PB3/PB4
};

/*============================================
 *              I2C 映射数组
 *============================================*/
const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_IST8310] = {&hi2c3}, // I2C3 IST8310磁力计 PA8/PC9
};

/*============================================
 *              ADC 映射数组
 *============================================*/
const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT_VOLTAGE] = {&hadc3, ADC_CHANNEL_8}, // 电池电压 ADC3_IN8 PF10
};

#endif // DJI_C
