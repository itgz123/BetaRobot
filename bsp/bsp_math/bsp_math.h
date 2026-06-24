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

/**
 * @brief 四元数类型
 * @note w 为实部，(x, y, z) 为虚部
 * @note 单位四元数表示旋转，满足 w² + x² + y² + z² = 1
 */
typedef struct
{
    float w;
    float x;
    float y;
    float z;
} quaternion_t;

/**
 * @brief 欧拉角类型
 * @note Z-Y-X 旋转顺序（aerospace 顺序）
 * @note roll 为绕 x 轴旋转角 (rad)
 * @note pitch 为绕 y 轴旋转角 (rad)
 * @note yaw 为绕 z 轴旋转角 (rad)
 */
typedef struct
{
    float roll;
    float pitch;
    float yaw;
} euler_t;

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
 * @note 不改变输入参数，返回新四元数
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
 * @param q 输入四元数
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
 * @param q 输入四元数
 * @return 模长
 */
static inline float BSP_Math_QuatLength(quaternion_t q)
{
    return BSP_Math_Sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

/**
 * @brief 四元数归一化
 * @param q 输入四元数
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
 * @param q 输入四元数
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
    /* q_vec = (x, y, z) */
    vector3_t q_vec = {q.x, q.y, q.z};

    /* t = cross(q_vec, v) */
    vector3_t t = BSP_Math_Vec3Cross(q_vec, v);
    t = BSP_Math_Vec3Scale(t, 2.0f); /* t = 2 * cross(q_vec, v) */

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
 * @param q 输入四元数
 * @return 欧拉角结构体 (roll, pitch, yaw)，单位 rad
 *
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

#endif // __BSP_MATH_H