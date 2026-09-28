/**
 * @file lib_ekf.c
 * @brief 通用非线性扩展卡尔曼滤波(EKF)实现
 *
 * @note 纯算法模块，无硬件 / 时间依赖；使用单精度 float
 * @note 协方差代数复用 lib_kf_core.h（与 lib_lkf / lib_eskf 同一份实现）；
 *       本文件只保留"非线性"特有的四处：状态传播 x⁺ = f(x,u,dt)、
 *       线性化 F = ∂f/∂x、量测预测 z_pred = h(x)、残差 y = z - h(x)
 */

#include "lib_ekf.h"
#include "lib_kf_core.h" /* 与 lib_lkf / lib_eskf 共用的 KF 代数内核 */

#ifndef LIB_EKF_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_EKF_USED

/*============================ 私有函数 ============================*/

/** @brief 把配置里的矩阵搬到实例（NULL → 清零），行为与 LkfInit 一致 */
static void Ekf_LoadMat(float *dst, const float *src, int cnt)
{
    for (int i = 0; i < cnt; i++)
    {
        dst[i] = (src != NULL) ? src[i] : 0.0f;
    }
}

/**
 * @brief 量测更新核心（共享于 EkfUpdate / EkfUpdateM）
 * @param m_now 本次量测通道数
 */
static Ekf_Status_e Ekf_UpdateCore(EkfInstance *ekf, int m_now, const float *z)
{
    const int n = ekf->n;
    float *x = ekf->x;
    float *P = ekf->P;
    float *wF = ekf->wF; /* 先放 z_pred，再就地变残差 */
    float *H = ekf->H;
    float *wR = ekf->wR; /* 紧凑 R（行距 m_now） */

    if (ekf->h_fn == NULL)
    {
        return EKF_ERR_CFG; /* 非线性量测必须有 h 回调 */
    }

    /* ---------- 1. z_pred = h(x) ---------- */
    ekf->h_fn(ekf->ctx, x, m_now, wF);

    /* ---------- 2. 残差 y = z - z_pred（就地） ---------- */
    for (int i = 0; i < m_now; i++)
    {
        wF[i] = z[i] - wF[i];
    }

    /* ---------- 3. 量测雅可比：回调优先，缺省用实例常值阵 ---------- */
    if (ekf->H_fn != NULL)
    {
        ekf->H_fn(ekf->ctx, x, m_now, H);
    }

    /* ---------- 4. R 转紧凑布局：实例内 R 行距为活跃维 m，内核要求行距 m_now ---------- */
    const int r_src_stride = ekf->m;
    for (int i = 0; i < m_now; i++)
    {
        for (int j = 0; j < m_now; j++)
        {
            wR[i * m_now + j] = ekf->R[i * r_src_stride + j];
        }
    }

    /* ---------- 5. 其余代数（S/K/x/P、经典式或 Joseph、对称化）走共享内核 ---------- */
    if (Lib_KfCore_Update(x, P, n, m_now, H, wR, wF, ekf->opt, ekf->wA, ekf->wB, ekf->wC, ekf->wG, ekf->wD) != 0)
    {
        return EKF_ERR_SINGULAR; /* x/P 未改动 */
    }

    return EKF_OK;
}

/*============================ 公开接口实现 ============================*/

Ekf_Status_e EkfInit(EkfInstance *ekf, const Ekf_Init_Config_s *cfg)
{
    if (ekf == NULL || cfg == NULL)
    {
        return EKF_ERR_NULL;
    }

    /* 维度合法性：1 ≤ n ≤ n_max，1 ≤ m ≤ m_max，0 ≤ l ≤ l_max */
    if (cfg->n < 1 || cfg->n > ekf->n_max || cfg->m < 1 || cfg->m > ekf->m_max || cfg->l > ekf->l_max)
    {
        return EKF_ERR_DIM;
    }

    const int n = cfg->n;
    const int m = cfg->m;
    const int l = cfg->l;
    ekf->n = (uint8_t)n;
    ekf->m = (uint8_t)m;
    ekf->l = (uint8_t)l;
    ekf->opt = cfg->opt;

    /* 回调 / 上下文 */
    ekf->ctx = cfg->ctx;
    ekf->f_fn = cfg->f_fn;
    ekf->F_fn = cfg->F_fn;
    ekf->h_fn = cfg->h_fn;
    ekf->H_fn = cfg->H_fn;

    /* 状态初值 x（缺省 0） */
    for (int i = 0; i < n; i++)
    {
        ekf->x[i] = (cfg->x0 != NULL) ? cfg->x0[i] : 0.0f;
    }

    /* 协方差初值 P（缺省单位阵） */
    if (cfg->P0 != NULL)
    {
        for (int i = 0; i < n * n; i++)
        {
            ekf->P[i] = cfg->P0[i];
        }
    }
    else
    {
        Lib_Math_MatSetEye(ekf->P, n, n);
    }

    /* 模型矩阵装载：缺省清零 */
    Ekf_LoadMat(ekf->F, cfg->F, n * n);
    Ekf_LoadMat(ekf->Q, cfg->Q, n * n);
    Ekf_LoadMat(ekf->H, cfg->H, m * n);
    Ekf_LoadMat(ekf->R, cfg->R, m * m);
    Ekf_LoadMat(ekf->B, cfg->B, n * l);

    return EKF_OK;
}

