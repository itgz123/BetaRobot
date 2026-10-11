/**
 * @file test_traj.c
 * @brief lib_traj + lib_traj_jerk PC 端测试
 *
 * 用例：
 *   1. 构造函数回归：ConstJerk / ConstSnap 的端点 p/v/a/j 与解析值一致；
 *      Cubic / QuarticHermite 的边界条件逐条满足；MakeRaw 非法参数返回 -1
 *   2. 采样等价：LibTrajEval 与逐步 LibTrajStep 逐点一致；t<0 / t>total 钳位；
 *      越界/空轨迹的 finished 语义
 *   3. 规划器随机压力（默认 20 万组）：终态精度 + 全程限幅 + 段间 p/v/a 连续；
 *      并把每个 ERR_INFEASIBLE 按"a 的端点引理"分类成 明显无解 / 临界 / 待复核
 *      （待复核 = 定理都看不出越限，说明可能漏解 → 这类必须为 0）
 *   4. 不可行 / 参数错误：返回对应错误码且 traj 逐字节未变
 *   5. 覆盖用例：静止↔静止、同向非零初速、反向初速、带初加速度、极短位移、
 *      纯消加速度（v0==v1 且 dp==0 但 a0≠a1）、jmax 极大/极小；
 *      并对"必然可行"的族（v0==v1 且 a0==a1==0）断言不被拒
 *
 * 编译见同目录 test_traj.sh；退出码 0 = 全部 PASS。
 * 用法：test_traj [--trials N] [--seed S] [--verbose]
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_traj.h"
#include "lib_traj_jerk.h"

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

/** @brief [-1, 1] 均匀 */
static float srand1(void)
{
    return (float)(2.0 * urand() - 1.0);
}

static int s_fail;
static int s_verbose;

static void check(int cond, const char *msg)
{
    if (!cond)
    {
        printf("    [FAIL] %s\n", msg);
        s_fail++;
    }
}

/** @brief 带数值的检查（失败时打印实测值，便于定位） */
static void checkf(int cond, const char *msg, double got, double tol)
{
    if (!cond)
    {
        printf("    [FAIL] %s（实测 %.6g，容差 %.3g）\n", msg, got, tol);
        s_fail++;
    }
}

/*============================ 工具 ============================*/

