/**
 * @file test_rls.c
 * @brief lib_rls PC 端测试（无硬件依赖，gcc 编译）
 *
 * 用例：
 *   1. 线性模型收敛（无噪声 / 有噪声）：y = φᵀθ，验证参数与预测
 *   2. 轮阻力模型：y = m·a + Fc·sign(v) + B·v + bias，验证 4 维回归
 *   3. 遗忘因子：参数中途改变，λ<1 能跟踪、λ=1 不跟踪
 *   4. 配置校验：非法 λ/P/参数范围/策略/初值 → ERR_ARG 且不初始化实例
 *   5. 非有限输入：NaN/Inf 按 FREEZE / RESET 两种策略处理；冻结后拒绝更新
 *   6. innovation 限幅：离群观测只按限幅值更新
 *   7. 参数上下限：估计被钳在 [param_min, param_max]
 *   8. 协方差保护：对角下限 p_min / 元素上限 p_max / 对称性
 *   9. SetParams / SetLambda / Reset 语义
 *  10. 空指针与未初始化实例
 *  11. 多实例互不干扰（无全局状态）
 *
 * 编译见同目录 test_rls.sh；退出码 0 = 全部 PASS。
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lib_rls.h"

/*============================ 确定性伪随机 ============================*/

static unsigned int s_seed = 12345u;

static double urand(void)
{
    s_seed = s_seed * 1103515245u + 12345u;
    return (double)((s_seed >> 8) & 0xFFFFFFu) / 16777216.0;
}

/** @brief [-1, 1] 均匀 */
static float srand1(void)
{
    return (float)(2.0 * urand() - 1.0);
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

static void group(const char *name)
{
    printf("\n== %s ==\n", name);
}

/** @brief 默认配置：λ=1、P0=1000、参数不限 */
static LibRls_Config_s default_cfg(void)
{
    LibRls_Config_s cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.lambda = 1.0f;
    cfg.p_init = 1000.0f;
    cfg.p_min = 0.0f;
    cfg.p_max = 1.0e6f;
    cfg.param_min = -3.0e38f;
    cfg.param_max = 3.0e38f;
    cfg.innovation_limit = 0.0f;
    cfg.fail_policy = LIB_RLS_FAIL_FREEZE;
    cfg.theta_init = NULL;
    return cfg;
}

static int near(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

/*============================ 1. 线性收敛 ============================*/

static void test_linear_convergence(void)
{
    const float truth[3] = {2.0f, 3.0f, -0.5f};
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 3);
    float phi[3];
    int k;
    int i;

    group("1. 线性模型收敛");

    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init");
    for (k = 0; k < 1000; k++)
    {
        phi[0] = 1.0f;
        phi[1] = srand1();
        phi[2] = srand1();
        if (LibRlsUpdate(&rls, phi, truth[0] * phi[0] + truth[1] * phi[1] + truth[2] * phi[2]) != LIB_RLS_OK)
        {
            check(0, "Update 无噪声");
            break;
        }
    }
    check(rls.update_count == 1000u, "update_count == 1000");
    for (i = 0; i < 3; i++)
    {
        check(near(rls.theta[i], truth[i], 1.0e-3f), "无噪声参数收敛");
    }

    {
        const float phi_p[3] = {1.0f, 0.25f, -0.75f};
        float expect = truth[0] * phi_p[0] + truth[1] * phi_p[1] + truth[2] * phi_p[2];
        check(near(LibRlsPredict(&rls, phi_p), expect, 1.0e-3f), "Predict == 解析值");
    }

    /* P 对称 + 迹有限 */
    check(LIB_RLS_P(&rls, 0, 1) == LIB_RLS_P(&rls, 1, 0), "P 对称 01");
    check(LIB_RLS_P(&rls, 0, 2) == LIB_RLS_P(&rls, 2, 0), "P 对称 02");
    check(LIB_RLS_P(&rls, 1, 2) == LIB_RLS_P(&rls, 2, 1), "P 对称 12");
    {
        float trace = LIB_RLS_P(&rls, 0, 0) + LIB_RLS_P(&rls, 1, 1) + LIB_RLS_P(&rls, 2, 2);
        check(isfinite(trace) != 0, "tr(P) 有限");
    }
    check(rls.status == LIB_RLS_OK && rls.frozen == 0u, "状态 OK");

    /* 有噪声：均值仍应收敛 */
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init 有噪声");
    for (k = 0; k < 5000; k++)
    {
        float noise = 0.01f * srand1();
        phi[0] = 1.0f;
        phi[1] = srand1();
        phi[2] = srand1();
        LibRlsUpdate(&rls, phi, truth[0] + truth[1] * phi[1] + truth[2] * phi[2] + noise);
    }
    for (i = 0; i < 3; i++)
    {
        check(near(rls.theta[i], truth[i], 0.02f), "有噪声参数收敛");
    }
}

/*============================ 2. 轮阻力模型 ============================*/

static void test_wheel_resistance_model(void)
{
    const float truth[4] = {25.0f, 8.0f, 5.0f, 1.0f}; /* m, Fc, B, bias */
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 4);
    float phi[4];
    int k;

    group("2. 轮阻力模型（4 维）");

    cfg.p_min = 1.0e-9f;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init");

    for (k = 0; k < 4000; k++)
    {
        float v = 0.0f;
        while (fabsf(v) < 0.1f)
        {
            v = 2.0f * srand1(); /* [-2, 2]，避开 0 附近 */
        }
        phi[0] = 1.0f * srand1();           /* a */
        phi[1] = (v > 0.0f) ? 1.0f : -1.0f; /* sign(v) */
        phi[2] = v;
        phi[3] = 1.0f;
        LibRlsUpdate(&rls, phi, truth[0] * phi[0] + truth[1] * phi[1] + truth[2] * phi[2] + truth[3]);
    }

    check(near(rls.theta[0], truth[0], 0.02f), "m_eff");
    check(near(rls.theta[1], truth[1], 0.02f), "Fc");
    check(near(rls.theta[2], truth[2], 0.02f), "B");
    check(near(rls.theta[3], truth[3], 0.02f), "bias");
}

