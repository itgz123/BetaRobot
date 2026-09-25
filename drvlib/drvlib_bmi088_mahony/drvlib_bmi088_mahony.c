/**
 * @file drvlib_bmi088_mahony.c
 * @brief BMI088 + Mahony 互补滤波：通信、标定参数装载、姿态解算一体（实现）
 *
 * @note 设计说明、适用域、标定语义全在 drvlib_bmi088_mahony.h 的头注释里，
 *       本文件只记实现层面的取舍（见各函数内注释）
 * @note 结构与 drvlib_bmi088_kalman.c 一一对应，便于对照阅读与互换验证
 */

#include "drvlib_bmi088_mahony.h"

#if defined(DRVLIB_BMI088_MAHONY_USED) && defined(DRV_BMI088_USED) && defined(LIB_MAHONY_USED)

#include <math.h> /* NAN：温度不可用的表示（lib_math 间接包含，这里显式写出） */
#include "bsp_log.h"
#include "drv_vofa.h" /* VOFA_USED / VOFA_UART 未定义时全部是空实现 */

/*============================ 日志实例 ============================*/

/* 本模块日志实例（日志关闭时宏为空、不分配，BSPLOG 空宏不引用） */
#ifndef DRVLIB_BMI088_MAHONY_LOG_LIMIT
#define DRVLIB_BMI088_MAHONY_LOG_LIMIT 10
#endif // !DRVLIB_BMI088_MAHONY_LOG_LIMIT
LOG_INSTANCE_DEF(g_bmi088_mahony_log, "bmi088_mahony", DRVLIB_BMI088_MAHONY_LOG_LIMIT);

/*============================ 内部常量 ============================*/

/* 本模块所有可调参数都是 #ifndef 可覆盖的：在 app_cfg.h（或任何先于本文件包含的
 * 头文件、编译选项 -D）里定义同名宏即可覆盖，不必改驱动源码。
 * 覆盖点只需早于本文件包含 —— 本 .c 首行就是 drvlib_bmi088_mahony.h，
 * 而它包含 app_cfg.h，所以写在 app_cfg.h 里一定生效。 */

/* 标准重力加速度 (m/s²)：倾角反算与 |acc| 判据都按它算 */
#ifndef BMI088_MAHONY_G
#define BMI088_MAHONY_G 9.80665f
#endif // !BMI088_MAHONY_G

/* 加速度计校正的模长门限（相对 1g 的比例）：|acc| 偏离 1g 超过该比例就认为
 * 不只剩重力（有线性加速度/振动），本帧不给内核校正量、退化为纯陀螺积分。
 * 0.3 → 0.7g~1.3g，绝对量约 ±2.94 m/s²，与 KF 版 ACC_REJECT 3.0 同量级。
 * 这个门限原本在 lib_mahony 内部（它属于"量测可不可信"的模型知识，不是算法）。 */
#ifndef BMI088_MAHONY_ACC_NORM_TOL
#define BMI088_MAHONY_ACC_NORM_TOL 0.3f
#endif // !BMI088_MAHONY_ACC_NORM_TOL

/* Config 字段为 0（未填）时的回填值。
 * ⚠ ki 不在其列：ki=0 是"不做零偏估计"的合法取值（本模块的推荐配置），
 *   不能被当成漏填（与 lib_pid 的 integral_limit=0 是同一类陷阱）。
 * kp=0 会让滤波器退化成纯陀螺积分（校正量恒为 0），没有实用价值，故视为漏填。 */
#ifndef BMI088_MAHONY_DEF_KP
#define BMI088_MAHONY_DEF_KP (1.0f) /* 六轴推荐 0.5~1.0 */
#endif                              // !BMI088_MAHONY_DEF_KP
#ifndef BMI088_MAHONY_DEF_DT_MAX
#define BMI088_MAHONY_DEF_DT_MAX (0.01f) /* s      */
#endif                                   // !BMI088_MAHONY_DEF_DT_MAX
#ifndef BMI088_MAHONY_DEF_TEMP_LPF_ALPHA
#define BMI088_MAHONY_DEF_TEMP_LPF_ALPHA (0.01f) /* ≈0.2s @500Hz：只留分钟级的温漂 */
#endif                                           // !BMI088_MAHONY_DEF_TEMP_LPF_ALPHA

/*============================ VOFA 调试输出 ============================*/

/* 把本帧输出写进 VOFA 通道并发送（通道表见 drvlib_bmi088_mahony.h 的同名一节）。
 * 不发帧就没人发 —— config.vofa_enable=1 时本模块自己发，app 不必再管 VOFA。
 * 只在 Update 的每个出口调用，保证一帧 Update 恰好一帧数据（dt==0 的帧也发，
 * 那正是要看的采样连续性）。 */
static void MahonyVofaOutput(const BMI088MahonyInstance *inst)
{
    if (inst == NULL || !inst->vofa_enable)
        return;

    const BMI088Mahony_Data_t *d = &inst->data;
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

    /* CH16-19：四元数（本模块的原生状态）。通道数是编译期常量，调小了这些写 */
    VofaSetChannel(16, d->quat.w);
    VofaSetChannel(17, d->quat.x);
    VofaSetChannel(18, d->quat.y);
    VofaSetChannel(19, d->quat.z);

    VofaSend();
}

