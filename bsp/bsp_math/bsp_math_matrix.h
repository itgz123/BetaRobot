/**
 * @file bsp_math_matrix.h
 * @brief 3x3 和 4x4 矩阵运算
 *
 * @note 本文件提供矩阵类型的基本运算：乘法、转置、行列式、求逆等
 * @note 以及旋转、缩放、剪切、平移等变换矩阵的构造
 * @note 矩阵使用 row-major 存储：data[row][col]
 */

#ifndef __BSP_MATH_MATRIX_H
#define __BSP_MATH_MATRIX_H

#include "bsp_math_types.h"
#include "bsp_math_trig.h"
#include "bsp_math_const.h"
#include "bsp_math_vector.h"

/*============================================
 *              3x3 矩阵运算
 *============================================*/

/**
 * @brief 3x3 单位矩阵
 */
static inline matrix3_t BSP_Math_Mat3Identity(void)
{
    matrix3_t m = {0};
    m.data[0][0] = 1.0f;
    m.data[1][1] = 1.0f;
    m.data[2][2] = 1.0f;
    return m;
}

/**
 * @brief 3x3 矩阵乘法 (m1 * m2)
 * @note result[i][j] = Σ_k(m1[i][k] * m2[k][j])
 */
static inline matrix3_t BSP_Math_Mat3Mul(matrix3_t m1, matrix3_t m2)
{
    matrix3_t result = {0};
    int i, j, k;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            for (k = 0; k < 3; k++)
            {
                result.data[i][j] += m1.data[i][k] * m2.data[k][j];
            }
        }
    }
    return result;
}

/**
 * @brief 3x3 矩阵转置
 */
static inline matrix3_t BSP_Math_Mat3Transpose(matrix3_t m)
{
    matrix3_t result;
    int i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            result.data[i][j] = m.data[j][i];
        }
    }
    return result;
}

/**
 * @brief 3x3 矩阵行列式
 * @return 行列式值
 */
static inline float BSP_Math_Mat3Determinant(matrix3_t m)
{
    return m.data[0][0] * (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) - m.data[0][1] * (m.data[1][0] * m.data[2][2] - m.data[1][2] * m.data[2][0]) + m.data[0][2] * (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]);
}

/**
 * @brief 3x3 矩阵求逆
 * @note 使用余子式 / 行列式方法
 * @note 行列式接近 0 时返回单位矩阵
 */
static inline matrix3_t BSP_Math_Mat3Inverse(matrix3_t m)
{
    float det = BSP_Math_Mat3Determinant(m);
    if (BSP_Math_Fabs(det) < 1e-12f)
    {
        return BSP_Math_Mat3Identity();
    }
    float inv_det = 1.0f / det;
    matrix3_t result;

    /* 余子式矩阵的转置（伴随矩阵）乘以 1/det */
    result.data[0][0] = (m.data[1][1] * m.data[2][2] - m.data[1][2] * m.data[2][1]) * inv_det;
    result.data[0][1] = (m.data[0][2] * m.data[2][1] - m.data[0][1] * m.data[2][2]) * inv_det;
    result.data[0][2] = (m.data[0][1] * m.data[1][2] - m.data[0][2] * m.data[1][1]) * inv_det;

    result.data[1][0] = (m.data[1][2] * m.data[2][0] - m.data[1][0] * m.data[2][2]) * inv_det;
    result.data[1][1] = (m.data[0][0] * m.data[2][2] - m.data[0][2] * m.data[2][0]) * inv_det;
    result.data[1][2] = (m.data[0][2] * m.data[1][0] - m.data[0][0] * m.data[1][2]) * inv_det;

    result.data[2][0] = (m.data[1][0] * m.data[2][1] - m.data[1][1] * m.data[2][0]) * inv_det;
    result.data[2][1] = (m.data[0][1] * m.data[2][0] - m.data[0][0] * m.data[2][1]) * inv_det;
    result.data[2][2] = (m.data[0][0] * m.data[1][1] - m.data[0][1] * m.data[1][0]) * inv_det;

    return result;
}

