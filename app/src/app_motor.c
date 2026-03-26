#include "app_motor.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
//
#include "bsp_tim.h"
#include "bsp_gpio.h"
#include "drv_bmi088.h"

// BMI088_Data_t imu_data;
// float gx = 0;
// float gy = 0;
// float gz = 0;
// float ax = 0;
// float ay = 0;
// float az = 0;
// float ttt = 0;
void bmi_callback(struct SPIInstance *iii)
{
    LOGINFO("cback");
}
SPI_INSTANCE_DEF(bmi_ins, SPI_BMI088, SPI_BLOCK_MODE, 20, bmi_callback);
GPIO_INSTANCE_DEF(cs1, GPIO_BMI088_CS1, NULL);
GPIO_INSTANCE_DEF(cs2, GPIO_BMI088_CS2, NULL);
uint8_t tx_buff[10];

/*============================ 初始化函数 ============================*/

static void MOTORInit(void)
{
    // 注册 SPI 和 GPIO 实例
    SPIRegister(&bmi_ins);
    GPIORegister(&cs1);
    GPIORegister(&cs2);

    // 初始状态：CS 拉高
    GPIOSet(&cs1);
    GPIOSet(&cs2);

    // BMI088 加速度计上电后默认是 I2C 模式，需要先进行一次 fake read 切换到 SPI 模式
    tx_buff[0] = 0x80; // 读命令
    tx_buff[1] = 0x55; // dummy byte
    tx_buff[2] = 0x55; // dummy byte
    GPIOReset(&cs1);
    SPITransmitReceive(&bmi_ins, tx_buff, 3);
    GPIOSet(&cs1);
    // 等待切换完成
    vTaskDelay(pdMS_TO_TICKS(10));
}

/*============================ 任务函数 ============================*/

static void MOTORTask(void)
{
    // BMI088 加速度计读取需要: 读命令(0x80) + dummy byte + dummy byte
    // 数据在第 3 个字节（index 2）返回
    tx_buff[0] = 0x80; // 读命令：读寄存器 0x00 (CHIP_ID)
    tx_buff[1] = 0x55; // dummy byte
    tx_buff[2] = 0x55; // dummy byte
    GPIOReset(&cs1);
    SPITransmitReceive(&bmi_ins, tx_buff, 3);
    GPIOSet(&cs1);
    uint8_t rx_data = bmi_ins.rx_buff[2]; // 加速度计数据在 index 2
    LOGINFO("ACC_CHIP_ID: 0x%02X (expect 0x1E)", rx_data);
    vTaskDelay(pdMS_TO_TICKS(1000));
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
            // LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = %d(ms)", (dt / 1000));
            vTaskDelay(pdMS_TO_TICKS(MOTOR_FREQ_MS));
    }
}