/**
 * @file lib_kf.c
 * @brief 通用离散线性卡尔曼滤波(LKF)实现
 *
 * @note 纯算法模块，无硬件 / 时间依赖；使用单精度 float
 * @note 运行期任意 n 维状态 / m 维量测 / 可选 l 维控制输入，矩阵与工作区
 *       由 KALMAN_INSTANCE_DEF 按上限维数静态分配
 *
 * 算法流程（离散线性卡尔曼）：
 *   时间更新（预测）： x = F·x + B·u
 *                      P = F·P·Fᵀ + Q
 *   量测更新（校正）：  y = z - H·x
 *                      S = H·P·Hᵀ + R
 *                      K = P·Hᵀ·S⁻¹
 *                      x = x + K·y
 *                      P = (I - K·H)·P            （经典式）
 *                      P = (I-KH)·P·(I-KH)ᵀ + K·R·Kᵀ （Joseph 式，置 KALMAN_OPT_JOSEPH）
 *   协方差更新后均做对称化以抑制长期数值漂移。
 */

#include "lib_kf.h"
#include "lib_math.h"
#include "app_cfg.h"

#ifdef LIB_KF_USED

/*============================ 私有函数声明 ============================*/

/**
 * @brief 内部量测更新核心（共享于 KalmanUpdate / KalmanUpdateM）
 * @note 工作区 wC/wG(n×m)、wD(增广)、wA/wB(n×n)、wF(m) 均已由 DEF 分配
 */
static Kalman_Status_e Kf_UpdateCore(KalmanInstance *kf, int m_now, const float *z, const float *H, const float *R);

/*============================ 公开接口实现 ============================*/

Kalman_Status_e KalmanInit(KalmanInstance *kf, const Kalman_Init_Config_s *cfg)
{
    if (kf == NULL || cfg == NULL)
    {
        return KALMAN_ERR_NULL;
    }

    /* 维度合法性：1 ≤ n ≤ n_max，1 ≤ m ≤ m_max，0 ≤ l ≤ l_max */
    if (cfg->n < 1 || cfg->n > kf->n_max ||
        cfg->m < 1 || cfg->m > kf->m_max ||
        cfg->l > kf->l_max)
    {
        return KALMAN_ERR_DIM;
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

    return KALMAN_OK;
}

Kalman_Status_e KalmanReset(KalmanInstance *kf)
{
    if (kf == NULL)
    {
        return KALMAN_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return KALMAN_ERR_DIM;
    }

    int n = kf->n;
    for (int i = 0; i < n; i++)
    {
        kf->x[i] = 0.0f;
    }
    Lib_Math_MatSetEye(kf->P, n, n);

    return KALMAN_OK;
}

Kalman_Status_e KalmanPredict(KalmanInstance *kf, const float *u)
{
    if (kf == NULL)
    {
        return KALMAN_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return KALMAN_ERR_DIM;
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

    /* ---------- P = F·P·Fᵀ + Q ---------- */
    /* wA = F·P */
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float acc = 0.0f;
            const float *frow = &F[i * n];
            for (int k = 0; k < n; k++)
            {
                acc += frow[k] * P[k * n + j];
            }
            wA[i * n + j] = acc;
        }
    }
    /* wB = wA·Fᵀ：wB[i][j] = Σ_k wA[i][k]·F[j][k] */
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float acc = 0.0f;
            const float *arow = &wA[i * n];
            const float *frow = &F[j * n];
            for (int k = 0; k < n; k++)
            {
                acc += arow[k] * frow[k];
            }
            wB[i * n + j] = acc;
        }
    }
    /* P = wB + Q 并对称化 */
    for (int i = 0; i < n; i++)
    {
        for (int j = i; j < n; j++)
        {
            float v = wB[i * n + j] + Q[i * n + j];
            P[i * n + j] = v;
            P[j * n + i] = v;
        }
    }

    /* 状态写回 */
    for (int i = 0; i < n; i++)
    {
        x[i] = wE[i];
    }

    return KALMAN_OK;
}

Kalman_Status_e KalmanUpdate(KalmanInstance *kf, const float *z)
{
    if (kf == NULL || z == NULL)
    {
        return KALMAN_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max ||
        kf->m < 1 || kf->m > kf->m_max)
    {
        return KALMAN_ERR_DIM;
    }

    return Kf_UpdateCore(kf, kf->m, z, kf->H, kf->R);
}

Kalman_Status_e KalmanUpdateM(KalmanInstance *kf, uint8_t m_now, const float *z, const float *H, const float *R)
{
    if (kf == NULL || z == NULL || H == NULL || R == NULL)
    {
        return KALMAN_ERR_NULL;
    }
    if (kf->n < 1 || kf->n > kf->n_max)
    {
        return KALMAN_ERR_DIM;
    }
    if (m_now < 1 || m_now > kf->m_max)
    {
        return KALMAN_ERR_DIM;
    }

    return Kf_UpdateCore(kf, m_now, z, H, R);
}

/*============================ 私有函数实现 ============================*/

static Kalman_Status_e Kf_UpdateCore(KalmanInstance *kf, int m_now, const float *z, const float *H, const float *R)
{
    const int n = kf->n;
    const int m = m_now;
    float *x = kf->x;
    float *P = kf->P;
    float *wA = kf->wA;
    float *wB = kf->wB;
    float *wC = kf->wC;
    float *wG = kf->wG;
    float *wD = kf->wD;
    float *wF = kf->wF;
    const int step = 2 * m; /* 增广矩阵行距 */

    /* ---------- 1. innovation  y = z - H·x（wF） ---------- */
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

    /* ---------- 2. wC = P·Hᵀ（n×m） ---------- */
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

    /* ---------- 3. 构建 S = H·wC + R 到 wD 左侧 m×m 块 ---------- */
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

    /* ---------- 4. 增广 [S|I] Gauss-Jordan 求 S⁻¹；奇异则跳过本次更新 ---------- */
    if (Lib_Math_MatInvGaussJordan(wD, m, step) != 0)
    {
        return KALMAN_ERR_SINGULAR; /* x/P 尚未改动 */
    }
    const float *sinv = &wD[m]; /* 增广矩阵右侧块 = S⁻¹，行距 step */

    /* ---------- 5. K = wC·S⁻¹（wG，n×m） ---------- */
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

    /* ---------- 6. x := x + K·y ---------- */
    for (int i = 0; i < n; i++)
    {
        float acc = x[i];
        const float *krow = &wG[i * m];
        for (int k = 0; k < m; k++)
        {
            acc += krow[k] * wF[k];
        }
        x[i] = acc;
    }

    /* ---------- 7. 协方差更新 ---------- */
    /* wA = I - K·H */
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
    /* wB = wA·P（使用更新前的 P） */
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

    if (kf->opt & KALMAN_OPT_JOSEPH)
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

    /* ---------- 8. P 对称化（回写下三角） ---------- */
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < i; j++)
        {
            float v = 0.5f * (P[i * n + j] + P[j * n + i]);
            P[i * n + j] = v;
            P[j * n + i] = v;
        }
    }

    return KALMAN_OK;
}

#endif /* LIB_KF_USED */
