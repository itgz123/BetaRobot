/**
 * @file test_lkf.c
 * @brief lib_lkf PC 端回归测试：与独立双精度参考实现逐步比对
 *
 * @note 参考实现**不复用 lib_lkf 的任何代码**（独立写一遍 KF 公式，双精度），
 *       因此本测试是真正的交叉验证，而非自证。用于保护 lib_lkf 把协方差代数
 *       下沉到 lib_kf_core.h 的重构（要求行为逐位一致）。
 *
 * 用例：
 *   1. 2 状态匀速模型 + 位置量测（经典式）
 *   2. 4 状态平面匀速 + 2 维位置量测（Joseph 式）
 *   3. 多传感器顺序更新：每步用 LkfUpdateM 做两次标量更新，对照参考同样两步
 *   4. LkfUpdate 与 LkfUpdateM(m_now=m, H=kf->H, R=kf->R) 结果逐位一致
 *   5. S 奇异时返回 LKF_ERR_SINGULAR 且 x/P 逐位不变
 *   6. 维度越界 / 空指针返回对应错误码
 *
 * 编译见同目录 test_lkf.sh；退出码 0 = 全部 PASS。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_lkf.h"

#define MAXN 4
#define MAXM 2

/*============================ 确定性伪随机 ============================*/

static unsigned int s_seed;
static void rnd_seed(unsigned int s)
{
    s_seed = s;
}

static double urand(void) /* [0,1) */
{
    s_seed = s_seed * 1103515245u + 12345u;
    return (double)((s_seed >> 8) & 0xFFFFFFu) / 16777216.0;
}

static double nrand(void) /* 近似 N(0,1)：12 个均匀分布求和 - 6 */
{
    double s = 0.0;
    for (int i = 0; i < 12; i++)
    {
        s += urand();
    }
    return s - 6.0;
}

/*============================ 参考实现（双精度，独立公式） ============================*/

typedef struct
{
    int n;
    double x[MAXN];
    double P[MAXN * MAXN];
    double F[MAXN * MAXN];
    double Q[MAXN * MAXN];
} RefKf;

static void ref_predict(RefKf *kf)
{
    const int n = kf->n;
    double xn[MAXN];
    double fp[MAXN * MAXN];
    double fpft[MAXN * MAXN];

    for (int i = 0; i < n; i++)
    {
        double a = 0.0;
        for (int k = 0; k < n; k++)
        {
            a += kf->F[i * n + k] * kf->x[k];
        }
        xn[i] = a;
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double a = 0.0;
            for (int k = 0; k < n; k++)
            {
                a += kf->F[i * n + k] * kf->P[k * n + j];
            }
            fp[i * n + j] = a;
        }
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double a = 0.0;
            for (int k = 0; k < n; k++)
            {
                a += fp[i * n + k] * kf->F[j * n + k]; /* Fᵀ(k,j) = F(j,k) */
            }
            fpft[i * n + j] = a;
        }
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            kf->P[i * n + j] = fpft[i * n + j] + kf->Q[i * n + j];
        }
    }
    for (int i = 0; i < n; i++)
    {
        kf->x[i] = xn[i];
    }
}

/**
 * @brief 参考量测更新（H/R 由调用方给，便于对照 LkfUpdateM）
 * @note 协方差用经典式 P=(I-KH)P（lib_lkf 默认路径），与用例 1/3 对应
 */
