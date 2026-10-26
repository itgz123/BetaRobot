/**
 * @file lib_imu_transform.c
 * @brief IMU 系 → 目标系变换 + 加速度计杆臂补偿实现
 *
 * 数学：
 *   omega_T = R · omega_I
 *   alpha   = LPF(clamp(d(omega_T)/dt))
 *   acc_T   = R · acc_I - alpha × r - omega_T × (omega_T × r)
 * 其中 R 的列 = IMU x/y/z 轴在目标系的方向，r = p_IMU - p_O。
 *
 * 顺序约束：先旋转再差分/限幅/低通。无逐轴限幅时与“先差分再旋转”代数等价，
 * 有限幅时不等价，本实现与底盘反馈蓝本保持一致。
 */

#ifndef LIB_IMU_TRANSFORM_STANDALONE
#include "app_cfg.h"
#endif

#include "lib_imu_transform.h"

#ifdef LIB_IMU_TRANSFORM_USED

#include <math.h>

#define IMU_TRANSFORM_EPS 1.0e-6f     /* 轴长度下限，低于此视为零向量 */
#define IMU_TRANSFORM_ROT_TOL 1.0e-3f /* 正交性 / 行列式校验容差 */

static uint8_t ImuTransform_VecFinite(vector3_t v)
{
    return (uint8_t)((isfinite(v.x) != 0) && (isfinite(v.y) != 0) && (isfinite(v.z) != 0));
}

static uint8_t ImuTransform_CfgFinite(const LibImuTransform_Config_s *cfg)
{
    return (uint8_t)((ImuTransform_VecFinite(cfg->pos) != 0u) && (ImuTransform_VecFinite(cfg->x_axis) != 0u) && (ImuTransform_VecFinite(cfg->y_axis) != 0u) &&
                     (ImuTransform_VecFinite(cfg->z_axis) != 0u) && (isfinite(cfg->alpha_lpf_rc) != 0) && (isfinite(cfg->alpha_limit) != 0));
}

/**
 * @brief 由三个轴方向构造 R_{T<-I}
 * @return 0 成功；-1 配置非法（轴退化 / 不正交 / 反射）
 * @note 三轴全零 → 单位阵（仓库“全 0 = 不修正”惯例）；否则各轴归一化后严格校验。
 */
static int8_t ImuTransform_BuildRot(const LibImuTransform_Config_s *cfg, matrix3_t *rot)
{
    const float lx = Lib_Math_Vec3Length(cfg->x_axis);
    const float ly = Lib_Math_Vec3Length(cfg->y_axis);
    const float lz = Lib_Math_Vec3Length(cfg->z_axis);

    if ((lx <= IMU_TRANSFORM_EPS) && (ly <= IMU_TRANSFORM_EPS) && (lz <= IMU_TRANSFORM_EPS))
    {
        *rot = Lib_Math_Mat3Identity();
        return 0;
    }
    if ((lx <= IMU_TRANSFORM_EPS) || (ly <= IMU_TRANSFORM_EPS) || (lz <= IMU_TRANSFORM_EPS))
    {
        return -1;
    }

    const vector3_t ax = Lib_Math_Vec3Scale(cfg->x_axis, 1.0f / lx);
    const vector3_t ay = Lib_Math_Vec3Scale(cfg->y_axis, 1.0f / ly);
    const vector3_t az = Lib_Math_Vec3Scale(cfg->z_axis, 1.0f / lz);

    if ((Lib_Math_Fabs(Lib_Math_Vec3Dot(ax, ay)) > IMU_TRANSFORM_ROT_TOL) || (Lib_Math_Fabs(Lib_Math_Vec3Dot(ax, az)) > IMU_TRANSFORM_ROT_TOL) ||
        (Lib_Math_Fabs(Lib_Math_Vec3Dot(ay, az)) > IMU_TRANSFORM_ROT_TOL))
    {
        return -1;
    }

    /* 列 = IMU 各轴在目标系的方向；与现有 4x4 安装矩阵的左上 3x3 同构 */
    rot->data[0][0] = ax.x;
    rot->data[1][0] = ax.y;
    rot->data[2][0] = ax.z;
    rot->data[0][1] = ay.x;
    rot->data[1][1] = ay.y;
    rot->data[2][1] = ay.z;
    rot->data[0][2] = az.x;
    rot->data[1][2] = az.y;
    rot->data[2][2] = az.z;

    /* 右手性：反射（det = -1）会同时反转叉乘方向，必须拒绝 */
    if (Lib_Math_Fabs(Lib_Math_Mat3Determinant(*rot) - 1.0f) > IMU_TRANSFORM_ROT_TOL)
    {
        return -1;
    }
    return 0;
}

static void ImuTransform_ZeroOutput(LibImuTransform_Output_s *out)
{
    *out = (LibImuTransform_Output_s){0};
}

