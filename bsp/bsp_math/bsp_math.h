/**
 * @file bsp_math.h
 * @brief BSP层数学函数封装：统一数学常量、宏定义、函数接口
 *
 * @note 本文件职责：
 *       1. 定义数学常量（PI、E、根号二、根号三等）
 *       2. 定义常用数学宏（绝对值、平方、角度弧度转换等）
 *       3. 提供数学函数的统一接口
 *
 * @note 两层回退机制：
 *       1. CMSIS-DSP库 - 有DSP指令集时使用查表/优化实现
 *       2. 标准库math.h - 兜底实现
 *
 * @note 硬件特性由 map 文件中的宏定义控制：
 *       - HAS_FPU:     浮点运算单元
 *       - HAS_DSP:     DSP指令集
 */

#ifndef __BSP_MATH_H
#define __BSP_MATH_H

#include "bsp.h"
#include <stdint.h>

/*============================================
 *              条件包含
 *============================================*/

#if HAS_DSP
#include "arm_math.h"
#else
#include <math.h>
#endif

/*============================================
 *              三角函数接口（inline 实现）
 *============================================*/

/**
 * @brief 计算sin(theta)
 * @param theta 角度（弧度）
 * @return sin(theta)
 * @note 优先级：CMSIS-DSP查表 > 标准库
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
 * @brief 计算cos(theta)
 * @param theta 角度（弧度）
 * @return cos(theta)
 * @note 优先级：CMSIS-DSP查表 > 标准库
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
 * @brief 同时计算sin和cos
 * @param theta 角度（弧度）
 * @param[out] p_sin sin(theta)输出
 * @param[out] p_cos cos(theta)输出
 * @note 优先级：CMSIS-DSP查表 > 标准库
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
 * @brief 计算atan2(y, x)
 * @param y Y坐标
 * @param x X坐标
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
 * @brief 计算sqrt(x)
 * @param x 输入值（必须>=0）
 * @return sqrt(x)
 * @note 优先级：CMSIS-DSP（VSQRT指令） > 标准库
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
 * @brief 计算acos(x)
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

/**
 * @brief 计算绝对值
 * @param x 输入值
 * @return |x|
 * @note 使用内联实现，无函数调用开销
 */
static inline float BSP_Math_Fabs(float x)
{
    return (x >= 0.0f) ? x : -x;
}

/*============================================
 *              通用数学宏定义
 *============================================*/

/* 常用数学常量 */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f
#endif

#ifndef M_2PI
#define M_2PI 6.28318530717958647692f
#endif

#ifndef M_E
#define M_E 2.71828182845904523536f
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880f
#endif

#ifndef M_SQRT3
#define M_SQRT3 1.73205080756887729352f
#endif

/* 角度弧度转换 */
#define DEG_TO_RAD(deg) ((deg) * M_PI / 180.0f)
#define RAD_TO_DEG(rad) ((rad) * 180.0f / M_PI)

/* RPM与rad/s转换 */
#define RPM_TO_RADPS(rpm) ((rpm) * M_2PI / 60.0f)
#define RADPS_TO_RPM(radps) ((radps) * 60.0f / M_2PI)

/* 限制范围 */
#define MATH_CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/**
 * @brief 浮点数限幅
 * @param val 输入值
 * @param min 最小值
 * @param max 最大值
 * @return 限制后的值
 */
static inline float BSP_Math_Clamp(float val, float min, float max)
{
    if (val > max)
        return max;
    if (val < min)
        return min;
    return val;
}

/* 符号函数 */
#define MATH_SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

/* 最大最小值 */
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* 绝对值 */
#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))
#endif

/* 浮点绝对值 */
#define FABS(x) ((x) > 0.0f ? (x) : -(x))

/* 平方 */
#define SQ(x) ((x) * (x))

/*============================================
 *              向量类型定义
 *============================================*/

/**
 * @brief 二维向量类型
 */
typedef struct
{
    float x;
    float y;
} vector2_t;

/**
 * @brief 三维向量类型
 */
