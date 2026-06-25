/**
 * @file bsp_math_vector.h
 * @brief 二维和三维向量运算
 *
 * @note 本文件提供 vector2_t 和 vector3_t 的常见向量运算
 * @note 所有函数均为 static inline 实现
 */

#ifndef __BSP_MATH_VECTOR_H
#define __BSP_MATH_VECTOR_H

#include "bsp_math_types.h"
#include "bsp_math_trig.h"
#include "bsp_math_const.h"

/*============================================
 *              Vector2 运算函数
 *============================================*/

/**
 * @brief 向量加法 (v1 + v2)
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
 */
static inline float BSP_Math_Vec2Dot(vector2_t v1, vector2_t v2)
{
    return v1.x * v2.x + v1.y * v2.y;
}

/**
 * @brief 向量模长
 */
static inline float BSP_Math_Vec2Length(vector2_t v)
{
    return BSP_Math_Sqrt(v.x * v.x + v.y * v.y);
}

/**
 * @brief 向量归一化
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
 * @brief 两点间距离
 */
static inline float BSP_Math_Vec2Distance(vector2_t v1, vector2_t v2)
{
    return BSP_Math_Vec2Length(BSP_Math_Vec2Sub(v1, v2));
}

/**
 * @brief 向量夹角（弧度）
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
 * @param t 插值参数 [0, 1]
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
 */
static inline float BSP_Math_Vec3Dot(vector3_t v1, vector3_t v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/**
 * @brief 向量叉积 (v1 × v2)
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
 */
static inline float BSP_Math_Vec3Length(vector3_t v)
{
    return BSP_Math_Sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/**
 * @brief 向量归一化
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
 * @brief 两点间距离
 */
static inline float BSP_Math_Vec3Distance(vector3_t v1, vector3_t v2)
{
    return BSP_Math_Vec3Length(BSP_Math_Vec3Sub(v1, v2));
}

/**
 * @brief 向量夹角（弧度）
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
 * @param t 插值参数 [0, 1]
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

#endif /* __BSP_MATH_VECTOR_H */
