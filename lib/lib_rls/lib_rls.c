/**
 * @file lib_rls.c
 * @brief 通用递推最小二乘（RLS）实现，见 lib_rls.h
 *
 * @note 递推（遗忘因子 λ）：
 *         K = P·φ / (λ + φᵀ·P·φ)
 *         θ ← θ + K·(y − φᵀθ)
 *         P ← (P − K·φᵀ·P) / λ
 *
 * @note 维数由实例在编译期确定；缓冲区全部由 LIB_RLS_INSTANCE_DEF 静态绑定，
 *       本文件不做任何动态分配。
 *
 * @note 更新是原子的：先在实例工作区 w_* 里算完并做完全部检查，
 *       再一次性写回 theta/P。失败（NaN/Inf/分母过小）时旧 θ/P 保持不变，
 *       不会留下"更新了一半"的协方差。
 *
 * @note 本文件不做采样门控、归一化、物理区间判断 —— 那些属于调用方。
 */

#ifndef LIB_RLS_STANDALONE
#include "app_cfg.h"
#endif

#include "lib_rls.h"

#ifdef LIB_RLS_USED

#include <math.h>

#include "lib_math.h"

/*============================ 私有助手 ============================*/

static uint8_t Rls_Finite(float v)
{
    return (uint8_t)(isfinite(v) != 0);
}

/**
 * @brief 实例缓冲区是否已由 LIB_RLS_INSTANCE_DEF 正确绑定
 */
static uint8_t Rls_InstanceValid(const LibRls_Instance_s *inst)
{
    if (inst == NULL || inst->dim < 1U)
    {
        return 0U;
    }
    return (uint8_t)(inst->theta != NULL && inst->theta_init != NULL && inst->P != NULL && inst->gain != NULL && inst->p_phi != NULL && inst->w_phi != NULL &&
                     inst->w_theta != NULL && inst->w_gain != NULL && inst->w_p_phi != NULL && inst->w_P != NULL);
}

/**
 * @brief 配置合法性检查（Init 失败时不得改动实例）
 */
static uint8_t Rls_ConfigValid(const LibRls_Config_s *cfg, uint8_t dim)
{
    uint8_t i;

    if (cfg == NULL || dim < 1U)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->lambda) == 0U || cfg->lambda <= 0.0f || cfg->lambda > 1.0f)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->p_init) == 0U || cfg->p_init <= 0.0f)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->p_min) == 0U || cfg->p_min < 0.0f)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->p_max) == 0U || cfg->p_max < cfg->p_min)
    {
        return 0U;
    }
    /* P(0) 必须落在允许区间内，否则第一次更新会立刻被钳位，语义不直观 */
    if (cfg->p_init < cfg->p_min || cfg->p_init > cfg->p_max)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->param_min) == 0U || Rls_Finite(cfg->param_max) == 0U || cfg->param_min > cfg->param_max)
    {
        return 0U;
    }
    if (Rls_Finite(cfg->innovation_limit) == 0U || cfg->innovation_limit < 0.0f)
    {
        return 0U;
    }
    if (cfg->fail_policy != LIB_RLS_FAIL_FREEZE && cfg->fail_policy != LIB_RLS_FAIL_RESET)
    {
        return 0U;
    }
    if (cfg->theta_init != NULL)
    {
        for (i = 0U; i < dim; i++)
        {
            if (Rls_Finite(cfg->theta_init[i]) == 0U)
            {
                return 0U;
            }
        }
    }
    return 1U;
}

/**
 * @brief 把实例恢复为初值（调用前 inst 必须已初始化）
 */
static void Rls_ResetState(LibRls_Instance_s *inst)
{
    uint8_t i;
    uint8_t j;
    uint8_t dim = inst->dim;

    inst->frozen = 0U;
    inst->status = LIB_RLS_OK;
    inst->update_count = 0U;
    inst->last_prediction = 0.0f;
    inst->last_innovation = 0.0f;
    inst->last_denominator = 0.0f;

    for (i = 0U; i < dim; i++)
    {
        inst->theta[i] = inst->theta_init[i];
        inst->gain[i] = 0.0f;
        inst->p_phi[i] = 0.0f;
        for (j = 0U; j < dim; j++)
        {
            inst->P[i * dim + j] = (i == j) ? inst->p_init : 0.0f;
        }
    }
}

/**
 * @brief 按 fail_policy 处理数值失效
 * @return 原样返回 status，便于调用方直接 `return Rls_Fail(...)`
 */
