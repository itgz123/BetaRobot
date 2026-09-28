/**
 * @file drvlib_bmi088_ist8310_eskf.h
 * @brief BMI088(陀螺+加速度计) + IST8310(磁力计) 误差状态卡尔曼(ESKF) 姿态融合
 *
 * ═══════════════════ 为什么是 ESKF 而不是欧拉角 KF ═══════════════════
 * 现有的 drvlib_bmi088_kalman 用两个 2 状态线性 KF 分别滤 roll/pitch，yaw 纯积分：
 * 六轴（无磁计）下航向本来就不可观测，零偏误差会让 yaw 无界漂移。
 * 本模块引入磁力计给航向可观性，同时改用**误差状态 ESKF**：
 *   - 名义状态用四元数（无万向锁、无三角函数近似），在滤波器**外面**，由本模块传播；
 *   - 滤波器内只跑 6 维小量 δx = [δθ(3); δb_g(3)]，线性化精度高、协方差数值条件好；
 *   - 每次量测更新后把 δx **注入**名义状态并清零。
 * 算法骨架在 lib_eskf（通用库，PC 端有独立测试锁死符号约定）。
 *
 * ═══════════════════ 坐标系与四元数约定（全文自洽，实机前不可改） ═══════════════════
 *   - 世界系：右手，X=北, Y=西, Z=上。重力参考 ĝ_w = [0,0,1]
 *     （BMI088 静止时 acc ≈ +Z，与 drvlib_bmi088_kalman 逐位一致）。
 *   - 磁参考：m_w = [cosI·cosD, -cosI·sinD, -sinI]（单位向量）
 *     I = 磁倾角（+ 表示指向地下，北半球为正）、D = 磁偏角（+ 表示东偏）。
 *     推导：水平分量 = cosI·(cosD·北 + sinD·东)，而东 = -Y ⇒ [cosI·cosD, -cosI·sinD]；
 *     垂直分量指向"下" = -Z ⇒ -sinI。
 *   - q：**机体系 → 世界系**，即 v_w = q ⊗ v_b ⊗ q⁻¹（= Lib_Math_QuatRotateVector）。
 *     故世界系向量转到机体系是 R(q)ᵀ·v_w = Lib_Math_QuatRotateVector(q⁻¹, v_w)。
 *   - 误差定义：q_true = q_nom ⊗ δq，δq = normalize([1, δθ/2])，δθ = **机体系**小旋转（右乘）。
 *     由此得出的线性化（lib_eskf 用例②⑥已锁死）：
 *       ω = 陀螺标定值 - b_nom
 *       F_c = [[ -[ω]× , -I₃ ],[ 0₃ , 0₃ ]]        （6×6，F = I₆ + F_c·dt）
 *       H   = [ [h]× , 0₃ ]                        （h = R(q_nom)ᵀ·参考方向）
 *       残差 y = z - h(q_nom)                      （**不是** h - z）
 *
 * ═══════════════════ 量测模型：归一化方向，不是原向量 ═══════════════════
 * 两条量测都先把观测**归一化成单位方向**再进滤波器：
 *   acc: z_a = acc_cal/|acc_cal|,  h_a = R(q_nom)ᵀ·ĝ_w
 *   mag: z_m = mag_cal/|mag_cal|,  h_m = R(q_nom)ᵀ·m_w
 * 好处是 R 直接就是"方向噪声方差"，与模长漂移/尺度误差解耦 —— 磁计的绝对模长本来
 * 就标不准（椭球拟合只能定形状，靠参考场强钉死），让它进量测就是引入系统性偏差。
 * 模长偏离只用于**门限**（判这一帧是不是被电机磁场/线加速度污染了）。
 *
 * ═══════════════════ 可观测性 ═══════════════════
 * acc 约束重力的 2 个自由度（绕重力轴的转动不可观测），mag 约束剩下的 1 个航向自由度。
 * 两者必须**不平行**才有满秩：方向近平行时（磁矢量与重力共线）航向退化，
 * 故有 `mag_dip_min` 门限。反过来，只有 acc（无磁计）时航向不可观测 ——
 * 这时 `H_mag` 整块缺失，δθ 的绕重力方向分量只由 Q 驱动、由陀螺维持，
 * 行为等价于原来的六轴模块（见"退化路径"）。
 *
 * ═══════════════════ 多速率 ═══════════════════
 * 三条流各自独立参与：陀螺新样本 → Predict；acc/mag 新样本 → 各自的 Update。
 * **不插值、不对齐时间戳** —— 插值会把各流真实的采样节奏抹平。dt 由陀螺相邻两帧
 * 时间戳差分得到（与参与积分的样本严格同源）。
 * 每帧最坏两次量测更新，**中间必须各注入一次**（lib_eskf 的硬约束：
 * Predict(δ=0) → Update(δ≠0) → Inject(δ=0) → …），本模块已按此编排。
 *
 * ═══════════════════ 退化路径（重要） ═══════════════════
 *   - 无 gyro（时间戳恒 0）：不传播，输出保持上次，valid 不变。
 *   - 无 acc：跳过 acc 更新；已播种则靠陀螺维持（短时间内可靠）。
 *   - 无 mag / 磁计全 0 / 时间戳不变：跳过 mag 更新，valid=1 但 mag_valid=mag_used=0，
 *     此时行为等同原六轴模块。
 *   - mag 有效但被门限拒（模长离谱 / 与重力近平行）：**仅本帧**不更新，mag_used=0，
 *     协方差由后续帧的 Q 增长自己纠回来。
 *   - 未播种：valid=0，app 应保持上电安全态。
 *
 * ═══════════════════ IST8310 用中断模式（硬约束） ═══════════════════
 * Config 内**强制** work_mode=INT、i2c_mode=IT（配置里有别的值会被覆盖并打日志）：
 * 轮询模式的 `IST8310Sample` 要在 2ms 任务里阻塞等测量（数 ms）不可接受；
 * 中断模式的采集链自维持，任务侧 `IST8310Read` 只取双缓冲最新帧**并兼链路看门狗**
 * —— 本模块每帧 Update 都调它，天然满足"必须周期性调用"的前提。
 *
 * ═══════════════════ VOFA 调试输出（通道表） ═══════════════════
 * 避开 kalman/mahony 的 CH1-15、drvlib_axis 的 CH1-12、app_shoot 的 CH1-3：
 * | 通道 | 含义 | 通道 | 含义 |
 * |---|---|---|---|
 * | CH16-19 | 名义四元数 w/x/y/z | CH23 | mag_used (0/1) |
 * | CH20-22 | 标定修正后 mag x/y/z (µT) | CH24 | acc 残差模长 \|z_a-h_a\| |
 * | CH25 | mag 残差模长 \|z_m-h_m\| | | |
 * （CH1-15 仍由本模块填成姿态/角速度/加速度/dt/零偏，与 kalman 版同表，
 *   便于两个模块同录对比 —— 见 VofaOutput。）
 *
 * ═══════════════════ 与 drvlib_bmi088_kalman 的取舍 ═══════════════════
 * 两个模块**不要同时开**：都会自己发 VOFA 帧、都会占一套 BMI088 实例。
 * 实机验证新模块通过后再决定是否删掉旧的开关。
 *
 * @note 标定（陀螺/acc 在 drvlib_bmi088_calib.h、磁在 drvlib_ist8310_calib.h）都是
 *       PC 端离线做完写死进固件，本模块不做任何运行期标定；ESKF 只估**残余**零偏。
 */

