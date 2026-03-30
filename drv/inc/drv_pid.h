/**
 * @file drv_pid.h
 * @brief PID 控制器驱动
 *
 * @note 功能特性：
 *       - 积分限幅 (抗积分饱和)
 *       - 梯形积分
 *       - 变速积分 (默认启用，CoefB=1)
 *       - 微分先行 (避免设定值突变冲击)
 *       - 微分项滤波 (抑制高频噪声)
 *       - 死区控制
 *       - 输出限幅
 *       - 前馈控制
 *
 * @note DRV 层职责：
 *       - 提供纯计算函数，无状态依赖
 *       - 不使用 FreeRTOS
 *
 * @note 拓展功能启用规则：
 *       - 参数为 0 时功能不启用
 *       - integral_limit != 0 → 启用积分限幅
 *       - d_lpf_rc > 0 → 启用微分滤波
 *       - deadband > 0 → 启用死区控制
 *       - coef_a > 0 → 启用变速积分
 */

#ifndef __DRV_PID_H
#define __DRV_PID_H

#include <stdint.h>

/*------------- 宏定义 --------------*/

#define PID_MAX 1.0f  // 输出上限
#define PID_MIN -1.0f // 输出下限

/*------------- 类型定义 --------------*/

/**
 * @brief PID 控制器实例结构体
 */
typedef struct PIDInstance
{
    /*------------- PID 基本参数 -------------*/
    float kp; // 比例系数
    float ki; // 积分系数
    float kd; // 微分系数

    /*------------- 优化环节配置 -------------*/
    // 积分限幅 (参数 != 0 时启用)
    float integral_limit; // 积分限幅阈值

    // 变速积分参数 (coef_a > 0 时启用)
    // 当 |Err| <= CoefB: 全积分
    // 当 CoefB < |Err| <= CoefA+CoefB: 积分系数 = (CoefA - |Err| + CoefB) / CoefA
    // 当 |Err| > CoefA+CoefB: 不积分
    float coef_a; // 变速积分参数 A
    float coef_b; // 变速积分参数 B (默认=1)

    // 微分滤波 (参数 > 0 时启用)
    float d_lpf_rc; // 微分滤波时间常数 RC = 1/omegac

    // 死区控制 (参数 > 0 时启用)
    float deadband; // 死区范围 (误差小于此值时不输出)

    // 前馈控制
    float kf; // 前馈系数

    /*------------- 状态变量 -------------*/
    float measure;      // 当前测量值
    float last_measure; // 上次测量值 (用于微分先行)
    float error;        // 当前误差
    float last_error;   // 上次误差

    float p_out;      // 比例项输出
    float i_out;      // 积分项输出
    float i_term;     // 当前积分增量
    float d_out;      // 微分项输出
    float last_d_out; // 上次微分项输出 (用于微分滤波)

    float feedforward_out; // 前馈输出
    float output;          // 总输出

    float dt; // 时间间隔 (秒)，由外部传入
} PIDInstance;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化 PID 实例
 * @param instance PID 实例指针
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 *
 * @note 初始化后高级功能默认禁用（参数为 0）：
 *       - 死区 = 0
 *       - 积分限幅 = 0
 *       - 前馈系数 = 0
 *       - 微分滤波 RC = 0
 *       - 变速积分参数 CoefA = 0, CoefB = 1
 *       需要使用时直接修改结构体成员
 */
void PIDInit(PIDInstance *instance, float kp, float ki, float kd);

/**
 * @brief 重置 PID 状态
 * @param instance PID 实例指针
 * @note 清零积分、上次误差等状态变量
 */
void PIDReset(PIDInstance *instance);

/**
 * @brief PID 计算 (位置式)
 * @param instance PID 实例指针
 * @param setpoint 目标值 (归一化 [-1, 1])
 * @param measure 测量值 (归一化 [-1, 1])
 * @param dt 时间间隔 (秒)，由 APP 层通过 DWT 获取后传入
 * @param feedforward 前馈值 (已乘以前馈系数的外部前馈)
 * @return 控制输出 (归一化 [-1, 1])
 *
 * @note 位置式 PID 公式：
 *       u(k) = Kp*e(k) + Ki*Σe + Kd*(e(k)-e(k-1))/dt
 *
 * @note 拓展功能根据参数自动启用：
 *       - integral_limit != 0 → 积分限幅
 *       - d_lpf_rc > 0 → 微分滤波
 *       - deadband > 0 → 死区控制
 *       - coef_a > 0 → 变速积分
 */
float PIDCalculate(PIDInstance *instance, float setpoint, float measure, float dt, float feedforward);

#endif /* __DRV_PID_H */