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

// 底盘电机实例定义（使用 INSTANCE_DEF 宏）
// M3508, ID=6, CAN1, 开环模式
DJIMOTOR_INSTANCE_DEF(chassis_motor, CAN_1, 6, DJI_MODEL_M3508,
                      MOTOR_LOOP_OPEN, MOTOR_LOOP_OPEN,
                      0.0f, 0.0f, 0.0f,  // 电流环 PID（不使用）
                      0.0f, 0.0f, 0.0f,  // 速度环 PID（不使用）
                      0.0f, 0.0f, 0.0f); // 位置环 PID（不使用）

static void Init(void)
{
    // 注册电机实例（通过虚函数表）
    if (MotorRegister(&chassis_motor.base) != 0)
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
    MotorSetRef(&chassis_motor.base, 200.0f);

    // 调用电机控制发送（立即发送 CAN）
    MotorControl(&chassis_motor.base);
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
