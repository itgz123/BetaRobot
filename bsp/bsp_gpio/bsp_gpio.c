/**
 * @file bsp_gpio.c
 * @brief GPIO驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置
 */

#include "bsp_gpio.h"

#if GPIO_INSTANCE_NUM > 0

#include "bsp_log.h"

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
#if GPIO_INSTANCE_NUM > 0
static GPIOInstance *s_gpio_instance[GPIO_INSTANCE_NUM] = {NULL};
static GPIOInstance *s_exti_pin_instance[16] = {NULL};
#else
static GPIOInstance **s_gpio_instance = NULL;
#endif

/**
 * @brief 判断 GPIO_Pin 是否为单比特掩码
 */
static uint8_t GPIOIsSingleBitPin(uint16_t pin)
{
    return (pin != 0U) && ((pin & (pin - 1U)) == 0U);
}

/**
 * @brief 将 GPIO_Pin 掩码转换为 pin index（0~15）
 * @return 0xFF 表示非法掩码
 */
static uint8_t GPIOPinToIndex(uint16_t pin)
{
    if (!GPIOIsSingleBitPin(pin))
    {
        return 0xFF;
    }

    uint8_t idx = 0;
    while ((pin >>= 1U) != 0U)
    {
        idx++;
    }
    return idx;
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief EXTI中断回调函数
 * @note 根据GPIO_Pin找到对应的GPIOInstance，并调用回调函数
 * @param GPIO_Pin 发生中断的GPIO引脚号
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint8_t pin_idx = GPIOPinToIndex(GPIO_Pin);
    if (pin_idx < 16)
    {
        GPIOInstance *gpio = s_exti_pin_instance[pin_idx];
        if (gpio != NULL && gpio->callback != NULL)
        {
            gpio->callback(gpio);
            return;
        }
    }

    // 异常情况下回退到线性扫描，保证兼容性
    for (uint8_t i = 0; i < s_idx; i++)
    {
        GPIOInstance *gpio = s_gpio_instance[i];
        if (gpio->map.pin == GPIO_Pin && gpio->callback != NULL)
        {
            gpio->callback(gpio);
            return;
        }
    }
}

/*------------- 外部接口实现 --------------*/

int8_t GPIORegister(GPIOInstance *instance, const GPIO_Init_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_gpio] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_gpio] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_idx >= GPIO_INSTANCE_NUM, -1, LOGERROR("[bsp_gpio] Exceeded max instance count!"));
    BSP_RETURN_IF_TRUE_LOG(instance->gpio_e >= GPIO_NUM_MAX, -1, LOGERROR("[bsp_gpio] gpio_e out of range!"));

    // 将配置拷贝到实例
    instance->callback = config->callback;

    // 根据枚举自动填充硬件映射
    instance->map = gpio_map[instance->gpio_e];

    BSP_RETURN_IF_TRUE_LOG(instance->map.port == NULL || !GPIOIsSingleBitPin(instance->map.pin), -1, LOGERROR("[bsp_gpio] Invalid GPIO map, check bsp_cfg mapping!"));

    uint8_t pin_idx = GPIOPinToIndex(instance->map.pin);
    BSP_RETURN_IF_TRUE_LOG(pin_idx >= 16, -1, LOGERROR("[bsp_gpio] Invalid pin index!"));

    // 仅对使用 EXTI 回调的实例做冲突检查
    // EXTI 回调只有 GPIO_Pin 参数，因此同一 pin 位只能绑定一个回调
    if (instance->callback != NULL)
    {
        if (s_exti_pin_instance[pin_idx] != NULL)
        {
            LOGERROR("[bsp_gpio] Duplicate EXTI pin registration is not allowed!");
            return -1;
        }
    }

    s_gpio_instance[s_idx++] = instance;
    if (instance->callback != NULL)
    {
        s_exti_pin_instance[pin_idx] = instance;
    }
    LOGINFO("[bsp_gpio] GPIO instance registered, idx=%d", s_idx - 1);
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

#endif
