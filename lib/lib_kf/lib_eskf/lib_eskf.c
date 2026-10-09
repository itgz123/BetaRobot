/**
 * @file lib_eskf.c
 * @brief 通用误差状态卡尔曼滤波(ESKF)实现
 *
 * @note 纯算法模块，无硬件 / 时间依赖；使用单精度 float
 * @note 协方差代数复用 lib_kf_core.h（与 lib_lkf / lib_ekf 同一份实现）；
 *       本文件只保留"误差状态"特有的四处：
 *         - 名义状态与 F/Q 由 propagate 回调在滤波器外给出
 *         - 残差 y、雅可比 H、噪声 R 由 measure 回调直接给出（本层不碰名义状态）
 *         - δx 的注入与清零由 inject 回调 + 本层的时序约束负责
 *         - 注入后 P ← G·P·Gᵀ 的重置（reset_jac 缺省 G=I）
 */

#include "lib_eskf.h"
#include "lib_kf_core.h" /* 与 lib_lkf / lib_ekf 共用的 KF 代数内核 */

#ifndef LIB_ESKF_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_ESKF_USED

/*============================ 私有函数 ============================*/

/** @brief 维度合法性：1 ≤ n ≤ n_max，1 ≤ m ≤ m_max，0 ≤ l ≤ l_max */
static int Eskf_DimOk(const EskfInstance *eskf, int n, int m, int l)
{
    return (n >= 1 && n <= (int)eskf->n_max && m >= 1 && m <= (int)eskf->m_max && l >= 0 && l <= (int)eskf->l_max);
}

/** @brief 装载矩阵（NULL → 清零） */
static void Eskf_LoadMat(float *dst, const float *src, int cnt)
{
    for (int i = 0; i < cnt; i++)
    {
        dst[i] = (src != NULL) ? src[i] : 0.0f;
    }
}

/*============================ 公开接口实现 ============================*/

