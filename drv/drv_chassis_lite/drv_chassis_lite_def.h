/**
 * @file drv_chassis_lite_def.h
 * @brief chassis lite 层公共类型（运动学正逆解的输入/输出）
 * @author TRW
 * @date 2026-09-13
 *
 * @note 命名维度：drv_chassis_lite_<position/force>_<type>
 *       - position（位控，现有）：输出速度 / 目标舵角，见 ChassisLitePositionRef_t
 *       - force（力控，预留）：输出力矩（按整车惯量换算），届时新增 ChassisLiteForceRef_t 等
 *
 * @note chassis lite 模块统一约定：只做运动学计算，不持有电机、不调用 Motor* 接口，也不含任何控制闭环。
 *       逆解把结果写进 ChassisLitePositionRef_t，由 app 层自行 MotorSetRef 下发；
 *       正解所需反馈同样由 app 层读好后传入。
 * @note 参考《运控相关.md》：1. 归一化  2. 不依赖 motor，只计算
 *
 * @note 输出语义：
 *       - 驱动轮 → 角速度 [rad/s]
 *       - 舵向   → 目标舵角 [rad]（取离当前舵角最近的等价方向；位置环/积分由 app 负责）
 */

#ifndef __DRV_CHASSIS_LITE_DEF_H
#define __DRV_CHASSIS_LITE_DEF_H

#include <stdint.h>
#include "drv_chassis_def.h"
#include "lib_math.h"

// 舵轮回转中心速度低于此阈值 [m/s] 时认为"不需要转向"：保持当前朝向、驱动置 0
#define CHASSIS_LITE_STEER_SPEED_EPS 1e-4f

/*============================================
 *              输入：底盘状态（反馈）
 *============================================*/
/**
 * @brief 底盘运动学正/逆解所需的反馈量
 * @note 轮系无关的公共状态；非舵轮底盘忽略对应字段
 */
typedef struct
{
    // 当前舵角 [rad]，输出侧（关节侧）、连续值（不 wrap）。
    // 只作为"当前轮子朝哪"这一事实，供 lite 层在 ±180° 两个等价解中选最近的那个；
    // lite 层不据此产生任何误差驱动的控制量（不闭环）。
    float steer_angle[WHEEL_NUM];
} ChassisLiteState_t;

/*============================================
 *              输出：运动学解算结果
 *============================================*/
/**
 * @brief 底盘运动学解算结果
 * @note 非舵轮底盘 steer_target 不使用（保持 0）
 */
typedef struct
{
    // 目标舵角 [rad]，输出侧、连续值（不 wrap）——"离当前舵角最近的等价方向"。
    // 方向决策（含 180° 最短路径与驱动轮换向）由 lite 层给出；
    // 位置闭环（含积分）由 app 负责。
    float steer_target[WHEEL_NUM];

    // 目标驱动速度 [rad/s]，电机侧（已含减速比与轮径换算）；负值表示反转
    float drive_speed[WHEEL_NUM];

    uint8_t valid; // 0=本次解算无效（指令未使能/参数不合法），app 应忽略本次输出
} ChassisLitePositionRef_t;

/*============================================
 *              公共纯函数（舵轮归一化）
 *============================================*/

/**
 * @brief  在 ±180° 两个等价轮向中，选离当前舵角最近的解（"最近方向"决策）
 * @param  phi_des 期望轮向 [rad]，任意值（atan2 结果）
 * @param  phi_cur 当前轮向 [rad]，连续值（不 wrap）
 * @param  reverse 输出：1=选了 +180° 的等价解，驱动轮需反转；0=不反转。可为 NULL
 * @return 目标轮向 [rad]，连续值 = phi_cur + 最近角差
 *
 * @note 只做几何选择，不产生任何误差驱动的控制量（不闭环）；
 *       舵向位置环（含积分）与驱动换向的执行由 app 负责
 * @note 轮子指向 φ 与 φ+180° 并反转驱动是同一运动的两种表示，
 *       选离当前更近的那个可让转向行程最小（|角差| ≤ 90°）
 */
static inline float ChassisLiteSteerNearest180(float phi_des, float phi_cur, int8_t *reverse)
{
    float e = Lib_Math_WrapAngleNegPIToPI(phi_des - phi_cur); // (-π, π]
    int8_t rev = 0;

    if (FABS(e) > M_PI_2)
    {
        e -= (e > 0.0f) ? M_PI : -M_PI;
        rev = 1;
    }

    if (reverse != NULL)
        *reverse = rev;

    return phi_cur + e;
}

#endif // __DRV_CHASSIS_LITE_DEF_H
