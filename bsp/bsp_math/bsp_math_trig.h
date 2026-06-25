/**
 * @file bsp_math_trig.h
 * @brief 三角函数和数学函数封装
 *
 * @note 本文件封装三角函数、平方根、反余弦等数学函数
 * @note 两层回退机制：
 *       1. CMSIS-DSP 库 — 有 DSP 指令集时使用查表/优化实现
 *       2. 标准库 math.h — 兜底实现
 *
 * @note 硬件特性由 bsp_map.h 中的 HAS_DSP 宏控制
 */

#ifndef __BSP_MATH_TRIG_H
#define __BSP_MATH_TRIG_H

#include "bsp_map.h"
#include <stdint.h>

#if HAS_DSP
#include "arm_math.h"
#else
#include <math.h>
#endif

/*============================================
 *              三角函数接口
 *============================================*/

/**
 * @brief 计算 sin(theta)
 * @param theta 角度（弧度）
 * @return sin(theta)
 * @note 优先级：CMSIS-DSP 查表 > 标准库
 */
static inline float BSP_Math_Sin(float theta)
{
#if HAS_DSP
    return arm_sin_f32(theta);
#else
    return sinf(theta);
#endif
}

/**
 * @brief 计算 cos(theta)
 * @param theta 角度（弧度）
 * @return cos(theta)
 * @note 优先级：CMSIS-DSP 查表 > 标准库
 */
static inline float BSP_Math_Cos(float theta)
{
#if HAS_DSP
    return arm_cos_f32(theta);
#else
    return cosf(theta);
#endif
}

/**
 * @brief 同时计算 sin 和 cos
 * @param theta 角度（弧度）
 * @param[out] p_sin sin(theta) 输出
 * @param[out] p_cos cos(theta) 输出
 * @note 优先级：CMSIS-DSP 查表 > 标准库
 */
static inline void BSP_Math_SinCos(float theta, float *p_sin, float *p_cos)
{
    if (p_sin == NULL || p_cos == NULL)
    {
        return;
    }
#if HAS_DSP
    *p_sin = arm_sin_f32(theta);
    *p_cos = arm_cos_f32(theta);
#else
    *p_sin = sinf(theta);
    *p_cos = cosf(theta);
#endif
}

/**
 * @brief 计算 atan2(y, x)
 * @param y Y 坐标
 * @param x X 坐标
 * @return atan2(y, x)（弧度）
 * @note 优先级：CMSIS-DSP (V1.9.0+) > 标准库
 * @note arm_atan2_f32 仅在 CMSIS-DSP V1.9.0+ 版本中可用
 *       DM_MC02 使用 V1.6.0，不支持 arm_atan2_f32，回退到标准库
 */
static inline float BSP_Math_Atan2(float y, float x)
{
#if HAS_DSP && (DEVELOPMENT_BOARD != DM_MC02)
    float result;
    arm_atan2_f32(y, x, &result);
    return result;
#else
    return atan2f(y, x);
#endif
}

/**
 * @brief 计算 sqrt(x)
 * @param x 输入值（必须 >= 0）
 * @return sqrt(x)
 * @note 优先级：CMSIS-DSP（VSQRT 指令）> 标准库
 */
static inline float BSP_Math_Sqrt(float x)
{
    if (x < 0.0f)
    {
        return 0.0f;
    }
#if HAS_DSP
    float result;
    arm_sqrt_f32(x, &result);
    return result;
#else
    return sqrtf(x);
#endif
}

/**
 * @brief 计算 acos(x)
 * @param x 输入值（范围 [-1, 1]）
 * @return acos(x)（弧度），范围 [0, π]
 * @note 使用标准库实现
 */
static inline float BSP_Math_Acos(float x)
{
    if (x > 1.0f)
        x = 1.0f;
    if (x < -1.0f)
        x = -1.0f;
    return acosf(x);
}

#endif /* __BSP_MATH_TRIG_H */
