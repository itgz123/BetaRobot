/**
 * @file bsp_math_trig_lut.h
 * @brief 自研查表三角函数（四分之一周期 / 2π 完整周期正弦表 + 线性插值，宏切换）
 *
 * @note 独立于 bsp_math_trig.h 的 CMSIS/libm 路径，可单独选用
 * @note 表数据由 tools/gen_trig_lut.py 生成于 bsp_math_trig_lut.c
 * @note 用 BSP_MATH_TRIG_TABLE_KIND 宏选择表结构：
 *       - BSP_MATH_TRIG_KIND_QUARTER（默认）：四分之一周期 [0,π/2] 表 + 象限映射，
 *         BSP_MATH_TRIG_TABLE_SIZE = 四分之一区间数 M，默认 2048（满 float32 精度档）；
 *       - BSP_MATH_TRIG_KIND_FULL：2π 完整周期表，无象限映射（速度优先），
 *         BSP_MATH_TRIG_TABLE_SIZE = 全周期区间数 N（=4M 同精度），满精度档 N=8192。
 *       可在编译前覆盖，如：
 *       -DBSP_MATH_TRIG_TABLE_KIND=BSP_MATH_TRIG_KIND_FULL -DBSP_MATH_TRIG_TABLE_SIZE=8192
 * @note 本文件仅依赖 <stdint.h>/<stddef.h>，无 CMSIS/libm 依赖，可在 PC 端直接编译检验
 */

#ifndef __BSP_MATH_TRIG_LUT_H
#define __BSP_MATH_TRIG_LUT_H

#include <stdint.h>
#include <stddef.h>

/*============================================
 *          精度配置与查表常量
 *============================================*/

/* 表结构类型：QUARTER = 四分之一周期表（省 flash，象限映射）；
 *              FULL    = 2π 完整周期表（无象限映射，速度优先）。 */
#ifndef BSP_MATH_TRIG_KIND_QUARTER
#define BSP_MATH_TRIG_KIND_QUARTER 0
#endif
#ifndef BSP_MATH_TRIG_KIND_FULL
#define BSP_MATH_TRIG_KIND_FULL 1
#endif

/* 当前选用的表结构，默认四分之一周期（兼容既有行为与固件 flash 占用）。
 * 需要全周期时编译前覆盖：-DBSP_MATH_TRIG_TABLE_KIND=BSP_MATH_TRIG_KIND_FULL */
#ifndef BSP_MATH_TRIG_TABLE_KIND
#define BSP_MATH_TRIG_TABLE_KIND BSP_MATH_TRIG_KIND_QUARTER
#endif

#if BSP_MATH_TRIG_TABLE_KIND != BSP_MATH_TRIG_KIND_QUARTER && \
    BSP_MATH_TRIG_TABLE_KIND != BSP_MATH_TRIG_KIND_FULL
#error "BSP_MATH_TRIG_TABLE_KIND 必须为 BSP_MATH_TRIG_KIND_QUARTER 或 BSP_MATH_TRIG_KIND_FULL"
#endif

/* 表区间数（表实际存储 SIZE+1 个 float）。
 * QUARTER：SIZE = 四分之一周期区间数 M，默认 2048（满 float32 精度档）；
 * FULL：   SIZE = 2π 全周期区间数 N（=4M 同精度），满精度档 N=8192。
 * 可选值见 bsp_math_trig_lut.c 中的 #if 分支。 */
#ifndef BSP_MATH_TRIG_TABLE_SIZE
#define BSP_MATH_TRIG_TABLE_SIZE 2048
#endif

/* 数学常量（float32 字面量，编译期折叠） */
#define BSP_MATH_TRIG_INV_2PI 0.15915494309189533577f     /* 1/2π   */
#define BSP_MATH_TRIG_2PI 6.28318530717958647692f         /* 2π     */
#define BSP_MATH_TRIG_INV_QUARTER 0.63661977236758134308f /* 2/π    */
#define BSP_MATH_TRIG_QUARTER 1.57079632679489661923f     /* π/2    */

