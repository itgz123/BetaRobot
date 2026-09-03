/**
 * @file lib_math_trig_lut.h
 * @brief 自研查表三角函数（四分之一周期 / 2π 完整周期正弦表 + 线性插值，宏切换）
 *
 * @note 独立于 lib_math_trig.h 的 CMSIS/libm 路径，可单独选用
 * @note 表数据由 tools/gen_trig_lut.py 生成于 lib_math_trig_lut.c
 * @note 表结构类型 LIB_MATH_TRIG_TABLE_KIND 用数字字面量：0=QUARTER（四分之一周期
 *       [0,π/2] 表 + 象限映射，省 flash）、1=FULL（2π 完整周期表，无象限映射，速度优先）。
 *       LIB_MATH_TRIG_TABLE_SIZE = 区间数（QUARTER 满精度档 M=2048，FULL 满精度档 N=8192，同精度 N=4M）。
 * @note 配置入口（共用同一套 SPEED/PREC → KIND/SIZE 映射）：
 *       1) 固件：app_cfg.h 定义 LIB_MATH_TRIG_LUT_USED、SPEED(0|1)、PREC(0..3)；
 *       2) PC 检验：定义 LIB_MATH_TRIG_LUT_STANDALONE 跳过 app_cfg.h，命令行
 *          -D LIB_MATH_TRIG_LUT_SPEED/PREC 选档（见 tools/test_trig_lut.sh）。
 * @note 本文件仅依赖 <stdint.h>/<stddef.h>，无 CMSIS/libm 依赖，可在 PC 端直接编译检验
 */

#ifndef __LIB_MATH_TRIG_LUT_H
#define __LIB_MATH_TRIG_LUT_H

#include <stdint.h>
#include <stddef.h>

/* 配置入口：
 *   固件编译——include app_cfg.h（lib 层可包含的唯一 app 文件），从中获取
 *   LIB_MATH_TRIG_LUT_USED/SPEED/PREC；
 *   PC 独立检验（tools/test_trig_lut.sh）——定义 LIB_MATH_TRIG_LUT_STANDALONE
 *   跳过 app_cfg.h（PC 端不引入工程配置），命令行直接 -D SPEED/PREC，
 *   与 app_cfg.h 走同一高层入口、覆盖任意一档。 */
#ifndef LIB_MATH_TRIG_LUT_STANDALONE
#include "app_cfg.h"
#endif

/*============================================
 *            启用开关（app_cfg.h）
 *============================================*/

/* 仅在 app_cfg.h 定义了 LIB_MATH_TRIG_LUT_USED 时本模块才编译表与接口；
 * 未定义时本头为空、表不占 flash（若调用 Lib_Math_*LUT 将编译报错）。 */
#ifdef LIB_MATH_TRIG_LUT_USED

#if defined(LIB_MATH_TRIG_LUT_SPEED) && defined(LIB_MATH_TRIG_LUT_PREC)
#if LIB_MATH_TRIG_LUT_SPEED == 1
#define LIB_MATH_TRIG_TABLE_KIND 1 /* FULL：2π 完整周期表（无象限映射，速度优先） */
#if LIB_MATH_TRIG_LUT_PREC == 0
#define LIB_MATH_TRIG_TABLE_SIZE 1024 /* FULL 1025 项 × 4B ≈ 4.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 1
#define LIB_MATH_TRIG_TABLE_SIZE 2048 /* FULL 2049 项 × 4B ≈ 8.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 2
#define LIB_MATH_TRIG_TABLE_SIZE 4096 /* FULL 4097 项 × 4B ≈ 16.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 3
#define LIB_MATH_TRIG_TABLE_SIZE 8192 /* FULL 8193 项 × 4B ≈ 32.0 KB flash */
#else
#error "LIB_MATH_TRIG_LUT_PREC 必须为 0..3（0=低 1=中 2=高 3=满精度）"
#endif
#elif LIB_MATH_TRIG_LUT_SPEED == 0
#define LIB_MATH_TRIG_TABLE_KIND 0 /* QUARTER：四分之一周期表（省 flash，象限映射） */
#if LIB_MATH_TRIG_LUT_PREC == 0
#define LIB_MATH_TRIG_TABLE_SIZE 256 /* QUARTER 257 项 × 4B ≈ 1.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 1
#define LIB_MATH_TRIG_TABLE_SIZE 512 /* QUARTER 513 项 × 4B ≈ 2.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 2
#define LIB_MATH_TRIG_TABLE_SIZE 1024 /* QUARTER 1025 项 × 4B ≈ 4.0 KB flash */
#elif LIB_MATH_TRIG_LUT_PREC == 3
#define LIB_MATH_TRIG_TABLE_SIZE 2048 /* QUARTER 2049 项 × 4B ≈ 8.0 KB flash */
#else
#error "LIB_MATH_TRIG_LUT_PREC 必须为 0..3（0=低 1=中 2=高 3=满精度）"
#endif
#else
#error "LIB_MATH_TRIG_LUT_SPEED 必须为 0（四分之一表）或 1（2π 全周期表）"
#endif
#else
#error "LIB_MATH_TRIG_LUT_SPEED 与 LIB_MATH_TRIG_LUT_PREC 须同时定义（app_cfg.h 或 -D）"
#endif

/*============================================
 *============================================*/
/* 数学常量（float32 字面量，编译期折叠） */
#define LIB_MATH_TRIG_INV_2PI 0.15915494309189533577f     /* 1/2π   */
#define LIB_MATH_TRIG_2PI 6.28318530717958647692f         /* 2π     */
#define LIB_MATH_TRIG_INV_QUARTER 0.63661977236758134308f /* 2/π    */
#define LIB_MATH_TRIG_QUARTER 1.57079632679489661923f     /* π/2    */

