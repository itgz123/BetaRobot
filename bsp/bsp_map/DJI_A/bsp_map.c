#include "bsp_map.h"

const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_OLED_KEY] = {GPIOA, GPIO_PIN_6},
    [GPIO_OLED_DC] = {GPIOB, GPIO_PIN_9},
    [GPIO_OLED_RST] = {GPIOB, GPIO_PIN_10},
    [GPIO_USER_KEY] = {GPIOB, GPIO_PIN_2},
    [GPIO_MPU6500_INT] = {GPIOB, GPIO_PIN_8},
    [GPIO_IST8310_RSTN] = {GPIOE, GPIO_PIN_2},
    [GPIO_IST8310_DRDY] = {GPIOE, GPIO_PIN_3},
    [GPIO_LED_R] = {GPIOE, GPIO_PIN_11},
    [GPIO_LED_G] = {GPIOF, GPIO_PIN_14},
    [GPIO_SD_EXTI] = {GPIOE, GPIO_PIN_15},
    [GPIO_LASER] = {GPIOG, GPIO_PIN_13},
    [GPIO_POWER_EN1] = {GPIOH, GPIO_PIN_2},
    [GPIO_POWER_EN2] = {GPIOH, GPIO_PIN_3},
    [GPIO_POWER_EN3] = {GPIOH, GPIO_PIN_4},
    [GPIO_POWER_EN4] = {GPIOH, GPIO_PIN_5},
    [GPIO_DAC_EXTI] = {GPIOI, GPIO_PIN_9},
};

const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    [TIM_HEATER] = {&htim3, TIM_CHANNEL_2},
    [TIM_BUZZER] = {&htim12, TIM_CHANNEL_1},
    [TIM_PWM_3Pin1] = {&htim1, TIM_CHANNEL_1},
    [TIM_PWM_3Pin2] = {&htim1, TIM_CHANNEL_2},
    [TIM_PWM_3Pin3] = {&htim1, TIM_CHANNEL_3},
    [TIM_PWM_3Pin4] = {&htim1, TIM_CHANNEL_4},
    [TIM_PWM_1] = {&htim2, TIM_CHANNEL_1},
    [TIM_PWM_2] = {&htim2, TIM_CHANNEL_2},
    [TIM_PWM_3] = {&htim2, TIM_CHANNEL_3},
    [TIM_PWM_4] = {&htim2, TIM_CHANNEL_4},
    [TIM_PWM_5] = {&htim4, TIM_CHANNEL_1},
    [TIM_PWM_6] = {&htim4, TIM_CHANNEL_2},
    [TIM_PWM_7] = {&htim4, TIM_CHANNEL_3},
    [TIM_PWM_8] = {&htim4, TIM_CHANNEL_4},
    [TIM_PWM_9] = {&htim5, TIM_CHANNEL_1},
    [TIM_PWM_10] = {&htim5, TIM_CHANNEL_2},
    [TIM_PWM_11] = {&htim5, TIM_CHANNEL_3},
    [TIM_PWM_12] = {&htim5, TIM_CHANNEL_4},
    [TIM_PWM_13] = {&htim8, TIM_CHANNEL_1},
    [TIM_PWM_14] = {&htim8, TIM_CHANNEL_2},
    [TIM_PWM_15] = {&htim8, TIM_CHANNEL_3},
    [TIM_PWM_16] = {&htim8, TIM_CHANNEL_4},
};

const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart1},
    [UART_2] = {&huart2},
    [UART_3] = {&huart3},
    [UART_7] = {&huart7},
    [UART_8] = {&huart8},
    [UART_4Pin_6] = {&huart6},
};

const CAN_Map_t can_map[CAN_NUM_MAX] = {
    [CAN_1] = {&hcan1},
    [CAN_2] = {&hcan2},
};

const SPI_Map_t spi_map[SPI_NUM_MAX] = {
    [SPI_OLED] = {&hspi1},
    [SPI_EX_4] = {&hspi4},
    [SPI_MPU6500] = {&hspi5},
};

const I2C_Map_t i2c_map[I2C_NUM_MAX] = {
    [I2C_EX_2] = {&hi2c2},
};

const ADC_Map_t adc_map[ADC_NUM_MAX] = {
    [ADC1_EX_8] = {&hadc1, ADC_CHANNEL_8},
    [ADC1_EX_9] = {&hadc1, ADC_CHANNEL_9},
    [ADC1_EX_10] = {&hadc1, ADC_CHANNEL_10},
    [ADC1_EX_11] = {&hadc1, ADC_CHANNEL_11},
    [ADC1_EX_12] = {&hadc1, ADC_CHANNEL_12},
    [ADC1_EX_13] = {&hadc1, ADC_CHANNEL_13},
    [ADC1_EX_14] = {&hadc1, ADC_CHANNEL_14},
    [ADC1_EX_15] = {&hadc1, ADC_CHANNEL_15},
    [ADC3_4] = {&hadc3, ADC_CHANNEL_4},
    [ADC3_5] = {&hadc3, ADC_CHANNEL_5},
    [ADC3_EX_8] = {&hadc3, ADC_CHANNEL_8},
};

const DAC_Map_t dac_map[DAC_NUM_MAX] = {
    [DAC_1] = {&hdac, DAC_CHANNEL_1},
    [DAC_2] = {&hdac, DAC_CHANNEL_2},
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(can_map, CAN_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(spi_map, SPI_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(i2c_map, I2C_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(adc_map, ADC_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(dac_map, DAC_NUM_MAX);