LibImuTransform_Status_e LibImuTransformInit(LibImuTransform_Instance_s *inst, const LibImuTransform_Config_s *cfg)
{
    if ((inst == NULL) || (cfg == NULL))
    {
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }
    if (ImuTransform_CfgFinite(cfg) == 0u)
    {
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }
    if ((cfg->alpha_lpf_rc < 0.0f) || (cfg->alpha_limit < 0.0f))
    {
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }

    matrix3_t rot;
    if (ImuTransform_BuildRot(cfg, &rot) != 0)
    {
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }

    /* 校验全部通过后才写实例：Init 失败不改动原状态 */
    inst->cfg = *cfg;
    inst->rot = rot;
    inst->gyro_prev = (vector3_t){0.0f, 0.0f, 0.0f};
    inst->alpha_lpf = (vector3_t){0.0f, 0.0f, 0.0f};
    inst->gyro_prev_valid = 0u;
    inst->alpha_valid = 0u;
    inst->inited = 1u;
    return LIB_IMU_TRANSFORM_OK;
}

void LibImuTransformReset(LibImuTransform_Instance_s *inst)
{
    if (inst == NULL)
    {
        return;
    }
    inst->gyro_prev = (vector3_t){0.0f, 0.0f, 0.0f};
    inst->alpha_lpf = (vector3_t){0.0f, 0.0f, 0.0f};
    inst->gyro_prev_valid = 0u;
    inst->alpha_valid = 0u;
}

LibImuTransform_Status_e LibImuTransformUpdate(LibImuTransform_Instance_s *inst, const LibImuTransform_Input_s *in, LibImuTransform_Output_s *out)
{
    if ((inst == NULL) || (in == NULL) || (out == NULL))
    {
        if (out != NULL)
        {
            ImuTransform_ZeroOutput(out);
        }
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }
    if (inst->inited == 0u)
    {
        ImuTransform_ZeroOutput(out);
        return LIB_IMU_TRANSFORM_ERR_NOT_INIT;
    }
    if ((ImuTransform_VecFinite(in->gyro) == 0u) || (ImuTransform_VecFinite(in->acc) == 0u) || (isfinite(in->dt) == 0))
    {
        ImuTransform_ZeroOutput(out);
        return LIB_IMU_TRANSFORM_ERR_NOT_FINITE;
    }
    if (in->dt < 0.0f)
    {
        ImuTransform_ZeroOutput(out);
        return LIB_IMU_TRANSFORM_ERR_ARG;
    }

    /* 先旋转到目标系 */
    const vector3_t gyro_t = Lib_Math_Mat3MulVec3(inst->rot, in->gyro);
    const vector3_t acc_t = Lib_Math_Mat3MulVec3(inst->rot, in->acc);
    if ((ImuTransform_VecFinite(gyro_t) == 0u) || (ImuTransform_VecFinite(acc_t) == 0u))
    {
        ImuTransform_ZeroOutput(out);
        return LIB_IMU_TRANSFORM_ERR_NOT_FINITE;
    }

    /* 角加速度：dt > 0 才更新；首拍只播种 gyro_prev，alpha 保持 0 */
    if (in->dt > 0.0f)
    {
        if (inst->gyro_prev_valid != 0u)
        {
            vector3_t alpha_raw = Lib_Math_Vec3Scale(Lib_Math_Vec3Sub(gyro_t, inst->gyro_prev), 1.0f / in->dt);
            if (ImuTransform_VecFinite(alpha_raw) == 0u)
            {
                ImuTransform_ZeroOutput(out);
                return LIB_IMU_TRANSFORM_ERR_NOT_FINITE;
            }
            if (inst->cfg.alpha_limit > 0.0f)
            {
                alpha_raw.x = Lib_Math_Clamp(alpha_raw.x, -inst->cfg.alpha_limit, inst->cfg.alpha_limit);
                alpha_raw.y = Lib_Math_Clamp(alpha_raw.y, -inst->cfg.alpha_limit, inst->cfg.alpha_limit);
                alpha_raw.z = Lib_Math_Clamp(alpha_raw.z, -inst->cfg.alpha_limit, inst->cfg.alpha_limit);
            }

            const float k = in->dt / (inst->cfg.alpha_lpf_rc + in->dt);
            inst->alpha_lpf.x += (alpha_raw.x - inst->alpha_lpf.x) * k;
            inst->alpha_lpf.y += (alpha_raw.y - inst->alpha_lpf.y) * k;
            inst->alpha_lpf.z += (alpha_raw.z - inst->alpha_lpf.z) * k;
            inst->alpha_valid = 1u;
        }
        inst->gyro_prev = gyro_t;
        inst->gyro_prev_valid = 1u;
    }

    /* 杆臂补偿：a_O = a_I - alpha×r - omega×(omega×r) */
    const vector3_t centripetal = Lib_Math_Vec3Cross(gyro_t, Lib_Math_Vec3Cross(gyro_t, inst->cfg.pos));
    const vector3_t tangential = Lib_Math_Vec3Cross(inst->alpha_lpf, inst->cfg.pos);

    out->gyro = gyro_t;
    out->acc = Lib_Math_Vec3Sub(Lib_Math_Vec3Sub(acc_t, tangential), centripetal);
    out->alpha = inst->alpha_lpf;
    out->pos = inst->cfg.pos;
    out->valid = (uint8_t)((in->valid != 0u) ? 1u : 0u);
    out->fresh = (uint8_t)(((out->valid != 0u) && (in->dt > 0.0f)) ? 1u : 0u);
    out->alpha_valid = inst->alpha_valid;
    return LIB_IMU_TRANSFORM_OK;
}

#endif /* LIB_IMU_TRANSFORM_USED */