/* QUARTER 相位位置换算：u = phi * (M * 2/π)，编译期折叠为单个 float 常数 */
#define LIB_MATH_TRIG_U_SCALE \
    ((float)LIB_MATH_TRIG_TABLE_SIZE * LIB_MATH_TRIG_INV_QUARTER)

/* FULL 相位位置换算：u = theta * (N * 1/2π)，编译期折叠为单个 float 常数 */
#define LIB_MATH_TRIG_FULL_U_SCALE \
    ((float)LIB_MATH_TRIG_TABLE_SIZE * LIB_MATH_TRIG_INV_2PI)

/* 表声明：定义在 lib_math_trig_lut.c，仅编译被选中 kind+size 的档位 */
extern const float Lib_Math_SinTable[];     /* QUARTER：四分之一周期正弦表 */
extern const float Lib_Math_FullSinTable[]; /* FULL：2π 完整周期正弦表      */

/*============================================
 *              查表三角函数接口
 *============================================*/

/**
 * @brief 内部函数：对四分之一周期表做线性插值
 * @param u 相位位置，取值范围 [0, M]
 * @return 插值结果 sin(u·π/(2M))
 * @note u == M（相位 π/2）时钳制到最后一个区间，避免数组越界
 */
static inline float Lib_Math_TrigTableLerp(float u)
{
    uint32_t i = (uint32_t)u;
    float frac = u - (float)i;

    if (i >= (uint32_t)LIB_MATH_TRIG_TABLE_SIZE)
    {
        i = (uint32_t)LIB_MATH_TRIG_TABLE_SIZE - 1u;
        frac = 1.0f;
    }
    return Lib_Math_SinTable[i] + frac * (Lib_Math_SinTable[i + 1] - Lib_Math_SinTable[i]);
}

/**
 * @brief 内部函数：对 2π 完整周期表做线性插值
 * @param u 相位位置，取值范围 [0, N]
 * @return 插值结果 sin(u·2π/N)
 * @note u == N（相位 2π）时钳制到最后一个区间，避免数组越界
 */
static inline float Lib_Math_FullTrigLerp(float u)
{
    uint32_t i = (uint32_t)u;
    float frac = u - (float)i;

    if (i >= (uint32_t)LIB_MATH_TRIG_TABLE_SIZE)
    {
        i = (uint32_t)LIB_MATH_TRIG_TABLE_SIZE - 1u;
        frac = 1.0f;
    }
    return Lib_Math_FullSinTable[i] + frac * (Lib_Math_FullSinTable[i + 1] - Lib_Math_FullSinTable[i]);
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
static inline void Lib_Math_SinCosLUT(float theta, float *p_sin, float *p_cos)
{
#if LIB_MATH_TRIG_TABLE_KIND == 1 /* FULL：2π 完整周期表，无象限映射，直接索引 */
    /* ---- FULL：2π 完整周期表，无象限映射，直接索引 ---- */
    const float two_pi = LIB_MATH_TRIG_2PI;
    const float inv_two_pi = LIB_MATH_TRIG_INV_2PI;
    const float quarter = LIB_MATH_TRIG_QUARTER;
    const float n_f = (float)LIB_MATH_TRIG_TABLE_SIZE;
    const float scale = LIB_MATH_TRIG_FULL_U_SCALE;
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
    sv = Lib_Math_FullTrigLerp(u);

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
    cv = Lib_Math_FullTrigLerp(u);

#else
    /* ---- QUARTER：四分之一周期表 + 象限映射 ---- */
    const float two_pi = LIB_MATH_TRIG_2PI;
    const float inv_two_pi = LIB_MATH_TRIG_INV_2PI;
    const float inv_quarter = LIB_MATH_TRIG_INV_QUARTER;
    const float quarter = LIB_MATH_TRIG_QUARTER;
    const float m_f = (float)LIB_MATH_TRIG_TABLE_SIZE;
    const float scale = LIB_MATH_TRIG_U_SCALE;
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
    sv = Lib_Math_TrigTableLerp(p);
    if (q & 2u)
    {
        sv = -sv;
    }

    /* 4. cos：sin(theta + π/2) ⇔ 象限 +1、相位不变 */
    qc = (q + 1u) & 3u;
    p = (qc & 1u) ? (m_f - u) : u;
    cv = Lib_Math_TrigTableLerp(p);
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
static inline float Lib_Math_SinLUT(float theta)
{
    float s;
    Lib_Math_SinCosLUT(theta, &s, NULL);
    return s;
}

/**
 * @brief 计算 cos(theta)
 * @param theta 角度（弧度），任意值
 * @return cos(theta)
 */
static inline float Lib_Math_CosLUT(float theta)
{
    float c;
    Lib_Math_SinCosLUT(theta, NULL, &c);
    return c;
}

/**
 * @brief 计算 tan(theta) = sin(theta) / cos(theta)
 * @param theta 角度（弧度），任意值
 * @return tan(theta)
 * @note θ 接近 π/2 或 3π/2 时 cos→0，结果幅值巨大，精度固有受限
 */
static inline float Lib_Math_TanLUT(float theta)
{
    float s, c;
    Lib_Math_SinCosLUT(theta, &s, &c);
    return s / c;
}

#endif /* LIB_MATH_TRIG_LUT_USED */

#endif /* __LIB_MATH_TRIG_LUT_H */
