/**
 * @file drv_djimotor.h
 * @brief DJI 智能电机驱动（M3508/M2006/GM6020）
 *
 * @note DRV 层职责：
 *       1. 继承 MotorInstance/MotorGroupInstance 基类
 *       2. 实现 DJI 电机特有的数据解析和控制
 *       3. 不使用 FreeRTOS
 *
 * @note 使用方式：
 *       - 单电机模式：DJIMOTOR_INSTANCE_DEF + DJIMotorRegister + MotorControl
 *       - 电机组模式：DJIMOTOR_GROUP_INSTANCE_DEF + DJIMotorGroupRegister + MotorGroupControl
 */

#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "drv_motor_base.h"
#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

/*============================================
 *              常量定义
 *============================================*/

// 编码器角度系数 (360/8192)
#define DJI_ECD_ANGLE_COEF 0.043945f

// RPM 转 rad/s 系数
#define DJI_RPM_TO_RADPS 0.10472f

/*============================================
 *              DJI 电机测量数据结构体
 *============================================*/

typedef struct
{
    uint16_t ecd;             // 编码器值 (0-8191)
    uint16_t last_ecd;        // 上一次编码器值
    float angle_single_round; // 单圈角度 (度)
    float total_angle;        // 总角度 (度)
    int32_t total_round;      // 总圈数
} DJIMotorMeasure_t;

/*============================================
 *              DJI 单电机私有数据
 *============================================*/

typedef struct
{
    CANInstance *rx_can;        // 接收 CAN 实例
    CANInstance *tx_can;        // 发送 CAN 实例（共享）
    DJIMotorMeasure_t measure;  // 测量数据
    uint8_t motor_id;           // 电机 ID (1-8)
    uint8_t tx_slot;            // 发送槽位 (0-3)
    uint32_t feed_cnt;          // 时间戳
} DJIMotorPriv_t;

/*============================================
 *              DJI 电机组私有数据
 *============================================*/

typedef struct
{
    CANInstance *rx_can[4];     // 4 个接收 CAN 实例
    CANInstance *tx_can;        // 发送 CAN 实例
    DJIMotorMeasure_t measure[4]; // 4 个测量数据
    uint8_t motor_id[4];        // 4 个电机 ID
    uint32_t feed_cnt;          // 时间戳
} DJIMotorGroupPriv_t;

/*============================================
 *              DJI 单电机实例结构体
 *============================================*/

typedef struct
{
    MotorInstance base;         // 基类（必须放在首位）
    DJIMotorPriv_t priv;        // 私有数据
} DJIMotorInstance;

/*============================================
 *              DJI 电机组实例结构体
 *============================================*/

typedef struct
{
    MotorGroupInstance base;    // 基类（必须放在首位）
    DJIMotorGroupPriv_t priv;   // 私有数据
} DJIMotorGroupInstance;

/*============================================
 *              虚函数表声明
 *============================================*/

extern const MotorInterface_s djimotor_vtable;
extern const MotorGroupInterface_s djimotor_group_vtable;

/*============================================
 *              单电机实例定义宏
 *============================================*/

/**
 * @brief 定义 DJI 单电机实例
 * @param name       实例名称
 * @param can_idx    板载 CAN 枚举 (CAN_1 / CAN_2)
 * @param mid        电机 ID (1-8)
 * @param mmodel     电机型号 (DJI_MODEL_M3508 / M2006 / GM6020)
 * @param outer_loop 外层闭环类型
 * @param close_loop 启用的闭环组合
 * @param cur_kp/ki/kd 电流环 PID 参数
 * @param spd_kp/ki/kd 速度环 PID 参数
 * @param ang_kp/ki/kd 位置环 PID 参数
 */
