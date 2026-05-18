/**
 * @file drv_dmmotor.h
 * @brief 达妙 DM4310 电机驱动
 *
 * @note DRV 层职责：
 *       1. 继承 AcMotorInstance 基类
 *       2. 实现 DM 电机 CAN 反馈解析
 *       3. 统一电流模式（MIT 帧只发 t_ff）
 *       4. 双温度支持、离线恢复、减速比 10:1
 *       5. 不使用 FreeRTOS
 */

#ifndef __DRV_DMMOTOR_H
#define __DRV_DMMOTOR_H

#include "drv_acmotor_base.h"
#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

/*============================================
 *              常量定义
 *============================================*/

// MIT 协议映射范围
#define DM_P_MIN -95.5f
#define DM_P_MAX 95.5f
#define DM_V_MIN -45.0f
#define DM_V_MAX 45.0f
#define DM_T_MIN -18.0f
#define DM_T_MAX 18.0f

// 减速比
#define DM_REDUCTION_RATIO 10.0f

// 扭矩系数 (N·m/A)，DM 直接输出力矩故设为 1.0
#define DM_TORQUE_COEF 1.0f

// daemon 超时（1kHz 下 10 个周期 = 10ms）
#define DM_DAEMON_RELOAD 10

/*============================================
 *              DM 电机命令枚举
 *============================================*/

typedef enum
{
    DM_CMD_MOTOR_MODE = 0xFC,    // 使能
    DM_CMD_RESET_MODE = 0xFD,    // 失能/停止
    DM_CMD_ZERO_POSITION = 0xFE, // 保存零点
    DM_CMD_CLEAR_ERROR = 0xFB,   // 清除错误
} DMMotorCmd_e;

/*============================================
 *              DM 电机状态枚举
 *============================================*/

typedef enum
{
    DM_ERR_ENABLE = 0x01,         // 使能（正常）
    DM_ERR_DISABLE = 0x00,        // 失能
    DM_ERR_OVER_VOLTAGE = 0x08,   // 超压
    DM_ERR_UNDER_VOLTAGE = 0x09,  // 欠压
    DM_ERR_OVER_CURRENT = 0x0A,   // 过流
    DM_ERR_MOS_OVER_TEMP = 0x0B,  // MOS 过温
    DM_ERR_COIL_OVER_TEMP = 0x0C, // 线圈过温
    DM_ERR_LOST_COMM = 0x0D,      // 通讯丢失
    DM_ERR_OVERLOAD = 0x0E,       // 过载
} DMMotorState_e;

/*============================================
 *              DM 单电机私有数据
 *============================================*/

typedef struct
{
    CANInstance *can_inst; // CAN 实例（收发共用）
    uint16_t last_ecd;     // 上一次编码器值（多圈计算）
    int32_t total_round;   // 总圈数
    uint64_t feed_cnt;     // DWT 时间戳
    uint8_t was_lost;      // 曾离线标志
    uint16_t lost_cnt;     // 离线计数（控制恢复间隔）
} DMMotorPriv_t;

/*============================================
 *              DM 单电机实例结构体
 *============================================*/

typedef struct
{
    ACMotorInstance base; // 基类（必须放在首位）
    DMMotorPriv_t priv;   // 私有数据
} DMMotorInstance;

/*============================================
 *              虚函数表声明
 *============================================*/

extern const ACMotorInterface_s dmmotor_vtable;

/*============================================
 *              单电机实例定义宏
 *============================================*/

/**
 * @brief 定义 DM 单电机实例
 * @param name       实例名称
 * @param can_idx    板载 CAN 枚举 (CAN_1 / CAN_2)
 * @param rx_id      电机反馈 CAN ID
 * @param tx_id      电机控制 CAN ID
 * @param outer_loop 外层闭环类型
 * @param close_loop 启用的闭环组合
 * @param cur_kp/ki/kd 电流环 PID 参数
 * @param spd_kp/ki/kd 速度环 PID 参数
 * @param ang_kp/ki/kd 位置环 PID 参数
 */
#define DMMOTOR_INSTANCE_DEF(name, can_idx, rx_id, tx_id,                  \
                             outer_loop, close_loop,                       \
                             cur_kp, cur_ki, cur_kd,                       \
                             spd_kp, spd_ki, spd_kd,                       \
                             ang_kp, ang_ki, ang_kd)                       \
    CAN_INSTANCE_DEF(name##_can, can_idx);                                 \
    DAEMON_INSTANCE_DEF(name##_daemon);                                    \
    static DMMotorInstance name = {                                        \
        .base = {                                                          \
            .vtable = &dmmotor_vtable,                                     \
            .brand = MOTOR_BRAND_DM,                                       \
            .model = DM_MODEL_4310,                                        \
            .can = &name##_can,                                            \
            .raw_data = {0},                                               \
            .data = {0},                                                   \
            .settings = {                                                  \
                .outer_loop_type = outer_loop,                             \
                .close_loop_type = close_loop,                             \
                .motor_reverse = 0,                                        \
                .feedback_reverse = 0,                                     \
                .angle_feedback_src = MOTOR_FEED,                          \
                .speed_feedback_src = MOTOR_FEED,                          \
                .feedforward_flag = FEEDFORWARD_NONE,                      \
            },                                                             \
            .controller = {                                                \
                .angle_feedback_ptr = NULL,                                \
                .speed_feedback_ptr = NULL,                                \
                .speed_ff_ptr = NULL,                                      \
                .current_ff_ptr = NULL,                                    \
                .current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd}, \
                .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd},   \
                .angle_pid = {.kp = ang_kp, .ki = ang_ki, .kd = ang_kd},   \
                .pid_ref = 0.0f,                                           \
            },                                                             \
            .limits = {                                                    \
                .position_min = -1e6f,                                     \
                .position_max = 1e6f,                                      \
                .speed_max = 1e6f,                                         \
                .current_max = DM_T_MAX,                                   \
                .position_limit_enable = 0,                                \
            },                                                             \
            .daemon = &name##_daemon,                                      \
            .reduction_ratio = DM_REDUCTION_RATIO,                         \
            .torque_coef = DM_TORQUE_COEF,                                 \
            .enable = 0,                                                   \
            .dt = 0.0f,                                                    \
            .priv = NULL,                                                  \
        },                                                                 \
        .priv = {                                                          \
            .can_inst = &name##_can,                                       \
            .last_ecd = 0,                                                 \
            .total_round = 0,                                              \
            .feed_cnt = 0,                                                 \
            .was_lost = 0,                                                 \
            .lost_cnt = 0,                                                 \
        },                                                                 \
    }

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DMMOTOR_H