Ekf_Status_e EkfReset(EkfInstance *ekf)
{
    if (ekf == NULL)
    {
        return EKF_ERR_NULL;
    }
    if (ekf->n < 1 || ekf->n > ekf->n_max)
    {
        return EKF_ERR_DIM;
    }

    const int n = ekf->n;
    for (int i = 0; i < n; i++)
    {
        ekf->x[i] = 0.0f;
    }
    Lib_Math_MatSetEye(ekf->P, n, n);

    return EKF_OK;
}

Ekf_Status_e EkfSetState(EkfInstance *ekf, const float *x, const float *P)
{
    if (ekf == NULL || x == NULL)
    {
        return EKF_ERR_NULL;
    }
    if (ekf->n < 1 || ekf->n > ekf->n_max)
    {
        return EKF_ERR_DIM;
    }

    const int n = ekf->n;
    for (int i = 0; i < n; i++)
    {
        ekf->x[i] = x[i];
    }
    if (P != NULL)
    {
        for (int i = 0; i < n * n; i++)
        {
            ekf->P[i] = P[i];
        }
    }

    return EKF_OK;
}

Ekf_Status_e EkfPredict(EkfInstance *ekf, const float *u, float dt)
{
    if (ekf == NULL)
    {
        return EKF_ERR_NULL;
    }
    if (ekf->n < 1 || ekf->n > ekf->n_max)
    {
        return EKF_ERR_DIM;
    }

    const int n = ekf->n;
    const int l = ekf->l;
    float *x = ekf->x;
    float *F = ekf->F;

    /* ---------- 1. 先在**传播前**的状态上求雅可比（EKF 惯例） ---------- */
    if (ekf->F_fn != NULL)
    {
        ekf->F_fn(ekf->ctx, x, u, dt, F);
    }

    /* ---------- 2. 状态传播 ---------- */
    if (ekf->f_fn != NULL)
    {
        ekf->f_fn(ekf->ctx, x, u, dt, ekf->wE); /* 经 wE 暂存，避免与 x 别名 */
        for (int i = 0; i < n; i++)
        {
            x[i] = ekf->wE[i];
        }
    }
    else
    {
        /* 缺省线性退化：x⁺ = F·x + B·u */
        for (int i = 0; i < n; i++)
        {
            float acc = 0.0f;
            const float *frow = &F[i * n];
            for (int k = 0; k < n; k++)
            {
                acc += frow[k] * x[k];
            }
            ekf->wE[i] = acc;
        }
        if (l > 0 && u != NULL)
        {
            const float *B = ekf->B;
            for (int i = 0; i < n; i++)
            {
                float acc = 0.0f;
                const float *brow = &B[i * l];
                for (int k = 0; k < l; k++)
                {
                    acc += brow[k] * u[k];
                }
                ekf->wE[i] += acc;
            }
        }
        for (int i = 0; i < n; i++)
        {
            x[i] = ekf->wE[i];
        }
    }

    /* ---------- 3. P = F·P·Fᵀ + Q ---------- */
    Lib_KfCore_Predict(ekf->P, F, ekf->Q, n, ekf->wA, ekf->wB);

    return EKF_OK;
}

Ekf_Status_e EkfUpdate(EkfInstance *ekf, const float *z)
{
    if (ekf == NULL || z == NULL)
    {
        return EKF_ERR_NULL;
    }
    if (ekf->n < 1 || ekf->n > ekf->n_max || ekf->m < 1 || ekf->m > ekf->m_max)
    {
        return EKF_ERR_DIM;
    }

    return Ekf_UpdateCore(ekf, ekf->m, z);
}

Ekf_Status_e EkfUpdateM(EkfInstance *ekf, uint8_t m_now, const float *z)
{
    if (ekf == NULL || z == NULL)
    {
        return EKF_ERR_NULL;
    }
    if (ekf->n < 1 || ekf->n > ekf->n_max)
    {
        return EKF_ERR_DIM;
    }
    if (m_now < 1 || m_now > ekf->m_max)
    {
        return EKF_ERR_DIM;
    }

    return Ekf_UpdateCore(ekf, (int)m_now, z);
}

#endif /* LIB_EKF_USED */
