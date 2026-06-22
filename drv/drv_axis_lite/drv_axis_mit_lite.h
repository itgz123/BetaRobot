/**
 * @file drv_axis_mit_lite.h
 * @brief 轻量级单轴关节控制驱动模块
 * @author TRW
 * @date 2026-06-07
 *
 * @note 控制算法: tau = kp*(ref_pos-pos) + kd*(ref_vel-vel) + tau_gravity + tau_friction + tau_inertia
 * @note 开发阶段:
 *       step1: 固定力矩，计算重力矩，确定控制和反馈方向
 *       step2: 重力基础上添加多频率正弦波（变频），辨识惯量、摩擦，校准重力
 *       step3: 固定频率正弦波位置设定值，kp,kd设置为0，检查辨识效果
 *       step4: 先调kd，再调kp，需要抗外界干扰（kp不能单独非零）
 *       step5: 正常控制
 * @note 这个文件轻量封装，只操作MotorVTable_s中的setref函数，其他的app层手动调用
 * @note drv\drv_axis_lite\identify_axis_mit_lite.py可以辨识参数
 */

#ifndef DRV_AXIS_MIT_LITE_H
#define DRV_AXIS_MIT_LITE_H

#include "drv_motor_def.h"
#include "drv_mit.h"
#include "drv_axis_lite_def.h"
#include <stdint.h>
#include "app_cfg.h"

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    // 电机基类要求：
    // 1. MotorLoopType_e=loop_type
    // 2. position_mode = MOTOR_POSITION_WRAP/MOTOR_POSITION_LIMITED
    MotorBase_s *motor;                 // 电机基类
    AxisLiteStage_e stage;              // 控制阶段
    AxisLiteParams_s params;            // 轴参数
    ChirpParam_s chirp_params;          // 扫频参数
    SineParam_s sine_params;            // 正弦参数
    MultiSineParam_s multi_sine_params; // 多正弦叠加参数
    MITInstance mit;                    // MIT控制器实例
    float *ref_position;                // 外部位置设定 (rad)
    float *ref_speed;                   // 外部速度设定 (rad/s)
    float *ref_acceleration;            // 外部加速度设定 (rad/s²)
    uint32_t delay_ms;                  // 延时时间 (ms)
    uint64_t init_time_us;              // 初始化时间戳 (us)
    uint8_t init_flag;                  // init_time_us 初始化标志，0=未初始化
} AxisMitLiteInstance;

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    MotorBase_s *motor;                 // 电机基类
    AxisLiteStage_e stage;              // 控制阶段
    uint32_t delay_ms;                  // 延时时间 (ms)
    AxisLiteParams_s params;            // 轴参数
    SineParam_s sine_params;            // 正弦参数
    ChirpParam_s chirp_params;          // 扫频参数
    MultiSineParam_s multi_sine_params; // 多正弦叠加参数
    // MIT控制器参数
    float kp; // 位置增益 (Nm/rad)
    float kd; // 速度增益
} AxisMitLite_Init_Config_s;

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化轴控制实例
 * @param inst 实例指针
 * @param cfg 配置结构体指针
 * @return 0: 成功, -1: 失败
 */
int8_t AxisMitLiteInit(AxisMitLiteInstance *inst, AxisMitLite_Init_Config_s *cfg);

/**
 * @brief 计算控制输出并调用MotorSetRef
 * @param inst 实例指针
 * @note setref =（重力前馈 + 惯量前馈 + 摩擦前馈） + （kp * 位置误差 + kd * 速度误差）
 * @note 启用 AxisMitVofaLiteSetChannelUsed 时，自动设定 12 个 VOFA 调试通道：
 *       CH1-CH3: 传感器测量 (独立物理量)
 *       ch1:  position       — 反馈位置 (rad)
 *       ch2:  speed          — 反馈速度 (rad/s)
 *       ch3:  MotorGetCurrent — 电机实际电流
 *
 *       CH4-CH6: 设定值
 *       ch4:  ref_pos        — 位置设定值 (rad)
 *       ch5:  ref_vel        — 速度设定值 (rad/s)
 *       ch6:  ref_acc        — 加速度设定值 (rad/s^2)
 *
 *       CH7-CH9: 前馈分量
 *       ch7:  gravity_ff     — 重力前馈 (Nm)
 *       ch8:  inertia_ff     — 惯量前馈 (Nm)
 *       ch9:  friction_ff    — 摩擦前馈 / chirp (Nm)
 *
 *       CH10-CH11: MIT 控制器 PD 输出
 *       ch10: pos_output     — 位置环输出 (Nm)
 *       ch11: speed_output   — 速度环输出 (Nm)
 *
 *       ch12: setref值，最终发送给电机的电流/力矩值
 */
void AxisMitLiteCalculate(AxisMitLiteInstance *inst);

#endif // !DRV_AXIS_MIT_LITE_H
