/**
 * @file lib_traj.c
 * @brief 通用单轴分段多项式轨迹（核心）实现：段构造 / 拼接 / 按时间采样 x-v-a-j
 *
 * @note 求值：对每段用 Horner 求 p，v/a/j 用系数逐阶求导后同样 Horner ——
 *       位置 4 次 → 最多 4 趟 ≤5 项，约 20 flop，O(1) 且无累计误差。
 *
 * @note 越界钳位：
 *       - t < 0        → 按 0 取（finished = (0 >= total)）
 *       - t ≥ total    → 按 total 取（末段 τ = duration，finished = 1）
 *       - 空轨迹       → 输出全 0、finished = 1
 *       段区间约定为**半开** [段起点, 段起点+duration)，边界点归属后一段。
 */

#include "lib_traj.h"

#ifndef LIB_TRAJ_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_TRAJ_USED

#include <math.h>
#include <string.h>

/*============================ 私有函数 ============================*/

/**
 * @brief 按系数直接求值（Horner），v/a/j 为逐阶导数
 * @note 不按 degree 分支：构造函数已把 degree 以上的系数清零，
 *       高阶导数自然退化为 0。
 */
static void Traj_EvalCoeff(const float *c, float tau, LibTraj_Output_s *out)
{
    out->position = (((c[4] * tau + c[3]) * tau + c[2]) * tau + c[1]) * tau + c[0];
    out->velocity = ((4.0f * c[4] * tau + 3.0f * c[3]) * tau + 2.0f * c[2]) * tau + c[1];
    out->acceleration = (12.0f * c[4] * tau + 6.0f * c[3]) * tau + 2.0f * c[2];
    out->jerk = 24.0f * c[4] * tau + 6.0f * c[3];
}

/** @brief 段非法（空指针 / 时长为负或 NaN）检查，同时提供"合法段"的默认值 */
static uint8_t Traj_SegValid(const LibTraj_Seg_s *seg)
{
    return (uint8_t)(seg != NULL && seg->degree <= LIB_TRAJ_DEG_MAX && seg->duration >= 0.0f);
}

/** @brief 输出清零（空轨迹/空段的统一回答） */
static void Traj_ZeroOutput(LibTraj_Output_s *out)
{
    out->position = 0.0f;
    out->velocity = 0.0f;
    out->acceleration = 0.0f;
    out->jerk = 0.0f;
    out->finished = 1;
}

/**
 * @brief 定位 t 落在哪一段，并给出段内时间 τ
 * @param traj 轨迹（非空）
 * @param t    绝对时间，调用方保证 0 ≤ t < total
 * @param idx  输出段下标
 * @return 段内时间 τ
 */
static float Traj_Locate(const LibTraj_Traj_s *traj, float t, uint16_t *idx)
{
    float cum = 0.0f;
    for (uint16_t i = 0; i < traj->seg_num; i++)
    {
        float d = traj->seg[i].duration;
        if (t < cum + d)
        {
            *idx = i;
            return t - cum;
        }
        cum += d;
    }
    *idx = (uint16_t)(traj->seg_num - 1); /* 浮点兜底：视为落在末段 */
    return traj->seg[traj->seg_num - 1].duration;
}

/*============================ 计划管理 ============================*/

void LibTrajReset(LibTraj_Traj_s *traj)
{
    if (traj == NULL)
    {
        return;
    }
    memset(traj, 0, sizeof(*traj));
}

uint8_t LibTrajIsEmpty(const LibTraj_Traj_s *traj)
{
    return (uint8_t)(traj == NULL || traj->seg_num == 0);
}

int8_t LibTrajAppendSeg(LibTraj_Traj_s *traj, const LibTraj_Seg_s *seg)
{
    if (traj == NULL || !Traj_SegValid(seg))
    {
        return -1;
    }
    if (traj->seg_num >= LIB_TRAJ_SEG_MAX)
    {
        return -1;
    }

    traj->seg[traj->seg_num] = *seg;
    traj->seg_num++;
    traj->total += seg->duration;
    return 0;
}