#ifndef __DRVLIB_BMI088_IST8310_ESKF_H
#define __DRVLIB_BMI088_IST8310_ESKF_H

#include "app_cfg.h"

/* 依赖四个被组合的模块：两个 drv 通信层 + lib_eskf + lib_math（任一未开则整体不编译） */
#if defined(DRVLIB_BMI088_IST8310_ESKF_USED) && defined(DRV_BMI088_USED) && defined(DRV_IST8310_USED) &&               \
    defined(LIB_ESKF_USED)

#include "drv_bmi088.h"
#include "drv_ist8310.h"
#include "lib_eskf.h"
#include "lib_math.h"
#include "drvlib_bmi088_calib.h"  /* gyro/acc 标定（与 kalman/mahony 共用） */
#include "drvlib_ist8310_calib.h" /* mag 标定（硬铁/软铁/安装旋转/尺度） */

/*============================ 可覆盖缺省值 ============================*/

/* 本模块可调参数都带 #ifndef 守卫：在 app_cfg.h（或任何先于本文件包含的地方、-D）
 * 定义同名宏即可覆盖。⚠ 值要带 f 后缀，否则会在 float 表达式里走 double 运算。 */

/* 标准重力加速度 (m/s²)：|acc| 判据与播种都按它算 */
#ifndef BMI088_IST8310_ESKF_G
#define BMI088_IST8310_ESKF_G 9.80665f
#endif // !BMI088_IST8310_ESKF_G

