/**
 * @file drvlib_bmi088_mahony.h
 * @brief BMI088 + Mahony 互补滤波：通信、标定参数装载、姿态解算一体
 *
 * ═══════════════════ 分层定位 ═══════════════════
 *   drv_bmi088 : 只做通信（寄存器/初始化/物理量换算/时间戳/温度）
 *   lib_mahony : 通用互补滤波内核（PI + 四元数积分，不认器件也不认重力）
 *   本模块     : 器件 + 姿态模型 —— 读通信层数据、按标定修正、算参考误差、
 *                跑内核、出姿态
 *   对上（app）: 只暴露 姿态 / 四元数 / 角速度 / 零偏 / 温度 / 数据有效性
 *
 * ⚠ "姿态模型"指：哪个向量是参考方向（世界系 +Z 重力）、本帧量测可不可信
 *   （|acc| 门限）、量测怎么归一化。内核只要一个"机体系参考误差向量"，
 *   这些判断全在本模块 —— 与 lib_kf / drvlib_bmi088_kalman 的分工一一对应
 *   （lib_kf 只解卡尔曼流程，模型矩阵与门限都在那边模块里）。
 *
 * 与 drvlib_bmi088_kalman 是**同一件事的两种实现**（二选一，不必同时开）：
 *   - 输出数据字段同名同义（euler/gyro/acc/yaw_rate/bias/dt/temperature/valid），
 *     本模块额外多一个 quat。app 侧换模块时基本只需改实例定义与 Update/GetData 名。
 *   - 标定结构体、修正函数、温度补偿、dt 来源、播种方式、yaw_rate 算法**完全共用**，
 *     唯一的差别是 roll/pitch 用什么去纠陀螺积分：
 *       kalman : 2 状态线性 KF（状态 [倾角, 零偏残差]），欧拉角表示
 *       mahony : 显式互补滤波（PI 反馈到角速率），四元数表示
 *
 * ═══════════════════ 为什么用四元数（与 KF 版的取舍）═══════════════════
 * KF 版姿态用欧拉角，欧拉角运动学的分母有 cosθ，θ → ±90° 是奇点，只能靠钳位
 * （|cosθ| ≥ 0.1，约 ±84°）兜住。本模块内部全程四元数，**没有万向锁**，
 * pitch 可以真正跑到 ±90°：若将来把 IMU 装到会大角度俯仰的关节下游（比如
 * pitch 轴之后的下游板），应该选本模块而不是 KF 版。
 * ⚠ 但对外暴露的 yaw_rate 仍按欧拉运动学算（为了与 KF 版输出可比），
 *   那一项在 |pitch| → 90° 附近仍会失真。
 *
 * ═══════════════════ 标定策略：全部在 PC 端离线完成 ═══════════════════
 * 与 KF 版一字不差：**不做任何运行期标定**。没有静止检测、没有自学习、没有标定
 * 状态机、没有 RAM 标定值。上场前用 PC（python）标定，结果写进 app 里的一个
 * static const BMI088_Calib_s，随固件落进 .rodata —— 即"写死到固件" ——
 * Config 时整份交给本模块。换 IMU / 拆装 / 改安装方式后必须重新标定并重新烧录。
 *
 * 误差项与修正模型（与 KF 版共用 drvlib_bmi088_calib.h，逐项一致）：
 *   陀螺/acc 各 4 项：零偏 bias、尺度 scale、非正交 misalign、零偏温度系数 bias_tempco
 *   m = M·(S⊙t) + b   ⇒   t = S⁻¹ ⊙ (M⁻¹·(m - b))
 *   顺序：先扣零偏 → 再去非正交 → 最后除尺度。
 * 未标定的项保持零值即可（scale 填 0 与填 1 等价），零初始化结构体 == 完全不修正。
 *
 * 可标定项与"没有转台/温箱"时各自的可得性：
 *   ✅ 陀螺零偏          静止均值
 *   ✅ 陀螺零偏温漂斜率  自热扫温（上电静置数十分钟，记温度与零偏），不需温箱
 *   ⚠️ 陀螺尺度/非正交   需已知角速率参考：用云台 yaw 轴自转 + 编码器即可，不需转台
 *   ⚠️ acc 零偏/尺度/非正交  需六位置夹具。注意**绕竖直轴转动对 acc 完全无用**
 *      （重力在 IMU 系的投影不随绕 Z 旋转改变），要标 acc 必须改变它的姿态
 *   ❌ 非线性/迟滞、g 敏感、饱和、杆臂、带宽/延迟 —— 需转台/温箱/离心机，暂不做
 *
 * ═══════════════════ 姿态与滤波结构 ═══════════════════
 * lib_mahony 六轴互补滤波：用加速度计反算的重力方向去纠陀螺积分，PI 形式的
 * 反馈量直接加到角速率上：
 *   误差 = 机体系重力估计 × 机体系重力实测（叉积，roll/pitch 误差）
 *   ω_corrected = ω - kp·误差 - ki·∫误差           （反馈是负号）
 * 四元数用一阶 RK 积分 + 归一化。
 *
 * kp / ki 的选取：
 *   kp 决定"跟陀螺还是跟加速度计"：大 → 更快收敛到加速度计、抗漂移、但把加速度
 *   引入的线性加速度 / 振动当成倾角误差吃进去；小 → 更信陀螺、抗扰动、但漂移慢。
 *   六轴推荐 0.5~1.0（lib_mahony 头注释），本模块缺省 1.0（与旧 app 代码一致）。
 *   ki **建议保持 0**：ki 的作用是估计陀螺零偏，但六轴模态下 acc 对 yaw 没有任何
 *   观测能力，积分项的 z 分量没有可信的校正源，只会在机动中缓慢累积成一个假零偏；
 *   而零偏真正的值我们已经用 PC 标定精确测出并硬编码，不需要滤波器再猜。
 *   （这就是 lib_mahony 建议"不需要零偏估计时设为 0.0f"的场景；与 lib_pid 的
 *   integral_limit=0 同类：**0 是合法取值，不是漏填**，故 ki 不做缺省回填。）
 *   若确实要开 ki（比如想跟踪上电零偏重复性），注意到 acc 门限不通过时误差为 0、
 *   积分随之停走，机动期间不会累积 —— 但 z 分量的假零偏风险仍在，需实测确认。
 *
 * yaw **没有任何观测**：六轴下重力方向不含航向信息，互补滤波的校正量对 yaw 恒为
 * 0（误差向量沿水平轴）。yaw 精度只取决于零偏准不准 + 标定做得好不好，滤波器
 * 调参救不了 —— 这条与 KF 版完全相同。
 *
 * ⚠ 加速度计门限在本模块（BMI088_MAHONY_ACC_NORM_TOL，|acc| 偏离 1g 超过 30%）
 *   而不是内核里：内核拿到零向量误差就退化成纯陀螺积分，这正是本模块判完门限后
 *   要做的事。绝对量约 ±2.94 m/s²，与 KF 版的 ACC_REJECT 3.0 同量级；播种那一步
 *   另有一个更严的判据（BMI088_MAHONY_SEED_ACC_TOL），两者互相独立。
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
 *   CH16-19: 姿态四元数 w/x/y/z —— 本模块的原生状态，**仅当 VOFA_CHANNELS ≥ 19
 *            时才写出**（drv_vofa 的通道数是编译期常量，缺省 15，多出来的通道
 *            会被 VofaSetChannel 静默丢弃）。要看四元数就在 app_cfg.h 里写一句
 *            `#define VOFA_CHANNELS 19`；只看姿态的话 CH1-3 已经够了。
 * CH1-15 与 drvlib_bmi088_kalman 的通道表**逐通道相同**（同一件事的两种实现，
 * 上位机的图不用重配），换模块时只需把 vofa_enable 挪过去。
 *   - 发送与任务同频（2ms 任务 = 500 帧/s），写帧/发帧都在 Update 里；
 *     dt==0 的帧照样发（那正是要看的东西），姿态量保持上一帧的值。
 *   - drv_vofa 是三缓冲 DMA + 忙则跳过，波特率不够时自然丢帧、不阻塞控制任务。
 *   - VOFA_USED / VOFA_UART 未定义时所有 Vofa* 是空实现，本项零开销、无副作用。
 * ⚠ 通道号是全驱动唯一的一组：drv_axis_mit_lite 的 vofa_enable 占 CH1~CH12、
 *   app_shoot 的调试块占 CH1~CH3，都与本模块的 CH1~CH19 重叠。同一时刻只能有
 *   一路在写（VofaSend 发的是缓冲区里**所有**通道当下的值，混写会互相覆盖），
 *   要同时看就把别的模块的通道号错开。
 *
 * ═══════════════════ 使用顺序 ═══════════════════
 * @code
 *     BMI088_MAHONY_INSTANCE_DEF(bmi088);
 *
 *     // PC 端标定结果，写死进固件（static const → .rodata）
 *     // 注意：与 KF 版共用同一份类型，两个模块可以直接喂同一个常量表
 *     static const BMI088_Calib_s s_bmi088_calib = {
 *         .gyro = { .bias = {0.00089f, -0.00011f, 0.00169f} },
 *     };
 *
 *     BMI088MahonyRegister(&bmi088);                  // 注册子模块（一次）
 *     BMI088Mahony_Config_s cfg = {
 *         .imu = { .spi_e = SPI_BMI088, ... },        // 转发给 drv_bmi088
 *         .calib = &s_bmi088_calib,                   // NULL = 完全不修正
 *         .kp = 1.0f,                                 // 0 视为未填，用缺省 1.0
 *         // .ki = 0.0f,                              // 建议保持 0，见上
 *         // .vofa_enable = 1,                        // 1 = 顺便写 VOFA 通道并自动发帧
 *     };
 *     BMI088MahonyConfig(&bmi088, &cfg);              // 配置（可重复调用）
 *
 *     // 周期任务里（本模块按陀螺时间戳差分算 dt，与调用抖动无关）
 *     BMI088MahonyUpdate(&bmi088);
 *     BMI088Mahony_Data_t d = BMI088MahonyGetData(&bmi088);
 *     if (d.valid) { 用 d.euler / d.quat / d.yaw_rate / d.gyro ... }
 * @endcode
 */

