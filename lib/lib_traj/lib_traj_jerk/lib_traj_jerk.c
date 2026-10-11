/**
 * @file lib_traj_jerk.c
 * @brief 单轴点到点轨迹规划器实现：完整任意起止 + jerk 受限
 *
 * ============================ 数学准备 ============================
 *
 * 一段常值 jerk 的相位的**前向积分原语**（位置 3 次）：
 *     a += j·t
 *     v += a·t + j·t²/2
 *     p += v·t + a·t²/2 + j·t³/6
 * 规划器全程只用这一个原语，所以"段间 p/v/a 连续"是构造性保证的。
 *
 * -------------------- 端点剖面（闭式解）--------------------
 *
 * 定义 JerkProfileSolve(v_s, a_s, v_e)：把状态 (v_s,a_s) 送到 (v_e, 0)，
 * |a| ≤ A、|j| ≤ J，形状是**三相位的加速度三角/梯形**：
 *     相位1: jerk = u·J，时长 t1     （a: a_s → a_pk = u·J·t3）
 *     相位2: jerk = 0 ，时长 t2      （a 恒为 a_pk）
 *     相位3: jerk = -u·J，时长 t3    （a: a_pk → 0）
 * 其中 u = ±1 由"需要正/负的净速度增量"决定。推一下可得：
 *     t1 = t3 − u·a_s/J ≥ 0
 *     Δv = u·J·t3·(t3 + t2) − u·a_s²/(2J)
 * 于是把 K ≜ (u·Δv + a_s²/(2J)) / J 记为 t3·(t3+t2)，就有闭式：
 *     t3 = clamp(√K, t3_min, A/J),  t3_min = max(0, u·a_s/J)
 *     t2 = K/t3 − t3  (≥ 0),  t1 = t3 − u·a_s/J
 * 关键：**u 的选择由 Δv 与阈值 thr = sign(a_s)·a_s²/(2J) 比较得出**
 *     Δv ≥ thr → u = +1；否则 u = −1
 * 这保证了 K ≥ 0 且 t3_min ≤ √K（即 t2 不会被钳负），故上式永远自洽。
 * 物理含义：a_s 非零时，光把加速度拉回 0 就已经强制改变了一段速度
 *（最小 Δv 就是 ±a_s²/(2J)），这就是 thr 的来历，也是"位移太短"时
 * 真的无解的根源（此时返回 ERR_INFEASIBLE 是**正确**的，不是漏解）。
 *
 * B 侧剖面（从静止的 a=0 顶点走到终点 (v1,a1)）不需要新的推导：
 * 时间反演。对解出来的 (v1, -a1) → (v_c, 0) 剖面做反演，得到的正是
 * (v_c, 0) → (v1, a1)，且**位移相等**。反演规则：jerk 顺序倒过来、
 * 时长顺序倒过来（见 Jerk_BuildJunctionPhases）。注意起点加速度用的是
 * **-a1**：反演会翻转加速度，所以喂进去的必须是取负后的终态加速度。
 *
 * ============================ 候选枚举 ============================
 *
 * 自由标量取"a = 0 顶点处的速度 v_c"，射击出总位移等于 dp 的那一个。
 * 只枚举**两个**候选，取"可行且更短"者：
 *
 *   候选 1（junction 型，≤7 段，常规场景就是标准 7 段 S 曲线）：
 *       [A 剖面 3 段] + [以 v_c 匀速巡航 t_c 秒] + [B 剖面 3 段]
 *       位移 D(v_c) = dp_A(v_c) + v_c·t_c + dp_B(v_c)，多出来的部分用巡航补。
 *
 *   候选 2（"先停再走"，≤13 段，安全网）：
 *       [A 剖面：把 (v0,a0) 刹到静止] + [静止 ↔ 静止走完剩余位移（可正可负）]
 *       + [B 剖面：从静止加速到 (v1,a1)]
 *       过静止点不最优，但覆盖大量候选 1 解不出的病态起止（例如"位置不变
 *       但速度要变"，必须往前走一段再倒回来）。
 *
 * v_c 的搜索**不假设 D(v_c) 单调** —— 实测它不是（比如 v0=v1≠0、位移极短时
 * D 的最小值出现在 v_c≈v0，两侧都上升）。搜索分三层，全程确定性、无 while 死循环：
 *   1) 粗扫 65 点：安全网（非单峰时唯一能兜住的），也给出局部再分的落点；
 *   2) 自适应锚点：三元搜索 D 的极小点 → 在 [v_min, V] 上二分打到 D = dp 的边界。
 *      **这一层是必需的**：可行区间可能窄于网格步长（静止起步的极小位移、
 *      v0=v1≠0 且位移极短），只靠粗扫会整段漏掉，导致误判 INFEASIBLE；
 *   3) 以最优点为中心再分 2 轮 ×16：把 v_c 收敛到网格分辨率以下。
 * 任一候选都不行才返回 ERR_INFEASIBLE。
 *
 * ============================ 落段与自校验 ============================
 *
 * 相位表（≤13 组 (jerk, 时长)，约 104 B 栈）先在局部积一遍，用**独立于落段**的
 * 积分路径检查终态精度与全程限幅；通过后才 LibTrajReset + 逐段 AppendJerk 落进
 * traj。落段后再用 LibTrajEval（走 Horner，又是第三条代码路径）复算终态。
 * 任何一步不过 → 返回错误码且 traj 不被写坏（ERR_INTERNAL 例外，会清空 traj）。
 */

