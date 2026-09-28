/**
 * @file drvlib_bmi088_ist8310_eskf.c
 * @brief BMI088 + IST8310 误差状态卡尔曼姿态融合（实现）
 *
 * @note 坐标系约定、量测模型、可观测性、退化路径、VOFA 通道表全在本模块 .h 的
 *       头注释里，本文件只记实现层面的取舍（见各函数内注释）
 */

#include "drvlib_bmi088_ist8310_eskf.h"

#if defined(DRVLIB_BMI088_IST8310_ESKF_USED) && defined(DRV_BMI088_USED) && defined(DRV_IST8310_USED) &&                 \
    defined(LIB_ESKF_USED)

#include <math.h> /* NAN */
#include "bsp_log.h"
#include "drv_vofa.h" /* VOFA_USED / VOFA_UART 未定义时全部是空实现 */

/*============================ 日志实例 ============================*/

#ifndef DRVLIB_BMI088_IST8310_ESKF_LOG_LIMIT
#define DRVLIB_BMI088_IST8310_ESKF_LOG_LIMIT 10
#endif // !DRVLIB_BMI088_IST8310_ESKF_LOG_LIMIT
LOG_INSTANCE_DEF(g_bmi088_ist8310_eskf_log, "bmi088_ist8310_eskf", DRVLIB_BMI088_IST8310_ESKF_LOG_LIMIT);

/*============================ 内部常量 ============================*/

/* 连续多少次 Update 没等到新磁帧就判链路已死（mag_valid=0）。
 * 2ms 任务 × 100 = 200ms：磁计 50Hz 时正常绝不会连续错这么多帧 */
#ifndef BMI088_IST8310_ESKF_MAG_MISS_LIMIT
#define BMI088_IST8310_ESKF_MAG_MISS_LIMIT 100
#endif // !BMI088_IST8310_ESKF_MAG_MISS_LIMIT

/* 归一化方向的下限保护：模长小于它就没法归一化（视为无有效量测） */
#ifndef BMI088_IST8310_ESKF_DIR_MIN
#define BMI088_IST8310_ESKF_DIR_MIN 1e-3f
#endif // !BMI088_IST8310_ESKF_DIR_MIN

/*============================ 小工具 ============================*/

/** @brief 世界系向量 → 机体系（= R(q)ᵀ·v = q⁻¹ ⊗ v ⊗ q） */
static void WorldToBody(const quaternion_t *q, const float v_w[3], float v_b[3])
{
    vector3_t v = {v_w[0], v_w[1], v_w[2]};
    vector3_t r = Lib_Math_QuatRotateVector(Lib_Math_QuatConjugate(*q), v);
    v_b[0] = r.x;
    v_b[1] = r.y;
    v_b[2] = r.z;
}

/** @brief 归一化方向：返回模长；模长过小返回 0（调用方据此放弃本次量测） */
static float NormalizeDir(const float v[3], float out[3])
{
    float n = Lib_Math_Sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n < BMI088_IST8310_ESKF_DIR_MIN)
        return 0.0f;
    float k = 1.0f / n;
    out[0] = v[0] * k;
    out[1] = v[1] * k;
    out[2] = v[2] * k;
    return n;
}

/*============================ ESKF 回调（ctx = 融合实例） ============================*/

/**
 * @brief 传播名义姿态并给出误差态 F/Q
 *
 * @note 名义传播用**精确指数**：q ← normalize(q ⊗ [cos(θ/2), sin(θ/2)·ω/|ω|])。
 *       这保证"常值角速率、无噪声"的理想情形下名义姿态与真值逐位一致
 *       （lib_eskf 用例②就是靠这一点锁死乘向的：若改成 F·δθ 的一阶近似，
 *        那个用例会稳定残留 ω·dt 量级的误差）。
 * @note F 取一阶 F = I₆ + F_c·dt（F_c = [[-[ω]×,-I₃],[0,0]]），与 lib_eskf 文档一致；
 *       Q 对角：姿态块 q_att·dt、零偏块 q_bias·dt。
 */
