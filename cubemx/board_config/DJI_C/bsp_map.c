#include "bsp.h"

const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_USER_KEY] = {GPIOA, GPIO_PIN_0},
    [GPIO_BMI088_CS_ACCEL] = {GPIOA, GPIO_PIN_4},
    [GPIO_BMI088_CS_GYRO] = {GPIOB, GPIO_PIN_0},
    [GPIO_BMI088_INT_ACCEL] = {GPIOC, GPIO_PIN_4},
    [GPIO_BMI088_INT_GYRO] = {GPIOC, GPIO_PIN_5},
    [GPIO_IST8310_DRDY] = {GPIOG, GPIO_PIN_3},
    [GPIO_IST8310_RSTN] = {GPIOG, GPIO_PIN_6},
};

const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    [TIM_PWM_1] = {&htim1, TIM_CHANNEL_1},
    [TIM_PWM_2] = {&htim1, TIM_CHANNEL_2},
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_3},
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_4},
    [TIM_PWM_5] = {&htim8, TIM_CHANNEL_1},
    [TIM_PWM_6] = {&htim8, TIM_CHANNEL_2},
    [TIM_PWM_7] = {&htim8, TIM_CHANNEL_3},
    [TIM_LED_B] = {&htim5, TIM_CHANNEL_1},
    [TIM_LED_G] = {&htim5, TIM_CHANNEL_2},
    [TIM_LED_R] = {&htim5, TIM_CHANNEL_3},
    [TIM_LASER] = {&htim3, TIM_CHANNEL_3},
    [TIM_BUZZER] = {&htim4, TIM_CHANNEL_3},
    [TIM_HEATER] = {&htim10, TIM_CHANNEL_1},
};

const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart3},
    [UART_1] = {&huart1},
    [UART_6] = {&huart6},
};

const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hcan1},
    [CAN_2] = {&hcan2},
};

const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_BMI088] = {&hspi1},
    [SPI_EX_2] = {&hspi2},
};

const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_EX_2] = {&hi2c2},
    [I2C_IST8310] = {&hi2c3},
};

const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT] = {&hadc3, ADC_CHANNEL_8},
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