#include "lib_traj_jerk.h"

#ifndef LIB_TRAJ_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_TRAJ_USED

#include <math.h>
#include <stddef.h> /* NULL */

/*============================ 常量 ============================*/

#define JERK_PHASE_MAX 13 /* A(3) + 静止↔静止(≤7) + B(3) */

#define JERK_GRID_NUM 65     /* 粗扫网格点数（含两端） */
#define JERK_REFINE_ROUNDS 2 /* 局部细分轮数 */
#define JERK_REFINE_DIV 16   /* 每轮把区间分多少份 */
#define JERK_TERNARY_ITER 32 /* 三元搜索迭代数：(2/3)^32 ≈ 2.6e-6，足够把 D 的极小点钉到浮点精度 */
#define JERK_BISECT_ITER 32  /* 边界二分迭代数（同上量级） */

#define JERK_POS_V_MIN 1e-4f /* 正半轴搜索下界系数（下界 = 该值 × V，避开 v_c → 0 的 1/v_c 病态） */

#define JERK_REL_SLACK 1e-4f /* 限幅相对松量（浮点噪声） */
#define JERK_EPS_V 1e-6f     /* 判定 v_c == 0 */
#define JERK_TOL_TIME 1e-6f  /* 时长比较阈值 (s) */

/*============================ 类型 ============================*/

/** @brief 一个常值 jerk 相位 */
typedef struct
{
    float jerk;
    float duration;
} Jerk_Phase_s;

/** @brief 候选轨迹（相位表，落段前的中间表示） */
typedef struct
{
    Jerk_Phase_s ph[JERK_PHASE_MAX];
    uint16_t num;
} Jerk_Cand_s;

/** @brief 端点剖面（三相位的闭式解） */
typedef struct
{
    float j[3]; /* 相位 jerk：u·J, 0, -u·J */
    float t[3]; /* 相位时长 */
    float dp;   /* 该剖面的位移 */
    float time; /* 该剖面的总时长 */
} Jerk_Profile_s;

/** @brief junction 型候选：起点剖面 + 巡航 + 终点剖面 */
typedef struct
{
    Jerk_Profile_s pa; /* (v0,a0) → (v_c, 0) */
    Jerk_Profile_s pb; /* (v1,-a1) → (v_c, 0)，B 段由其时间反演得到 */
    float v_c;         /* 顶点/巡航速度 */
    float t_c;         /* 巡航时长 */
    float time;        /* 总时长 */
} Jerk_Junction_s;

/** @brief 极值累计（限幅检查用） */
typedef struct
{
    float v_min;
    float v_max;
    float a_max_abs;
    float j_max_abs;
} Jerk_Ext_s;

/*============================ 容差 ============================*/

/** @brief 有界检查（NaN/±∞ → 0） */
static uint8_t Jerk_Finite(float x)
{
    return (uint8_t)(isfinite(x) != 0);
}

/* 容差取"绝对值下限 + 相对尺度"：太紧会在长链积分上假失败，太松会掩盖真错 */
static float Jerk_TolP(float dp, float v0, float v1)
{
    float t = 1e-4f * (fabsf(dp) + fabsf(v0) + fabsf(v1));
    return (t > 1e-3f) ? t : 1e-3f;
}

