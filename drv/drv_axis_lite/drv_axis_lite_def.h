/**
 * @file drv_axis_mit_lite.h
 * @brief 轻量级单轴关节控制驱动模块实现
 * @author TRW
 * @date 2026-06-07
 *
 * @note 考虑：重力(实际位置)；惯量(参考加速度)；双向库伦摩擦(实际速度)；双向粘性摩擦(实际速度)。
 */
#ifndef DRV_AXIS_LITE_DEF_H
#define DRV_AXIS_LITE_DEF_H

#include <stdint.h>

/*============================================
 *              控制阶段枚举
 *============================================*/
typedef enum : uint8_t
{
    AXIS_LITE_STAGE_FIXED_TORQUE = 0, // step1: 给定重力矩（计算重力矩，确定控制和反馈方向）
    AXIS_LITE_STAGE_IDENTIFY,         // step2: 给定重力矩+变频正弦力矩（频域法辨识惯量、双向摩擦）
    AXIS_LITE_STAGE_IDENTIFY_OLS,     // step3: 给定重力矩+多正弦力矩（时域正交可分离最小二乘法辨识惯量、双向摩擦）
    // step4的闭环设定值给正弦加速度和微分速度和位置；step5的闭环设定值给实际设定值。
    AXIS_LITE_STAGE_TUNE,   // step4: 给定重力矩+惯量力矩+摩擦力矩+闭环力矩（调闭环参数）
    AXIS_LITE_STAGE_NORMAL, // step5: 给定重力矩+惯量力矩+摩擦力矩+闭环力矩（正常控制）
} AxisLiteStage_e;

/*============================================
 *              轴参数结构体
 *============================================*/
typedef struct
{
    float torque_coeff;         // 有的电机setref是电流，有的是扭矩，有的有减速箱。这个参数自己看着填。
    float gravity;              // 重力矩 (Nm)
    float inertia;              // 转动惯量 (kg·m²)
    float friction_coulomb_pos; // 正向库仑摩擦 (Nm)
    float friction_coulomb_neg; // 负向库仑摩擦 (Nm)
    float friction_viscous_pos; // 正向粘性摩擦系数 (Nm·s/rad)
    float friction_viscous_neg; // 负向粘性摩擦系数 (Nm·s/rad)
    // 调试参数
    float gravity_ff;  // 重力前馈
    float inertia_ff;  // 惯量前馈
    float friction_ff; // 摩擦前馈
    float total_ff;    // 总前馈
} AxisLiteParams_s;

/*============================================
 *              扫频参数结构体
 *============================================*/
typedef struct
{
    float start_freq;      // 起始频率 (Hz)
    float end_freq;        // 结束频率 (Hz)
    float amplitude_start; // 起始力矩振幅 (Nm)
    float amplitude_end;   // 终止力矩振幅 (Nm)
    float duration;        // 扫频时长 (s)
} ChirpParam_s;

/*============================================
 *              正弦参数结构体
 *============================================*/
typedef struct
{
    float freq;      // 频率 (Hz)
    float amplitude; // 位置振幅 (rad)
} SineParam_s;

/*============================================
 *              多正弦叠加参数结构体
 *============================================*/
#define AXIS_LITE_MULTI_SINE_MAX_FREQS 8 // 最大正弦频率数量

typedef struct
{
    float freqs[AXIS_LITE_MULTI_SINE_MAX_FREQS];      // 频率数组 (Hz)
    float amplitudes[AXIS_LITE_MULTI_SINE_MAX_FREQS]; // 振幅数组 (Nm)
    uint8_t num_freqs;                                // 正弦波数量
    float duration;                                   // 总时长 (s)
} MultiSineParam_s;

#endif // !DRV_AXIS_LITE_DEF_H
