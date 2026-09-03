/**
 * @file lib_math.h
 * @brief lib层数学函数封装：伞形头文件
 *
 * @note 本文件是 lib_math 模块的公共头文件，包含所有子模块
 * @note 已有代码只需 #include "lib_math.h" 即可使用所有功能
 *
 * @note 子模块列表：
 *       - lib_math_const.h    数学常量、宏
 *       - lib_math_int64.h    64 位整数乘除（高半乘法/魔数除法，零除法指令）
 *       - lib_math_types.h    类型定义（向量、四元数、矩阵）
 *       - lib_math_trig.h     三角函数封装
 *       - lib_math_trig_lut.h 自研查表三角函数
 *       - lib_math_angle.h    角度处理函数
 *       - lib_math_vector.h   向量运算（Vec2/Vec3）
 *       - lib_math_quat.h     四元数运算
 *       - lib_math_matrix.h   矩阵运算（3x3/4x4）
 *       - lib_math_transform.h 变换函数（旋转、平移、剪切、Rodrigues）
 *       - lib_math_linalg.h   任意维通用线性代数（增广求逆、块置单位/对角）
 *
 * @note 三角函数（lib_math_trig.h）全部使用标准库 math.h；
 *       若定义 LIB_MATH_TRIG_LUT_USED，sin/cos/sincos 自动改用查表法
 */

#ifndef __LIB_MATH_H
#define __LIB_MATH_H

/* 包含顺序：叶依赖优先，上层依赖在后 */
#include "lib_math_const.h"     /* 数学常量、宏、通用内联函数 */
#include "lib_math_int64.h"     /* 64 位整数乘除（高半乘法/魔数除法） */
#include "lib_math_types.h"     /* 类型定义 */
#include "lib_math_trig_lut.h"  /* 自研查表三角函数（先于 trig.h，供其分发） */
#include "lib_math_trig.h"      /* 三角函数封装（sin/cos/sincos 可选走 LUT） */
#include "lib_math_angle.h"     /* 角度处理函数 */
#include "lib_math_vector.h"    /* Vec2 和 Vec3 运算 */
#include "lib_math_quat.h"      /* 四元数运算 */
#include "lib_math_matrix.h"    /* 3x3 和 4x4 矩阵运算 */
#include "lib_math_transform.h" /* 变换函数 */
#include "lib_math_linalg.h"    /* 任意维通用线性代数 */

#endif /* __LIB_MATH_H */
