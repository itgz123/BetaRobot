/**
 * @file bsp_math.h
 * @brief BSP层数学函数封装：伞形头文件
 *
 * @note 本文件是 bsp_math 模块的公共头文件，包含所有子模块
 * @note 已有代码只需 #include "bsp_math.h" 即可使用所有功能
 *
 * @note 子模块列表：
 *       - bsp_math_const.h    数学常量、宏
 *       - bsp_math_types.h    类型定义（向量、四元数、矩阵）
 *       - bsp_math_trig.h     三角函数封装
 *       - bsp_math_angle.h    角度处理函数
 *       - bsp_math_vector.h   向量运算（Vec2/Vec3）
 *       - bsp_math_quat.h     四元数运算
 *       - bsp_math_matrix.h   矩阵运算（3x3/4x4）
 *       - bsp_math_transform.h 变换函数（旋转、平移、剪切、Rodrigues）
 *
 * @note 硬件特性由 bsp_map.h 中的宏定义控制：
 *       - HAS_FPU:     浮点运算单元
 *       - HAS_DSP:     DSP指令集
 */

#ifndef __BSP_MATH_H
#define __BSP_MATH_H

/* 包含顺序：叶依赖优先，上层依赖在后 */
#include "bsp_math_const.h"      /* 数学常量、宏、通用内联函数 */
#include "bsp_math_types.h"      /* 类型定义 */
#include "bsp_math_trig.h"       /* 三角函数封装 */
#include "bsp_math_angle.h"      /* 角度处理函数 */
#include "bsp_math_vector.h"     /* Vec2 和 Vec3 运算 */
#include "bsp_math_quat.h"       /* 四元数运算 */
#include "bsp_math_matrix.h"     /* 3x3 和 4x4 矩阵运算 */
#include "bsp_math_transform.h"  /* 变换函数 */

#endif /* __BSP_MATH_H */
