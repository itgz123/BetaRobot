/**
 * @file drvlib_bmi088_kalman.h
 * @brief BMI088 + 线性卡尔曼：通信、标定参数装载、姿态解算一体
 *
 * ═══════════════════ 分层定位 ═══════════════════
 *   drv_bmi088 : 只做通信（寄存器/初始化/物理量换算/时间戳/温度）
 *   lib_kf     : 通用线性卡尔曼（纯算法，无时间/硬件依赖）
 *   本模块     : 器件 + 算法联合 —— 读通信层数据、按标定参数修正、跑线性 KF 出姿态
 *   对上（app）: 只暴露 姿态 / 角速度 / 零偏 / 温度 / 数据有效性
 *
 * ═══════════════════ 标定策略：全部在 PC 端离线完成 ═══════════════════
 * 本模块**不做任何运行期标定**：没有静止检测、没有自学习、没有标定状态机、没有
 * RAM 标定值。标定在上场前用 PC（python）做完，结果写进 app 里的一个
 * static const BMI088_Calib_s，随固件落进 .rodata —— 即"写死到固件" ——
 * Config 时整份交给本模块。换 IMU / 拆装 / 改安装方式后必须重新标定并重新烧录。
 *
 * 可标定项，以及"没有转台/温箱"时各自的可得性：
 *   ✅ 陀螺零偏          静止均值
 *   ✅ 陀螺零偏温漂斜率  自热扫温（上电静置数十分钟，记温度与零偏），不需温箱
 *   ⚠️ 陀螺尺度/非正交   需已知角速率参考：用云台 yaw 轴自转 + 编码器即可，不需转台
 *   ⚠️ acc 零偏/尺度/非正交  需六位置夹具。注意**绕竖直轴转动对 acc 完全无用**
 *      （重力在 IMU 系的投影不随绕 Z 旋转改变），要标 acc 必须改变它的姿态
 *   ❌ 非线性/迟滞、g 敏感、饱和、杆臂、带宽/延迟 —— 需转台/温箱/离心机，暂不做
 * 未标定的项保持结构体零值即可：scale 填 0 与填 1 等价（都不修正），其余 0 即不修正。
 * 也就是说 `static const BMI088_Calib_s c = {0};` 与"完全不修正"完全等价。
 *
 * 修正模型（陀螺/acc 同一套，顺序：先扣零偏 → 再去非正交 → 最后除尺度）：
 *   m = M·(S⊙t) + b   ⇒   t = S⁻¹ ⊙ (M⁻¹·(m - b))
 *   M（非正交/轴间失准）取对角为 1 的上三角，一阶近似 M⁻¹ ≈ I - offdiag。
 *
 * ═══════════════════ 姿态与滤波结构 ═══════════════════
 * 姿态用欧拉角（roll/pitch/yaw）+ 世界系角速率表示，**不用四元数**：
 *   欧拉角运动学速率（机体系角速度 → 世界系欧拉角速率，精确式）：
 *     φ̇ = ωx + tanθ·(sinφ·ωy + cosφ·ωz)
 *     θ̇ = cosφ·ωy - sinφ·ωz
 *     ψ̇ = (sinφ·ωy + cosφ·ωz) / cosθ
 *   旋转后的 yaw 直接用 ψ̇ 积分得到世界系航向，不存在"gyro.z 只有真值
 *   cosθ·cosφ≈0.96"的标度损失（那种损失只在"直接拿 ωz 当 ψ̇"时出现）。
 *
 * roll / pitch 各用一个 2 状态线性 KF（lib_kf），状态 x = [倾角, 零偏残差]：
 *   预测：θ ← θ + (rate - b)·dt ，b ← b      （F=[[1,-dt],[0,1]]，B·u=[dt·rate, 0]）
 *   量测：z = 加速度计反算倾角                  （H=[1,0]）
 *   其中 rate 是**已扣标定零偏**的运动学速率，故 b 是"标定之后剩余的残差"：
 *   上电零偏重复性（同一颗 IMU 每次上电的零偏散布）与未建模的慢漂。标定做得好时
 *   它应该很小，q_bias 置 0 即可（与 lib_pid 的 integral_limit=0 同类：0 是合法
 *   取值，不是漏填）。
 *   ⚠ 但 q_bias=0 **不等于关闭残差零偏估计**：F=[[1,-dt],[0,1]] 的耦合让
 *     P12 稳态 ≈ -dt·R、K2 ≠ 0，量测残差照样推动 b（仿真：静止 1~2s 内收敛到
 *     真值）。q_bias 的真实作用是给 b 加过程噪声，而 q_bias>0 反而让 b 过冲
 *     （仿真：q_bias=1e-6 时学到真值的 257%）。所以该项保持 0 是更稳的选择。
 *   与标定精度强耦合的是 P0_BIAS（b 的初值不确定度），标定越准应给得越小。
 *
 * yaw **不设 KF**：六轴下加速度计对 yaw 没有任何观测能力（重力方向不含航向
 * 信息），设一个永不更新的滤波器只是自欺欺人。yaw 精度只取决于零偏准不准。
 *
 * ═══════════════════ VOFA 调试输出 ═══════════════════
 * Config 里 vofa_enable = 1 时，本模块每次 Update 都把下面这些量写进 VOFA 的
 * JustFloat 通道并直接发帧（ch0 时间戳由 drv_vofa 自动填充，无需 app 干预）：
 *   CH1-3  : euler roll/pitch/yaw (rad)，yaw wrap 到 (-π, π]
 *   CH4-6  : gyro x/y/z (rad/s)，已按标定 + 温漂修正
 *   CH7-9  : acc x/y/z (m/s²)，已按标定修正
 *   CH10   : dt (ms)，0 = 本帧没有新陀螺样本（配 CH0 时间戳可查采样连续性）
 *   CH11   : 世界系 yaw 角速度 yaw_rate (rad/s)
 *   CH12   : BMI088 温度 (℃)，未采到为 NAN
 *   CH13-15: 本帧生效零偏 x/y/z (rad/s) = 标定值 + 温度系数·ΔT
 * 与 drvlib_bmi088_mahony 的通道表**逐通道相同**（同一件事的两种实现，上位机的
 * 图不用重配），换模块时只需把 vofa_enable 挪过去。
 *   - 发送与任务同频（2ms 任务 = 500 帧/s），写帧/发帧都在 Update 里；
 *     dt==0 的帧照样发（那正是要看的东西），姿态量保持上一帧的值。
 *   - drv_vofa 是三缓冲 DMA + 忙则跳过，波特率不够时自然丢帧、不阻塞控制任务。
 *   - VOFA_USED / VOFA_UART 未定义时所有 Vofa* 是空实现，本项零开销、无副作用。
 * ⚠ 通道号是全驱动唯一的一组：drv_axis_mit_lite 的 vofa_enable 占 CH1~CH12、
 *   app_shoot 的调试块占 CH1~CH3，都与本模块的 CH1~CH15 重叠。同一时刻只能有
 *   一路在写（VofaSend 发的是缓冲区里**所有**通道当下的值，混写会互相覆盖），
 *   要同时看就把别的模块的通道号错开。
 *
 * ⚠ 适用域：cosθ 出现在 φ̇ 与 ψ̇ 的分母上，θ → ±90° 时退化（欧拉角本身的
 *   奇点）。本模块按 |cosθ| ≥ BMI088_KALMAN_COS_TILT_MIN（0.1，约 ±84°）钳位，
 *   超出后 yaw 积分会失真。云台 IMU 装在 yaw 轴上游、相对重力只有固定安装
 *   倾角（本机约 15°），正常工作域内远离奇点；若将来把 IMU 移到会大角度
 *   俯仰的关节下游，需要改用四元数表示。
 *
 * ═══════════════════ 使用顺序 ═══════════════════
 * @code
 *     BMI088_KALMAN_INSTANCE_DEF(bmi088);
 *
 *     // PC 端标定结果，写死进固件（static const → .rodata）
 *     static const BMI088_Calib_s s_bmi088_calib = {
 *         .gyro = { .bias = {0.00089f, -0.00011f, 0.00169f} },
 *     };
 *
 *     BMI088KalmanRegister(&bmi088);                  // 注册子模块（一次）
 *     BMI088Kalman_Config_s cfg = {
 *         .imu = { .spi_e = SPI_BMI088, ... },        // 转发给 drv_bmi088
 *         .calib = &s_bmi088_calib,                   // NULL = 完全不修正
 *         // .vofa_enable = 1,                        // 1 = 顺便写 VOFA 通道并自动发帧
 *     };
 *     BMI088KalmanConfig(&bmi088, &cfg);              // 配置（可重复调用）
 *
 *     // 周期任务里（本模块按陀螺时间戳差分算 dt，与调用抖动无关）
 *     BMI088KalmanUpdate(&bmi088);
 *     BMI088Kalman_Data_t d = BMI088KalmanGetData(&bmi088);
 *     if (d.valid) { 用 d.euler / d.yaw_rate / d.gyro ... }
 * @endcode
 */