/*============================ 3. 遗忘因子 ============================*/

static void test_forgetting(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 1);
    float phi[1] = {1.0f};
    int k;

    group("3. 遗忘因子");

    cfg.lambda = 0.98f;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init λ=0.98");
    for (k = 0; k < 200; k++)
    {
        LibRlsUpdate(&rls, phi, 1.0f);
    }
    check(near(rls.theta[0], 1.0f, 0.02f), "第一阶段 θ≈1");
    for (k = 0; k < 2000; k++)
    {
        LibRlsUpdate(&rls, phi, 3.0f);
    }
    check(near(rls.theta[0], 3.0f, 0.05f), "λ<1 跟踪到 θ≈3");

    /* λ=1：同样数据下估计停在加权平均，明显跟不上新值 */
    cfg.lambda = 1.0f;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init λ=1");
    for (k = 0; k < 200; k++)
    {
        LibRlsUpdate(&rls, phi, 1.0f);
    }
    for (k = 0; k < 2000; k++)
    {
        LibRlsUpdate(&rls, phi, 3.0f);
    }
    check(fabsf(rls.theta[0] - 3.0f) > 0.1f, "λ=1 不跟踪突变");
}

/*============================ 4. 配置校验 ============================*/

static void test_invalid_config(void)
{
    LibRls_Config_s cfg;
    LIB_RLS_INSTANCE_DEF(rls, 2);
    float bad_theta[2] = {1.0f, NAN};

    group("4. 配置校验");

    check(LibRlsInit(NULL, &cfg) == LIB_RLS_ERR_ARG, "inst=NULL");
    check(LibRlsInit(&rls, NULL) == LIB_RLS_ERR_ARG, "cfg=NULL");

#define EXPECT_ARG(expr, msg)                                                                                                                                  \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        check((expr) == LIB_RLS_ERR_ARG, (msg));                                                                                                               \
        check(rls.initialized == 0u, "失败时不初始化实例");                                                                                                    \
    } while (0)

    cfg = default_cfg();
    cfg.lambda = 0.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "lambda=0");

    cfg = default_cfg();
    cfg.lambda = 1.1f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "lambda>1");

    cfg = default_cfg();
    cfg.lambda = NAN;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "lambda=NaN");

    cfg = default_cfg();
    cfg.p_init = 0.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "p_init=0");

    cfg = default_cfg();
    cfg.p_init = -1.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "p_init<0");

    cfg = default_cfg();
    cfg.p_min = -1.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "p_min<0");

    cfg = default_cfg();
    cfg.p_min = 10.0f;
    cfg.p_max = 1.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "p_min>p_max");

    cfg = default_cfg();
    cfg.p_init = 2000.0f; /* p_max 默认 1e6，这里先收窄 */
    cfg.p_max = 1000.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "p_init>p_max");

    cfg = default_cfg();
    cfg.param_min = 1.0f;
    cfg.param_max = -1.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "param_min>param_max");

    cfg = default_cfg();
    cfg.innovation_limit = -1.0f;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "innovation_limit<0");

    cfg = default_cfg();
    cfg.fail_policy = (LibRls_FailPolicy_e)7;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "fail_policy 非法");

    cfg = default_cfg();
    cfg.theta_init = bad_theta;
    EXPECT_ARG(LibRlsInit(&rls, &cfg), "theta_init 含 NaN");

    /* 失败路径不应改写配置字段（宏实例原本全零） */
    check(rls.lambda == 0.0f && rls.p_init == 0.0f, "失败时不改写实例配置");