#ifndef __DRVLIB_BMI088_MAHONY_H
#define __DRVLIB_BMI088_MAHONY_H

#include "app_cfg.h"

/* 依赖两个被组合的模块：drv 通信层、lib_mahony 算法层（任一未开则本模块整体不编译） */
#if defined(DRVLIB_BMI088_MAHONY_USED) && defined(DRV_BMI088_USED) && defined(LIB_MAHONY_USED)

#include "drv_bmi088.h"
#include "lib_mahony.h"
#include "lib_math.h"
#include "drvlib_bmi088_calib.h" /* 与 drvlib_bmi088_kalman 共用的标定结构与修正函数 */

/*============================ 常量 ============================*/

/* 本模块所有可调参数都是 #ifndef 可覆盖的：在 app_cfg.h（或任何先于本头文件包含的
 * 头文件、编译选项 -D）里定义同名宏即可覆盖，不必改驱动源码。
 * 覆盖点只需早于本文件包含 —— 本 .h 在常量之前就 #include "app_cfg.h"，
 * 所以写在 app_cfg.h 里一定生效。
 * ⚠ 覆盖值要带类型后缀（如 4.0f 而不是 4），否则会在 float 表达式里走 double 运算。 */

/* 欧拉角运动学分母 cosθ 的下限（|cosθ| < 该值即钳位，约 ±84° 奇点保护）。
 * 只用于对外输出的 yaw_rate —— 滤波器内部是四元数，不受此限。 */