#ifndef __DRVLIB_BMI088_KALMAN_H
#define __DRVLIB_BMI088_KALMAN_H

#include "app_cfg.h"

/* 依赖两个被组合的模块：drv 通信层、lib_kf 算法层（任一未开则本模块整体不编译） */
#if defined(DRVLIB_BMI088_KALMAN_USED) && defined(DRV_BMI088_USED) && defined(LIB_KF_USED)

#include "drv_bmi088.h"
#include "lib_kf.h"
#include "lib_math.h"
#include "drvlib_bmi088_calib.h" /* 与 drvlib_bmi088_mahony 共用的标定结构与修正函数 */

/*============================ 常量 ============================*/

/* 本模块所有可调参数都是 #ifndef 可覆盖的：在 app_cfg.h（或任何先于本头文件包含的
 * 头文件、编译选项 -D）里定义同名宏即可覆盖，不必改驱动源码。
 * 覆盖点只需早于本文件包含 —— 本 .h 在常量之前就 #include "app_cfg.h"，
 * 所以写在 app_cfg.h 里一定生效。
 * ⚠ 覆盖值要带类型后缀（如 4.0f 而不是 4），否则会在 float 表达式里走 double 运算。 */

/* 欧拉角运动学分母 cosθ 的下限（|cosθ| < 该值即钳位，约 ±84° 奇点保护） */
#ifndef BMI088_KALMAN_COS_TILT_MIN
#define BMI088_KALMAN_COS_TILT_MIN 0.1f
#endif // !BMI088_KALMAN_COS_TILT_MIN