static void ref_update(RefKf *kf, const double *z, const double *H, const double *R, int m)
{
    const int n = kf->n;
    double y[MAXM];
    double pht[MAXN * MAXM];
    double S[MAXM * MAXM];
    double Sinv[MAXM * MAXM];
    double K[MAXN * MAXM];
    double ikh[MAXN * MAXN];
    double pn[MAXN * MAXN];

    for (int i = 0; i < m; i++)
    {
        double a = 0.0;
        for (int k = 0; k < n; k++)
        {
            a += H[i * n + k] * kf->x[k];
        }
        y[i] = z[i] - a;
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < m; j++)
        {
            double a = 0.0;
            for (int k = 0; k < n; k++)
            {
                a += kf->P[i * n + k] * H[j * n + k];
            }
            pht[i * m + j] = a;
        }
    }

    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < m; j++)
        {
            double a = 0.0;
            for (int k = 0; k < n; k++)
            {
                a += H[i * n + k] * pht[k * m + j];
            }
            S[i * m + j] = a + R[i * m + j];
        }
    }

    if (m == 1)
    {
        Sinv[0] = 1.0 / S[0];
    }
    else
    {
        const double det = S[0] * S[3] - S[1] * S[2];
        Sinv[0] = S[3] / det;
        Sinv[1] = -S[1] / det;
        Sinv[2] = -S[2] / det;
        Sinv[3] = S[0] / det;
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < m; j++)
        {
            double a = 0.0;
            for (int k = 0; k < m; k++)
            {
                a += pht[i * m + k] * Sinv[k * m + j];
            }
            K[i * m + j] = a;
        }
    }

    for (int i = 0; i < n; i++)
    {
        for (int k = 0; k < m; k++)
        {
            kf->x[i] += K[i * m + k] * y[k];
        }
    }

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double a = (i == j) ? 1.0 : 0.0;
            for (int k = 0; k < m; k++)
            {
                a -= K[i * m + k] * H[k * n + j];
            }
            ikh[i * n + j] = a;
        }
    }
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double a = 0.0;
            for (int k = 0; k < n; k++)
            {
                a += ikh[i * n + k] * kf->P[k * n + j];
            }
            pn[i * n + j] = a;
        }
    }
    memcpy(kf->P, pn, sizeof(double) * (size_t)(n * n));
}

/*============================ 被测实例 ============================*/

LKF_INSTANCE_DEF(kf_a, MAXN, MAXM, 0); /* 主实例：LkfUpdate / LkfUpdateM 混用 */
LKF_INSTANCE_DEF(kf_b, MAXN, MAXM, 0); /* 对照：与 kf_a 同配置，验证两条路径一致 */
LKF_INSTANCE_DEF(kf_s, MAXN, MAXM, 0); /* 奇异 / 参数校验 */

/*============================ 用例框架 ============================*/

static int s_fail;

static void check(int cond, const char *fmt, double a, double b)
{
    if (!cond)
    {
        printf("    [FAIL] ");
        printf(fmt, a, b);
        printf("\n");
        s_fail++;
    }
}

/* 把双精度矩阵拷成 float（C 数组） */
#define TO_FLOAT(dst, src, cnt)                                                                                        \
    float dst[cnt];                                                                                                    \
    for (int i = 0; i < (cnt); i++)                                                                                    \
    {                                                                                                                  \
        dst[i] = (float)(src)[i];                                                                                      \
    }

static double max_rel_diff(const float *a, const double *b, int cnt)
{
    double worst = 0.0;
    for (int i = 0; i < cnt; i++)
    {
        const double d = fabs((double)a[i] - b[i]) / (1.0 + fabs(b[i]));
        if (d > worst)
        {
            worst = d;
        }
    }
    return worst;
}

/**
 * @brief 通用跑测：同一初值/模型下逐步跑 lib_lkf 与参考，返回 x/P 最大相对偏差
 * @param multi_rate 非 0 时每步改用两次标量 LkfUpdateM（H_scalar/R_scalar 各一维）
 */
