#include "bsp.h"

const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_LED_G] = {GPIOE, GPIO_PIN_14},
    [GPIO_LED_R] = {GPIOA, GPIO_PIN_1},
    [GPIO_MOTOR1_IN1] = {GPIOB, GPIO_PIN_12},
    [GPIO_MOTOR1_IN2] = {GPIOB, GPIO_PIN_13},
    [GPIO_MOTOR2_IN1] = {GPIOB, GPIO_PIN_14},
    [GPIO_MOTOR2_IN2] = {GPIOB, GPIO_PIN_15},
    [GPIO_MOTOR3_IN1] = {GPIOC, GPIO_PIN_9},
    [GPIO_MOTOR3_IN2] = {GPIOC, GPIO_PIN_10},
    [GPIO_MOTOR4_IN1] = {GPIOC, GPIO_PIN_11},
    [GPIO_MOTOR4_IN2] = {GPIOC, GPIO_PIN_12},
};

const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    [TIM_PWM_1] = {&htim1, TIM_CHANNEL_1},
    [TIM_PWM_2] = {&htim1, TIM_CHANNEL_2},
    [TIM_PWM_3] = {&htim1, TIM_CHANNEL_3},
    [TIM_PWM_4] = {&htim1, TIM_CHANNEL_4},
    [TIM_ENCODER_1] = {&htim2, 0},
    [TIM_ENCODER_2] = {&htim3, 0},
    [TIM_ENCODER_3] = {&htim4, 0},
    [TIM_ENCODER_4] = {&htim8, 0},
};

const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart2},
};

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
