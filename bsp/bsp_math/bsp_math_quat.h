/**
 * @file bsp_math_quat.h
 * @brief 四元数运算函数
 *
 * @note 本文件提供四元数的基本运算：乘法、共轭、归一化、求逆、
 *       旋转向量、与欧拉角的相互转换、球面线性插值等
 */

#ifndef __BSP_MATH_QUAT_H
#define __BSP_MATH_QUAT_H

#include "bsp_math_types.h"
#include "bsp_math_vector.h"
#include "bsp_math_trig.h"
#include "bsp_math_const.h"
#include <math.h>

/*============================================
 *              四元数运算函数
 *============================================*/

/**
 * @brief 单位四元数（无旋转）
 * @return 单位四元数 (1, 0, 0, 0)
 */
static inline quaternion_t BSP_Math_QuatIdentity(void)
{
    quaternion_t q = {1.0f, 0.0f, 0.0f, 0.0f};
    return q;
}

/**
 * @brief 四元数乘法 (q1 * q2)
 * @param q1 左四元数（先应用的旋转）
 * @param q2 右四元数（后应用的旋转）
 * @return 复合旋转的四元数
 * @note 对应的复合旋转：先应用 q2，再应用 q1
 */
static inline quaternion_t BSP_Math_QuatMul(quaternion_t q1, quaternion_t q2)
{
    quaternion_t result;
    result.w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z;
    result.x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y;
    result.y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x;
    result.z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w;
    return result;
}

/**
 * @brief 四元数共轭 (q*)
 * @return 共轭四元数
 * @note 单位四元数的共轭等于其逆
 */
static inline quaternion_t BSP_Math_QuatConjugate(quaternion_t q)
{
    quaternion_t result;
    result.w = q.w;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    return result;
}

/**
 * @brief 四元数模长 |q|
 */
