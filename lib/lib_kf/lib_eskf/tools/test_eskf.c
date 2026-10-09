/**
 * @file test_eskf.c
 * @brief lib_eskf PC 端测试（姿态 ESKF：BMI088 式 acc/gyro + IST8310 式 mag）
 *
 * 本文件自带一套**独立于实现**的四元数/姿态真值模型，用于锁死 ESKF 的符号与乘向：
 *   - q：机体系 → 世界系（v_w = q ⊗ v_b ⊗ q*）
 *   - 误差：q_true = q_nom ⊗ δq，δq = normalize([1, δθ/2])，**δθ 在机体系（右乘）**
 *   - 世界系：X=北, Y=西, Z=上；重力参考 ĝ_w = [0,0,1]
 *   - 磁参考：m_w = [cosI·cosD, -cosI·sinD, -sinI]（I=磁倾角(+下), D=偏角(+东)）
 *   - 线性化结论（本测试即验证它）：F_c = [[-[ω]×, -I₃],[0₃,0₃]]，H = [[h]×, 0₃]
 *
 * 用例：
 *   1. 静置收敛：初始姿态误差 ~30°，10 s 后 roll/pitch/yaw 误差 < 0.005 rad、δb < 5e-4
 *   2. 解析旋转：绕**机体** Z 恒速转 360°（常值机体角速率 ⇒ 名义传播逐步精确，无 ZOH 模型
 *      误差），末端姿态夹角 < 0.002 rad（实测 0.00000 rad、中途最大 0.00098 rad）。
 *      H 的 [v]× 符号或 δq 左/右乘若写反，本用例会立刻发散。
 *      **时序硬约束**：量测必须由"本步 predict 之后"的真值采样——真值要先推进 dt 再取观测，
 *      否则整整错开一步（ω·dt ≈ 0.3°，实测会稳定停在 0.0053 rad 而非 0）
 *   3. 零偏可观测：真值零偏 5e-3 rad/s、名义从 0 起，30 s 后残差 < 1e-4
 *   4. 注入正确性：q←q⊗δq、b+=δb，P 与 G·P·Gᵀ 一致到 1e-6
 *   5. G=I 简化的量化影响 < 1e-3
 *   6. 残差符号约定：一次更新得到的 δθ̂ 与真误差同向
 *   7. 多速率顺序更新（acc 200Hz / mag 50Hz）不发散
 *   8. 时序与参数保护：Update→Predict 未 Inject、奇异、m_now 越界、回调缺失
 *
 * 编译见同目录 test_eskf.sh；退出码 0 = 全部 PASS。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_eskf.h"

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

/*============================ 独立四元数工具（q 为 [w,x,y,z]） ============================*/