Eskf_Status_e EskfInit(EskfInstance *eskf, const Eskf_Init_Config_s *cfg)
{
    if (eskf == NULL || cfg == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (!Eskf_DimOk(eskf, cfg->n, cfg->m, cfg->l))
    {
        return ESKF_ERR_DIM;
    }
    /* 三个核心回调缺一不可：没有它们就不构成误差状态滤波 */
    if (cfg->propagate == NULL || cfg->measure == NULL || cfg->inject == NULL)
    {
        return ESKF_ERR_CFG;
    }

    const int n = cfg->n;
    const int m = cfg->m;
    eskf->n = (uint8_t)n;
    eskf->m = (uint8_t)m;
    eskf->l = (uint8_t)cfg->l;
    eskf->opt = cfg->opt;

    /* 回调 / 上下文 */
    eskf->ctx = cfg->ctx;
    eskf->propagate = cfg->propagate;
    eskf->measure = cfg->measure;
    eskf->inject = cfg->inject;
    eskf->reset_jac = cfg->reset_jac;

    /* δx 恒以 0 起滤（误差态没有"初值"；名义状态初值由用户写在 ctx 里） */
    for (int i = 0; i < n; i++)
    {
        eskf->delta[i] = 0.0f;
    }

    /* 协方差初值 P0：既写活跃 P，也留一份供 EskfReset 复原 */
    if (cfg->P0 != NULL)
    {
        for (int i = 0; i < n * n; i++)
        {
            eskf->P0[i] = cfg->P0[i];
            eskf->P[i] = cfg->P0[i];
        }
    }
    else
    {
        Lib_Math_MatSetEye(eskf->P0, n, n);
        Lib_Math_MatSetEye(eskf->P, n, n);
    }

    /* Q/R 只给初值：每步都会被 propagate / measure 回调整块重写 */
    Eskf_LoadMat(eskf->Q, cfg->Q, n * n);
    Eskf_LoadMat(eskf->R, cfg->R, m * m);

    eskf->delta_dirty = 0;
    return ESKF_OK;
}

Eskf_Status_e EskfReset(EskfInstance *eskf)
{
    if (eskf == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (eskf->n < 1 || eskf->n > eskf->n_max)
    {
        return ESKF_ERR_DIM;
    }

    const int n = eskf->n;
    for (int i = 0; i < n; i++)
    {
        eskf->delta[i] = 0.0f;
    }
    for (int i = 0; i < n * n; i++)
    {
        eskf->P[i] = eskf->P0[i];
    }
    eskf->delta_dirty = 0;

    return ESKF_OK;
}

Eskf_Status_e EskfPredict(EskfInstance *eskf, const float *u, float dt)
{
    if (eskf == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (eskf->n < 1 || eskf->n > eskf->n_max)
    {
        return ESKF_ERR_DIM;
    }
    if (eskf->delta_dirty)
    {
        return ESKF_ERR_CFG; /* 上一次量测更新尚未注入，线性化点已失效 */
    }

    const int n = eskf->n;

    /* 名义状态传播 + 误差态 F/Q 一并由回调给出（本层不触碰名义状态） */
    eskf->propagate(eskf->ctx, u, dt, eskf->F, eskf->Q);

    /* P = F·P·Fᵀ + Q */
    Lib_KfCore_Predict(eskf->P, eskf->F, eskf->Q, n, eskf->wA, eskf->wB);

    return ESKF_OK;
}

Eskf_Status_e EskfUpdate(EskfInstance *eskf, const float *z)
{
    if (eskf == NULL || z == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (eskf->n < 1 || eskf->n > eskf->n_max || eskf->m < 1 || eskf->m > eskf->m_max)
    {
        return ESKF_ERR_DIM;
    }

    return EskfUpdateM(eskf, eskf->m, z, NULL);
}

Eskf_Status_e EskfUpdateM(EskfInstance *eskf, uint8_t m_now, const float *z, Eskf_Measure_fn override)
{
    if (eskf == NULL || z == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (eskf->n < 1 || eskf->n > eskf->n_max)
    {
        return ESKF_ERR_DIM;
    }
    if (m_now < 1 || m_now > eskf->m_max)
    {
        return ESKF_ERR_DIM;
    }
    if (eskf->delta_dirty)
    {
        return ESKF_ERR_CFG; /* 须先 EskfInject 再继续 */
    }

    const Eskf_Measure_fn mfn = (override != NULL) ? override : eskf->measure;
    if (mfn == NULL)
    {
        return ESKF_ERR_CFG;
    }

    const int n = eskf->n;
    const int m = (int)m_now;
    float *y = eskf->wF; /* 残差 y = z - h(名义) */

    /* 量测回调：直接给出残差、雅可比、噪声（H 行距 n、R 紧凑行距 m_now） */
    mfn(eskf->ctx, z, m, y, eskf->H, eskf->R);

    /* δ ← δ + K·y，P ← (I-KH)·P 或 Joseph；奇异时不改动 δ/P */
    if (Lib_KfCore_Update(eskf->delta, eskf->P, n, m, eskf->H, eskf->R, y, eskf->opt, eskf->wA, eskf->wB, eskf->wC, eskf->wG, eskf->wD) != 0)
    {
        return ESKF_ERR_SINGULAR;
    }

    eskf->delta_dirty = 1;
    return ESKF_OK;
}

Eskf_Status_e EskfInject(EskfInstance *eskf)
{
    if (eskf == NULL)
    {
        return ESKF_ERR_NULL;
    }
    if (eskf->n < 1 || eskf->n > eskf->n_max)
    {
        return ESKF_ERR_DIM;
    }
    if (!eskf->delta_dirty)
    {
        return ESKF_OK; /* 幂等：没有待注入的 δ */
    }

    const int n = eskf->n;

    /* 1. 注入：名义状态 ← 名义状态 ⊞ δ（唯一修改名义状态的地方） */
    eskf->inject(eskf->ctx, eskf->delta);

    /* 2. P 重置：P ← G·P·Gᵀ（G 缺省为单位阵，即不重置） */
    if (eskf->reset_jac != NULL)
    {
        eskf->reset_jac(eskf->ctx, eskf->delta, eskf->G);
        Lib_KfCore_MatMul(eskf->wA, eskf->G, eskf->P, n, n, n);     /* wA = G·P   */
        Lib_KfCore_MatMulABt(eskf->wB, eskf->wA, eskf->G, n, n, n); /* wB = wA·Gᵀ */
        for (int i = 0; i < n * n; i++)
        {
            eskf->P[i] = eskf->wB[i];
        }
        Lib_KfCore_Symmetrize(eskf->P, n);
    }

    /* 3. δ 清零，允许下一次 Predict */
    for (int i = 0; i < n; i++)
    {
        eskf->delta[i] = 0.0f;
    }
    eskf->delta_dirty = 0;

    return ESKF_OK;
}

#endif /* LIB_ESKF_USED */
