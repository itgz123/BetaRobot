/**
 * @file bsp_math_trig.h
 * @brief 三角函数和数学函数封装
 *
 * @note 本文件封装三角函数、平方根、反余弦等数学函数
 * @note 全部使用 C 标准库 math.h（libm）实现，不依赖 CMSIS-DSP；
 *       若 app_cfg.h 定义了 BSP_MATH_TRIG_LUT_USED，
 *       BSP_Math_Sin/Cos/SinCos 三个三角函数改用自研查表法
 *       （BSP_Math_SinLUT/CosLUT/SinCosLUT，见 bsp_math_trig_lut.h）。
 *
 * @note 依赖：
 *       - <math.h>：标准库三角函数
 *       - app_cfg.h：仅用于读取 BSP_MATH_TRIG_LUT_USED 开关
 *       - bsp_math_trig_lut.h：查表接口（供 LUT 分支调用）
 */

#ifndef __BSP_MATH_TRIG_H
#define __BSP_MATH_TRIG_H

#include <stdint.h>
#include <math.h>

/* 配置入口（与 bsp_math_trig_lut.h 一致）：
 *   固件编译——include app_cfg.h，读取 BSP_MATH_TRIG_LUT_USED；
 *   PC 独立检验——定义 BSP_MATH_TRIG_LUT_STANDALONE 跳过 app_cfg.h
 *   （PC 端不引入工程配置），命令行直接 -D BSP_MATH_TRIG_LUT_USED 显式开启。 */
#ifndef BSP_MATH_TRIG_LUT_STANDALONE
#include "app_cfg.h"
#endif

/* 查表接口须在本文件分发函数之前可见（bsp_math_matrix.h / vector.h /
 * transform.h / quat.h 等子模块直接 include 本头，也能解析 BSP_Math_*LUT） */
#include "bsp_math_trig_lut.h"

/*============================================
 *              三角函数接口
 *============================================*/

/**
 * @brief 计算 sin(theta)
 * @param theta 角度（弧度）
 * @return sin(theta)
 * @note 定义 BSP_MATH_TRIG_LUT_USED 时走查表法（BSP_Math_SinLUT），
 *       否则走标准库 sinf
 */
static inline float BSP_Math_Sin(float theta)
{
#ifdef BSP_MATH_TRIG_LUT_USED
    return BSP_Math_SinLUT(theta);
#else
    return sinf(theta);
#endif
}

/**
 * @brief 计算 cos(theta)
 * @param theta 角度（弧度）
 * @return cos(theta)
 * @note 定义 BSP_MATH_TRIG_LUT_USED 时走查表法（BSP_Math_CosLUT），
 *       否则走标准库 cosf
 */
static inline float BSP_Math_Cos(float theta)
{
#ifdef BSP_MATH_TRIG_LUT_USED
    return BSP_Math_CosLUT(theta);
#else
    return cosf(theta);
#endif
}

/**
 * @brief 同时计算 sin 和 cos
 * @param theta 角度（弧度）
 * @param[out] p_sin sin(theta) 输出
 * @param[out] p_cos cos(theta) 输出
 * @note 定义 BSP_MATH_TRIG_LUT_USED 时走查表法（BSP_Math_SinCosLUT），
 *       否则走标准库 sinf/cosf
 */
static inline void BSP_Math_SinCos(float theta, float *p_sin, float *p_cos)
{
    if (p_sin == NULL || p_cos == NULL)
    {
        return;
    }
#ifdef BSP_MATH_TRIG_LUT_USED
    BSP_Math_SinCosLUT(theta, p_sin, p_cos);
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
 * @note 标准库 atan2f
 */
static inline float BSP_Math_Atan2(float y, float x)
{
    return atan2f(y, x);
}

/**
 * @brief 计算 sqrt(x)
 * @param x 输入值（必须 >= 0）
 * @return sqrt(x)
 * @note 标准库 sqrtf；x<0 时返回 0
 */
static inline float BSP_Math_Sqrt(float x)
{
    if (x < 0.0f)
    {
        return 0.0f;
    }
    return sqrtf(x);
}

/**
 * @brief 计算 acos(x)
 * @param x 输入值（范围 [-1, 1]）
 * @return acos(x)（弧度），范围 [0, π]
 * @note 标准库 acosf
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