static inline float BSP_Math_QuatLength(quaternion_t q)
{
    return BSP_Math_Sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

/**
 * @brief 四元数归一化
 * @return 单位四元数
 * @note 零四元数返回单位四元数
 */
static inline quaternion_t BSP_Math_QuatNormalize(quaternion_t q)
{
    float len_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (len_sq < 1e-8f)
    {
        quaternion_t zero = {1.0f, 0.0f, 0.0f, 0.0f};
        return zero;
    }
    float inv_len = 1.0f / BSP_Math_Sqrt(len_sq);
    quaternion_t result;
    result.w = q.w * inv_len;
    result.x = q.x * inv_len;
    result.y = q.y * inv_len;
    result.z = q.z * inv_len;
    return result;
}

/**
 * @brief 四元数逆 (q⁻¹ = q* / |q|²)
 * @return 逆四元数
 * @note 对于单位四元数，逆 = 共轭
 */
static inline quaternion_t BSP_Math_QuatInverse(quaternion_t q)
{
    float len_sq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (len_sq < 1e-8f)
    {
        quaternion_t zero = {1.0f, 0.0f, 0.0f, 0.0f};
        return zero;
    }
    float inv = 1.0f / len_sq;
    quaternion_t result;
    result.w = q.w * inv;
    result.x = -q.x * inv;
    result.y = -q.y * inv;
    result.z = -q.z * inv;
    return result;
}

/**
 * @brief 四元数旋转向量 (v' = q * v * q⁻¹)
 * @param q 旋转四元数（需为单位四元数或归一化后使用）
 * @param v 待旋转的三维向量
 * @return 旋转后的向量
 *
 * @note 使用简化公式直接计算：
 *       t = 2 * cross(q.xyz, v)
 *       v' = v + q.w * t + cross(q.xyz, t)
 */
static inline vector3_t BSP_Math_QuatRotateVector(quaternion_t q, vector3_t v)
{
    vector3_t q_vec = {q.x, q.y, q.z};

    /* t = 2 * cross(q_vec, v) */
    vector3_t t = BSP_Math_Vec3Cross(q_vec, v);
    t = BSP_Math_Vec3Scale(t, 2.0f);

    /* u = q.w * t + cross(q_vec, t) */
    vector3_t u;
    u.x = q.w * t.x + (q_vec.y * t.z - q_vec.z * t.y);
    u.y = q.w * t.y + (q_vec.z * t.x - q_vec.x * t.z);
    u.z = q.w * t.z + (q_vec.x * t.y - q_vec.y * t.x);

    /* v' = v + u */
    return BSP_Math_Vec3Add(v, u);
}

/**
 * @brief 四元数转欧拉角 (Z-Y-X 旋转顺序)
 * @return 欧拉角结构体 (roll, pitch, yaw)，单位 rad
 * @note 万向锁处理：当 pitch 接近 ±90° 时，yaw 被置为 0
 */
static inline euler_t BSP_Math_QuatToEuler(quaternion_t q)
{
    euler_t e = {0};

    /* roll (x 轴旋转) */
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    e.roll = BSP_Math_Atan2(sinr_cosp, cosr_cosp);

    /* pitch (y 轴旋转)，处理万向锁 */
    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    if (sinp >= 1.0f)
    {
        e.pitch = M_PI_2;
    }
    else if (sinp <= -1.0f)
    {
        e.pitch = -M_PI_2;
    }
    else
    {
        e.pitch = asinf(sinp);
    }

    /* yaw (z 轴旋转) */
    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    e.yaw = BSP_Math_Atan2(siny_cosp, cosy_cosp);

    return e;
}

/**
 * @brief 欧拉角转四元数 (Z-Y-X 旋转顺序)
 * @param e 输入欧拉角 (roll, pitch, yaw)，单位 rad
 * @return 旋转四元数
 *
 * @note 旋转顺序：先绕 z 轴偏航，再绕 y 轴俯仰，最后绕 x 轴横滚
 *       对应 q = q_yaw * q_pitch * q_roll
 */
static inline quaternion_t BSP_Math_EulerToQuat(euler_t e)
{
    float cy = cosf(e.yaw * 0.5f);
    float sy = sinf(e.yaw * 0.5f);
    float cp = cosf(e.pitch * 0.5f);
    float sp = sinf(e.pitch * 0.5f);
    float cr = cosf(e.roll * 0.5f);
    float sr = sinf(e.roll * 0.5f);

    quaternion_t q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
}

/**
 * @brief 四元数球面线性插值 (Slerp)
 * @param q1 起始四元数
 * @param q2 目标四元数
 * @param t  插值参数 [0, 1]
 * @return 插值后的四元数
 *
 * @note 保持单位四元数约束下的最短路径插值
 * @note 当 q1 与 q2 的夹角接近 0 时回退到线性插值再归一化
 */
static inline quaternion_t BSP_Math_QuatSlerp(quaternion_t q1, quaternion_t q2, float t)
{
    t = BSP_Math_Clamp(t, 0.0f, 1.0f);

    /* 计算夹角余弦 */
    float cos_omega = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

    /* 确保取最短路径 */
    if (cos_omega < 0.0f)
    {
        q2.w = -q2.w;
        q2.x = -q2.x;
        q2.y = -q2.y;
        q2.z = -q2.z;
        cos_omega = -cos_omega;
    }

    /* 夹角很小，回退到线性插值 + 归一化 */
    if (cos_omega > 0.9999f)
    {
        quaternion_t result;
        result.w = q1.w + t * (q2.w - q1.w);
        result.x = q1.x + t * (q2.x - q1.x);
        result.y = q1.y + t * (q2.y - q1.y);
        result.z = q1.z + t * (q2.z - q1.z);
        return BSP_Math_QuatNormalize(result);
    }

    float omega = acosf(cos_omega);
    float sin_omega = sinf(omega);
    float inv_sin = 1.0f / sin_omega;
    float k1 = sinf((1.0f - t) * omega) * inv_sin;
    float k2 = sinf(t * omega) * inv_sin;

    quaternion_t result;
    result.w = q1.w * k1 + q2.w * k2;
    result.x = q1.x * k1 + q2.x * k2;
    result.y = q1.y * k1 + q2.y * k2;
    result.z = q1.z * k1 + q2.z * k2;
    return result;
}

#endif /* __BSP_MATH_QUAT_H */
