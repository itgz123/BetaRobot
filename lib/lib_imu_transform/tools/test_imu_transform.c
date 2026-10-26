/**
 * @file test_imu_transform.c
 * @brief lib_imu_transform PC 端单元测试
 *
 * 覆盖：恒等/纯旋转、向心项与切向项符号、alpha 差分/限幅/低通、
 *       dt==0 保持、Reset、旋转前后差分等价性与限幅顺序、配置校验、
 *       错误输入不污染状态、多实例独立。
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lib_imu_transform.h"

static int s_fail;

static void check(int cond, const char *msg)
{
    if (cond != 0)
    {
        printf("ok   : %s\n", msg);
    }
    else
    {
        printf("FAIL : %s\n", msg);
        s_fail++;
    }
}

static void check_near(float got, float want, float tol, const char *msg)
{
    if (fabsf(got - want) > tol)
    {
        printf("FAIL : %s (got %.6f want %.6f)\n", msg, (double)got, (double)want);
        s_fail++;
    }
    else
    {
        printf("ok   : %s\n", msg);
    }
}

static void check_vec(vector3_t got, vector3_t want, float tol, const char *msg)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s.x", msg);
    check_near(got.x, want.x, tol, buf);
    snprintf(buf, sizeof(buf), "%s.y", msg);
    check_near(got.y, want.y, tol, buf);
    snprintf(buf, sizeof(buf), "%s.z", msg);
    check_near(got.z, want.z, tol, buf);
}

static LibImuTransform_Config_s CfgIdentity(void)
{
    LibImuTransform_Config_s c = {0};
    c.x_axis = (vector3_t){1.0f, 0.0f, 0.0f};
    c.y_axis = (vector3_t){0.0f, 1.0f, 0.0f};
    c.z_axis = (vector3_t){0.0f, 0.0f, 1.0f};
    return c;
}

static LibImuTransform_Input_s Input(float gx, float gy, float gz, float ax, float ay, float az, float dt)
{
    LibImuTransform_Input_s in = {0};
    in.gyro = (vector3_t){gx, gy, gz};
    in.acc = (vector3_t){ax, ay, az};
    in.dt = dt;
    in.valid = 1u;
    return in;
}

/* 实车安装：IMU 倒装，R 的三列 = IMU 轴在底盘系的方向 */
#define S45 0.70710678f

static LibImuTransform_Config_s CfgActualMount(void)
{
    LibImuTransform_Config_s c = {0};
    c.x_axis = (vector3_t){-S45, S45, 0.0f};
    c.y_axis = (vector3_t){S45, S45, 0.0f};
    c.z_axis = (vector3_t){0.0f, 0.0f, -1.0f};
    return c;
}

static void TestIdentity(void)
{
    printf("\n[1] ident config\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init ok");

    LibImuTransform_Input_s in = Input(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 0.01f);
    LibImuTransform_Output_s out;
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update ok");
    check_vec(out.gyro, (vector3_t){1.0f, 2.0f, 3.0f}, 1e-6f, "gyro passthrough");
    check_vec(out.acc, (vector3_t){4.0f, 5.0f, 6.0f}, 1e-6f, "acc passthrough");
    check_vec(out.alpha, (vector3_t){0.0f, 0.0f, 0.0f}, 1e-6f, "alpha seeded 0");
    check(out.alpha_valid == 0u, "alpha_valid first tick");
    check(out.fresh == 1u, "fresh first tick");
    check(out.valid == 1u, "valid first tick");

    in = Input(1.1f, 2.0f, 2.9f, 4.0f, 5.0f, 6.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update 2 ok");
    check_vec(out.alpha, (vector3_t){10.0f, 0.0f, -10.0f}, 1e-3f, "alpha diff");
    check(out.alpha_valid == 1u, "alpha_valid second tick");
}

static void TestRotation(void)
{
    printf("\n[2] pure rotation\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgActualMount();
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init actual mount");

    LibImuTransform_Input_s in = Input(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.01f);
    LibImuTransform_Output_s out;
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update actual mount");
    check_vec(out.gyro, (vector3_t){-S45, S45, 0.0f}, 1e-5f, "actual mount gyro");
    check_vec(out.acc, (vector3_t){0.0f, 0.0f, -1.0f}, 1e-5f, "actual mount acc");

    /* Rz(90°) */
    cfg = CfgIdentity();
    cfg.x_axis = (vector3_t){0.0f, 1.0f, 0.0f};
    cfg.y_axis = (vector3_t){-1.0f, 0.0f, 0.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init Rz90");
    in = Input(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update Rz90");
    check_vec(out.gyro, (vector3_t){0.0f, 1.0f, 0.0f}, 1e-6f, "Rz90 gyro");
    check_vec(out.acc, (vector3_t){-1.0f, 0.0f, 0.0f}, 1e-6f, "Rz90 acc");
}

static void TestCentripetal(void)
{
    printf("\n[3] centripetal sign\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    cfg.pos = (vector3_t){1.0f, 0.0f, 0.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init");

    LibImuTransform_Input_s in = Input(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    LibImuTransform_Output_s out;
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "tick 1");
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "tick 2");
    /* a_O = 0 - 0 - omega×(omega×r) = -(0,0,1)×((0,0,1)×(1,0,0)) = (1,0,0) */
    check_vec(out.acc, (vector3_t){1.0f, 0.0f, 0.0f}, 1e-6f, "centripetal compensation");
}

static void TestTangential(void)
{
    printf("\n[4] tangential sign\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    cfg.pos = (vector3_t){0.0f, 0.0f, 1.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init");

    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "tick 2");
    /* alpha=(1,0,0)，omega=0 ⇒ a_O = -alpha×r = -(0,-1,0) = (0,1,0) */
    check_vec(out.alpha, (vector3_t){1.0f, 0.0f, 0.0f}, 1e-6f, "alpha raw");
    check_vec(out.acc, (vector3_t){0.0f, 1.0f, 0.0f}, 1e-6f, "tangential compensation");
}

static void TestLpf(void)
{
    printf("\n[5] alpha LPF\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    cfg.alpha_lpf_rc = 0.02f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init rc=0.02");

    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "tick raw=300");
    check_near(out.alpha.x, 100.0f, 1e-4f, "LPF first step = 100");
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "tick raw=0");
    check_near(out.alpha.x, 66.66667f, 1e-3f, "LPF second step = 66.667");
}