#undef EXPECT_ARG
}

/*============================ 5. 非有限输入 ============================*/

static void test_not_finite(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 2);
    float phi[2] = {1.0f, 0.5f};
    float saved[2];
    uint32_t count_before;

    group("5. 非有限输入 / 冻结 / 复位");

    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init FREEZE");
    {
        int k;
        for (k = 0; k < 10; k++)
        {
            LibRlsUpdate(&rls, phi, 2.0f);
        }
    }
    memcpy(saved, rls.theta, sizeof(saved));
    count_before = rls.update_count;

    check(LibRlsUpdate(&rls, phi, NAN) == LIB_RLS_ERR_NOT_FINITE, "y=NaN → NOT_FINITE");
    check(rls.frozen == 1u, "FREEZE: frozen=1");
    check(rls.status == LIB_RLS_ERR_NOT_FINITE, "status 保留根因");
    check(memcmp(saved, rls.theta, sizeof(saved)) == 0, "θ 不被改写");
    check(rls.update_count == count_before, "update_count 不变");

    check(LibRlsUpdate(&rls, phi, 2.0f) == LIB_RLS_ERR_FROZEN, "冻结后拒绝更新");
    check(memcmp(saved, rls.theta, sizeof(saved)) == 0, "冻结后 θ 仍不变");

    check(LibRlsReset(&rls) == LIB_RLS_OK, "Reset");
    check(rls.frozen == 0u && rls.update_count == 0u, "Reset 清冻结/计数");
    check(rls.theta[0] == 0.0f && rls.theta[1] == 0.0f, "Reset 恢复初值");
    check(LibRlsUpdate(&rls, phi, 2.0f) == LIB_RLS_OK, "Reset 后可更新");

    /* phi 含 Inf */
    {
        float bad_phi[2] = {1.0f, INFINITY};
        check(LibRlsUpdate(&rls, bad_phi, 2.0f) == LIB_RLS_ERR_NOT_FINITE, "phi=Inf → NOT_FINITE");
    }

    /* RESET 策略：失败后自动恢复初值，且可继续更新 */
    cfg.fail_policy = LIB_RLS_FAIL_RESET;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init RESET");
    {
        int k;
        for (k = 0; k < 10; k++)
        {
            LibRlsUpdate(&rls, phi, 2.0f);
        }
    }
    check(LibRlsUpdate(&rls, phi, NAN) == LIB_RLS_ERR_NOT_FINITE, "RESET: NaN → NOT_FINITE");
    check(rls.frozen == 0u, "RESET: 不冻结");
    check(rls.update_count == 0u, "RESET: 计数归零");
    check(rls.theta[0] == 0.0f && rls.theta[1] == 0.0f, "RESET: θ 恢复初值");
    check(rls.status == LIB_RLS_ERR_NOT_FINITE, "RESET: status 保留根因");
    check(LibRlsUpdate(&rls, phi, 2.0f) == LIB_RLS_OK, "RESET 后可继续");
}

