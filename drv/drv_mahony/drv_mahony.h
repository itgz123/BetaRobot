/**
 * @file drv_mahony.h
 * @brief Mahony 姿态解算滤波器驱动
 *
 * @note 纯算法模块，无硬件依赖
 * @note 基于 bsp_math 类型封装：quaternion_t / euler_t / vector3_t
 * @note 互补滤波融合加速度计和陀螺仪数据，估计载体姿态
 *
 * @note 使用示例：
 * @code
 *     static MahonyInstance mahony;
 *
 *     Mahony_Init_Config_s cfg = {
 *         .kp = 0.5f,
 *         .ki = 0.0f,
 *     };
 *     MahonyInit(&mahony, &cfg);
 *
 *     // 六轴更新：陀螺仪 (rad/s) + 加速度计 (m/s²) + dt (s)
 *     vector3_t gyro = {gx, gy, gz};
 *     vector3_t acc  = {ax, ay, az};
 *     float dt = 0.001f; // dt 由 APP 层根据 IMU 插值时间戳计算传入
 *     MahonyUpdate(&mahony, gyro, acc, dt);
 *
 *     // 获取姿态
 *     Mahony_Data_t data = MahonyGetData(&mahony);
 *     // data.quat, data.euler.roll, data.euler.pitch, data.euler.yaw
 * @endcode
 */

#ifndef __DRV_MAHONY_H
#define __DRV_MAHONY_H

#include "bsp_math.h"

/*============================ 数据输出结构体 ============================*/

/**
 * @brief Mahony 滤波器输出数据结构体
 */
typedef struct
{
    quaternion_t quat;   /* 姿态四元数                     */
    euler_t euler;       /* 欧拉角 (rad)                  */
    vector3_t gyro_bias; /* 陀螺仪零偏估计 (rad/s)        */
} Mahony_Data_t;

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
 */
typedef struct MahonyInstance
{
    /* 滤波器参数 */
    float kp; /* 比例增益                     */
    float ki; /* 积分增益                     */
    /* 状态 */
    quaternion_t quat;     /* 姿态四元数                   */
    vector3_t integral_fb; /* 积分误差累积 (rad/s)         */
} MahonyInstance;

/*============================ 公开接口声明 ============================*/
#define MAHONY_INSTANCE_DEF(name) \
    static MahonyInstance name = {0}

/**
 * @brief 初始化 Mahony 滤波器
 * @param inst   Mahony 实例指针
 * @param config 初始化配置结构体指针
 *
 * @note 参数推荐：
 *       - Kp：六轴 0.5~1.0，九轴 0.5~2.0
 *       - Ki：不需要零偏估计时设为 0.0f；需要时建议 0.01~0.1
 */
void MahonyInit(MahonyInstance *inst, const Mahony_Init_Config_s *config);

/**
 * @brief Mahony 滤波器更新（六轴：陀螺仪 + 加速度计）
 * @param inst Mahony 实例指针
 * @param gyro 陀螺仪数据 (rad/s)
 * @param acc  加速度计数据 (m/s²)
 * @param dt   距上次更新的时间间隔 (s)，由调用方（APP层）根据 BMI088 插值时间戳计算传入
 *
 * @note 加速度计校正横滚角和俯仰角，偏航角会随时间漂移
 * @note 当合加速度远离 1g 时跳过加速度计校正
 */
void MahonyUpdate(MahonyInstance *inst, vector3_t gyro, vector3_t acc, float dt);

/**
 * @brief Mahony 滤波器更新（九轴：陀螺仪 + 加速度计 + 磁力计）
 * @param inst Mahony 实例指针
 * @param gyro 陀螺仪数据 (rad/s)
 * @param acc  加速度计数据 (m/s²)
 * @param mag  磁力计数据（需要已完成硬铁/软铁校正）
 * @param dt   距上次更新的时间间隔 (s)，由调用方传入
 *
 * @note 磁力计校正偏航角漂移
 */
void MahonyUpdateMag(MahonyInstance *inst, vector3_t gyro, vector3_t acc, vector3_t mag, float dt);

/**
 * @brief 重置 Mahony 滤波器状态
 * @param inst Mahony 实例指针
 *
 * @note 四元数重置为单位四元数，清零积分误差
 * @note 不清除滤波器参数 kp / ki
 */
void MahonyReset(MahonyInstance *inst);

#endif /* __DRV_MAHONY_H */
