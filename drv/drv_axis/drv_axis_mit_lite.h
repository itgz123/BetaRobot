/**
 * @file drv_axis_mit_lite.h
 * @brief 轻量级单轴关节控制驱动模块
 * @author TRW
 * @date 2026-06-07
 *
 * @note 控制算法: tau = kp*(ref_pos-pos) + kd*(ref_vel-vel) + tau_gravity + tau_friction + tau_inertia
 * @note 开发阶段:
 *       step1: 固定力矩，计算重力矩，确定控制和反馈方向
 *       step2: 重力基础上添加多频率正弦波（变频），辨识惯量、摩擦，校准重力
 *       step3: 固定频率正弦波位置设定值，kp,kd设置为0，检查辨识效果
 *       step4: 先调kd，再调kp，需要抗外界干扰（kp不能单独非零）
 *       step5: 正常控制
 * @note 5s后开始控制电机
 * @note 这个文件轻量封装，只操作MotorVTable_s中的setref函数，其他的app层手动调用
 * @note drv\drv_axis_lite\identify_axis_mit_lite.py可以辨识参数
 */

#ifndef DRV_AXIS_MIT_LITE_H
#define DRV_AXIS_MIT_LITE_H

#include "drv_djimotor.h"
#include <stdint.h>

/*============================================
 *              控制阶段枚举
 *============================================*/
typedef enum : uint8_t
{
    AXIS_LITE_STAGE_FIXED_TORQUE = 0, // step1: 固定力矩，计算重力矩，确定控制和反馈方向
    AXIS_LITE_STAGE_IDENTIFY,         // step2: 重力基础上添加多频率正弦波（变频），辨识惯量、摩擦，校准重力
    AXIS_LITE_STAGE_TUNE_PD,          // step3,4: 固定频率正弦波位置设定值，kp,kd设置为0，检查辨识效果。先调kd，再调kp，需要抗外界干扰
    AXIS_LITE_STAGE_NORMAL,           // step5: 正常控制
} AxisLiteStage_e;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct AxisLiteInstance
{
    MotorBase_s *motor;
} AxisLiteInstance;

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    MotorBase_s *motor;
} AxisLite_Init_Config_s;

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 注册轴控制实例
 * @param inst 实例指针
 * @param cfg 配置结构体指针
 * @return 0: 成功, -1: 失败
 */
int8_t AxisMitLiteInit(AxisLiteInstance *inst, AxisLite_Init_Config_s *cfg);

/**
 * @brief 计算设定值并调用setref
 * @param inst 实例指针
 */
void AxisMitLiteCalculate(AxisLiteInstance *inst);

#endif // !DRV_AXIS_MIT_LITE_H