static float fmax3(float a, float b, float c)
{
    float m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

/** @brief 段/轨迹级极值统计（用于打印证据） */
typedef struct
{
    float v_over; /* max(|v| - vmax) */
    float a_over;
    float j_over;
} LimitStat;

static void stat_walk(const LibTraj_Traj_s *traj, float V, float A, float J, int nsamp, LimitStat *st)
{
    st->v_over = 0.0f;
    st->a_over = 0.0f;
    st->j_over = 0.0f;

    for (uint16_t i = 0; i < traj->seg_num; i++)
    {
        const LibTraj_Seg_s *seg = &traj->seg[i];
        for (int k = 0; k <= nsamp; k++)
        {
            float tau = seg->duration * (float)k / (float)nsamp;
            LibTraj_Output_s o;
            LibTrajSegEval(seg, tau, &o);
            float vo = fabsf(o.velocity) - V;
            float ao = fabsf(o.acceleration) - A;
            float jo = fabsf(o.jerk) - J;
            if (vo > st->v_over)
            {
                st->v_over = vo;
            }
            if (ao > st->a_over)
            {
                st->a_over = ao;
            }
            if (jo > st->j_over)
            {
                st->j_over = jo;
            }
        }
    }
}

/** @brief 段间 p/v/a 连续性：前段末端 vs 后段起点 */
static float max_discontinuity(const LibTraj_Traj_s *traj)
{
    float worst = 0.0f;
    for (uint16_t i = 1; i < traj->seg_num; i++)
    {
        LibTraj_Output_s e, b;
        LibTrajSegEval(&traj->seg[i - 1], traj->seg[i - 1].duration, &e);
        LibTrajSegEval(&traj->seg[i], 0.0f, &b);
        float d = fmax3(fabsf(e.position - b.position), fabsf(e.velocity - b.velocity), fabsf(e.acceleration - b.acceleration));
        if (d > worst)
        {
            worst = d;
        }
    }
    return worst;
}

/*============================ 1. 构造函数 ============================*/

static void test_constructors(void)
{
    printf("[1] 构造函数回归\n");
    LibTraj_Output_s o;
    const float eps = 1e-5f;

    /* ConstJerk：解析值 p0+v0h+a0h²/2+jh³/6 */
    {
        float p0 = -1.25f, v0 = 3.5f, a0 = -7.0f, j = 12.0f, h = 0.37f;
        LibTraj_Seg_s seg;
        LibTrajSegMakeConstJerk(&seg, p0, v0, a0, j, h);
        check(seg.degree == 3, "ConstJerk degree 应为 3");

        LibTrajSegEval(&seg, 0.0f, &o);
        checkf(fabsf(o.position - p0) < eps, "ConstJerk τ=0 位置", o.position - p0, eps);
        checkf(fabsf(o.velocity - v0) < eps, "ConstJerk τ=0 速度", o.velocity - v0, eps);
        checkf(fabsf(o.acceleration - a0) < eps, "ConstJerk τ=0 加速度", o.acceleration - a0, eps);
        checkf(fabsf(o.jerk - j) < eps, "ConstJerk 全程 jerk", o.jerk - j, eps);

        LibTrajSegEval(&seg, h, &o);
        checkf(fabsf(o.position - (p0 + v0 * h + 0.5f * a0 * h * h + j * h * h * h / 6.0f)) < eps, "ConstJerk τ=h 位置", o.position, eps);
        checkf(fabsf(o.velocity - (v0 + a0 * h + 0.5f * j * h * h)) < eps, "ConstJerk τ=h 速度", o.velocity, eps);
        checkf(fabsf(o.acceleration - (a0 + j * h)) < eps, "ConstJerk τ=h 加速度", o.acceleration, eps);
        check(o.finished == 1, "ConstJerk τ=h finished");
    }

    /* ConstSnap：解析值多一项 S/24 */
    {
        float p0 = 0.5f, v0 = -2.0f, a0 = 1.0f, j0 = -3.0f, S = 40.0f, h = 0.25f;
        LibTraj_Seg_s seg;
        LibTrajSegMakeConstSnap(&seg, p0, v0, a0, j0, S, h);
        check(seg.degree == 4, "ConstSnap degree 应为 4");

        LibTrajSegEval(&seg, 0.0f, &o);
        checkf(fabsf(o.jerk - j0) < eps, "ConstSnap τ=0 jerk", o.jerk - j0, eps);

        LibTrajSegEval(&seg, h, &o);
        checkf(fabsf(o.position - (p0 + v0 * h + 0.5f * a0 * h * h + j0 * h * h * h / 6.0f + S * h * h * h * h / 24.0f)) < eps, "ConstSnap τ=h 位置",
               o.position, eps);
        checkf(fabsf(o.velocity - (v0 + a0 * h + 0.5f * j0 * h * h + S * h * h * h / 6.0f)) < eps, "ConstSnap τ=h 速度", o.velocity, eps);
        checkf(fabsf(o.acceleration - (a0 + j0 * h + 0.5f * S * h * h)) < eps, "ConstSnap τ=h 加速度", o.acceleration, eps);
        checkf(fabsf(o.jerk - (j0 + S * h)) < eps, "ConstSnap τ=h jerk", o.jerk, eps);
    }

    /* CubicHermite：钉 p/v 两端 */
    {
        float p0 = 1.0f, v0 = 0.5f, p1 = 4.0f, v1 = -1.5f, h = 0.8f;
        LibTraj_Seg_s seg;
        LibTrajSegMakeCubicHermite(&seg, p0, v0, p1, v1, h);
        LibTrajSegEval(&seg, 0.0f, &o);
        checkf(fabsf(o.position - p0) < eps, "CubicHermite 起点位置", o.position - p0, eps);
        checkf(fabsf(o.velocity - v0) < eps, "CubicHermite 起点速度", o.velocity - v0, eps);
        LibTrajSegEval(&seg, h, &o);
        checkf(fabsf(o.position - p1) < eps, "CubicHermite 终点位置", o.position - p1, eps);
        checkf(fabsf(o.velocity - v1) < eps, "CubicHermite 终点速度", o.velocity - v1, eps);
    }

    /* QuarticHermite：钉起点 p/v/a 与终点 p/v；终点 a 是自由量 */
    {
        float p0 = -1.0f, v0 = 2.0f, a0 = -5.0f, p1 = 3.0f, v1 = 0.25f, h = 1.3f;
        LibTraj_Seg_s seg;
        LibTrajSegMakeQuarticHermite(&seg, p0, v0, a0, p1, v1, h);
        LibTrajSegEval(&seg, 0.0f, &o);
        checkf(fabsf(o.position - p0) < eps, "QuarticHermite 起点位置", o.position - p0, eps);
        checkf(fabsf(o.velocity - v0) < eps, "QuarticHermite 起点速度", o.velocity - v0, eps);
        checkf(fabsf(o.acceleration - a0) < eps, "QuarticHermite 起点加速度", o.acceleration - a0, eps);
        LibTrajSegEval(&seg, h, &o);
        checkf(fabsf(o.position - p1) < eps, "QuarticHermite 终点位置", o.position - p1, eps);
        checkf(fabsf(o.velocity - v1) < eps, "QuarticHermite 终点速度", o.velocity - v1, eps);
    }

    /* MakeRaw：非法参数 */
    {
        LibTraj_Seg_s seg;
        float c[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        check(LibTrajSegMakeRaw(NULL, c, 3, 1.0f) == -1, "MakeRaw seg=NULL → -1");
        check(LibTrajSegMakeRaw(&seg, NULL, 3, 1.0f) == -1, "MakeRaw coeff=NULL → -1");
        check(LibTrajSegMakeRaw(&seg, c, LIB_TRAJ_DEG_MAX + 1, 1.0f) == -1, "MakeRaw degree 超限 → -1");
        check(LibTrajSegMakeRaw(&seg, c, 3, -1.0f) == -1, "MakeRaw 负时长 → -1");

        check(LibTrajSegMakeRaw(&seg, c, 2, 1.0f) == 0, "MakeRaw 合法 → 0");
        check(seg.c[3] == 0.0f && seg.c[4] == 0.0f, "MakeRaw 高阶系数被清零");
        check(seg.degree == 2 && seg.duration == 1.0f, "MakeRaw degree/duration 正确");
    }
    printf("    完成\n");
}

/*============================ 2. 采样等价 ============================*/

static void test_eval_step(void)
{
    printf("[2] 采样等价 / 钳位 / finished 语义\n");

    /* 先规划一条有内容的轨迹，再逐点比对 eval 与 step */
    LibTraj_Traj_s traj;
    LibTrajJerkPlan_Request_s req = {.p0 = 0.0f, .v0 = 0.0f, .a0 = 0.0f, .p1 = 12.0f, .v1 = 0.0f, .a1 = 0.0f, .vmax = 20.0f, .amax = 120.0f, .jmax = 1500.0f};
    check(LibTrajJerkPlan(&traj, &req) == LIB_TRAJ_JERK_PLAN_OK, "eval/step 前置规划成功");
    check(!LibTrajIsEmpty(&traj), "规划结果非空");

    const int nstep = 2000;
    const float dt = traj.total / (float)nstep;
    LibTraj_Traj_s copy = traj;
    float worst = 0.0f;
    int worst_k = -1;
    float worst_dp = 0.0f, worst_dv = 0.0f, worst_da = 0.0f;
    LibTraj_Output_s worst_os = {0};
    int fin_mismatch = 0;

    /* 逐点比对必须让两条路径**吃同一个时刻**：Step 的 t_now 是 dt 的累加和，
       dt*k 是乘法，二者差 ~1e-5 s（尾段 jerk 会把它放大成 0.02 的加速度差）。
       那是时间基的浮点差，不是段定位/求值的分歧 —— 故参照点取 copy.t_now。 */
    for (int k = 0; k <= nstep; k++)
    {
        LibTraj_Output_s oe, os_;
        LibTrajStep(&copy, (k == 0) ? 0.0f : dt, &os_);
        LibTrajEval(&traj, copy.t_now, &oe);

        float d = fmax3(fabsf(oe.position - os_.position), fabsf(oe.velocity - os_.velocity), fabsf(oe.acceleration - os_.acceleration));
        if (d > worst)
        {
            worst = d;
            worst_k = k;
            worst_dp = oe.position - os_.position;
            worst_dv = oe.velocity - os_.velocity;
            worst_da = oe.acceleration - os_.acceleration;
            worst_os = os_;
        }
        if (oe.finished != os_.finished)
        {
            fin_mismatch++;
            if (fin_mismatch <= 3)
            {
                printf("    [info] t=%.6g 处 finished 不一致（eval=%d step=%d）\n", copy.t_now, oe.finished, os_.finished);
            }
        }
    }
    checkf(worst < 1e-5f, "Eval 与 Step 在同一时刻逐点一致", worst, 1e-5f);
    printf("    Eval/Step 最大偏差 = %.3g（k=%d, t=%.6g, total=%.6g, dt=%.6g）\n", worst, worst_k, copy.t_now, traj.total, dt);
    printf("      Δp=%.3g Δv=%.3g Δa=%.3g  step{t_now=%.6g, finished=%d, p=%.6g, v=%.6g, a=%.6g}\n", worst_dp, worst_dv, worst_da, copy.t_now,
           worst_os.finished, worst_os.position, worst_os.velocity, worst_os.acceleration);
    check(fin_mismatch == 0, "finished 在两路径上一致");

    /* Step 的时间基只是 dt 的累加：允许的漂移是浮点噪声量级，不能是系统性的 */
    checkf(fabsf(copy.t_now - traj.total) < 1e-4f * traj.total + 1e-5f, "Step 的 t_now 与 total 的累计漂移在浮点噪声量级", fabsf(copy.t_now - traj.total),
           1e-4f * traj.total + 1e-5f);

    /* 钳位 */
    {
        LibTraj_Output_s a, b;
        LibTrajEval(&traj, -5.0f, &a);
        LibTrajEval(&traj, 0.0f, &b);
        checkf(fabsf(a.position - b.position) < 1e-6f, "t<0 按 0 取", a.position - b.position, 1e-6f);
        check(a.finished == 0, "t<0 且未含未到点 → finished=0");

        LibTrajEval(&traj, 1e6f, &a);
        LibTrajEval(&traj, traj.total, &b);
        checkf(fabsf(a.position - b.position) < 1e-6f, "t>total 按 total 取", a.position - b.position, 1e-6f);
        check(a.finished == 1, "t>total → finished=1");
        checkf(fabsf(b.position - 12.0f) < 1e-3f, "末态位置 = 目标", b.position - 12.0f, 1e-3f);
    }

    /* 再走几拍必须收敛到末态并置 finished（d_t 不是 total 的整数分时尾拍会差一点点） */
    {
        LibTraj_Output_s o = {0};
        for (int i = 0; i < 3; i++)
        {
            LibTrajStep(&copy, dt, &o);
        }
        check(o.finished == 1, "多走几拍后 Step finished=1");
        checkf(fabsf(o.position - 12.0f) < 1e-3f, "多走几拍后 Step 末态位置 = 目标", o.position - 12.0f, 1e-3f);
    }

    /* 空轨迹（手动 Reset）：全 0 + finished=1 */
    {
        LibTraj_Traj_s empty;
        LibTrajReset(&empty);
        check(LibTrajIsEmpty(&empty) == 1, "Reset 后为空");

        LibTraj_Output_s o;
        LibTrajEval(&empty, 0.0f, &o);
        check(o.position == 0.0f && o.velocity == 0.0f && o.acceleration == 0.0f && o.jerk == 0.0f, "空轨迹 Eval 输出全 0");
        check(o.finished == 1, "空轨迹 Eval finished=1");

        LibTrajStep(&empty, 0.01f, &o);
        check(o.finished == 1, "空轨迹 Step finished=1");

        /* 空指针 */
        LibTrajEval(NULL, 0.0f, &o);
        check(o.finished == 1, "traj=NULL → finished=1");
        LibTrajStep(NULL, 0.01f, &o);
        check(o.finished == 1, "traj=NULL Step → finished=1");
        LibTrajReset(NULL);
        check(LibTrajIsEmpty(NULL) == 1, "NULL 视为空");
        check(LibTrajAppendJerk(NULL, 1.0f, 1.0f) == -1, "AppendJerk(NULL) → -1");
        check(LibTrajAppendSeg(NULL, NULL) == -1, "AppendSeg(NULL,NULL) → -1");
    }

    /* AppendJerk 的连续性契约：初值取末端状态 */
    {
        LibTraj_Traj_s t2;
        LibTrajReset(&t2);
        LibTraj_Seg_s seg;
        LibTrajSegMakeConstJerk(&seg, 0.0f, 0.0f, 0.0f, 100.0f, 0.5f);
        check(LibTrajAppendSeg(&t2, &seg) == 0, "AppendSeg 首段成功");
        check(LibTrajAppendJerk(&t2, -100.0f, 0.5f) == 0, "AppendJerk 次段成功");
        checkf(max_discontinuity(&t2) < 1e-6f, "AppendJerk 接出的段 C² 连续", max_discontinuity(&t2), 1e-6f);

        /* 空段被跳过（不算失败） */
        uint16_t n = t2.seg_num;
        check(LibTrajAppendJerk(&t2, 1.0f, 0.0f) == 0 && t2.seg_num == n, "零时长段被跳过");
        check(LibTrajAppendJerk(&t2, 1.0f, -1.0f) == 0 && t2.seg_num == n, "负时长段被跳过");
    }
    printf("    完成\n");
}

/*============================ 3. 随机压力 ============================*/

typedef struct
{
    int ok;
    int infeasible;
    int infeasible_ok;      /* 被拒且明显越限（必要条件越限 > 0.01%） */
    int infeasible_border;  /* 被拒但只在精度量级的 slack 下才越限（临界，仍然无解） */
    int infeasible_suspect; /* 被拒且必要条件完全看不出越限 → 需要人工复核 */
    int limit;
    int param;
    int internal;
    int unexpected; /* OK 但校验不过 */
} PlanStat;

/**
 * @brief 无解的**必要条件**是否成立（给被拒样本分类用，不是充分条件）
 * @note 关键引理：a 从 0 变到 a₁ 的过程中（|j| ≤ J），∫a·dt ≥ a₁²/(2J)
 *       （最快也只能用 j = sign(a₁)·J 恒值）。据此四个速度界都是**无条件**成立的：
 *         a0 > 0 → 存在 t 使 v ≥ v0 + a0²/(2J)（故 v_max 至少这么高）
 *         a0 < 0 → 存在 t 使 v ≤ v0 − a0²/(2J)
 *         a1 > 0 → 存在 t 使 v ≤ v1 − a1²/(2J)
 *         a1 < 0 → 存在 t 使 v ≥ v1 + a1²/(2J)
 *       证：若全程不换号（例如 a ≥ 0），则 v 单调，而 ∫a ≥ (a0²+a1²)/(2J)，
 *       端点处就兑现了该界；若换号，则**穿零处**兑现该界。两种情形都绕不开。
 *       注意界与"位于 a0 或 a1 处"无关 —— 这就是能抓住"末段必须把 a 从 0 拉到 a1"
 *       这一类无解的原因（避免 v 掉到 −(a1²/2J) 之下是做不到的）。
 *       不满足这些界的被拒样本"没有被证明无解" → 单独统计，人工复核。
 */
static int infeasible_expected(float v0, float a0, float v1, float a1, float V, float J, float rel_slack)
{
    const float slack = 1.0f + rel_slack;
    const float k = 2.0f * J;

    if (a0 > 0.0f && v0 + a0 * a0 / k > V * slack)
    {
        return 1;
    }
    if (a0 < 0.0f && v0 - a0 * a0 / k < -V * slack)
    {
        return 1;
    }
    if (a1 > 0.0f && v1 - a1 * a1 / k < -V * slack)
    {
        return 1;
    }
    if (a1 < 0.0f && v1 + a1 * a1 / k > V * slack)
    {
        return 1;
    }
    return 0;
}

/** @brief 随机生成一组请求；mode 控制分布 */
static void gen_request(LibTrajJerkPlan_Request_s *req, int mode)
{
    float V = 1.0f + 99.0f * (float)urand();
    float A = V * (0.5f + 19.5f * (float)urand());
    float J = A * (0.5f + 49.5f * (float)urand());
    float dscale = V * V / A; /* 特征位移：加速到 V 再刹回来的量级 */

    req->vmax = V;
    req->amax = A;
    req->jmax = J;

    /* 边界：多数取 0（常规），少数取大值（病态），保证覆盖面 */
    float v0 = 0.0f, v1 = 0.0f, a0 = 0.0f, a1 = 0.0f;
    switch (mode)
    {
    case 0: /* 静止 ↔ 静止 */
        break;
    case 1: /* 同向非零初速 */
        v0 = 0.9f * V * (float)urand();
        v1 = 0.9f * V * (float)urand();
        break;
    case 2: /* 反向初速 */
        v0 = srand1() * 0.9f * V;
        v1 = -srand1() * 0.9f * V;
        break;
    case 3: /* 带初加速度 */
        a0 = srand1() * 0.95f * A;
        a1 = srand1() * 0.95f * A;
        v0 = 0.5f * V * srand1();
        v1 = 0.5f * V * srand1();
        break;
    default: /* 全随机 */
        v0 = srand1() * V;
        v1 = srand1() * V;
        a0 = srand1() * A;
        a1 = srand1() * A;
        break;
    }

    float dp;
    switch (mode)
    {
    case 5: /* 极短位移（最可能触发无解/兜底的区域） */
        dp = dscale * (float)(1e-5 + 0.2 * urand()) * srand1();
        break;
    case 6: /* 长位移（需要巡航） */
        dp = dscale * (float)(1.0 + 63.0 * urand()) * srand1();
        break;
    case 7: /* 纯消加速度：位置速度不变，只把 a 拉回 */
        dp = 0.0f;
        v1 = v0;
        break;
    default:
        dp = dscale * (float)(4.0 * urand()) * srand1();
        break;
    }

    req->p0 = 0.0f;
    req->p1 = dp;
    req->v0 = v0;
    req->v1 = v1;
    req->a0 = a0;
    req->a1 = a1;
}

/** @brief 校验一条已规划轨迹：终态精度 + 限幅 + 段间连续 */
static int verify_planned(const LibTraj_Traj_s *traj, const LibTrajJerkPlan_Request_s *req, float *p_err, float *v_err, float *a_err, float *disc)
{
    LibTraj_Output_s s, e;
    LibTrajStartState(traj, &s);
    LibTrajEndState(traj, &e);

    float tp = 1e-4f * (fabsf(req->p1 - req->p0) + fabsf(req->v0) + fabsf(req->v1)) + 1e-3f;
    float tv = 1e-4f * (fabsf(req->v0) + fabsf(req->v1) + req->vmax) + 1e-3f;
    float ta = 1e-3f * (fabsf(req->a0) + fabsf(req->a1) + req->amax) + 1e-2f;

    *p_err = fabsf(e.position - req->p1);
    *v_err = fabsf(e.velocity - req->v1);
    *a_err = fabsf(e.acceleration - req->a1);
    *disc = max_discontinuity(traj);

    int bad = 0;
    if (*p_err > tp || *v_err > tv || *a_err > ta)
    {
        bad = 1;
    }
    if (*disc > 1e-3f)
    {
        bad = 1;
    }
    /* 起点也要对上 */
    if (fabsf(s.position - req->p0) > tp || fabsf(s.velocity - req->v0) > tv || fabsf(s.acceleration - req->a0) > ta)
    {
        bad = 1;
    }

    LimitStat st;
    stat_walk(traj, req->vmax, req->amax, req->jmax, 16, &st);
    if (st.v_over > req->vmax * 1e-3f + 1e-3f || st.a_over > req->amax * 1e-3f + 1e-3f || st.j_over > req->jmax * 1e-3f + 1e-3f)
    {
        bad = 1;
    }
    if (traj->seg_num == 0 || traj->seg_num > LIB_TRAJ_SEG_MAX)
    {
        bad = 1;
    }
    return !bad;
}

static void test_planner_stress(int trials, PlanStat *stat)
{
    printf("[3] 规划器随机压力（%d 组）\n", trials);

    float worst_p = 0.0f, worst_v = 0.0f, worst_a = 0.0f, worst_disc = 0.0f;
    int max_seg = 0;
    int shown = 0;

    for (int i = 0; i < trials; i++)
    {
        LibTrajJerkPlan_Request_s req;
        gen_request(&req, i % 8);

        LibTraj_Traj_s traj;
        LibTrajReset(&traj);
        LibTrajJerkPlan_Status_e r = LibTrajJerkPlan(&traj, &req);

        switch (r)
        {
        case LIB_TRAJ_JERK_PLAN_OK:
            stat->ok++;
            break;
        case LIB_TRAJ_JERK_PLAN_ERR_INFEASIBLE:
            stat->infeasible++;
            if (infeasible_expected(req.v0, req.a0, req.v1, req.a1, req.vmax, req.jmax, 1e-4f))
            {
                stat->infeasible_ok++; /* 明显越限（> 0.01%） */
            }
            else if (infeasible_expected(req.v0, req.a0, req.v1, req.a1, req.vmax, req.jmax, 1e-6f))
            {
                stat->infeasible_border++; /* 只在精度量级的 slack 下才越限 → 临界（仍然无解） */
            }
            else
            {
                stat->infeasible_suspect++;
                if (stat->infeasible_suspect <= 8)
                {
                    printf("    [?!] 被拒但必要条件不成立（需复核）：p0=%.4g p1=%.4g v0=%.4g v1=%.4g a0=%.4g a1=%.4g V=%.4g A=%.4g J=%.4g\n", req.p0, req.p1,
                           req.v0, req.v1, req.a0, req.a1, req.vmax, req.amax, req.jmax);
                }
            }
            if (s_verbose && shown < 20)
            {
                printf("    [INFEASIBLE] p0=%.4g p1=%.4g v0=%.4g v1=%.4g a0=%.4g a1=%.4g V=%.4g A=%.4g J=%.4g\n", req.p0, req.p1, req.v0, req.v1, req.a0,
                       req.a1, req.vmax, req.amax, req.jmax);
                shown++;
            }
            continue;
        case LIB_TRAJ_JERK_PLAN_ERR_LIMIT:
            stat->limit++;
            continue;
        case LIB_TRAJ_JERK_PLAN_ERR_PARAM:
            stat->param++;
            continue;
        default:
            stat->internal++;
            continue;
        }

        float pe, ve, ae, dc;
        if (!verify_planned(&traj, &req, &pe, &ve, &ae, &dc))
        {
            stat->unexpected++;
            if (stat->unexpected <= 5)
            {
                printf("    [FAIL] 规划结果校验不过：p_err=%.4g v_err=%.4g a_err=%.4g disc=%.4g | p1=%.5g v1=%.5g a1=%.5g V=%.5g A=%.5g J=%.5g\n", pe, ve, ae,
                       dc, req.p1, req.v1, req.a1, req.vmax, req.amax, req.jmax);
            }
            s_fail++;
        }
        if (pe > worst_p)
        {
            worst_p = pe;
        }
        if (ve > worst_v)
        {
            worst_v = ve;
        }
        if (ae > worst_a)
        {
            worst_a = ae;
        }
        if (dc > worst_disc)
        {
            worst_disc = dc;
        }
        if ((int)traj.seg_num > max_seg)
        {
            max_seg = traj.seg_num;
        }

        /* 同一个请求连算两次必须逐位一致（确定性） */
        if ((i % 37) == 0)
        {
            LibTraj_Traj_s again;
            if (LibTrajJerkPlan(&again, &req) != LIB_TRAJ_JERK_PLAN_OK || memcmp(&traj, &again, sizeof(traj)) != 0)
            {
                s_fail++;
                printf("    [FAIL] 同一请求两次规划结果不一致（trial %d）\n", i);
            }
        }
    }

    printf("    OK=%d  INFEASIBLE=%d(%.3f%%)[明显无解 %d / 临界 %d / 待复核 %d]  LIMIT=%d  PARAM=%d  INTERNAL=%d  校验不过=%d\n", stat->ok, stat->infeasible,
           100.0 * (double)stat->infeasible / (double)(trials > 0 ? trials : 1), stat->infeasible_ok, stat->infeasible_border, stat->infeasible_suspect,
           stat->limit, stat->param, stat->internal, stat->unexpected);
    printf("    终态最大误差：位置 %.3g / 速度 %.3g / 加速度 %.3g；最大段间跳变 %.3g；最大段数 %d\n", worst_p, worst_v, worst_a, worst_disc, max_seg);

    check(stat->unexpected == 0, "随机压力：所有 OK 的规划都通过校验");
    check(stat->internal == 0, "随机压力：不出现 ERR_INTERNAL");
    check(max_seg <= LIB_TRAJ_SEG_MAX, "段数不超过容量");
    /* 没有任何被拒样本是"定理看不出越限"的 —— 即规划器的拒绝都是必要的，没有漏解 */
    check(stat->infeasible_suspect == 0, "随机压力：被拒样本全部能被端点引理解释（无误拒）");
}

/** @brief "必然可行"族：v0==v1 且 a0==a1==0 时，任何 dp ≥ 0 都该有解 */
static void test_must_be_feasible(void)
{
    printf("[3b] 必然可行族（v0==v1, a0==a1==0）反例搜索\n");
    int rejected = 0;
    for (int i = 0; i < 2000; i++)
    {
        LibTrajJerkPlan_Request_s req;
        req.vmax = 1.0f + 99.0f * (float)urand();
        req.amax = req.vmax * (0.5f + 19.5f * (float)urand());
        req.jmax = req.amax * (0.5f + 49.5f * (float)urand());
        req.v0 = req.vmax * 0.9f * (float)urand() * ((i % 3 == 0) ? -1.0f : 1.0f);
        req.v1 = req.v0;
        req.a0 = 0.0f;
        req.a1 = 0.0f;
        req.p0 = 0.0f;
        req.p1 = (req.vmax * req.vmax / req.amax) * (float)(1e-4 + 8.0 * urand());

        LibTraj_Traj_s traj;
        if (LibTrajJerkPlan(&traj, &req) != LIB_TRAJ_JERK_PLAN_OK)
        {
            if (rejected < 5)
            {
                printf("    [!] 被拒：dp=%.6g v0=v1=%.6g V=%.6g A=%.6g J=%.6g\n", req.p1, req.v0, req.vmax, req.amax, req.jmax);
            }
            rejected++;
        }
        else
        {
            float pe, ve, ae, dc;
            if (!verify_planned(&traj, &req, &pe, &ve, &ae, &dc))
            {
                s_fail++;
                printf("    [FAIL] 必然可行族的规划结果校验不过\n");
            }
        }
    }
    printf("    被拒 %d / 2000\n", rejected);
    check(rejected == 0, "必然可行族不应被拒");
}

/*============================ 4. 错误码与不变性 ============================*/

static void test_errors(void)
{
    printf("[4] 错误码 / traj 不变性\n");

    /* 先做一条有效轨迹作为"上一版" */
    LibTrajJerkPlan_Request_s good = {.p0 = 0.0f, .v0 = 0.0f, .a0 = 0.0f, .p1 = 5.0f, .v1 = 0.0f, .a1 = 0.0f, .vmax = 10.0f, .amax = 50.0f, .jmax = 500.0f};
    LibTraj_Traj_s traj;
    check(LibTrajJerkPlan(&traj, &good) == LIB_TRAJ_JERK_PLAN_OK, "基准规划成功");
    LibTraj_Traj_s snapshot = traj;

    /* 参数错误 */
    check(LibTrajJerkPlan(NULL, &good) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "traj=NULL → ERR_PARAM");
    check(LibTrajJerkPlan(&traj, NULL) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "req=NULL → ERR_PARAM");

    LibTrajJerkPlan_Request_s bad = good;
    bad.vmax = 0.0f;
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "vmax=0 → ERR_PARAM");
    bad = good;
    bad.amax = -1.0f;
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "amax<0 → ERR_PARAM");
    bad = good;
    bad.jmax = 0.0f;
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "jmax=0 → ERR_PARAM");

    bad = good;
    bad.v0 = NAN;
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_PARAM, "v0=NaN → ERR_PARAM");

    /* 起止越限 */
    bad = good;
    bad.v0 = 20.0f; /* > vmax */
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_LIMIT, "v0>vmax → ERR_LIMIT");
    bad = good;
    bad.a1 = 1000.0f; /* > amax */
    check(LibTrajJerkPlan(&traj, &bad) == LIB_TRAJ_JERK_PLAN_ERR_LIMIT, "a1>amax → ERR_LIMIT");

    /* 上面所有失败调用之后，traj 必须逐字节不变 */
    check(memcmp(&traj, &snapshot, sizeof(traj)) == 0, "失败后 traj 逐字节未变");

    /* 真·无解：起点同时顶住 v 上限与 a 上限且同号 —— 加速度为正意味着速度只会先
       涨上去，把 a 拉回 0 最少也要涨 a0²/(2J) = 2.5 > 0，必然越过 vmax（任何走势都
       绕不开，故是真的无解，不是搜索漏解） */
    {
        LibTrajJerkPlan_Request_s inf = {
            .p0 = 0.0f, .v0 = 20.0f, .a0 = 50.0f, .p1 = 5.0f, .v1 = 0.0f, .a1 = 0.0f, .vmax = 20.0f, .amax = 50.0f, .jmax = 1000.0f};
        check(LibTrajJerkPlan(&traj, &inf) == LIB_TRAJ_JERK_PLAN_ERR_INFEASIBLE, "v0=vmax + a0=amax 同号 → ERR_INFEASIBLE");
        check(memcmp(&traj, &snapshot, sizeof(traj)) == 0, "ERR_INFEASIBLE 后 traj 逐字节未变");
    }

    /* 反例：起点同样顶格，但 a0 与 v0 **反号** —— 加速度只会把 |v| 往回拉，扫不动限幅，
       必须有解（这才是与上一条的对照：决定可行性的是 a0 把 v 推出去还是拉回来） */
    {
        LibTrajJerkPlan_Request_s ok2 = {
            .p0 = 0.0f, .v0 = -20.0f, .a0 = 50.0f, .p1 = 5.0f, .v1 = 0.0f, .a1 = 0.0f, .vmax = 20.0f, .amax = 50.0f, .jmax = 1000.0f};
        check(LibTrajJerkPlan(&traj, &ok2) == LIB_TRAJ_JERK_PLAN_OK, "v0=−vmax + a0=+amax（反号）→ 必须有解");
        float pe, ve, ae, dc;
        check(verify_planned(&traj, &ok2, &pe, &ve, &ae, &dc), "反号顶格解出的轨迹自校验通过");
    }
    printf("    完成\n");
}

