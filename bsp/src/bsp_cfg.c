/**
 * @file bsp_cfg.c
 * @brief 板级硬件映射实现
 *
 * @note 不同开发板需要修改本文件中的映射数组
 *       CubeMX重新生成后需确认映射是否正确
 */

#include "bsp_cfg.h"
#include "tim.h"
#include "usart.h"

#if DEVELOPMENT_BOARD == STM32F407VET6

/*============================================
 *              GPIO 映射数组
 *============================================*/
// 根据 BoardGPIO_e 枚举顺序定义
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
// 根据 BoardTIM_e 枚举顺序定义
// PWM通道在 drv 层通过参数传递，此处 channel 填 0
const TIM_Map_t tim_map[TIM_NUM_MAX] = {
    // PWM定时器
    [TIM_MOTOR_PWM] = {&htim1, 0}, // 电机PWM TIM1

    // 编码器定时器
    [TIM_ENCODER_1] = {&htim2, 0}, // 编码器1 TIM2
    [TIM_ENCODER_2] = {&htim3, 0}, // 编码器2 TIM3
    [TIM_ENCODER_3] = {&htim4, 0}, // 编码器3 TIM4
    [TIM_ENCODER_4] = {&htim8, 0}, // 编码器4 TIM8
};

/*============================================
 *              UART 映射数组
 *============================================*/
// 根据 BoardUART_e 枚举顺序定义
const UART_Map_t uart_map[UART_NUM_MAX] = {
    [UART_SBUS] = {&huart2}, // SBUS遥控接收 UART2
};

#endif // STM32F407VET6