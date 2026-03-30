#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "drv_dcmotor.h"

#include "app_cmd.h"
float speed1 = 0;
float set_duty = 0;
/*============================ 实例定义 ============================*/

// 电机1：编码器 11 PPR，减速比 9.6:1，滤波系数 0.01
DCMOTOR_INSTANCE_DEF(motor1, TIM_PWM_1, TIM_ENCODER_1, GPIO_MOTOR_1_IN1, GPIO_MOTOR_1_IN2, 11, 9.6f, 0.01f);

/*============================ 初始化函数 ============================*/

static void MOTORInit(void)
{
    DCMotorRegister(&motor1);
    LOGINFO("[app_motor] Motor1 registered");
}

/*============================ 任务函数 ============================*/

ITCM_RAM static void MOTORTask(void)
{
    DCMotorSetDutyRatio(&motor1, set_duty);
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