static void qmul(const float *a, const float *b, float *o)
{
    o[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    o[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    o[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    o[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

static void qnormalize(float *q)
{
    const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n > 1e-20f)
    {
        q[0] /= n;
        q[1] /= n;
        q[2] /= n;
        q[3] /= n;
    }
}

static void qconj(const float *q, float *o)
{
    o[0] = q[0];
    o[1] = -q[1];
    o[2] = -q[2];
    o[3] = -q[3];
}

/** @brief o = R(q)ᵀ·v（世界系 → 机体系） */
static void qrotT(const float *q, const float *v, float *o)
{
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    o[0] = (1.0f - 2.0f * (y * y + z * z)) * v[0] + 2.0f * (x * y + w * z) * v[1] + 2.0f * (x * z - w * y) * v[2];
    o[1] = 2.0f * (x * y - w * z) * v[0] + (1.0f - 2.0f * (x * x + z * z)) * v[1] + 2.0f * (y * z + w * x) * v[2];
    o[2] = 2.0f * (x * z + w * y) * v[0] + 2.0f * (y * z - w * x) * v[1] + (1.0f - 2.0f * (x * x + y * y)) * v[2];
}

/** @brief 由 ZYX 欧拉角（先滚转、再俯仰、最后偏航）构造机体系→世界系四元数 */
static void q_from_euler(float roll, float pitch, float yaw, float *q)
{
    const float cr = cosf(roll * 0.5f), sr = sinf(roll * 0.5f);
    const float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    const float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
    q[0] = cy * cp * cr + sy * sp * sr;
    q[1] = cy * cp * sr - sy * sp * cr;
    q[2] = cy * sp * cr + sy * cp * sr;
    q[3] = sy * cp * cr - cy * sp * sr;
    qnormalize(q);
}

/** @brief 两姿态夹角 [rad] */
static float q_angle(const float *a, const float *b)
{
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0.0f)
    {
        d = -d;
    }
    if (d > 1.0f)
    {
        d = 1.0f;
    }
    return 2.0f * acosf(d);
}

/** @brief 由轴角构造四元数 */
static void q_from_axis_angle(const float *axis, float angle, float *q)
{
    const float n = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (n < 1e-20f)
    {
        q[0] = 1.0f;
        q[1] = q[2] = q[3] = 0.0f;
        return;
    }
    const float s = sinf(angle * 0.5f) / n;
    q[0] = cosf(angle * 0.5f);
    q[1] = axis[0] * s;
    q[2] = axis[1] * s;
    q[3] = axis[2] * s;
}

/*============================ 姿态 ESKF 的 ctx 与回调 ============================*/

#define N_ATT 6 /* δx = [δθ(3); δb_g(3)] */
#define M_ATT 3 /* 归一化方向量测（acc / mag 各 3 维） */

typedef struct
{
    /* 名义状态（滤波器外） */
    float q[4]; /* 机体系→世界系 */
    float b[3]; /* 陀螺零偏估计 rad/s */

    /* 本步输入（由测试驱动写入） */
    float gyro[3]; /* rad/s，已扣标定零偏 */

    /* 参考矢量与噪声 */
    float g_w[3];
    float m_w[3];
    float q_att;  /* 姿态过程噪声谱密度 rad²/s   */
    float q_bias; /* 零偏随机游走谱密度 (rad/s)²/s */
    float r_acc;  /* 量测方差 rad² */
    float r_mag;

    /* 观测用 */
    int inject_calls;
    float last_inject_delta[N_ATT];
    int mag_used;
} AttCtx;

static void att_propagate(void *ctx, const float *u, float dt, float *F, float *Q)
{
    (void)u;
    AttCtx *c = (AttCtx *)ctx;

    /* ω = 陀螺 - 名义零偏 */
    float w[3];
    for (int i = 0; i < 3; i++)
    {
        w[i] = c->gyro[i] - c->b[i];
    }

    /* 名义姿态：q ← q ⊗ Δq（δq 右乘 ⇒ 机体系角速率用右乘） */
    const float wn = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    float dq[4];
    const float theta = wn * dt;
    if (theta > 1e-9f)
    {
        const float s = sinf(theta * 0.5f) / wn;
        dq[0] = cosf(theta * 0.5f);
        dq[1] = w[0] * s;
        dq[2] = w[1] * s;
        dq[3] = w[2] * s;
    }
    else
    {
        dq[0] = 1.0f;
        dq[1] = w[0] * dt * 0.5f;
        dq[2] = w[1] * dt * 0.5f;
        dq[3] = w[2] * dt * 0.5f;
        qnormalize(dq);
    }
    float qn[4];
    qmul(c->q, dq, qn);
    for (int i = 0; i < 4; i++)
    {
        c->q[i] = qn[i];
    }
    qnormalize(c->q);

    /* F = I₆ + [[-[ω]×, -I₃],[0₃,0₃]]·dt */
    for (int i = 0; i < N_ATT * N_ATT; i++)
    {
        F[i] = 0.0f;
    }
    for (int i = 0; i < N_ATT; i++)
    {
        F[i * N_ATT + i] = 1.0f;
    }
    F[0 * N_ATT + 1] = w[2] * dt;  /* -[ω]×: (0,1)=+wz */
    F[0 * N_ATT + 2] = -w[1] * dt; /*        (0,2)=-wy */
    F[1 * N_ATT + 0] = -w[2] * dt;
    F[1 * N_ATT + 2] = w[0] * dt;
    F[2 * N_ATT + 0] = w[1] * dt;
    F[2 * N_ATT + 1] = -w[0] * dt;
    for (int i = 0; i < 3; i++)
    {
        F[i * N_ATT + 3 + i] = -dt; /* -I₃ */
    }

    /* Q = diag(q_att·dt·I₃, q_bias·dt·I₃) */
    for (int i = 0; i < N_ATT * N_ATT; i++)
    {
        Q[i] = 0.0f;
    }
    for (int i = 0; i < 3; i++)
    {
        Q[i * N_ATT + i] = c->q_att * dt;
        Q[(3 + i) * N_ATT + (3 + i)] = c->q_bias * dt;
    }
}

/** @brief 归一化方向量测的公共部分：y = z - R(q_nom)ᵀ·ref，H = [[h]×, 0₃] */
static void att_dir_measure(AttCtx *c, const float *z, int m_now, float *y, float *H, float *R, const float *ref, float var)
{
    float h[M_ATT];
    qrotT(c->q, ref, h);

    for (int i = 0; i < m_now; i++)
    {
        y[i] = z[i] - h[i];
    }
    /* H = [[h]×, 0₃]：第 i 行 = e_i × h；行距恒为 N_ATT */
    for (int i = 0; i < m_now; i++)
    {
        for (int j = 0; j < N_ATT; j++)
        {
            H[i * N_ATT + j] = 0.0f;
        }
    }
    if (m_now >= 3)
    {
        H[0 * N_ATT + 1] = -h[2];
        H[0 * N_ATT + 2] = h[1];
        H[1 * N_ATT + 0] = h[2];
        H[1 * N_ATT + 2] = -h[0];
        H[2 * N_ATT + 0] = -h[1];
        H[2 * N_ATT + 1] = h[0];
    }
    for (int i = 0; i < m_now; i++)
    {
        for (int j = 0; j < m_now; j++)
        {
            R[i * m_now + j] = (i == j) ? var : 0.0f;
        }
    }
}

static void att_measure_acc(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
{
    AttCtx *c = (AttCtx *)ctx;
    att_dir_measure(c, z, m_now, y, H, R, c->g_w, c->r_acc);
}

static void att_measure_mag(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
{
    AttCtx *c = (AttCtx *)ctx;
    att_dir_measure(c, z, m_now, y, H, R, c->m_w, c->r_mag);
    c->mag_used++;
}

static void att_inject(void *ctx, const float *delta)
{
    AttCtx *c = (AttCtx *)ctx;
    c->inject_calls++;
    for (int i = 0; i < N_ATT; i++)
    {
        c->last_inject_delta[i] = delta[i];
    }

    /* δq = normalize([1, δθ/2])，q ← normalize(q ⊗ δq) */
    float dq[4] = {1.0f, delta[0] * 0.5f, delta[1] * 0.5f, delta[2] * 0.5f};
    qnormalize(dq);
    float qn[4];
    qmul(c->q, dq, qn);
    for (int i = 0; i < 4; i++)
    {
        c->q[i] = qn[i];
    }
    qnormalize(c->q);

    for (int i = 0; i < 3; i++)
    {
        c->b[i] += delta[3 + i];
    }
}

/** @brief 理论重置雅可比 G = diag(I₃ - ½[δθ]×, I₃) */
static void att_reset_jac(void *ctx, const float *delta, float *G)
{
    (void)ctx;
    for (int i = 0; i < N_ATT * N_ATT; i++)
    {
        G[i] = 0.0f;
    }
    for (int i = 0; i < N_ATT; i++)
    {
        G[i * N_ATT + i] = 1.0f;
    }
    const float hx = delta[0] * 0.5f, hy = delta[1] * 0.5f, hz = delta[2] * 0.5f;
    /* -(½[δθ]×) 的上三角 */
    G[0 * N_ATT + 1] = hz;
    G[0 * N_ATT + 2] = -hy;
    G[1 * N_ATT + 0] = -hz;
    G[1 * N_ATT + 2] = hx;
    G[2 * N_ATT + 0] = hy;
    G[2 * N_ATT + 1] = -hx;
}

ESKF_INSTANCE_DEF(eskf, N_ATT, M_ATT, 0);

/*============================ 公共驱动 ============================*/

static void att_ctx_init(AttCtx *c, float incl, float decl)
{
    memset(c, 0, sizeof(*c));
    c->q[0] = 1.0f;
    c->g_w[0] = 0.0f;
    c->g_w[1] = 0.0f;
    c->g_w[2] = 1.0f;
    /* m_w = [cosI·cosD, -cosI·sinD, -sinI] */
    c->m_w[0] = cosf(incl) * cosf(decl);
    c->m_w[1] = -cosf(incl) * sinf(decl);
    c->m_w[2] = -sinf(incl);
    c->q_att = 1e-6f;
    c->q_bias = 1e-10f;
    c->r_acc = 1e-5f;
    c->r_mag = 1e-5f;
}

static void p0_diag(float *P0, float att_var, float bias_var)
{
    memset(P0, 0, sizeof(float) * (size_t)(N_ATT * N_ATT));
    for (int i = 0; i < 3; i++)
    {
        P0[i * N_ATT + i] = att_var;
        P0[(3 + i) * N_ATT + (3 + i)] = bias_var;
    }
}

/** @brief 用真值姿态生成一套无噪声观测（单位方向） */
static void sample_dirs(const AttCtx *c, const float *q_true, float *acc_b, float *mag_b)
{
    qrotT(q_true, c->g_w, acc_b);
    qrotT(q_true, c->m_w, mag_b);
}

static int g_update_fail; /* 记录 EskfUpdate/Inject 返回非 OK 的次数 */

/*============================ 用例 1：静置收敛 ============================*/

static void case_static(void)
{
    AttCtx c;
    att_ctx_init(&c, 50.0f * 3.14159265f / 180.0f, 0.0f);
    c.q_att = 1e-7f;
    c.q_bias = 1e-10f;
    c.r_acc = 1e-6f;
    c.r_mag = 1e-6f;

    /* 真值姿态：roll 0.2 / pitch -0.3 / yaw 0.5；名义从单位阵起（初始误差 ~35°） */
    float q_true[4];
    q_from_euler(0.2f, -0.3f, 0.5f, q_true);

    float P0[N_ATT * N_ATT];
    p0_diag(P0, 0.25f, 1e-4f); /* σ_att=0.5 rad, σ_bias=0.01 rad/s */
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = ESKF_OPT_JOSEPH,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = NULL,
                              .P0 = P0};
    check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");

    const float dt = 0.005f;
    const int steps = 2000; /* 10 s */
    g_update_fail = 0;
    rnd_seed(1u);

    for (int t = 0; t < steps; t++)
    {
        c.gyro[0] = (float)(0.002 * nrand()); /* 静止：陀螺只读噪声 */
        c.gyro[1] = (float)(0.002 * nrand());
        c.gyro[2] = (float)(0.002 * nrand());

        float acc_z[3], mag_z[3];
        sample_dirs(&c, q_true, acc_z, mag_z);
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] += (float)(0.002 * nrand());
            mag_z[i] += (float)(0.002 * nrand());
        }
        float n1 = 0.0f, n2 = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            n1 += acc_z[i] * acc_z[i];
            n2 += mag_z[i] * mag_z[i];
        }
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] /= sqrtf(n1);
            mag_z[i] /= sqrtf(n2);
        }

        if (EskfPredict(&eskf, NULL, dt) != ESKF_OK)
        {
            g_update_fail++;
            break;
        }
        if (EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        else
        {
            g_update_fail++;
        }
        if (EskfUpdateM(&eskf, M_ATT, mag_z, att_measure_mag) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        else
        {
            g_update_fail++;
        }
    }

    /* 名义与真值之差（机体系误差角） */
    float qe[4], qc[4];
    qconj(c.q, qc);
    qmul(qc, q_true, qe);
    if (qe[0] < 0.0f)
    {
        for (int i = 0; i < 4; i++)
        {
            qe[i] = -qe[i];
        }
    }
    const float err[3] = {2.0f * qe[1], 2.0f * qe[2], 2.0f * qe[3]};
    const float total = q_angle(c.q, q_true);
    float db = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        if (fabsf(c.b[i]) > db)
        {
            db = fabsf(c.b[i]);
        }
    }

    printf("  用例1 静置收敛: 误差角 roll=%.5f pitch=%.5f yaw=%.5f |总|=%.5f rad, max|δb|=%.2e, 更新失败=%d\n", err[0], err[1], err[2], total, db,
           g_update_fail);
    check(g_update_fail == 0, "静置过程出现更新失败");
    check(fabsf(err[0]) < 0.005f && fabsf(err[1]) < 0.005f && fabsf(err[2]) < 0.005f, "静置姿态误差超限");
    check(db < 5e-4f, "零偏残差超限");
    check(total < 0.005f, "总姿态误差超限");

    /* 姿态协方差应收敛（σ_att 从 0.5 rad 降到 <0.01 rad） */
    const float s_att = sqrtf(ESKF_P(&eskf, 0, 0));
    printf("            收敛后 σ_att=%.5f rad（初值 0.5）\n", s_att);
    check(s_att < 0.01f, "σ_att 未收敛");
}

