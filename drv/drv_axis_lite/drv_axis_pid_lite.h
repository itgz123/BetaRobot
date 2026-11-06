/**
 * @file drv_axis_pid_lite.h
 * @brief 轻量级单轴关节控制驱动模块（位置 + 速度串级双 PID）
 * @author TRW
 * @date 2026-11-06
 *
 * @note 前馈与 drv_axis_mit_lite 完全一致：
 *       total_ff = gravity * cos(pos) + inertia * ref_acc + friction(actual speed)
 *
 * @note 反馈采用串级双 PID（位置环为外环、速度环为内环）：
 *       vel_set = POS_PID(ref_pos, pos, ref_vel)   // 位置环输出即速度设定值，前馈为设定速度
 *       output  = VEL_PID(vel_set, vel, total_ff)  // 速度环前馈为力矩前馈
 *       其中 dt 由 DWT 自动测量（首次调用 dt=0，可用 dt_max 钳位异常间隔）。
 *
 * @note 与 drv_axis_mit_lite 相同，本文件轻量封装，不依赖 motor：
 *       Calculate 接收 AxisLiteState_s 反馈、计算并返回 setref，由 app 层手动调用 motor.set_ref 设置力矩。
 */

#ifndef DRV_AXIS_PID_LITE_H
#define DRV_AXIS_PID_LITE_H

#include "lib_pid.h"
#include "drv_axis_lite_def.h"
#include <stdint.h>
#include "app_cfg.h"

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    AxisLiteStage_e stage;              // 控制阶段
    AxisLiteParams_s params;            // 轴参数
    ChirpParam_s chirp_params;          // 扫频参数
    SineParam_s sine_params;            // 正弦参数
    MultiSineParam_s multi_sine_params; // 多正弦叠加参数
    PIDInstance pos_pid;                // 位置环 PID（输出 = 速度设定值）
    PIDInstance vel_pid;                // 速度环 PID（输出 = 力矩）
    uint32_t delay_ms;                  // 延时时间 (ms)
    uint64_t init_time_us;              // 初始化时间戳 (us)
    uint64_t last_calc_us;              // 上次计算时间戳 (us)，用于自动计算 dt
    uint8_t init_flag;                  // init_time_us 初始化标志，0=未初始化
    uint8_t calc_flag;                  // last_calc_us 初始化标志，0=首帧 dt=0
    uint8_t pid_seeded;                 // PID 微分状态已播种标志，0=首次闭环需播种
    float dt_max;                       // 单帧 dt 上限 (s)，<=0 不钳位
    uint8_t vofa_enable;                // VOFA 调试输出使能，1=写 12 通道
    uint8_t vofa_base_ch;               // VOFA 起始通道，占用 base_ch ~ base_ch+11，最小为 1
    float tune_base_angle;              // TUNE 阶段正弦参考基准角度 (rad)，延时结束时记录
    uint8_t tune_base_flag;             // 基准角度已记录标志，0=未记录
} AxisPidLiteInstance;

/*============================================
 *              配置结构体
 *============================================*/
typedef struct
{
    AxisLiteStage_e stage;              // 控制阶段
    uint32_t delay_ms;                  // 延时时间 (ms)
    uint8_t vofa_enable;                // VOFA 调试输出使能，1=写 12 通道
    uint8_t vofa_base_ch;               // VOFA 起始通道，占用 base_ch ~ base_ch+11，0 按 1 处理
                                        // 多个轴同时输出时分别设 1、13、25……即可一帧发多轴数据
    AxisLiteParams_s params;            // 轴参数
    SineParam_s sine_params;            // 正弦参数
    ChirpParam_s chirp_params;          // 扫频参数
    MultiSineParam_s multi_sine_params; // 多正弦叠加参数

    // 串级双环 PID 配置（输出限幅/积分限幅/微分滤波/误差归一化等均由 lib_pid 掩码控制）
    PID_Init_Config_s pos_pid; // 位置环：输出为速度设定值 (rad/s)
    PID_Init_Config_s vel_pid; // 速度环：输出为力矩 (Nm)
    float dt_max;              // 单帧 dt 上限 (s)，<=0 不钳位，防止通信中断后积分/微分打尖峰

    // 位置环误差归一化（环绕轴 WRAP 设 2π：误差取最短路径 wrap 到 [-π, π)，边界处不跳变）
    // 置位时会自动写入 pos_pid.error_normalize_range 并开启 PID_ENABLE_ERROR_NORMALIZE
    float error_normalize_range;    // 位置误差归一化范围 (rad)，0=不启用
    uint8_t error_normalize_enable; // 归一化使能
} AxisPidLite_Init_Config_s;

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化轴控制实例
 * @param inst 实例指针
 * @param cfg 配置结构体指针（只读）
 * @return 0: 成功, -1: 失败
 */
int8_t AxisPidLiteInit(AxisPidLiteInstance *inst, const AxisPidLite_Init_Config_s *cfg);

/**
 * @brief 计算控制输出，返回 setref 力矩值（不调用 motor.set_ref，由 app 手动设置）
 * @param inst 实例指针
 * @param state 关节反馈指针（位置/速度/力矩，只读），由 app 层从电机读出后填入
 * @param ref 外部设定值指针（位置/速度/加速度，仅 NORMAL 阶段使用，只读）
 * @return setref 最终发送给电机的力矩值 (Nm)
 * @note 反馈数据坐标系由 app 决定：state 应填与 params.gear_ratio 匹配的电机侧数据，
 *       内部按 angle = angle/gear 换算到输出侧；若 app 直接给输出侧数据请将 gear_ratio 置 1
 * @note setref =（重力前馈 + 惯量前馈 + 摩擦前馈） + 速度环 PID 输出，
 *       速度环设定值 = 位置环 PID 输出（位置环前馈为设定速度）
 * @note 当实例 vofa_enable 置 1 时，自动设定 12 个 VOFA 调试通道，
 *       起始通道由 vofa_base_ch 指定（记为 B，默认 1）：
 *       CH(B+0)~CH(B+2): 反馈量（轴侧，经 gear_ratio 转换）
 *       B+0:  position       — 反馈位置 (rad)
 *       B+1:  speed          — 反馈速度 (rad/s)
 *       B+2:  torque         — 轴侧实际力矩 (Nm)
 *
 *       CH(B+3)~CH(B+5): 设定值
 *       B+3:  ref_pos        — 位置设定值 (rad)
 *       B+4:  ref_vel        — 速度设定值 (rad/s)
 *       B+5:  ref_acc        — 加速度设定值 (rad/s^2)
 *
 *       CH(B+6)~CH(B+8): 前馈分量
 *       B+6:  gravity_ff     — 重力前馈 (Nm)
 *       B+7:  inertia_ff     — 惯量前馈 (Nm)
 *       B+8:  friction_ff    — 摩擦前馈 / chirp (Nm)
 *
 *       CH(B+9)~CH(B+10): PID 输出
 *       B+9:  pos_pid_out    — 位置环输出，即速度环设定值 (rad/s)
 *       B+10: vel_pid_out    — 速度环输出 (Nm)
 *
 *       CH(B+11): setref值，最终发送给电机的力矩值 (Nm)
 * @note 超出 VOFA_CHANNELS 的通道由 VofaSetChannel 自动忽略，不会越界写缓冲
 */
float AxisPidLiteCalculate(AxisPidLiteInstance *inst, const AxisLiteState_s *state, const AxisLiteRef_s *ref);

#endif // !DRV_AXIS_PID_LITE_H
