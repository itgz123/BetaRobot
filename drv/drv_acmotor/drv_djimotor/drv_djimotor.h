/**
 * @file drv_djimotor.h
 * @brief DJI 智能电机驱动（M3508/M2006/GM6020）
 *
 * @note DRV 层职责：
 *       1. 继承 AcMotorInstance 基类
 *       2. 实现 DJI 电机特有的数据解析和控制
 *       3. 统一电流模式（控制帧只发电流）
 *       4. 不使用 FreeRTOS
 */

#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "drv_acmotor_base.h"
#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

/*============================================
 *              常量定义
 *============================================*/

// 编码器角度系数 (360/8192)
#define DJI_ECD_ANGLE_COEF 0.043945f

// RPM 转 rad/s 系数 (2*PI/60)
#define DJI_RPM_TO_RADPS 0.10472f

// 度转弧度
#define DJI_DEG_TO_RAD 0.017453f

// 电流系数 (20A/16384)
#define DJI_CURRENT_COEF 0.00122f

// 低通滤波系数（越小越平滑）
#define DJI_SPEED_LPF_ALPHA 0.15f
#define DJI_CURRENT_LPF_ALPHA 0.10f

// 减速比
#define DJI_REDUCTION_RATIO_M3508 19.2f
#define DJI_REDUCTION_RATIO_M2006 36.0f
#define DJI_REDUCTION_RATIO_GM6020 1.0f

// 扭矩系数 (N·m/A)
#define DJI_TORQUE_COEF_M3508 0.30f
#define DJI_TORQUE_COEF_M2006 0.18f
#define DJI_TORQUE_COEF_GM6020 0.74f

/*============================================
 *              DJI 单电机私有数据
 *============================================*/

typedef struct
{
    CANInstance *rx_can; // 接收 CAN 实例
    CANInstance *tx_can; // 发送 CAN 实例（共享）
    uint16_t last_ecd;   // 上一次编码器值（多圈计算）
    int32_t total_round; // 总圈数
    uint8_t motor_id;    // 电机 ID (1-8)
    uint8_t tx_slot;     // 发送槽位 (0-3)
    uint64_t feed_cnt;   // DWT 时间戳
} DJIMotorPriv_t;

/*============================================
 *              DJI 单电机实例结构体
 *============================================*/

typedef struct
{
    ACMotorInstance base; // 基类（必须放在首位）
    DJIMotorPriv_t priv;  // 私有数据
} DJIMotorInstance;

/*============================================
 *              虚函数表声明
 *============================================*/

extern const ACMotorInterface_s djimotor_vtable;

/*============================================
 *              单电机实例定义宏
 *============================================*/

/**
 * @brief 定义 DJI 单电机实例（简化版）
 * @param name    实例名称
 * @param can_idx 板载 CAN 枚举 (CAN_1 / CAN_2)
 * @note 初始化通过 ACMotor_Init_Config_s 在运行时完成
 */
#define DJIMOTOR_INSTANCE_DEF(name, can_idx) \
    CAN_INSTANCE_DEF(name##_rx, can_idx);    \
    DAEMON_INSTANCE_DEF(name##_daemon);      \
    static DJIMotorInstance name = {         \
        .base = {                            \
            .vtable = &djimotor_vtable,      \
            .can = &name##_rx,               \
            .daemon = &name##_daemon,        \
        },                                   \
        .priv = {                            \
            .rx_can = &name##_rx,            \
        },                                   \
    }

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H