#ifndef BMI088_MAHONY_COS_TILT_MIN
#define BMI088_MAHONY_COS_TILT_MIN 0.1f
#endif // !BMI088_MAHONY_COS_TILT_MIN

/* 播种时判定"加速度可信"的 |acc| 容差 (m/s²)：比 lib_mahony 内部的 ±30% 严，
 * 因为播种的结果直接写进姿态（只播种一次）；放宽到 2.0 是为了上电时手在抖也能播上种 */
#ifndef BMI088_MAHONY_SEED_ACC_TOL
#define BMI088_MAHONY_SEED_ACC_TOL 2.0f
#endif // !BMI088_MAHONY_SEED_ACC_TOL

/*============================ 标定结构体 ============================*/

/* 标定参数结构体（BMI088_AxisCalib_s / BMI088_Calib_s）与修正函数
 * （BMI088CalibLoad / BMI088AxisCorrect）是**器件属性**，不是本算法的属性，
 * 因此与 drvlib_bmi088_kalman 共用，定义在 drvlib_bmi088_calib.h。
 * app 里同一份 static const BMI088_Calib_s 可以喂给两个模块。 */

/*============================ 配置结构体 ============================*/

/**
 * @brief drvlib_bmi088_mahony 配置结构体
 */
typedef struct
{
    /* ---- 转发给 drv_bmi088（硬件枚举 + 传感器参数 + daemon） ---- */
    BMI088_Config_s imu;

    /* ---- PC 端标定结果（NULL = 完全不修正）；结构体与 kalman 版共用 ---- */
    const BMI088_Calib_s *calib;

    /* ---- Mahony 互补滤波参数 ---- */
    float kp; /* 比例增益：0 视为未填，回填缺省 1.0 */
    float ki; /* 积分增益：**0 是合法值**（建议保持 0），不做缺省回填 */

    /* ---- 集成参数 ---- */
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
} BMI088Mahony_Config_s;