/**
 * @brief 3x3 矩阵乘以三维向量 (m * v)
 */
static inline vector3_t BSP_Math_Mat3MulVec3(matrix3_t m, vector3_t v)
{
    vector3_t result;
    result.x = m.data[0][0] * v.x + m.data[0][1] * v.y + m.data[0][2] * v.z;
    result.y = m.data[1][0] * v.x + m.data[1][1] * v.y + m.data[1][2] * v.z;
    result.z = m.data[2][0] * v.x + m.data[2][1] * v.y + m.data[2][2] * v.z;
    return result;
}

/*============================================
 *          3x3 变换矩阵构造
 *============================================*/

/**
 * @brief 3x3 绕 X 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix3_t BSP_Math_Mat3RotationX(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix3_t m = {0};
    m.data[0][0] = 1.0f;
    m.data[1][1] = c;
    m.data[1][2] = -s;
    m.data[2][1] = s;
    m.data[2][2] = c;
    return m;
}

/**
 * @brief 3x3 绕 Y 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix3_t BSP_Math_Mat3RotationY(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix3_t m = {0};
    m.data[0][0] = c;
    m.data[0][2] = s;
    m.data[1][1] = 1.0f;
    m.data[2][0] = -s;
    m.data[2][2] = c;
    return m;
}

/**
 * @brief 3x3 绕 Z 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix3_t BSP_Math_Mat3RotationZ(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix3_t m = {0};
    m.data[0][0] = c;
    m.data[0][1] = -s;
    m.data[1][0] = s;
    m.data[1][1] = c;
    m.data[2][2] = 1.0f;
    return m;
}

/**
 * @brief 3x3 缩放矩阵
 * @param sx X 轴缩放因子
 * @param sy Y 轴缩放因子
 * @param sz Z 轴缩放因子
 */
static inline matrix3_t BSP_Math_Mat3Scale(float sx, float sy, float sz)
{
    matrix3_t m = {0};
    m.data[0][0] = sx;
    m.data[1][1] = sy;
    m.data[2][2] = sz;
    return m;
}

/**
 * @brief 3x3 XY 剪切矩阵
 * @note X 坐标根据 Y 坐标剪切：x' = x + sxy * y
 */
static inline matrix3_t BSP_Math_Mat3ShearXY(float sxy)
{
    matrix3_t m = BSP_Math_Mat3Identity();
    m.data[0][1] = sxy;
    return m;
}

/**
 * @brief 3x3 XZ 剪切矩阵
 * @note X 坐标根据 Z 坐标剪切：x' = x + sxz * z
 */
static inline matrix3_t BSP_Math_Mat3ShearXZ(float sxz)
{
    matrix3_t m = BSP_Math_Mat3Identity();
    m.data[0][2] = sxz;
    return m;
}

/**
 * @brief 3x3 YZ 剪切矩阵
 * @note Y 坐标根据 Z 坐标剪切：y' = y + syz * z
 */
static inline matrix3_t BSP_Math_Mat3ShearYZ(float syz)
{
    matrix3_t m = BSP_Math_Mat3Identity();
    m.data[1][2] = syz;
    return m;
}

/*============================================
 *              4x4 矩阵运算
 *============================================*/

/**
 * @brief 4x4 单位矩阵
 */
static inline matrix4_t BSP_Math_Mat4Identity(void)
{
    matrix4_t m = {0};
    m.data[0][0] = 1.0f;
    m.data[1][1] = 1.0f;
    m.data[2][2] = 1.0f;
    m.data[3][3] = 1.0f;
    return m;
}

/**
 * @brief 4x4 矩阵乘法 (m1 * m2)
 */
static inline matrix4_t BSP_Math_Mat4Mul(matrix4_t m1, matrix4_t m2)
{
    matrix4_t result = {0};
    int i, j, k;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            for (k = 0; k < 4; k++)
            {
                result.data[i][j] += m1.data[i][k] * m2.data[k][j];
            }
        }
    }
    return result;
}

