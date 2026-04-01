#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "drv_dcmotor.h"
// 测试代码
#include "app_cmd.h"
float speed1 = 0;
float set_speed = 0;
float set_duty = 0;
float kp = 3.5f;
float ki = 0.016f;
float kd = 0.018f;
float i_max = 5.0f;
float max_speed = 110;
float kl = 0.003979f;
float bl = 0.50f;
float kh = 0.004528;
float bh = 0.47928f;
float v_v = 65.0f;
/*============================ 实例定义 ============================*/

// 电机1：PWM TIM1, 编码器 TIM2, GPIO IN1/IN2
DCMOTOR_INSTANCE_DEF(motor1, TIM_PWM_1, TIM_ENCODER_1, GPIO_MOTOR_1_IN1, GPIO_MOTOR_1_IN2);

/*============================ 初始化函数 ============================*/

static void MOTORInit(void)
{
    DCMotorInit(&motor1, 11, 9.6f, 0.01f);
    DCMotorSetPID(&motor1, kp, ki, kd, i_max, max_speed, kl, bl, kh, bh, v_v);
    LOGINFO("[app_motor] Motor1 initialized");
}

/*============================ 任务函数 ============================*/

ITCM_RAM static void MOTORTask(void)
{
    static float dt = 0.0f;
    static uint64_t last_time = 0;
    // 计算时间间隔
    uint64_t current_time = DWT_GetTimeline_us();
    if (last_time > 0)
    {
        dt = (current_time - last_time) / 1000000.0f; // us -> s
    }
    last_time = current_time;
    set_speed = ch1 * motor1.max_speed;
    DCMotorSetPID(&motor1, kp, ki, kd, i_max, max_speed, kl, bl, kh, bh, v_v);
    // 速度控制
    DCMotorSetSpeed(&motor1, set_speed, dt);

    // DCMotorSetDutyRatio(&motor1, set_duty);
    speed1 = DCMotorGetSpeed(&motor1);
}

/*============================ 公开接口 ============================*/

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
ITCM_RAM __attribute__((noreturn)) void StartMotorTask(void *argument)
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