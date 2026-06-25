/**
 * @file drv_mahony.c
 * @brief Mahony 姿态解算滤波器实现
 *
 * @note 基于 Mahony 互补滤波算法
 * @note 参考：Mahony et al. "Nonlinear Complementary Filters on
 *       the Special Orthogonal Group" IEEE TAC 2008
 * @note 使用 bsp_math 类型封装：quaternion_t / euler_t / vector3_t
 *
 * 算法流程：
 * 1. 归一化加速度计（和磁力计）测量值
 * 2. 从四元数预估重力/磁场方向（机体坐标系）
 * 3. 测量值与预估值的叉积 = 姿态误差
 * 4. PI 控制器用误差修正陀螺仪数据
 * 5. 一阶龙格-库塔积分更新四元数
 * 6. 四元数归一化
 */

#include "drv_mahony.h"
#include "bsp_dwt.h"

/*============================ 默认参数 ============================*/

#define MAHONY_DT_MIN (1e-7f)             /* 最小 dt (s)                  */
#define MAHONY_ACC_NORM_TARGET (9.80665f) /* 重力加速度 (m/s²)           */
#define MAHONY_ACC_NORM_TOLERANCE (0.3f)  /* 加速度模长容差比例（加速度值在 0.7g ~ 1.3g 之间才用加速度计校正）           */

/*============================ 私有函数声明 ============================*/

/**
 * @brief Mahony 滤波器内部通用更新
 * @param inst    实例指针
 * @param gyro    陀螺仪数据 (rad/s)
 * @param acc     加速度计数据 (m/s²)
 * @param mag     磁力计数据（未使用时传 {0,0,0}）
 * @param use_mag 是否融合磁力计
 */
static void Mahony_UpdateInternal(MahonyInstance *inst, vector3_t gyro, vector3_t acc, vector3_t mag, uint8_t use_mag);

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
    inst->quat = BSP_Math_QuatIdentity();

    /* 清零积分误差 */
    inst->integral_fb.x = 0.0f;
    inst->integral_fb.y = 0.0f;
    inst->integral_fb.z = 0.0f;

    inst->last_time_us = DWT_GetTimeUs();
}

void MahonyUpdate(MahonyInstance *inst, vector3_t gyro, vector3_t acc)
{
    if (inst == NULL)
    {
        return;
    }

    vector3_t mag = {0.0f, 0.0f, 0.0f};
    Mahony_UpdateInternal(inst, gyro, acc, mag, 0);
}

void MahonyUpdateMag(MahonyInstance *inst, vector3_t gyro, vector3_t acc,
                     vector3_t mag)
{
    if (inst == NULL)
    {
        return;
    }

    Mahony_UpdateInternal(inst, gyro, acc, mag, 1);
}

void MahonyReset(MahonyInstance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    inst->quat = BSP_Math_QuatIdentity();

    inst->integral_fb.x = 0.0f;
    inst->integral_fb.y = 0.0f;
    inst->integral_fb.z = 0.0f;

    inst->last_time_us = DWT_GetTimeUs();
}

/*============================ 私有函数实现 ============================*/