/*============================ 5. 覆盖用例 ============================*/

static void case_plan(const char *name, LibTrajJerkPlan_Request_s req, LibTrajJerkPlan_Status_e expect)
{
    LibTraj_Traj_s traj;
    LibTrajReset(&traj);
    LibTrajJerkPlan_Status_e r = LibTrajJerkPlan(&traj, &req);
    if (r != expect)
    {
        printf("    [FAIL] %s：期望 %d，实际 %d\n", name, (int)expect, (int)r);
        s_fail++;
        return;
    }
    if (r != LIB_TRAJ_JERK_PLAN_OK)
    {
        printf("    %-34s → 码 %d（预期）\n", name, (int)r);
        return;
    }

    float pe, ve, ae, dc;
    int ok = verify_planned(&traj, &req, &pe, &ve, &ae, &dc);
    printf("    %-34s → OK 段数=%2u 时长=%.4g s  p_err=%.2g v_err=%.2g a_err=%.2g\n", name, traj.seg_num, traj.total, pe, ve, ae);
    if (!ok)
    {
        printf("    [FAIL] %s 的轨迹校验不过\n", name);
        s_fail++;
    }
}

static void test_coverage(void)
{
    printf("[5] 覆盖用例\n");

    const float V = 20.0f, A = 100.0f, J = 1000.0f;

    /* 静止 ↔ 静止：标准 7 段 S 曲线 */
    case_plan("静止↔静止（标准 S 曲线）", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 短距离：退化到更少段 */
    case_plan("极短位移（退化）", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, 0.0f, 0.01f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 长距离：需要匀速段 */
    case_plan("长距离（含巡航）", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, 0.0f, 500.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 同向非零初速 */
    case_plan("同向非零初速", (LibTrajJerkPlan_Request_s){0.0f, 8.0f, 0.0f, 30.0f, 5.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 反向初速 */
    case_plan("反向初速", (LibTrajJerkPlan_Request_s){0.0f, -9.0f, 0.0f, 15.0f, 6.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 带初加速度 */
    case_plan("带初加速度 a0/a1", (LibTrajJerkPlan_Request_s){0.0f, 3.0f, -40.0f, 25.0f, -2.0f, 30.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 纯消加速度：位置不变、速度不变，只把 a 拉回 0 —— 必须靠"先停再走"兜底 */
    case_plan("纯消加速度（dp=0, v0=v1）", (LibTrajJerkPlan_Request_s){0.0f, 5.0f, 50.0f, 0.0f, 5.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    case_plan("纯消加速度（dp=0, v=0）", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, -80.0f, 0.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* jmax 极大：退化成梯形/三角（加速度阶跃） */
    case_plan("jmax 极大（准梯形）", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, V, A, 1e7f}, LIB_TRAJ_JERK_PLAN_OK);
    /* jmax 极小：时间被拖得很长 */
    case_plan("jmax 极小", (LibTrajJerkPlan_Request_s){0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, V, A, 5.0f}, LIB_TRAJ_JERK_PLAN_OK);
    /* 负位移（镜像路径） */
    case_plan("负位移（镜像）", (LibTrajJerkPlan_Request_s){3.0f, 0.0f, 0.0f, -7.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 起点位置非零 */
    case_plan("起点位置非零", (LibTrajJerkPlan_Request_s){123.456f, 0.0f, 0.0f, 130.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 原地不动 */
    case_plan("原地不动（六边界全等）", (LibTrajJerkPlan_Request_s){1.5f, 0.5f, 2.0f, 1.5f, 0.5f, 2.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 起点顶格速度（边界可行性的极限） */
    case_plan("v0 顶格 + 减速", (LibTrajJerkPlan_Request_s){0.0f, V, 0.0f, 30.0f, 0.0f, 0.0f, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
    /* 原地不动但边界全顶格 */
    case_plan("顶格边界原地不动", (LibTrajJerkPlan_Request_s){0.0f, V, A, 0.0f, V, A, V, A, J}, LIB_TRAJ_JERK_PLAN_OK);
}

/*============================ main ============================*/

int main(int argc, char **argv)
{
    unsigned int seed = 20261011u;
    int trials = 200000;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--trials") == 0 && i + 1 < argc)
        {
            trials = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
        {
            seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--verbose") == 0)
        {
            s_verbose = 1;
        }
        else
        {
            printf("用法：%s [--trials N] [--seed S] [--verbose]\n", argv[0]);
            return 2;
        }
    }

    printf("lib_traj PC 测试（seed=%u, trials=%d）\n", seed, trials);
    printf("LIB_TRAJ_DEG_MAX=%d  LIB_TRAJ_SEG_MAX=%d  sizeof(LibTraj_Traj_s)=%zu\n\n", LIB_TRAJ_DEG_MAX, LIB_TRAJ_SEG_MAX, sizeof(LibTraj_Traj_s));

    rnd_seed(seed);

    test_constructors();
    test_eval_step();

    PlanStat stat = {0};
    test_planner_stress(trials, &stat);
    test_must_be_feasible();
    test_errors();
    test_coverage();

    printf("\n%s（失败项 %d）\n", (s_fail == 0) ? "==> PASS" : "==> FAIL", s_fail);
    return (s_fail == 0) ? 0 : 1;
}