/* QUARTER 相位位置换算：u = phi * (M * 2/π)，编译期折叠为单个 float 常数 */
#define BSP_MATH_TRIG_U_SCALE \
    ((float)BSP_MATH_TRIG_TABLE_SIZE * BSP_MATH_TRIG_INV_QUARTER)

/* FULL 相位位置换算：u = theta * (N * 1/2π)，编译期折叠为单个 float 常数 */
#define BSP_MATH_TRIG_FULL_U_SCALE \
    ((float)BSP_MATH_TRIG_TABLE_SIZE * BSP_MATH_TRIG_INV_2PI)

/* 表声明：定义在 bsp_math_trig_lut.c，仅编译被选中 kind+size 的档位 */
extern const float BSP_Math_SinTable[];     /* QUARTER：四分之一周期正弦表 */
extern const float BSP_Math_FullSinTable[]; /* FULL：2π 完整周期正弦表      */

/*============================================
 *              查表三角函数接口
 *============================================*/

/**
 * @brief 内部函数：对四分之一周期表做线性插值
 * @param u 相位位置，取值范围 [0, M]
 * @return 插值结果 sin(u·π/(2M))
 * @note u == M（相位 π/2）时钳制到最后一个区间，避免数组越界
 */
static inline float BSP_Math_TrigTableLerp(float u)
{
    uint32_t i = (uint32_t)u;
    float frac = u - (float)i;

    if (i >= (uint32_t)BSP_MATH_TRIG_TABLE_SIZE)
    {
        i = (uint32_t)BSP_MATH_TRIG_TABLE_SIZE - 1u;
        frac = 1.0f;
    }
    return BSP_Math_SinTable[i] + frac * (BSP_Math_SinTable[i + 1] - BSP_Math_SinTable[i]);
}

/**
 * @brief 内部函数：对 2π 完整周期表做线性插值
 * @param u 相位位置，取值范围 [0, N]
 * @return 插值结果 sin(u·2π/N)
 * @note u == N（相位 2π）时钳制到最后一个区间，避免数组越界
 */
static inline float BSP_Math_FullTrigLerp(float u)
{
    uint32_t i = (uint32_t)u;
    float frac = u - (float)i;

    if (i >= (uint32_t)BSP_MATH_TRIG_TABLE_SIZE)
    {
        i = (uint32_t)BSP_MATH_TRIG_TABLE_SIZE - 1u;
        frac = 1.0f;
    }
    return BSP_Math_FullSinTable[i] + frac * (BSP_Math_FullSinTable[i + 1] - BSP_Math_FullSinTable[i]);
}

/**
 * @brief 同时计算 sin(theta) 和 cos(theta)
 * @param theta 角度（弧度），任意值（内部归一化到 [0, 2π)）
 * @param[out] p_sin sin(theta) 输出（可为 NULL）
 * @param[out] p_cos cos(theta) 输出（可为 NULL）
 * @note cos 通过 sin(x + π/2) 复用同一张表：QUARTER 象限 +1、相位不变；
 *       FULL 相位 +π/2 后直接索引
 * @note 大角度（|theta| ≳ 1e3 rad）因 float32 归一回舍入精度下降，属固有局限
 * @attention 输入须满足 |theta| < 2π·2^31（int32 截断极限，实际场景远小于此）
 */