/*============================ 输出数据结构 ============================*/

/**
 * @brief 融合输出
 * @note 字段与 BMI088Kalman_Data_t 同名同义（多一个 quat），便于两个模块互换
 */
typedef struct
{
    euler_t euler;         /* 姿态 (rad)：roll/pitch 来自互补滤波，yaw 为世界系积分值，wrap 到 (-π, π] */
    quaternion_t quat;     /* 姿态四元数：本模块的原生状态，不受万向锁限制 */
    float gyro[3];         /* 已按标定修正的角速度 (rad/s)，机体系 */
    float acc[3];          /* 已按标定修正的加速度 (m/s²) */
    float yaw_rate;        /* 世界系 yaw 角速度 (rad/s)（即 ψ̇，已扣零偏）；*/
                           /* |pitch| → 90° 附近受欧拉奇点影响，见头注释 */
    float bias[3];         /* 本帧生效的陀螺零偏 (rad/s)，= 标定值 + 温度系数·ΔT */
    float dt;              /* 本帧积分步长 (s)：0 = 本帧没有新陀螺样本、未积分
                            * （任务比陀螺 ODR 快时必然出现，属正常） */
    float temperature;     /* 原始温度 (℃)，不可用为 NAN（补偿用的是它滤过之后的版本） */
    uint64_t time_stamp_g; /* 本帧陀螺时间戳 (us)，0 = 尚无数据 */
    uint8_t valid;         /* 1 = 姿态可用（已用加速度计播种）；dt==0 的帧不清零，*/
                           /*     euler 仍是最后一次积分的结果             */
} BMI088Mahony_Data_t;

/*============================ 实例结构体 ============================*/

/**
 * @brief 融合实例
 * @note 实例里只有姿态/滤波器状态，标定值是 config 里 const 结构体的拷贝
 *       （拷贝换取调用方不必保证指针长期有效；源头仍在 flash 里）
 */