static float Jerk_TolV(float v0, float v1, float V)
{
    float t = 1e-4f * (fabsf(v0) + fabsf(v1) + V);
    return (t > 1e-3f) ? t : 1e-3f;
}

static float Jerk_TolA(float a0, float a1, float A)
{
    float t = 1e-3f * (fabsf(a0) + fabsf(a1) + A);
    return (t > 1e-2f) ? t : 1e-2f;
}

/*============================ 积分原语 ============================*/

/** @brief 前向积分一个常值 jerk 相位（位置 3 次） */
static void Jerk_Integrate(float *p, float *v, float *a, float jerk, float t)
{
    *p += (*v) * t + 0.5f * (*a) * t * t + jerk * t * t * t / 6.0f;
    *v += (*a) * t + 0.5f * jerk * t * t;
    *a += jerk * t;
}

/** @brief 极值累计初始化 */
static void Jerk_ExtInit(Jerk_Ext_s *e, float v, float a)
{
    e->v_min = v;
    e->v_max = v;
    e->a_max_abs = fabsf(a);
    e->j_max_abs = 0.0f;
}

/**
 * @brief 记录一个相位内的 v/a/j 极值
 * @note a 在相位内线性 → 极值必在相位端点；v 为二次 → 极值在端点或 a=0 处。
 *       都按闭式取，保证限幅判据是**精确**的而不是靠采样碰运气。
 */
static void Jerk_ExtPhase(Jerk_Ext_s *e, float v, float a, float jerk, float t)
{
    float a_end = a + jerk * t;
    float v_end = v + a * t + 0.5f * jerk * t * t;

    if (v < e->v_min)
    {
        e->v_min = v;
    }
    if (v > e->v_max)
    {
        e->v_max = v;
    }
    if (v_end < e->v_min)
    {
        e->v_min = v_end;
    }
    if (v_end > e->v_max)
    {
        e->v_max = v_end;
    }
    if (fabsf(a) > e->a_max_abs)
    {
        e->a_max_abs = fabsf(a);
    }
    if (fabsf(a_end) > e->a_max_abs)
    {
        e->a_max_abs = fabsf(a_end);
    }
    if (fabsf(jerk) > e->j_max_abs)
    {
        e->j_max_abs = fabsf(jerk);
    }

    if (jerk != 0.0f) /* v 的极值处 a = 0 */
    {
        float ts = -a / jerk;
        if (ts > 0.0f && ts < t)
        {
            float vs = v + a * ts + 0.5f * jerk * ts * ts;
            if (vs < e->v_min)
            {
                e->v_min = vs;
            }
            if (vs > e->v_max)
            {
                e->v_max = vs;
            }
        }
    }
}

/*============================ 端点剖面 ============================*/

/**
 * @brief 闭式求解 (v_s, a_s) → (v_e, 0) 的三相位剖面（详见文件头数学准备）
 * @param v_s,a_s 起点 速度/加速度
 * @param v_e     目标速度（目标加速度固定为 0）
 * @param A,J     加速度上限、jerk 上限（均 > 0）
 * @param out     输出剖面
 * @return 1 成功；0 参数非法
 */
static uint8_t JerkProfileSolve(float v_s, float a_s, float v_e, float A, float J, Jerk_Profile_s *out)
{
    if (!(J > 0.0f) || !(A > 0.0f))
    {
        return 0;
    }

    const float dv = v_e - v_s;
    const float thr = a_s * fabsf(a_s) / (2.0f * J); /* sign(a_s)·a_s²/(2J) */
    const float u = (dv >= thr) ? 1.0f : -1.0f;

    float K = (u * dv + a_s * a_s / (2.0f * J)) / J; /* ≜ t3·(t3+t2)，恒 ≥ 0 */
    if (!(K >= 0.0f))
    {
        K = 0.0f;
    }

    const float t3_cap = A / J;
    const float t3_min = (u * a_s > 0.0f) ? (u * a_s / J) : 0.0f;

    float t3 = sqrtf(K);
    if (t3 > t3_cap)
    {
        t3 = t3_cap;
    }
    if (t3 < t3_min)
    {
        t3 = t3_min;
    }

    float t2 = 0.0f;
    if (t3 > 0.0f)
    {
        t2 = K / t3 - t3; /* 由 t3 ≤ √K 保证 ≥ 0 */
    }
    if (t2 < 0.0f)
    {
        t2 = 0.0f;
    }

    float t1 = t3 - u * a_s / J; /* 由 t3 ≥ t3_min 保证 ≥ 0 */
    if (t1 < 0.0f)
    {
        t1 = 0.0f;
    }

    out->j[0] = u * J;
    out->j[1] = 0.0f;
    out->j[2] = -u * J;
    out->t[0] = t1;
    out->t[1] = t2;
    out->t[2] = t3;

    float p = 0.0f, v = v_s, a = a_s;
    Jerk_Integrate(&p, &v, &a, out->j[0], out->t[0]);
    Jerk_Integrate(&p, &v, &a, out->j[1], out->t[1]);
    Jerk_Integrate(&p, &v, &a, out->j[2], out->t[2]);
    out->dp = p;
    out->time = t1 + t2 + t3;
    return 1;
}

