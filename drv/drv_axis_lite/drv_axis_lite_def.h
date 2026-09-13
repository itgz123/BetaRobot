/**
 * @file drv_axis_lite_def.h
 * @brief axis lite 层公共类型（关节控制的输入/输出/参数）
 * @author TRW
 * @date 2026-06-07
 *
 * @note 考虑：重力(实际位置)；惯量(参考加速度)；双向库伦摩擦(实际速度)；双向粘性摩擦(实际速度)。
 *
 * @note axis lite 模块统一约定：只做控制律计算，不持有电机、不调用 Motor* 接口。
 *       反馈由 app 层读好后填进 AxisLiteState_s 传入，输出 setref 由 app 层自行 MotorSetRef 下发。
 */
#ifndef DRV_AXIS_LITE_DEF_H
#define DRV_AXIS_LITE_DEF_H

#include <stdint.h>

/*============================================
 *              输入：关节反馈（电机侧）
 *============================================*/
/**
 * @brief  关节反馈量
 * @note   app 层从电机读出后填入本结构传入，lite 层不依赖 motor 类型。
 *         数值为**电机侧**（减速比之前），与 AxisLiteParams_s.gear_ratio 匹配；
 *         若 app 直接给输出侧数据，请把 gear_ratio 置 1。
 */
typedef struct
{
    float position; // 位置 (rad)，多圈累加值
    float speed;    // 速度 (rad/s)
    float torque;   // 力矩 (Nm)，仅用于 VOFA 观测，不参与控制律
} AxisLiteState_s;

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
    float gear_ratio;           // 减速比 (电机转速 / 输出转速)
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
 *
 * 用于时域正交可分离最小二乘法辨识惯量、双向摩擦。
 * 频率自动设为 f_i = i / duration (i=1..num_freqs)，
 * 正交周期 = duration，满足 ∫₀ᵀ sin(2πfᵢt)·sin(2πfⱼt)dt = 0 (i≠j)。
 * 各频率等幅 excitation，计算复杂度 O(1)（三角恒等封闭形式），
 * num_freqs 增加不额外消耗算力。
 *============================================*/

typedef struct
{
    float amplitude;   // 统一振幅 (Nm)，所有频率等幅
    float duration;    // 正交周期 (s)，频率为 i / duration (i=1..num_freqs)
    uint8_t num_freqs; // 正弦波数量
} MultiSineParam_s;

/*============================================
 *              外部设定值结构体
 *============================================*/
typedef struct
{
    float position;     // 外部位置设定 (rad)
    float speed;        // 外部速度设定 (rad/s)
    float acceleration; // 外部加速度设定 (rad/s²)
} AxisMitLiteRef_s;

#endif // !DRV_AXIS_LITE_DEF_H