/*============================ 用例 2：解析旋转 ============================*/

static void case_rotation_360(void)
{
    AttCtx c;
    att_ctx_init(&c, 50.0f * 3.14159265f / 180.0f, 0.0f);
    c.q_att = 1e-8f;
    c.r_acc = 1e-5f;
    c.r_mag = 1e-5f;

    /* 真值：绕**机体 Z 轴**恒速转，6 s 整 360°。选机体轴而非世界轴，是为了让
     * "常值机体角速率" 的传播在数值上精确成立，从而使本用例只考符号/乘向约定，
     * 不掺离散化模型误差。初始带一点倾斜使 acc 不退化。 */
    float q_init[4];
    q_from_euler(0.15f, 0.08f, 0.0f, q_init);
    float q_true[4];
    memcpy(q_true, q_init, sizeof(q_true));

    const float w_b_const[3] = {0.0f, 0.0f, 2.0f * 3.14159265f / 6.0f};
    const float dt = 0.005f;
    const int steps = 1200; /* 6 s */

    float P0[N_ATT * N_ATT];
    p0_diag(P0, 1e-3f, 1e-6f);
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = ESKF_OPT_JOSEPH,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = NULL,
                              .P0 = P0};
    check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");

    g_update_fail = 0;
    rnd_seed(2u);
    float worst = 0.0f;
    float mid_turn = 0.0f;

    for (int t = 0; t < steps; t++)
    {
        /* 真值先推进到 t+dt：量测必须与滤波器 predict 之后的时刻对齐，
         * 否则会引入整整一步（ω·dt ≈ 0.3°）的时序错位。常值机体角速率 ⇒ 精确。 */
        float dq[4];
        q_from_axis_angle(w_b_const, w_b_const[2] * dt, dq);
        float qn[4];
        qmul(q_true, dq, qn);
        for (int i = 0; i < 4; i++)
        {
            q_true[i] = qn[i];
        }
        qnormalize(q_true);

        if (t == steps / 2)
        {
            mid_turn = q_angle(q_true, q_init); /* 折半后应≈180°（π rad） */
        }

        for (int i = 0; i < 3; i++)
        {
            c.gyro[i] = w_b_const[i]; /* 零偏为 0，读数即真值 */
        }
        float acc_z[3], mag_z[3];
        sample_dirs(&c, q_true, acc_z, mag_z);

        if (EskfPredict(&eskf, NULL, dt) != ESKF_OK)
        {
            g_update_fail++;
            break;
        }
        if (EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        else
        {
            g_update_fail++;
        }
        if (EskfUpdateM(&eskf, M_ATT, mag_z, att_measure_mag) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        else
        {
            g_update_fail++;
        }

        if (t > 50)
        {
            const float e = q_angle(c.q, q_true);
            if (e > worst)
            {
                worst = e;
            }
        }
    }

    const float end_err = q_angle(c.q, q_true);
    printf("  用例2 解析旋转: 半程转过=%.2f°(期望180°) 末端误差=%.5f rad 中途最大=%.5f rad, 更新失败=%d\n", mid_turn * 180.0f / 3.14159265f, end_err, worst,
           g_update_fail);
    check(g_update_fail == 0, "旋转过程出现更新失败");
    check(fabsf(mid_turn - 3.14159265f) < 0.05f, "真值未按预期转过 180°（测试自身失效）");
    check(end_err < 0.002f, "末端姿态误差超限（H 符号 / δq 乘向可能写反）");
}

/*============================ 用例 3：零偏可观测 ============================*/

static void case_bias_observable(void)
{
    AttCtx c;
    att_ctx_init(&c, 50.0f * 3.14159265f / 180.0f, 0.0f);
    c.q_att = 1e-8f;
    c.q_bias = 1e-13f;
    c.r_acc = 1e-6f;
    c.r_mag = 1e-6f;

    const float b_true[3] = {0.005f, -0.003f, 0.002f};
    float q_true[4];
    q_from_euler(0.1f, -0.05f, 0.3f, q_true);

    float P0[N_ATT * N_ATT];
    p0_diag(P0, 1e-4f, 1e-4f); /* σ_att=0.01 rad, σ_bias=0.01 rad/s（零偏必须"不自负"） */
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = ESKF_OPT_JOSEPH,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = NULL,
                              .P0 = P0};
    check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");

    const float dt = 0.005f;
    const int steps = 6000; /* 30 s */
    g_update_fail = 0;
    rnd_seed(3u);

    for (int t = 0; t < steps; t++)
    {
        for (int i = 0; i < 3; i++)
        {
            c.gyro[i] = b_true[i] + (float)(0.002 * nrand());
        }
        float acc_z[3], mag_z[3];
        sample_dirs(&c, q_true, acc_z, mag_z);
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] += (float)(0.002 * nrand());
            mag_z[i] += (float)(0.002 * nrand());
        }
        float n1 = 0.0f, n2 = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            n1 += acc_z[i] * acc_z[i];
            n2 += mag_z[i] * mag_z[i];
        }
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] /= sqrtf(n1);
            mag_z[i] /= sqrtf(n2);
        }

        if (EskfPredict(&eskf, NULL, dt) != ESKF_OK)
        {
            g_update_fail++;
            break;
        }
        if (EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        if (EskfUpdateM(&eskf, M_ATT, mag_z, att_measure_mag) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
    }

    float db = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        const float e = fabsf(c.b[i] - b_true[i]);
        if (e > db)
        {
            db = e;
        }
    }
    const float att_err = q_angle(c.q, q_true);
    printf("  用例3 零偏可观测: 30s 后 max|Δb|=%.2e rad/s（真值 5e-3）, 姿态误差=%.5f rad, 更新失败=%d\n", db, att_err, g_update_fail);
    check(g_update_fail == 0, "零偏收敛过程出现更新失败");
    check(db < 1e-4f, "零偏残差超限");
}