int8_t LibTrajAppendJerk(LibTraj_Traj_s *traj, float jerk, float duration)
{
    if (traj == NULL)
    {
        return -1;
    }
    if (!(duration >= LIB_TRAJ_EPS_TIME)) /* 空段/负段/NaN：跳过，不算失败 */
    {
        return 0;
    }

    /* 以当前末端状态为新段初值 → 按构造即 C² 连续 */
    LibTraj_Output_s end;
    LibTrajEndState(traj, &end);

    LibTraj_Seg_s seg;
    LibTrajSegMakeConstJerk(&seg, end.position, end.velocity, end.acceleration, jerk, duration);
    return LibTrajAppendSeg(traj, &seg);
}

void LibTrajStartState(const LibTraj_Traj_s *traj, LibTraj_Output_s *out)
{
    if (out == NULL)
    {
        return;
    }
    LibTrajEval(traj, 0.0f, out);
}

void LibTrajEndState(const LibTraj_Traj_s *traj, LibTraj_Output_s *out)
{
    if (out == NULL)
    {
        return;
    }
    LibTrajEval(traj, (traj != NULL) ? traj->total : 0.0f, out);
}

/*============================ 段构造 ============================*/

void LibTrajSegMakeConstJerk(LibTraj_Seg_s *seg, float p0, float v0, float a0, float jerk, float duration)
{
    if (seg == NULL)
    {
        return;
    }
    memset(seg, 0, sizeof(*seg));
    seg->c[0] = p0;
    seg->c[1] = v0;
    seg->c[2] = 0.5f * a0;
    seg->c[3] = jerk / 6.0f;
    seg->duration = duration;
    seg->degree = 3;
}

void LibTrajSegMakeConstSnap(LibTraj_Seg_s *seg, float p0, float v0, float a0, float j0, float snap, float duration)
{
    if (seg == NULL)
    {
        return;
    }
    memset(seg, 0, sizeof(*seg));
    seg->c[0] = p0;
    seg->c[1] = v0;
    seg->c[2] = 0.5f * a0;
    seg->c[3] = j0 / 6.0f;
    seg->c[4] = snap / 24.0f;
    seg->duration = duration;
    seg->degree = 4;
}

void LibTrajSegMakeCubicHermite(LibTraj_Seg_s *seg, float p0, float v0, float p1, float v1, float duration)
{
    if (seg == NULL)
    {
        return;
    }
    memset(seg, 0, sizeof(*seg));

    float h = duration;
    if (!(h > 0.0f)) /* 时长为 0/负/NaN：退化成一个点，避免除零 */
    {
        seg->c[0] = p0;
        seg->c[1] = v0;
        seg->duration = 0.0f;
        seg->degree = 3;
        return;
    }

    float dp = p1 - p0 - v0 * h;
    float dv = v1 - v0;

    seg->c[0] = p0;
    seg->c[1] = v0;
    seg->c[2] = (3.0f * dp - h * dv) / (h * h);
    seg->c[3] = (h * dv - 2.0f * dp) / (h * h * h);
    seg->duration = h;
    seg->degree = 3;
}

void LibTrajSegMakeQuarticHermite(LibTraj_Seg_s *seg, float p0, float v0, float a0, float p1, float v1, float duration)
{
    if (seg == NULL)
    {
        return;
    }
    memset(seg, 0, sizeof(*seg));

    float h = duration;
    if (!(h > 0.0f))
    {
        seg->c[0] = p0;
        seg->c[1] = v0;
        seg->c[2] = 0.5f * a0;
        seg->duration = 0.0f;
        seg->degree = 4;
        return;
    }

    float dp = p1 - p0 - v0 * h - 0.5f * a0 * h * h;
    float dv = v1 - v0 - a0 * h;

    seg->c[0] = p0;
    seg->c[1] = v0;
    seg->c[2] = 0.5f * a0;
    seg->c[3] = (4.0f * dp - h * dv) / (h * h * h);
    seg->c[4] = (h * dv - 3.0f * dp) / (h * h * h * h);
    seg->duration = h;
    seg->degree = 4;
}