#define DJIMOTOR_INSTANCE_DEF(name, can_idx, mid, mmodel, \
                               outer_loop, close_loop, \
                               cur_kp, cur_ki, cur_kd, \
                               spd_kp, spd_ki, spd_kd, \
                               ang_kp, ang_ki, ang_kd) \
    CAN_INSTANCE_DEF_LIST(name##_rx, can_idx, CAN_ID_UNUSED, \
                          ((mmodel) == DJI_MODEL_GM6020 ? (0x204 + (mid)) : (0x200 + (mid))), \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL); \
    static DJIMotorInstance name = { \
        .base = { \
            .vtable = &djimotor_vtable, \
            .brand = MOTOR_BRAND_DJI, \
            .model = mmodel, \
            .settings = { \
                .outer_loop_type = outer_loop, \
                .close_loop_type = close_loop, \
                .motor_reverse = 0, \
                .feedback_reverse = 0, \
                .angle_feedback_src = MOTOR_FEED, \
                .speed_feedback_src = MOTOR_FEED, \
                .feedforward_flag = FEEDFORWARD_NONE, \
            }, \
            .controller = { \
                .angle_feedback_ptr = NULL, \
                .speed_feedback_ptr = NULL, \
                .speed_ff_ptr = NULL, \
                .current_ff_ptr = NULL, \
                .current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd}, \
                .angle_pid = {.kp = ang_kp, .ki = ang_ki, .kd = ang_kd}, \
                .pid_ref = 0.0f, \
            }, \
            .status = {0}, \
            .dt = 0.0f, \
            .priv = NULL, \
        }, \
        .priv = { \
            .rx_can = &name##_rx, \
            .tx_can = NULL, \
            .measure = {0}, \
            .motor_id = mid, \
            .tx_slot = 0, \
            .feed_cnt = 0, \
        }, \
    }

/*============================================
 *              电机组实例定义宏
 *============================================*/

/**
 * @brief 定义 DJI 电机组实例（最多 4 个电机）
 * @param name       实例名称
 * @param can_idx    板载 CAN 枚举
 * @param mmodel     电机型号
 * @param id1-id4    4 个电机 ID (0 表示未使用)
 * @param outer_loop 外层闭环类型
 * @param close_loop 启用的闭环组合
 * @param cur_kp/ki/kd 电流环 PID 参数
 * @param spd_kp/ki/kd 速度环 PID 参数
 */
#define DJIMOTOR_GROUP_INSTANCE_DEF(name, can_idx, mmodel, \
                                     id1, id2, id3, id4, \
                                     outer_loop, close_loop, \
                                     cur_kp, cur_ki, cur_kd, \
                                     spd_kp, spd_ki, spd_kd) \
    CAN_INSTANCE_DEF_LIST(name##_rx1, can_idx, CAN_ID_UNUSED, \
                          (id1) > 0 ? ((mmodel) == DJI_MODEL_GM6020 ? (0x204 + (id1)) : (0x200 + (id1))) : CAN_ID_UNUSED, \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL); \
    CAN_INSTANCE_DEF_LIST(name##_rx2, can_idx, CAN_ID_UNUSED, \
                          (id2) > 0 ? ((mmodel) == DJI_MODEL_GM6020 ? (0x204 + (id2)) : (0x200 + (id2))) : CAN_ID_UNUSED, \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL); \
    CAN_INSTANCE_DEF_LIST(name##_rx3, can_idx, CAN_ID_UNUSED, \
                          (id3) > 0 ? ((mmodel) == DJI_MODEL_GM6020 ? (0x204 + (id3)) : (0x200 + (id3))) : CAN_ID_UNUSED, \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL); \
    CAN_INSTANCE_DEF_LIST(name##_rx4, can_idx, CAN_ID_UNUSED, \
                          (id4) > 0 ? ((mmodel) == DJI_MODEL_GM6020 ? (0x204 + (id4)) : (0x200 + (id4))) : CAN_ID_UNUSED, \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL); \
    static DJIMotorGroupInstance name = { \
        .base = { \
            .vtable = &djimotor_group_vtable, \
            .brand = MOTOR_BRAND_DJI, \
            .model = mmodel, \
            .motor_count = ((id1) > 0) + ((id2) > 0) + ((id3) > 0) + ((id4) > 0), \
            .status = {{0}}, \
            .controller = { \
                [0] = {.current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                       .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd}}, \
                [1] = {.current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                       .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd}}, \
                [2] = {.current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                       .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd}}, \
                [3] = {.current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                       .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd}}, \
            }, \
            .dt = 0.0f, \
            .priv = NULL, \
        }, \
        .priv = { \
            .rx_can = {&name##_rx1, &name##_rx2, &name##_rx3, &name##_rx4}, \
            .tx_can = NULL, \
            .measure = {{0}}, \
            .motor_id = {id1, id2, id3, id4}, \
            .feed_cnt = 0, \
        }, \
    }

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