/*============================ 公开接口实现 ============================*/

int8_t BMI088MahonyRegister(BMI088MahonyInstance *inst)
{
    if (inst == NULL || inst->imu == NULL)
    {
        BSPLOG(&g_bmi088_mahony_log, LOG_LEVEL_ERROR, "Register: null instance");
        return -1;
    }
    return BMI088Register(inst->imu);
}

int8_t BMI088MahonyConfig(BMI088MahonyInstance *inst, const BMI088Mahony_Config_s *config)
{
    if (inst == NULL || config == NULL || inst->mahony == NULL)
    {
        BSPLOG(&g_bmi088_mahony_log, LOG_LEVEL_ERROR, "Config: null instance");
        return -1;
    }

    /* 硬件枚举 + 传感器参数转发给通信层 */
    if (BMI088Config(inst->imu, &config->imu) != 0)
    {
        BSPLOG(&g_bmi088_mahony_log, LOG_LEVEL_ERROR, "Config: imu config failed");
        return -1;
    }

    /* ---- 标定参数：整份拷贝进实例（NULL 视为完全不修正），scale ≤ 0 归一成 1 ---- */
    BMI088CalibLoad(&inst->gyro_calib, (config->calib != NULL) ? &config->calib->gyro : NULL);
    BMI088CalibLoad(&inst->acc_calib, (config->calib != NULL) ? &config->calib->acc : NULL);
    inst->temp_ref = (config->calib != NULL) ? config->calib->temp_ref : 0.0f;

    /* ---- 参数快照（0 视为未填，回填缺省；ki 除外，0 是合法值） ---- */
    inst->kp = (config->kp > 0.0f) ? config->kp : BMI088_MAHONY_DEF_KP;
    inst->ki = config->ki;
    inst->dt_max = (config->dt_max > 0.0f) ? config->dt_max : BMI088_MAHONY_DEF_DT_MAX;
    inst->temp_lpf_alpha = (config->temp_lpf_alpha > 0.0f) ? config->temp_lpf_alpha : BMI088_MAHONY_DEF_TEMP_LPF_ALPHA;
    inst->vofa_enable = (config->vofa_enable != 0); /* 归一成 0/1，非 0 都算开 */

    /* ---- 滤波器复位：四元数回单位、积分清零（参数也一并写进 lib_mahony） ---- */
    Mahony_Init_Config_s mcfg = {
        .kp = inst->kp,
        .ki = inst->ki,
    };
    MahonyInit(inst->mahony, &mcfg);

    /* ---- 状态复位：姿态/温度低通全部重来 ---- */
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
    BMI088Mahony_Data_t zero = {0};
    zero.temperature = NAN;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        zero.bias[i] = inst->gyro_bias[i];
    inst->data = zero;

    BSPLOG(&g_bmi088_mahony_log, LOG_LEVEL_INFO,
           "config: kp=%d(1e-3) ki=%d(1e-3) gyro_bias(urad/s) %d %d %d tempco(1e-6/degC) %d %d %d temp_ref=%d",
           (int)(inst->kp * 1000.0f), (int)(inst->ki * 1000.0f), (int)(inst->gyro_calib.bias[0] * 1e6f),
           (int)(inst->gyro_calib.bias[1] * 1e6f), (int)(inst->gyro_calib.bias[2] * 1e6f),
           (int)(inst->gyro_calib.bias_tempco[0] * 1e6f), (int)(inst->gyro_calib.bias_tempco[1] * 1e6f),
           (int)(inst->gyro_calib.bias_tempco[2] * 1e6f), (int)inst->temp_ref);
    return 0;
}