int8_t LibTrajSegMakeRaw(LibTraj_Seg_s *seg, const float *coeff, uint8_t degree, float duration)
{
    if (seg == NULL || coeff == NULL || degree > LIB_TRAJ_DEG_MAX)
    {
        return -1;
    }
    if (!(duration >= 0.0f))
    {
        return -1;
    }

    memset(seg, 0, sizeof(*seg));
    for (uint8_t k = 0; k <= degree; k++)
    {
        seg->c[k] = coeff[k];
    }
    seg->duration = duration;
    seg->degree = degree;
    return 0;
}

/*============================ 采样 ============================*/

void LibTrajSegEval(const LibTraj_Seg_s *seg, float tau, LibTraj_Output_s *out)
{
    if (out == NULL)
    {
        return;
    }
    if (!Traj_SegValid(seg))
    {
        Traj_ZeroOutput(out);
        return;
    }

    if (tau < 0.0f)
    {
        tau = 0.0f;
    }
    else if (tau > seg->duration)
    {
        tau = seg->duration;
    }

    Traj_EvalCoeff(seg->c, tau, out);
    out->finished = (uint8_t)(tau >= seg->duration);
}

void LibTrajEval(const LibTraj_Traj_s *traj, float t, LibTraj_Output_s *out)
{
    if (out == NULL)
    {
        return;
    }
    if (traj == NULL || traj->seg_num == 0)
    {
        Traj_ZeroOutput(out);
        return;
    }

    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t >= traj->total)
    {
        LibTrajSegEval(&traj->seg[traj->seg_num - 1], traj->seg[traj->seg_num - 1].duration, out);
        out->finished = 1;
        return;
    }

    uint16_t idx;
    float tau = Traj_Locate(traj, t, &idx);
    LibTraj_Seg_s seg = traj->seg[idx]; /* 按值取，避免调用方传进来的别名问题 */
    LibTrajSegEval(&seg, tau, out);
    out->finished = 0;
}

void LibTrajStep(LibTraj_Traj_s *traj, float dt, LibTraj_Output_s *out)
{
    if (out == NULL)
    {
        return;
    }
    if (traj == NULL || traj->seg_num == 0)
    {
        Traj_ZeroOutput(out);
        return;
    }
    if (!(dt > 0.0f)) /* 负值/NaN 按 0 取：只重采样，不推进 */
    {
        dt = 0.0f;
    }

    traj->t_now += dt;

    /* 借 seg_hint 顺序推进（正常调用均摊 O(1)）；边界点归属后一段 */
    while (traj->seg_hint < traj->seg_num)
    {
        float d = traj->seg[traj->seg_hint].duration;
        if (traj->t_now < traj->seg_start + d)
        {
            break;
        }
        traj->seg_start += d;
        traj->seg_hint++;
    }

    if (traj->seg_hint >= traj->seg_num) /* 到点：钳在总时长上，输出末态 */
    {
        traj->t_now = traj->total;
        traj->seg_hint = (uint16_t)(traj->seg_num - 1);
        traj->seg_start = traj->total - traj->seg[traj->seg_num - 1].duration;
        LibTrajSegEval(&traj->seg[traj->seg_num - 1], traj->seg[traj->seg_num - 1].duration, out);
        out->finished = 1;
        return;
    }

    float tau = traj->t_now - traj->seg_start;
    LibTraj_Seg_s seg = traj->seg[traj->seg_hint];
    LibTrajSegEval(&seg, tau, out);
    out->finished = 0;
}

#endif /* LIB_TRAJ_USED */