/*============================ 用例 4/5：注入正确性与 P 重置 ============================*/

/** @brief 跑一次测量更新（不做注入），返回 δ */
static void one_update(AttCtx *c, float *z, float *delta_out, float (*P_out)[N_ATT])
{
    c->gyro[0] = c->gyro[1] = c->gyro[2] = 0.0f;
    check(EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK, "Predict 失败");
    check(EskfUpdateM(&eskf, M_ATT, z, att_measure_acc) == ESKF_OK, "Update 失败");
    for (int i = 0; i < N_ATT; i++)
    {
        delta_out[i] = eskf.delta[i];
        for (int j = 0; j < N_ATT; j++)
        {
            P_out[i][j] = ESKF_P(&eskf, i, j);
        }
    }
}

static void case_inject_and_reset(void)
{
    /* ---------- 4a. reset_jac = NULL：注入后 P 不变，q/b 按约定更新 ---------- */
    {
        AttCtx c;
        att_ctx_init(&c, 0.87f, 0.0f);
        float P0[N_ATT * N_ATT];
        p0_diag(P0, 0.01f, 1e-4f);
        Eskf_Init_Config_s cfg = {.n = N_ATT,
                                  .m = M_ATT,
                                  .l = 0,
                                  .opt = 0,
                                  .ctx = &c,
                                  .propagate = att_propagate,
                                  .measure = att_measure_acc,
                                  .inject = att_inject,
                                  .reset_jac = NULL,
                                  .P0 = P0};
        check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");
        q_from_euler(0.05f, -0.02f, 0.1f, c.q);
        c.b[0] = 0.003f;
        c.b[1] = -0.001f;
        c.b[2] = 0.002f;

        float z[3] = {0.1f, -0.2f, 0.97f};
        float n = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        for (int i = 0; i < 3; i++)
        {
            z[i] /= n;
        }

        float q_before[4], b_before[3];
        memcpy(q_before, c.q, sizeof(q_before));
        memcpy(b_before, c.b, sizeof(b_before));

        float delta[N_ATT];
        float P_before[N_ATT][N_ATT];
        one_update(&c, z, delta, P_before);

        check(EskfInject(&eskf) == ESKF_OK, "Inject 失败");
        check(c.inject_calls == 1, "inject 回调未被调用");

        /* q_nom == q_before ⊗ normalize([1, δθ/2]) */
        float dq[4] = {1.0f, delta[0] * 0.5f, delta[1] * 0.5f, delta[2] * 0.5f};
        qnormalize(dq);
        float q_expect[4];
        qmul(q_before, dq, q_expect);
        qnormalize(q_expect);
        const float dq_err = q_angle(c.q, q_expect);

        float db_err = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            const float e = fabsf(c.b[i] - (b_before[i] + delta[3 + i]));
            if (e > db_err)
            {
                db_err = e;
            }
        }

        /* P 应保持不变（G = I） */
        float dp = 0.0f;
        for (int i = 0; i < N_ATT; i++)
        {
            for (int j = 0; j < N_ATT; j++)
            {
                const float e = fabsf(ESKF_P(&eskf, i, j) - P_before[i][j]);
                if (e > dp)
                {
                    dp = e;
                }
            }
        }
        printf("  用例4a 注入(G=I): |δθ|=%.4f rad, q 误差=%.2e rad, b 误差=%.2e, P 变化=%.2e\n",
               sqrtf(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]), dq_err, db_err, dp);
        check(dq_err < 1e-6f, "q ← q⊗δq 注入不符");
        check(db_err < 1e-9f, "b += δb 注入不符");
        check(dp == 0.0f, "G=I 时 P 不应被改动");
        check(eskf.delta_dirty == 0, "注入后脏标志未清除");
        for (int i = 0; i < N_ATT; i++)
        {
            check(eskf.delta[i] == 0.0f, "注入后 δ 未清零");
        }
        /* 幂等 */
        check(EskfInject(&eskf) == ESKF_OK && c.inject_calls == 1, "Inject 不幂等");
    }

    /* ---------- 4b. reset_jac 非空：P == G·P⁻·Gᵀ ---------- */
    {
        AttCtx c;
        att_ctx_init(&c, 0.87f, 0.0f);
        float P0[N_ATT * N_ATT];
        p0_diag(P0, 0.01f, 1e-4f);
        Eskf_Init_Config_s cfg = {.n = N_ATT,
                                  .m = M_ATT,
                                  .l = 0,
                                  .opt = 0,
                                  .ctx = &c,
                                  .propagate = att_propagate,
                                  .measure = att_measure_acc,
                                  .inject = att_inject,
                                  .reset_jac = att_reset_jac,
                                  .P0 = P0};
        check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");
        q_from_euler(0.05f, -0.02f, 0.1f, c.q);

        float z[3] = {0.1f, -0.2f, 0.97f};
        float n = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        for (int i = 0; i < 3; i++)
        {
            z[i] /= n;
        }
        float delta[N_ATT];
        float P_before[N_ATT][N_ATT];
        one_update(&c, z, delta, P_before);
        check(EskfInject(&eskf) == ESKF_OK, "Inject 失败");

        /* 测试内独立算 G·P⁻·Gᵀ（double） */
        double G[N_ATT][N_ATT], Pn[N_ATT][N_ATT], tmp[N_ATT][N_ATT], expect[N_ATT][N_ATT];
        for (int i = 0; i < N_ATT; i++)
        {
            for (int j = 0; j < N_ATT; j++)
            {
                G[i][j] = (i == j) ? 1.0 : 0.0;
                Pn[i][j] = (double)P_before[i][j];
            }
        }
        const double hx = delta[0] * 0.5, hy = delta[1] * 0.5, hz = delta[2] * 0.5;
        G[0][1] = hz;
        G[0][2] = -hy;
        G[1][0] = -hz;
        G[1][2] = hx;
        G[2][0] = hy;
        G[2][1] = -hx;
        for (int i = 0; i < N_ATT; i++)
        {
            for (int j = 0; j < N_ATT; j++)
            {
                double a = 0.0;
                for (int k = 0; k < N_ATT; k++)
                {
                    a += G[i][k] * Pn[k][j];
                }
                tmp[i][j] = a;
            }
        }
        for (int i = 0; i < N_ATT; i++)
        {
            for (int j = 0; j < N_ATT; j++)
            {
                double a = 0.0;
                for (int k = 0; k < N_ATT; k++)
                {
                    a += tmp[i][k] * G[j][k];
                }
                expect[i][j] = a;
            }
        }
        double dp = 0.0;
        for (int i = 0; i < N_ATT; i++)
        {
            for (int j = 0; j < N_ATT; j++)
            {
                const double e = fabs((double)ESKF_P(&eskf, i, j) - expect[i][j]) / (1.0 + fabs(expect[i][j]));
                if (e > dp)
                {
                    dp = e;
                }
            }
        }
        printf("  用例4b 注入(G≠I): P 与 G·P⁻·Gᵀ 的相对偏差=%.3e\n", dp);
        check(dp < 1e-6, "P 重置未按 G·P·Gᵀ 执行");
    }
}