/* 播种时判定"加速度可信"的 |acc| 容差 (m/s²)：比量测门限严，因为播种的结果
 * 直接写进姿态（只播种一次）；放宽到 2.0 是为了上电时手在抖也能播上种 */
#ifndef BMI088_KALMAN_SEED_ACC_TOL
#define BMI088_KALMAN_SEED_ACC_TOL 2.0f
#endif // !BMI088_KALMAN_SEED_ACC_TOL

/* 加速度计量测更新的门限 (m/s²)：|acc| 偏离 1g 超过该值就整帧跳过量测更新 */
#ifndef BMI088_KALMAN_ACC_REJECT
#define BMI088_KALMAN_ACC_REJECT 3.0f
#endif // !BMI088_KALMAN_ACC_REJECT

/*============================ 标定结构体 ============================*/

/* 标定参数结构体（BMI088_AxisCalib_s / BMI088_Calib_s）与修正函数
 * （BMI088CalibLoad / BMI088AxisCorrect）是**器件属性**，不是本算法的属性，
 * 因此与 drvlib_bmi088_mahony 共用，定义在 drvlib_bmi088_calib.h。
 * app 里同一份 static const BMI088_Calib_s 可以喂给两个模块。 */

/*============================ 配置结构体 ============================*/

/**
 * @brief drvlib_bmi088_kalman 配置结构体
 *
 * @note 噪声参数均为连续时间量纲（Jacobian 形式），与 dt 线性相关：
 *       Q = diag(q_tilt·dt, q_bias·dt)，R = r_tilt
 *       调参顺序：先让 r_tilt 匹配加速度计倾角噪声（静止实测 std）与振动强度，
 *       再用 q_tilt 决定"跟陀螺还是跟加速度计"（大→跟陀螺，抗加速度干扰；
 *       小→跟加速度，抗漂移）。q_bias 一般比 q_tilt 小 3~4 个数量级。
 */
