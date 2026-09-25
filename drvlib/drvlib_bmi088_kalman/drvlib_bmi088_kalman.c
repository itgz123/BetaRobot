/**
 * @file drvlib_bmi088_kalman.c
 * @brief BMI088 + 线性卡尔曼：通信、标定参数装载、姿态解算一体（实现）
 *
 * @note 设计说明、适用域、标定语义全在 drvlib_bmi088_kalman.h 的头注释里，
 *       本文件只记实现层面的取舍（见各函数内注释）
 */

#include "drvlib_bmi088_kalman.h"

#if defined(DRVLIB_BMI088_KALMAN_USED) && defined(DRV_BMI088_USED) && defined(LIB_KF_USED)

#include <math.h> /* NAN：温度不可用的表示（lib_math 间接包含，这里显式写出） */
#include "bsp_log.h"
#include "drv_vofa.h" /* VOFA_USED / VOFA_UART 未定义时全部是空实现 */

/*============================ 日志实例 ============================*/

/* 本模块日志实例（日志关闭时宏为空、不分配，BSPLOG 空宏不引用） */
#ifndef DRVLIB_BMI088_KALMAN_LOG_LIMIT
#define DRVLIB_BMI088_KALMAN_LOG_LIMIT 10
#endif // !DRVLIB_BMI088_KALMAN_LOG_LIMIT
LOG_INSTANCE_DEF(g_bmi088_kalman_log, "bmi088_kalman", DRVLIB_BMI088_KALMAN_LOG_LIMIT);

/*============================ 内部常量 ============================*/

/* 本模块所有可调参数都是 #ifndef 可覆盖的：在 app_cfg.h（或任何先于本文件包含的
 * 头文件、编译选项 -D）里定义同名宏即可覆盖，不必改驱动源码。
 * 覆盖点只需早于本文件包含 —— 本 .c 首行就是 drvlib_bmi088_kalman.h，
 * 而它包含 app_cfg.h，所以写在 app_cfg.h 里一定生效。 */

/* 标准重力加速度 (m/s²)：倾角反算与 |acc| 判据都按它算 */
#ifndef BMI088_KALMAN_G
#define BMI088_KALMAN_G 9.80665f
#endif // !BMI088_KALMAN_G

/* KF 状态初值不确定度：倾角按 ±5.7°、零偏残差按 ±0.01 rad/s 给 */
#ifndef BMI088_KALMAN_P0_TILT
#define BMI088_KALMAN_P0_TILT (0.01f)
#endif // !BMI088_KALMAN_P0_TILT
#ifndef BMI088_KALMAN_P0_BIAS
#define BMI088_KALMAN_P0_BIAS (1e-4f)
#endif // !BMI088_KALMAN_P0_BIAS

/* R 随 |acc| 偏离 1g 的程度连续放大的最大倍数（实际放大 1+该值 倍）：
 * 越不像"只剩重力"就越不信它。用连续放大而不是硬性切断 —— 硬切断会让
 * 大机动期间倾角完全靠陀螺积分布漂 */
#ifndef BMI088_KALMAN_R_INFLATE_MAX
#define BMI088_KALMAN_R_INFLATE_MAX (10.0f)
#endif // !BMI088_KALMAN_R_INFLATE_MAX

/* Config 字段为 0（未填）时的回填值。
 * ⚠ 只回填 q_bias 之外的那些：q_bias=0 是合法取值，不能被当成漏填
 *   （与 lib_pid 的 integral_limit=0 是同一类陷阱）。
 *   注意 0 并不等于"关闭残差零偏估计"——见头文件的说明。 */
#ifndef BMI088_KALMAN_DEF_Q_TILT
#define BMI088_KALMAN_DEF_Q_TILT (1e-5f) /* rad²/s */
#endif                                   // !BMI088_KALMAN_DEF_Q_TILT
#ifndef BMI088_KALMAN_DEF_R_TILT
#define BMI088_KALMAN_DEF_R_TILT (4e-4f) /* rad²   */
#endif                                   // !BMI088_KALMAN_DEF_R_TILT
#ifndef BMI088_KALMAN_DEF_DT_MAX
#define BMI088_KALMAN_DEF_DT_MAX (0.01f) /* s      */
#endif                                   // !BMI088_KALMAN_DEF_DT_MAX
#ifndef BMI088_KALMAN_DEF_TEMP_LPF_ALPHA
#define BMI088_KALMAN_DEF_TEMP_LPF_ALPHA (0.01f) /* ≈0.2s @500Hz：只留分钟级的温漂 */
#endif                                           // !BMI088_KALMAN_DEF_TEMP_LPF_ALPHA