static void TestClampAndDtZero(void)
{
    printf("\n[6/7] clamp + dt==0\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    cfg.alpha_limit = 100.0f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init limit=100");

    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(3.0f, -3.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "clamped tick");
    check_vec(out.alpha, (vector3_t){100.0f, -100.0f, 50.0f}, 1e-4f, "per-axis clamp");

    /* dt == 0：alpha 保持，仍按当前样本变换 */
    in = Input(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 0.0f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "dt=0 tick");
    check_vec(out.alpha, (vector3_t){100.0f, -100.0f, 50.0f}, 1e-4f, "alpha held on dt=0");
    check_vec(out.gyro, (vector3_t){1.0f, 2.0f, 3.0f}, 1e-6f, "dt=0 still transforms");
    check(out.fresh == 0u, "dt=0 fresh=0");
    check(out.valid == 1u, "dt=0 valid from input");
}

static void TestReset(void)
{
    printf("\n[8] reset\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgIdentity();
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init");

    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "build alpha");
    check_near(out.alpha.x, 10.0f, 1e-4f, "alpha before reset");

    LibImuTransformReset(&inst);
    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "post-reset seed");
    check_vec(out.alpha, (vector3_t){0.0f, 0.0f, 0.0f}, 1e-6f, "alpha zero after reset");
    check(out.alpha_valid == 0u, "alpha_valid 0 after reset");
    in = Input(0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "post-reset diff");
    check_near(out.alpha.x, 10.0f, 1e-4f, "alpha resumes after reset");
}

