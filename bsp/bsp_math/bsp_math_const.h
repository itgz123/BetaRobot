/**
 * @file bsp_math_const.h
 * @brief 数学常量、单位转换宏、通用数学宏
 *
 * @note 本文件定义：
 *       1. 常用数学常量（PI、E、根号二、根号三等）
 *       2. 角度/弧度、RPM/rad·s⁻¹ 转换宏
 *       3. 常用数学宏（绝对值、平方、限幅等）
 *       4. 通用数学内联函数
 *
 * @note 本文件不依赖 bsp.h 或 math.h，是纯常量/宏定义
 */

#ifndef __BSP_MATH_CONST_H
#define __BSP_MATH_CONST_H

#include <stdint.h>

/*============================================
 *              常用数学常量
 *============================================*/

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

/*============================================
 *              角度弧度转换
 *============================================*/

#define DEG_TO_RAD(deg) ((deg) * M_PI / 180.0f)
#define RAD_TO_DEG(rad) ((rad) * 180.0f / M_PI)

/*============================================
 *              RPM与rad/s转换
 *============================================*/

#define RPM_TO_RADPS(rpm) ((rpm) * M_2PI / 60.0f)
#define RADPS_TO_RPM(radps) ((radps) * 60.0f / M_2PI)

/*============================================
 *              通用数学宏
 *============================================*/

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
#define FABS(x) ((x) > 0.0f ? (x) : -(x))

/* 平方 */
#define SQ(x) ((x) * (x))

/*============================================
 *              通用数学内联函数
 *============================================*/

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

/**
 * @brief 浮点绝对值
 * @param x 输入值
 * @return |x|
 */
static inline float BSP_Math_Fabs(float x)
{
    return (x >= 0.0f) ? x : -x;
}

#endif /* __BSP_MATH_CONST_H */
