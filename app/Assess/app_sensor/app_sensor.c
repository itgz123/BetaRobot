/**
 * @file app_sensor.c
 * @brief 传感器任务模块：IMU 数据采集与处理
 */

#include "app_sensor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

#include "drv_bmi088.h"
#include "app_robot.h"
#include "drv_chassis.h"

/*============================ 私有变量 ============================*/
#define headless_w 1.0f
BMI088_INSTANCE_DEF(bmi088, SPI_BMI088, GPIO_BMI088_CS_ACCEL, GPIO_BMI088_CS_GYRO, GPIO_BMI088_INT_ACCEL, GPIO_BMI088_INT_GYRO, TIM_HEATER);
BMI088_Data_t data;
float gz_now;                   // 现在读取的gz
float filter_out_gz;            // 滤波后的gz
float last_gz;                  // 上次滤波后的gz
float alpha = 0.1f;             // 滤波系数
ChassisVelCmd_t chassis_cmd;    // 底盘命令结构体
ChassisInstance my_chassis_ins; // 底盘实例
float gimbal_w;                 // 云台转速

/*============================ 初始化函数 ============================*/

static void Init(void)
{
    // 注册 BMI088 实例
    BMI088Register(&bmi088);

    // 配置并初始化 BMI088
    // 加速度计：±3G量程，正常滤波模式，400Hz ODR
    // 陀螺仪：±2000°/s量程，2000Hz ODR，230Hz带宽
    BMI088SetConfig(&bmi088, BMI088_ACC_RANGE_3G, BMI088_ACC_BWP_NORMAL, BMI088_ACC_ODR_400, BMI088_GYRO_RANGE_2000, BMI088_GYRO_ODR_2000, BMI088_GYRO_BW_230, BMI088_MODE_POLLING);

    // 初始化底盘实例（电机指针暂时传 NULL，后续完善）
    ChassisInit(&my_chassis_ins, NULL, NULL, NULL, NULL, 0.05f, 0.0f, 0.0f, 0.0f, 0.15f, 0.15f, CHASSIS_TYPE_OMNI_CROSS, 0.0f, 0.0f);
} // 半径，长，宽

/*============================ 任务函数 ============================*/
ITCM_RAM static void Run(void)
{
    // 读取角度
    data = BMI088ReadBlocking(&bmi088);
    gz_now = data.gyro[2];
    filter_out_gz = alpha * gz_now + (1.0f - alpha) * last_gz;
    last_gz = filter_out_gz;

    // 从队列获取底盘命令（非阻塞，使用最新数据）
    xQueuePeek(chassis_cmd_queue, &chassis_cmd, 0);
    // 控制电机
    if (CHASSIS_MODE_DISABLE == chassis_cmd.mode)
    {
        my_chassis_ins.wheel_speed.w1 = 0;
        my_chassis_ins.wheel_speed.w2 = 0;
        my_chassis_ins.wheel_speed.w3 = 0;
        my_chassis_ins.wheel_speed.w4 = 0;
    }
    else if (CHASSIS_MODE_HEADLESS == chassis_cmd.mode)
    {
        gimbal_w = chassis_cmd.w; // 记录云台速度
        InverseOmniCross(&my_chassis_ins, chassis_cmd.vx, chassis_cmd.vy, headless_w, &(my_chassis_ins.wheel_speed));
        // 速度旋转角度，旋转到头的方向
        // 控制云台电机
    }
    else if (CHASSIS_MODE_ENABLE == chassis_cmd.mode)
    {
        InverseOmniCross(&my_chassis_ins, chassis_cmd.vx, chassis_cmd.vy, chassis_cmd.w, &(my_chassis_ins.wheel_speed));
    }

    // 控制底盘电机
    // my_chassis_ins.wheel_speed.w1
}

/**
 * @brief 传感器任务入口函数
 * @param argument 未使用
 */
ITCM_RAM __attribute__((noreturn)) void StartSensorTask(void *argument)
{
    Init();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] SENSOR Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        Run();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > SENSOR_FREQ_MS)
            LOGERROR("[freeRTOS] SENSOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
        vTaskDelay(pdMS_TO_TICKS(SENSOR_FREQ_MS));
    }
}