static LibRls_Status_e Rls_Fail(LibRls_Instance_s *inst, LibRls_Status_e status)
{
    if (inst->fail_policy == LIB_RLS_FAIL_RESET)
    {
        Rls_ResetState(inst);
    }
    else
    {
        inst->frozen = 1U;
    }
    inst->status = status;
    return status;
}

/*============================ 外部接口 ============================*/

LibRls_Status_e LibRlsInit(LibRls_Instance_s *inst, const LibRls_Config_s *cfg)
{
    uint8_t i;
    uint8_t dim;

    if (Rls_InstanceValid(inst) == 0U)
    {
        return LIB_RLS_ERR_ARG;
    }
    dim = inst->dim;
    if (Rls_ConfigValid(cfg, dim) == 0U)
    {
        return LIB_RLS_ERR_ARG;
    }

    inst->lambda = cfg->lambda;
    inst->p_init = cfg->p_init;
    inst->p_min = cfg->p_min;
    inst->p_max = cfg->p_max;
    inst->param_min = cfg->param_min;
    inst->param_max = cfg->param_max;
    inst->innovation_limit = cfg->innovation_limit;
    inst->fail_policy = cfg->fail_policy;

    for (i = 0U; i < dim; i++)
    {
        float init = (cfg->theta_init != NULL) ? cfg->theta_init[i] : 0.0f;
        inst->theta_init[i] = Lib_Math_Clamp(init, inst->param_min, inst->param_max);
    }

    inst->initialized = 1U;
    Rls_ResetState(inst);
    return LIB_RLS_OK;
}

LibRls_Status_e LibRlsReset(LibRls_Instance_s *inst)
{
    if (Rls_InstanceValid(inst) == 0U || inst->initialized == 0U)
    {
        return LIB_RLS_ERR_ARG;
    }
    Rls_ResetState(inst);
    return LIB_RLS_OK;
}

LibRls_Status_e LibRlsSetParams(LibRls_Instance_s *inst, const float *theta)
{
    uint8_t i;
    uint8_t dim;
    float *clamped;

    if (Rls_InstanceValid(inst) == 0U || inst->initialized == 0U || theta == NULL)
    {
        return LIB_RLS_ERR_ARG;
    }
    dim = inst->dim;
    clamped = inst->w_theta;

    for (i = 0U; i < dim; i++)
    {
        if (Rls_Finite(theta[i]) == 0U)
        {
            return LIB_RLS_ERR_ARG;
        }
        clamped[i] = Lib_Math_Clamp(theta[i], inst->param_min, inst->param_max);
    }
    for (i = 0U; i < dim; i++)
    {
        inst->theta[i] = clamped[i];
        inst->theta_init[i] = clamped[i];
    }
    return LIB_RLS_OK;
}

LibRls_Status_e LibRlsSetLambda(LibRls_Instance_s *inst, float lambda)
{
    if (Rls_InstanceValid(inst) == 0U || inst->initialized == 0U)
    {
        return LIB_RLS_ERR_ARG;
    }
    if (Rls_Finite(lambda) == 0U || lambda <= 0.0f || lambda > 1.0f)
    {
        return LIB_RLS_ERR_ARG;
    }
    inst->lambda = lambda;
    return LIB_RLS_OK;
}

float LibRlsPredict(const LibRls_Instance_s *inst, const float *phi)
{
    uint8_t i;
    float prediction = 0.0f;

    if (Rls_InstanceValid(inst) == 0U || inst->initialized == 0U || phi == NULL)
    {
        return 0.0f;
    }
    for (i = 0U; i < inst->dim; i++)
    {
        if (Rls_Finite(phi[i]) == 0U)
        {
            return 0.0f;
        }
        prediction += inst->theta[i] * phi[i];
    }
    return (Rls_Finite(prediction) != 0U) ? prediction : 0.0f;
}

