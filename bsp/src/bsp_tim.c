/**
 * @file bsp_tim.c
 * @brief 定时器驱动封装实现（PWM + 编码器）
 *
 * @note 只负责实例管理、回调分发，不负责硬件配置
 */

#include "bsp_tim.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "string.h"

/*============= PWM 部分 =============*/

/*------------- 私有变量 --------------*/
static uint8_t s_pwm_idx = 0;
#if PWM_INSTANCE_NUM > 0
static PWMInstance *s_pwm_instance[PWM_INSTANCE_NUM] = {NULL};
#else
static PWMInstance **s_pwm_instance = NULL;
#endif

/*------------- PWM接口实现 --------------*/

int8_t PWMRegister(PWMInstance *instance)
{
    if (instance == NULL)
    {
        LOGERROR("[bsp_tim] PWM instance is NULL!");
        return -1;
    }

    if (s_pwm_idx >= PWM_INSTANCE_NUM)
    {
        LOGERROR("[bsp_tim] PWM exceeded max instance count!");
        return -1;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_pwm_idx; i++)
    {
        if (s_pwm_instance[i]->map.htim == tim_map[instance->tim_e].htim &&
            s_pwm_instance[i]->map.channel == tim_map[instance->tim_e].channel)
        {
            LOGERROR("[bsp_tim] PWM instance already registered!");
            return -1;
        }
    }

    // 根据枚举自动填充硬件映射
    instance->map = tim_map[instance->tim_e];

    // 启动PWM
    HAL_TIM_PWM_Start(instance->map.htim, instance->map.channel);

    // 设置初始占空比
    PWMSetDutyRatio(instance, instance->dutyratio);

    s_pwm_instance[s_pwm_idx++] = instance;

    LOGINFO("[bsp_tim] PWM instance registered, idx=%d", s_pwm_idx - 1);
    return 0;
}

void PWMSetDutyRatio(PWMInstance *instance, float dutyratio)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_tim] PWM instance is NULL!");
        return;
    }

    // 钳位到有效范围
    if (dutyratio < 0.0f)
        dutyratio = 0.0f;
    if (dutyratio > 1.0f)
        dutyratio = 1.0f;

    instance->dutyratio = dutyratio;
    uint32_t ccr = (uint32_t)(dutyratio * instance->map.htim->Instance->ARR);
    __HAL_TIM_SET_COMPARE(instance->map.htim, instance->map.channel, ccr);
}

/*============= 编码器 部分 =============*/

/*------------- 私有变量 --------------*/
static uint8_t s_encoder_idx = 0;
#if ENCODER_INSTANCE_NUM > 0
static EncoderInstance *s_encoder_instance[ENCODER_INSTANCE_NUM] = {NULL};
#else
static EncoderInstance **s_encoder_instance = NULL;
#endif

/*------------- 编码器接口实现 --------------*/

int8_t EncoderRegister(EncoderInstance *instance)
{
    if (instance == NULL)
    {
        LOGERROR("[bsp_tim] Encoder instance is NULL!");
        return -1;
    }

    if (s_encoder_idx >= ENCODER_INSTANCE_NUM)
    {
        LOGERROR("[bsp_tim] Encoder exceeded max instance count!");
        return -1;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_encoder_idx; i++)
    {
        if (s_encoder_instance[i]->map.htim == tim_map[instance->tim_e].htim)
        {
            LOGERROR("[bsp_tim] Encoder instance already registered!");
            return -1;
        }
    }

    // 根据枚举自动填充硬件映射
    instance->map = tim_map[instance->tim_e];
    instance->arr = instance->map.htim->Instance->ARR + 1; // 溢出周期 = ARR + 1
    instance->last_time = DWT_GetTimeline_s();

    // 启动编码器
    HAL_TIM_Encoder_Start(instance->map.htim, TIM_CHANNEL_ALL);

    // 单独使能更新中断（用于溢出检测）
    __HAL_TIM_ENABLE_IT(instance->map.htim, TIM_IT_UPDATE);

    s_encoder_instance[s_encoder_idx++] = instance;

    LOGINFO("[bsp_tim] Encoder instance registered, idx=%d", s_encoder_idx - 1);
    return 0;
}

float EncoderGetSpeed(EncoderInstance *instance)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    float current_time = DWT_GetTimeline_s();
    int32_t current_count = (int32_t)__HAL_TIM_GET_COUNTER(instance->map.htim);

    // 计算扩展后的总计数：溢出次数 * 溢出周期 + 当前计数值
    instance->total_count = (int64_t)instance->overflow_count * instance->arr + current_count;

    float dt = current_time - instance->last_time;
    if (dt > 0.0001f)
    {
        // 使用扩展后的总计数计算速度，正确处理溢出
        instance->speed = (float)(instance->total_count - instance->last_total_count) / dt;
    }

    instance->last_total_count = instance->total_count;
    instance->last_time = current_time;

    return instance->speed;
}

void EncoderClearCount(EncoderInstance *instance)
{
    if (instance == NULL)
    {
        LOGWARNING("[bsp_tim] Encoder instance is NULL!");
        return;
    }

    __HAL_TIM_SET_COUNTER(instance->map.htim, 0);
    instance->overflow_count = 0;
    instance->total_count = 0;
    instance->last_total_count = 0;
}

/*============= 溢出回调分发 =============*/

void BSPTim_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // 匹配编码器实例
    for (uint8_t i = 0; i < s_encoder_idx; i++)
    {
        if (s_encoder_instance[i]->map.htim == htim)
        {
            // 根据计数方向更新溢出次数
            if (__HAL_TIM_IS_TIM_COUNTING_DOWN(htim))
            {
                s_encoder_instance[i]->overflow_count--;
            }
            else
            {
                s_encoder_instance[i]->overflow_count++;
            }
            return;
        }
    }
}