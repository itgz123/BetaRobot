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

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