/** @brief 用例 5：G=I 与 G=理论值的量化差异 */
static double run_and_get_p00(int use_jac, const AttCtx *c_src)
{
    AttCtx c = *c_src;
    float P0[N_ATT * N_ATT];
    p0_diag(P0, 0.01f, 1e-4f);
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = 0,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = use_jac ? att_reset_jac : NULL,
                              .P0 = P0};
    if (EskfInit(&eskf, &cfg) != ESKF_OK)
    {
        return -1.0;
    }
    q_from_euler(0.05f, -0.02f, 0.1f, c.q);

    float q_true[4];
    q_from_euler(0.2f, 0.1f, -0.3f, q_true);
    rnd_seed(9u);
    for (int t = 0; t < 400; t++)
    {
        c.gyro[0] = (float)(0.002 * nrand());
        c.gyro[1] = (float)(0.002 * nrand());
        c.gyro[2] = (float)(0.002 * nrand());
        float acc_z[3], mag_z[3];
        sample_dirs(&c, q_true, acc_z, mag_z);
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] += (float)(0.01 * nrand());
            mag_z[i] += (float)(0.01 * nrand());
        }
        float n1 = 0.0f, n2 = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            n1 += acc_z[i] * acc_z[i];
            n2 += mag_z[i] * mag_z[i];
        }
        for (int i = 0; i < 3; i++)
        {
            acc_z[i] /= sqrtf(n1);
            mag_z[i] /= sqrtf(n2);
        }
        if (EskfPredict(&eskf, NULL, 0.005f) != ESKF_OK)
        {
            break;
        }
        if (EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        if (EskfUpdateM(&eskf, M_ATT, mag_z, att_measure_mag) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
    }
    return (double)ESKF_P(&eskf, 0, 0);
}

