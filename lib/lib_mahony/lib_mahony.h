/**
 * @file lib_mahony.h
 * @brief 通用 Mahony 互补滤波内核（SO(3) 姿态；纯算法，无器件 / 时间依赖）
 *
 * @note 基于 lib_math 类型封装：quaternion_t / euler_t / vector3_t
 *
 * ═══════════════════ 分层定位：内核 vs 模型 ═══════════════════
 * 本模块是**通用内核**：它不认识加速度计、不认识重力方向、不认识"1g 门限"，
 * 只做姿态估计里纯数学的那一步：
 *     ω_corrected = ω + kp·err + ki·∫err·dt     （err 取半后，见下）
 *     q̇ = ½·q⊗ω_corrected                       （一阶积分 + 归一化）
 * 参考误差 err 由**调用方**给出 —— 谁持有"量测是什么、参考方向是什么、本帧量测
 * 可不可信"这些模型知识，谁就来算 err（器件层，见 drvlib_bmi088_mahony）。
 * 分工与 lib_kf 一致：lib_kf 只解卡尔曼流程，模型矩阵全由调用方给。
 *
 * ═══════════════════ 参数分两处，不要混 ═══════════════════
 *   - kp / ki：每个实例自己的整定参数，走 Mahony_Init_Config_s（运行期可换）
 *   - MAHONY_DT_MIN：数值保护常量，在 lib_mahony.c 里以 #ifndef 方式定义，
 *     可在 app_cfg.h 或编译选项里覆盖，详见该文件"内部常量"一节
 *
 * ═══════════════════ err 的定义与由来 ═══════════════════
 * err = 「机体系量测向量 × 机体系估计向量」，两者均为**单位向量**时模长恰为
 * sin(夹角)，方向垂直于误差旋转轴。用 MahonyErr() 算最省事（它负责把世界系
 * 参考方向投到机体系、再按正确顺序叉乘）；自己算也行，但**叉乘顺序不能反**
 * （量测在前）—— 旧实现里这一处约定写错过，滤波器会朝反方向收敛。
 * 零向量表示"本帧没有可信的校正源"，内核退化为纯陀螺积分。
 *
 * 内核内部把 err 再乘 ½ 后才进 PI（那 ½ 来自 q̇ = ½·q⊗ω），所以 kp / ki 的口径
 * 与传统嵌入式 Mahony 实现一致，六轴推荐 kp 0.5~1.0、不需要零偏估计时 ki = 0.0f
 * （ki > 0 才累加积分项；六轴下 acc 对 yaw 无观测，开着可能积出一个假零偏）。
 *
 * ═══════════════════ 使用示例（六轴：陀螺 + 加速度计）═══════════════════
 * @code
 *     MAHONY_INSTANCE_DEF(mahony);               // static MahonyInstance，需周期前定义
 *     Mahony_Init_Config_s cfg = { .kp = 1.0f, .ki = 0.0f };
 *     MahonyInit(&mahony, &cfg);
 *
 *     // 周期任务里：模型在调用方 —— 判有效性、归一化、选参考方向
 *     vector3_t zero = {0.0f, 0.0f, 0.0f};
 *     vector3_t gyro = {gx, gy, gz};             // rad/s，机体系，已扣零偏
 *     vector3_t acc  = {ax, ay, az};             // m/s²
 *     float    len   = Lib_Math_Vec3Length(acc);
 *     uint8_t  ok    = Lib_Math_Fabs(len - 9.80665f) < 0.3f * 9.80665f;
 *     vector3_t meas = Lib_Math_Vec3Scale(acc, 1.0f / len);   // 单位向量
 *     vector3_t ref  = {0.0f, 0.0f, 1.0f};       // 世界系重力方向
 *     vector3_t err  = ok ? MahonyErr(&mahony, meas, ref) : zero;
 *
 *     MahonyUpdate(&mahony, gyro, err, dt);      // dt 由 APP 层按 IMU 时间戳算
 *     // 姿态读 mahony.quat（原生状态，公开可读；单位四元数）
 * @endcode
 *
 * @note 九轴（带磁力计）**不需要单独接口**：对第二个参考再算一次 err 相加即可，
 *       与旧 MahonyUpdateMag 内部把磁误差累加进同一 PI 的做法等价：
 * @code
 *     vector3_t e1 = MahonyErr(&mahony, acc_unit, grav_ref);   // 重力对
 *     vector3_t e2 = MahonyErr(&mahony, mag_unit, mag_ref);    // 磁对
 *     MahonyUpdate(&mahony, gyro, Lib_Math_Vec3Add(e1, e2), dt);
 * @endcode
 *       磁力计的硬铁 / 软铁校正必须由调用方完成 —— 内核不做任何标定。
 *
 * @note 六轴下 yaw 不可观测：重力方向不含航向信息，误差向量恒垂直于航向轴，
 *       所以 yaw 精度只取决于陀螺零偏准不准，调 kp / ki 救不了（ki 的 z 分量
 *       还会在机动中积出假零偏，故默认建议 ki = 0）。
 */

