/**
 * @file lib_imu_transform.h
 * @brief IMU 原始量从 IMU 系变换到目标系，并把加速度计杆臂补偿到目标原点
 *
 * @note 只做两件事：
 *       1. 陀螺仪纯旋转：角速度是自由矢量，无杆臂项；
 *       2. 加速度计旋转 + 杆臂补偿：把 IMU 安装点处的比力换算到目标系原点。
 *       不负责姿态估计、重力扣除、位置/速度积分、acc 低通与采样调度。
 *
 * 坐标与符号约定：
 *   - rot = R_{T<-I}，v_T = R · v_I；由三列组成，第 1~3 列分别是
 *     IMU 的 x/y/z 轴在目标系中的单位方向（与 robot_def.h 的安装宏同构）。
 *   - pos = r = p_IMU - p_O，目标系下 IMU 原点相对目标原点的位置 (m)。
 *     ⚠ 直接使用现有 4x4 齐次变换的平移 t，不要取负。
 *   - omega_T = R · omega_I
 *   - alpha 流程（顺序不可交换）：先旋转 → 差分 → 逐轴限幅 → 一阶低通
 *     k = dt / (rc + dt)
 *   - acc_out = R · acc_in - alpha × r - omega_T × (omega_T × r)
 *     比力与惯性加速度的杆臂项相同，故无需扣重力。
 *
 * 适用域：IMU 与目标系刚性固定（R 不随时间变化）。若两者存在相对运动，
 * 本 lib 不包含相应的附加项与 R 导数修正。
 *
 * 时间：dt 由调用方传入（秒）；dt == 0 表示本拍没有新陀螺样本，
 *       输出仍按当前样本做变换，但 alpha 保持上次值。
 *
 * 编译开关：
 *   固件在 app_cfg.h 定义 LIB_IMU_TRANSFORM_USED；
 *   PC 单测用 -DLIB_IMU_TRANSFORM_USED -DLIB_IMU_TRANSFORM_STANDALONE。
 */

#ifndef __LIB_IMU_TRANSFORM_H
#define __LIB_IMU_TRANSFORM_H

#include <stdint.h>

#include "lib_math.h" /* vector3_t / matrix3_t / Lib_Math_* */

typedef enum : int8_t
{
    LIB_IMU_TRANSFORM_OK = 0,
    LIB_IMU_TRANSFORM_ERR_ARG = -1,        /* 空指针 / 配置非法 / dt < 0 */
    LIB_IMU_TRANSFORM_ERR_NOT_INIT = -2,   /* 未 Init 就 Update */
    LIB_IMU_TRANSFORM_ERR_NOT_FINITE = -3, /* 运行时输入含 NaN/Inf */
} LibImuTransform_Status_e;

typedef struct
{
    vector3_t pos;      /* r = p_IMU - p_O，目标系 (m) */
    vector3_t x_axis;   /* IMU x 轴在目标系的单位方向 */
    vector3_t y_axis;   /* IMU y 轴在目标系的单位方向 */
    vector3_t z_axis;   /* IMU z 轴在目标系的单位方向 */
    float alpha_lpf_rc; /* 角加速度一阶低通 RC (s)；0 = 不滤波 */
    float alpha_limit;  /* 角加速度逐轴限幅 (rad/s²)；0 = 不限幅 */
} LibImuTransform_Config_s;

typedef struct
{
    vector3_t gyro; /* IMU 系角速度 (rad/s) */
    vector3_t acc;  /* IMU 系比力 (m/s²)，未扣重力 */
    float dt;       /* 新样本间隔 (s)；>0 = 有新样本，0 = 保持 alpha，<0 非法 */
    uint8_t valid;  /* 1 = 本样本可信 */
} LibImuTransform_Input_s;

typedef struct
{
    vector3_t gyro;      /* 目标系角速度 (rad/s) */
    vector3_t acc;       /* 目标系比力，已杆臂补偿到目标原点 (m/s²) */
    vector3_t alpha;     /* 目标系角加速度，已限幅+低通 (rad/s²) */
    vector3_t pos;       /* 配置 pos 回显 (m) */
    uint8_t valid;       /* 输入有效 && 已 Init && 输入有限 */
    uint8_t fresh;       /* valid && dt > 0：本拍消费到新样本 */
    uint8_t alpha_valid; /* 1 = 自 Reset 后至少完成过一次 alpha 更新 */
} LibImuTransform_Output_s;

typedef struct
{
    LibImuTransform_Config_s cfg;
    matrix3_t rot;           /* 由三轴构造的 R_{T<-I}，供外部复用（如去重力） */
    vector3_t gyro_prev;     /* 上一次目标系角速度 */
    vector3_t alpha_lpf;     /* 低通后的目标系角加速度 */
    uint8_t gyro_prev_valid; /* 0 = 还没有上一样本（首拍/复位后） */
    uint8_t alpha_valid;     /* 自 Reset 后是否完成过 alpha 更新 */
    uint8_t inited;
} LibImuTransform_Instance_s;

/**
 * @brief 初始化实例
 * @param inst 实例
 * @param cfg  配置（pos + 三个轴方向 + alpha 滤波/限幅参数）
 * @return LIB_IMU_TRANSFORM_OK 成功；其余见 LibImuTransform_Status_e
 * @note 三轴全零视为单位阵（"不修正"）；否则各轴先归一化，再校验两两正交
 *       与右手性（det ≈ +1）。校验失败不修改实例。
 * @note alpha_limit == 0 表示不限幅，alpha_lpf_rc == 0 表示不滤波。
 */
LibImuTransform_Status_e LibImuTransformInit(LibImuTransform_Instance_s *inst, const LibImuTransform_Config_s *cfg);

/**
 * @brief 复位 alpha 差分/滤波状态（保留配置）
 */
void LibImuTransformReset(LibImuTransform_Instance_s *inst);

/**
 * @brief 每拍更新：IMU 系 → 目标系，并对加速度计做杆臂补偿
 * @param inst 实例（必须已 Init）
 * @param in   IMU 原始量（IMU 系）
 * @param out  目标系结果（错误时清零）
 * @return OK 或错误码
 * @note 只有 dt > 0 才更新 alpha；首次 dt > 0 仅播种 gyro_prev。
 * @note valid == 0 时仍按 dt > 0 更新 alpha（与现有底盘反馈行为一致），
 *       valid 只影响输出标志。
 */
LibImuTransform_Status_e LibImuTransformUpdate(LibImuTransform_Instance_s *inst, const LibImuTransform_Input_s *in, LibImuTransform_Output_s *out);

#endif /* __LIB_IMU_TRANSFORM_H */
