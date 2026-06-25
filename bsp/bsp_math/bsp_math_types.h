/**
 * @file bsp_math_types.h
 * @brief 向量、四元数、欧拉角、矩阵类型定义
 *
 * @note 本文件定义所有数学类型，不包含任何运算函数
 * @note 矩阵类型使用 row-major 存储：data[row][col]
 */

#ifndef __BSP_MATH_TYPES_H
#define __BSP_MATH_TYPES_H

#include <stdint.h>

/*============================================
 *              二维向量类型
 *============================================*/

/**
 * @brief 二维向量
 */
typedef struct
{
    float x;
    float y;
} vector2_t;

/*============================================
 *              三维向量类型
 *============================================*/

/**
 * @brief 三维向量
 */
typedef struct
{
    float x;
    float y;
    float z;
} vector3_t;

/*============================================
 *              四元数类型
 *============================================*/

/**
 * @brief 四元数
 * @note w 为实部，(x, y, z) 为虚部
 * @note 单位四元数满足 w² + x² + y² + z² = 1
 */
typedef struct
{
    float w;
    float x;
    float y;
    float z;
} quaternion_t;

/*============================================
 *              欧拉角类型
 *============================================*/

/**
 * @brief 欧拉角（Z-Y-X 旋转顺序，aerospace 顺序）
 * @note roll  — 绕 X 轴旋转角 (rad)
 * @note pitch — 绕 Y 轴旋转角 (rad)
 * @note yaw   — 绕 Z 轴旋转角 (rad)
 */
typedef struct
{
    float roll;
    float pitch;
    float yaw;
} euler_t;

/*============================================
 *              3x3 矩阵类型
 *============================================*/

/**
 * @brief 3x3 矩阵
 * @note data[row][col]，row-major 存储
 * @note 适用于 3D 旋转、缩放、剪切变换
 */
typedef struct
{
    float data[3][3];
} matrix3_t;

/*============================================
 *              4x4 矩阵类型
 *============================================*/

/**
 * @brief 4x4 矩阵
 * @note data[row][col]，row-major 存储
 * @note 适用于 3D 仿射变换（旋转、平移、缩放、剪切）
 * @note 最后一行通常为 [0 0 0 1]
 */
typedef struct
{
    float data[4][4];
} matrix4_t;

#endif /* __BSP_MATH_TYPES_H */