/**
 * @brief 4x4 矩阵转置
 */
static inline matrix4_t BSP_Math_Mat4Transpose(matrix4_t m)
{
    matrix4_t result;
    int i, j;
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            result.data[i][j] = m.data[j][i];
        }
    }
    return result;
}

/**
 * @brief 4x4 矩阵乘以三维点（w=1，含透视除法）
 * @param m 4x4 变换矩阵
 * @param v 三维点坐标
 * @return 变换后的三维点
 * @note 当 w 分量接近 0 时返回零向量
 */
static inline vector3_t BSP_Math_Mat4MulPoint(matrix4_t m, vector3_t v)
{
    float x = m.data[0][0] * v.x + m.data[0][1] * v.y + m.data[0][2] * v.z + m.data[0][3];
    float y = m.data[1][0] * v.x + m.data[1][1] * v.y + m.data[1][2] * v.z + m.data[1][3];
    float z = m.data[2][0] * v.x + m.data[2][1] * v.y + m.data[2][2] * v.z + m.data[2][3];
    float w = m.data[3][0] * v.x + m.data[3][1] * v.y + m.data[3][2] * v.z + m.data[3][3];

    if (BSP_Math_Fabs(w) < 1e-12f)
    {
        vector3_t zero = {0.0f, 0.0f, 0.0f};
        return zero;
    }
    float inv_w = 1.0f / w;
    vector3_t result;
    result.x = x * inv_w;
    result.y = y * inv_w;
    result.z = z * inv_w;
    return result;
}

/**
 * @brief 4x4 矩阵乘以三维方向向量（w=0，无平移）
 * @param m 4x4 变换矩阵
 * @param v 三维方向向量
 * @return 变换后的方向向量
 * @note 方向向量不受平移影响，仅应用旋转/缩放
 */
static inline vector3_t BSP_Math_Mat4MulDir(matrix4_t m, vector3_t v)
{
    vector3_t result;
    result.x = m.data[0][0] * v.x + m.data[0][1] * v.y + m.data[0][2] * v.z;
    result.y = m.data[1][0] * v.x + m.data[1][1] * v.y + m.data[1][2] * v.z;
    result.z = m.data[2][0] * v.x + m.data[2][1] * v.y + m.data[2][2] * v.z;
    return result;
}

/*============================================
 *          4x4 变换矩阵构造
 *============================================*/

/**
 * @brief 4x4 绕 X 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix4_t BSP_Math_Mat4RotationX(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix4_t m = {0};
    m.data[0][0] = 1.0f;
    m.data[1][1] = c;
    m.data[1][2] = -s;
    m.data[2][1] = s;
    m.data[2][2] = c;
    m.data[3][3] = 1.0f;
    return m;
}

/**
 * @brief 4x4 绕 Y 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix4_t BSP_Math_Mat4RotationY(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix4_t m = {0};
    m.data[0][0] = c;
    m.data[0][2] = s;
    m.data[1][1] = 1.0f;
    m.data[2][0] = -s;
    m.data[2][2] = c;
    m.data[3][3] = 1.0f;
    return m;
}

/**
 * @brief 4x4 绕 Z 轴旋转矩阵
 * @param angle 旋转角度 (rad)
 */
static inline matrix4_t BSP_Math_Mat4RotationZ(float angle)
{
    float c = BSP_Math_Cos(angle);
    float s = BSP_Math_Sin(angle);
    matrix4_t m = {0};
    m.data[0][0] = c;
    m.data[0][1] = -s;
    m.data[1][0] = s;
    m.data[1][1] = c;
    m.data[2][2] = 1.0f;
    m.data[3][3] = 1.0f;
    return m;
}

/**
 * @brief 4x4 缩放矩阵
 * @param sx X 轴缩放因子
 * @param sy Y 轴缩放因子
 * @param sz Z 轴缩放因子
 */