void BMI088MahonyUpdate(BMI088MahonyInstance *inst)
{
    if (inst == NULL || inst->imu == NULL || inst->mahony == NULL)
        return;

    /* Mahony 是单速率姿态解算：acc 与 gyro 必须落在同一时刻，故取插值对齐后的帧
     * （两条流时间戳相等；Kalman 那种多速率融合才用 BMI088_READ_LATEST） */
    BMI088_Data_t m = BMI088Read(inst->imu, BMI088_READ_INTERP);

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
        MahonyVofaOutput(inst);
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
        MahonyVofaOutput(inst);
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

    /* ---- 上电播种：六轴无法观测航向，yaw 以播种时刻的朝向为基准 0 ----
     * 反算用 aerospace(ZYX) 约定，与 lib_math 的四元数/欧拉角约定一致：
     * 静止时 acc 指向 +Z（模长 1g），a = R^T·ẑ = (-sinθ, cosθsinφ, cosθcosφ) */
    if (!inst->seeded && acc_ok && FABS(acc_norm - BMI088_MAHONY_G) < BMI088_MAHONY_SEED_ACC_TOL)
    {
        euler_t init_e;
        init_e.roll = Lib_Math_Atan2(acc[1], acc[2]);
        init_e.pitch = Lib_Math_Atan2(-acc[0], Lib_Math_Sqrt(acc[1] * acc[1] + acc[2] * acc[2]));
        init_e.yaw = 0.0f;

        /* 直接写四元数：播种前那段纯陀螺积分的姿态是错的，这里整个覆盖掉
         * （也顺带把积分项清零 —— 播种前的误差累积没有意义） */
        inst->mahony->quat = Lib_Math_EulerToQuat(init_e);
        inst->mahony->integral_fb.x = 0.0f;
        inst->mahony->integral_fb.y = 0.0f;
        inst->mahony->integral_fb.z = 0.0f;

        inst->seeded = 1;
        inst->valid = 1;

        BSPLOG(&g_bmi088_mahony_log, LOG_LEVEL_INFO, "attitude seeded: roll=%d pitch=%d (mrad)",
               (int)(init_e.roll * 1000.0f), (int)(init_e.pitch * 1000.0f));
    }

    /* ---- 姿态模型（本模块持有）：选参考方向 + 判量测可信 + 归一化 ----
     * lib_mahony 是通用内核：不认加速度计、不认重力、不认门限，只要一个"机体系
     * 参考误差向量"。所以"参考方向是什么、本帧量测可不可信"由这里决定 ——
     * 与 drvlib_bmi088_kalman 对称（那边 ACC_REJECT 也是本模块自己判的）。 */
    vector3_t g3 = {gyro[0], gyro[1], gyro[2]};
    vector3_t err = {0.0f, 0.0f, 0.0f};
    if (acc_ok && FABS(acc_norm - BMI088_MAHONY_G) < BMI088_MAHONY_G * BMI088_MAHONY_ACC_NORM_TOL)
    {
        /* 世界系重力方向取 +Z：静止时 acc 测得的就是 R^T·(0,0,+g)。
         * 量测必须先归一化 —— 内核的误差向量以单位向量为前提 */
        vector3_t meas = {acc[0], acc[1], acc[2]};
        vector3_t ref = {0.0f, 0.0f, 1.0f};
        meas = Lib_Math_Vec3Scale(meas, 1.0f / acc_norm);
        err = MahonyErr(inst->mahony, meas, ref);
    }

    /* acc 无新样本、或模长不像 1g 时 err 为零向量 → 本帧退化为纯陀螺积分 */
    MahonyUpdate(inst->mahony, g3, err, dt);

    /* ---- 姿态：全部从四元数取（roll/pitch/yaw 同源，天然自洽）----
     * 与 KF 版的区别：那里 yaw 是本模块单独积分的，这里 yaw 就在四元数里 ——
     * 四元数传播用的是完整姿态运动学（不受欧拉角奇点限制），积分精度只会更好。
     * 加速度计校正量与重力方向垂直（水平轴），对 yaw 分量恒为 0，故 yaw 仍是
     * 纯陀螺积分，精度只取决于零偏准不准。 */
    inst->euler = Lib_Math_QuatToEuler(inst->mahony->quat);

    float yaw_rate = 0.0f;
    if (inst->seeded)
    {
        /* ---- 世界系 yaw 角速率：欧拉角运动学精确式 ψ̇ = (sinφ·ωy + cosφ·ωz)/cosθ
         * 只用于对外输出（与 KF 版同名同义、可比），不参与姿态解算。
         * 用精确式而不是"把 ω 投影到世界 Z"：后者得到的是 ψ̇ - sinθ·φ̇，
         * 倾角安装时差一个 sinθ·φ̇ 的交叉项（θ=15° 时约 0.27·φ̇）。
         * ⚠ 这一项在 |pitch| → 90° 附近受欧拉奇点影响（四元数本身不受影响）。 */
        float sr, cr, sp, cp;
        Lib_Math_SinCos(inst->euler.roll, &sr, &cr);
        Lib_Math_SinCos(inst->euler.pitch, &sp, &cp);
        if (cp < BMI088_MAHONY_COS_TILT_MIN)
            cp = BMI088_MAHONY_COS_TILT_MIN; /* 奇点保护，见头文件"适用域" */

        yaw_rate = (sr * gyro[1] + cr * gyro[2]) / cp;
    }

    /* ---- 输出快照 ---- */
    inst->data.euler = inst->euler;
    inst->data.quat = inst->mahony->quat;
    inst->data.yaw_rate = yaw_rate;
    inst->data.dt = dt;
    inst->data.time_stamp_g = m.time_stamp_g;
    inst->data.valid = inst->valid;
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
        inst->data.bias[i] = inst->gyro_bias[i];

    MahonyVofaOutput(inst);
}

BMI088Mahony_Data_t BMI088MahonyGetData(const BMI088MahonyInstance *inst)
{
    BMI088Mahony_Data_t d = {0};
    d.temperature = NAN;

    if (inst != NULL)
        d = inst->data;

    return d;
}

#endif /* DRVLIB_BMI088_MAHONY_USED && DRV_BMI088_USED && LIB_MAHONY_USED */
