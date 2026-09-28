/**
 * @file test_ekf.c
 * @brief lib_ekf PC 端测试
 *
 * 用例：
 *   1. 线性退化：f/h 回调写成 F·x、H·x 时，结果与 lib_lkf **逐位一致**
 *      （lib_lkf 已由 tools/test_lkf.c 对照独立双精度参考验证过，故本项是间接交叉验证）
 *   2. 非线性单摆：量测 y = sin(θ)，θ 估计误差 < 0.02 rad 且 NIS 均值落在 [0.5, 2]
 *   3. 2D 雷达 range/bearing：位置 RMSE < 0.05 m
 *   4. 部分量测：两次 EkfUpdateM(1) 与一次 EkfUpdate(2) 一致（对角 R 下等价）
 *   5. S 奇异：返回 EKF_ERR_SINGULAR 且 x/P 逐位不变
 *   6. 病态 P0 + Joseph：P 保持对称正定
 *   7. 维度越界 / 空指针 / h 回调缺失返回对应错误码
 *   8. ctx 原样透传（回调里计数）
 *
 * 编译见同目录 test_ekf.sh；退出码 0 = 全部 PASS。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_ekf.h"
#include "lib_lkf.h"

/*============================ 确定性伪随机 ============================*/

static unsigned int s_seed;
static void rnd_seed(unsigned int s)
{
    s_seed = s;
}
static double urand(void)
{
    s_seed = s_seed * 1103515245u + 12345u;
    return (double)((s_seed >> 8) & 0xFFFFFFu) / 16777216.0;
}
static double nrand(void)
{
    double s = 0.0;
    for (int i = 0; i < 12; i++)
    {
        s += urand();
    }
    return s - 6.0;
}

static int s_fail;
static void check(int cond, const char *msg)
{
    if (!cond)
    {
        printf("    [FAIL] %s\n", msg);
        s_fail++;
    }
}

/*============================ 回调：线性退化 ============================*/

/* ctx = 实例指针：直接用实例内的常值 F / H 阵，累加顺序与 lib_lkf 完全相同 */
static void lin_f(void *ctx, const float *x, const float *u, float dt, float *x_next)
{
    (void)u;
    (void)dt;
    const EkfInstance *e = (const EkfInstance *)ctx;
    const int n = e->n;
    for (int i = 0; i < n; i++)
    {
        float acc = 0.0f;
        const float *frow = &e->F[i * n];
        for (int k = 0; k < n; k++)
        {
            acc += frow[k] * x[k];
        }
        x_next[i] = acc;
    }
}

static void lin_h(void *ctx, const float *x, int m_now, float *z_pred)
{
    const EkfInstance *e = (const EkfInstance *)ctx;
    const int n = e->n;
    for (int i = 0; i < m_now; i++)
    {
        float acc = 0.0f;
        const float *hrow = &e->H[i * n];
        for (int k = 0; k < n; k++)
        {
            acc += hrow[k] * x[k];
        }
        z_pred[i] = acc;
    }
}

/* 带通道选择 + 调用计数的线性回调（用例 4 / 6 / 7 / 8）
 * H 行源放在 ctx 里而不是读实例阵：部分量测会把第 sel 行搬到第 0 行，
 * 若从实例阵取源会被自己改写的副本污染。 */
typedef struct
{
    const EkfInstance *e;
    const float *Hsrc; /* 原始 H 行源（m_max×n，行距 n） */
    int sel;           /* >=0：只用第 sel 行作单通道；-1：输出前 m_now 行 */
    int h_calls;       /* h 回调调用次数 */
    int H_calls;       /* H 回调调用次数 */
} SelCtx;

