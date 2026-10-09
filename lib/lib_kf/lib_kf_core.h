/**
 * @file lib_kf_core.h
 * @brief 卡尔曼族共享代数内核：预测协方差 / 量测更新（lib_lkf、lib_ekf、lib_eskf 共用）
 *
 * ═══════════════════ 为什么单独抽出来 ═══════════════════
 * 无论线性 KF、非线性 EKF 还是误差状态 ESKF，**协方差代数完全相同**：
 *   预测：P = F·P·Fᵀ + Q
 *   更新：S = H·P·Hᵀ + R → K = P·Hᵀ·S⁻¹ → x += K·innov → P = (I-KH)·P 或 Joseph
 * 三者只是"x 是什么、F/H 从哪来、innov 怎么算"不同。把这段代数放在这里共用，
 * 三层的实现就都退化成薄封装，不会各写一套矩阵代码（也保证数值行为逐位一致）。
 *
 * ═══════════════════ 以 innovation 为入参 ═══════════════════
 * 内核**不自己算残差**，残差由调用方给：
 *   lib_lkf   → innov = z - H·x          （线性）
 *   lib_ekf  → innov = z - h(x)         （非线性量测）
 *   lib_eskf → innov = z - h(x_nominal) （名义状态在滤波器外）
 * 这样内核不需要知道调用方是线性还是非线性，也就没有任何 hack。
 *
 * @note 纯算法、硬件无关、无日志、无动态内存、不感知时间（dt 由调用方折进 F/Q）。
 * @note header-only 全 static inline，自身**不分配任何工作区**：工作区由各层的
 *       `*_INSTANCE_DEF` 按上限维数静态分配后传入，不会重复占 RAM。
 * @note 本文件**不做 `*_USED` 门控**：它只是内联函数集合，未用到的会被丢弃；
 *       lib_lkf / lib_ekf / lib_eskf 任一开启都需要它。
 * @note 为保证 lib_lkf 重构前后**逐位一致**，本文件各循环的下标顺序与累加表达式
 *       与旧版 lib_lkf.c 的实现逐条对应，改动时请勿"顺手优化"结合顺序。
 */

#ifndef __LIB_KF_CORE_H
#define __LIB_KF_CORE_H

#include <stdint.h>
#include "lib_math_linalg.h"

/*============================ 选项位 ============================*/

/* 置位 = 使用 Joseph 协方差更新（数值最稳）；未置位 = 经典式 P = (I-KH)·P。
 * 与 lib_lkf.h 的 LKF_OPT_JOSEPH 同值，各层可各自定义同值枚举。 */
#define LIB_KF_CORE_OPT_JOSEPH (1u << 0)

/*============================ 通用矩阵助手 ============================*/

/**
 * @brief C = A·B（行主序紧凑矩阵）
 * @param C 结果 r×c
 * @param A 左矩阵 r×k
 * @param B 右矩阵 k×c
 */
static inline void Lib_KfCore_MatMul(float *C, const float *A, const float *B, int r, int k, int c)
{
    if (C == NULL || A == NULL || B == NULL || r < 1 || k < 1 || c < 1)
    {
        return;
    }
    for (int i = 0; i < r; i++)
    {
        const float *arow = &A[i * k];
        for (int j = 0; j < c; j++)
        {
            float acc = 0.0f;
            for (int t = 0; t < k; t++)
            {
                acc += arow[t] * B[t * c + j];
            }
            C[i * c + j] = acc;
        }
    }
}

/**
 * @brief C = A·Bᵀ（行主序紧凑矩阵）
 * @param C 结果 r×c
 * @param A 左矩阵 r×k
 * @param B 右矩阵 c×k（按行取用，故转置后为 k×c）
 */
static inline void Lib_KfCore_MatMulABt(float *C, const float *A, const float *B, int r, int k, int c)
{
    if (C == NULL || A == NULL || B == NULL || r < 1 || k < 1 || c < 1)
    {
        return;
    }
    for (int i = 0; i < r; i++)
    {
        const float *arow = &A[i * k];
        for (int j = 0; j < c; j++)
        {
            const float *brow = &B[j * k];
            float acc = 0.0f;
            for (int t = 0; t < k; t++)
            {
                acc += arow[t] * brow[t];
            }
            C[i * c + j] = acc;
        }
    }
}