/*============================ 6. innovation 限幅 ============================*/

static void test_innovation_limit(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 1);
    float phi[1] = {1.0f};

    group("6. innovation 限幅");

    cfg.p_init = 1.0f;
    cfg.innovation_limit = 0.5f;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init");
    check(LibRlsUpdate(&rls, phi, 100.0f) == LIB_RLS_OK, "Update");
    /* K = P/(λ+P) = 0.5；残差限到 0.5 ⇒ θ = 0.25 */
    check(near(rls.theta[0], 0.25f, 1.0e-5f), "θ 只按限幅值更新");
    check(near(rls.last_innovation, 100.0f, 1.0e-3f), "last_innovation 保留原始残差");
    check(rls.last_prediction == 0.0f, "last_prediction == 0");
}

/*============================ 7. 参数上下限 ============================*/

static void test_param_bounds(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 1);
    float phi[1] = {1.0f};
    int k;

    group("7. 参数上下限");

    cfg.p_init = 1.0f;
    cfg.param_min = 0.0f;
    cfg.param_max = 1.0f;
    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init");

    for (k = 0; k < 100; k++)
    {
        LibRlsUpdate(&rls, phi, 10.0f);
    }
    check(rls.theta[0] == 1.0f, "上限钳位 θ=1");

    for (k = 0; k < 100; k++)
    {
        LibRlsUpdate(&rls, phi, -10.0f);
    }
    check(rls.theta[0] == 0.0f, "下限钳位 θ=0");

    /* SetParams 同样钳位 */
    {
        float p[1] = {5.0f};
        check(LibRlsSetParams(&rls, p) == LIB_RLS_OK, "SetParams");
        check(rls.theta[0] == 1.0f, "SetParams 上限钳位");
        p[0] = -5.0f;
        check(LibRlsSetParams(&rls, p) == LIB_RLS_OK, "SetParams 负值");
        check(rls.theta[0] == 0.0f, "SetParams 下限钳位");
    }
}

/*============================ 8. 协方差保护 ============================*/

static void test_covariance_bounds(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls1, 1);
    LIB_RLS_INSTANCE_DEF(rls2, 2);
    float phi[1] = {1.0f};
    int k;

    group("8. 协方差保护");

    cfg.p_init = 1.0f;
    cfg.p_min = 1.0e-3f;
    check(LibRlsInit(&rls1, &cfg) == LIB_RLS_OK, "Init 下限");
    for (k = 0; k < 5000; k++)
    {
        LibRlsUpdate(&rls1, phi, 0.0f); /* θ=0，innovation=0，P 自然收缩 */
    }
    check(LIB_RLS_P(&rls1, 0, 0) >= cfg.p_min - 1.0e-9f, "P 对角不低于 p_min");
    check(LIB_RLS_P(&rls1, 0, 0) <= cfg.p_min + 1.0e-6f, "P 停在 p_min 附近");

    cfg = default_cfg();
    cfg.p_init = 1.0f;
    cfg.p_max = 2.0f;
    cfg.lambda = 0.5f;
    check(LibRlsInit(&rls1, &cfg) == LIB_RLS_OK, "Init 上限");
    for (k = 0; k < 100; k++)
    {
        LibRlsUpdate(&rls1, phi, 0.0f);
    }
    check(LIB_RLS_P(&rls1, 0, 0) <= cfg.p_max + 1.0e-6f, "P 元素不超过 p_max");

    /* 二维对称性 */
    cfg = default_cfg();
    check(LibRlsInit(&rls2, &cfg) == LIB_RLS_OK, "Init 2D");
    {
        int i;
        for (i = 0; i < 100; i++)
        {
            float ph[2] = {1.0f, srand1()};
            LibRlsUpdate(&rls2, ph, srand1());
        }
    }
    check(LIB_RLS_P(&rls2, 0, 1) == LIB_RLS_P(&rls2, 1, 0), "2D P 对称");
}

/*============================ 9. SetParams / SetLambda / Reset ============================*/