/** @brief 把剖面的极值并进累计器（剖面内 p 是局部量，不参与限幅） */
static void Jerk_ProfileExtremes(const Jerk_Profile_s *pf, float v_s, float a_s, Jerk_Ext_s *e)
{
    float p = 0.0f, v = v_s, a = a_s;
    for (uint8_t k = 0; k < 3; k++)
    {
        Jerk_ExtPhase(e, v, a, pf->j[k], pf->t[k]);
        Jerk_Integrate(&p, &v, &a, pf->j[k], pf->t[k]);
    }
}

/*============================ junction 候选 ============================*/

/**
 * @brief 给定顶点速度 v_c，解出 junction 候选并检查时长合法性
 * @return 1 成功；0 v_c 这个点取不到（巡航时长为负 / v_c=0 但位移对不上）
 * @note 不做限幅检查（那是 JerkJunctionLimits 的事），只保证"存在且时长非负"。
 */
static uint8_t JerkJunctionSolve(float v0, float a0, float v1, float a1, float dp, float A, float J, float v_c, Jerk_Junction_s *out)
{
    if (!JerkProfileSolve(v0, a0, v_c, A, J, &out->pa))
    {
        return 0;
    }
    if (!JerkProfileSolve(v1, -a1, v_c, A, J, &out->pb))
    {
        return 0;
    }

    const float d = dp - out->pa.dp - out->pb.dp;

    if (fabsf(v_c) < JERK_EPS_V)
    {
        /* v_c = 0：不能靠巡航补位移，位移必须本来就对 */
        if (fabsf(d) > Jerk_TolP(dp, v0, v1))
        {
            return 0;
        }
        out->t_c = 0.0f;
    }
    else
    {
        float t_c = d / v_c;
        if (t_c < -JERK_TOL_TIME)
        {
            return 0; /* 巡航方向不对（要倒着走才能补上） */
        }
        out->t_c = (t_c > 0.0f) ? t_c : 0.0f;
    }

    out->v_c = v_c;
    out->time = out->pa.time + out->t_c + out->pb.time;
    return 1;
}

/** @brief junction 候选的全程限幅检查（精确极值，非采样） */
static uint8_t JerkJunctionLimits(const Jerk_Junction_s *jn, float v0, float a0, float v1, float a1, float V, float A, float J)
{
    Jerk_Ext_s e;
    Jerk_ExtInit(&e, 0.0f, 0.0f);
    Jerk_ProfileExtremes(&jn->pa, v0, a0, &e);
    Jerk_ProfileExtremes(&jn->pb, v1, -a1, &e);
    if (jn->v_c < e.v_min)
    {
        e.v_min = jn->v_c;
    }
    if (jn->v_c > e.v_max)
    {
        e.v_max = jn->v_c;
    }

    const float slack = 1.0f + JERK_REL_SLACK;
    if (e.v_max > V * slack || e.v_min < -V * slack)
    {
        return 0;
    }
    if (e.a_max_abs > A * slack)
    {
        return 0;
    }
    if (e.j_max_abs > J * slack)
    {
        return 0;
    }
    return 1;
}

/*============================ v_c 搜索 ============================*/

/** @brief D(v_c) ≜ 两个端点剖面的位移之和（该点取不到 → NaN） */
static float JerkDisp(float v0, float a0, float v1, float a1, float A, float J, float v_c)
{
    Jerk_Profile_s pa, pb;
    if (!JerkProfileSolve(v0, a0, v_c, A, J, &pa) || !JerkProfileSolve(v1, -a1, v_c, A, J, &pb))
    {
        return NAN;
    }
    return pa.dp + pb.dp;
}