static inline void BSP_Math_SinCosLUT(float theta, float *p_sin, float *p_cos)
{
#if BSP_MATH_TRIG_TABLE_KIND == BSP_MATH_TRIG_KIND_FULL
    /* ---- FULL：2π 完整周期表，无象限映射，直接索引 ---- */
    const float two_pi = BSP_MATH_TRIG_2PI;
    const float inv_two_pi = BSP_MATH_TRIG_INV_2PI;
    const float quarter = BSP_MATH_TRIG_QUARTER;
    const float n_f = (float)BSP_MATH_TRIG_TABLE_SIZE;
    const float scale = BSP_MATH_TRIG_FULL_U_SCALE;
    float theta0, thc, u, sv, cv;

    /* 1. 归一化到 [0, 2π)（int 截断 + 负数补偿） */
    theta0 = theta - two_pi * (float)(int32_t)(theta * inv_two_pi);
    if (theta0 < 0.0f)
    {
        theta0 += two_pi;
    }
    /* 2π 恰好舍入到 two_pi 时归零（防 2π 被当作有效角） */
    if (theta0 >= two_pi)
    {
        theta0 -= two_pi;
    }

    /* 2. sin：直接索引插值 */
    u = theta0 * scale;
    if (u > n_f) /* 防 θ≈2π 舍入致 u 越界 */
    {
        u = n_f;
    }
    sv = BSP_Math_FullTrigLerp(u);

    /* 3. cos：sin(theta + π/2)，相位 +π/2 后若 ≥2π 减一圈 */
    thc = theta0 + quarter;
    if (thc >= two_pi)
    {
        thc -= two_pi;
    }
    u = thc * scale;
    if (u > n_f)
    {
        u = n_f;
    }
    cv = BSP_Math_FullTrigLerp(u);

#else
    /* ---- QUARTER：四分之一周期表 + 象限映射 ---- */
    const float two_pi = BSP_MATH_TRIG_2PI;
    const float inv_two_pi = BSP_MATH_TRIG_INV_2PI;
    const float inv_quarter = BSP_MATH_TRIG_INV_QUARTER;
    const float quarter = BSP_MATH_TRIG_QUARTER;
    const float m_f = (float)BSP_MATH_TRIG_TABLE_SIZE;
    const float scale = BSP_MATH_TRIG_U_SCALE;
    float phi, u, p, sv, cv;
    uint32_t q, qc;

    /* 1. 归一化到 [0, 2π)（int 截断 + 负数补偿） */
    theta = theta - two_pi * (float)(int32_t)(theta * inv_two_pi);
    if (theta < 0.0f)
    {
        theta += two_pi;
    }
    /* 2π 恰好舍入到 two_pi 时归零（防 2π 被当作有效角） */
    if (theta >= two_pi)
    {
        theta -= two_pi;
    }

    /* 2. 象限与相位 */
    q = (uint32_t)(theta * inv_quarter);
    if (q > 3u)
    {
        q = 3u;
    }
    phi = theta - (float)q * quarter;
    u = phi * scale;
    if (u > m_f)
    {
        u = m_f;
    }

    /* 3. sin：奇象限反向索引（镜像），后两象限取负 */
    p = (q & 1u) ? (m_f - u) : u;
    sv = BSP_Math_TrigTableLerp(p);
    if (q & 2u)
    {
        sv = -sv;
    }

    /* 4. cos：sin(theta + π/2) ⇔ 象限 +1、相位不变 */
    qc = (q + 1u) & 3u;
    p = (qc & 1u) ? (m_f - u) : u;
    cv = BSP_Math_TrigTableLerp(p);
    if (qc & 2u)
    {
        cv = -cv;
    }
#endif

    if (p_sin != NULL)
    {
        *p_sin = sv;
    }
    if (p_cos != NULL)
    {
        *p_cos = cv;
    }
}

/**
 * @brief 计算 sin(theta)
 * @param theta 角度（弧度），任意值
 * @return sin(theta)
 */
static inline float BSP_Math_SinLUT(float theta)
{
    float s;
    BSP_Math_SinCosLUT(theta, &s, NULL);
    return s;
}

/**
 * @brief 计算 cos(theta)
 * @param theta 角度（弧度），任意值
 * @return cos(theta)
 */
static inline float BSP_Math_CosLUT(float theta)
{
    float c;
    BSP_Math_SinCosLUT(theta, NULL, &c);
    return c;
}

/**
 * @brief 计算 tan(theta) = sin(theta) / cos(theta)
 * @param theta 角度（弧度），任意值
 * @return tan(theta)
 * @note θ 接近 π/2 或 3π/2 时 cos→0，结果幅值巨大，精度固有受限
 */
static inline float BSP_Math_TanLUT(float theta)
{
    float s, c;
    BSP_Math_SinCosLUT(theta, &s, &c);
    return s / c;
}

#endif /* __BSP_MATH_TRIG_LUT_H */
