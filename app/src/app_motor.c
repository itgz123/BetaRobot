#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "drv_bmi088.h"

/*============================ 私有变量 ============================*/

// 定义 BMI088 实例（一次性定义所有子实例）
// 参数：name, spi_idx, cs_acc_idx, cs_gyro_idx, int_acc_idx, int_gyro_idx, heater_idx
BMI088_INSTANCE_DEF(bmi088, SPI_BMI088, GPIO_BMI088_CS1, GPIO_BMI088_CS2, GPIO_BMI088_INT1, GPIO_BMI088_INT3, TIM_HEATER);
BMI088_Data_t data;
float ax;
float ay;
float az;
float gx;
float gy;
float gz;
float temperature;
/*============================ 初始化函数 ============================*/

static void MOTORInit(void)
{
    // 注册 BMI088 实例
    BMI088Register(&bmi088);

    // 配置并初始化 BMI088
    // 加速度计：±3G量程，正常滤波模式，400Hz ODR
    // 陀螺仪：±2000°/s量程，2000Hz ODR，230Hz带宽
    BMI088SetConfig(&bmi088, BMI088_ACC_RANGE_3G, BMI088_ACC_BWP_NORMAL, BMI088_ACC_ODR_400, BMI088_GYRO_RANGE_2000, BMI088_GYRO_ODR_2000, BMI088_GYRO_BW_230, BMI088_MODE_POLLING);
}

/*============================ 任务函数 ============================*/

static void MOTORTask(void)
{
    data = BMI088ReadBlocking(&bmi088);
    ax = data.acc[0];
    ay = data.acc[1];
    az = data.acc[2];
    gx = data.gyro[0];
    gy = data.gyro[1];
    gz = data.gyro[2];
    temperature = data.temp;
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