static void EskfPropagate(void *ctx, const float *u, float dt, float *F, float *Q)
{
    BMI088IST8310EskfInstance *inst = (BMI088IST8310EskfInstance *)ctx;
    (void)u; /* l = 0：陀螺样本经 inst->gyro_cal 传递，不走控制输入通道 */

    /* ω = 陀螺标定值 - 名义零偏（残余） */
    float w[3];
    for (int i = 0; i < 3; i++)
    {
        w[i] = inst->gyro_cal[i] - inst->bias_nom[i];
    }

    /* 名义四元数：Δq = [cos(θ/2), sin(θ/2)/|ω| · ω]；|ω|→0 时取极限 [1, ω·dt/2] */
    const float wn = Lib_Math_Sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    float cw, k;
    if (wn > 1e-6f)
    {
        float sw;
        Lib_Math_SinCos(wn * dt * 0.5f, &sw, &cw);
        k = sw / wn; /* sin(θ/2)/|ω| */
    }
    else
    {
        cw = 1.0f;
        k = dt * 0.5f;
    }
    quaternion_t dq = {cw, w[0] * k, w[1] * k, w[2] * k};
    inst->q_nom = Lib_Math_QuatNormalize(Lib_Math_QuatMul(inst->q_nom, dq));

    /* F = I₆ + [[-[ω]×, -I₃],[0₃, 0₃]]·dt */
    const float wx = w[0], wy = w[1], wz = w[2];
    for (int i = 0; i < 36; i++)
    {
        F[i] = 0.0f;
        Q[i] = 0.0f;
    }
    F[0 * 6 + 0] = 1.0f;
    F[0 * 6 + 1] = wz * dt;
    F[0 * 6 + 2] = -wy * dt;
    F[0 * 6 + 3] = -dt;
    F[1 * 6 + 0] = -wz * dt;
    F[1 * 6 + 1] = 1.0f;
    F[1 * 6 + 2] = wx * dt;
    F[1 * 6 + 4] = -dt;
    F[2 * 6 + 0] = wy * dt;
    F[2 * 6 + 1] = -wx * dt;
    F[2 * 6 + 2] = 1.0f;
    F[2 * 6 + 5] = -dt;
    F[3 * 6 + 3] = 1.0f;
    F[4 * 6 + 4] = 1.0f;
    F[5 * 6 + 5] = 1.0f;

    const float qa = inst->q_att * dt;
    const float qb = inst->q_bias * dt;
    Q[0 * 6 + 0] = qa;
    Q[1 * 6 + 1] = qa;
    Q[2 * 6 + 2] = qa;
    Q[3 * 6 + 3] = qb;
    Q[4 * 6 + 4] = qb;
    Q[5 * 6 + 5] = qb;
}

/**
 * @brief 方向量测的公共实现：y = z - R(q)ᵀ·ref，H = [[h]×, 0₃]，R = r·I₃
 * @param ref 世界系单位参考方向（ĝ_w 或 m_w）
 * @param r 本帧量测噪声方差 (rad²)
 * @param y/H/R lib_eskf 给的出参（长度 m_now=3、H 行距 n=6、R 紧凑行距 3）
 * @param res 输出：残差模长（注入前算，供 VOFA 观察坏帧）
 *
 * @note 残差必须写成 z - h(名义)：ESKF 的 δx 定义是 x_true = x_nom ⊞ δx，
 *       故 H·δx = h(x_true) - h(x_nom)，y = z - h(x_nom) = H·δx + v。
 *       写反（h - z）会让每次更新都把姿态推离真值 —— lib_eskf 用例⑥锁死这一点。
 */
static void MeasureDir(const quaternion_t *q_nom, const float *z, const float ref[3], float r, float *y, float *H,
                       float *R, float *res)
{
    float h[3];
    WorldToBody(q_nom, ref, h);

    float sum = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        y[i] = z[i] - h[i];
        sum += y[i] * y[i];
        /* H = [[h]×, 0₃]：δb 三列恒 0（方向量测不含零偏信息） */
        for (int j = 0; j < 6; j++)
        {
            H[i * 6 + j] = 0.0f;
        }
        H[i * 6 + (i + 1) % 3] = -h[(i + 2) % 3];
        H[i * 6 + (i + 2) % 3] = h[(i + 1) % 3];
    }
    *res = Lib_Math_Sqrt(sum);

    /* R 紧凑（行距 m_now = 3），各向同性 */
    for (int i = 0; i < 9; i++)
    {
        R[i] = 0.0f;
    }
    R[0] = r;
    R[4] = r;
    R[8] = r;
}

