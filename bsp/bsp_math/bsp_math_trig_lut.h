/**
 * @file bsp_math_trig_lut.h
 * @brief 自研查表三角函数（四分之一周期正弦表 + 线性插值）
 *
 * @note 独立于 bsp_math_trig.h 的 CMSIS/libm 路径，可单独选用
 * @note 表数据由 tools/gen_trig_lut.py 生成于 bsp_math_trig_lut.c
 * @note 用 BSP_MATH_TRIG_TABLE_SIZE 宏切换精度档（四分之一周期 [0,π/2] 的区间数 M），
 *       默认 2048（满 float32 精度档：纯插值+量化误差 ≈1.0e-7 < ε=2^-23），
 *       可在编译前覆盖，如：-DBSP_MATH_TRIG_TABLE_SIZE=1024
 * @note 本文件仅依赖 <stdint.h>，无 CMSIS/libm 依赖，可在 PC 端直接编译检验
 */

#ifndef __BSP_MATH_TRIG_LUT_H
#define __BSP_MATH_TRIG_LUT_H

#include <stdint.h>
#include <stddef.h>

/*============================================
 *          精度配置与查表常量
 *============================================*/

/* 四分之一周期表区间数 M（表实际存储 M+1 个 float）。
 * 可选值见 bsp_math_trig_lut.c 中的 #if 分支（默认生成 256 / 1024 / 2048 / 4096）。 */
#ifndef BSP_MATH_TRIG_TABLE_SIZE
#define BSP_MATH_TRIG_TABLE_SIZE 2048
#endif

/* 数学常量（float32 字面量，编译期折叠） */
#define BSP_MATH_TRIG_INV_2PI 0.15915494309189533577f     /* 1/2π   */
#define BSP_MATH_TRIG_2PI 6.28318530717958647692f         /* 2π     */
#define BSP_MATH_TRIG_INV_QUARTER 0.63661977236758134308f /* 2/π    */
#define BSP_MATH_TRIG_QUARTER 1.57079632679489661923f     /* π/2    */

/* 相位位置换算：u = phi * (M * 2/π)，编译期折叠为单个 float 常数 */
#define BSP_MATH_TRIG_U_SCALE \
    ((float)BSP_MATH_TRIG_TABLE_SIZE * BSP_MATH_TRIG_INV_QUARTER)

/* 表声明：定义在 bsp_math_trig_lut.c，仅编译被选中的档位 */
extern const float BSP_Math_SinTable[];

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
 * @brief 同时计算 sin(theta) 和 cos(theta)
 * @param theta 角度（弧度），任意值（内部归一化到 [0, 2π)）
 * @param[out] p_sin sin(theta) 输出（可为 NULL）
 * @param[out] p_cos cos(theta) 输出（可为 NULL）
 * @note cos 通过 sin(x + π/2) 复用同一张表（象限 +1、相位不变）
 * @note 大角度（|theta| ≳ 1e3 rad）因 float32 归一回舍入精度下降，属固有局限
 * @attention 输入须满足 |theta| < 2π·2^31（int32 截断极限，实际场景远小于此）
 */
static inline void BSP_Math_SinCosLUT(float theta, float *p_sin, float *p_cos)
{
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
