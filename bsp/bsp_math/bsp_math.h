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
 *              硬件特性查询接口
 *============================================*/

/**
 * @brief 检查FPU是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasFPU(void)
{
#if HAS_FPU
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查DSP指令集是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasDSP(void)
{
#if HAS_DSP
    return 1;
#else
    return 0;
#endif
}

/*============================================
 *              初始化接口
 *============================================*/

/**
 * @brief 初始化数学模块
 * @note 根据硬件特性自动初始化：
 *       - FPU: 启用浮点运算（已在启动代码中完成）
 */
void BSP_Math_Init(void);

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
 *       DJI_A 使用 V1.10.0，支持 arm_atan2_f32
 *       其他开发板使用旧版本，回退到标准库
 */
static inline float BSP_Math_Atan2(float y, float x)
{
#if HAS_DSP && (DEVELOPMENT_BOARD == DJI_A)
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
 *              CRC 计算接口（纯软件实现）
 *============================================*/

/**
 * @brief CRC计算配置结构体
 */
typedef struct
{
    uint32_t init_value; // 初始值
    uint32_t poly_size;  // 多项式宽度: 7, 8, 16, 32
    uint32_t poly;       // 生成多项式
    uint8_t reverse_in;  // 输入反转
    uint8_t reverse_out; // 输出反转
} BSP_CRC_Config_t;

/**
 * @brief CRC7计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC7校验值
 */
uint8_t BSP_Math_CRC7(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC8计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC8校验值
 */
uint8_t BSP_Math_CRC8(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC16计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC16校验值
 */
uint16_t BSP_Math_CRC16(const uint8_t *data, uint32_t len, uint16_t init_val);

/**
 * @brief CRC32计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC32校验值
 */
uint32_t BSP_Math_CRC32(const uint8_t *data, uint32_t len, uint32_t init_val);

/**
 * @brief 自定义CRC计算
 * @param config CRC配置
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC校验值
 */
uint32_t BSP_Math_CRC_Custom(const BSP_CRC_Config_t *config, const uint8_t *data, uint32_t len);

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

/* 限制范围 */
#define MATH_CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

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
#define MATH_FABS(x) ((x) > 0.0f ? (x) : -(x))

/* 平方 */
#define SQ(x) ((x) * (x))

#endif // __BSP_MATH_H
