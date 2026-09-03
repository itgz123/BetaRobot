/**
 * @file lib_math_linalg.h
 * @brief 任意维通用线性代数工具（作用于调用方提供的 float 缓冲区）
 *
 * @note 与 lib_math_matrix.h（定长 3×3/4×4，结构体按值传递）不同，本文件面向
 *       运行期任意维的方阵：矩阵行主序存储，可用 row_stride 在更大缓冲内操作
 *       子块，适合 lib_kf 等需要通用维数的算法模块
 * @note 全部 static inline 头文件实现，无外部依赖
 */

#ifndef __LIB_MATH_LINALG_H
#define __LIB_MATH_LINALG_H

#include <stddef.h>

/*============================ 奇异判定阈值 ============================*/

/* Gauss-Jordan 求逆的奇异判定阈值 */
#define LIB_MATH_MAT_INV_REL_EPS (1e-7f)  /* 主元模 / 块内最大模 < 阈值判奇异 */
#define LIB_MATH_MAT_INV_ABS_MIN (1e-30f) /* 绝对下限，防止全零矩阵误判正常 */

/*============================ 任意维方阵原地求逆 ============================*/

/**
 * @brief 任意维方阵增广 [A|I] Gauss-Jordan 原地求逆
 * @param aug 缓冲区：每行 row_stride 个 float，左侧 dim 列为待求逆矩阵 A
 *            （行主序）；函数会先把右侧 [dim, 2·dim) 列写为单位阵，消元结束后
 *            右侧块即 A⁻¹（左侧块变为单位阵）。原地改写。
 * @param dim        方阵阶数
 * @param row_stride 相邻行行首元素间距（元素个数），须 ≥ 2·dim
 * @return 0 成功；非 0 奇异（或参数非法），此时 aug 内容不定，调用方不应使用
 *
 * @note 列主元 + 相对阈值判奇异（阈值见 LIB_MATH_MAT_INV_* 宏）
 * @note 用法：先在 aug 左侧填 A，再以 dim 阶调用本函数，结果 A⁻¹ 位于
 *       &aug[dim]（行距仍为 row_stride）
 */
static inline int Lib_Math_MatInvGaussJordan(float *aug, int dim, int row_stride)
{
    if (aug == NULL || dim < 1 || row_stride < 2 * dim)
    {
        return -1;
    }

    /* 右侧写单位阵，构成 [A|I] */
    for (int i = 0; i < dim; i++)
    {
        float *row = &aug[i * row_stride];
        for (int j = dim; j < row_stride; j++)
        {
            row[j] = (j == dim + i) ? 1.0f : 0.0f;
        }
    }

    /* 初始最大模（仅统计左侧 A 块），用于相对阈值判奇异 */
    float maxabs = 0.0f;
    for (int i = 0; i < dim; i++)
    {
        const float *row = &aug[i * row_stride];
        for (int j = 0; j < dim; j++)
        {
            float a = (row[j] < 0.0f) ? -row[j] : row[j];
            if (a > maxabs)
            {
                maxabs = a;
            }
        }
    }
    if (maxabs < LIB_MATH_MAT_INV_ABS_MIN)
    {
        return -1;
    }
    const float th = maxabs * LIB_MATH_MAT_INV_REL_EPS;

    for (int col = 0; col < dim; col++)
    {
        /* 列主元：在 col..dim-1 行中找 |A[row][col]| 最大者 */
        int pivot = col;
        float pv = aug[col * row_stride + col];
        float pva = (pv < 0.0f) ? -pv : pv;
        for (int r = col + 1; r < dim; r++)
        {
            float v = aug[r * row_stride + col];
            float va = (v < 0.0f) ? -v : v;
            if (va > pva)
            {
                pva = va;
                pv = v;
                pivot = r;
            }
        }
        if (pva <= th)
        {
            return -1;
        }

        /* 交换 pivot 行与 col 行（含右侧 I 块） */
        if (pivot != col)
        {
            float *r0 = &aug[col * row_stride];
            float *r1 = &aug[pivot * row_stride];
            for (int j = 0; j < row_stride; j++)
            {
                float tmp = r0[j];
                r0[j] = r1[j];
                r1[j] = tmp;
            }
        }

        /* 主元行归一化（左侧 col 之前已为 0，可略过） */
        float *prow = &aug[col * row_stride];
        float inv = 1.0f / pv;
        for (int j = col; j < row_stride; j++)
        {
            prow[j] *= inv;
        }

        /* 消去其他行 */
        for (int r = 0; r < dim; r++)
        {
            if (r == col)
            {
                continue;
            }
            float *crow = &aug[r * row_stride];
            float factor = crow[col];
            if (factor == 0.0f)
            {
                continue;
            }
            for (int j = col; j < row_stride; j++)
            {
                crow[j] -= factor * prow[j];
            }
        }
    }

    return 0;
}

/*============================ 任意维方阵块写入 ============================*/

/**
 * @brief 将 dim×dim（行距 row_stride）方块置为单位阵
 * @param mat        缓冲区
 * @param dim        方块阶数
 * @param row_stride 行首元素间距（≥ dim）
 */
static inline void Lib_Math_MatSetEye(float *mat, int dim, int row_stride)
{
    if (mat == NULL || dim < 1 || row_stride < dim)
    {
        return;
    }
    for (int i = 0; i < dim; i++)
    {
        float *row = &mat[i * row_stride];
        for (int j = 0; j < dim; j++)
        {
            row[j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

/**
 * @brief 将 dim×dim（行距 row_stride）方块清零并把对角线写为 diag[]
 * @param mat        缓冲区
 * @param dim        方块阶数
 * @param row_stride 行首元素间距（≥ dim）
 * @param diag       对角线数组（长度 dim）；传 NULL 等价于整块清零
 */
static inline void Lib_Math_MatSetDiag(float *mat, int dim, int row_stride, const float *diag)
{
    if (mat == NULL || dim < 1 || row_stride < dim)
    {
        return;
    }
    for (int i = 0; i < dim; i++)
    {
        float *row = &mat[i * row_stride];
        for (int j = 0; j < dim; j++)
        {
            row[j] = 0.0f;
        }
        if (diag != NULL)
        {
            row[i] = diag[i];
        }
    }
}

/**
 * @brief 将 dim×dim（行距 row_stride）方块清零并把对角线统一写为 value
 * @param mat        缓冲区
 * @param dim        方块阶数
 * @param row_stride 行首元素间距（≥ dim）
 * @param value      对角线元素值
 */
static inline void Lib_Math_MatSetDiagVal(float *mat, int dim, int row_stride, float value)
{
    if (mat == NULL || dim < 1 || row_stride < dim)
    {
        return;
    }
    for (int i = 0; i < dim; i++)
    {
        float *row = &mat[i * row_stride];
        for (int j = 0; j < dim; j++)
        {
            row[j] = 0.0f;
        }
        row[i] = value;
    }
}

#endif /* __LIB_MATH_LINALG_H */