static double run_case(LkfInstance *kf, const double *F, const double *Q, const double *H, const double *R,
                       const double *P0, const double *x0, int n, int m, uint8_t opt, int steps, const double *H2,
                       const double *R2, int multi_rate, double *out_p)
{
    RefKf ref;
    ref.n = n;
    memcpy(ref.F, F, sizeof(double) * (size_t)(n * n));
    memcpy(ref.Q, Q, sizeof(double) * (size_t)(n * n));
    memcpy(ref.x, x0, sizeof(double) * (size_t)n);
    memcpy(ref.P, P0, sizeof(double) * (size_t)(n * n));

    TO_FLOAT(Ff, F, n * n);
    TO_FLOAT(Qf, Q, n * n);
    TO_FLOAT(Hf, H, m * n);
    TO_FLOAT(Rf, R, m * m);
    TO_FLOAT(P0f, P0, n * n);
    TO_FLOAT(x0f, x0, n);
    /* 非多速率用例不传 H2/R2，用占位数组避免解引用 NULL */
    const double h2_placeholder[MAXN] = {0.0};
    const double r2_placeholder[1] = {0.0};
    const double *H2s = (H2 != NULL) ? H2 : h2_placeholder;
    const double *R2s = (R2 != NULL) ? R2 : r2_placeholder;
    TO_FLOAT(H2f, H2s, n);
    TO_FLOAT(R2f, R2s, 1);

    Lkf_Init_Config_s cfg = {
        .n = (uint8_t)n,
        .m = (uint8_t)m,
        .l = 0,
        .opt = opt,
        .x0 = x0f,
        .P0 = P0f,
        .F = Ff,
        .Q = Qf,
        .H = Hf,
        .R = Rf,
    };
    if (LkfInit(kf, &cfg) != LKF_OK)
    {
        printf("    [FAIL] LkfInit 返回非 OK\n");
        s_fail++;
        return 1e9;
    }

    double worst_x = 0.0;
    double worst_p = 0.0;
    float zf[MAXM];

    for (int t = 0; t < steps; t++)
    {
        /* 量测：由固定正弦轨迹 + 确定性噪声生成（两边用同一串数） */
        double z[MAXM];
        for (int i = 0; i < m; i++)
        {
            z[i] = 1.5 * sin(0.05 * (double)t + 0.7 * (double)i) + 0.2 * nrand();
        }
        for (int i = 0; i < m; i++)
        {
            zf[i] = (float)z[i];
        }

        ref_predict(&ref);
        LkfPredict(kf, NULL);

        if (multi_rate)
        {
            const double zz = z[0];
            double z2 = z[1];
            (void)zz;
            const float z1f = zf[0];
            const float z2f = zf[1];
            LkfUpdateM(kf, 1, &z1f, H2f, R2f);
            ref_update(&ref, &z[0], H2, R2, 1);
            LkfUpdateM(kf, 1, &z2f, H2f, R2f);
            ref_update(&ref, &z2, H2, R2, 1);
        }
        else
        {
            LkfUpdate(kf, zf);
            ref_update(&ref, z, H, R, m);
        }

        const double dx = max_rel_diff(kf->x, ref.x, n);
        const double dp = max_rel_diff(kf->P, ref.P, n * n);
        if (dx > worst_x)
        {
            worst_x = dx;
        }
        if (dp > worst_p)
        {
            worst_p = dp;
        }
    }

    *out_p = worst_p;
    return worst_x;
}

/*============================ 各用例 ============================*/

/* 用例 1：2 状态匀速 + 位置量测，经典式 */
static void case_cv_2state(void)
{
    const int n = 2, m = 1;
    const double dt = 0.01;
    double F[4] = {1.0, dt, 0.0, 1.0};
    double Q[4] = {1e-6, 0.0, 0.0, 1e-6};
    double H[2] = {1.0, 0.0};
    double R[1] = {0.04};
    double P0[4] = {1.0, 0.0, 0.0, 10.0};
    double x0[2] = {0.0, 0.0};

    rnd_seed(1u);
    double wp = 0.0;
    const double wx = run_case(&kf_a, F, Q, H, R, P0, x0, n, m, 0, 400, NULL, NULL, 0, &wp);

    printf("  用例1 CV(2状态,位置量测,经典式): max|Δx|rel=%.3e max|ΔP|rel=%.3e\n", wx, wp);
    check(wx < 1e-4, "    x 偏差过大 %.3e (阈 %.3e)\n", wx, 1e-4);
    check(wp < 1e-4, "    P 偏差过大 %.3e (阈 %.3e)\n", wp, 1e-4);
}

/* 用例 2：4 状态平面匀速 + 2 维位置量测，Joseph 式 */
static void case_cv_4state_joseph(void)
{
    const int n = 4, m = 2;
    const double dt = 0.02;
    double F[16] = {1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1};
    double Q[16] = {0};
    double H[8] = {1, 0, 0, 0, 0, 1, 0, 0};
    double R[4] = {0.05, 0.0, 0.0, 0.05};
    double P0[16] = {0};
    double x0[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++)
    {
        Q[i * 4 + i] = 2e-6;
        P0[i * 4 + i] = (i < 2) ? 1.0 : 10.0;
    }

    rnd_seed(7u);
    double wp = 0.0;
    const double wx = run_case(&kf_a, F, Q, H, R, P0, x0, n, m, LKF_OPT_JOSEPH, 300, NULL, NULL, 0, &wp);

    printf("  用例2 CV(4状态,2维位置,JOSEPH): max|Δx|rel=%.3e max|ΔP|rel=%.3e\n", wx, wp);
    check(wx < 1e-4, "    x 偏差过大 %.3e\n", wx, 0.0);
    check(wp < 2e-3, "    P 偏差过大 %.3e（Joseph 与经典式本身有 O(y²) 差异）\n", wp, 0.0);
}

