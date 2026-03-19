#include "app_robot.h"
#include "bsp_cfg.h"
#include "bsp_gpio.h"

void function_in_main_c(void)
{
    GPIO_INSTANCE_DEF(led_green, GPIO_LED_GREEN, NULL);
    GPIORegister(&led_green);
}