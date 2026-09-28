/**
 * @file lib_lkf.c
 * @brief 通用离散线性卡尔曼滤波(LKF)实现
 *
 * @note 纯算法模块，无硬件 / 时间依赖；使用单精度 float
 * @note 运行期任意 n 维状态 / m 维量测 / 可选 l 维控制输入，矩阵与工作区
 *       由 LKF_INSTANCE_DEF 按上限维数静态分配
 *
 * 算法流程（离散线性卡尔曼）：
 *   时间更新（预测）： x = F·x + B·u
 *                      P = F·P·Fᵀ + Q
 *   量测更新（校正）：  y = z - H·x
 *                      S = H·P·Hᵀ + R
 *                      K = P·Hᵀ·S⁻¹
 *                      x = x + K·y
 *                      P = (I - K·H)·P            （经典式）
 *                      P = (I-KH)·P·(I-KH)ᵀ + K·R·Kᵀ （Joseph 式，置 LKF_OPT_JOSEPH）
 *   协方差更新后均做对称化以抑制长期数值漂移。
 *
 * @note 协方差部分的代数已抽到 lib_kf_core.h，由 lib_lkf / lib_ekf / lib_eskf 共用；
 *       本文件只保留"线性"特有的两处：状态传播 x = F·x + B·u 与残差 y = z - H·x。
 *       内核的循环顺序与旧实现逐条对应，故重构前后结果逐位一致。
 */

#include "lib_lkf.h"
#include "lib_kf_core.h" /* 与 lib_ekf / lib_eskf 共用的 KF 代数内核 */
#include "lib_math.h"

#ifndef LIB_LKF_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_LKF_USED

/*============================ 私有函数声明 ============================*/

/**
 * @brief 内部量测更新核心（共享于 LkfUpdate / LkfUpdateM）
 * @note 只算线性残差 y = z - H·x，其余代数交给 Lib_KfCore_Update
 */
static Lkf_Status_e Kf_UpdateCore(LkfInstance *kf, int m_now, const float *z, const float *H, const float *R);

/*============================ 公开接口实现 ============================*/

Lkf_Status_e LkfInit(LkfInstance *kf, const Lkf_Init_Config_s *cfg)
{
    if (kf == NULL || cfg == NULL)
    {
        return LKF_ERR_NULL;
    }

    /* 维度合法性：1 ≤ n ≤ n_max，1 ≤ m ≤ m_max，0 ≤ l ≤ l_max */
    if (cfg->n < 1 || cfg->n > kf->n_max || cfg->m < 1 || cfg->m > kf->m_max || cfg->l > kf->l_max)
    {
        return LKF_ERR_DIM;
    }

    int n = cfg->n;
    int m = cfg->m;
    int l = cfg->l;
    kf->n = (uint8_t)n;
    kf->m = (uint8_t)m;
    kf->l = (uint8_t)l;
    kf->opt = cfg->opt;

    /* 状态初值 x（缺省 0） */
    for (int i = 0; i < n; i++)
    {
        kf->x[i] = (cfg->x0 != NULL) ? cfg->x0[i] : 0.0f;
    }

    /* 协方差初值 P（缺省单位阵） */
    if (cfg->P0 != NULL)
    {
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                kf->P[i * n + j] = cfg->P0[i * n + j];
            }
        }
    }
    else
    {
        Lib_Math_MatSetEye(kf->P, n, n);
    }

    /* 模型矩阵装载：缺省清零 */
    for (int i = 0; i < n * n; i++)
    {
        kf->F[i] = (cfg->F != NULL) ? cfg->F[i] : 0.0f;
        kf->Q[i] = (cfg->Q != NULL) ? cfg->Q[i] : 0.0f;
    }
    for (int i = 0; i < m * n; i++)
    {
        kf->H[i] = (cfg->H != NULL) ? cfg->H[i] : 0.0f;
    }
    for (int i = 0; i < m * m; i++)
    {
        kf->R[i] = (cfg->R != NULL) ? cfg->R[i] : 0.0f;
    }
    for (int i = 0; i < n * l; i++)
    {
        kf->B[i] = (cfg->B != NULL) ? cfg->B[i] : 0.0f;
    }

    return LKF_OK;
}