/** @brief 加速度计量测：参考重力 ĝ_w = [0,0,1] */
static void EskfMeasureAcc(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
{
    BMI088IST8310EskfInstance *inst = (BMI088IST8310EskfInstance *)ctx;
    (void)m_now; /* 恒为 3 */
    MeasureDir(&inst->q_nom, z, inst->g_w, inst->r_acc_now, y, H, R, &inst->acc_resid);
}

/** @brief 磁力计量测：参考世界系单位磁矢量 m_w */
static void EskfMeasureMag(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
{
    BMI088IST8310EskfInstance *inst = (BMI088IST8310EskfInstance *)ctx;
    (void)m_now;
    MeasureDir(&inst->q_nom, z, inst->m_w, inst->r_mag_now, y, H, R, &inst->mag_resid);
}

/**
 * @brief 注入：δq = normalize([1, δθ/2])，q_nom ← normalize(q_nom ⊗ δq)；b_nom += δb
 * @note δq 的**右乘**必须与 MeasureDir 的 H 符号、Propagate 的 F 一致（lib_eskf 用例⑥锁死）
 * @note 取名 InjectDelta 而非 Inject：lib_eskf 的公开 API 里有同名的 `EskfInject(EskfInstance*)`，
 *       重名会被它的声明当成重复定义（static 与外部链接同名仍是同一个标识符）
 */
static void EskfInjectDelta(void *ctx, const float *delta)
{
    BMI088IST8310EskfInstance *inst = (BMI088IST8310EskfInstance *)ctx;

    quaternion_t dq = {1.0f, 0.5f * delta[0], 0.5f * delta[1], 0.5f * delta[2]};
    dq = Lib_Math_QuatNormalize(dq);
    inst->q_nom = Lib_Math_QuatNormalize(Lib_Math_QuatMul(inst->q_nom, dq));

    for (int i = 0; i < 3; i++)
    {
        inst->bias_nom[i] += delta[3 + i];
    }
}

/*============================ 上电播种 ============================*/

/**
 * @brief 用 acc + mag 定初始姿态（TRIAD），写入 inst->q_nom
 * @param inst 融合实例
 * @param a_b 机体系重力方向（未归一化，+Z 朝上）
 * @param m_b 机体系磁矢量（未归一化）；NULL = 无磁，yaw 以播种时刻为 0
 * @return 1 = 已写入 q_nom
 *
 * @note TRIAD：构造两组正交基，机体 `[â, t2_b, â×t2_b]` 对世界 `[ĝ, t2_w, ĝ×t2_w]`，
 *       则 R = W·Bᵀ。两基的**轴符号**由 lib_eskf 用例⑥的同一套约定锁定
 *       （ĝ_w=[0,0,1]、m_w 由倾角/偏角推、q: 机体系→世界系）。
 * @note 无磁时退化为"acc 定 roll/pitch + yaw=0"（与 drvlib_bmi088_kalman 播种逐位一致），
 *       之后磁计有效时 ESKF 会把 yaw 拉向磁北。
 */
static uint8_t AttitudeSeed(BMI088IST8310EskfInstance *inst, const float a_b[3], const float m_b[3])
{
    float a[3], m[3];
    if (NormalizeDir(a_b, a) == 0.0f)
        return 0;

    /* --- 无磁：acc 只定 roll/pitch，yaw 以播种时刻的朝向为基准 0 ---
     * 与 drvlib_bmi088_kalman 的播种逐位一致（同一套反算式），故两个模块的初始
     * 姿态在同一安装下相同；之后磁计有效时 ESKF 会把 yaw 拉向磁北 */
    if (m_b == NULL || NormalizeDir(m_b, m) == 0.0f)
    {
        euler_t e;
        e.roll = Lib_Math_Atan2(a[1], a[2]);
        e.pitch = Lib_Math_Atan2(-a[0], Lib_Math_Sqrt(a[1] * a[1] + a[2] * a[2]));
        e.yaw = 0.0f;
        inst->q_nom = Lib_Math_EulerToQuat(e);
        inst->bias_nom[0] = inst->bias_nom[1] = inst->bias_nom[2] = 0.0f;
        return 1;
    }

    /* --- 机体基 --- */
    float b2[3] = {a[1] * m[2] - a[2] * m[1], a[2] * m[0] - a[0] * m[2], a[0] * m[1] - a[1] * m[0]};
    float b2n[3];
    if (NormalizeDir(b2, b2n) == 0.0f)
        return 0; /* 磁与重力共线：航向不可观测，退回无磁播种由调用方处理 */
    float b3[3] = {a[1] * b2n[2] - a[2] * b2n[1], a[2] * b2n[0] - a[0] * b2n[2], a[0] * b2n[1] - a[1] * b2n[0]};

    /* --- 世界基 --- */
    const float *g = inst->g_w;
    const float *mw = inst->m_w;
    float w2[3] = {g[1] * mw[2] - g[2] * mw[1], g[2] * mw[0] - g[0] * mw[2], g[0] * mw[1] - g[1] * mw[0]};
    float w2n[3];
    if (NormalizeDir(w2, w2n) == 0.0f)
        return 0;
    float w3[3] = {g[1] * w2n[2] - g[2] * w2n[1], g[2] * w2n[0] - g[0] * w2n[2], g[0] * w2n[1] - g[1] * w2n[0]};

    /* R = W·Bᵀ（行主序 3×3）：W 的列是 [g, w2n, w3]，B 的列是 [a, b2n, b3] */
    const float *Wc[3] = {g, w2n, w3};
    const float *Bc[3] = {a, b2n, b3};
    float R[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            R[i][j] = Wc[0][i] * Bc[0][j] + Wc[1][i] * Bc[1][j] + Wc[2][i] * Bc[2][j];
        }
    }

    /* ZYX(aerospace) 取角 —— 与 Lib_Math_EulerToQuat 同一约定 */
    euler_t e;
    e.roll = Lib_Math_Atan2(R[2][1], R[2][2]);
    e.pitch = Lib_Math_Atan2(-R[2][0], Lib_Math_Sqrt(R[0][0] * R[0][0] + R[1][0] * R[1][0]));
    e.yaw = Lib_Math_Atan2(R[1][0], R[0][0]);

    inst->q_nom = Lib_Math_EulerToQuat(e);
    inst->bias_nom[0] = inst->bias_nom[1] = inst->bias_nom[2] = 0.0f;
    return 1;
}

/*============================ VOFA 调试输出 ============================*/

/**
 * @brief 把本帧输出写进 VOFA 通道并发送（通道表见 .h 头注释）
 * @note CH1-15 与 drvlib_bmi088_kalman 同表（便于两个模块同录对比姿态），
 *       CH16-25 是本模块特有的四元数/磁数据/残差
 */
static void VofaOutput(const BMI088IST8310EskfInstance *inst)
{
    if (inst == NULL || !inst->vofa_enable)
        return;

    const BMI088IST8310Eskf_Data_t *d = &inst->data;
    VofaSetChannel(1, d->euler.roll);
    VofaSetChannel(2, d->euler.pitch);
    VofaSetChannel(3, d->euler.yaw);
    VofaSetChannel(4, d->gyro[0]);
    VofaSetChannel(5, d->gyro[1]);
    VofaSetChannel(6, d->gyro[2]);
    VofaSetChannel(7, d->acc[0]);
    VofaSetChannel(8, d->acc[1]);
    VofaSetChannel(9, d->acc[2]);
    VofaSetChannel(10, d->dt * 1000.0f);
    VofaSetChannel(11, d->yaw_rate);
    VofaSetChannel(12, d->temperature);
    VofaSetChannel(13, d->bias[0]);
    VofaSetChannel(14, d->bias[1]);
    VofaSetChannel(15, d->bias[2]);
    VofaSetChannel(16, d->quat.w);
    VofaSetChannel(17, d->quat.x);
    VofaSetChannel(18, d->quat.y);
    VofaSetChannel(19, d->quat.z);
    VofaSetChannel(20, d->mag[0]);
    VofaSetChannel(21, d->mag[1]);
    VofaSetChannel(22, d->mag[2]);
    VofaSetChannel(23, (float)d->mag_used);
    VofaSetChannel(24, inst->acc_resid);
    VofaSetChannel(25, inst->mag_resid);
    VofaSend();
}

/*============================ 公开接口实现 ============================*/

int8_t BMI088IST8310EskfRegister(BMI088IST8310EskfInstance *inst)
{
    if (inst == NULL || inst->imu == NULL || inst->magdrv == NULL)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Register: null instance");
        return -1;
    }
    if (BMI088Register(inst->imu) != 0)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Register: bmi088 failed");
        return -1;
    }
    if (IST8310Register(inst->magdrv) != 0)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Register: ist8310 failed");
        return -1;
    }
    return 0;
}

