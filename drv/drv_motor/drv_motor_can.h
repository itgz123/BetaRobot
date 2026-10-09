/**
 * @file drv_motor_can.h
 * @brief 电机驱动的 CAN 发送统一入口（失败计数）
 *
 * @note **为什么单独一个头文件**：`drvs_motor/` 是刻意不依赖 `drv_motor_base.h` 的独立驱动集合
 *       （见 drvs_dmmotor.h 的说明），而本 helper 只依赖 `bsp_can.h`。
 *       放进 drv_motor_base.h 就会把 `MotorBase_s` / `drv_pid` / `drv_daemon` 一块拖进 drvs，
 *       为 10 行代码付那个代价不值。
 *
 * @note 本文件不含"发送队列/重试"：电机控制帧是周期性的，丢一帧下一周期自然补上，
 *       在这里阻塞重试会把控制周期拖乱（`drv_pid` 的稳态假设是固定周期）。
 */

#ifndef DRV_MOTOR_CAN_H
#define DRV_MOTOR_CAN_H

#include "stdint.h"
#include "bsp_can.h"

/**
 * @brief 电机 CAN 发送统一入口：转发 CANTransmit，并把失败累加到调用方的计数器
 * @param can        CAN 实例（非空由调用方保证，各驱动的发送函数入口都已判过）
 * @param pack       待发送的数据包
 * @param timeout_ms 发送资源等待超时（ms），语义见 CANTransmit
 * @param tx_fail    出参：失败时自增的计数器；可传 NULL（= 调用方没有可归属的计数器）
 * @return CANTransmit 的状态码原样返回（BSP_OK / BSP_BUSY / BSP_TIMEOUT / BSP_PARAM_ERR / BSP_HW_ERR）
 *
 * @note **为什么要有这个口**：改造前 19 个发送点全部忽略返回值，一帧发不出去
 *       （总线断开 / 邮箱·Tx FIFO 占满 / 实例没 Config 成功）在代码里**没有任何痕迹**，
 *       只能靠"看电机不动"反推。统一走这里后，任何驱动忘了看返回值，至少计数是有的。
 *
 * @note **只计数、不打日志**：总线断开时每帧都会失败，按控制频率（数百 Hz~1kHz）打日志
 *       会把 log 环冲垮，还把控制任务的时间吃掉。计数放状态结构体里，调试器 Watch 或
 *       上层按低频率读，这才是能用的观测通道。
 *
 * @note **组播帧记在"组"上，不是记在电机上**：DJI / LK 的广播帧一帧带 4 个电机
 *       （`0x1FF`/`0x200`/`0x280`），它不属于任何单个电机实例，失败摊到哪个电机都不对，
 *       所以那两个调用点传的是组自己的计数器（`DJIMotorBroadcastSendGroup_s.tx_fail` /
 *       `LKMotorBroadcastSendGroup_s.tx_fail`，见 drv_djimotor/drv_djimotor_broadcast.c:691、
 *       drv_lkmotor/drv_lkmotor_broadcast.c:614）。
 *       `tx_fail == NULL` 只留给"确实没有归属计数器"的调用方，当前没有这样的调用点；
 *       bsp 层的 per-CAN 计数（`s_bxcan_status[].tx_fail` / `s_fdcan_status[].tx_fail`）
 *       是另一条独立的观测通道，不替代上面这个归属。
 */
static inline BSP_Status_e MotorCanTransmit(CANInstance *can, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_fail)
{
    BSP_Status_e status = CANTransmit(can, pack, timeout_ms, NULL, NULL);

    if (status != BSP_OK && tx_fail != NULL)
    {
        (*tx_fail)++;
    }

    return status;
}

#endif /* DRV_MOTOR_CAN_H */
