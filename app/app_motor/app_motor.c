#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "drv_dcmotor.h"

// #include "app_cmd.h"
// float set_speed;
// float speed1;
// DCMOTOR_INSTANCE_DEF(motor_1, TIM_PWM_4, TIM_ENCODER_4, GPIO_MOTOR_4_IN1, GPIO_MOTOR_4_IN2);

static void Init(void)
{
    // DCMotorInit(&motor_1, 11, 9.6f, 0.01f);
    // DCMotorSetPID(&motor_1, 3.5f, 0.016f, 0.018f, 5.0f, 110.0f, 0.003979f, 0.50f, 0.004528f, 0.47928f, 65.0f);
}

/*============================ 任务函数 ============================*/

ITCM_RAM static void Run(void)
{
    // static float dt = 0.0f;
    // static uint64_t last_time = 0;
    // // 计算时间间隔
    // uint64_t current_time = DWT_GetTimeline_us();
    // if (last_time > 0)
    // {
    //     dt = (current_time - last_time) / 1000000.0f; // us -> s
    // }
    // last_time = current_time;
    // set_speed = ch1 * motor_1.max_speed;
    // // 速度控制
    // DCMotorSetSpeed(&motor_1, set_speed, dt);
    // speed1 = DCMotorGetSpeed(&motor_1);
}

/*============================ 公开接口 ============================*/

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
ITCM_RAM __attribute__((noreturn)) void StartMotorTask(void *argument)
{
    Init();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        Run();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > MOTOR_FREQ_MS)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(MOTOR_FREQ_MS));
    }
}