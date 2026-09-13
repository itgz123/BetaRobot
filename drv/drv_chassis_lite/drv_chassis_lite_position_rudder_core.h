/**
 * @file drv_chassis_lite_position_rudder_core.h
 * @brief 舵轮运动学公共内核（半舵/全舵共用，纯计算）
 * @author TRW
 * @date 2026-09-13
 *
 * @note 本文件是 drv_chassis_lite_position_half_rudder / drv_chassis_lite_position_all_rudder 的**内部**公共实现，
 *       两者每组舵轮的公式完全一致，只差组数与回转中心位置，故抽到此文件避免重复。
 *       不对外作为模块使用。
 */

#ifndef __DRV_CHASSIS_LITE_POSITION_RUDDER_CORE_H
#define __DRV_CHASSIS_LITE_POSITION_RUDDER_CORE_H

#include "drv_chassis_lite_def.h"

/*============================================
 *              逆解内核
 *============================================*/
/**
 * @brief  n 组舵轮逆解：底盘速度指令 → 各组 {目标舵角, 驱动速度}
 * @param  n              舵轮组数
 * @param  pos_x/pos_y    各组回转中心位置 [m]（车体系，前+/左+）
 * @param  steer_angle    各组当前舵角 [rad]（车体系连续角）
 * @param  out            结果写到 steer_target[0..n-1] / drive_speed[0..n-1]
 * @note   回转中心速度低于阈值时该组保持当前朝向、驱动置 0
 */
static inline void ChassisLitePositionRudderInverseCore(uint8_t n, const float *pos_x, const float *pos_y,
                                                        float wheel_radius, float reduction_ratio,
                                                        ChassisCmd_t cmd, const float *steer_angle,
                                                        ChassisLitePositionRef_t *out)
{
    const float k = reduction_ratio / wheel_radius;

    for (uint8_t i = 0; i < n; i++)
    {
        /* 组 i 回转中心的期望速度：v_i = v_center + w × r_i */
        float vx_i = cmd.vx - cmd.w * pos_y[i];
        float vy_i = cmd.vy + cmd.w * pos_x[i];
        float speed = Lib_Math_Sqrt(vx_i * vx_i + vy_i * vy_i);
        float phi_cur = steer_angle[i];

        if (speed < CHASSIS_LITE_STEER_SPEED_EPS)
        {
            /* 静止：不改朝向，驱动停转 */
            out->steer_target[i] = phi_cur;
            out->drive_speed[i] = 0.0f;
            continue;
        }

        int8_t reverse = 0;
        float phi_des = Lib_Math_Atan2(vy_i, vx_i);
        out->steer_target[i] = ChassisLiteSteerNearest180(phi_des, phi_cur, &reverse);
        out->drive_speed[i] = (reverse ? -speed : speed) * k;
    }
}

/*============================================
 *              正解内核（里程计）
 *============================================*/
/**
 * @brief  n 组舵轮正解：各组 {驱动速度, 当前舵角} → 底盘速度（最小二乘）
 * @param  drive_speed 各组驱动速度 [rad/s]（电机侧，带符号）
 * @param  steer_angle 各组当前舵角 [rad]（车体系连续角）
 * @return 估算的底盘速度 (vx, vy, w)
 *
 * @note   每组给出接触点速度 v_i = s_i·(cosφ_i, sinφ_i)，与运动学 v_i =(vx - w·y_i, vy + w·x_i)
 *         构成 2n 个方程、3 个未知量（超定），取最小二乘解。
 *         法方程为 3x3，且首行/次行结构简单，det = n·(n·Σr² - (Σx)² - (Σy)²)。
 * @note   位置退化（回转中心重合/共线）时返回 0
 */
static inline ChassisCmd_t ChassisLitePositionRudderForwardCore(uint8_t n, const float *pos_x, const float *pos_y,
                                                                float wheel_radius, float reduction_ratio,
                                                                const float *drive_speed, const float *steer_angle)
{
    ChassisCmd_t cmd = {0};

    float Sx = 0.0f, Sy = 0.0f, Srr = 0.0f; /* Σx, Σy, Σ(x²+y²) */
    float Sc = 0.0f, Sd = 0.0f, Sb = 0.0f;  /* Σc, Σd, Σ(d·x - c·y) */
    const float kr = wheel_radius / reduction_ratio;

    for (uint8_t i = 0; i < n; i++)
    {
        float s = drive_speed[i] * kr; /* 接触点线速度 [m/s]，带符号 */
        float c = s * Lib_Math_Cos(steer_angle[i]);
        float d = s * Lib_Math_Sin(steer_angle[i]);

        Sx += pos_x[i];
        Sy += pos_y[i];
        Srr += pos_x[i] * pos_x[i] + pos_y[i] * pos_y[i];
        Sc += c;
        Sd += d;
        Sb += d * pos_x[i] - c * pos_y[i];
    }

    float N = (float)n;
    float det = N * (N * Srr - Sx * Sx - Sy * Sy);
    if (FABS(det) < 1e-9f)
        return cmd; /* 位置退化 */

    cmd.vx = (Sc * (N * Srr - Sx * Sx) - Sy * Sd * Sx + N * Sy * Sb) / det;
    cmd.vy = (N * (Sd * Srr - Sx * Sb) - Sc * Sx * Sy - Sd * Sy * Sy) / det;
    cmd.w = (N * Sb - Sd * Sx + Sc * Sy) / (N * Srr - Sx * Sx - Sy * Sy);
    cmd.enable = 0;
    return cmd;
}

#endif // __DRV_CHASSIS_LITE_POSITION_RUDDER_CORE_H