static void sel_h(void *ctx, const float *x, int m_now, float *z_pred)
{
    SelCtx *c = (SelCtx *)ctx;
    const int n = c->e->n;
    c->h_calls++;
    if (c->sel >= 0)
    {
        float acc = 0.0f;
        const float *hrow = &c->Hsrc[c->sel * n];
        for (int k = 0; k < n; k++)
        {
            acc += hrow[k] * x[k];
        }
        z_pred[0] = acc;
        return;
    }
    for (int i = 0; i < m_now; i++)
    {
        float acc = 0.0f;
        const float *hrow = &c->Hsrc[i * n];
        for (int k = 0; k < n; k++)
        {
            acc += hrow[k] * x[k];
        }
        z_pred[i] = acc;
    }
}

static void sel_H(void *ctx, const float *x, int m_now, float *H)
{
    (void)x;
    SelCtx *c = (SelCtx *)ctx;
    const int n = c->e->n;
    c->H_calls++;
    if (c->sel >= 0)
    {
        for (int k = 0; k < n; k++)
        {
            H[k] = c->Hsrc[c->sel * n + k]; /* 第 sel 行搬到第 0 行 */
        }
        return;
    }
    for (int i = 0; i < m_now; i++)
    {
        for (int k = 0; k < n; k++)
        {
            H[i * n + k] = c->Hsrc[i * n + k];
        }
    }
}

/*============================ 实例 ============================*/

EKF_INSTANCE_DEF(ekf_lin, 2, 2, 0);
EKF_INSTANCE_DEF(ekf_jos, 2, 2, 0);
EKF_INSTANCE_DEF(ekf_pend, 2, 1, 0);
EKF_INSTANCE_DEF(ekf_radar, 4, 2, 0);
EKF_INSTANCE_DEF(ekf_seq, 2, 2, 0);
EKF_INSTANCE_DEF(ekf_sing, 2, 1, 0);
EKF_INSTANCE_DEF(ekf_bad, 2, 1, 0);
LKF_INSTANCE_DEF(kf_ref, 2, 2, 0);

/*============================ 用例 1：线性退化 ≡ lib_lkf ============================*/