/*============================ 内部函数声明 ============================*/

static void KalmanAxisInit(KalmanInstance *kf, float r_tilt);
static float KalmanAxisStep(KalmanInstance *kf, float rate, float dt, const BMI088KalmanInstance *inst, uint8_t meas_ok, float z, float r_now);

/*============================ 内部函数实现 ============================*/

/**
 * @brief 初始化一个轴的 KF（n=2 状态 [倾角, 零偏残差] / m=1 量测 / l=1 控制）
 * @param kf 滤波器实例
 * @param r_tilt 倾角量测噪声方差 (rad²)
 *
 * @note 只有 H/R 是常量，F/Q/B 的 dt 相关项每帧在 KalmanAxisStep 里改写；
 *       opt 用 Joseph 更新：n=2 的代价可忽略，换来协方差长期不发散
 */
static void KalmanAxisInit(KalmanInstance *kf, float r_tilt)
{
    const float P0[4] = {BMI088_KALMAN_P0_TILT, 0.0f, 0.0f, BMI088_KALMAN_P0_BIAS};
    const float F[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    const float H[2] = {1.0f, 0.0f};
    const float R[1] = {r_tilt};
    const float B[2] = {0.0f, 0.0f};

    Kalman_Init_Config_s cfg = {
        .n = 2,
        .m = 1,
        .l = 1, /* 控制输入 u = 运动学角速率 (rad/s) */
        .opt = KALMAN_OPT_JOSEPH,
        .x0 = NULL, /* 状态零起步：姿态紧接着被播种值覆盖 */
        .P0 = P0,
        .F = F,
        .Q = NULL, /* 每帧按 dt 改写 */
        .H = H,
        .R = R,
        .B = B,
    };
    KalmanInit(kf, &cfg);
}

/**
 * @brief 一个轴的预测 + 量测更新，返回更新后的倾角
 * @param kf 滤波器实例
 * @param rate 该轴的运动学角速率 (rad/s)，已扣标定零偏
 * @param dt 步长 (s)
 * @param inst 融合实例（取 q_tilt/q_bias）
 * @param meas_ok 1 = 本帧加速度计可用于校正
 * @param z 加速度计反算倾角 (rad)
 * @param r_now 本帧量测噪声方差 (rad²)
 * @return 倾角估计 (rad)
 */
static float KalmanAxisStep(KalmanInstance *kf, float rate, float dt, const BMI088KalmanInstance *inst, uint8_t meas_ok, float z, float r_now)
{
    float u[1] = {rate};

    /* 时变模型：lib_kf 不感知时间，dt 由调用方折进 F/B/Q 后传入 */
    KF_F(kf, 0, 1) = -dt; /* θ ← θ + (rate - b)·dt */
    KF_B(kf, 0, 0) = dt;
    KF_Q(kf, 0, 0) = inst->q_tilt * dt;
    KF_Q(kf, 1, 1) = inst->q_bias * dt;

    KalmanPredict(kf, u);

    if (meas_ok)
    {
        KF_R(kf, 0, 0) = r_now;
        KalmanUpdate(kf, &z);
    }

    return KF_X(kf, 0);
}

/*⚠ 标定修正函数 BMI088AxisCorrect 与标定结构体在 drvlib_bmi088_calib.h（共用），
 *  本文件不再自己实现一份：同一颗 IMU 无论接哪个算法模块，修正模型与顺序必须
 *  完全一致，否则两个模块的输出没法互相印证。 */

/*============================ VOFA 调试输出 ============================*/

/* 把本帧输出写进 VOFA 通道并发送（通道表见 drvlib_bmi088_kalman.h 的同名一节）。
 * 不发帧就没人发 —— config.vofa_enable=1 时本模块自己发，app 不必再管 VOFA。
 * 只在 Update 的每个出口调用，保证一帧 Update 恰好一帧数据（dt==0 的帧也发，
 * 那正是要看的采样连续性）。 */
static void KalmanVofaOutput(const BMI088KalmanInstance *inst)
{
    if (inst == NULL || !inst->vofa_enable)
        return;

    const BMI088Kalman_Data_t *d = &inst->data;
    VofaSetChannel(1, d->euler.roll); /* 姿态 (rad) */
    VofaSetChannel(2, d->euler.pitch);
    VofaSetChannel(3, d->euler.yaw);
    VofaSetChannel(4, d->gyro[0]); /* 修正后角速度 (rad/s) */
    VofaSetChannel(5, d->gyro[1]);
    VofaSetChannel(6, d->gyro[2]);
    VofaSetChannel(7, d->acc[0]); /* 修正后加速度 (m/s²) */
    VofaSetChannel(8, d->acc[1]);
    VofaSetChannel(9, d->acc[2]);
    VofaSetChannel(10, d->dt * 1000.0f); /* 本帧步长 (ms)，0 = 无新陀螺样本 */
    VofaSetChannel(11, d->yaw_rate);     /* 世界系 yaw 角速度 (rad/s) */
    VofaSetChannel(12, d->temperature);  /* 温度 (℃)，未采到为 NAN */
    VofaSetChannel(13, d->bias[0]);      /* 本帧生效零偏 (rad/s) */
    VofaSetChannel(14, d->bias[1]);
    VofaSetChannel(15, d->bias[2]);
    VofaSend();
}

/*============================ 公开接口实现 ============================*/

int8_t BMI088KalmanRegister(BMI088KalmanInstance *inst)
{
    if (inst == NULL || inst->imu == NULL)
    {
        BSPLOG(&g_bmi088_kalman_log, LOG_LEVEL_ERROR, "Register: null instance");
        return -1;
    }
    return BMI088Register(inst->imu);
}

int8_t BMI088KalmanConfig(BMI088KalmanInstance *inst, const BMI088Kalman_Config_s *config)
{
    if (inst == NULL || config == NULL)
    {
        BSPLOG(&g_bmi088_kalman_log, LOG_LEVEL_ERROR, "Config: null instance");
        return -1;
    }

    /* 硬件枚举 + 传感器参数转发给通信层 */
    if (BMI088Config(inst->imu, &config->imu) != 0)
    {
        BSPLOG(&g_bmi088_kalman_log, LOG_LEVEL_ERROR, "Config: imu config failed");
        return -1;
    }

    /* ---- 标定参数：整份拷贝进实例（NULL 视为完全不修正），scale ≤ 0 归一成 1 ---- */
    BMI088CalibLoad(&inst->gyro_calib, (config->calib != NULL) ? &config->calib->gyro : NULL);
    BMI088CalibLoad(&inst->acc_calib, (config->calib != NULL) ? &config->calib->acc : NULL);
    inst->temp_ref = (config->calib != NULL) ? config->calib->temp_ref : 0.0f;

    /* ---- 参数快照（0 视为未填，回填缺省；q_bias 除外，0 是合法值） ---- */
    inst->q_tilt = (config->q_tilt > 0.0f) ? config->q_tilt : BMI088_KALMAN_DEF_Q_TILT;
    inst->q_bias = config->q_bias;
    inst->r_tilt = (config->r_tilt > 0.0f) ? config->r_tilt : BMI088_KALMAN_DEF_R_TILT;
    inst->dt_max = (config->dt_max > 0.0f) ? config->dt_max : BMI088_KALMAN_DEF_DT_MAX;
    inst->temp_lpf_alpha = (config->temp_lpf_alpha > 0.0f) ? config->temp_lpf_alpha : BMI088_KALMAN_DEF_TEMP_LPF_ALPHA;
    inst->vofa_enable = (config->vofa_enable != 0); /* 归一成 0/1，非 0 都算开 */

    /* ---- 状态复位：姿态/滤波器/温度低通全部重来 ---- */
    KalmanAxisInit(inst->kf_roll, inst->r_tilt);
    KalmanAxisInit(inst->kf_pitch, inst->r_tilt);

    inst->euler.roll = 0.0f;
    inst->euler.pitch = 0.0f;
    inst->euler.yaw = 0.0f;
    inst->seeded = 0;
    inst->valid = 0;

    inst->last_gyro_ts = 0;
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
    BMI088Kalman_Data_t zero = {0};
    zero.temperature = NAN;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        zero.bias[i] = inst->gyro_bias[i];
    inst->data = zero;

    BSPLOG(&g_bmi088_kalman_log, LOG_LEVEL_INFO,
           "config: gyro_bias(urad/s) %d %d %d tempco(1e-6/degC) %d %d %d temp_ref=%d",
           (int)(inst->gyro_calib.bias[0] * 1e6f), (int)(inst->gyro_calib.bias[1] * 1e6f),
           (int)(inst->gyro_calib.bias[2] * 1e6f),
           (int)(inst->gyro_calib.bias_tempco[0] * 1e6f), (int)(inst->gyro_calib.bias_tempco[1] * 1e6f),
           (int)(inst->gyro_calib.bias_tempco[2] * 1e6f),
           (int)inst->temp_ref);
    return 0;
}

void BMI088KalmanUpdate(BMI088KalmanInstance *inst)
{
    if (inst == NULL || inst->imu == NULL)
        return;

    /* Kalman 是多速率融合：acc 与 gyro 各自独立参与预测/更新，插值会把两条流的
     * 真实节奏（各自的 dt）抹平，故取各自最新一帧、保留独立时间戳 */
    BMI088_Data_t m = BMI088Read(inst->imu, BMI088_READ_LATEST);

    /* ---- 温度：原始值直接进 data（给 VOFA/终端看），补偿用滤过的版本 ----
     * 首次拿到有效温度直接装载，否则滤波器从 0 冷启动会产生一段假 ΔT
     * （几十度 × 温度系数）导致零偏阶跃。NaN 自比较为假，故 t == t 即有效 */
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

    /* ---- 生效零偏：标定值 + 温度系数·ΔT（温漂斜率未标定时恒等于标定值）---- */
    float dT = inst->temp_valid ? (inst->temp_filt - inst->temp_ref) : 0.0f;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        inst->gyro_bias[i] = inst->gyro_calib.bias[i] + inst->gyro_calib.bias_tempco[i] * dT;
        inst->acc_bias[i] = inst->acc_calib.bias[i] + inst->acc_calib.bias_tempco[i] * dT;
    }

    /* 启动瞬态：陀螺还没出数据，姿态保持上一次（valid 不变） */
    if (m.time_stamp_g == 0)
    {
        inst->data.dt = 0.0f;
        inst->data.time_stamp_g = 0;
        KalmanVofaOutput(inst);
        return;
    }

    /* ---- dt：陀螺仪相邻两帧时间戳之差（与参与积分的样本严格同源）----
     * 任务比陀螺 ODR 快时必然出现 dt==0（本帧没有新样本），属正常，不积分 */
    inst->dt = 0.0f;
    if (inst->last_gyro_ts != 0 && m.time_stamp_g > inst->last_gyro_ts)
    {
        inst->dt = (float)(m.time_stamp_g - inst->last_gyro_ts) * 1e-6f;
        /* 任务被拖延时钳位：宁可少积分，也不要一步大跳变（时间戳倒走/异常
         * 大间隔同理，钳到 dt_max 后记 (int) 上限内，不会溢出） */
        if (inst->dt > inst->dt_max)
            inst->dt = inst->dt_max;
    }
    inst->last_gyro_ts = m.time_stamp_g;

    /* 本帧没有新陀螺样本：不积分，但照样出 VOFA（dt=0 就是这里发出去的） */
    if (inst->dt <= 0.0f)
    {
        inst->data.dt = 0.0f;
        inst->data.time_stamp_g = m.time_stamp_g;
        KalmanVofaOutput(inst);
        return;
    }

    float dt = inst->dt;
    uint8_t acc_ok = (m.time_stamp_a != 0);

    /* ---- 标定修正：drv 层输出的是未补偿物理量，补偿全在这里 ---- */
    float gyro[BMI088_AXIS_NUM];
    BMI088AxisCorrect(m.gyro, inst->gyro_bias, &inst->gyro_calib, gyro);
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        inst->data.gyro[i] = gyro[i];

    float acc[BMI088_AXIS_NUM] = {0};
    float acc_norm = 0.0f;
    if (acc_ok)
    {
        BMI088AxisCorrect(m.acc, inst->acc_bias, &inst->acc_calib, acc);
        for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
            inst->data.acc[i] = acc[i];
        acc_norm = Lib_Math_Sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
    }

    /* ---- 加速度计反算倾角 + 量测门限 ---- */
    uint8_t meas_ok = 0;
    float r_now = inst->r_tilt;
    float acc_roll = 0.0f;
    float acc_pitch = 0.0f;
    if (acc_ok)
    {
        float err = acc_norm - BMI088_KALMAN_G;
        if (FABS(err) < BMI088_KALMAN_ACC_REJECT)
        {
            meas_ok = 1;
            /* R 随 |acc| 偏离 1g 的程度连续放大（最多 1+R_INFLATE_MAX 倍），
             * 越不像"只剩重力"就越不信它 */
            r_now = inst->r_tilt * (1.0f + BMI088_KALMAN_R_INFLATE_MAX * (err * err) / (BMI088_KALMAN_ACC_REJECT * BMI088_KALMAN_ACC_REJECT));
            /* 静止时加速度计指向 +Z、模长 1g，反算取 aerospace(ZYX) 约定 */
            acc_roll = Lib_Math_Atan2(acc[1], acc[2]);
            acc_pitch = Lib_Math_Atan2(-acc[0], Lib_Math_Sqrt(acc[1] * acc[1] + acc[2] * acc[2]));
        }
    }

    /* ---- 上电播种：六轴无法观测航向，yaw 以播种时刻的朝向为基准 0 ---- */
    if (!inst->seeded && acc_ok && FABS(acc_norm - BMI088_KALMAN_G) < BMI088_KALMAN_SEED_ACC_TOL)
    {
        inst->euler.roll = acc_roll;
        inst->euler.pitch = acc_pitch;
        inst->euler.yaw = 0.0f;
        inst->seeded = 1;
        inst->valid = 1;
        /* KF 状态同步到播种值：否则滤波器要从 0 慢慢爬到安装倾角，这段时间姿态是错的 */
        KF_X(inst->kf_roll, 0) = inst->euler.roll;
        KF_X(inst->kf_roll, 1) = 0.0f;
        KF_X(inst->kf_pitch, 0) = inst->euler.pitch;
        KF_X(inst->kf_pitch, 1) = 0.0f;

        BSPLOG(&g_bmi088_kalman_log, LOG_LEVEL_INFO,
               "attitude seeded: roll=%d pitch=%d (mrad)",
               (int)(inst->euler.roll * 1000.0f), (int)(inst->euler.pitch * 1000.0f));
    }

    float yaw_rate = 0.0f;
    if (inst->seeded)
    {
        /* ---- 欧拉角运动学速率（机体系 → 世界系欧拉角速率，精确式）----
         *   φ̇ = ωx + tanθ·(sinφ·ωy + cosφ·ωz)
         *   θ̇ = cosφ·ωy - sinφ·ωz
         *   ψ̇ = (sinφ·ωy + cosφ·ωz)/cosθ
         * 用精确式而不是"把 ω 投影到世界 Z"：后者得到的是 ψ̇ - sinθ·φ̇，
         * 倾角安装时差一个 sinθ·φ̇ 的交叉项（θ=15° 时约 0.27·φ̇）。 */
        float sr, cr, sp, cp;
        Lib_Math_SinCos(inst->euler.roll, &sr, &cr);
        Lib_Math_SinCos(inst->euler.pitch, &sp, &cp);
        if (cp < BMI088_KALMAN_COS_TILT_MIN)
            cp = BMI088_KALMAN_COS_TILT_MIN; /* 奇点保护，见头文件"适用域" */

        float tan_pitch = sp / cp;
        float roll_rate = gyro[0] + tan_pitch * (sr * gyro[1] + cr * gyro[2]);
        float pitch_rate = cr * gyro[1] - sr * gyro[2];
        yaw_rate = (sr * gyro[1] + cr * gyro[2]) / cp;

        /* ---- roll/pitch：KF 预测（用运动学速率）+ 加速度计量测 ---- */
        inst->euler.roll = KalmanAxisStep(inst->kf_roll, roll_rate, dt, inst, meas_ok, acc_roll, r_now);
        inst->euler.pitch = KalmanAxisStep(inst->kf_pitch, pitch_rate, dt, inst, meas_ok, acc_pitch, r_now);

        /* ---- yaw：六轴不可观测，纯积分，零偏准不准直接决定精度 ---- */
        inst->euler.yaw = Lib_Math_WrapAngleNegPIToPI(inst->euler.yaw + yaw_rate * dt);
    }

    /* ---- 输出快照 ---- */
    inst->data.euler = inst->euler;
    inst->data.yaw_rate = yaw_rate;
    inst->data.dt = dt;
    inst->data.time_stamp_g = m.time_stamp_g;
    inst->data.valid = inst->valid;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        inst->data.bias[i] = inst->gyro_bias[i];

    KalmanVofaOutput(inst);
}

BMI088Kalman_Data_t BMI088KalmanGetData(const BMI088KalmanInstance *inst)
{
    BMI088Kalman_Data_t d = {0};
    d.temperature = NAN;

    if (inst != NULL)
        d = inst->data;

    return d;
}

#endif /* DRVLIB_BMI088_KALMAN_USED && DRV_BMI088_USED && LIB_KF_USED */