typedef struct
{
    /* ---- 转发给 drv_bmi088（硬件枚举 + 传感器参数 + daemon） ---- */
    BMI088_Config_s imu;

    /* ---- PC 端标定结果（NULL = 完全不修正）；结构体与 mahony 版共用 ---- */
    const BMI088_Calib_s *calib;

    /* ---- 线性 KF 噪声参数 ---- */
    float q_tilt; /* 倾角过程噪声 (rad²/s)：陀螺积分的不确定度增长 */
    float q_bias; /* 零偏残差过程噪声 (rad²/s³)：0 是合法值（推荐），但 ≠ 关闭 b 的估计 */
    float r_tilt; /* 倾角量测噪声 (rad²)：加速度计反算倾角的噪声 */
    float dt_max; /* 单步积分 dt 上限 (s)：任务被拖长时钳位，防一步跳变 */

    /* ---- 温漂补偿用的温度低通系数 (0~1) ----
     * 温度量测带量化噪声与采样抖动，直接用瞬时温度乘 bias_tempco 会把温度噪声
     * 注入角速率（yaw 是它的纯积分，噪声会累积）。0 视为未填、用缺省值。
     * bias_tempco 全为 0 时本项无影响。 */
    float temp_lpf_alpha;

    /* ---- VOFA 调试输出 ----
     * 1 = 每次 Update 把姿态/gyro/acc/dt/yaw_rate/温度/零偏写到 VOFA 通道并**自己
     * 发帧**（通道表见头注释"VOFA 调试输出"一节）；0（默认）= 完全不碰 VOFA，
     * 要画什么由 app 自己 SetChannel + VofaSend。
     * ⚠ 本项为 1 时帧由本模块发出，app 就不要再对同一帧调 VofaSend；
     *   若 app 还有自己的通道要发，保持本项为 0、统一在 app 侧发。 */
    uint8_t vofa_enable;
} BMI088Kalman_Config_s;

/*============================ 输出数据结构 ============================*/

/**
 * @brief 融合输出
 */
typedef struct
{
    euler_t euler;         /* 姿态 (rad)：roll/pitch 来自 KF，yaw 为世界系积分值，wrap 到 (-π, π] */
    float gyro[3];         /* 已按标定修正的角速度 (rad/s)，机体系 */
    float acc[3];          /* 已按标定修正的加速度 (m/s²) */
    float yaw_rate;        /* 世界系 yaw 角速度 (rad/s)（即 ψ̇，已扣零偏） */
    float bias[3];         /* 本帧生效的陀螺零偏 (rad/s)，= 标定值 + 温度系数·ΔT */
    float dt;              /* 本帧积分步长 (s)：0 = 本帧没有新陀螺样本、未积分
                            * （任务比陀螺 ODR 快时必然出现，属正常） */
    float temperature;     /* 原始温度 (℃)，不可用为 NAN（补偿用的是它滤过之后的版本） */
    uint64_t time_stamp_g; /* 本帧陀螺时间戳 (us)，0 = 尚无数据 */
    uint8_t valid;         /* 1 = 姿态可用（已用加速度计播种）；dt==0 的帧不清零，*/
                           /*     euler 仍是最后一次积分的结果             */
} BMI088Kalman_Data_t;

/*============================ 实例结构体 ============================*/

/**
 * @brief 融合实例
 * @note 实例里只有姿态/滤波器状态，标定值是 config 里 const 结构体的拷贝
 *       （拷贝换取调用方不必保证指针长期有效；源头仍在 flash 里）
 */
