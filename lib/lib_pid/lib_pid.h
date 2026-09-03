/**
 * @file lib_pid.h
 * @brief PID 控制器（lib 层纯算法，dt 由调用方传入）
 *
 * @note 功能特性：
 *       - 积分限幅 (抗积分饱和)
 *       - 梯形积分
 *       - 变速积分 (误差大时减弱积分作用)
 *       - 比例先行 (对测量值比例计算，避免设定值突变冲击)
 *       - 微分先行 (对测量值微分，避免设定值突变冲击)
 *       - 微分项滤波 (抑制高频噪声)
 *       - 死区控制
 *       - 输出滤波 (一阶低通滤波平滑输出)
 *       - 输出限幅 (可配置)
 *       - 前馈控制
 *
 * @note 本模块为 lib 层纯算法，无硬件/时间源依赖：
 *       - dt 由调用方作为参数传入，不内嵌时间戳
 *       - 需要自动计算 dt 时使用 drv_pid（封装 DWT）或调用方自行维护时间
 *
 * @note 拓展功能启用规则：
 *       - 所有高级功能统一通过掩码 config_mask 控制启用/禁用
 *       - 所有高级功能通过 PID_Init_Config_s 中的 config_mask 掩码位或配置
 */

#ifndef __LIB_PID_H
#define __LIB_PID_H

#include <stdint.h>

/*------------- 掩码枚举 --------------*/

/**
 * @brief PID 功能配置掩码
 * @note 用于需要掩码区分"禁用"和"参数为0"的功能
 */
typedef enum : uint16_t
{
    PID_IMPROVE_NONE = 0x00,                // 无高级功能
    PID_ENABLE_INTEGRAL_LIMIT = 0x01,       // 启用积分限幅
    PID_ENABLE_DERIVATIVE_ON_MEAS = 0x02,   // 启用微分先行
    PID_ENABLE_TRAPEZOID_INTEGRAL = 0x04,   // 启用梯形积分
    PID_ENABLE_CHANGING_INTEGRATION = 0x08, // 启用变速积分
    PID_ENABLE_PROPORTIONAL_ON_MEAS = 0x10, // 启用比例先行
    PID_ENABLE_DERIVATIVE_FILTER = 0x20,    // 启用微分滤波
    PID_ENABLE_OUTPUT_FILTER = 0x40,        // 启用输出滤波
    PID_ENABLE_OUTPUT_LIMIT = 0x80,         // 启用输出限幅
    PID_ENABLE_DEADBAND = 0x100,            // 启用死区控制
    PID_ENABLE_ERROR_NORMALIZE = 0x200,     // 启用误差归一化
} PIDConfigMask;

/*------------- 配置结构体 --------------*/

/**
 * @brief PID 初始化配置结构体
 */