/* δθ 过程噪声谱密度 (rad²/s)：陀螺白噪声 + 未建模动态。
 * 量级参考：陀螺噪声密度 0.014°/s/√Hz ≈ 2.4e-4 rad/s/√Hz ⇒ 方差密度 ≈ 6e-8 rad²/s，
 * 再留 1~2 个数量级给未建模动态，故取 1e-6 */
#ifndef BMI088_IST8310_ESKF_DEF_Q_ATT
#define BMI088_IST8310_ESKF_DEF_Q_ATT 1e-6f
#endif // !BMI088_IST8310_ESKF_DEF_Q_ATT

/* δb 过程噪声谱密度 (rad²/s³)：决定滤波器"多相信零偏会变"。
 * 0 是合法取值（等价于把零偏当常量），但会让 P_bias 收敛到 0 后不再跟随温漂 */
#ifndef BMI088_IST8310_ESKF_DEF_Q_BIAS
#define BMI088_IST8310_ESKF_DEF_Q_BIAS 1e-9f
#endif // !BMI088_IST8310_ESKF_DEF_Q_BIAS

/* 方向量测噪声方差 (rad²)：σ = 0.1 rad ≈ 5.7° 的保守起点。
 * 实机上 acc 受线加速度/振动污染、mag 受电机磁场污染，都不是白噪声，
 * 门限与 R 自适应负责把坏帧的等效 R 拉大 */
#ifndef BMI088_IST8310_ESKF_DEF_R_ACC
#define BMI088_IST8310_ESKF_DEF_R_ACC 1e-2f
#endif // !BMI088_IST8310_ESKF_DEF_R_ACC
#ifndef BMI088_IST8310_ESKF_DEF_R_MAG
#define BMI088_IST8310_ESKF_DEF_R_MAG 1e-2f
#endif // !BMI088_IST8310_ESKF_DEF_R_MAG

/* δθ / δb 协方差初值：姿态按 ±3°、零偏残差按 ±0.01 rad/s */
#ifndef BMI088_IST8310_ESKF_DEF_P0_ATT
#define BMI088_IST8310_ESKF_DEF_P0_ATT 3e-3f /* (3°=0.0524rad)² ≈ 2.7e-3 */
#endif                                       // !BMI088_IST8310_ESKF_DEF_P0_ATT
#ifndef BMI088_IST8310_ESKF_DEF_P0_BIAS
#define BMI088_IST8310_ESKF_DEF_P0_BIAS 1e-4f /* (0.01 rad/s)² */
#endif                                        // !BMI088_IST8310_ESKF_DEF_P0_BIAS

/* acc 门限：|acc| 偏离 1g 超过该值 (m/s²) 就整帧跳过 acc 更新 */
#ifndef BMI088_IST8310_ESKF_ACC_REJECT
#define BMI088_IST8310_ESKF_ACC_REJECT 3.0f
#endif // !BMI088_IST8310_ESKF_ACC_REJECT