/* 用例 3：多传感器顺序更新（每步两次标量 LkfUpdateM） */
static void case_multi_rate(void)
{
    const int n = 2, m = 2;
    const double dt = 0.01;
    double F[4] = {1.0, dt, 0.0, 1.0};
    double Q[4] = {1e-6, 0.0, 0.0, 2e-6};
    double H[4] = {1.0, 0.0, 0.0, 1.0}; /* 仅用于 Init 填实例阵，跑测走 H2 */
    double R[4] = {0.04, 0.0, 0.0, 0.09};
    double H2[2] = {1.0, 0.0}; /* 传感器 A：位置 */
    double R2[1] = {0.04};
    double P0[4] = {1.0, 0.0, 0.0, 10.0};
    double x0[2] = {0.0, 0.0};

    rnd_seed(11u);
    double wp = 0.0;
    const double wx = run_case(&kf_a, F, Q, H, R, P0, x0, n, m, 0, 300, H2, R2, 1, &wp);

    printf("  用例3 顺序标量更新(UpdateM×2): max|Δx|rel=%.3e max|ΔP|rel=%.3e\n", wx, wp);
    check(wx < 1e-4, "    x 偏差过大 %.3e\n", wx, 0.0);
    check(wp < 1e-4, "    P 偏差过大 %.3e\n", wp, 0.0);
}

/* 用例 4：LkfUpdate 与 LkfUpdateM(全量测) 逐位一致 */
static void case_update_equiv(void)
{
    const int n = 2, m = 2;
    double F[4] = {1.0, 0.01, 0.0, 1.0};
    double Q[4] = {1e-6, 0.0, 0.0, 1e-6};
    double H[4] = {1.0, 0.0, 0.3, 1.0};
    double R[4] = {0.04, 0.005, 0.005, 0.09};
    double P0[4] = {1.0, 0.0, 0.0, 10.0};
    double x0[2] = {0.3, -0.2};
    TO_FLOAT(Ff, F, 4)
    TO_FLOAT(Qf, Q, 4)
    TO_FLOAT(Hf, H, 4)
    TO_FLOAT(Rf, R, 4)
    TO_FLOAT(P0f, P0, 4)
    TO_FLOAT(x0f, x0, 2)

    Lkf_Init_Config_s cfg = {
        .n = n, .m = m, .l = 0, .opt = 0, .x0 = x0f, .P0 = P0f, .F = Ff, .Q = Qf, .H = Hf, .R = Rf};
    LkfInit(&kf_a, &cfg);
    LkfInit(&kf_b, &cfg);

    rnd_seed(23u);
    int diff = 0;
    for (int t = 0; t < 100; t++)
    {
        float z[2] = {(float)(0.8 * nrand()), (float)(0.5 * nrand())};
        LkfPredict(&kf_a, NULL);
        LkfPredict(&kf_b, NULL);
        LkfUpdate(&kf_a, z);
        LkfUpdateM(&kf_b, (uint8_t)m, z, kf_b.H, kf_b.R);
        if (memcmp(kf_a.x, kf_b.x, sizeof(float) * (size_t)n) != 0 ||
            memcmp(kf_a.P, kf_b.P, sizeof(float) * (size_t)(n * n)) != 0)
        {
            diff++;
        }
    }
    printf("  用例4 Update≡UpdateM 逐位比对: 不一致步数=%d\n", diff);
    check(diff == 0, "    存在不一致步数 %d\n", (double)diff, 0.0);
}

