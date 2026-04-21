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

BSP_STATIC_ASSERT_MAP_SIZE(gpio_map, GPIO_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(tim_map, TIM_NUM_MAX);
BSP_STATIC_ASSERT_MAP_SIZE(uart_map, UART_NUM_MAX);