/* 门限内 R 连续放大的最大倍数（实际放大 1+该值 倍）：越不像"只剩重力/只有地磁"
 * 就越不信它。用连续放大而非硬切 —— 硬切会让大机动期间完全靠陀螺积分布漂 */
#ifndef BMI088_IST8310_ESKF_R_INFLATE
#define BMI088_IST8310_ESKF_R_INFLATE 10.0f
#endif // !BMI088_IST8310_ESKF_R_INFLATE

/* mag 门限：|mag_cal| 偏离 mag_ref_uT 超过该值 (µT) 就跳过。
 * 0 = 用 max(0.3·mag_ref_uT, 15µT) 自动推 */
#ifndef BMI088_IST8310_ESKF_DEF_MAG_REJECT
#define BMI088_IST8310_ESKF_DEF_MAG_REJECT 15.0f
#endif // !BMI088_IST8310_ESKF_DEF_MAG_REJECT

/* 参考场强缺省值 (µT)：`mag_ref_uT` 填 0 时用它兜底。
 * 不用 0 —— 那样模长残差 e_m 恒等于实测模长，必然超门限，磁更新会被**静默**全关掉。
 * 中纬度地磁总场大致 45~55 µT，取 50 只是量级正确；真值必须按实际地点/标定脚本填 */
#ifndef BMI088_IST8310_ESKF_DEF_MAG_REF
#define BMI088_IST8310_ESKF_DEF_MAG_REF 50.0f
#endif // !BMI088_IST8310_ESKF_DEF_MAG_REF

/* 近平行保护：|z_m × z_a| 小于该值说明磁矢量与重力近乎共线，航向退化，跳过 mag 更新 */
#ifndef BMI088_IST8310_ESKF_MAG_DIP_MIN
#define BMI088_IST8310_ESKF_MAG_DIP_MIN 0.15f
#endif // !BMI088_IST8310_ESKF_MAG_DIP_MIN

/* 播种：|acc| 偏离 1g 小于该值 (m/s²) 才允许拿它定初始姿态 */
#ifndef BMI088_IST8310_ESKF_SEED_ACC_TOL
#define BMI088_IST8310_ESKF_SEED_ACC_TOL 2.0f
#endif // !BMI088_IST8310_ESKF_SEED_ACC_TOL

/* 单步传播 dt 上限 (s)：任务被拖长时钳位，宁可少积分也不要一步大跳变 */
#ifndef BMI088_IST8310_ESKF_DT_MAX
#define BMI088_IST8310_ESKF_DT_MAX 0.01f
#endif // !BMI088_IST8310_ESKF_DT_MAX

/* 欧拉角运动学分母 cos(pitch) 的下限（|cosθ| < 该值即钳位，约 ±84° 奇点保护）：
 * 只用于 yaw_rate 的显示式，姿态本身是四元数，没有奇点 */
#ifndef BMI088_IST8310_ESKF_COS_TILT_MIN
#define BMI088_IST8310_ESKF_COS_TILT_MIN 0.1f
#endif // !BMI088_IST8310_ESKF_COS_TILT_MIN

/* 温漂补偿用的温度低通系数 (0~1)：温度噪声直接用会把噪声注入角速率（yaw 是它的纯积分） */
#ifndef BMI088_IST8310_ESKF_TEMP_LPF_ALPHA
#define BMI088_IST8310_ESKF_TEMP_LPF_ALPHA 0.01f /* ≈0.2s @500Hz */
#endif                                           // !BMI088_IST8310_ESKF_TEMP_LPF_ALPHA

/*============================ 配置结构体 ============================*/

/**
 * @brief ESKF 融合配置
 *
 * @note 所有 *噪声/门限* 字段填 0 视为"未填、用缺省宏"；唯一例外是 `q_bias`
 *       （0 是合法取值，与 drvlib_bmi088_kalman 的同一取舍）。
 */