static void TestRotateBeforeDiff(void)
{
    printf("\n[9] rotate-before-diff ordering\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Config_s cfg = CfgActualMount();
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init");

    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(1.5f, 2.5f, 3.5f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "diff");
    /* alpha_I = (50,50,50) -> R·alpha_I = (0, 70.7107, -50) */
    check_vec(out.alpha, (vector3_t){0.0f, S45 * 100.0f, -50.0f}, 1e-4f, "alpha_T = R*alpha_I");

    /* 有限幅时必须先旋转再限幅：raw_T=(-212.13,212.13,0) -> clamp (-100,100,0)；
     * 若先限幅再旋转会得到 (-70.71,70.71,0)。 */
    cfg = CfgActualMount();
    cfg.alpha_limit = 100.0f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init limited");
    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed limited");
    in = Input(300.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "limited diff");
    check_vec(out.alpha, (vector3_t){-100.0f, 100.0f, 0.0f}, 1e-4f, "clamp in target frame");
}

static void TestConfigErrors(void)
{
    printf("\n[10] config / runtime errors\n");
    LibImuTransform_Instance_s inst;
    LibImuTransform_Output_s out;
    LibImuTransform_Input_s in = Input(1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.01f);

    check(LibImuTransformInit(NULL, NULL) == LIB_IMU_TRANSFORM_ERR_ARG, "init NULL");
    LibImuTransform_Config_s cfg = CfgIdentity();
    check(LibImuTransformInit(&inst, NULL) == LIB_IMU_TRANSFORM_ERR_ARG, "init cfg NULL");

    /* 全零三轴 = 单位阵 */
    LibImuTransform_Config_s zero = {0};
    check(LibImuTransformInit(&inst, &zero) == LIB_IMU_TRANSFORM_OK, "all-zero axes = identity");
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update zero cfg");
    check_vec(out.gyro, (vector3_t){1.0f, 2.0f, 3.0f}, 1e-6f, "zero cfg passthrough");

    cfg = CfgIdentity();
    cfg.y_axis = (vector3_t){1.0f, 0.0f, 0.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_ERR_ARG, "non-orthogonal rejected");

    cfg = CfgIdentity();
    cfg.z_axis = (vector3_t){0.0f, 0.0f, -1.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_ERR_ARG, "reflection rejected");

    /* 缩放轴应被归一化接受 */
    cfg = CfgIdentity();
    cfg.x_axis = (vector3_t){2.0f, 0.0f, 0.0f};
    cfg.y_axis = (vector3_t){0.0f, 2.0f, 0.0f};
    cfg.z_axis = (vector3_t){0.0f, 0.0f, 2.0f};
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "scaled axes normalized");
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "update scaled");
    check_vec(out.gyro, (vector3_t){1.0f, 2.0f, 3.0f}, 1e-6f, "scaled axes = identity");

    cfg = CfgIdentity();
    cfg.alpha_lpf_rc = -1.0f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_ERR_ARG, "negative rc rejected");
    cfg = CfgIdentity();
    cfg.alpha_limit = -1.0f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_ERR_ARG, "negative limit rejected");
    cfg = CfgIdentity();
    cfg.pos.x = NAN;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_ERR_ARG, "NaN config rejected");

    /* 未 Init */
    LibImuTransform_Instance_s not_init;
    memset(&not_init, 0, sizeof(not_init));
    check(LibImuTransformUpdate(&not_init, &in, &out) == LIB_IMU_TRANSFORM_ERR_NOT_INIT, "not init rejected");
    check_vec(out.gyro, (vector3_t){0.0f, 0.0f, 0.0f}, 1e-6f, "not init zeroes out");

    /* 运行时错误不污染状态 */
    cfg = CfgIdentity();
    cfg.alpha_limit = 100.0f;
    check(LibImuTransformInit(&inst, &cfg) == LIB_IMU_TRANSFORM_OK, "init for runtime errors");
    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "seed");
    in = Input(NAN, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_ERR_NOT_FINITE, "NaN input rejected");
    check_vec(out.gyro, (vector3_t){0.0f, 0.0f, 0.0f}, 1e-6f, "error zeroes out");
    in = Input(0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_OK, "recover after NaN");
    check_near(out.alpha.x, 10.0f, 1e-4f, "NaN did not pollute gyro_prev");

    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.01f);
    check(LibImuTransformUpdate(&inst, &in, &out) == LIB_IMU_TRANSFORM_ERR_ARG, "negative dt rejected");
    in = Input(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&inst, &in, NULL) == LIB_IMU_TRANSFORM_ERR_ARG, "NULL out rejected");
}

static void TestMultiInstance(void)
{
    printf("\n[11] multiple instances\n");
    LibImuTransform_Instance_s a;
    LibImuTransform_Instance_s b;
    LibImuTransform_Config_s cfg_a = CfgIdentity();
    LibImuTransform_Config_s cfg_b = CfgIdentity();
    cfg_b.x_axis = (vector3_t){0.0f, 1.0f, 0.0f};
    cfg_b.y_axis = (vector3_t){-1.0f, 0.0f, 0.0f};
    check(LibImuTransformInit(&a, &cfg_a) == LIB_IMU_TRANSFORM_OK, "init A");
    check(LibImuTransformInit(&b, &cfg_b) == LIB_IMU_TRANSFORM_OK, "init B");

    LibImuTransform_Output_s out_a;
    LibImuTransform_Output_s out_b;
    LibImuTransform_Input_s in = Input(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.01f);
    check(LibImuTransformUpdate(&a, &in, &out_a) == LIB_IMU_TRANSFORM_OK, "A update");
    check(LibImuTransformUpdate(&b, &in, &out_b) == LIB_IMU_TRANSFORM_OK, "B update");
    check_vec(out_a.gyro, (vector3_t){1.0f, 0.0f, 0.0f}, 1e-6f, "A gyro");
    check_vec(out_b.gyro, (vector3_t){0.0f, 1.0f, 0.0f}, 1e-6f, "B gyro");
}

int main(void)
{
    printf("== lib_imu_transform tests ==\n");

    TestIdentity();
    TestRotation();
    TestCentripetal();
    TestTangential();
    TestLpf();
    TestClampAndDtZero();
    TestReset();
    TestRotateBeforeDiff();
    TestConfigErrors();
    TestMultiInstance();

    printf("\n%s (%d failure%s)\n", (s_fail == 0) ? "ALL PASS" : "FAILED", s_fail, (s_fail == 1) ? "" : "s");
    return (s_fail == 0) ? 0 : 1;
}
