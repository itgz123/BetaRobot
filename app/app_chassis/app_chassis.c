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

#if (PWM_INSTANCE_NUM > 0) && (ENCODER_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)
#include "drv_dcmotor.h"
#include "drv_chassis.h"
#endif

/*============================================
 *              电机参数配置
 *============================================*/

// // PID 参数
// #define MOTOR_KP 3.5f
// #define MOTOR_KI 0.016f
// #define MOTOR_KD 0.018f
// #define MOTOR_I_MAX 5.0f
// #define MOTOR_MAX_SPEED 110.0f // 最大速度 rad/s

// // 前馈参数
// #define MOTOR_KL 0.003979f
// #define MOTOR_BL 0.50f
// #define MOTOR_KH 0.004528f
// #define MOTOR_BH 0.47928f
// #define MOTOR_V_V 65.0f

// // 电机编码器线数、减速比和低通滤波系数
// #define MOTOR_ENCODER_NUM 11
// #define MOTOR_GEAR_RATIO 9.6f
// #define MOTOR_LPF_ALPHA 0.01f

/*============================================
 *              电机实例定义
 *============================================*/

// // 电机1: 左前 LF
// DCMOTOR_INSTANCE_DEF(motor_lf, TIM_PWM_1, TIM_ENCODER_1, GPIO_MOTOR_1_IN1, GPIO_MOTOR_1_IN2);
// // 电机2: 左后 LB
// DCMOTOR_INSTANCE_DEF(motor_lb, TIM_PWM_2, TIM_ENCODER_2, GPIO_MOTOR_2_IN1, GPIO_MOTOR_2_IN2);
// // 电机3: 右后 RB
// DCMOTOR_INSTANCE_DEF(motor_rb, TIM_PWM_3, TIM_ENCODER_3, GPIO_MOTOR_3_IN1, GPIO_MOTOR_3_IN2);
// // 电机4: 右前 RF
// DCMOTOR_INSTANCE_DEF(motor_rf, TIM_PWM_4, TIM_ENCODER_4, GPIO_MOTOR_4_IN1, GPIO_MOTOR_4_IN2);

/*============================================
 *              底盘实例定义
 *============================================*/

// static ChassisInstance chassis;

/*============================================
 *              私有变量
 *============================================*/

// static ChassisVelCmd_t chassis_cmd = {0};

/*============================================
 *              初始化函数
 *============================================*/

static void Init(void)
{
    // // 初始化 4 个电机
    // DCMotorInit(&motor_lf, MOTOR_ENCODER_NUM, MOTOR_GEAR_RATIO, MOTOR_LPF_ALPHA);
    // DCMotorInit(&motor_lb, MOTOR_ENCODER_NUM, MOTOR_GEAR_RATIO, MOTOR_LPF_ALPHA);
    // DCMotorInit(&motor_rb, MOTOR_ENCODER_NUM, MOTOR_GEAR_RATIO, MOTOR_LPF_ALPHA);
    // DCMotorInit(&motor_rf, MOTOR_ENCODER_NUM, MOTOR_GEAR_RATIO, MOTOR_LPF_ALPHA);

    // // 设置 PID 参数
    // DCMotorSetPID(&motor_lf, MOTOR_KP, MOTOR_KI, MOTOR_KD, MOTOR_I_MAX, MOTOR_MAX_SPEED, MOTOR_KL, MOTOR_BL, MOTOR_KH, MOTOR_BH, MOTOR_V_V);
    // DCMotorSetPID(&motor_lb, MOTOR_KP, MOTOR_KI, MOTOR_KD, MOTOR_I_MAX, MOTOR_MAX_SPEED, MOTOR_KL, MOTOR_BL, MOTOR_KH, MOTOR_BH, MOTOR_V_V);
    // DCMotorSetPID(&motor_rb, MOTOR_KP, MOTOR_KI, MOTOR_KD, MOTOR_I_MAX, MOTOR_MAX_SPEED, MOTOR_KL, MOTOR_BL, MOTOR_KH, MOTOR_BH, MOTOR_V_V);
    // DCMotorSetPID(&motor_rf, MOTOR_KP, MOTOR_KI, MOTOR_KD, MOTOR_I_MAX, MOTOR_MAX_SPEED, MOTOR_KL, MOTOR_BL, MOTOR_KH, MOTOR_BH, MOTOR_V_V);

    // // 初始化底盘实例
    // ChassisInit(&chassis, &motor_lf.base, &motor_lb.base, &motor_rb.base, &motor_rf.base, WHEEL_RADIUS, WHEELBASE_A, WHEELBASE_B, CHASSIS_L, OMNI_CROSS_A, OMNI_CROSS_B, (ChassisType_e)CHASSIS_TYPE, CHASSIS_MAX_SPEED, CHASSIS_MAX_W);

    // LOGINFO("[app_chassis] Chassis initialized with 4 motors");
}

/*============================================
 *              任务函数
 *============================================*/

ITCM_RAM static void Run(void)
{
    // // 从队列获取底盘命令（非阻塞，使用最新数据）
    // xQueuePeek(chassis_cmd_queue, &chassis_cmd, 0);

    // // 使用实例接口控制底盘（模式判断已在 ChassisSetVel 内部处理）
    // ChassisSetVel(&chassis, &chassis_cmd);
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