typedef struct
{
    /* ---- 转发给 drv_bmi088（硬件枚举 + 传感器参数 + daemon） ---- */
    BMI088_Config_s imu;

    /* ---- 转发给 drv_ist8310（硬件枚举 + 器件参数 + daemon） ----
     * ⚠ work_mode / i2c_mode 会被强制为 INT / IT（理由见文件头），填别的值只打日志 */
    IST8310_Config_s mag;

    /* ---- PC 端标定结果（NULL = 完全不修正） ---- */
    const BMI088_Calib_s *calib_imu; /* 陀螺 + 加速度计 */
    const IST8310Calib_s *calib_mag; /* 硬铁/软铁/安装旋转/尺度 */

    /* ---- 世界系参考 ----
     * mag_* 决定 yaw 相对**哪个方向**：若以视觉世界系为准，用 declination 对齐。
     * mag_ref_uT 必须与实际标定时钉死的场强一致（模长门限看的就是它）。 */
    float gravity;         /* 重力加速度 (m/s²)；0 → 用 BMI088_IST8310_ESKF_G      */
    float mag_inclination; /* 磁倾角 (rad，+ = 指向地下)                          */
    float mag_declination; /* 磁偏角 (rad，+ = 东偏)                              */
    float mag_ref_uT;      /* 磁感应强度 (µT)；0 → 用缺省宏并告警（必须按实际标定填） */

    /* ---- ESKF 噪声参数 ---- */
    float q_att;   /* δθ 过程噪声 (rad²/s)；0 → 缺省宏 */
    float q_bias;  /* δb 过程噪声 (rad²/s³)；**0 是合法值**，原样使用 */
    float r_acc;   /* acc 方向量测噪声 (rad²)；0 → 缺省宏 */
    float r_mag;   /* mag 方向量测噪声 (rad²)；0 → 缺省宏 */
    float p0_att;  /* δθ 初值方差 (rad²)；0 → 缺省宏 */
    float p0_bias; /* δb 初值方差 (rad²/s²)；0 → 缺省宏 */

    /* ---- 门限与自适应（见头文件可覆盖缺省值一节） ---- */
    float acc_reject;     /* 0 → 缺省宏 */
    float r_inflate;      /* 0 → 缺省宏（acc 与 mag 共用同一放大律） */
    float mag_reject;     /* 0 → max(0.3·mag_ref_uT, 缺省宏) */
    float mag_dip_min;    /* 0 → 缺省宏 */
    float seed_acc_tol;   /* 0 → 缺省宏 */
    float dt_max;         /* 0 → 缺省宏 */
    float temp_lpf_alpha; /* 0 → 缺省宏 */

    /* ---- VOFA 调试输出 ----
     * 1 = 每次 Update 把姿态/角速度/... 写进 VOFA 通道并**自己发帧**（通道表见文件头）；
     * 0（默认）= 完全不碰 VOFA。
     * ⚠ 为 1 时帧由本模块发出，app 就不要再对同一帧调 VofaSend。 */
    uint8_t vofa_enable;
} BMI088IST8310Eskf_Config_s;

/*============================ 输出数据结构 ============================*/

/**
 * @brief 融合输出
 */
typedef struct
{
    euler_t euler;           /* 姿态 (rad)，ZYX 顺序，yaw 已由磁计约束、不再无界漂移 */
    quaternion_t quat;       /* 名义四元数（机体系→世界系），与 euler 同源 */
    float gyro[3];           /* 已按标定修正的角速度 (rad/s)，机体系 */
    float acc[3];            /* 已按标定修正的加速度 (m/s²)，机体系 */
    float mag[3];            /* 已按标定修正的磁感应强度 (µT)，IMU 机体系 */
    float yaw_rate;          /* 世界系 yaw 角速度 (rad/s)，即 ψ̇，已扣全部零偏 */
    float bias[3];           /* 本帧生效零偏 (rad/s) = 标定值 + 温漂 + ESKF 残余估计 */
    float dt;                /* 本帧陀螺传播步长 (s)：0 = 本帧无新陀螺样本 */
    float temperature;       /* 原始温度 (℃)，不可用为 NAN（补偿用的是它滤过之后的版本） */
    uint64_t time_stamp_g;   /* 本帧陀螺时间戳 (us)，0 = 尚无数据 */
    uint64_t time_stamp_mag; /* 最近一帧磁计时间戳 (us)，0 = 从未拿到 */
    uint8_t valid;           /* 1 = 姿态可用（已播种）；dt==0 的帧不清零 */
    uint8_t mag_valid;       /* 1 = 磁计链路有数据（时间戳在前推、模长非 0） */
    uint8_t mag_used;        /* 1 = 本帧的磁量测真的进了滤波器（通过门限） */
    uint8_t acc_used;        /* 1 = 本帧的 acc 量测真的进了滤波器（通过门限） */
} BMI088IST8310Eskf_Data_t;