static void case_reset_jac_impact(void)
{
    AttCtx c;
    att_ctx_init(&c, 0.87f, 0.0f);
    const double p_no = run_and_get_p00(0, &c);
    const double p_jac = run_and_get_p00(1, &c);
    const double rel = fabs(p_no - p_jac) / (1.0 + fabs(p_jac));
    printf("  用例5 G=I 简化影响: P(0,0) %.6e vs %.6e, 相对差=%.3e\n", p_no, p_jac, rel);
    check(rel < 1e-3, "G=I 简化的量化影响超出 1e-3");
}

/*============================ 用例 6：残差符号 ============================*/

static void case_residual_sign(void)
{
    AttCtx c;
    att_ctx_init(&c, 0.87f, 0.0f);
    float P0[N_ATT * N_ATT];
    p0_diag(P0, 0.01f, 1e-6f);
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = 0,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = NULL,
                              .P0 = P0};
    check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");

    /* 名义 = 单位阵；真值带已知的小姿态误差 δθ_true（机体系） */
    const float dtheta_true[3] = {0.02f, -0.03f, 0.01f};
    float dq[4] = {1.0f, dtheta_true[0] * 0.5f, dtheta_true[1] * 0.5f, dtheta_true[2] * 0.5f};
    qnormalize(dq);
    float q_true[4];
    qmul(c.q, dq, q_true);
    qnormalize(q_true);

    float acc_z[3], mag_z[3];
    sample_dirs(&c, q_true, acc_z, mag_z);

    c.gyro[0] = c.gyro[1] = c.gyro[2] = 0.0f;
    check(EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK, "Predict 失败");

    /* 用 acc 单次更新：δθ̂ 应与 δθ_true 同向且量级接近 */
    check(EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK, "Update 失败");
    const float *dt_hat = eskf.delta;

    float dot = 0.0f, n1 = 0.0f, n2 = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        dot += dt_hat[i] * dtheta_true[i];
        n1 += dt_hat[i] * dt_hat[i];
        n2 += dtheta_true[i] * dtheta_true[i];
    }
    const float rel_scale = sqrtf(n1 / n2);
    printf("  用例6 残差符号: ẑ·δθ_true=%.3e（应>0）, |δθ̂|/|δθ_true|=%.3f\n", (double)dot, (double)rel_scale);
    check(dot > 0.0f, "δθ̂ 与真误差反向 —— 残差或 H 符号写反");
    check(rel_scale < 1.0f, "单次 acc 更新的 δθ̂ 不应超过真误差");
    EskfInject(&eskf);
}

