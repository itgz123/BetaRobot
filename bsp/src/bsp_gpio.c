/**
 * @file bsp_gpio.c
 * @brief GPIO驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置
 */

#include "bsp_gpio.h"
#include "bsp_log.h"

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static GPIOInstance *s_gpio_instance[GPIO_INSTANCE_NUM] = {NULL};

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief EXTI中断回调函数
 * @note 根据GPIO_Pin找到对应的GPIOInstance，并调用回调函数
 * @param GPIO_Pin 发生中断的GPIO引脚号
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    GPIOInstance *gpio;
    for (uint8_t i = 0; i < s_idx; i++)
    {
        gpio = s_gpio_instance[i];
        if (gpio->map.pin == GPIO_Pin && gpio->callback != NULL)
        {
            gpio->callback(gpio);
            return;
        }
    }
}

/*------------- 外部接口实现 --------------*/

int8_t GPIORegister(GPIOInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[bsp_gpio] Instance is NULL!");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= GPIO_INSTANCE_NUM)
    {
        LOGERROR("[bsp_gpio] Exceeded max instance count!");
        return -1;
    }

    // 根据枚举自动填充硬件映射
    instance->map = gpio_map[instance->gpio_e];

    s_gpio_instance[s_idx++] = instance;
    LOGINFO("[bsp_gpio] Instance registered, idx=%d", s_idx - 1);
    return 0;
}

void GPIOToggle(GPIOInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_gpio] GPIOToggle: instance is NULL!");
        return;
    }
    HAL_GPIO_TogglePin(instance->map.port, instance->map.pin);
}

void GPIOSet(GPIOInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_gpio] GPIOSet: instance is NULL!");
        return;
    }
    HAL_GPIO_WritePin(instance->map.port, instance->map.pin, GPIO_PIN_SET);
}

void GPIOReset(GPIOInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_gpio] GPIOReset: instance is NULL!");
        return;
    }
    HAL_GPIO_WritePin(instance->map.port, instance->map.pin, GPIO_PIN_RESET);
}

GPIO_PinState GPIORead(GPIOInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_gpio] GPIORead: instance is NULL!");
        return GPIO_PIN_RESET;
    }
    return HAL_GPIO_ReadPin(instance->map.port, instance->map.pin);
}

void GPIOWrite(GPIOInstance *instance, GPIO_PinState state)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_gpio] GPIOWrite: instance is NULL!");
        return;
    }
    HAL_GPIO_WritePin(instance->map.port, instance->map.pin, state);
}