typedef struct
{
    float x;
    float y;
    float z;
} vector3_t;

/*============================================
 *              角度处理函数
 *============================================*/

/**
 * @brief 计算两个角度的最短角度差（弧度）
 * @param from 起始角度（弧度）
 * @param to 目标角度（弧度）
 * @return 角度差（弧度），范围 (-π, π]
 * @note 用于计算多圈位置累加、最短路径运动等
 */
static inline float BSP_Math_AngleDiff(float from, float to)
{
    float diff = to - from;
    // 使用整除计算需要加减的圈数，避免循环
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

/*============================================
 *              Vector2 运算函数
 *============================================*/

/**
 * @brief 向量加法 (v1 + v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 结果向量
 */
static inline vector2_t BSP_Math_Vec2Add(vector2_t v1, vector2_t v2)
{
    vector2_t result;
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    return result;
}

/**
 * @brief 向量减法 (v1 - v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 结果向量
 */
static inline vector2_t BSP_Math_Vec2Sub(vector2_t v1, vector2_t v2)
{
    vector2_t result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    return result;
}

/**
 * @brief 向量数乘 (v * scalar)
 * @param v 向量
 * @param scalar 标量
 * @return 结果向量
 */
static inline vector2_t BSP_Math_Vec2Scale(vector2_t v, float scalar)
{
    vector2_t result;
    result.x = v.x * scalar;
    result.y = v.y * scalar;
    return result;
}

/**
 * @brief 向量点积 (v1 · v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 点积结果
 */
static inline float BSP_Math_Vec2Dot(vector2_t v1, vector2_t v2)
{
    return v1.x * v2.x + v1.y * v2.y;
}

/**
 * @brief 向量模长
 * @param v 向量
 * @return 模长
 */
static inline float BSP_Math_Vec2Length(vector2_t v)
{
    return BSP_Math_Sqrt(v.x * v.x + v.y * v.y);
}

/**
 * @brief 向量归一化
 * @param v 向量
 * @return 单位向量
 * @note 零向量返回 (0, 0)
 */
static inline vector2_t BSP_Math_Vec2Normalize(vector2_t v)
{
    float len = BSP_Math_Vec2Length(v);
    if (len < 1e-6f)
    {
        vector2_t zero = {0.0f, 0.0f};
        return zero;
    }
    return BSP_Math_Vec2Scale(v, 1.0f / len);
}

/**
 * @brief 向量距离
 * @param v1 向量1
 * @param v2 向量2
 * @return 两点间距离
 */
static inline float BSP_Math_Vec2Distance(vector2_t v1, vector2_t v2)
{
    return BSP_Math_Vec2Length(BSP_Math_Vec2Sub(v1, v2));
}

/**
 * @brief 向量夹角（弧度）
 * @param v1 向量1
 * @param v2 向量2
 * @return 夹角（弧度），范围 (-π, π]
 */
static inline float BSP_Math_Vec2Angle(vector2_t v1, vector2_t v2)
{
    float len1 = BSP_Math_Vec2Length(v1);
    float len2 = BSP_Math_Vec2Length(v2);
    if (len1 < 1e-6f || len2 < 1e-6f)
    {
        return 0.0f;
    }
    float cos_theta = BSP_Math_Vec2Dot(v1, v2) / (len1 * len2);
    cos_theta = BSP_Math_Clamp(cos_theta, -1.0f, 1.0f);
    return BSP_Math_Acos(cos_theta);
}

/**
 * @brief 向量线性插值
 * @param v1 起始向量
 * @param v2 目标向量
 * @param t 插值参数 [0, 1]
 * @return 插值结果
 */
static inline vector2_t BSP_Math_Vec2Lerp(vector2_t v1, vector2_t v2, float t)
{
    t = BSP_Math_Clamp(t, 0.0f, 1.0f);
    vector2_t result;
    result.x = v1.x + (v2.x - v1.x) * t;
    result.y = v1.y + (v2.y - v1.y) * t;
    return result;
}

/*============================================
 *              Vector3 运算函数
 *============================================*/

/**
 * @brief 向量加法 (v1 + v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 结果向量
 */
static inline vector3_t BSP_Math_Vec3Add(vector3_t v1, vector3_t v2)
{
    vector3_t result;
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    result.z = v1.z + v2.z;
    return result;
}

/**
 * @brief 向量减法 (v1 - v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 结果向量
 */
static inline vector3_t BSP_Math_Vec3Sub(vector3_t v1, vector3_t v2)
{
    vector3_t result;
    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    result.z = v1.z - v2.z;
    return result;
}

/**
 * @brief 向量数乘 (v * scalar)
 * @param v 向量
 * @param scalar 标量
 * @return 结果向量
 */
static inline vector3_t BSP_Math_Vec3Scale(vector3_t v, float scalar)
{
    vector3_t result;
    result.x = v.x * scalar;
    result.y = v.y * scalar;
    result.z = v.z * scalar;
    return result;
}

/**
 * @brief 向量点积 (v1 · v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 点积结果
 */
static inline float BSP_Math_Vec3Dot(vector3_t v1, vector3_t v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/**
 * @brief 向量叉积 (v1 × v2)
 * @param v1 向量1
 * @param v2 向量2
 * @return 叉积结果向量
 */
static inline vector3_t BSP_Math_Vec3Cross(vector3_t v1, vector3_t v2)
{
    vector3_t result;
    result.x = v1.y * v2.z - v1.z * v2.y;
    result.y = v1.z * v2.x - v1.x * v2.z;
    result.z = v1.x * v2.y - v1.y * v2.x;
    return result;
}

/**
 * @brief 向量模长
 * @param v 向量
 * @return 模长
 */
static inline float BSP_Math_Vec3Length(vector3_t v)
{
    return BSP_Math_Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/**
 * @brief 向量归一化
 * @param v 向量
 * @return 单位向量
 * @note 零向量返回 (0, 0, 0)
 */
static inline vector3_t BSP_Math_Vec3Normalize(vector3_t v)
{
    float len = BSP_Math_Vec3Length(v);
    if (len < 1e-6f)
    {
        vector3_t zero = {0.0f, 0.0f, 0.0f};
        return zero;
    }
    return BSP_Math_Vec3Scale(v, 1.0f / len);
}

/**
 * @brief 向量距离
 * @param v1 向量1
 * @param v2 向量2
 * @return 两点间距离
 */
static inline float BSP_Math_Vec3Distance(vector3_t v1, vector3_t v2)
{
    return BSP_Math_Vec3Length(BSP_Math_Vec3Sub(v1, v2));
}

/**
 * @brief 向量夹角（弧度）
 * @param v1 向量1
 * @param v2 向量2
 * @return 夹角（弧度），范围 [0, π]
 */
static inline float BSP_Math_Vec3Angle(vector3_t v1, vector3_t v2)
{
    float len1 = BSP_Math_Vec3Length(v1);
    float len2 = BSP_Math_Vec3Length(v2);
    if (len1 < 1e-6f || len2 < 1e-6f)
    {
        return 0.0f;
    }
    float cos_theta = BSP_Math_Vec3Dot(v1, v2) / (len1 * len2);
    cos_theta = BSP_Math_Clamp(cos_theta, -1.0f, 1.0f);
    return BSP_Math_Acos(cos_theta);
}

/**
 * @brief 向量线性插值
 * @param v1 起始向量
 * @param v2 目标向量
 * @param t 插值参数 [0, 1]
 * @return 插值结果
 */
static inline vector3_t BSP_Math_Vec3Lerp(vector3_t v1, vector3_t v2, float t)
{
    t = BSP_Math_Clamp(t, 0.0f, 1.0f);
    vector3_t result;
    result.x = v1.x + (v2.x - v1.x) * t;
    result.y = v1.y + (v2.y - v1.y) * t;
    result.z = v1.z + (v2.z - v1.z) * t;
    return result;
}

#endif // __BSP_MATH_H