static void test_setters_reset(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 2);
    float theta[2] = {1.5f, -2.5f};
    float phi[2] = {2.0f, 3.0f};

    group("9. SetParams / SetLambda / Reset");

    check(LibRlsInit(&rls, &cfg) == LIB_RLS_OK, "Init");
    check(LibRlsSetParams(&rls, theta) == LIB_RLS_OK, "SetParams");
    check(near(LibRlsPredict(&rls, phi), -4.5f, 1.0e-6f), "Predict 使用新参数");
    check(LibRlsReset(&rls) == LIB_RLS_OK, "Reset");
    check(rls.theta[0] == 1.5f && rls.theta[1] == -2.5f, "Reset 恢复到 SetParams 的基准");

    {
        float bad[2] = {1.0f, NAN};
        check(LibRlsSetParams(&rls, bad) == LIB_RLS_ERR_ARG, "SetParams NaN → ARG");
        check(rls.theta[1] == -2.5f, "失败时 θ 不变");
    }

    check(LibRlsSetLambda(&rls, 0.99f) == LIB_RLS_OK, "SetLambda 0.99");
    check(rls.lambda == 0.99f, "lambda 已更新");
    check(LibRlsSetLambda(&rls, 0.0f) == LIB_RLS_ERR_ARG, "SetLambda 0 → ARG");
    check(LibRlsSetLambda(&rls, 1.01f) == LIB_RLS_ERR_ARG, "SetLambda >1 → ARG");
    check(LibRlsSetLambda(&rls, NAN) == LIB_RLS_ERR_ARG, "SetLambda NaN → ARG");
    check(rls.lambda == 0.99f, "非法 lambda 不改写");
}

/*============================ 10. 空指针 / 未初始化 ============================*/

static void test_null_and_uninit(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(rls, 1);
    float phi[1] = {1.0f};

    group("10. 空指针 / 未初始化");

    check(LibRlsPredict(NULL, phi) == 0.0f, "Predict(NULL)");
    check(LibRlsReset(NULL) == LIB_RLS_ERR_ARG, "Reset(NULL)");
    check(LibRlsSetParams(NULL, phi) == LIB_RLS_ERR_ARG, "SetParams(NULL)");
    check(LibRlsSetLambda(NULL, 0.99f) == LIB_RLS_ERR_ARG, "SetLambda(NULL)");
    check(LibRlsUpdate(NULL, phi, 0.0f) == LIB_RLS_ERR_ARG, "Update(NULL)");

    check(rls.initialized == 0u, "宏实例初始未初始化");
    check(LibRlsUpdate(&rls, phi, 0.0f) == LIB_RLS_ERR_ARG, "Update(未初始化)");
    check(LibRlsReset(&rls) == LIB_RLS_ERR_ARG, "Reset(未初始化)");
    check(LibRlsPredict(&rls, phi) == 0.0f, "Predict(未初始化)");
    (void)cfg;
}

/*============================ 11. 多实例独立 ============================*/

static void test_independent_instances(void)
{
    LibRls_Config_s cfg = default_cfg();
    LIB_RLS_INSTANCE_DEF(a, 1);
    LIB_RLS_INSTANCE_DEF(b, 1);
    float phi[1] = {1.0f};
    int k;

    group("11. 多实例独立");

    check(LibRlsInit(&a, &cfg) == LIB_RLS_OK, "Init A");
    check(LibRlsInit(&b, &cfg) == LIB_RLS_OK, "Init B");
    for (k = 0; k < 500; k++)
    {
        LibRlsUpdate(&a, phi, 5.0f);
        LibRlsUpdate(&b, phi, -2.0f);
    }
    check(near(a.theta[0], 5.0f, 0.01f), "A 收敛到 5");
    check(near(b.theta[0], -2.0f, 0.01f), "B 收敛到 -2");
}

/*============================ main ============================*/

int main(void)
{
    printf("lib_rls PC test\n");

    test_linear_convergence();
    test_wheel_resistance_model();
    test_forgetting();
    test_invalid_config();
    test_not_finite();
    test_innovation_limit();
    test_param_bounds();
    test_covariance_bounds();
    test_setters_reset();
    test_null_and_uninit();
    test_independent_instances();

    printf("\n%s（失败 %d 项）\n", (s_fail == 0) ? "PASS" : "FAIL", s_fail);
    return (s_fail == 0) ? 0 : 1;
}