int8_t BMI088IST8310EskfConfig(BMI088IST8310EskfInstance *inst, const BMI088IST8310Eskf_Config_s *config)
{
    if (inst == NULL || config == NULL || inst->eskf == NULL)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Config: null instance");
        return -1;
    }

    /* ---- 硬件枚举 + 传感器参数转发给两个通信层 ---- */
    if (BMI088Config(inst->imu, &config->imu) != 0)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Config: bmi088 config failed");
        return -1;
    }

    /* IST8310 强制中断模式：轮询模式的阻塞采样与本模块的 2ms 任务是冲突的（见 .h 文件头） */
    IST8310_Config_s magcfg = config->mag;
    if (magcfg.work_mode != IST8310_MODE_INT || magcfg.i2c_mode != BSP_IT_MODE)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_WARNING,
               "Config: ist8310 work_mode/i2c_mode forced to INT/IT (was %d/%d)",
               (int)magcfg.work_mode, (int)magcfg.i2c_mode);
        magcfg.work_mode = IST8310_MODE_INT;
        magcfg.i2c_mode = BSP_IT_MODE;
    }
    if (IST8310Config(inst->magdrv, &magcfg) != 0)
    {
        /* 磁计配不上就按"无磁"退化跑：姿态仍由六轴维持（见 .h 退化路径），不阻断启动 */
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_WARNING, "Config: ist8310 config failed, magnetic dead-reckoning only");
    }

    /* ---- 标定参数：整份拷贝进实例（NULL = 完全不修正） ---- */
    BMI088CalibLoad(&inst->gyro_calib, (config->calib_imu != NULL) ? &config->calib_imu->gyro : NULL);
    BMI088CalibLoad(&inst->acc_calib, (config->calib_imu != NULL) ? &config->calib_imu->acc : NULL);
    inst->temp_ref = (config->calib_imu != NULL) ? config->calib_imu->temp_ref : 0.0f;
    IST8310CalibLoad(&inst->mag_calib, config->calib_mag);

    /* ---- 世界系参考 ---- */
    inst->gravity = (config->gravity > 0.0f) ? config->gravity : BMI088_IST8310_ESKF_G;
    inst->mag_inclination = config->mag_inclination;
    inst->mag_declination = config->mag_declination;
    /* 参考场强为 0 会让模长残差恒超门限 → 磁更新被静默全关。宁可拿个量级正确的
     * 缺省值顶着（并告警），也别让"有磁计却一点没用上"这种故障无声发生 */
    if (config->mag_ref_uT > 0.0f)
    {
        inst->mag_ref_uT = config->mag_ref_uT;
    }
    else
    {
        inst->mag_ref_uT = BMI088_IST8310_ESKF_DEF_MAG_REF;
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_WARNING, "Config: mag_ref_uT unset, using default %d uT",
               (int)inst->mag_ref_uT);
    }

    inst->g_w[0] = 0.0f;
    inst->g_w[1] = 0.0f;
    inst->g_w[2] = 1.0f;
    {
        float ci, si, cd, sd;
        Lib_Math_SinCos(inst->mag_inclination, &si, &ci);
        Lib_Math_SinCos(inst->mag_declination, &sd, &cd);
        inst->m_w[0] = ci * cd;
        inst->m_w[1] = -ci * sd;
        inst->m_w[2] = -si;
        (void)NormalizeDir(inst->m_w, inst->m_w); /* 倾角/偏角填反也不至于让量测失配 */
    }

    /* ---- 参数快照（0 视为未填；q_bias 例外，0 是合法值） ---- */
    inst->q_att = (config->q_att > 0.0f) ? config->q_att : BMI088_IST8310_ESKF_DEF_Q_ATT;
    inst->q_bias = config->q_bias;
    inst->r_acc = (config->r_acc > 0.0f) ? config->r_acc : BMI088_IST8310_ESKF_DEF_R_ACC;
    inst->r_mag = (config->r_mag > 0.0f) ? config->r_mag : BMI088_IST8310_ESKF_DEF_R_MAG;
    inst->p0_att = (config->p0_att > 0.0f) ? config->p0_att : BMI088_IST8310_ESKF_DEF_P0_ATT;
    inst->p0_bias = (config->p0_bias > 0.0f) ? config->p0_bias : BMI088_IST8310_ESKF_DEF_P0_BIAS;

    inst->acc_reject = (config->acc_reject > 0.0f) ? config->acc_reject : BMI088_IST8310_ESKF_ACC_REJECT;
    inst->r_inflate = (config->r_inflate > 0.0f) ? config->r_inflate : BMI088_IST8310_ESKF_R_INFLATE;
    inst->mag_dip_min = (config->mag_dip_min > 0.0f) ? config->mag_dip_min : BMI088_IST8310_ESKF_MAG_DIP_MIN;
    inst->seed_acc_tol = (config->seed_acc_tol > 0.0f) ? config->seed_acc_tol : BMI088_IST8310_ESKF_SEED_ACC_TOL;
    inst->dt_max = (config->dt_max > 0.0f) ? config->dt_max : BMI088_IST8310_ESKF_DT_MAX;
    inst->temp_lpf_alpha =
        (config->temp_lpf_alpha > 0.0f) ? config->temp_lpf_alpha : BMI088_IST8310_ESKF_TEMP_LPF_ALPHA;
    inst->vofa_enable = (config->vofa_enable != 0);

    /* 磁模长门限：未填时按参考场强的 30% 与一个下限取大者 */
    if (config->mag_reject > 0.0f)
    {
        inst->mag_reject = config->mag_reject;
    }
    else
    {
        float auto_rej = 0.3f * inst->mag_ref_uT;
        inst->mag_reject = (auto_rej > BMI088_IST8310_ESKF_DEF_MAG_REJECT) ? auto_rej
                                                                          : BMI088_IST8310_ESKF_DEF_MAG_REJECT;
    }

    /* ---- 建 ESKF：δx = [δθ(3); δb(3)]，量测 3 维（方向），无控制输入 ---- */
    float P0[36] = {0};
    for (int i = 0; i < 3; i++)
    {
        P0[i * 6 + i] = inst->p0_att;
        P0[(3 + i) * 6 + (3 + i)] = inst->p0_bias;
    }
    Eskf_Init_Config_s ecfg = {
        .n = 6,
        .m = 3,
        .l = 0,
        /* Joseph 更新：n=6 的额外代价可忽略，换来协方差长期不发散 */
        .opt = ESKF_OPT_JOSEPH,
        .ctx = inst, /* 实例就是 ctx：名义状态放在实例里 */
        .propagate = EskfPropagate,
        .measure = EskfMeasureAcc,
        .inject = EskfInjectDelta,
        /* G = I 的量化影响 <1e-3（lib_eskf 用例⑤），不挂重置雅可比 */
        .reset_jac = NULL,
        .P0 = P0,
        .Q = NULL,
        .R = NULL,
    };
    if (EskfInit(inst->eskf, &ecfg) != ESKF_OK)
    {
        BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_ERROR, "Config: eskf init failed");
        return -1;
    }

    /* ---- 状态复位 ---- */
    inst->q_nom = Lib_Math_QuatIdentity();
    inst->bias_nom[0] = inst->bias_nom[1] = inst->bias_nom[2] = 0.0f;
    inst->gyro_cal[0] = inst->gyro_cal[1] = inst->gyro_cal[2] = 0.0f;
    inst->r_acc_now = inst->r_acc;
    inst->r_mag_now = inst->r_mag;
    inst->acc_resid = inst->mag_resid = 0.0f;

    inst->seeded = 0;
    inst->valid = 0;
    inst->mag_valid = 0;
    inst->mag_used = 0;
    inst->acc_used = 0;

    inst->last_gyro_ts = 0;
    inst->last_acc_ts = 0;
    inst->last_mag_ts = 0;
    inst->mag_ts_latest = 0;
    inst->mag_miss = 0;
    inst->dt = 0.0f;

    inst->temp_filt = 0.0f;
    inst->temp_valid = 0;

    /* 温度还没有效值 → ΔT 按 0 处理，先按标定值原样生效 */
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        inst->gyro_bias[i] = inst->gyro_calib.bias[i];
        inst->acc_bias[i] = inst->acc_calib.bias[i];
    }

    /* 输出快照清零（temperature 保持 NAN 语义，由 Update 每帧刷新） */
    BMI088IST8310Eskf_Data_t zero = {0};
    zero.temperature = NAN;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        zero.bias[i] = inst->gyro_bias[i];
    inst->data = zero;

    BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_INFO, "config: g=%d mG ref=%d uT incl=%d mrad decl=%d mrad",
           (int)(inst->gravity * 1000.0f), (int)inst->mag_ref_uT, (int)(inst->mag_inclination * 1000.0f),
           (int)(inst->mag_declination * 1000.0f));
    return 0;
}

