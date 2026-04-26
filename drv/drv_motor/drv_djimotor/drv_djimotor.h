/**
 * @file drv_djimotor.h
 * @brief DJI 智能电机驱动（M3508/M2006/GM6020）
 *
 * @note DRV 层职责：
 *       1. 继承 MotorInstance 基类
 *       2. 实现 DJI 电机特有的数据解析和控制
 *       3. 不使用 FreeRTOS
 *
 * @note 多态设计：
 *       - 控制函数通过基类虚函数表调用
 *       - 使用 MotorEnable()、MotorStop()、MotorSetRef() 等基类函数
 */

#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "drv_motor_base.h"
#include "bsp_can.h"

#ifdef BSP_CAN_MODULE_ENABLED

/*============================================
 *              常量定义
 *============================================*/

#define DJI_MOTOR_MAX_COUNT 12 // 最大电机数量

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
 *              DJI 电机实例结构体
 *============================================*/

/**
 * @brief DJI 电机实例结构体
 * @note 继承 MotorInstance 基类，base 成员放在首位
 */
typedef struct
{
    MotorInstance base; // 基类（必须放在首位）

    DJIMotorMeasure_t measure; // DJI 电机测量数据
    CANInstance *can_inst;     // CAN 实例指针
    uint8_t sender_group;      // 发送分组索引
    uint8_t message_num;       // 组内消息编号
    uint32_t feed_cnt;         // 用于计算 dt 的时间戳
} DJIMotorInstance;

/*============================================
 *              虚函数表声明
 *============================================*/

/**
 * @brief DJI 电机虚函数表（供宏定义使用）
 */
extern const MotorInterface_s djimotor_vtable;

/*============================================
 *              实例定义宏
 *============================================*/

/**
 * @brief 定义 DJI 电机实例
 * @param name       实例名称
 * @param can_idx    板载 CAN 枚举 (CAN_1 / CAN_2)
 * @param motor_id   电机 ID (1-8)
 * @param motor_type 电机类型 (MOTOR_TYPE_DJI_M3508 / M2006 / GM6020)
 * @param outer_loop 外层闭环类型
 * @param close_loop 启用的闭环组合
 * @param cur_kp/ki/kd 电流环 PID 参数
 * @param spd_kp/ki/kd 速度环 PID 参数
 * @param ang_kp/ki/kd 位置环 PID 参数
 *
 * @example 开环模式：
 *   DJIMOTOR_INSTANCE_DEF(motor_fr, CAN_1, 1, MOTOR_TYPE_DJI_M3508,
 *       MOTOR_LOOP_OPEN, MOTOR_LOOP_OPEN,
 *       0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
 *
 * @example 速度环模式：
 *   DJIMOTOR_INSTANCE_DEF(motor_fr, CAN_1, 1, MOTOR_TYPE_DJI_M3508,
 *       MOTOR_LOOP_SPEED, MOTOR_LOOP_SPEED | MOTOR_LOOP_CURRENT,
 *       0.5f, 0.0f, 0.0f, 1.5f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f);
 */
#define DJIMOTOR_INSTANCE_DEF(name, can_idx, motor_id, motor_type,                                               \
                              outer_loop, close_loop,                                                            \
                              cur_kp, cur_ki, cur_kd,                                                            \
                              spd_kp, spd_ki, spd_kd,                                                            \
                              ang_kp, ang_ki, ang_kd)                                                            \
    CAN_INSTANCE_DEF_LIST(name##_can, can_idx, CAN_ID_UNUSED,                                                    \
                          ((motor_type) == MOTOR_TYPE_DJI_GM6020 ? (0x204 + (motor_id)) : (0x200 + (motor_id))), \
                          CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, NULL);                                    \
    static DJIMotorInstance name = {                                                                             \
        .base = {                                                                                                \
            .vtable = &djimotor_vtable,                                                                          \
            .type = motor_type,                                                                                  \
            .settings = {                                                                                        \
                .outer_loop_type = outer_loop,                                                                   \
                .close_loop_type = close_loop,                                                                   \
                .motor_reverse = 0,                                                                              \
                .feedback_reverse = 0,                                                                           \
                .angle_feedback_src = MOTOR_FEED,                                                                \
                .speed_feedback_src = MOTOR_FEED,                                                                \
                .feedforward_flag = FEEDFORWARD_NONE,                                                            \
            },                                                                                                   \
            .controller = {                                                                                      \
                .angle_feedback_ptr = NULL,                                                                      \
                .speed_feedback_ptr = NULL,                                                                      \
                .speed_ff_ptr = NULL,                                                                            \
                .current_ff_ptr = NULL,                                                                          \
                .current_pid = {.kp = cur_kp, .ki = cur_ki, .kd = cur_kd},                                       \
                .speed_pid = {.kp = spd_kp, .ki = spd_ki, .kd = spd_kd},                                         \
                .angle_pid = {.kp = ang_kp, .ki = ang_ki, .kd = ang_kd},                                         \
                .pid_ref = 0.0f,                                                                                 \
            },                                                                                                   \
            .status = {0},                                                                                       \
            .dt = 0.0f,                                                                                          \
        },                                                                                                       \
        .measure = {0},                                                                                          \
        .can_inst = &name##_can,                                                                                 \
        .sender_group = 0,                                                                                       \
        .message_num = 0,                                                                                        \
        .feed_cnt = 0,                                                                                           \
    }

/*============================================
 *              接口函数
 *============================================*/

/**
 * @brief 注册 DJI 电机实例
 * @param inst DJI 电机实例指针（需先通过宏定义）
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t DJIMotorRegister(DJIMotorInstance *inst);

/**
 * @brief DJI 电机控制发送（周期调用）
 * @note 遍历所有电机，计算 PID 并填充发送缓冲区
 */
void DJIMotorControl(void);

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