Lkf_Status_e LkfReset(LkfInstance *kf)
{
    if (kf == NULL)
    {
        return LKF_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return LKF_ERR_DIM;
    }

    int n = kf->n;
    for (int i = 0; i < n; i++)
    {
        kf->x[i] = 0.0f;
    }
    Lib_Math_MatSetEye(kf->P, n, n);

    return LKF_OK;
}

Lkf_Status_e LkfPredict(LkfInstance *kf, const float *u)
{
    if (kf == NULL)
    {
        return LKF_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return LKF_ERR_DIM;
    }

    const int n = kf->n;
    const int l = kf->l;
    float *x = kf->x;
    float *P = kf->P;
    const float *F = kf->F;
    const float *Q = kf->Q;
    float *wA = kf->wA;
    float *wB = kf->wB;
    float *wE = kf->wE;

    /* ---------- x_new = F·x (+ B·u)，经 wE 暂存避免就地覆盖源 ---------- */
    for (int i = 0; i < n; i++)
    {
        float acc = 0.0f;
        const float *frow = &F[i * n];
        for (int k = 0; k < n; k++)
        {
            acc += frow[k] * x[k];
        }
        wE[i] = acc;
    }
    if (l > 0 && u != NULL && kf->B != NULL) /* 控制输入项 B·u（B 维 n×l） */
    {
        const float *B = kf->B;
        for (int i = 0; i < n; i++)
        {
            float acc = 0.0f;
            const float *brow = &B[i * l];
            for (int k = 0; k < l; k++)
            {
                acc += brow[k] * u[k];
            }
            wE[i] += acc;
        }
    }

    /* ---------- P = F·P·Fᵀ + Q（代数走共享内核，与 lib_ekf / lib_eskf 同一份实现）---------- */
    Lib_KfCore_Predict(P, F, Q, n, wA, wB);

    /* 状态写回 */
    for (int i = 0; i < n; i++)
    {
        x[i] = wE[i];
    }

    return LKF_OK;
}

Lkf_Status_e LkfUpdate(LkfInstance *kf, const float *z)
{
    if (kf == NULL || z == NULL)
    {
        return LKF_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max || kf->m < 1 || kf->m > kf->m_max)
    {
        return LKF_ERR_DIM;
    }

    return Kf_UpdateCore(kf, kf->m, z, kf->H, kf->R);
}

Lkf_Status_e LkfUpdateM(LkfInstance *kf, uint8_t m_now, const float *z, const float *H, const float *R)
{
    if (kf == NULL || z == NULL || H == NULL || R == NULL)
    {
        return LKF_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return LKF_ERR_DIM;
    }
    if (m_now < 1 || m_now > kf->m_max)
    {
        return LKF_ERR_DIM;
    }

    return Kf_UpdateCore(kf, m_now, z, H, R);
}

/*============================ 私有函数实现 ============================*/

static Lkf_Status_e Kf_UpdateCore(LkfInstance *kf, int m_now, const float *z, const float *H, const float *R)
{
    const int n = kf->n;
    const int m = m_now;
    float *x = kf->x;
    float *P = kf->P;
    float *wF = kf->wF;

    /* ---------- innovation  y = z - H·x（wF）---------- */
    for (int i = 0; i < m; i++)
    {
        float hx = 0.0f;
        const float *hrow = &H[i * n];
        for (int k = 0; k < n; k++)
        {
            hx += hrow[k] * x[k];
        }
        wF[i] = z[i] - hx;
    }

    /* ---------- 其余代数（S/K/x/P、经典式或 Joseph、对称化）走共享内核 ---------- */
    if (Lib_KfCore_Update(x, P, n, m, H, R, wF, kf->opt, kf->wA, kf->wB, kf->wC, kf->wG, kf->wD) != 0)
    {
        return LKF_ERR_SINGULAR; /* x/P 未改动 */
    }

    return LKF_OK;
}

#endif /* LIB_LKF_USED */