static inline matrix4_t BSP_Math_Mat4Scale(float sx, float sy, float sz)
{
    matrix4_t m = {0};
    m.data[0][0] = sx;
    m.data[1][1] = sy;
    m.data[2][2] = sz;
    m.data[3][3] = 1.0f;
    return m;
}

/**
 * @brief 4x4 平移矩阵
 * @param tx X 方向平移
 * @param ty Y 方向平移
 * @param tz Z 方向平移
 */
static inline matrix4_t BSP_Math_Mat4Translate(float tx, float ty, float tz)
{
    matrix4_t m = BSP_Math_Mat4Identity();
    m.data[0][3] = tx;
    m.data[1][3] = ty;
    m.data[2][3] = tz;
    return m;
}

/**
 * @brief 4x4 XY 剪切矩阵
 * @note X 坐标根据 Y 坐标剪切
 */
static inline matrix4_t BSP_Math_Mat4ShearXY(float sxy)
{
    matrix4_t m = BSP_Math_Mat4Identity();
    m.data[0][1] = sxy;
    return m;
}

/**
 * @brief 4x4 XZ 剪切矩阵
 * @note X 坐标根据 Z 坐标剪切
 */
static inline matrix4_t BSP_Math_Mat4ShearXZ(float sxz)
{
    matrix4_t m = BSP_Math_Mat4Identity();
    m.data[0][2] = sxz;
    return m;
}

/**
 * @brief 4x4 YZ 剪切矩阵
 * @note Y 坐标根据 Z 坐标剪切
 */
static inline matrix4_t BSP_Math_Mat4ShearYZ(float syz)
{
    matrix4_t m = BSP_Math_Mat4Identity();
    m.data[1][2] = syz;
    return m;
}

/**
 * @brief 4x4 仿射矩阵求逆（仅限旋转 + 平移组合）
 * @param m 仿射变换矩阵 [R T; 0 1]
 * @return 逆矩阵 [R^T  -R^T*T; 0 1]
 * @note 不适用于包含缩放或剪切的矩阵
 */
static inline matrix4_t BSP_Math_Mat4InverseAffine(matrix4_t m)
{
    /* 提取旋转子矩阵 (左上 3x3) */
    matrix3_t r;
    r.data[0][0] = m.data[0][0];
    r.data[0][1] = m.data[0][1];
    r.data[0][2] = m.data[0][2];
    r.data[1][0] = m.data[1][0];
    r.data[1][1] = m.data[1][1];
    r.data[1][2] = m.data[1][2];
    r.data[2][0] = m.data[2][0];
    r.data[2][1] = m.data[2][1];
    r.data[2][2] = m.data[2][2];

    matrix3_t r_trans = BSP_Math_Mat3Transpose(r);

    /* 提取平移向量 */
    vector3_t t = {m.data[0][3], m.data[1][3], m.data[2][3]};
    vector3_t neg_rt_t = BSP_Math_Vec3Scale(BSP_Math_Mat3MulVec3(r_trans, t), -1.0f);

    matrix4_t result;
    /* 左上 3x3 = R^T */
    result.data[0][0] = r_trans.data[0][0];
    result.data[0][1] = r_trans.data[0][1];
    result.data[0][2] = r_trans.data[0][2];
    result.data[1][0] = r_trans.data[1][0];
    result.data[1][1] = r_trans.data[1][1];
    result.data[1][2] = r_trans.data[1][2];
    result.data[2][0] = r_trans.data[2][0];
    result.data[2][1] = r_trans.data[2][1];
    result.data[2][2] = r_trans.data[2][2];

    /* 平移 = -R^T * t */
    result.data[0][3] = neg_rt_t.x;
    result.data[1][3] = neg_rt_t.y;
    result.data[2][3] = neg_rt_t.z;

    /* 底行 */
    result.data[3][0] = 0.0f;
    result.data[3][1] = 0.0f;
    result.data[3][2] = 0.0f;
    result.data[3][3] = 1.0f;

    return result;
}

#endif /* __BSP_MATH_MATRIX_H */