#ifndef __LIB_MAHONY_H
#define __LIB_MAHONY_H

#include "lib_math.h"

/*============================ 配置结构体 ============================*/

/**
 * @brief Mahony 滤波器初始化配置结构体
 */
typedef struct
{
    float kp; /* 比例增益，用于加速度计/磁力计校正陀螺仪积分漂移 */
    float ki; /* 积分增益，用于陀螺仪零偏估计                   */
} Mahony_Init_Config_s;

/*============================ 实例结构体 ============================*/

/**
 * @brief Mahony 滤波器实例结构体
 * @note 纯算法模块，无 BSP 依赖
 * @note 状态字段公开可读写（同 lib_kf 的 KF_X 宏），便于调试与调用方取姿态
 */
typedef struct MahonyInstance
{
    /* 滤波器参数 */
    float kp; /* 比例增益                     */
    float ki; /* 积分增益                     */
    /* 状态 */
    quaternion_t quat;     /* 姿态四元数（单位四元数）      */
    vector3_t integral_fb; /* 积分误差累积 (rad/s)         */
} MahonyInstance;

/*============================ 公开接口声明 ============================*/
#define MAHONY_INSTANCE_DEF(name) static MahonyInstance name = {0}

/**
 * @brief 初始化 Mahony 滤波器
 * @param inst   Mahony 实例指针
 * @param config 初始化配置结构体指针
 *
 * @note 参数推荐：
 *       - kp：六轴 0.5~1.0，九轴 0.5~2.0
 *       - ki：不需要零偏估计时设为 0.0f；需要时建议 0.01~0.1
 */
void MahonyInit(MahonyInstance *inst, const Mahony_Init_Config_s *config);

/**
 * @brief 计算参考误差向量（世界系参考方向 → 机体系估计，再与量测叉乘）
 * @param inst 实例指针（用其中的姿态四元数作估计）
 * @param meas 机体系量测向量，**必须是单位向量**（是否有效、是否归一化由调用方判）
 * @param ref  世界系参考方向（如重力取 {0,0,1}），不要求单位化，但通常是
 * @return err = meas × (R^T·ref)，模长 = sin(夹角)，方向垂直于误差旋转轴
 *
 * @note 没有可信量测时**不要调用**，直接给 MahonyUpdate 传零向量即可
 * @note 多个参考源（重力 + 磁）各自调用后相加，再交给一次 MahonyUpdate
 */
vector3_t MahonyErr(const MahonyInstance *inst, vector3_t meas, vector3_t ref);

/**
 * @brief Mahony 滤波器更新（PI 校正 + 四元数积分 + 归一化）
 * @param inst 实例指针
 * @param gyro 机体系角速率 (rad/s)，已扣零偏
 * @param err  参考误差向量（MahonyErr 的输出）；零向量 = 本帧无校正，纯陀螺积分
 * @param dt   距上次更新的时间间隔 (s)，由调用方（APP 层）按 IMU 时间戳计算传入
 *
 * @note dt < MAHONY_DT_MIN 时本帧直接跳过（防零步长 / 重复积分同一采样）
 * @note 调用频率与姿态解算无关：内核不感知时间，dt 由调用方给
 */
void MahonyUpdate(MahonyInstance *inst, vector3_t gyro, vector3_t err, float dt);

/**
 * @brief 重置 Mahony 滤波器状态
 * @param inst Mahony 实例指针
 *
 * @note 四元数重置为单位四元数，清零积分误差
 * @note 不清除滤波器参数 kp / ki
 */
void MahonyReset(MahonyInstance *inst);

#endif /* __LIB_MAHONY_H */