/**
 * @brief 三元搜索 D 在 [a,b] 上的极小点
 * @note D 大致 V 形：v_c 越靠近 v0（或 v1），两个端点剖面各自要走的路就越短。
 *       不保证严格单峰，但它只用来产生"锚点"，结果仍会过 Jerk_Try 的限幅与时长校验。
 */
static float JerkMinDisp(float v0, float a0, float v1, float a1, float A, float J, float a, float b)
{
    for (uint8_t i = 0; i < JERK_TERNARY_ITER; i++)
    {
        const float m1 = a + (b - a) / 3.0f;
        const float m2 = b - (b - a) / 3.0f;
        const float d1 = JerkDisp(v0, a0, v1, a1, A, J, m1);
        const float d2 = JerkDisp(v0, a0, v1, a1, A, J, m2);
        if (!Jerk_Finite(d1))
        {
            a = m1;
        }
        else if (!Jerk_Finite(d2))
        {
            b = m2;
        }
        else if (d1 <= d2)
        {
            b = m2;
        }
        else
        {
            a = m1;
        }
    }
    return 0.5f * (a + b);
}

/**
 * @brief 正半轴的最优 v_c：满足 D(v_c) ≤ dp 的**最大** v_c
 * @note 正半轴的可行判据是 D ≤ dp（多出来的位移由 v_c > 0 的巡航补上）；而总时长里
 *       巡航项 (dp−D)/v_c 随 v_c → 0 发散，故正半轴上**越靠边界越优**，直接二分打到
 *       D = dp 的分界点即可（D(V) ≤ dp 时答案就是"在 V 上巡航"）。
 * @return 1 有可行点（*out_vc 已填）；0 正半轴无解（D 的极小值都 > dp）
 */
static uint8_t JerkBestPositive(float v0, float a0, float v1, float a1, float dp, float A, float J, float V, float *out_vc)
{
    const float lo0 = JERK_POS_V_MIN * V;
    const float vm = JerkMinDisp(v0, a0, v1, a1, A, J, lo0, V);
    const float dvm = JerkDisp(v0, a0, v1, a1, A, J, vm);

    if (!Jerk_Finite(dvm) || dvm > dp)
    {
        return 0; /* 连 D 的最小值都超过 dp：正半轴无论怎么走都"走多了" */
    }

    const float dV = JerkDisp(v0, a0, v1, a1, A, J, V);
    if (Jerk_Finite(dV) && dV <= dp)
    {
        *out_vc = V; /* 按 V 巡航也走不够 → 直接在 V 上巡航 */
        return 1;
    }

    float lo = vm, hi = V; /* D(lo) ≤ dp ≤ D(hi)：二分找分界 */
    for (uint8_t i = 0; i < JERK_BISECT_ITER; i++)
    {
        const float m = 0.5f * (lo + hi);
        const float dm = JerkDisp(v0, a0, v1, a1, A, J, m);
        if (Jerk_Finite(dm) && dm <= dp)
        {
            lo = m;
        }
        else
        {
            hi = m;
        }
    }
    *out_vc = lo;
    return 1;
}

/** @brief 试探一个 v_c：可行且更短就刷新当前最优 */
static void Jerk_Try(float v0, float a0, float v1, float a1, float dp, float A, float J, float V, float v_c, uint8_t *found, float *best_time, float *best_vc)
{
    Jerk_Junction_s jn;
    if (!JerkJunctionSolve(v0, a0, v1, a1, dp, A, J, v_c, &jn))
    {
        return;
    }
    if (!JerkJunctionLimits(&jn, v0, a0, v1, a1, V, A, J))
    {
        return;
    }
    if (!*found || jn.time < *best_time)
    {
        *found = 1;
        *best_time = jn.time;
        *best_vc = v_c;
    }
}

/**
 * @brief 在 v_c ∈ [-V, V] 上找最优的 junction 候选
 * @param dp 归一化位移，调用方保证 ≥ 0
 * @return 1 找到（best 已填好）；0 整个区间都不行
 * @note 不假设 D(v_c) 单调：粗扫 + 自适应锚点 + 以最优点为中心的局部再分。
 * @note **必须**有自适应锚点：可行区间可能整个落在粗扫网格点之间。典型是
 *       v0=v1≠0 且位移极短（D 的极小值在 v_c≈v0 处才 < dp，区间宽度 ≪ 网格步长），
 *       以及静止起步的极小位移（需要的 v_c 远小于网格步长）。只靠粗扫会整段漏掉。
 */
