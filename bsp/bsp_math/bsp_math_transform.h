/**
 * @file bsp_math_transform.h
 * @brief 变换函数：旋转、缩放、剪切、平移、罗德里格斯公式
 *
 * @note 本文件提供高级变换运算，包括：
 *       1. 二维向量旋转
 *       2. 三维向量绕任意轴旋转（Rodrigues' rotation formula）
 *       3. 从轴角表示生成旋转矩阵（Rodrigues' formula）
 *       4. 合成 TRS（平移-旋转-缩放）变换矩阵
 */

#ifndef __BSP_MATH_TRANSFORM_H
#define __BSP_MATH_TRANSFORM_H

#include "bsp_math_types.h"
#include "bsp_math_trig.h"
#include "bsp_math_vector.h"
#include "bsp_math_const.h"
#include "bsp_math_matrix.h"
#include "bsp_math_quat.h"

/*============================================
 *              二维向量旋转
 *============================================*/

/**
 * @brief 二维向量逆时针旋转
 * @param v 输入向量
 * @param angle 旋转角度 (rad)，正值为逆时针
 * @return 旋转后的向量
 *
 * @note 公式：x' = x*cos - y*sin
 *            y' = x*sin + y*cos
 */
static inline vector2_t BSP_Math_Vec2Rotate(vector2_t v, float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    vector2_t result;
    result.x = v.x * c - v.y * s;
    result.y = v.x * s + v.y * c;
    return result;
}

/*============================================
 *          罗德里格斯公式 (Rodrigues')
 *============================================*/

/**
 * @brief 三维向量绕任意轴旋转（Rodrigues' rotation formula）
 * @param v 待旋转向量
 * @param axis 旋转轴（不必为单位向量，函数内部归一化）
 * @param angle 旋转角度 (rad)
 * @return 旋转后的向量
 *
 * @note 公式：v_rot = v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ))
 *       其中 k 为单位轴向量
 * @note 零轴返回原向量
 */
static inline vector3_t BSP_Math_Vec3RotateAxis(vector3_t v, vector3_t axis, float angle)
{
    float len = BSP_Math_Vec3Length(axis);
    if (len < 1e-8f)
    {
        return v; /* 零轴，返回原向量 */
    }

    /* 归一化旋转轴 */
    vector3_t k = BSP_Math_Vec3Scale(axis, 1.0f / len);
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    float one_minus_c = 1.0f - c;

    /* v_rot = v * cos(θ) + (k × v) * sin(θ) + k * (k · v) * (1 - cos(θ)) */
    vector3_t k_cross_v = BSP_Math_Vec3Cross(k, v);
    vector3_t k_dot_v_k = BSP_Math_Vec3Scale(k, BSP_Math_Vec3Dot(k, v));

    vector3_t term1 = BSP_Math_Vec3Scale(v, c);
    vector3_t term2 = BSP_Math_Vec3Scale(k_cross_v, s);
    vector3_t term3 = BSP_Math_Vec3Scale(k_dot_v_k, one_minus_c);

    vector3_t result = BSP_Math_Vec3Add(term1, term2);
    result = BSP_Math_Vec3Add(result, term3);
    return result;
}

/**
 * @brief 从轴角表示生成 3x3 旋转矩阵（Rodrigues' formula）
 * @param axis 旋转轴（不必为单位向量，函数内部归一化）
 * @param angle 旋转角度 (rad)
 * @return 3x3 旋转矩阵
 *
 * @note 公式：R = I + sin(θ)·[k]_× + (1-cos(θ))·[k]_×²
 *       其中 [k]_× 为单位轴向量 k 的叉积矩阵
 * @note 零轴返回单位矩阵
 */
static inline matrix3_t BSP_Math_Mat3FromAxisAngle(vector3_t axis, float angle)
{
    float len = BSP_Math_Vec3Length(axis);
    if (len < 1e-8f)
    {
        return BSP_Math_Mat3Identity();
    }

    vector3_t k = BSP_Math_Vec3Scale(axis, 1.0f / len);
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    float one_minus_c = 1.0f - c;

    matrix3_t m;

    /* R = I + sin(θ)·K + (1-cos(θ))·K²
     * 其中 K 为 k 的叉积矩阵
     * K = [ 0   -kz  ky ]
     *     [ kz   0  -kx ]
     *     [-ky  kx   0  ]
     */
    m.data[0][0] = 1.0f + one_minus_c * (k.x * k.x - 1.0f);
    m.data[0][1] = -s * k.z + one_minus_c * k.x * k.y;
    m.data[0][2] = s * k.y + one_minus_c * k.x * k.z;

    m.data[1][0] = s * k.z + one_minus_c * k.y * k.x;
    m.data[1][1] = 1.0f + one_minus_c * (k.y * k.y - 1.0f);
    m.data[1][2] = -s * k.x + one_minus_c * k.y * k.z;

    m.data[2][0] = -s * k.y + one_minus_c * k.z * k.x;
    m.data[2][1] = s * k.x + one_minus_c * k.z * k.y;
    m.data[2][2] = 1.0f + one_minus_c * (k.z * k.z - 1.0f);

    return m;
}

/*============================================
 *          TRS 变换矩阵合成
 *============================================*/

/**
 * @brief 合成 4x4 TRS 变换矩阵（平移-旋转-缩放）
 * @param t 平移向量
 * @param r 旋转四元数
 * @param s 缩放向量
 * @return 4x4 TRS 矩阵
 *
 * @note 组合顺序：M = T * R * S（先缩放，再旋转，再平移）
 * @note 旋转部分由四元数直接填入矩阵，避免欧拉角转换
 */
static inline matrix4_t BSP_Math_Mat4TRS(vector3_t t, quaternion_t r, vector3_t s)
{
    float qw = r.w, qx = r.x, qy = r.y, qz = r.z;
    float sx = s.x, sy = s.y, sz = s.z;

    matrix4_t m = {0};

    /* 第 0 行 */
    m.data[0][0] = (1.0f - 2.0f * (qy * qy + qz * qz)) * sx;
    m.data[0][1] = (2.0f * (qx * qy - qw * qz)) * sy;
    m.data[0][2] = (2.0f * (qx * qz + qw * qy)) * sz;
    m.data[0][3] = t.x;

    /* 第 1 行 */
    m.data[1][0] = (2.0f * (qx * qy + qw * qz)) * sx;
    m.data[1][1] = (1.0f - 2.0f * (qx * qx + qz * qz)) * sy;
    m.data[1][2] = (2.0f * (qy * qz - qw * qx)) * sz;
    m.data[1][3] = t.y;

    /* 第 2 行 */
    m.data[2][0] = (2.0f * (qx * qz - qw * qy)) * sx;
    m.data[2][1] = (2.0f * (qy * qz + qw * qx)) * sy;
    m.data[2][2] = (1.0f - 2.0f * (qx * qx + qy * qy)) * sz;
    m.data[2][3] = t.z;

    /* 第 3 行 */
    m.data[3][3] = 1.0f;

    return m;
}

#endif /* __BSP_MATH_TRANSFORM_H */
