/**
 * @file lib_mahony.c
 * @brief 通用 Mahony 互补滤波内核实现
 *
 * @note 分层、err 的定义、参数分区全在 lib_mahony.h 的头注释里，本文件只记
 *       实现层面的取舍
 * @note 参考：Mahony et al. "Nonlinear Complementary Filters on
 *       the Special Orthogonal Group" IEEE TAC 2008
 *
 * 内核流程（模型部分在调用方）：
 * 1. 取调用方给的参考误差 err（= 机体系「量测 × 估计」）
 * 2. PI 控制器用误差修正陀螺仪角速率
 * 3. 一阶（欧拉）积分更新四元数
 * 4. 四元数归一化
 */

#include "lib_mahony.h"
#include "app_cfg.h"

#ifdef LIB_MAHONY_USED

/*============================ 内部常量 ============================*/

/* 内核常量是 #ifndef 可覆盖的：在 app_cfg.h（或任何先于本文件包含的头文件、
 * 编译选项 -D）里定义同名宏即可覆盖，不必改算法源码。
 * 覆盖点只需早于本文件包含 —— 本 .c 在常量之前就 #include "app_cfg.h"。
 * ⚠ 覆盖值要带类型后缀（如 1e-6f 而不是 1e-6），否则会在 float 表达式里走 double。 */

/* 最小 dt (s)：dt 小于它直接跳过本帧（防零步长、防同一采样被重复积分） */
#ifndef MAHONY_DT_MIN
#define MAHONY_DT_MIN (1e-7f)
#endif // !MAHONY_DT_MIN

/*============================ 公开接口实现 ============================*/

void MahonyInit(MahonyInstance *inst, const Mahony_Init_Config_s *config)
{
    if (inst == NULL || config == NULL)
    {
        return;
    }

    inst->kp = config->kp;
    inst->ki = config->ki;

    /* 四元数重置为单位四元数 */
    inst->quat = Lib_Math_QuatIdentity();

    /* 清零积分误差 */
    inst->integral_fb.x = 0.0f;
    inst->integral_fb.y = 0.0f;
    inst->integral_fb.z = 0.0f;
}

vector3_t MahonyErr(const MahonyInstance *inst, vector3_t meas, vector3_t ref)
{
    vector3_t err = {0.0f, 0.0f, 0.0f};

    if (inst == NULL)
    {
        return err;
    }

    /* 世界系参考方向 → 机体系估计：est = R^T·ref
     * Lib_Math_QuatRotateVector 是 q⊗v⊗q⁻¹（把机体系向量转到世界系），
     * 取共轭即反向（世界系 → 机体系）。四元数全程归一化，共轭即逆。
     * 注意别把这一步写成"用 R 而不是 R^T"——那样误差方向会反。 */
    vector3_t est = Lib_Math_QuatRotateVector(Lib_Math_QuatConjugate(inst->quat), ref);

    /* 误差 = 量测 × 估计（叉积，顺序不能反）；两者都是单位向量时模长 = sin(夹角) */
    err = Lib_Math_Vec3Cross(meas, est);

    return err;
}

void MahonyUpdate(MahonyInstance *inst, vector3_t gyro, vector3_t err, float dt)
{
    if (inst == NULL)
    {
        return;
    }

    /* ======================== 1. 校验 dt ======================== */

    if (dt < MAHONY_DT_MIN)
    {
        return;
    }

    /* ======================== 2. 误差取半 ========================
     * q̇ = ½·q⊗ω 的那个 ½：传统嵌入式 Mahony 实现把它折进误差向量，本内核按同一
     * 口径取半，这样 kp / ki 的整定值（六轴 0.5~1.0）可以直接沿用。
     * err 为零向量时下面三项为零 → 退化为纯陀螺积分。 */

    const float hex = 0.5f * err.x;
    const float hey = 0.5f * err.y;
    const float hez = 0.5f * err.z;

    /* ======================== 3. PI 控制器修正角速率 ======================== */

    if (inst->ki > 0.0f)
    {
        /* 积分项累积（ki = 0 时整个积分通路不参与，见头注释） */
        inst->integral_fb.x += inst->ki * hex * dt;
        inst->integral_fb.y += inst->ki * hey * dt;
        inst->integral_fb.z += inst->ki * hez * dt;

        /* 比例 + 积分修正角速率 */
        gyro.x += inst->kp * hex + inst->integral_fb.x;
        gyro.y += inst->kp * hey + inst->integral_fb.y;
        gyro.z += inst->kp * hez + inst->integral_fb.z;
    }
    else
    {
        /* 仅比例修正 */
        gyro.x += inst->kp * hex;
        gyro.y += inst->kp * hey;
        gyro.z += inst->kp * hez;
    }

    /* ======================== 4. 四元数更新（一阶积分）================ */

    const float q0 = inst->quat.w;
    const float q1 = inst->quat.x;
    const float q2 = inst->quat.y;
    const float q3 = inst->quat.z;

    quaternion_t q_new;
    q_new.w = q0 + (-q1 * gyro.x - q2 * gyro.y - q3 * gyro.z) * (0.5f * dt);
    q_new.x = q1 + (q0 * gyro.x + q2 * gyro.z - q3 * gyro.y) * (0.5f * dt);
    q_new.y = q2 + (q0 * gyro.y - q1 * gyro.z + q3 * gyro.x) * (0.5f * dt);
    q_new.z = q3 + (q0 * gyro.z + q1 * gyro.y - q2 * gyro.x) * (0.5f * dt);

    /* ======================== 5. 四元数归一化 ======================== */

    inst->quat = Lib_Math_QuatNormalize(q_new);
}

void MahonyReset(MahonyInstance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    inst->quat = Lib_Math_QuatIdentity();

    inst->integral_fb.x = 0.0f;
    inst->integral_fb.y = 0.0f;
    inst->integral_fb.z = 0.0f;
}

#endif /* LIB_MAHONY_USED */
