/**
 * @file test_trig_lut.c
 * @brief PC 端查表三角函数检验程序（gcc 编译，double 三角函数作真值参考）
 *
 * @note 用法（在 tools/ 目录下）：
 *       gcc -O2 -Wall -Wextra -DBSP_MATH_TRIG_TABLE_SIZE=4096 \
 *           -DTEST_MODE_A_TARGET=1.1920928955078125e-07 \
 *           -I .. -o test_trig_lut test_trig_lut.c ../bsp_math_trig_lut.c -lm
 *       ./test_trig_lut
 *
 * @note 两类检验：
 *       - ModeA（纯插值+表量化）：精确 double 相位喂入插值核心，判定"查表线性
 *         插值误差"是否小于 TEST_MODE_A_TARGET（满精度档取 float32 机器精度
 *         ε = 2^-23 ≈ 1.192e-7）。
 *       - ModeB（整管 float32）：全区域抽样所有点，跑完整 BSP_Math_SinLUT/
 *         CosLUT/SinCosLUT/TanLUT 与 double sin/cos/tan 比对，含归一/象限/索引
 *         计算的 float32 舍入，报告 max/rms。
 * @note 退出码：ModeA 或 ModeB 超限则返回非零。
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "bsp_math_trig_lut.h"

#define PI_D      (3.14159265358979323846264338327950288)
#define TWO_PI_D  (2.0 * PI_D)
#define QUARTER_D (0.5 * PI_D)
#define EPS_F32   (1.1920928955078125e-07)   /* 2^-23 */

#ifndef TEST_MODE_A_TARGET
#define TEST_MODE_A_TARGET EPS_F32
#endif

static double g_sin_max = 0.0, g_cos_max = 0.0, g_tan_max = 0.0;
static double g_sin_rms = 0.0, g_cos_rms = 0.0, g_tan_rms = 0.0;
static double g_cons_max = 0.0;      /* CosLUT(θ) 与 SinLUT(θ+π/2) 一致性       */
static double g_sincos_max = 0.0;    /* SinCosLUT 输出与单独调用一致性           */
static unsigned long g_n_sin = 0, g_n_cos = 0, g_n_tan = 0;

static void upd(double e, double *mx, double *rms, unsigned long *n)
{
    if (e > *mx)
    {
        *mx = e;
    }
    *rms += e * e;
    (*n)++;
}

/*---------- Mode A：纯插值 + 表量化误差 ----------*/
static double mode_a_max(void)
{
    const int M = BSP_MATH_TRIG_TABLE_SIZE;
    const int K = 32;                     /* 每个区间 32 个采样偏移 */
    double mx = 0.0;
    long k;

    for (k = 0; k <= (long)M * K; k++)
    {
        double u = (double)k / (double)K; /* [0, M]，含所有表索引点 */
        long i = (long)floor(u);
        double frac = u - (double)i;
        if (i >= M)
        {
            i = M - 1;
            frac = 1.0;
        }
        double v0 = (double)BSP_Math_SinTable[i];
        double v1 = (double)BSP_Math_SinTable[i + 1];
        double got = v0 + frac * (v1 - v0);
        double ref = sin(u / (double)M * QUARTER_D);
        double e = fabs(got - ref);
        if (e > mx)
        {
            mx = e;
        }
    }
    return mx;
}

/*---------- Mode B：整管 float32 管线 ----------*/
static void run_pipeline(double theta)
{
    float thf = (float)theta;
    double thd = (double)thf;
    double rs = sin(thd), rc = cos(thd);
    float s = BSP_Math_SinLUT(thf);
    float c = BSP_Math_CosLUT(thf);

    upd(fabs((double)s - rs), &g_sin_max, &g_sin_rms, &g_n_sin);
    upd(fabs((double)c - rc), &g_cos_max, &g_cos_rms, &g_n_cos);

    /* tan：排除 cos≈0 的极点附近（两边都发散，比较无意义） */
    if (fabs(rc) > 1e-3)
    {
        float t = BSP_Math_TanLUT(thf);
        double rt = tan(thd);
        upd(fabs((double)t - rt), &g_tan_max, &g_tan_rms, &g_n_tan);
    }

    /* 一致性：cos(x) == sin(x + π/2) */
    {
        double e = fabs((double)BSP_Math_SinLUT(thf + (float)QUARTER_D) - (double)c);
        if (e > g_cons_max)
        {
            g_cons_max = e;
        }
    }
    /* 一致性：SinCosLUT 输出与单独调用一致 */
    {
        float ss, cc;
        double e1, e2;
        BSP_Math_SinCosLUT(thf, &ss, &cc);
        e1 = fabs((double)ss - (double)s);
        e2 = fabs((double)cc - (double)c);
        if (e1 > g_sincos_max)
        {
            g_sincos_max = e1;
        }
        if (e2 > g_sincos_max)
        {
            g_sincos_max = e2;
        }
    }
}

