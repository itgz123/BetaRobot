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

#ifdef BSP_CAN_MODULE_ENABLED
CAN_INSTANCE_DEF(s_chassis_can, CAN_1, 0x701, 0x711, NULL);
static uint32_t s_can_tx_seq = 0;
static uint32_t s_can_tx_fail_cnt = 0;
#endif

static void Init(void)
{
#ifdef BSP_CAN_MODULE_ENABLED
    if (CANRegister(&s_chassis_can) != 0)
    {
        LOGERROR("[app_chassis] CAN register failed");
        return;
    }
    CANSetDLC(&s_chassis_can, 8);
    LOGINFO("[app_chassis] CAN ready tx=0x%03lX rx=0x%03lX", s_chassis_can.tx_id, s_chassis_can.rx_id);
#endif
}

ITCM_RAM static void Run(void)
{
#ifdef BSP_CAN_MODULE_ENABLED
    uint32_t seq = s_can_tx_seq++;
    s_chassis_can.tx_buff[0] = (uint8_t)(seq & 0xFFU);
    s_chassis_can.tx_buff[1] = (uint8_t)((seq >> 8) & 0xFFU);
    s_chassis_can.tx_buff[2] = (uint8_t)((seq >> 16) & 0xFFU);
    s_chassis_can.tx_buff[3] = (uint8_t)((seq >> 24) & 0xFFU);
    s_chassis_can.tx_buff[4] = 0x43U; // 'C'
    s_chassis_can.tx_buff[5] = 0x48U; // 'H'
    s_chassis_can.tx_buff[6] = 0x53U; // 'S'
    s_chassis_can.tx_buff[7] = 0x38U; // '8'

    if (!CANTransmit(&s_chassis_can, 1U))
    {
        s_can_tx_fail_cnt++;
        if ((s_can_tx_fail_cnt % 200U) == 0U)
        {
            LOGWARNING("[app_chassis] CAN transmit failed cnt=%lu", s_can_tx_fail_cnt);
        }
    }
#endif
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
