/**
 * @file bsp_math_angle.h
 * @brief 角度处理函数
 *
 * @note 本文件提供角度差计算、角度归一化等角度处理功能
 * @note 所有角度单位均为弧度
 */

#ifndef __BSP_MATH_ANGLE_H
#define __BSP_MATH_ANGLE_H

#include "bsp_math_const.h"
#include <math.h>
#include <stdint.h>

/*============================================
 *              角度处理函数
 *============================================*/

/**
 * @brief 计算两个角度的最短角度差
 * @param from 起始角度（弧度）
 * @param to 目标角度（弧度）
 * @return 角度差（弧度），范围 (-π, π]
 * @note 用于计算多圈位置累加、最短路径运动等
 */
static inline float BSP_Math_AngleDiff(float from, float to)
{
    float diff = to - from;
    if (diff > M_PI)
    {
        int32_t n = (int32_t)((diff + M_PI) / M_2PI);
        diff -= n * M_2PI;
    }
    else if (diff <= -M_PI)
    {
        int32_t n = (int32_t)((-diff + M_PI) / M_2PI);
        diff += n * M_2PI;
    }
    return diff;
}

/**
 * @brief 将角度归一化到 [0, 2π) 范围
 * @param angle 输入角度（弧度），支持多圈
 * @return 归一化后的角度（弧度），范围 [0, 2π)
 * @note 例：3.4π → 1.4π，-0.3π → 1.7π
 */
static inline float BSP_Math_WrapAngle0To2PI(float angle)
{
    angle = fmodf(angle, M_2PI);
    if (angle < 0.0f)
    {
        angle += M_2PI;
    }
    return angle;
}

/**
 * @brief 将角度归一化到 (-π, π] 范围
 * @param angle 输入角度（弧度），支持多圈
 * @return 归一化后的角度（弧度），范围 (-π, π]
 * @note 例：3.4π → -0.6π，-2.3π → -0.3π，π → -π，-π → π
 */
static inline float BSP_Math_WrapAngleNegPIToPI(float angle)
{
    angle = fmodf(angle, M_2PI);
    if (angle >= M_PI)
    {
        angle -= M_2PI;
    }
    else if (angle < -M_PI)
    {
        angle += M_2PI;
    }
    return angle;
}

/**
 * @brief 将角度归一化到指定范围 [min_val, max_val)
 * @param angle 输入角度（弧度），支持多圈
 * @param min_val 范围下限
 * @param max_val 范围上限
 * @return 归一化后的角度（弧度），范围 [min_val, max_val)
 * @note 例：WrapAngle(3.4π, 0, π) → 0.4π，WrapAngle(-2.3π, 0, π) → 0.7π
 */
static inline float BSP_Math_WrapAngle(float angle, float min_val, float max_val)
{
    float range = max_val - min_val;
    if (range <= 0.0f)
    {
        return angle;
    }
    angle = fmodf(angle - min_val, range);
    if (angle < 0.0f)
    {
        angle += range;
    }
    return angle + min_val;
}

#endif /* __BSP_MATH_ANGLE_H */
