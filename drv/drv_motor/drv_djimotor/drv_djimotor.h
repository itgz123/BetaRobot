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
 *       - 控制函数不声明，通过基类虚函数表调用
 *       - 使用 MotorEnable()、MotorStop()、MotorSetRef() 等基类函数
 *       - 仅声明注册函数 DJIMotorRegister() 和控制发送函数 DJIMotorControl()
 */

#ifndef __DRV_DJIMOTOR_H
#define __DRV_DJIMOTOR_H

#include "drv_motor_base.h"
#include "bsp_can.h"

#ifdef BSP_CAN_MODULE_ENABLED

/*============================================
 *              常量定义
 *============================================*/

#define DJI_MOTOR_MAX_COUNT 12  // 最大电机数量

// 编码器角度系数 (360/8192)
#define DJI_ECD_ANGLE_COEF 0.043945f

// RPM 转 rad/s 系数
#define DJI_RPM_TO_RADPS 0.10472f

/*============================================
 *              DJI 电机测量数据结构体
 *============================================*/

typedef struct
{
    uint16_t ecd;              // 编码器值 (0-8191)
    uint16_t last_ecd;         // 上一次编码器值
    float angle_single_round;  // 单圈角度 (度)
    float total_angle;         // 总角度 (度)
    int32_t total_round;       // 总圈数
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
    MotorInstance base;            // 基类（必须放在首位）

    DJIMotorMeasure_t measure;     // DJI 电机测量数据
    CANInstance *can_inst;         // CAN 实例指针
    uint8_t sender_group;          // 发送分组索引
    uint8_t message_num;           // 组内消息编号
    uint32_t feed_cnt;             // 用于计算 dt 的时间戳
} DJIMotorInstance;

/*============================================
 *              初始化配置结构体
 *============================================*/

/**
 * @brief DJI 电机初始化配置
 */
typedef struct
{
    MotorType_e type;              // 电机类型 (M3508/M2006/GM6020)
    BoardCAN_e can_e;              // 板载 CAN 枚举
    uint8_t motor_id;              // 电机 ID (1-8)

    // 基类配置
    MotorLoopType_e outer_loop_type;
    MotorLoopType_e close_loop_type;
    uint8_t motor_reverse;
    uint8_t feedback_reverse;

    // PID 参数
    float current_kp, current_ki, current_kd;
    float speed_kp, speed_ki, speed_kd;
    float angle_kp, angle_ki, angle_kd;

    // 外部反馈指针（可选）
    float *angle_feedback_ptr;
    float *speed_feedback_ptr;
    float *speed_ff_ptr;
    float *current_ff_ptr;
} DJIMotorInitConfig_t;

/*============================================
 *              必要的函数声明
 *============================================*/

/**
 * @brief 注册 DJI 电机实例
 * @param motor  DJI 电机实例指针（需由调用者分配内存）
 * @param config 初始化配置
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t DJIMotorRegister(DJIMotorInstance *motor, DJIMotorInitConfig_t *config);

/**
 * @brief DJI 电机控制发送（周期调用）
 * @note 遍历所有电机，计算 PID 并填充发送缓冲区
 */
void DJIMotorControl(void);

#endif // BSP_CAN_MODULE_ENABLED

#endif // __DRV_DJIMOTOR_H
