/**
 * @file app_sensor.c
 * @brief 传感器任务模块：IMU 数据采集与处理
 */

#include "app_sensor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

#if (SPI_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)
#include "drv_bmi088.h"
/*============================ 私有变量 ============================*/
BMI088_INSTANCE_DEF(bmi088, SPI_BMI088, GPIO_BMI088_CS1_ACCEL, GPIO_BMI088_CS1_GYRO, GPIO_BMI088_INT1_ACCEL, GPIO_BMI088_INT1_GYRO, TIM_HEATER);
BMI088_Data_t data;
float ax;
float ay;
float az;
float gx;
float gy;
float gz;
float temperature;
#endif

/*============================ 初始化函数 ============================*/

static void Init(void)
{
#if (SPI_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)
    // 注册 BMI088 实例
    BMI088Register(&bmi088);

    // 配置并初始化 BMI088
    // 加速度计：±3G量程，正常滤波模式，400Hz ODR
    // 陀螺仪：±2000°/s量程，2000Hz ODR，230Hz带宽
    BMI088SetConfig(&bmi088, BMI088_ACC_RANGE_3G, BMI088_ACC_BWP_NORMAL, BMI088_ACC_ODR_400, BMI088_GYRO_RANGE_2000, BMI088_GYRO_ODR_2000, BMI088_GYRO_BW_230, BMI088_MODE_POLLING);
#endif
}

/*============================ 任务函数 ============================*/

ITCM_RAM static void Run(void)
{
#if (SPI_INSTANCE_NUM > 0) && (GPIO_INSTANCE_NUM > 0)
    data = BMI088ReadBlocking(&bmi088);
    ax = data.acc[0];
    ay = data.acc[1];
    az = data.acc[2];
    gx = data.gyro[0];
    gy = data.gyro[1];
    gz = data.gyro[2];
    temperature = data.temp;
#endif
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