/*============================ 实例结构体 ============================*/

/**
 * @brief 融合实例
 * @note **本实例就是 ESKF 的 ctx**：名义状态（q_nom / bias_nom）就放在这里，
 *       供 propagate / measure / inject 回调就地读写。
 */
typedef struct BMI088IST8310EskfInstance
{
    /* 子模块实例（由 DEF 宏绑定指针） */
    BMI088Instance *imu;     /* drv_bmi088 */
    IST8310Instance *magdrv; /* drv_ist8310 */
    EskfInstance *eskf;      /* lib_eskf 误差状态滤波器（n=6, m=3, l=0） */

    /* ---- 标定参数（config 拷贝）与本帧生效零偏 ---- */
    BMI088_AxisCalib_s gyro_calib;
    BMI088_AxisCalib_s acc_calib;
    IST8310Calib_s mag_calib;
    float temp_ref;
    float gyro_bias[BMI088_AXIS_NUM]; /* 标定 + 温漂 */
    float acc_bias[BMI088_AXIS_NUM];  /* 标定 + 温漂 */

    /* ---- 名义状态（ESKF 的"外层状态"） ---- */
    quaternion_t q_nom; /* 机体系→世界系名义四元数 */
    float bias_nom[3];  /* ESKF 估计的残余零偏 (rad/s) */

    /* ---- 回调间传递的每帧暂存（由 Update 填，各回调读） ---- */
    float gyro_cal[3]; /* 本帧修正后角速度 (rad/s) */
    float r_acc_now;   /* 本帧 acc 量测噪声方差 (rad²)，含门限自适应 */
    float r_mag_now;   /* 本帧 mag 量测噪声方差 (rad²)，含门限自适应 */
    float acc_resid;   /* 本帧 acc 残差模长 |z_a - h_a|（注入前算，供 VOFA 观察） */
    float mag_resid;   /* 本帧 mag 残差模长 |z_m - h_m|（注入前算，供 VOFA 观察） */

    /* ---- 世界系单位磁参考（Config 时由倾角/偏角算好） ---- */
    float m_w[3];
    float g_w[3];

    /* ---- 姿态可用性 ---- */
    uint8_t seeded;    /* 是否已播种（播了才 valid） */
    uint8_t valid;     /* 姿态是否可用 */
    uint8_t mag_valid; /* 磁计链路是否有数据 */
    uint8_t mag_used;  /* 本帧磁量测是否进了滤波器 */
    uint8_t acc_used;  /* 本帧 acc 量测是否进了滤波器 */

    /* ---- 各流的时间戳（"新样本"判据） ---- */
    uint64_t last_gyro_ts;
    uint64_t last_acc_ts;
    uint64_t last_mag_ts;
    uint64_t mag_ts_latest; /* 最近一帧磁计时间戳（输出用） */
    uint16_t mag_miss;      /* 连续多少次 Update 没等到新磁帧；超过上限则 mag_valid=0 */
    float dt;

    /* ---- 温漂补偿用的温度低通 ---- */
    float temp_lpf_alpha;
    float temp_filt;
    uint8_t temp_valid; /* 0 = 还没拿到过有效温度（此时按 ΔT=0 处理） */

    /* ---- config 快照 ---- */
    float gravity;
    float mag_inclination, mag_declination, mag_ref_uT;
    float q_att, q_bias, r_acc, r_mag, p0_att, p0_bias;
    float acc_reject, r_inflate, mag_reject, mag_dip_min, seed_acc_tol, dt_max;
    uint8_t vofa_enable;

    /* ---- 最近一次输出 ---- */
    BMI088IST8310Eskf_Data_t data;
} BMI088IST8310EskfInstance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief 定义融合实例（内嵌 drv_bmi088 + drv_ist8310 + ESKF(6,3,0)）
 * @param name 实例名
 *
 * @note 内嵌而非外挂：ODR/量程/BW 这些"滤波假设的前置条件"由本模块统一配置，
 *       避免 app 侧两处配置割裂；app 只持有本实例。
 * @note 静态 RAM：ESKF(6,3,0) ≈ 1.31KB + 两个 drv 实例（BMI088 含环形缓冲、IST8310 含 I2C 缓冲）
 *
 * @example BMI088_IST8310_ESKF_INSTANCE_DEF(imu);
 */
