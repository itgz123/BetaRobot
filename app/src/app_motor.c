#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "bsp_tim.h"
#include "app_cfg.h"

/*------------- 实例定义 --------------*/
ENCODER_INSTANCE_DEF(encoder_1, TIM_ENCODER_1);
PWM_INSTANCE_DEF(pwm_1, TIM_PWM_1);
float speed;

static void MOTORInit(void)
{
    EncoderRegister(&encoder_1);
    PWMRegister(&pwm_1);
}

static void MOTORTask(void)
{
    speed = EncoderGetSpeed(&encoder_1);
}

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartMotorTask(void *argument)
{
    MOTORInit();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        MOTORTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > MOTOR_FREQ_MS)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(MOTOR_FREQ_MS));
    }
}