static uint8_t JerkScan(float v0, float a0, float v1, float a1, float dp, float A, float J, float V, Jerk_Junction_s *best)
{
    uint8_t found = 0;
    float best_time = 0.0f;
    float best_vc = 0.0f;

    const float step = (2.0f * V) / (float)(JERK_GRID_NUM - 1);

    for (uint16_t i = 0; i < JERK_GRID_NUM; i++)
    {
        float v_c = -V + step * (float)i;
        Jerk_Try(v0, a0, v1, a1, dp, A, J, V, v_c, &found, &best_time, &best_vc);
    }

    /* 自适应锚点：正半轴上"恰好打到 dp"的那个 v_c（解析地二分出来） */
    float v_anchor = 0.0f;
    if (JerkBestPositive(v0, a0, v1, a1, dp, A, J, V, &v_anchor))
    {
        Jerk_Try(v0, a0, v1, a1, dp, A, J, V, v_anchor, &found, &best_time, &best_vc);
    }

    if (!found)
    {
        return 0;
    }

    float lo = best_vc - step;
    float hi = best_vc + step;
    for (uint8_t r = 0; r < JERK_REFINE_ROUNDS; r++)
    {
        if (lo < -V)
        {
            lo = -V;
        }
        if (hi > V)
        {
            hi = V;
        }
        const float h = (hi - lo) / (float)JERK_REFINE_DIV;
        if (!(h > 0.0f))
        {
            break;
        }
        for (uint16_t k = 1; k < JERK_REFINE_DIV; k++)
        {
            Jerk_Try(v0, a0, v1, a1, dp, A, J, V, lo + h * (float)k, &found, &best_time, &best_vc);
        }
        lo = best_vc - h;
        hi = best_vc + h;
    }

    return (uint8_t)(JerkJunctionSolve(v0, a0, v1, a1, dp, A, J, best_vc, best) && JerkJunctionLimits(best, v0, a0, v1, a1, V, A, J));
}

/*============================ 相位表 ============================*/

/** @brief 追加一个相位（空相位/溢出直接跳过） */
static void Jerk_PushPhase(Jerk_Cand_s *c, float jerk, float duration)
{
    if (!(duration >= LIB_TRAJ_EPS_TIME) || c->num >= JERK_PHASE_MAX)
    {
        return;
    }
    c->ph[c->num].jerk = jerk;
    c->ph[c->num].duration = duration;
    c->num++;
}

/** @brief junction 候选 → 相位表（B 段是 pb 的时间反演：顺序倒过来） */
static void Jerk_BuildJunctionPhases(const Jerk_Junction_s *jn, Jerk_Cand_s *c)
{
    c->num = 0;
    Jerk_PushPhase(c, jn->pa.j[0], jn->pa.t[0]);
    Jerk_PushPhase(c, jn->pa.j[1], jn->pa.t[1]);
    Jerk_PushPhase(c, jn->pa.j[2], jn->pa.t[2]);
    Jerk_PushPhase(c, 0.0f, jn->t_c);
    Jerk_PushPhase(c, jn->pb.j[2], jn->pb.t[2]);
    Jerk_PushPhase(c, jn->pb.j[1], jn->pb.t[1]);
    Jerk_PushPhase(c, jn->pb.j[0], jn->pb.t[0]);
}

/**
 * @brief 候选 2：先停再走（A → 静止↔静止 → B）
 * @return 1 已生成相位表（未做终态/限幅校验，交给 JerkValidate）；0 连静止都停不下
 * @note 中间段位移可正可负：负位移是把整段镜像后再解（乘 sgn 还原）。
 */
