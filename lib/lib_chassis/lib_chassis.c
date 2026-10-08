/**
 * @file lib_chassis.c
 * @brief 底盘运动学统一内核实现（雅可比正/逆解 + 力分配）
 * @author TRW
 * @date 2026-10-08
 *
 * @note 公式与约定见 lib_chassis.h / lib_chassis_def.h。本文件只做几何解算，不碰电机/闭环。
 */

#include "lib_chassis.h"
#include "lib_math.h"

/* 奇异判据：|det(JᵀJ)| 低于此值视为退化 */
#define CHASSIS_DET_EPS 1e-9f

/*============================================
 *              内部工具
 *============================================*/

/**
 * @brief 求某轮"驱动方向向量" g = e_r - cot(β)·e_t
 * @param w     轮子几何
 * @param alpha 该轮当前轮面方向角 [rad]（固定轮=安装角，舵轮=当前舵角）
 * @param g     输出
 * @note 全向/舵轮 β=π/2 → cotβ=0 → g=e_r；麦轮 β=±π/4 → g=e_r ∓ e_t（模长 √2）
 */
static vector3_t WheelG(const LibChassisWheel_s *w, float alpha)
{
    float ca = Lib_Math_Cos(alpha);
    float sa = Lib_Math_Sin(alpha);
    vector3_t g;

    if (w->kind == LIB_CHASSIS_WHEEL_MECANUM)
    {
        /* e_r=(ca,sa)，e_t=(-sa,ca)；g = e_r - cot(β)·e_t */
        float cotb = Lib_Math_Cos(w->roller_angle) / Lib_Math_Sin(w->roller_angle);
        g.x = ca + cotb * sa;
        g.y = sa - cotb * ca;
    }
    else
    {
        g.x = ca;
        g.y = sa;
    }

    return g;
}

/**
 * @brief 逐轮组装 J 的行（全向/麦轮 1 行，舵轮 2 行）
 * @param inst 实例
 * @param st   反馈状态（可 NULL，仅无舵轮时）
 * @retval 0  成功
 * @retval -1 有舵轮但 st == NULL
 * @note 舵轮追加的侧向约束行来自"轮面不侧滑"：g^⊥·v_i = 0，g^⊥ = (-gy, gx)
 */
static int8_t BuildRows(LibChassisInstance_s *inst, const LibChassisState_s *st)
{
    const LibChassisConfig_s *cfg = &inst->cfg;
    uint8_t r = 0;

    for (uint8_t i = 0; i < cfg->num; i++)
    {
        const LibChassisWheel_s *w = &cfg->wheel[i];
        float alpha;

        if (w->kind == LIB_CHASSIS_WHEEL_RUDDER)
        {
            if (st == NULL)
                return -1;
            alpha = st->steer_angle[i];
        }
        else
        {
            alpha = w->mount_angle;
        }

        vector3_t g = WheelG(w, alpha);

        /* 驱动行：g·v_i = R·ω，即 [g_x, g_y, g_y·x - g_x·y] */
        inst->jacobian[r].x = g.x;
        inst->jacobian[r].y = g.y;
        inst->jacobian[r].z = g.y * w->x - g.x * w->y;
        inst->row_wheel[r] = i;
        inst->row_lateral[r] = 0;
        r++;

        /* 舵轮追加侧向约束行：g^⊥·v_i = 0，即 [-g_y, g_x, g_x·x + g_y·y] */
        if (w->kind == LIB_CHASSIS_WHEEL_RUDDER)
        {
            inst->jacobian[r].x = -g.y;
            inst->jacobian[r].y = g.x;
            inst->jacobian[r].z = g.x * w->x + g.y * w->y;
            inst->row_wheel[r] = i;
            inst->row_lateral[r] = 1;
            r++;
        }
    }

    inst->row_num = r;
    return 0;
}

/**
 * @brief 组装 J 并求 (JᵀJ)⁻¹，结果留在 inst 内（正解/力分配共用）
 * @retval 0  成功（inst->valid = 1）
 * @retval -1 有舵轮但 st == NULL / JᵀJ 奇异
 */
static int8_t Update(LibChassisInstance_s *inst, const LibChassisState_s *st)
{
    inst->valid = 0;

    if (BuildRows(inst, st) != 0)
        return -1;

    /* A = JᵀJ（3×3 row-major） */
    matrix3_t a = {0};
    for (uint8_t i = 0; i < inst->row_num; i++)
    {
        float e[3] = {inst->jacobian[i].x, inst->jacobian[i].y, inst->jacobian[i].z};
        for (uint8_t r = 0; r < 3; r++)
            for (uint8_t c = 0; c < 3; c++)
                a.data[r][c] += e[r] * e[c];
    }

    if (FABS(Lib_Math_Mat3Determinant(a)) < CHASSIS_DET_EPS)
        return -1;

    inst->jtj_inv = Lib_Math_Mat3Inverse(a);
    inst->valid = 1;
    return 0;
}

/**
 * @brief 行值 = 行 · 向量：out[r] = J_r · v
 * @note 目前只有力分配直接调用（v = q = (JᵀJ)⁻¹·W）。逆解不走这里：它算的
 *       `g·v_i` 在固定轮上等价于"驱动行 · ξ"，但舵轮是由 v_i 直接定舵角/轮速。
 */
static void RowsDot(const LibChassisInstance_s *inst, vector3_t v, float *out)
{
    for (uint8_t i = 0; i < inst->row_num; i++)
        out[i] = Lib_Math_Vec3Dot(inst->jacobian[i], v);
}