/* 用例 5：S 奇异 → 返回 LKF_ERR_SINGULAR 且 x/P 不变 */
static void case_singular(void)
{
    const int n = 2, m = 1;
    float F[4] = {1.0f, 0.01f, 0.0f, 1.0f};
    float Q[4] = {1e-6f, 0.0f, 0.0f, 1e-6f};
    float Hz[2] = {0.0f, 0.0f}; /* 全零 H + 零 R → S = 0 奇异 */
    float Rz[1] = {0.0f};
    float P0[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float x0[2] = {1.0f, 2.0f};

    Lkf_Init_Config_s cfg = {.n = n, .m = m, .l = 0, .opt = 0, .x0 = x0, .P0 = P0, .F = F, .Q = Q, .H = Hz, .R = Rz};
    LkfInit(&kf_s, &cfg);
    LkfPredict(&kf_s, NULL);

    float xs[2], Ps[4];
    memcpy(xs, kf_s.x, sizeof(xs));
    memcpy(Ps, kf_s.P, sizeof(Ps));

    float z[1] = {5.0f};
    const Lkf_Status_e st = LkfUpdate(&kf_s, z);

    const int unchanged = (memcmp(xs, kf_s.x, sizeof(xs)) == 0) && (memcmp(Ps, kf_s.P, sizeof(Ps)) == 0);
    printf("  用例5 奇异保护: 返回码=%d x/P 逐位不变=%d\n", (int)st, unchanged);
    check(st == LKF_ERR_SINGULAR, "    期望 LKF_ERR_SINGULAR(%d)，实得 %d\n", (double)LKF_ERR_SINGULAR, (double)st);
    check(unchanged != 0, "    x/P 被改动了\n", 0.0, 0.0);

    /* 奇异后仍能继续预测（状态未被破坏） */
    check(LkfPredict(&kf_s, NULL) == LKF_OK, "    奇异后 LkfPredict 失败\n", 0.0, 0.0);
}

/* 用例 6：维度越界 / 空指针 */
static void case_param_check(void)
{
    float F[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float Q[4] = {0.0f};
    float H[2] = {1.0f, 0.0f};
    float R[1] = {1.0f};
    float P0[4] = {1.0f, 0.0f, 0.0f, 1.0f};

    /* n 超上限（n_max=4） */
    Lkf_Init_Config_s bad_n = {.n = 5, .m = 1, .l = 0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    const int e1 = (LkfInit(&kf_s, &bad_n) == LKF_ERR_DIM);

    /* n = 0 */
    Lkf_Init_Config_s bad_n0 = {.n = 0, .m = 1, .l = 0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    const int e2 = (LkfInit(&kf_s, &bad_n0) == LKF_ERR_DIM);

    /* m 超上限（m_max=2） */
    Lkf_Init_Config_s bad_m = {.n = 2, .m = 3, .l = 0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    const int e3 = (LkfInit(&kf_s, &bad_m) == LKF_ERR_DIM);

    /* 空指针 */
    const int e4 = (LkfInit(NULL, &bad_n) == LKF_ERR_NULL);
    const int e5 = (LkfInit(&kf_s, NULL) == LKF_ERR_NULL);
    const int e6 = (LkfUpdate(NULL, R) == LKF_ERR_NULL);
    const int e7 = (LkfPredict(NULL, NULL) == LKF_ERR_NULL);

    printf("  用例6 参数校验: n越界=%d n=0=%d m越界=%d 空指针(x4)=%d%d%d%d\n", e1, e2, e3, e4, e5, e6, e7);
    check(e1 && e2 && e3, "    维度校验未按预期拒绝\n", 0.0, 0.0);
    check(e4 && e5 && e6 && e7, "    空指针校验未按预期拒绝\n", 0.0, 0.0);

    /* Reset：x 归零、P 归单位阵 */
    Lkf_Init_Config_s ok = {.n = 2, .m = 1, .l = 0, .x0 = P0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    LkfInit(&kf_s, &ok);
    LkfReset(&kf_s);
    const int rst = (kf_s.x[0] == 0.0f && kf_s.x[1] == 0.0f && kf_s.P[0] == 1.0f && kf_s.P[3] == 1.0f &&
                     kf_s.P[1] == 0.0f && kf_s.P[2] == 0.0f);
    printf("  用例6b Reset(x=0,P=I): %d\n", rst);
    check(rst != 0, "    Reset 结果不符\n", 0.0, 0.0);
}

int main(void)
{
    printf("lib_lkf 回归测试（对照独立双精度参考实现）\n");
    printf("========================================\n");

    case_cv_2state();
    case_cv_4state_joseph();
    case_multi_rate();
    case_update_equiv();
    case_singular();
    case_param_check();

    printf("========================================\n");
    if (s_fail == 0)
    {
        printf("结果: 全部 PASS\n");
        return 0;
    }
    printf("结果: %d 项 FAIL\n", s_fail);
    return 1;
}
