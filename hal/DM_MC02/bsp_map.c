#include "bsp.h"

const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_BMI088_CS_ACCEL] = {GPIOC, GPIO_PIN_0},
    [GPIO_BMI088_CS_GYRO] = {GPIOC, GPIO_PIN_3},
    [GPIO_BMI088_INT_ACCEL] = {GPIOE, GPIO_PIN_10},
    [GPIO_BMI088_INT_GYRO] = {GPIOE, GPIO_PIN_12},
    [GPIO_POWER_24V_EN1] = {GPIOC, GPIO_PIN_14},
    [GPIO_POWER_24V_EN2] = {GPIOC, GPIO_PIN_13},
    [GPIO_POWER_5V_EN] = {GPIOC, GPIO_PIN_15},
    [GPIO_LCD_KEY1] = {GPIOA, GPIO_PIN_5},
    [GPIO_LCD_KEY2] = {GPIOD, GPIO_PIN_10},
    [GPIO_EX_KEY] = {GPIOA, GPIO_PIN_15},
    [GPIO_USER_KEY] = {GPIOE, GPIO_PIN_14},
};

const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    [TIM_PWM_1] = {&htim2, TIM_CHANNEL_1},
    [TIM_PWM_2] = {&htim2, TIM_CHANNEL_3},
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_1},
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_3},
    [TIM_HEATER] = {&htim3, TIM_CHANNEL_4},
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_2},
};

const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_1] = {&huart1},
    [UART_SBUS] = {&huart5},
    [UART_RS485_2] = {&huart2},
    [UART_RS485_3] = {&huart3},
    [UART_7] = {&huart7},
    [UART_EX_8] = {&huart8},
    [UART_EX_9] = {&huart9},
    [UART_10] = {&huart10},
};

const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hfdcan1},
    [CAN_2] = {&hfdcan2},
    [CAN_3] = {&hfdcan3},
};

const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_LCD_1] = {&hspi1},
    [SPI_BMI088] = {&hspi2},
};

const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_LCD_2] = {&hi2c2},
};

const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC_BAT] = {&hadc1, ADC_CHANNEL_4},
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