/*============================ 用例 7：多速率顺序更新 ============================*/

static void case_multirate(void)
{
    AttCtx c;
    att_ctx_init(&c, 0.87f, 0.0f);
    c.q_att = 1e-7f;
    c.q_bias = 1e-12f;
    c.r_acc = 1e-6f;
    c.r_mag = 1e-6f;

    float q_true[4];
    q_from_euler(0.1f, 0.05f, 0.0f, q_true);
    const float w_world[3] = {0.0f, 0.0f, 1.2f};
    float P0[N_ATT * N_ATT];
    p0_diag(P0, 1e-3f, 1e-6f);
    Eskf_Init_Config_s cfg = {.n = N_ATT,
                              .m = M_ATT,
                              .l = 0,
                              .opt = ESKF_OPT_JOSEPH,
                              .ctx = &c,
                              .propagate = att_propagate,
                              .measure = att_measure_acc,
                              .inject = att_inject,
                              .reset_jac = NULL,
                              .P0 = P0};
    check(EskfInit(&eskf, &cfg) == ESKF_OK, "EskfInit 失败");

    const float dt = 0.005f;
    const int steps = 4000; /* 20 s */
    g_update_fail = 0;
    rnd_seed(7u);
    float worst = 0.0f;
    int mag_cnt = 0;

    for (int t = 0; t < steps; t++)
    {
        /* 真值先推进到 t+dt，使量测与 predict 后的时刻对齐（见用例 2 说明） */
        for (int s = 0; s < 2; s++)
        {
            float axis[3] = {0.0f, 0.0f, 1.0f};
            float dq[4];
            q_from_axis_angle(axis, w_world[2] * dt / 2.0f, dq);
            float qn[4];
            qmul(dq, q_true, qn);
            for (int i = 0; i < 4; i++)
            {
                q_true[i] = qn[i];
            }
            qnormalize(q_true);
        }

        float w_b[3];
        qrotT(q_true, w_world, w_b);
        for (int i = 0; i < 3; i++)
        {
            c.gyro[i] = w_b[i] + (float)(0.001 * nrand());
        }
        float acc_z[3], mag_z[3];
        sample_dirs(&c, q_true, acc_z, mag_z);

        if (EskfPredict(&eskf, NULL, dt) != ESKF_OK)
        {
            g_update_fail++;
            break;
        }
        if (EskfUpdateM(&eskf, M_ATT, acc_z, att_measure_acc) == ESKF_OK)
        {
            EskfInject(&eskf);
        }
        else
        {
            g_update_fail++;
        }
        /* mag 只有 1/4 速率 */
        if (t % 4 == 0)
        {
            mag_cnt++;
            if (EskfUpdateM(&eskf, M_ATT, mag_z, att_measure_mag) == ESKF_OK)
            {
                EskfInject(&eskf);
            }
            else
            {
                g_update_fail++;
            }
        }

        if (t > 100)
        {
            const float e = q_angle(c.q, q_true);
            if (e > worst)
            {
                worst = e;
            }
        }
    }

    printf("  用例7 多速率(acc 200Hz/mag 50Hz): mag 更新=%d 次, 最大姿态误差=%.5f rad, 更新失败=%d\n", mag_cnt, worst, g_update_fail);
    check(g_update_fail == 0, "多速率过程出现更新失败");
    check(worst < 0.02f, "多速率下姿态发散");
    check(ESKF_P(&eskf, 0, 0) < 1.0f, "P 发散");
}

/*============================ 用例 8：时序与参数保护 ============================*/

static void bad_propagate(void *ctx, const float *u, float dt, float *F, float *Q)
{
    (void)ctx;
    (void)u;
    (void)dt;
    for (int i = 0; i < N_ATT * N_ATT; i++)
    {
        F[i] = 0.0f;
        Q[i] = 0.0f;
    }
    for (int i = 0; i < N_ATT; i++)
    {
        F[i * N_ATT + i] = 1.0f;
    }
}