typedef struct BMI088KalmanInstance
{
    /* 子模块实例（由 DEF 宏绑定指针） */
    BMI088Instance *imu;      /* drv_bmi088 实例 */
    KalmanInstance *kf_roll;  /* roll 轴 2 状态线性 KF（状态 [倾角, 零偏残差]） */
    KalmanInstance *kf_pitch; /* pitch 轴，同上 */

    /* 标定参数（config 拷贝）与本帧生效零偏 */
    BMI088_AxisCalib_s gyro_calib;
    BMI088_AxisCalib_s acc_calib;
    float temp_ref;
    float gyro_bias[BMI088_AXIS_NUM]; /* 含温漂项 */
    float acc_bias[BMI088_AXIS_NUM];  /* 含温漂项 */

    /* 姿态 */
    euler_t euler;
    uint8_t seeded; /* 是否已用加速度计播种 roll/pitch */
    uint8_t valid;  /* 姿态是否可用 */

    /* dt（由陀螺时间戳差分得到） */
    uint64_t last_gyro_ts;
    float dt;

    /* 温漂补偿用的温度低通 */
    float temp_lpf_alpha;
    float temp_filt;
    uint8_t temp_valid; /* 0 = 还没拿到过有效温度（此时按 ΔT=0 处理） */

    /* config 快照 */
    float q_tilt, q_bias, r_tilt, dt_max;
    uint8_t vofa_enable; /* 1 = 每帧写 VOFA 通道并发送，见 Config 的说明 */

    /* 最近一次输出 */
    BMI088Kalman_Data_t data;
} BMI088KalmanInstance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief 定义融合实例（含内嵌的 drv_bmi088 实例与两个 KF）
 * @param name 实例名
 *
 * @note 内嵌而非外挂 drv 实例：ODR/量程/BW 这些"滤波假设的前置条件"由本模块
 *       统一配置，避免 app 侧两处配置割裂；app 只持有本实例。
 * @note 两个 KF 各 2 状态 / 1 量测 / 1 控制输入，静态 RAM ≈ 2×160B
 *
 * @example BMI088_KALMAN_INSTANCE_DEF(imu);
 */
#define BMI088_KALMAN_INSTANCE_DEF(name)                                                                               \
    BMI088_INSTANCE_DEF(name##_imu);                                                                                   \
    KALMAN_INSTANCE_DEF(name##_kf_roll, 2, 1, 1);                                                                      \
    KALMAN_INSTANCE_DEF(name##_kf_pitch, 2, 1, 1);                                                                     \
    static BMI088KalmanInstance name = {.imu = &name##_imu, .kf_roll = &name##_kf_roll, .kf_pitch = &name##_kf_pitch}

/*============================ 公开接口 ============================*/

/**
 * @brief 注册（只注册子模块，硬件映射与参数在 Config 里给）
 * @param inst 实例指针
 * @return 0 成功，-1 失败
 * @note 调用一次即可；要求在 BMI088KalmanConfig 之前
 */
int8_t BMI088KalmanRegister(BMI088KalmanInstance *inst);

/**
 * @brief 配置（转发 IMU 配置 + 装载标定参数与滤波参数，可重复调用）
 * @param inst 实例指针
 * @param config 配置结构体指针
 * @return 0 成功，-1 失败
 * @note 标定参数在此时整份拷贝进实例并做合法性处理（scale ≤ 0 按 1 处理），
 *       之后不再变化 —— 要改只能重编译
 * @note 重复调用会重置姿态与两个 KF
 */
int8_t BMI088KalmanConfig(BMI088KalmanInstance *inst, const BMI088Kalman_Config_s *config);

/**
 * @brief 更新一帧（读数据 → dt → 标定修正 → KF → yaw 积分）
 * @param inst 实例指针
 * @note 由 app 周期任务调用，调用频率决定采样利用率（本模块自己按陀螺
 *       时间戳差分算 dt，与调用抖动无关）
 * @note 数据未就绪时本帧不积分，输出 data.valid=0、dt=0
 */
void BMI088KalmanUpdate(BMI088KalmanInstance *inst);

/**
 * @brief 取最近一次输出（按值返回，与 drv 层读取接口风格一致）
 * @param inst 实例指针
 * @return 输出数据；inst 为空返回全 0（valid=0）
 */
BMI088Kalman_Data_t BMI088KalmanGetData(const BMI088KalmanInstance *inst);

#endif /* DRVLIB_BMI088_KALMAN_USED */

#endif /* __DRVLIB_BMI088_KALMAN_H */
