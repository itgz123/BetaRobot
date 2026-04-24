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

void ch_callback(struct CANInstance *_isn)
{
    uint8_t data = _isn->rx_buff[0];
    // LOGINFO("callback%d", data);
}

// CAN_INSTANCE_DEF(s_chassis_can, CAN_1, 0x141, 0x206, ch_callback);
// 使用掩码模式，0x7FF 掩码实现精确匹配单个 ID
CAN_INSTANCE_DEF_MASK(s_chassis_can, CAN_1, 0x1FF, 0x208, 0x7FF, ch_callback);

static void Init(void)
{
    CANRegister(&s_chassis_can);
    CANSetDLC(&s_chassis_can, 8);
}

ITCM_RAM static void Run(void)
{
    s_chassis_can.tx_buff[0] = 0x00;
    s_chassis_can.tx_buff[1] = 0x00;
    s_chassis_can.tx_buff[2] = 0x00;
    s_chassis_can.tx_buff[3] = 0xff;
    s_chassis_can.tx_buff[4] = 0x00;
    s_chassis_can.tx_buff[5] = 0x33;
    s_chassis_can.tx_buff[6] = 0x33;
    s_chassis_can.tx_buff[7] = 0xff;
    CANTransmit(&s_chassis_can, 1U);
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
