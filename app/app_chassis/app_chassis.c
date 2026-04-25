/**
 * @file app_chassis.c
 * @brief 底盘控制任务模块
 *
 * @note APP 层职责：
 *       1. 定义电机实例并初始化
 *       2. 从 app_cmd 接收速度命令
 *       3. 调用 DRV 层运动学解算
 *       4. 控制电机速度
 */

#include "app_chassis.h"
#include "app_cmd.h"
#include "app_robot.h"
#include "app_cfg.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "bsp_can.h"
#include "drv_djimotor.h"

// 底盘电机实例
static DJIMotorInstance chassis_motor;

static void Init(void)
{
    // 初始化底盘电机 (M3508, ID=6, CAN1)
    DJIMotorInitConfig_t motor_config = {
        .type = MOTOR_TYPE_DJI_M3508,
        .can_e = CAN_1,
        .motor_id = 6,

        // 开环模式：直接输出电流值
        .outer_loop_type = MOTOR_LOOP_OPEN,
        .close_loop_type = MOTOR_LOOP_OPEN,
        .motor_reverse = 0,
        .feedback_reverse = 0,

        // PID 参数（开环模式不使用）
        .current_kp = 0.0f,
        .current_ki = 0.0f,
        .current_kd = 0.0f,
        .speed_kp = 0.0f,
        .speed_ki = 0.0f,
        .speed_kd = 0.0f,
        .angle_kp = 0.0f,
        .angle_ki = 0.0f,
        .angle_kd = 0.0f,

        // 外部反馈（不使用）
        .angle_feedback_ptr = NULL,
        .speed_feedback_ptr = NULL,
        .speed_ff_ptr = NULL,
        .current_ff_ptr = NULL,
    };

    if (DJIMotorRegister(&chassis_motor, &motor_config) != 0)
    {
        LOGERROR("[chassis] Motor register failed!");
    }
    else
    {
        LOGINFO("[chassis] Motor registered successfully");
        MotorEnable(&chassis_motor.base);
    }
}

ITCM_RAM static void Run(void)
{
    // 开环模式：直接设置输出电流值
    // M3508 电流范围: -16384 ~ 16384
    // 设置一个较小的电流值让电机转动
    MotorSetRef(&chassis_motor.base, 200.0f);

    // 调用 DJI 电机控制发送
    DJIMotorControl();
}

/*============================================
 *              公开接口
 *============================================*/

/**
 * @brief  Chassis 任务函数
 * @param  argument: 未使用
 * @retval None
 */
ITCM_RAM __attribute__((noreturn)) void StartChassisTask(void *argument)
{
    Init();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] CHASSIS Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        Run();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > CHASSIS_FREQ_MS)
            LOGERROR("[freeRTOS] CHASSIS Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(CHASSIS_FREQ_MS));
    }
}