#define BMI088_IST8310_ESKF_INSTANCE_DEF(name)                                                                         \
    BMI088_INSTANCE_DEF(name##_imu);                                                                                   \
    IST8310_INSTANCE_DEF(name##_magdrv);                                                                               \
    ESKF_INSTANCE_DEF(name##_eskf, 6, 3, 0);                                                                           \
    static BMI088IST8310EskfInstance name = {.imu = &name##_imu,                                                       \
                                             .magdrv = &name##_magdrv,                                                 \
                                             .eskf = &name##_eskf,                                                     \
                                             .q_nom = {1.0f, 0.0f, 0.0f, 0.0f},                                        \
                                             .g_w = {0.0f, 0.0f, 1.0f}}

/*============================ 公开接口 ============================*/

/**
 * @brief 注册（注册 BMI088 与 IST8310 两组子模块；硬件映射与参数在 Config 里给）
 * @param inst 实例指针
 * @return 0 成功，-1 失败
 * @note 要求在 Config 之前调用
 */
int8_t BMI088IST8310EskfRegister(BMI088IST8310EskfInstance *inst);

/**
 * @brief 配置（转发两个 drv 配置 + 装载标定 + 建 ESKF + 复位姿态）
 * @param inst 实例指针
 * @param config 配置结构体指针
 * @return 0 成功，-1 失败
 * @note `config->mag.work_mode/i2c_mode` 被强制为 INT/IT（见文件头）；填别的值会打日志
 * @note 可重复调用（会重置姿态与滤波器）
 */
int8_t BMI088IST8310EskfConfig(BMI088IST8310EskfInstance *inst, const BMI088IST8310Eskf_Config_s *config);

/**
 * @brief 更新一帧（读两条 drv 流 → dt → 标定 → 播种 → ESKF 预测/量测更新 → 输出）
 * @param inst 实例指针
 * @note 由 app 周期任务调用（本模块设计按 2ms 任务）。各流的实际采样节奏由
 *       时间戳决定，与调用频率解耦（调快只是更及时地取到新样本）
 * @note 本函数兼作 IST8310 中断模式的**链路看门狗**入口（`IST8310Read` 内部）：
 *       必须周期性调用，否则磁采集链停住不会被发现
 */
void BMI088IST8310EskfUpdate(BMI088IST8310EskfInstance *inst);

/**
 * @brief 取最近一次输出（按值返回）
 * @param inst 实例指针
 * @return 输出数据；inst 为空返回全 0（valid=0，temperature=NAN）
 */
BMI088IST8310Eskf_Data_t BMI088IST8310EskfGetData(const BMI088IST8310EskfInstance *inst);

#endif /* DRVLIB_BMI088_IST8310_ESKF_USED && DRV_BMI088_USED && DRV_IST8310_USED && LIB_ESKF_USED */

#endif /* __DRVLIB_BMI088_IST8310_ESKF_H */