void BMI088IST8310EskfUpdate(BMI088IST8310EskfInstance *inst)
{
    if (inst == NULL || inst->imu == NULL || inst->eskf == NULL)
        return;

    /* ---- 两条流各取最新一帧；插值会把各流真实节奏抹平，故不对齐时间戳 ---- */
    BMI088_Data_t m = BMI088Read(inst->imu, BMI088_READ_LATEST);

    /* IST8310Read 兼作中断模式的链路看门狗入口：必须每帧调 */
    IST8310_Data_t mg = {0};
    if (inst->magdrv != NULL)
    {
        mg = IST8310Read(inst->magdrv);
    }

    /* ---- 温度：原始值进 data（给人看），补偿用滤过的版本 ----
     * 首次拿到有效温度直接装载，否则滤波从 0 冷启动会产生一段假 ΔT（几十度 × 温度
     * 系数）导致零偏阶跃。NaN 自比较为假，故 t == t 即有效 */
    float t = BMI088GetTemperature(inst->imu);
    inst->data.temperature = t;
    if (t == t)
    {
        if (!inst->temp_valid)
        {
            inst->temp_filt = t;
            inst->temp_valid = 1;
        }
        else
        {
            inst->temp_filt += inst->temp_lpf_alpha * (t - inst->temp_filt);
        }
    }

    /* ---- 生效零偏：标定值 + 温度系数·ΔT（温漂未标定时恒等于标定值） ---- */
    float dT = inst->temp_valid ? (inst->temp_filt - inst->temp_ref) : 0.0f;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        inst->gyro_bias[i] = inst->gyro_calib.bias[i] + inst->gyro_calib.bias_tempco[i] * dT;
        inst->acc_bias[i] = inst->acc_calib.bias[i] + inst->acc_calib.bias_tempco[i] * dT;
    }

    /* ---- 磁计新帧判据 + 链路死活判定 ---- */
    uint8_t mag_new = 0;
    if (mg.time_stamp != 0 && mg.time_stamp != inst->last_mag_ts)
    {
        inst->last_mag_ts = mg.time_stamp;
        inst->mag_ts_latest = mg.time_stamp;
        inst->mag_miss = 0;
        mag_new = 1;
    }
    else if (inst->mag_miss < 0xFFFFu)
    {
        inst->mag_miss++;
        if (inst->mag_miss >= BMI088_IST8310_ESKF_MAG_MISS_LIMIT)
            inst->mag_valid = 0; /* 链路已死：不再参与融合，等恢复 */
    }

    /* ---- 启动瞬态：陀螺还没出数据，姿态保持上一次（valid 不变） ---- */
    if (m.time_stamp_g == 0)
    {
        inst->data.dt = 0.0f;
        inst->data.time_stamp_g = 0;
        VofaOutput(inst);
        return;
    }

    /* ---- dt：陀螺相邻两帧时间戳之差（与参与积分的样本严格同源） ---- */
    inst->dt = 0.0f;
    if (inst->last_gyro_ts != 0 && m.time_stamp_g > inst->last_gyro_ts)
    {
        inst->dt = (float)(m.time_stamp_g - inst->last_gyro_ts) * 1e-6f;
        if (inst->dt > inst->dt_max)
            inst->dt = inst->dt_max; /* 任务被拖长时钳位，宁可少积分也不要一步大跳变 */
    }
    inst->last_gyro_ts = m.time_stamp_g;

    /* ---- 标定修正：drv 层输出的是未补偿物理量，补偿全在这里 ---- */
    BMI088AxisCorrect(m.gyro, inst->gyro_bias, &inst->gyro_calib, inst->gyro_cal);
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        inst->data.gyro[i] = inst->gyro_cal[i];

    uint8_t acc_ok = (m.time_stamp_a != 0);
    float acc[BMI088_AXIS_NUM] = {0};
    float acc_norm = 0.0f;
    if (acc_ok)
    {
        BMI088AxisCorrect(m.acc, inst->acc_bias, &inst->acc_calib, acc);
        for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
            inst->data.acc[i] = acc[i];
        acc_norm = Lib_Math_Sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
    }

    /* 磁：每帧都修一遍（中断模式下读到的是同一帧，值稳定，便于观察） */
    float mag_c[IST8310_AXIS_NUM] = {0};
    float mag_norm = 0.0f;
    if (mg.time_stamp != 0)
    {
        IST8310MagCorrect(mg.mag, &inst->mag_calib, mag_c);
        for (uint8_t i = 0; i < IST8310_AXIS_NUM; i++)
            inst->data.mag[i] = mag_c[i];
        mag_norm = Lib_Math_Sqrt(mag_c[0] * mag_c[0] + mag_c[1] * mag_c[1] + mag_c[2] * mag_c[2]);
        /* 必须是**新帧**才算链路活着：链路死后 IST8310Read 仍返回上一次的旧值
         * （时间戳不变、模长照旧正常），只看 mag_norm 会把刚被 miss 计数判死的
         * mag_valid 又拉回 1，看门狗形同虚设 */
        if (mag_new && mag_norm > BMI088_IST8310_ESKF_DIR_MIN)
            inst->mag_valid = 1;
    }

    /* ---- 上电播种（只做一次） ---- */
    if (!inst->seeded && acc_ok && Lib_Math_Fabs(acc_norm - inst->gravity) < inst->seed_acc_tol)
    {
        const float *mseed = (mag_norm > BMI088_IST8310_ESKF_DIR_MIN) ? mag_c : NULL;
        if (AttitudeSeed(inst, acc, mseed))
        {
            inst->seeded = 1;
            inst->valid = 1;
            /* 名义状态已写进 q_nom → 清 δ、P 回 P0。必须在任何 Update 之前做 */
            (void)EskfReset(inst->eskf);
            euler_t se = Lib_Math_QuatToEuler(inst->q_nom);
            BSPLOG(&g_bmi088_ist8310_eskf_log, LOG_LEVEL_INFO, "seeded: roll=%d pitch=%d yaw=%d (mrad) mag=%d",
                   (int)(se.roll * 1000.0f), (int)(se.pitch * 1000.0f), (int)(se.yaw * 1000.0f),
                   (int)(mseed != NULL));
        }
    }

    uint8_t acc_used = 0;
    uint8_t mag_used = 0;
    inst->acc_used = 0;
    inst->mag_used = 0;

    if (inst->seeded)
    {
        /* ---- 1. 陀螺 → 预测（唯一传播名义姿态的地方） ----
         * dt == 0 表示本帧没有新陀螺样本（任务比陀螺 ODR 快时必然出现），不传播 */
        if (inst->dt > 0.0f)
        {
            (void)EskfPredict(inst->eskf, NULL, inst->dt);
        }

        /* ---- 2. acc 量测（新样本 + 过门限才用） ---- */
        uint8_t acc_new = (acc_ok && m.time_stamp_a != inst->last_acc_ts);
        float z_a[3] = {0};
        uint8_t z_a_ok = 0;
        if (acc_ok)
        {
            z_a_ok = (NormalizeDir(acc, z_a) > 0.0f);
        }
        if (acc_new)
        {
            inst->last_acc_ts = m.time_stamp_a;
            float err = acc_norm - inst->gravity;
            if (z_a_ok && Lib_Math_Fabs(err) < inst->acc_reject)
            {
                /* 门限内 R 连续放大：越不像"只剩重力"越不信它 */
                inst->r_acc_now = inst->r_acc * (1.0f + inst->r_inflate * (err * err) /
                                                           (inst->acc_reject * inst->acc_reject));
                if (EskfUpdateM(inst->eskf, 3, z_a, EskfMeasureAcc) == ESKF_OK)
                {
                    (void)EskfInject(inst->eskf); /* 硬约束：Update 后必须立即 Inject */
                    acc_used = 1;
                }
            }
        }

        /* ---- 3. mag 量测：模长门限 + 与重力近平行保护 ---- */
        if (mag_new && inst->mag_valid && mag_norm > BMI088_IST8310_ESKF_DIR_MIN)
        {
            float z_m[3];
            NormalizeDir(mag_c, z_m);
            float e_m = mag_norm - inst->mag_ref_uT;

            /* 磁矢量与重力近平行时航向退化：|z_m × z_a| 太小就跳过本次 mag 更新 */
            float dip = 1.0f;
            if (z_a_ok)
            {
                float c[3] = {z_m[1] * z_a[2] - z_m[2] * z_a[1], z_m[2] * z_a[0] - z_m[0] * z_a[2],
                              z_m[0] * z_a[1] - z_m[1] * z_a[0]};
                dip = Lib_Math_Sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
            }

            if (dip < inst->mag_dip_min)
            {
                /* 航向暂不可观测：本帧不吃磁，靠陀螺维持 */
            }
            else if (Lib_Math_Fabs(e_m) < inst->mag_reject)
            {
                inst->r_mag_now = inst->r_mag * (1.0f + inst->r_inflate * (e_m * e_m) /
                                                           (inst->mag_reject * inst->mag_reject));
                if (EskfUpdateM(inst->eskf, 3, z_m, EskfMeasureMag) == ESKF_OK)
                {
                    (void)EskfInject(inst->eskf);
                    mag_used = 1;
                }
            }
        }
    }

    inst->acc_used = acc_used;
    inst->mag_used = mag_used;

    /* ---- 输出 ----
     * yaw_rate 用欧拉角运动学的精确式（与 drvlib_bmi088_kalman 同式），
     * 角速率取**扣掉全部零偏**后的值（标定 + 温漂 + ESKF 残余） */
    euler_t e = Lib_Math_QuatToEuler(inst->q_nom);
    float yaw_rate = 0.0f;
    if (inst->seeded)
    {
        float wb[3];
        for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
            wb[i] = inst->gyro_cal[i] - inst->bias_nom[i];

        float sr, cr, sp, cp;
        Lib_Math_SinCos(e.roll, &sr, &cr);
        Lib_Math_SinCos(e.pitch, &sp, &cp);
        if (cp < BMI088_IST8310_ESKF_COS_TILT_MIN)
            cp = BMI088_IST8310_ESKF_COS_TILT_MIN; /* 奇点保护（pitch → ±90°） */
        yaw_rate = (sr * wb[1] + cr * wb[2]) / cp;
    }

    inst->data.euler = e;
    inst->data.quat = inst->q_nom;
    inst->data.yaw_rate = yaw_rate;
    inst->data.dt = inst->dt;
    inst->data.time_stamp_g = m.time_stamp_g;
    inst->data.time_stamp_mag = inst->mag_ts_latest;
    inst->data.valid = inst->valid;
    inst->data.mag_valid = inst->mag_valid;
    inst->data.mag_used = inst->mag_used;
    inst->data.acc_used = inst->acc_used;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        inst->data.bias[i] = inst->gyro_bias[i] + inst->bias_nom[i];

    VofaOutput(inst);
}

BMI088IST8310Eskf_Data_t BMI088IST8310EskfGetData(const BMI088IST8310EskfInstance *inst)
{
    BMI088IST8310Eskf_Data_t d = {0};
    d.temperature = NAN;

    if (inst != NULL)
        d = inst->data;

    return d;
}

#endif /* DRVLIB_BMI088_IST8310_ESKF_USED && DRV_BMI088_USED && DRV_IST8310_USED && LIB_ESKF_USED */