/**
 * @brief 对称化方阵：用两侧均值回写，抑制长期数值漂移
 * @param P n×n（行主序紧凑）
 */
static inline void Lib_KfCore_Symmetrize(float *P, int n)
{
    if (P == NULL || n < 1)
    {
        return;
    }
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < i; j++)
        {
            float v = 0.5f * (P[i * n + j] + P[j * n + i]);
            P[i * n + j] = v;
            P[j * n + i] = v;
        }
    }
}

/*============================ 预测：P = F·P·Fᵀ + Q ============================*/

/**
 * @brief 协方差时间更新：P ← F·P·Fᵀ + Q（含对称化）
 * @param P  n×n 误差协方差（原地读写）
 * @param F  n×n 状态转移矩阵（EKF/ESKF 由调用方写入线性化雅可比）
 * @param Q  n×n 过程噪声协方差
 * @param n  维数
 * @param wA n×n 工作区（调用方提供，不得与 P/F/Q 重叠）
 * @param wB n×n 工作区（调用方提供）
 */
static inline void Lib_KfCore_Predict(float *P, const float *F, const float *Q, int n, float *wA, float *wB)
{
    if (P == NULL || F == NULL || Q == NULL || wA == NULL || wB == NULL || n < 1)
    {
        return;
    }

    /* wA = F·P */
    Lib_KfCore_MatMul(wA, F, P, n, n, n);
    /* wB = wA·Fᵀ */
    Lib_KfCore_MatMulABt(wB, wA, F, n, n, n);
    /* P = wB + Q 并对称化（只算上三角再回写两侧） */
    for (int i = 0; i < n; i++)
    {
        for (int j = i; j < n; j++)
        {
            float v = wB[i * n + j] + Q[i * n + j];
            P[i * n + j] = v;
            P[j * n + i] = v;
        }
    }
}

/*============================ 量测更新 ============================*/

/**
 * @brief 量测更新（校正）：x ← x + K·innov，P ← (I-KH)·P 或 Joseph
 * @param x     n 状态（原地读写）
 * @param P     n×n 误差协方差（原地读写）
 * @param n     状态维
 * @param m     本次量测通道数（可 < 上限，支持部分量测 / 多速率）
 * @param H     m×n 量测雅可比（行主序紧凑）
 * @param R     m×m 量测噪声协方差（行主序紧凑）
 * @param innov m 残差向量（**由调用方算好**：z - h(x)）
 * @param opt   LIB_KF_CORE_OPT_JOSEPH 位标志
 * @param wA    n×n 工作区
 * @param wB    n×n 工作区
 * @param wC    n×m 工作区（同时复用作 K·R）
 * @param wG    n×m 工作区（卡尔曼增益 K）
 * @param wD    2·m·m 工作区（增广 [S|I] 求逆）
 * @return 0 成功；-1 表示 S 奇异（本次更新跳过，x/P 未改动）
 */
