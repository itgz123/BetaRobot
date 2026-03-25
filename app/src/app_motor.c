#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
//
#include "bsp_tim.h"
#include "drv_bmi088.h"

BMI088_Data_t imu_data;
float gx = 0;
float gy = 0;
float gz = 0;
float ax = 0;
float ay = 0;
float az = 0;
float ttt = 0;

/*============================ 私有变量 ============================*/

/*
 * BMI088 实例定义
 * 参数: 实例名, SPI枚举, 加速度计CS, 陀螺仪CS, 加速度计INT, 陀螺仪INT, 加热TIM, 回调函数
 */
BMI088_INSTANCE_DEF(imu, SPI_BMI088, GPIO_BMI088_CS1, GPIO_BMI088_CS2, GPIO_BMI088_INT1, GPIO_BMI088_INT3, TIM_HEATER, NULL);

/*============================ 初始化函数 ============================*/

static void MOTORInit(void)
{
    // 注册 BMI088
    if (BMI088Register(&imu) != 0)
    {
        LOGERROR("[MOTOR] BMI088 register failed");
    }
    else
    {
        LOGINFO("[MOTOR] BMI088 register success");
    }
}

/*============================ 任务函数 ============================*/

static void MOTORTask(void)
{
    // 获取 IMU 数据
    if (BMI088GetData(&imu, &imu_data))
    {
        ax = imu_data.acc[0];
        ay = imu_data.acc[1];
        az = imu_data.acc[2];
        gx = imu_data.gyro[0];
        gy = imu_data.gyro[1];
        gz = imu_data.gyro[2];
        ttt = imu_data.temp;
    }
}

/*============================ 公开接口 ============================*/

/**
 * @brief  Motor 任务函数
 * @param  argument: 未使用
 * @retval None
 */
__attribute__((noreturn)) void StartMotorTask(void *argument)
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