typedef struct
{
    float kp;                    // 比例系数
    float ki;                    // 积分系数
    float kd;                    // 微分系数
    float integral_limit;        // 积分限幅阈值 (0 = 禁用)
    float coef_a;                // 变速积分参数 A (0 = 禁用)
    float coef_b;                // 变速积分参数 B
    float d_lpf_rc;              // 微分滤波时间常数 RC (0 = 禁用)
    float out_lpf_rc;            // 输出滤波时间常数 RC (0 = 禁用)
    float deadband;              // 死区范围 (0 = 禁用)
    float error_normalize_range; // 误差归一化范围 (0 = 禁用, 需要 PID_ENABLE_ERROR_NORMALIZE)
    float out_max;               // 输出上限 (需要 PID_ENABLE_OUTPUT_LIMIT)
    float out_min;               // 输出下限 (需要 PID_ENABLE_OUTPUT_LIMIT)
    PIDConfigMask config_mask;   // 功能配置掩码
} PID_Init_Config_s;

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
    // 积分限幅 (需要掩码 PID_ENABLE_INTEGRAL_LIMIT 启用)
    float integral_limit; // 积分限幅阈值

    // 变速积分参数 (coef_a > 0 时启用)
    // 当 |Err| <= CoefB: 全积分
    // 当 CoefB < |Err| <= CoefA+CoefB: 积分系数 = (CoefA - |Err| + CoefB) / CoefA
    // 当 |Err| > CoefA+CoefB: 不积分
    float coef_a; // 变速积分参数 A
    float coef_b; // 变速积分参数 B (默认=1)

    // 微分滤波 (参数 > 0 时启用)
    float d_lpf_rc; // 微分滤波时间常数 RC = 1/omegac

    // 输出滤波 (需要掩码 PID_ENABLE_OUTPUT_FILTER 启用)
    float out_lpf_rc; // 输出滤波时间常数 RC = 1/omegac

    // 误差归一化 (需要掩码 PID_ENABLE_ERROR_NORMALIZE 启用)
    float error_normalize_range; // 误差归一化范围 (0=不启用)

    // 死区控制 (需要掩码 PID_ENABLE_DEADBAND 启用)
    float deadband; // 死区范围 (误差小于此值时不输出)

    // 输出限幅 (需要掩码 PID_ENABLE_OUTPUT_LIMIT 启用)
    float out_max; // 输出上限
    float out_min; // 输出下限

    /*------------- 功能配置掩码 -------------*/
    PIDConfigMask config_mask; // 功能配置掩码

    /*------------- 状态变量 -------------*/
    float measure;      // 当前测量值
    float last_measure; // 上次测量值 (用于微分先行/比例先行)
    float error;        // 当前误差
    float last_error;   // 上次误差

    float p_out;      // 比例项输出
    float i_out;      // 积分项输出
    float i_term;     // 当前积分增量
    float d_out;      // 微分项输出
    float last_d_out; // 上次微分项输出 (用于微分滤波)

    float feedforward_out; // 前馈输出
    float output;          // 总输出
    float last_output;     // 上次输出 (用于输出滤波)

    float dt;         // 本次计算时间间隔 (秒)，由 LibPIDCalculate 的 dt 参数写入
    uint64_t time_us; // 上次计算时间戳 (us)，供时间供给层(drv_pid)计算 dt；lib 算法不读取
} PIDInstance;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化 PID 实例
 * @param instance PID 实例指针
 * @param config   初始化配置结构体指针
 *
 * @note 配置结构体包含所有参数和功能掩码，一次性完成初始化
 *       - 不需要的高级功能将参数设为 0、掩码位不置即可
 */
void LibPIDInit(PIDInstance *instance, const PID_Init_Config_s *config);

/**
 * @brief 重置 PID 状态
 * @param instance PID 实例指针
 * @note 清零积分、上次误差等算法状态变量
 *       - time_us 属于时间供给层，本函数不触碰；如需重算 dt 起点由调用方清零
 */
void LibPIDReset(PIDInstance *instance);

/**
 * @brief PID 计算 (位置式)
 * @param instance PID 实例指针
 * @param setpoint 目标值
 * @param measure 测量值
 * @param feedforward 前馈值
 * @param dt 本次计算时间间隔 (秒)，由调用方提供；首次调用/无时间基准可传 0
 * @return 控制输出
 *
 * @note 位置式 PID 公式：
 *       u(k) = Kp*e(k) + Ki*Σe + Kd*(e(k)-e(k-1))/dt
 *
 * @note dt 由调用方传入：微分项、微分滤波、输出滤波均按该 dt 计算，
 *       dt<=0 时相应项按 0/关闭处理（等价原自动时间戳的首帧行为）
 *
 * @note 拓展功能根据 config_mask 掩码启用：
 *       - PID_ENABLE_TRAPEZOID_INTEGRAL → 梯形积分
 *       - PID_ENABLE_CHANGING_INTEGRATION → 变速积分
 *       - PID_ENABLE_INTEGRAL_LIMIT → 积分限幅
 *       - PID_ENABLE_PROPORTIONAL_ON_MEAS → 比例先行
 *       - PID_ENABLE_DERIVATIVE_ON_MEAS → 微分先行
 *       - PID_ENABLE_DERIVATIVE_FILTER → 微分滤波
 *       - PID_ENABLE_OUTPUT_FILTER → 输出滤波
 *       - PID_ENABLE_OUTPUT_LIMIT → 输出限幅
 *       - PID_ENABLE_DEADBAND → 死区控制
 *       - feedforward 参数传 0 表示无前馈
 */
float LibPIDCalculate(PIDInstance *instance, float setpoint, float measure, float feedforward, float dt);

#endif /* __LIB_PID_H */