static void zero_measure(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
{
    (void)ctx;
    (void)z;
    for (int i = 0; i < m_now; i++)
    {
        y[i] = 0.0f;
        for (int j = 0; j < N_ATT; j++)
        {
            H[i * N_ATT + j] = 0.0f; /* 零 H */
        }
        for (int j = 0; j < m_now; j++)
        {
            R[i * m_now + j] = 0.0f; /* 零 R → S = 0 奇异 */
        }
    }
}

static void noop_inject(void *ctx, const float *delta)
{
    (void)ctx;
    (void)delta;
}

static void case_guard(void)
{
    AttCtx c;
    att_ctx_init(&c, 0.87f, 0.0f);
    float P0[N_ATT * N_ATT];
    p0_diag(P0, 0.01f, 1e-4f);

    /* 回调缺失 → ESKF_ERR_CFG */
    Eskf_Init_Config_s no_p = {.n = N_ATT, .m = M_ATT, .l = 0, .ctx = &c, .propagate = NULL, .measure = att_measure_acc, .inject = att_inject, .P0 = P0};
    Eskf_Init_Config_s no_m = no_p;
    no_m.propagate = att_propagate;
    no_m.measure = NULL;
    Eskf_Init_Config_s no_i = no_m;
    no_i.measure = att_measure_acc;
    no_i.inject = NULL;
    const int e_cfg = (EskfInit(&eskf, &no_p) == ESKF_ERR_CFG) && (EskfInit(&eskf, &no_m) == ESKF_ERR_CFG) && (EskfInit(&eskf, &no_i) == ESKF_ERR_CFG);

    /* 维度越界 */
    Eskf_Init_Config_s bad_n = no_p;
    bad_n.n = N_ATT + 1;
    bad_n.propagate = att_propagate;
    bad_n.measure = att_measure_acc;
    bad_n.inject = att_inject;
    Eskf_Init_Config_s bad_m = bad_n;
    bad_m.n = N_ATT;
    bad_m.m = M_ATT + 1;
    Eskf_Init_Config_s bad_l = bad_m;
    bad_l.m = M_ATT;
    bad_l.l = 1;
    const int e_dim = (EskfInit(&eskf, &bad_n) == ESKF_ERR_DIM) && (EskfInit(&eskf, &bad_m) == ESKF_ERR_DIM) && (EskfInit(&eskf, &bad_l) == ESKF_ERR_DIM);
    const int e_null = (EskfInit(NULL, &bad_l) == ESKF_ERR_NULL) && (EskfInit(&eskf, NULL) == ESKF_ERR_NULL) &&
                       (EskfPredict(NULL, NULL, 0.0f) == ESKF_ERR_NULL) && (EskfUpdate(NULL, P0) == ESKF_ERR_NULL);
    printf("  用例8 参数校验: 回调缺失=%d 维度越界=%d 空指针=%d\n", e_cfg, e_dim, e_null);
    check(e_cfg && e_dim && e_null, "参数校验未按预期拒绝");

    /* 时序违规：Update 后未 Inject 就 Predict / 再 Update */
    Eskf_Init_Config_s ok = {.n = N_ATT,
                             .m = M_ATT,
                             .l = 0,
                             .opt = 0,
                             .ctx = &c,
                             .propagate = att_propagate,
                             .measure = att_measure_acc,
                             .inject = att_inject,
                             .reset_jac = NULL,
                             .P0 = P0};
    check(EskfInit(&eskf, &ok) == ESKF_OK, "EskfInit 失败");
    float z[3] = {0.1f, -0.2f, 0.97f};
    check(EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK, "Predict 失败");
    check(EskfUpdate(&eskf, z) == ESKF_OK, "Update 失败");
    const int seq1 = (EskfPredict(&eskf, NULL, 0.005f) == ESKF_ERR_CFG);
    const int seq2 = (EskfUpdate(&eskf, z) == ESKF_ERR_CFG);
    const int seq3 = (EskfInject(&eskf) == ESKF_OK) && (EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK);

    /* 奇异保护：δ/P 逐位不变 */
    Eskf_Init_Config_s sing = {.n = N_ATT,
                               .m = M_ATT,
                               .l = 0,
                               .opt = 0,
                               .ctx = &c,
                               .propagate = bad_propagate,
                               .measure = zero_measure,
                               .inject = noop_inject,
                               .reset_jac = NULL,
                               .P0 = P0};
    check(EskfInit(&eskf, &sing) == ESKF_OK, "EskfInit 失败");
    check(EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK, "Predict 失败");
    float ds[N_ATT], Ps[N_ATT * N_ATT];
    memcpy(ds, eskf.delta, sizeof(ds));
    memcpy(Ps, eskf.P, sizeof(Ps));
    const int e_sing = (EskfUpdate(&eskf, z) == ESKF_ERR_SINGULAR);
    const int unchanged = (memcmp(ds, eskf.delta, sizeof(ds)) == 0) && (memcmp(Ps, eskf.P, sizeof(Ps)) == 0);
    /* 奇异后仍可继续 Predict（δ 仍为 0） */
    const int after = (EskfPredict(&eskf, NULL, 0.005f) == ESKF_OK);

    /* m_now 越界 */
    const int e_mnow = (EskfUpdateM(&eskf, 0, z, NULL) == ESKF_ERR_DIM) && (EskfUpdateM(&eskf, M_ATT + 1, z, NULL) == ESKF_ERR_DIM);

    printf("  用例8b 时序/奇异: 越序Predict=%d 越序Update=%d Inject后恢复=%d 奇异=%d 逐位不变=%d 之后可Predict=%d "
           "m_now越界=%d\n",
           seq1, seq2, seq3, e_sing, unchanged, after, e_mnow);
    check(seq1 && seq2 && seq3, "时序约束未生效");
    check(e_sing && unchanged && after, "奇异保护未生效");
    check(e_mnow, "m_now 越界未拒绝");

    /* EskfReset：δ 清零、P 回到 P0 */
    check(EskfReset(&eskf) == ESKF_OK, "Reset 失败");
    int rst = 1;
    for (int i = 0; i < N_ATT; i++)
    {
        if (eskf.delta[i] != 0.0f)
        {
            rst = 0;
        }
        for (int j = 0; j < N_ATT; j++)
        {
            if (eskf.P[i * N_ATT + j] != P0[i * N_ATT + j])
            {
                rst = 0;
            }
        }
    }
    printf("  用例8c Reset(δ=0, P=P0): %d\n", rst);
    check(rst != 0, "Reset 未还原 P0");
}

int main(void)
{
    printf("lib_eskf 姿态测试（独立四元数真值模型）\n");
    printf("========================================\n");
    case_static();
    case_rotation_360();
    case_bias_observable();
    case_inject_and_reset();
    case_reset_jac_impact();
    case_residual_sign();
    case_multirate();
    case_guard();
    printf("========================================\n");
    if (s_fail == 0)
    {
        printf("结果: 全部 PASS\n");
        return 0;
    }
    printf("结果: %d 项 FAIL\n", s_fail);
    return 1;
}