static uint8_t JerkStopAndGo(float v0, float a0, float v1, float a1, float dp, float A, float J, float V, Jerk_Cand_s *c)
{
    Jerk_Profile_s pa, pb;
    if (!JerkProfileSolve(v0, a0, 0.0f, A, J, &pa))
    {
        return 0;
    }
    if (!JerkProfileSolve(v1, -a1, 0.0f, A, J, &pb))
    {
        return 0;
    }

    const float d_m = dp - pa.dp - pb.dp;

    c->num = 0;
    Jerk_PushPhase(c, pa.j[0], pa.t[0]);
    Jerk_PushPhase(c, pa.j[1], pa.t[1]);
    Jerk_PushPhase(c, pa.j[2], pa.t[2]);

    if (fabsf(d_m) > Jerk_TolP(dp, v0, v1))
    {
        const float sgn = (d_m < 0.0f) ? -1.0f : 1.0f;
        Jerk_Junction_s mid;
        if (!JerkScan(0.0f, 0.0f, 0.0f, 0.0f, sgn * d_m, A, J, V, &mid))
        {
            return 0;
        }
        Jerk_Cand_s mc;
        Jerk_BuildJunctionPhases(&mid, &mc);
        for (uint16_t i = 0; i < mc.num; i++)
        {
            Jerk_PushPhase(c, sgn * mc.ph[i].jerk, mc.ph[i].duration);
        }
    }

    Jerk_PushPhase(c, pb.j[2], pb.t[2]);
    Jerk_PushPhase(c, pb.j[1], pb.t[1]);
    Jerk_PushPhase(c, pb.j[0], pb.t[0]);
    return 1;
}

/*============================ 校验与落段 ============================*/

/**
 * @brief 独立校验相位表：从 (0, v0, a0) 积一遍，检查终态精度 + 全程限幅
 * @param out_time 输出总时长 (s)
 * @return 1 通过；0 不通过
 */
static uint8_t JerkValidate(const Jerk_Cand_s *c, float v0, float a0, float dp, float v1, float a1, float V, float A, float J, float *out_time)
{
    float p = 0.0f, v = v0, a = a0, time = 0.0f;
    Jerk_Ext_s e;
    Jerk_ExtInit(&e, v0, a0);

    for (uint16_t i = 0; i < c->num; i++)
    {
        Jerk_ExtPhase(&e, v, a, c->ph[i].jerk, c->ph[i].duration);
        Jerk_Integrate(&p, &v, &a, c->ph[i].jerk, c->ph[i].duration);
        time += c->ph[i].duration;
    }

    const float slack = 1.0f + JERK_REL_SLACK;
    if (e.v_max > V * slack || e.v_min < -V * slack)
    {
        return 0;
    }
    if (e.a_max_abs > A * slack)
    {
        return 0;
    }
    if (e.j_max_abs > J * slack)
    {
        return 0;
    }

    if (fabsf(p - dp) > Jerk_TolP(dp, v0, v1))
    {
        return 0;
    }
    if (fabsf(v - v1) > Jerk_TolV(v0, v1, V))
    {
        return 0;
    }
    if (fabsf(a - a1) > Jerk_TolA(a0, a1, A))
    {
        return 0;
    }

    *out_time = time;
    return 1;
}

/**
 * @brief 相位表 → LibTraj_Traj_s
 * @param dir 归一化镜像系数（实际 jerk = dir × 相位 jerk）
 * @param p0,v0,a0 真实起点状态（首段直接用真值构造，保证起点不是 (0,0,0)）
 * @return 0 成功；-1 失败（调用方负责把 traj 复原）
 */