static void test_grid(const char *name, double lo, double hi, int num)
{
    int k;
    for (k = 0; k < num; k++)
    {
        run_pipeline(lo + (hi - lo) * (double)k / (double)num);
    }
    printf("  grid %-22s %10d pts\n", name, num);
}

int main(void)
{
    const int M = BSP_MATH_TRIG_TABLE_SIZE;
    const int num = (4 * M * 16 > (1 << 16)) ? 4 * M * 16 : (1 << 16);
    static const double special[] = {0.0, QUARTER_D, PI_D, 3.0 * QUARTER_D,
                                     TWO_PI_D - 1e-3, QUARTER_D - 1e-4,
                                     QUARTER_D + 1e-4, -1e-4, 1e-4,
                                     -PI_D, -TWO_PI_D, TWO_PI_D + 1e-3};
    size_t k;
    double modeb_gate;
    int fail = 0;

    printf("=== test_trig_lut: BSP_MATH_TRIG_TABLE_SIZE = %d ===\n", M);
    printf("表条目 %d（约 %u KB flash）\n", M + 1, (unsigned)((M + 1) * 4u / 1024u));

    /* Mode A */
    {
        double ma = mode_a_max();
        int ok = ma < TEST_MODE_A_TARGET;
        printf("ModeA 纯插值+量化 max = %.3e   目标 < %.3e  [%s]\n",
               ma, (double)TEST_MODE_A_TARGET, ok ? "PASS" : "FAIL");
        if (!ok)
        {
            fail = 1;
        }
    }

    /* Mode B：各网格 + 特殊边界点 */
    test_grid("[0, 2pi)", 0.0, TWO_PI_D, num);
    test_grid("[-4pi, 0)", -4.0 * PI_D, 0.0, num);
    for (k = 0; k < sizeof(special) / sizeof(special[0]); k++)
    {
        run_pipeline(special[k]);
    }
    printf("  grid %-22s %10d pts\n", "特殊边界点", (int)(sizeof(special) / sizeof(special[0])));

    printf("ModeB sin  整管 max = %.3e  rms = %.3e  (%lu pts)\n",
           g_sin_max, sqrt(g_sin_rms / (double)g_n_sin), g_n_sin);
    printf("ModeB cos  整管 max = %.3e  rms = %.3e  (%lu pts)\n",
           g_cos_max, sqrt(g_cos_rms / (double)g_n_cos), g_n_cos);
    if (g_n_tan > 0)
    {
        printf("ModeB tan  整管 max = %.3e  rms = %.3e  (%lu pts, |cos|>1e-3)\n",
               g_tan_max, sqrt(g_tan_rms / (double)g_n_tan), g_n_tan);
    }
    printf("一致性 cos(x)=sin(x+pi/2)   max = %.3e\n", g_cons_max);
    printf("一致性 SinCos 与单独调用    max = %.3e\n", g_sincos_max);

    /* ModeB 门限：与尺寸相关（含 float32 索引舍入），只用于抓逻辑错误 */
    modeb_gate = (TEST_MODE_A_TARGET * 8.0 > 2.0e-6) ? TEST_MODE_A_TARGET * 8.0 : 2.0e-6;
    printf("ModeB 门限 < %.3e\n", modeb_gate);
    if (g_sin_max >= modeb_gate || g_cos_max >= modeb_gate)
    {
        printf("[FAIL] ModeB sin/cos 超限\n");
        fail = 1;
    }
    if (g_cons_max > 1e-6 || g_sincos_max > 1e-6)
    {
        printf("[FAIL] 一致性超限\n");
        fail = 1;
    }

    /* 大角度信息（float32 归一回舍入固有，不作判定） */
    {
        static const double big[] = {50.0, 100.0, 500.0, 1000.0, -50.0, -500.0, -1000.0};
        double mx = 0.0;
        for (k = 0; k < sizeof(big) / sizeof(big[0]); k++)
        {
            double thf = (double)(float)big[k];
            double e = fabs((double)BSP_Math_SinLUT((float)big[k]) - sin(thf));
            if (e > mx)
            {
                mx = e;
            }
        }
        printf("大角度 |theta|<=1000 sin 信息 max = %.3e（不作判定）\n", mx);
    }

    printf("\n结果: %s\n", fail ? "FAIL" : "PASS");
    return fail;
}