LibRls_Status_e LibRlsUpdate(LibRls_Instance_s *inst, const float *phi, float y)
{
    uint8_t i;
    uint8_t j;
    uint8_t dim;
    float *phi_local;
    float *p_phi_new;
    float *gain_new;
    float *theta_new;
    float *p_new;
    float denominator;
    float prediction = 0.0f;
    float innovation;
    float innovation_used;
    float inv_lambda;

    if (Rls_InstanceValid(inst) == 0U)
    {
        return LIB_RLS_ERR_ARG;
    }
    if (inst->initialized == 0U || phi == NULL)
    {
        return LIB_RLS_ERR_ARG;
    }
    if (inst->frozen != 0U)
    {
        /* 不覆盖 status：保留进入冻结时的根因，便于上层诊断 */
        return LIB_RLS_ERR_FROZEN;
    }

    dim = inst->dim;
    phi_local = inst->w_phi;
    p_phi_new = inst->w_p_phi;
    gain_new = inst->w_gain;
    theta_new = inst->w_theta;
    p_new = inst->w_P;

    /* 先把回归量拷到工作区：允许调用方传入实例内部数组，也避免中途被改写 */
    for (i = 0U; i < dim; i++)
    {
        if (Rls_Finite(phi[i]) == 0U)
        {
            return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
        }
        phi_local[i] = phi[i];
    }
    if (Rls_Finite(y) == 0U)
    {
        return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
    }

    /* P·φ 与分母 λ + φᵀPφ */
    for (i = 0U; i < dim; i++)
    {
        float acc = 0.0f;
        for (j = 0U; j < dim; j++)
        {
            acc += inst->P[i * dim + j] * phi_local[j];
        }
        p_phi_new[i] = acc;
    }
    denominator = inst->lambda;
    for (i = 0U; i < dim; i++)
    {
        denominator += phi_local[i] * p_phi_new[i];
    }
    if (Rls_Finite(denominator) == 0U || denominator <= LIB_RLS_DENOM_EPS)
    {
        return Rls_Fail(inst, LIB_RLS_ERR_DENOM);
    }

    /* 预测与残差 */
    for (i = 0U; i < dim; i++)
    {
        prediction += inst->theta[i] * phi_local[i];
    }
    if (Rls_Finite(prediction) == 0U)
    {
        return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
    }
    innovation = y - prediction;
    if (Rls_Finite(innovation) == 0U)
    {
        return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
    }

    innovation_used = innovation;
    if (inst->innovation_limit > 0.0f && Lib_Math_Fabs(innovation_used) > inst->innovation_limit)
    {
        innovation_used = (innovation_used > 0.0f) ? inst->innovation_limit : -inst->innovation_limit;
    }

    inv_lambda = 1.0f / inst->lambda;

    /* 新参数：θ + K·innovation */
    for (i = 0U; i < dim; i++)
    {
        gain_new[i] = p_phi_new[i] / denominator;
        theta_new[i] = inst->theta[i] + gain_new[i] * innovation_used;
        theta_new[i] = Lib_Math_Clamp(theta_new[i], inst->param_min, inst->param_max);
        if (Rls_Finite(theta_new[i]) == 0U)
        {
            return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
        }
    }

    /* 新协方差：P ← (P − K·φᵀ·P) / λ */
    for (i = 0U; i < dim; i++)
    {
        for (j = 0U; j < dim; j++)
        {
            p_new[i * dim + j] = (inst->P[i * dim + j] - gain_new[i] * p_phi_new[j]) * inv_lambda;
        }
    }

    /* 对称化（只算上三角再回写），抑制长期数值漂移 */
    for (i = 0U; i < dim; i++)
    {
        for (j = (uint8_t)(i + 1U); j < dim; j++)
        {
            float avg = 0.5f * (p_new[i * dim + j] + p_new[j * dim + i]);
            p_new[i * dim + j] = avg;
            p_new[j * dim + i] = avg;
        }
    }

    /* 协方差钳位：先元素上下限，再对角下限 */
    for (i = 0U; i < dim; i++)
    {
        for (j = 0U; j < dim; j++)
        {
            p_new[i * dim + j] = Lib_Math_Clamp(p_new[i * dim + j], -inst->p_max, inst->p_max);
            if (Rls_Finite(p_new[i * dim + j]) == 0U)
            {
                return Rls_Fail(inst, LIB_RLS_ERR_NOT_FINITE);
            }
        }
        p_new[i * dim + i] = Lib_Math_Clamp(p_new[i * dim + i], inst->p_min, inst->p_max);
    }

    /* 全部检查通过，一次性提交 */
    for (i = 0U; i < dim; i++)
    {
        inst->theta[i] = theta_new[i];
        inst->gain[i] = gain_new[i];
        inst->p_phi[i] = p_phi_new[i];
        for (j = 0U; j < dim; j++)
        {
            inst->P[i * dim + j] = p_new[i * dim + j];
        }
    }
    inst->last_prediction = prediction;
    inst->last_innovation = innovation;
    inst->last_denominator = denominator;
    inst->update_count++;
    inst->status = LIB_RLS_OK;
    return LIB_RLS_OK;
}

#endif /* LIB_RLS_USED */