static int8_t JerkMaterialize(LibTraj_Traj_s *traj, const Jerk_Cand_s *c, float dir, float p0, float v0, float a0)
{
    LibTrajReset(traj);

    uint8_t first = 1;
    for (uint16_t i = 0; i < c->num; i++)
    {
        if (first)
        {
            LibTraj_Seg_s seg;
            LibTrajSegMakeConstJerk(&seg, p0, v0, a0, dir * c->ph[i].jerk, c->ph[i].duration);
            if (LibTrajAppendSeg(traj, &seg) != 0)
            {
                return -1;
            }
            first = 0;
        }
        else if (LibTrajAppendJerk(traj, dir * c->ph[i].jerk, c->ph[i].duration) != 0)
        {
            return -1;
        }
    }

    if (first) /* 相位表全空：给一个零时长段，让 traj 非空、采样输出就是起点 */
    {
        LibTraj_Seg_s seg;
        LibTrajSegMakeConstJerk(&seg, p0, v0, a0, 0.0f, 0.0f);
        if (LibTrajAppendSeg(traj, &seg) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/*============================ 规划入口 ============================*/

LibTrajJerkPlan_Status_e LibTrajJerkPlan(LibTraj_Traj_s *traj, const LibTrajJerkPlan_Request_s *req)
{
    if (traj == NULL || req == NULL)
    {
        return LIB_TRAJ_JERK_PLAN_ERR_PARAM;
    }
    if (!(req->vmax > 0.0f) || !(req->amax > 0.0f) || !(req->jmax > 0.0f))
    {
        return LIB_TRAJ_JERK_PLAN_ERR_PARAM;
    }

    const float V = req->vmax;
    const float A = req->amax;
    const float J = req->jmax;
    const float v0 = req->v0, a0 = req->a0, v1 = req->v1, a1 = req->a1;
    const float dp_raw = req->p1 - req->p0;

    if (!Jerk_Finite(v0) || !Jerk_Finite(v1) || !Jerk_Finite(a0) || !Jerk_Finite(a1) || !Jerk_Finite(dp_raw))
    {
        return LIB_TRAJ_JERK_PLAN_ERR_PARAM;
    }

    const float slack = 1.0f + JERK_REL_SLACK;
    if (fabsf(v0) > V * slack || fabsf(v1) > V * slack)
    {
        return LIB_TRAJ_JERK_PLAN_ERR_LIMIT;
    }
    if (fabsf(a0) > A * slack || fabsf(a1) > A * slack)
    {
        return LIB_TRAJ_JERK_PLAN_ERR_LIMIT;
    }

    /* 归一化：位移为负就整体镜像 (p,v,a) → (-p,-v,-a)，jerk 最后乘 dir 还原 */
    const float dir = (dp_raw < 0.0f) ? -1.0f : 1.0f;
    const float dp = dir * dp_raw; /* ≥ 0 */
    const float nv0 = dir * v0, na0 = dir * a0;
    const float nv1 = dir * v1, na1 = dir * a1;

    /* 原地不动（六边界全等）：给一个零时长段，保证 traj 非空、输出就是 p0 */
    if (fabsf(dp) <= Jerk_TolP(0.0f, nv0, nv1) && fabsf(nv1 - nv0) <= Jerk_TolV(nv0, nv1, V) && fabsf(na1 - na0) <= Jerk_TolA(na0, na1, A))
    {
        LibTraj_Seg_s seg;
        LibTrajSegMakeConstJerk(&seg, req->p0, v0, a0, 0.0f, 0.0f);
        LibTrajReset(traj);
        return (LibTrajAppendSeg(traj, &seg) == 0) ? LIB_TRAJ_JERK_PLAN_OK : LIB_TRAJ_JERK_PLAN_ERR_INTERNAL;
    }

    Jerk_Cand_s cand;
    float cand_time = 0.0f;
    uint8_t have_cand = 0;

    /* 候选 1：junction 型（常规场景即标准 7 段 S 曲线） */
    Jerk_Junction_s jn;
    if (JerkScan(nv0, na0, nv1, na1, dp, A, J, V, &jn))
    {
        Jerk_Cand_s c1;
        float t1;
        Jerk_BuildJunctionPhases(&jn, &c1);
        if (JerkValidate(&c1, nv0, na0, dp, nv1, na1, V, A, J, &t1))
        {
            cand = c1;
            cand_time = t1;
            have_cand = 1;
        }
    }

    /* 候选 2：先停再走（安全网），按"可行且更短"择优 */
    Jerk_Cand_s c2;
    float t2;
    if (JerkStopAndGo(nv0, na0, nv1, na1, dp, A, J, V, &c2) && JerkValidate(&c2, nv0, na0, dp, nv1, na1, V, A, J, &t2))
    {
        if (!have_cand || t2 < cand_time)
        {
            cand = c2;
            cand_time = t2;
            have_cand = 1;
        }
    }

    if (!have_cand)
    {
        return LIB_TRAJ_JERK_PLAN_ERR_INFEASIBLE; /* traj 保持上一版有效计划 */
    }

    if (JerkMaterialize(traj, &cand, dir, req->p0, v0, a0) != 0)
    {
        LibTrajReset(traj);
        return LIB_TRAJ_JERK_PLAN_ERR_INTERNAL;
    }

    /* 独立复算：走 LibTrajEval 的 Horner 路径，与上面的积分是两条代码路径 */
    LibTraj_Output_s end;
    LibTrajEndState(traj, &end);
    if (fabsf(end.position - req->p1) > Jerk_TolP(dp, v0, v1) || fabsf(end.velocity - req->v1) > Jerk_TolV(v0, v1, V) ||
        fabsf(end.acceleration - req->a1) > Jerk_TolA(a0, a1, A))
    {
        LibTrajReset(traj);
        return LIB_TRAJ_JERK_PLAN_ERR_INTERNAL;
    }

    return LIB_TRAJ_JERK_PLAN_OK;
}

#endif /* LIB_TRAJ_USED */