static void Mahony_UpdateInternal(MahonyInstance *inst, vector3_t gyro, vector3_t acc, vector3_t mag, uint8_t use_mag)
{
    float recip_norm;
    float q0, q1, q2, q3;
    float q0q0, q0q1, q0q2, q0q3;
    float q1q1, q1q2, q1q3;
    float q2q2, q2q3, q3q3;
    float hx, hy, bx, bz;
    float halfvx, halfvy, halfvz;
    float halfwx, halfwy, halfwz;
    float halfex, halfey, halfez;
    float qa, qb, qc;
    uint64_t now_us;
    float dt;

    /* ======================== 1. 计算 dt ======================== */

    now_us = DWT_GetTimeUs();
    dt = (float)(now_us - inst->last_time_us) * 1e-6f;
    inst->last_time_us = now_us;

    if (dt < MAHONY_DT_MIN)
    {
        return;
    }

    /* ======================== 2. 预计算四元数乘积项 ======================== */

    q0 = inst->quat.w;
    q1 = inst->quat.x;
    q2 = inst->quat.y;
    q3 = inst->quat.z;

    q0q0 = q0 * q0;
    q0q1 = q0 * q1;
    q0q2 = q0 * q2;
    q0q3 = q0 * q3;
    q1q1 = q1 * q1;
    q1q2 = q1 * q2;
    q1q3 = q1 * q3;
    q2q2 = q2 * q2;
    q2q3 = q2 * q3;
    q3q3 = q3 * q3;

    /* ======================== 3. 归一化加速度计 ======================== */

    float acc_norm = BSP_Math_Vec3Length(acc);

    /* 检查合加速度是否接近 1g，跳过自由落体/剧烈运动时的校正 */
    if (BSP_Math_Fabs(acc_norm - MAHONY_ACC_NORM_TARGET) <
        MAHONY_ACC_NORM_TARGET * MAHONY_ACC_NORM_TOLERANCE)
    {
        recip_norm = 1.0f / acc_norm;
        acc = BSP_Math_Vec3Scale(acc, recip_norm);

        /* 预估重力方向在机体坐标系中的分量 */
        halfvx = q1q3 - q0q2;
        halfvy = q0q1 + q2q3;
        halfvz = q0q0 - q1q1 - q2q2 + q3q3;

        /* 加速度计误差 = 测量值 × 预估值（叉积） */
        halfex = acc.y * halfvz - acc.z * halfvy;
        halfey = acc.z * halfvx - acc.x * halfvz;
        halfez = acc.x * halfvy - acc.y * halfvx;
    }
    else
    {
        halfex = 0.0f;
        halfey = 0.0f;
        halfez = 0.0f;
    }

    /* ======================== 4. 磁力计校正 ======================== */

    if (use_mag)
    {
        /* 归一化磁力计 */
        float mag_len = BSP_Math_Vec3Length(mag);
        if (mag_len < 1e-3f)
        {
            return;
        }
        mag = BSP_Math_Vec3Scale(mag, 1.0f / mag_len);

        /* 旋转磁力计到世界坐标系，得到水平磁场分量 */
        hx = (q0q0 + q1q1 - q2q2 - q3q3) * mag.x + 2.0f * (q1q2 - q0q3) * mag.y + 2.0f * (q1q3 + q0q2) * mag.z;
        hy = 2.0f * (q1q2 + q0q3) * mag.x + (q0q0 - q1q1 + q2q2 - q3q3) * mag.y + 2.0f * (q2q3 - q0q1) * mag.z;

        /* 参考磁场方向：水平分量指向北，垂直分量指向地 */
        bx = BSP_Math_Sqrt(hx * hx + hy * hy);
        bz = 2.0f * (q1q3 - q0q2) * mag.x + 2.0f * (q2q3 + q0q1) * mag.y + (q0q0 - q1q1 - q2q2 + q3q3) * mag.z;

        /* 预估磁场方向在机体坐标系中的分量 */
        halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);
        halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);
        halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);

        /* 累加磁力计误差（叉积） */
        halfex += mag.y * halfwz - mag.z * halfwy;
        halfey += mag.z * halfwx - mag.x * halfwz;
        halfez += mag.x * halfwy - mag.y * halfwx;
    }

    /* ======================== 5. PI 控制器修正角速度 ======================== */

    if (inst->ki > 0.0f)
    {
        /* 积分项累积 */
        inst->integral_fb.x += inst->ki * halfex * dt;
        inst->integral_fb.y += inst->ki * halfey * dt;
        inst->integral_fb.z += inst->ki * halfez * dt;

        /* 比例 + 积分修正角速度 */
        gyro.x += inst->kp * halfex + inst->integral_fb.x;
        gyro.y += inst->kp * halfey + inst->integral_fb.y;
        gyro.z += inst->kp * halfez + inst->integral_fb.z;
    }
    else
    {
        /* 仅比例修正 */
        gyro.x += inst->kp * halfex;
        gyro.y += inst->kp * halfey;
        gyro.z += inst->kp * halfez;
    }

    /* ======================== 6. 四元数更新（一阶龙格-库塔）================ */

    qa = q0;
    qb = q1;
    qc = q2;

    q0 += (-qb * gyro.x - qc * gyro.y - q3 * gyro.z) * (0.5f * dt);
    q1 += (qa * gyro.x + qc * gyro.z - q3 * gyro.y) * (0.5f * dt);
    q2 += (qa * gyro.y - qb * gyro.z + q3 * gyro.x) * (0.5f * dt);
    q3 += (qa * gyro.z + qb * gyro.y - qc * gyro.x) * (0.5f * dt);

    /* ======================== 7. 四元数归一化 ======================== */

    quaternion_t q_new = {q0, q1, q2, q3};
    inst->quat = BSP_Math_QuatNormalize(q_new);
}