static void case_linear_equivalent(void)
{
    const int n = 2, m = 2;
    const double dt = 0.01;
    float F[4] = {1.0f, (float)dt, 0.0f, 1.0f};
    float Q[4] = {1e-6f, 0.0f, 0.0f, 1e-6f};
    float H[4] = {1.0f, 0.0f, 0.3f, 1.0f};
    float R[4] = {0.04f, 0.0f, 0.0f, 0.09f};
    float P0[4] = {1.0f, 0.0f, 0.0f, 10.0f};
    float x0[2] = {0.2f, -0.1f};

    Ekf_Init_Config_s ecfg = {.n = n,
                              .m = m,
                              .l = 0,
                              .opt = 0,
                              .ctx = &ekf_lin,
                              .f_fn = lin_f,
                              .F_fn = NULL,
                              .h_fn = lin_h,
                              .H_fn = NULL,
                              .x0 = x0,
                              .P0 = P0,
                              .F = F,
                              .Q = Q,
                              .H = H,
                              .R = R};
    check(EkfInit(&ekf_lin, &ecfg) == EKF_OK, "EkfInit 失败");

    Lkf_Init_Config_s kcfg = {.n = n, .m = m, .l = 0, .opt = 0, .x0 = x0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    check(LkfInit(&kf_ref, &kcfg) == LKF_OK, "LkfInit 失败");

    rnd_seed(31u);
    int bad = 0;
    for (int t = 0; t < 150; t++)
    {
        float z[2] = {(float)(0.7 * nrand()), (float)(0.4 * nrand())};
        EkfPredict(&ekf_lin, NULL, (float)dt);
        LkfPredict(&kf_ref, NULL);
        EkfUpdate(&ekf_lin, z);
        LkfUpdate(&kf_ref, z);
        if (memcmp(ekf_lin.x, kf_ref.x, sizeof(float) * (size_t)n) != 0 ||
            memcmp(ekf_lin.P, kf_ref.P, sizeof(float) * (size_t)(n * n)) != 0)
        {
            bad++;
        }
    }
    printf("  用例1 线性退化 vs lib_lkf 逐位比对: 不一致步数=%d\n", bad);
    check(bad == 0, "线性退化与 lib_lkf 不完全一致");
}

/*============================ 用例 2：非线性单摆 ============================*/

/*
 * 连续模型： dθ/dt = ω ； dω/dt = -(g/L)·sinθ - c·ω
 * 真值用 RK4 积分（每滤波步 10 个子步），滤波器用 dt 步长的欧拉离散
 * （模型失配 O(dt²) 即过程噪声 Q 的物理来源）
 * 量测： y = sin(θ)   —— 非线性，H = [cosθ, 0]
 */
#define PEND_G_OVER_L 9.81f
#define PEND_DAMP 0.1f
#define PEND_DT 0.01f

typedef struct
{
    float z_noise;
    int nis_cnt;
    double nis_sum;
} PendCtx;

static void pend_f(void *ctx, const float *x, const float *u, float dt, float *x_next)
{
    (void)ctx;
    (void)u;
    x_next[0] = x[0] + x[1] * dt;
    x_next[1] = x[1] + (-PEND_G_OVER_L * sinf(x[0]) - PEND_DAMP * x[1]) * dt;
}

static void pend_F(void *ctx, const float *x, const float *u, float dt, float *F)
{
    (void)ctx;
    (void)u;
    F[0] = 1.0f;
    F[1] = dt;
    F[2] = -PEND_G_OVER_L * cosf(x[0]) * dt;
    F[3] = 1.0f - PEND_DAMP * dt;
}

static void pend_h(void *ctx, const float *x, int m_now, float *z_pred)
{
    (void)ctx;
    (void)m_now;
    z_pred[0] = sinf(x[0]);
}

static void pend_H(void *ctx, const float *x, int m_now, float *H)
{
    (void)ctx;
    (void)m_now;
    H[0] = cosf(x[0]);
    H[1] = 0.0f;
}

static void pend_rk4(float *s, float dt)
{
    float k1[2], k2[2], k3[2], k4[2], tmp[2];
    /* k1 */
    k1[0] = s[1];
    k1[1] = -PEND_G_OVER_L * sinf(s[0]) - PEND_DAMP * s[1];
    /* k2 */
    tmp[0] = s[0] + 0.5f * dt * k1[0];
    tmp[1] = s[1] + 0.5f * dt * k1[1];
    k2[0] = tmp[1];
    k2[1] = -PEND_G_OVER_L * sinf(tmp[0]) - PEND_DAMP * tmp[1];
    /* k3 */
    tmp[0] = s[0] + 0.5f * dt * k2[0];
    tmp[1] = s[1] + 0.5f * dt * k2[1];
    k3[0] = tmp[1];
    k3[1] = -PEND_G_OVER_L * sinf(tmp[0]) - PEND_DAMP * tmp[1];
    /* k4 */
    tmp[0] = s[0] + dt * k3[0];
    tmp[1] = s[1] + dt * k3[1];
    k4[0] = tmp[1];
    k4[1] = -PEND_G_OVER_L * sinf(tmp[0]) - PEND_DAMP * tmp[1];

    s[0] += dt / 6.0f * (k1[0] + 2.0f * k2[0] + 2.0f * k3[0] + k4[0]);
    s[1] += dt / 6.0f * (k1[1] + 2.0f * k2[1] + 2.0f * k3[1] + k4[1]);
}

static void case_pendulum(void)
{
    PendCtx ctx = {.z_noise = 0.01f};
    float Q[4] = {1e-6f, 0.0f, 0.0f, 1e-4f};
    float R[1] = {0.01f * 0.01f};
    float P0[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float x0[2] = {0.0f, 0.0f}; /* 初值故意偏离真值 θ=0.5 */

    Ekf_Init_Config_s cfg = {.n = 2,
                             .m = 1,
                             .l = 0,
                             .opt = 0,
                             .ctx = &ctx,
                             .f_fn = pend_f,
                             .F_fn = pend_F,
                             .h_fn = pend_h,
                             .H_fn = pend_H,
                             .x0 = x0,
                             .P0 = P0,
                             .Q = Q,
                             .R = R};
    check(EkfInit(&ekf_pend, &cfg) == EKF_OK, "EkfInit 失败");

    float truth[2] = {0.5f, 0.0f};
    const int steps = 800;
    float worst_tail = 0.0f;

    rnd_seed(101u);
    for (int t = 0; t < steps; t++)
    {
        /* 真值积分（10 个 RK4 子步） */
        for (int s = 0; s < 10; s++)
        {
            pend_rk4(truth, PEND_DT / 10.0f);
        }
        const float z = sinf(truth[0]) + (float)(ctx.z_noise * nrand());

        EkfPredict(&ekf_pend, NULL, PEND_DT);

        /* NIS（m=1）：预测之后、更新之前，用预测的 x/P 手算 y 与 S */
        const float th = ekf_pend.x[0];
        const float y = z - sinf(th);
        const float h0 = cosf(th);
        const float S = h0 * EKF_P(&ekf_pend, 0, 0) * h0 + R[0];
        ctx.nis_sum += (double)(y * y / S);
        ctx.nis_cnt++;

        EkfUpdate(&ekf_pend, &z);

        if (t >= steps - 100)
        {
            const float e = fabsf(ekf_pend.x[0] - truth[0]);
            if (e > worst_tail)
            {
                worst_tail = e;
            }
        }
    }

    const double nis_mean = ctx.nis_sum / (double)ctx.nis_cnt;
    printf("  用例2 单摆: 末段 max|Δθ|=%.4f rad  NIS 均值=%.3f  末端 θ_est=%.4f θ_true=%.4f\n", worst_tail, nis_mean,
           ekf_pend.x[0], truth[0]);
    check(worst_tail < 0.02f, "单摆 θ 稳态误差超限");
    check(nis_mean > 0.5 && nis_mean < 2.0, "单摆 NIS 均值不在 [0.5, 2]");
}

/*============================ 用例 3：2D 雷达 range/bearing ============================*/

typedef struct
{
    float r_noise;
    float b_noise;
    int sel; /* 见 SelCtx 说明；-1 = 全量测 */
} RadarCtx;

static void radar_f(void *ctx, const float *x, const float *u, float dt, float *x_next)
{
    (void)ctx;
    (void)u;
    x_next[0] = x[0] + x[2] * dt;
    x_next[1] = x[1] + x[3] * dt;
    x_next[2] = x[2];
    x_next[3] = x[3];
}

static void radar_F(void *ctx, const float *x, const float *u, float dt, float *F)
{
    (void)ctx;
    (void)u;
    (void)x;
    memset(F, 0, sizeof(float) * 16u);
    F[0] = 1.0f;
    F[2] = dt;
    F[5] = 1.0f;
    F[7] = dt;
    F[10] = 1.0f;
    F[15] = 1.0f;
}

static void radar_h(void *ctx, const float *x, int m_now, float *z_pred)
{
    RadarCtx *c = (RadarCtx *)ctx;
    const float px = x[0], py = x[1];
    const float r = sqrtf(px * px + py * py);
    if (c->sel == 0)
    {
        z_pred[0] = r;
        return;
    }
    if (c->sel == 1)
    {
        z_pred[0] = atan2f(py, px);
        return;
    }
    (void)m_now;
    z_pred[0] = r;
    z_pred[1] = atan2f(py, px);
}

static void radar_H(void *ctx, const float *x, int m_now, float *H)
{
    RadarCtx *c = (RadarCtx *)ctx;
    const float px = x[0], py = x[1];
    const float r2 = px * px + py * py;
    const float r = sqrtf(r2);
    float h0[4], h1[4];
    for (int k = 0; k < 4; k++)
    {
        h0[k] = 0.0f;
        h1[k] = 0.0f;
    }
    if (r > 1e-6f)
    {
        h0[0] = px / r;
        h0[1] = py / r;
        h1[0] = -py / r2;
        h1[1] = px / r2;
    }
    if (c->sel == 0)
    {
        for (int k = 0; k < 4; k++)
        {
            H[k] = h0[k];
        }
        return;
    }
    if (c->sel == 1)
    {
        for (int k = 0; k < 4; k++)
        {
            H[k] = h1[k];
        }
        return;
    }
    (void)m_now;
    for (int k = 0; k < 4; k++)
    {
        H[k] = h0[k];
        H[4 + k] = h1[k];
    }
}

static void case_radar(void)
{
    RadarCtx ctx = {.r_noise = 0.04f, .b_noise = 0.002f, .sel = -1};
    float Q[16] = {0};
    float R[4] = {0.04f * 0.04f, 0.0f, 0.0f, 0.002f * 0.002f};
    float P0[16] = {0};
    float x0[4] = {12.0f, 8.0f, 0.5f, 0.4f}; /* 初值与真值有偏差 */
    for (int i = 0; i < 4; i++)
    {
        Q[i * 4 + i] = 1e-8f;
        P0[i * 4 + i] = 1.0f;
    }

    Ekf_Init_Config_s cfg = {.n = 4,
                             .m = 2,
                             .l = 0,
                             .opt = EKF_OPT_JOSEPH,
                             .ctx = &ctx,
                             .f_fn = radar_f,
                             .F_fn = radar_F,
                             .h_fn = radar_h,
                             .H_fn = radar_H,
                             .x0 = x0,
                             .P0 = P0,
                             .Q = Q,
                             .R = R};
    check(EkfInit(&ekf_radar, &cfg) == EKF_OK, "EkfInit 失败");

    float truth[4] = {10.0f, 5.0f, 1.0f, 0.5f};
    const float dt = 0.02f;
    const int steps = 600;
    double se = 0.0;
    int cnt = 0;

    rnd_seed(2023u);
    for (int t = 0; t < steps; t++)
    {
        truth[0] += truth[2] * dt;
        truth[1] += truth[3] * dt;

        const float r = sqrtf(truth[0] * truth[0] + truth[1] * truth[1]);
        const float b = atan2f(truth[1], truth[0]);
        float z[2];
        z[0] = r + (float)(ctx.r_noise * nrand());
        z[1] = b + (float)(ctx.b_noise * nrand());

        EkfPredict(&ekf_radar, NULL, dt);
        EkfUpdate(&ekf_radar, z);

        if (t >= 200)
        {
            const float ex = ekf_radar.x[0] - truth[0];
            const float ey = ekf_radar.x[1] - truth[1];
            se += (double)(ex * ex + ey * ey);
            cnt++;
        }
    }
    const double rmse = sqrt(se / (double)cnt);
    printf("  用例3 雷达: 位置 RMSE=%.4f m（阈 0.05）\n", rmse);
    check(rmse < 0.05, "雷达位置 RMSE 超限");
}

/*============================ 用例 4：部分量测 ============================*/

static void case_partial(void)
{
    const int n = 2, m = 2;
    float F[4] = {1.0f, 0.01f, 0.0f, 1.0f};
    float Q[4] = {1e-6f, 0.0f, 0.0f, 1e-6f};
    float H[4] = {1.0f, 0.0f, 0.0f, 1.0f};    /* 行0 测位置、行1 测速度 */
    float Rb[4] = {0.04f, 0.0f, 0.0f, 0.09f}; /* 对角 → 顺序更新与批更新等价 */
    float P0[4] = {1.0f, 0.0f, 0.0f, 10.0f};
    float x0[2] = {0.0f, 0.0f};

    SelCtx ctx = {.e = &ekf_seq, .Hsrc = H, .sel = -1};
    Ekf_Init_Config_s cfg = {.n = n,
                             .m = m,
                             .l = 0,
                             .opt = 0,
                             .ctx = &ctx,
                             .f_fn = NULL,
                             .F_fn = NULL,
                             .h_fn = sel_h,
                             .H_fn = sel_H,
                             .x0 = x0,
                             .P0 = P0,
                             .F = F,
                             .Q = Q,
                             .H = H,
                             .R = Rb};
    check(EkfInit(&ekf_seq, &cfg) == EKF_OK, "EkfInit 失败");

    /* 对照实例：同样的常量模型，但走一次批更新 Update(2) */
    SelCtx ctx2 = {.e = &ekf_jos, .Hsrc = H, .sel = -1};
    Ekf_Init_Config_s cfg2 = cfg;
    cfg2.ctx = &ctx2;
    cfg2.f_fn = NULL;
    cfg2.h_fn = sel_h;
    cfg2.H_fn = sel_H;
    check(EkfInit(&ekf_jos, &cfg2) == EKF_OK, "EkfInit 失败");

    rnd_seed(55u);
    double worst = 0.0;
    for (int t = 0; t < 150; t++)
    {
        float z[2] = {(float)(0.6 * nrand()), (float)(0.3 * nrand())};

        EkfPredict(&ekf_seq, NULL, 0.01f);
        EkfPredict(&ekf_jos, NULL, 0.01f);

        /* 顺序：先位置通道（sel=0，R 取 r00），再速度通道（sel=1，R 取 r11） */
        ctx.sel = 0;
        EKF_R(&ekf_seq, 0, 0) = Rb[0];
        EkfUpdateM(&ekf_seq, 1, &z[0]);
        ctx.sel = 1;
        EKF_R(&ekf_seq, 0, 0) = Rb[3];
        EkfUpdateM(&ekf_seq, 1, &z[1]);
        ctx.sel = -1;

        EkfUpdate(&ekf_jos, z);

        for (int i = 0; i < n; i++)
        {
            const double d = fabs((double)ekf_seq.x[i] - (double)ekf_jos.x[i]);
            if (d > worst)
            {
                worst = d;
            }
        }
        for (int i = 0; i < n * n; i++)
        {
            const double d = fabs((double)ekf_seq.P[i] - (double)ekf_jos.P[i]);
            if (d > worst)
            {
                worst = d;
            }
        }
    }
    printf("  用例4 部分量测: 与批更新 max|Δ|=%.3e（阈 1e-4）\n", worst);
    check(worst < 1e-4, "顺序标量更新与批更新不一致");
}

/*============================ 用例 5：S 奇异 ============================*/

static void sing_h(void *ctx, const float *x, int m_now, float *z_pred)
{
    (void)ctx;
    (void)x;
    (void)m_now;
    z_pred[0] = 0.0f;
}

static void sing_H(void *ctx, const float *x, int m_now, float *H)
{
    (void)ctx;
    (void)x;
    (void)m_now;
    H[0] = 0.0f;
    H[1] = 0.0f;
}

static void case_singular(void)
{
    float F[4] = {1.0f, 0.01f, 0.0f, 1.0f};
    float Q[4] = {1e-6f, 0.0f, 0.0f, 1e-6f};
    float R[1] = {0.0f}; /* 零 R + 零 H → S = 0 */
    float P0[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float x0[2] = {1.0f, 2.0f};

    Ekf_Init_Config_s cfg = {.n = 2,
                             .m = 1,
                             .l = 0,
                             .opt = 0,
                             .ctx = NULL,
                             .f_fn = NULL,
                             .F_fn = NULL,
                             .h_fn = sing_h,
                             .H_fn = sing_H,
                             .x0 = x0,
                             .P0 = P0,
                             .F = F,
                             .Q = Q,
                             .H = NULL,
                             .R = R};
    check(EkfInit(&ekf_sing, &cfg) == EKF_OK, "EkfInit 失败");
    EkfPredict(&ekf_sing, NULL, 0.01f);

    float xs[2], Ps[4];
    memcpy(xs, ekf_sing.x, sizeof(xs));
    memcpy(Ps, ekf_sing.P, sizeof(Ps));

    const float z = 5.0f;
    const Ekf_Status_e st = EkfUpdate(&ekf_sing, &z);
    const int unchanged = (memcmp(xs, ekf_sing.x, sizeof(xs)) == 0) && (memcmp(Ps, ekf_sing.P, sizeof(Ps)) == 0);
    printf("  用例5 奇异保护: 返回码=%d x/P 逐位不变=%d\n", (int)st, unchanged);
    check(st == EKF_ERR_SINGULAR, "期望 EKF_ERR_SINGULAR");
    check(unchanged != 0, "奇异时 x/P 被改动");
}

/*============================ 用例 6：病态 P0 + Joseph ============================*/

static void case_joseph_pd(void)
{
    float F[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float Q[4] = {1e-12f, 0.0f, 0.0f, 1e-12f};
    float H[2] = {1.0f, 0.0f};
    float R[1] = {1e-9f};
    float P0[4] = {1e-6f, 0.0f, 0.0f, 1e6f}; /* 条件数 1e12，量测只观测 x0 */

    SelCtx ctx = {.e = &ekf_jos, .Hsrc = H, .sel = -1};
    Ekf_Init_Config_s cfg = {.n = 2,
                             .m = 1,
                             .l = 0,
                             .opt = EKF_OPT_JOSEPH,
                             .ctx = &ctx,
                             .f_fn = NULL,
                             .F_fn = NULL,
                             .h_fn = sel_h,
                             .H_fn = NULL,
                             .x0 = NULL,
                             .P0 = P0,
                             .F = F,
                             .Q = Q,
                             .H = H,
                             .R = R};
    check(EkfInit(&ekf_jos, &cfg) == EKF_OK, "EkfInit 失败");

    rnd_seed(77u);
    int pd_bad = 0, sym_bad = 0;
    for (int t = 0; t < 100; t++)
    {
        const float z = (float)(0.1 * nrand());
        EkfPredict(&ekf_jos, NULL, 0.0f);
        EkfUpdate(&ekf_jos, &z);

        const float p00 = EKF_P(&ekf_jos, 0, 0);
        const float p11 = EKF_P(&ekf_jos, 1, 1);
        const float p01 = EKF_P(&ekf_jos, 0, 1);
        const float det = p00 * p11 - p01 * p01;
        if (!(p00 > 0.0f && p11 > 0.0f && det > 0.0f))
        {
            pd_bad++;
        }
        if (fabsf(p01 - EKF_P(&ekf_jos, 1, 0)) > 1e-9f * (1.0f + fabsf(p01)))
        {
            sym_bad++;
        }
    }
    printf("  用例6 病态P0+Joseph: 非正定步数=%d 非对称步数=%d 末端 P=%g/%g\n", pd_bad, sym_bad,
           (double)EKF_P(&ekf_jos, 0, 0), (double)EKF_P(&ekf_jos, 1, 1));
    check(pd_bad == 0, "Joseph 下 P 失去正定性");
    check(sym_bad == 0, "P 未保持对称");
}

/*============================ 用例 7/8：参数校验与 ctx 透传 ============================*/

static void case_param_and_ctx(void)
{
    float F[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float Q[4] = {0.0f};
    float H[2] = {1.0f, 0.0f};
    float R[1] = {1.0f};
    float P0[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float x0[2] = {0.0f, 0.0f};

    /* n 越上限（n_max=2）/ n=0 / m 越上限（m_max=1） */
    Ekf_Init_Config_s bad1 = {.n = 3, .m = 1, .l = 0, .h_fn = lin_h, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    Ekf_Init_Config_s bad2 = {.n = 0, .m = 1, .l = 0, .h_fn = lin_h, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    Ekf_Init_Config_s bad3 = {.n = 2, .m = 2, .l = 0, .h_fn = lin_h, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    const int d1 = (EkfInit(&ekf_bad, &bad1) == EKF_ERR_DIM);
    const int d2 = (EkfInit(&ekf_bad, &bad2) == EKF_ERR_DIM);
    const int d3 = (EkfInit(&ekf_bad, &bad3) == EKF_ERR_DIM);
    const int nl = (EkfInit(NULL, &bad2) == EKF_ERR_NULL) && (EkfInit(&ekf_bad, NULL) == EKF_ERR_NULL) &&
                   (EkfPredict(NULL, NULL, 0.0f) == EKF_ERR_NULL) && (EkfUpdate(NULL, R) == EKF_ERR_NULL);

    /* h_fn 缺失 → EKF_ERR_CFG */
    Ekf_Init_Config_s noh = {.n = 2, .m = 1, .l = 0, .h_fn = NULL, .x0 = x0, .P0 = P0, .F = F, .Q = Q, .H = H, .R = R};
    check(EkfInit(&ekf_bad, &noh) == EKF_OK, "EkfInit 失败");
    const int cfg_missing = (EkfUpdate(&ekf_bad, R) == EKF_ERR_CFG);

    /* m_now 越界 */
    EkfInit(&ekf_bad, &noh);
    const int m_now_bad = (EkfUpdateM(&ekf_bad, 0, R) == EKF_ERR_DIM) && (EkfUpdateM(&ekf_bad, 2, R) == EKF_ERR_DIM);

    printf("  用例7 参数校验: n越界=%d n=0=%d m越界=%d 空指针=%d h缺失=%d m_now越界=%d\n", d1, d2, d3, nl, cfg_missing,
           m_now_bad);
    check(d1 && d2 && d3 && nl && cfg_missing && m_now_bad, "参数校验未按预期拒绝");

    /* ctx 透传：回调里计数 */
    SelCtx c = {.e = &ekf_bad, .Hsrc = H, .sel = -1};
    Ekf_Init_Config_s ok = {.n = 2,
                            .m = 1,
                            .l = 0,
                            .ctx = &c,
                            .f_fn = NULL,
                            .F_fn = NULL,
                            .h_fn = sel_h,
                            .H_fn = sel_H,
                            .x0 = x0,
                            .P0 = P0,
                            .F = F,
                            .Q = Q,
                            .H = H,
                            .R = R};
    check(EkfInit(&ekf_bad, &ok) == EKF_OK, "EkfInit 失败");
    float z = 0.5f;
    for (int i = 0; i < 10; i++)
    {
        EkfPredict(&ekf_bad, NULL, 0.0f);
        EkfUpdate(&ekf_bad, &z);
    }
    printf("  用例8 ctx 透传: h 回调=%d 次 H 回调=%d 次（期望 10/10）\n", c.h_calls, c.H_calls);
    check(c.h_calls == 10 && c.H_calls == 10, "ctx 未正确透传");

    /* EkfSetState / EkfReset */
    float ns[2] = {7.0f, -3.0f};
    check(EkfSetState(&ekf_bad, ns, NULL) == EKF_OK && ekf_bad.x[0] == 7.0f && ekf_bad.x[1] == -3.0f,
          "EkfSetState 未生效");
    check(EkfReset(&ekf_bad) == EKF_OK && ekf_bad.x[0] == 0.0f && ekf_bad.P[0] == 1.0f && ekf_bad.P[3] == 1.0f,
          "EkfReset 未生效");
}

int main(void)
{
    printf("lib_ekf 测试\n");
    printf("========================================\n");
    case_linear_equivalent();
    case_pendulum();
    case_radar();
    case_partial();
    case_singular();
    case_joseph_pd();
    case_param_and_ctx();
    printf("========================================\n");
    if (s_fail == 0)
    {
        printf("结果: 全部 PASS\n");
        return 0;
    }
    printf("结果: %d 项 FAIL\n", s_fail);
    return 1;
}