typedef struct BMI088MahonyInstance
{
    /* 子模块实例（由 DEF 宏绑定指针） */
    BMI088Instance *imu;    /* drv_bmi088 实例 */
    MahonyInstance *mahony; /* lib_mahony 实例 */

    /* 标定参数（config 拷贝）与本帧生效零偏 */
    BMI088_AxisCalib_s gyro_calib;
    BMI088_AxisCalib_s acc_calib;
    float temp_ref;
    float gyro_bias[BMI088_AXIS_NUM]; /* 含温漂项 */
    float acc_bias[BMI088_AXIS_NUM];  /* 含温漂项 */

    /* 姿态 */
    euler_t euler;
    uint8_t seeded; /* 是否已用加速度计播种姿态 */
    uint8_t valid;  /* 姿态是否可用 */

    /* dt（由陀螺时间戳差分得到） */
    uint64_t last_gyro_ts;
    float dt;

    /* 温漂补偿用的温度低通 */
    float temp_lpf_alpha;
    float temp_filt;
    uint8_t temp_valid; /* 0 = 还没拿到过有效温度（此时按 ΔT=0 处理） */

    /* config 快照（kp/ki 同时也写进 lib_mahony 实例，这里留一份供自省） */
    float kp, ki, dt_max;
    uint8_t vofa_enable; /* 1 = 每帧写 VOFA 通道并发送，见 Config 的说明 */

    /* 最近一次输出 */
    BMI088Mahony_Data_t data;
} BMI088MahonyInstance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief 定义融合实例（含内嵌的 drv_bmi088 实例与 Mahony 滤波器）
 * @param name 实例名
 *
 * @note 内嵌而非外挂 drv 实例：ODR/量程/BW 这些"滤波假设的前置条件"由本模块
 *       统一配置，避免 app 侧两处配置割裂；app 只持有本实例。
 *
 * @example BMI088_MAHONY_INSTANCE_DEF(imu);
 */
#define BMI088_MAHONY_INSTANCE_DEF(name)                                                                               \
    BMI088_INSTANCE_DEF(name##_imu);                                                                                   \
    MAHONY_INSTANCE_DEF(name##_mahony);                                                                                \
    static BMI088MahonyInstance name = {.imu = &name##_imu, .mahony = &name##_mahony}

/*============================ 公开接口 ============================*/

/**
 * @brief 注册（只注册子模块，硬件映射与参数在 Config 里给）
 * @param inst 实例指针
 * @return 0 成功，-1 失败
 * @note 调用一次即可；要求在 BMI088MahonyConfig 之前
 */
int8_t BMI088MahonyRegister(BMI088MahonyInstance *inst);

/**
 * @brief 配置（转发 IMU 配置 + 装载标定参数与滤波参数，可重复调用）
 * @param inst 实例指针
 * @param config 配置结构体指针
 * @return 0 成功，-1 失败
 * @note 标定参数在此时整份拷贝进实例并做合法性处理（scale ≤ 0 按 1 处理），
 *       之后不再变化 —— 要改只能重编译
 * @note 重复调用会重置姿态与滤波器（四元数回到单位、积分清零）
 */
int8_t BMI088MahonyConfig(BMI088MahonyInstance *inst, const BMI088Mahony_Config_s *config);

/**
 * @brief 更新一帧（读数据 → dt → 标定修正 → 播种 → 互补滤波 → yaw 积分）
 * @param inst 实例指针
 * @note 由 app 周期任务调用，调用频率决定采样利用率（本模块自己按陀螺
 *       时间戳差分算 dt，与调用抖动无关）
 * @note 数据未就绪时本帧不积分，输出 data.valid=0、dt=0
 */
void BMI088MahonyUpdate(BMI088MahonyInstance *inst);

/**
 * @brief 取最近一次输出（按值返回，与 drv 层读取接口风格一致）
 * @param inst 实例指针
 * @return 输出数据；inst 为空返回全 0（valid=0）
 */
BMI088Mahony_Data_t BMI088MahonyGetData(const BMI088MahonyInstance *inst);

#endif /* DRVLIB_BMI088_MAHONY_USED */

#endif /* __DRVLIB_BMI088_MAHONY_H */