static inline int Lib_KfCore_Update(float *x, float *P, int n, int m, const float *H, const float *R, const float *innov, uint8_t opt, float *wA, float *wB,
                                    float *wC, float *wG, float *wD)
{
    if (x == NULL || P == NULL || H == NULL || R == NULL || innov == NULL || wA == NULL || wB == NULL || wC == NULL || wG == NULL || wD == NULL || n < 1 ||
        m < 1)
    {
        return -1;
    }

    const int step = 2 * m; /* 增广矩阵行距 */

    /* ---------- 1. wC = P·Hᵀ（n×m） ---------- */
    for (int i = 0; i < n; i++)
    {
        const float *prow = &P[i * n];
        for (int j = 0; j < m; j++)
        {
            float acc = 0.0f;
            const float *hrow = &H[j * n];
            for (int k = 0; k < n; k++)
            {
                acc += prow[k] * hrow[k];
            }
            wC[i * m + j] = acc;
        }
    }

    /* ---------- 2. 构建 S = H·wC + R 到 wD 左侧 m×m 块 ---------- */
    for (int i = 0; i < m; i++)
    {
        const float *hrow = &H[i * n];
        for (int j = 0; j < m; j++)
        {
            float acc = 0.0f;
            for (int k = 0; k < n; k++)
            {
                acc += hrow[k] * wC[k * m + j];
            }
            wD[i * step + j] = acc + R[i * m + j];
        }
    }

    /* ---------- 3. 增广 [S|I] Gauss-Jordan 求 S⁻¹；奇异则跳过本次更新 ---------- */
    if (Lib_Math_MatInvGaussJordan(wD, m, step) != 0)
    {
        return -1; /* x/P 尚未改动 */
    }
    const float *sinv = &wD[m]; /* 增广矩阵右侧块 = S⁻¹，行距 step */

    /* ---------- 4. K = wC·S⁻¹（wG，n×m） ---------- */
    for (int i = 0; i < n; i++)
    {
        const float *crow = &wC[i * m];
        for (int j = 0; j < m; j++)
        {
            float acc = 0.0f;
            for (int k = 0; k < m; k++)
            {
                acc += crow[k] * sinv[k * step + j];
            }
            wG[i * m + j] = acc;
        }
    }

    /* ---------- 5. x := x + K·innov ---------- */
    for (int i = 0; i < n; i++)
    {
        float acc = x[i];
        const float *krow = &wG[i * m];
        for (int k = 0; k < m; k++)
        {
            acc += krow[k] * innov[k];
        }
        x[i] = acc;
    }

    /* ---------- 6. wA = I - K·H ---------- */
    for (int i = 0; i < n; i++)
    {
        const float *krow = &wG[i * m];
        for (int j = 0; j < n; j++)
        {
            float acc = (i == j) ? 1.0f : 0.0f;
            for (int k = 0; k < m; k++)
            {
                acc -= krow[k] * H[k * n + j];
            }
            wA[i * n + j] = acc;
        }
    }
    /* ---------- 7. wB = wA·P（使用更新前的 P） ---------- */
    for (int i = 0; i < n; i++)
    {
        const float *arow = &wA[i * n];
        for (int j = 0; j < n; j++)
        {
            float acc = 0.0f;
            for (int k = 0; k < n; k++)
            {
                acc += arow[k] * P[k * n + j];
            }
            wB[i * n + j] = acc;
        }
    }

    if (opt & LIB_KF_CORE_OPT_JOSEPH)
    {
        /* P = wB·wAᵀ */
        for (int i = 0; i < n; i++)
        {
            const float *brow = &wB[i * n];
            for (int j = 0; j < n; j++)
            {
                float acc = 0.0f;
                const float *arow = &wA[j * n];
                for (int k = 0; k < n; k++)
                {
                    acc += brow[k] * arow[k];
                }
                P[i * n + j] = acc;
            }
        }
        /* P += K·R·Kᵀ：先 wC = K·R，再累加 wC·wGᵀ */
        for (int i = 0; i < n; i++)
        {
            const float *krow = &wG[i * m];
            for (int j = 0; j < m; j++)
            {
                float acc = 0.0f;
                for (int k = 0; k < m; k++)
                {
                    acc += krow[k] * R[k * m + j];
                }
                wC[i * m + j] = acc;
            }
        }
        for (int i = 0; i < n; i++)
        {
            const float *crow = &wC[i * m];
            for (int j = 0; j < n; j++)
            {
                float acc = P[i * n + j];
                const float *krow = &wG[j * m];
                for (int k = 0; k < m; k++)
                {
                    acc += crow[k] * krow[k];
                }
                P[i * n + j] = acc;
            }
        }
    }
    else
    {
        /* 经典式：P = wB */
        for (int i = 0; i < n * n; i++)
        {
            P[i] = wB[i];
        }
    }

    /* ---------- 8. P 对称化 ---------- */
    Lib_KfCore_Symmetrize(P, n);

    return 0;
}

#endif /* __LIB_KF_CORE_H */