/*============================================
 *              外部接口
 *============================================*/
int8_t LibChassisInit(LibChassisInstance_s *inst, const LibChassisConfig_s *cfg)
{
    if (inst == NULL || cfg == NULL)
        return -1;
    if (cfg->num == 0 || cfg->num > LIB_CHASSIS_MAX_WHEEL)
        return -1;

    for (uint8_t i = 0; i < cfg->num; i++)
    {
        if (cfg->wheel[i].kind > LIB_CHASSIS_WHEEL_RUDDER)
            return -3;
        if (cfg->wheel[i].radius <= 0.0f)
            return -2;
    }

    inst->cfg = *cfg;
    inst->valid = 0;

    /* 无舵轮时 J 是常量，这里顺带建好；有舵轮时 J 随舵角变，留给每次解算重建 */
    (void)Update(inst, NULL);
    return 0;
}

void LibChassisInverse(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t twist, LibChassisRef_s *out)
{
    if (inst == NULL || out == NULL)
        return;

    for (uint8_t i = 0; i < LIB_CHASSIS_MAX_WHEEL; i++)
    {
        out->steer_target[i] = 0.0f;
        out->wheel_speed[i] = 0.0f;
    }

    /* 舵轮取 state 舵角；无舵轮时 st 可为 NULL。逐轮直接解，不求 JᵀJ
     * （逆解不需要，且退化时仍应给出分量）。 */
    const LibChassisConfig_s *cfg = &inst->cfg;

    for (uint8_t i = 0; i < cfg->num; i++)
    {
        const LibChassisWheel_s *w = &cfg->wheel[i];

        /* 组 i 回转中心的期望速度：v_i = v_center + w × r_i */
        float vix = twist.x - twist.z * w->y;
        float viy = twist.y + twist.z * w->x;

        if (w->kind == LIB_CHASSIS_WHEEL_RUDDER)
        {
            float speed = Lib_Math_Sqrt(vix * vix + viy * viy);
            float phi_cur = (st != NULL) ? st->steer_angle[i] : 0.0f;

            if (speed < LIB_CHASSIS_STEER_SPEED_EPS)
            {
                /* 静止：不改朝向，驱动停转 */
                out->steer_target[i] = phi_cur;
                out->wheel_speed[i] = 0.0f;
                continue;
            }

            int8_t reverse = 0;
            float phi_des = Lib_Math_Atan2(viy, vix);
            out->steer_target[i] = LibChassisSteerNearest180(phi_des, phi_cur, &reverse);
            out->wheel_speed[i] = (reverse ? -speed : speed) / w->radius;
        }
        else
        {
            vector3_t g = WheelG(w, w->mount_angle);

            out->steer_target[i] = w->mount_angle;
            out->wheel_speed[i] = (g.x * vix + g.y * viy) / w->radius;
        }
    }
}

int8_t LibChassisForward(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t *twist)
{
    if (inst == NULL || st == NULL || twist == NULL)
        return -1;

    twist->x = 0.0f;
    twist->y = 0.0f;
    twist->z = 0.0f;

    if (Update(inst, st) != 0)
        return -1;

    /* b = Σ 驱动行 J_r·(R_i·ω_i)（侧向行右端为 0，不参与）；ξ = (JᵀJ)⁻¹·b */
    vector3_t b = {0};
    for (uint8_t i = 0; i < inst->row_num; i++)
    {
        if (inst->row_lateral[i] != 0)
            continue;

        uint8_t wi = inst->row_wheel[i];
        float s = st->wheel_speed[wi] * inst->cfg.wheel[wi].radius;
        b = Lib_Math_Vec3Add(b, Lib_Math_Vec3Scale(inst->jacobian[i], s));
    }

    *twist = Lib_Math_Mat3MulVec3(inst->jtj_inv, b);
    return 0;
}

int8_t LibChassisAllocateTorque(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t wrench,
                                LibChassisTorque_s *out)
{
    if (inst == NULL || out == NULL)
        return -1;

    for (uint8_t i = 0; i < LIB_CHASSIS_MAX_WHEEL; i++)
    {
        out->wheel_torque[i] = 0.0f;
        out->lateral_force[i] = 0.0f;
    }

    if (Update(inst, st) != 0)
        return -1;

    /* q = (JᵀJ)⁻¹·W；各行力 f_r = J_r·q；驱动行 τ_i = R_i·f_i，侧向行即侧向接触力 */
    vector3_t q = Lib_Math_Mat3MulVec3(inst->jtj_inv, wrench);
    float f[LIB_CHASSIS_MAX_ROW];
    RowsDot(inst, q, f);

    for (uint8_t i = 0; i < inst->row_num; i++)
    {
        uint8_t wi = inst->row_wheel[i];

        if (inst->row_lateral[i] != 0)
            out->lateral_force[wi] = f[i];
        else
            out->wheel_torque[wi] = f[i] * inst->cfg.wheel[wi].radius;
    }

    return 0;
}

float LibChassisSteerNearest180(float phi_des, float phi_cur, int8_t *reverse)
{
    float e = Lib_Math_WrapAngleNegPIToPI(phi_des - phi_cur); // (-π, π]
    int8_t rev = 0;

    if (FABS(e) > M_PI_2)
    {
        e -= (e > 0.0f) ? M_PI : -M_PI;
        rev = 1;
    }

    if (reverse != NULL)
        *reverse = rev;

    return phi_cur + e;
}
