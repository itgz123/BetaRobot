/**
 * @file drv_pid.c
 * @brief PID 控制器驱动实现
 *
 * @note 功能特性：
 *       1. 积分限幅 (抗积分饱和)：限制积分项累加上限
 *       2. 梯形积分：提高积分精度
 *       3. 变速积分：误差大时减弱积分，兼具积分分离效果
 *       4. 微分先行：仅对测量值微分，避免设定值突变冲击
 *       5. 微分滤波：一阶低通滤波抑制高频噪声
 *       6. 死区控制：误差小于死区时不输出
 *       7. 输出限幅：输出限制在 [-1, 1]
 *       8. 前馈控制：提高系统响应速度
 *
 * @note 拓展功能启用规则：
 *       - 所有高级功能统一通过掩码 config_mask 控制启用/禁用
 *       - deadband 和 kf 通过参数判零控制（deadband > 0 / kf != 0 时生效）
 *       - 所有高级功能默认禁用，通过对应的 PIDSetXxx 函数逐个开启
 */

#include "drv_pid.h"
#include "bsp_math.h"
#include <string.h>

/*------------- 私有函数声明 --------------*/
static float clamp(float value, float min, float max);
static void f_Trapezoid_Intergral(PIDInstance *pid);
static void f_Changing_Integration_Rate(PIDInstance *pid);
static void f_Integral_Limit(PIDInstance *pid);
static void f_Derivative_On_Measurement(PIDInstance *pid);
static void f_Derivative_On_Error(PIDInstance *pid);
static void f_Derivative_Filter(PIDInstance *pid);

/*------------- 私有函数实现 --------------*/

/**
 * @brief 限幅函数
 */
static float clamp(float value, float min, float max)
{
    if (value > max)
        return max;
    if (value < min)
        return min;
    return value;
}

/**
 * @brief 梯形积分
 *        使用梯形面积代替矩形，提高积分精度
 *        ITerm = Ki * (Err + Last_Err) / 2
 */
static void f_Trapezoid_Intergral(PIDInstance *pid)
{
    pid->i_term = pid->ki * ((pid->error + pid->last_error) / 2.0f);
}

/**
 * @brief 变速积分
 *        根据误差大小动态调整积分强度
 *        - |Err| <= CoefB: 全积分
 *        - CoefB < |Err| <= CoefA+CoefB: 积分系数递减
 *        - |Err| > CoefA+CoefB: 不积分
 *
 * @note 由 PID_ENABLE_CHANGING_INTEGRATION 掩码控制是否调用
 */
static void f_Changing_Integration_Rate(PIDInstance *pid)
{
    // 积分呈累积趋势时才处理
    if (pid->error * pid->i_out > 0)
    {
        float abs_err = MATH_FABS(pid->error);
        if (abs_err <= pid->coef_b)
        {
            return;
        }
        else if (abs_err <= (pid->coef_a + pid->coef_b))
        {
            pid->i_term *= (pid->coef_a - abs_err + pid->coef_b) / pid->coef_a;
        }
        else
        {
            pid->i_term = 0;
        }
    }
}

/**
 * @brief 积分限幅
 *        防止积分饱和 (Windup)
 *
 * @note 由 PID_ENABLE_INTEGRAL_LIMIT 掩码控制是否调用
 */
static void f_Integral_Limit(PIDInstance *pid)
{
    float temp_i_out = pid->i_out + pid->i_term;
    float temp_output = pid->p_out + temp_i_out + pid->d_out;

    // 输出超限时，如果积分还在累积，则停止当前积分增量
    if (MATH_FABS(temp_output) > PID_MAX)
    {
        if (pid->error * pid->i_out > 0)
        {
            pid->i_term = 0;
        }
    }

    // 积分限幅
    if (temp_i_out > pid->integral_limit)
    {
        pid->i_term = 0;
        pid->i_out = pid->integral_limit;
    }
    else if (temp_i_out < -pid->integral_limit)
    {
        pid->i_term = 0;
        pid->i_out = -pid->integral_limit;
    }
}

/**
 * @brief 微分先行
 *        仅对测量值微分，避免设定值突变引起的微分冲击
 *        Dout = Kd * (Last_Measure - Measure) / dt
 */
static void f_Derivative_On_Measurement(PIDInstance *pid)
{
    if (pid->dt > 0.0001f)
    {
        pid->d_out = pid->kd * (pid->last_measure - pid->measure) / pid->dt;
    }
    else
    {
        pid->d_out = 0;
    }
}

/**
 * @brief 对误差微分
 *        标准微分方式，对误差进行微分
 *        Dout = Kd * (Err - Last_Err) / dt
 */
static void f_Derivative_On_Error(PIDInstance *pid)
{
    if (pid->dt > 0.0001f)
    {
        pid->d_out = pid->kd * (pid->error - pid->last_error) / pid->dt;
    }
    else
    {
        pid->d_out = 0;
    }
}

/**
 * @brief 微分滤波
 *        一阶低通滤波，抑制高频噪声
 *        Dout = Dout * dt / (RC + dt) + Last_Dout * RC / (RC + dt)
 *
 * @note 由 PID_ENABLE_DERIVATIVE_FILTER 掩码控制是否调用
 */
static void f_Derivative_Filter(PIDInstance *pid)
{
    float alpha = pid->dt / (pid->d_lpf_rc + pid->dt);
    pid->d_out = pid->d_out * alpha + pid->last_d_out * (1.0f - alpha);
}

/*------------- 公开接口实现 --------------*/

void PIDInit(PIDInstance *instance, float kp, float ki, float kd)
{
    if (instance == NULL)
    {
        return;
    }

    // 清零所有字段
    memset(instance, 0, sizeof(PIDInstance));

    // 基本参数
    instance->kp = kp;
    instance->ki = ki;
    instance->kd = kd;

    // 高级功能默认禁用 (参数为 0，不影响基本 PID 计算)
    instance->integral_limit = 0.0f; // 积分限幅默认禁用
    instance->coef_a = 0.0f;         // 变速积分参数 A 默认 0
    instance->coef_b = 1.0f;         // 变速积分参数 B 默认 1
    instance->d_lpf_rc = 0.0f;       // 微分滤波默认禁用
    instance->deadband = 0.0f;       // 死区默认禁用
    instance->kf = 0.0f;             // 前馈系数默认 0

    // 功能掩码
    instance->config_mask = PID_IMPROVE_NONE;
}

void PIDReset(PIDInstance *instance)
{
    if (instance == NULL)
    {
        return;
    }

    instance->measure = 0.0f;
    instance->last_measure = 0.0f;
    instance->error = 0.0f;
    instance->last_error = 0.0f;

    instance->p_out = 0.0f;
    instance->i_out = 0.0f;
    instance->i_term = 0.0f;
    instance->d_out = 0.0f;
    instance->last_d_out = 0.0f;

    instance->feedforward_out = 0.0f;
    instance->output = 0.0f;
    instance->dt = 0.0f;
}

float PIDCalculate(PIDInstance *instance, float setpoint, float measure, float dt, float feedforward)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 保存时间间隔
    instance->dt = dt;

    // 0. 输入限幅 (归一化到 [-1, 1])
    setpoint = clamp(setpoint, PID_MIN, PID_MAX);
    measure = clamp(measure, PID_MIN, PID_MAX);

    // 保存测量值
    instance->measure = measure;

    // 1. 计算误差
    instance->error = setpoint - measure;

    // 2. 死区控制 (deadband > 0 时启用)
    if (instance->deadband > 0.0001f && MATH_FABS(instance->error) < instance->deadband)
    {
        instance->i_out = 0.0f;
        instance->last_error = instance->error;
        instance->last_measure = measure;
        instance->output = 0.0f;
        return 0.0f;
    }

    // 3. 比例项
    instance->p_out = instance->kp * instance->error;

    // 4. 积分项
    instance->i_term = instance->ki * instance->error;

    // 梯形积分
    if (instance->config_mask & PID_ENABLE_TRAPEZOID_INTEGRAL)
    {
        f_Trapezoid_Intergral(instance);
    }

    // 变速积分
    if (instance->config_mask & PID_ENABLE_CHANGING_INTEGRATION)
    {
        f_Changing_Integration_Rate(instance);
    }

    // 累加积分
    instance->i_out += instance->i_term;

    // 积分限幅
    if (instance->config_mask & PID_ENABLE_INTEGRAL_LIMIT)
    {
        f_Integral_Limit(instance);
    }

    // 5. 微分项
    if (instance->config_mask & PID_ENABLE_DERIVATIVE_ON_MEAS)
    {
        f_Derivative_On_Measurement(instance);
    }
    else
    {
        f_Derivative_On_Error(instance);
    }

    // 微分滤波
    if (instance->config_mask & PID_ENABLE_DERIVATIVE_FILTER)
    {
        f_Derivative_Filter(instance);
    }

    // 6. 前馈控制
    instance->feedforward_out = instance->kf * feedforward;

    // 7. 计算总输出
    instance->output = instance->p_out + instance->i_out + instance->d_out + instance->feedforward_out;

    // 8. 输出限幅
    instance->output = clamp(instance->output, PID_MIN, PID_MAX);

    // 9. 保存状态
    instance->last_error = instance->error;
    instance->last_measure = measure;
    instance->last_d_out = instance->d_out;

    return instance->output;
}

/*------------- 配置函数实现 --------------*/

void PIDSetDeadband(PIDInstance *instance, float deadband)
{
    if (instance == NULL)
    {
        return;
    }
    instance->deadband = deadband;
}

void PIDSetIntegralLimit(PIDInstance *instance, float limit, uint8_t enable)
{
    if (instance == NULL)
    {
        return;
    }
    instance->integral_limit = limit;
    if (enable)
    {
        instance->config_mask |= PID_ENABLE_INTEGRAL_LIMIT;
    }
    else
    {
        instance->config_mask &= ~PID_ENABLE_INTEGRAL_LIMIT;
    }
}

void PIDSetChangingIntegration(PIDInstance *instance, float coef_a, float coef_b, uint8_t enable)
{
    if (instance == NULL)
    {
        return;
    }
    instance->coef_a = coef_a;
    instance->coef_b = coef_b;
    if (enable)
    {
        instance->config_mask |= PID_ENABLE_CHANGING_INTEGRATION;
    }
    else
    {
        instance->config_mask &= ~PID_ENABLE_CHANGING_INTEGRATION;
    }
}

void PIDSetDerivativeFilter(PIDInstance *instance, float rc, uint8_t enable)
{
    if (instance == NULL)
    {
        return;
    }
    instance->d_lpf_rc = rc;
    if (enable)
    {
        instance->config_mask |= PID_ENABLE_DERIVATIVE_FILTER;
    }
    else
    {
        instance->config_mask &= ~PID_ENABLE_DERIVATIVE_FILTER;
    }
}

void PIDSetDerivativeOnMeasurement(PIDInstance *instance, uint8_t enable)
{
    if (instance == NULL)
    {
        return;
    }
    if (enable)
    {
        instance->config_mask |= PID_ENABLE_DERIVATIVE_ON_MEAS;
    }
    else
    {
        instance->config_mask &= ~PID_ENABLE_DERIVATIVE_ON_MEAS;
    }
}

void PIDSetTrapezoidIntegral(PIDInstance *instance, uint8_t enable)
{
    if (instance == NULL)
    {
        return;
    }
    if (enable)
    {
        instance->config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
    }
    else
    {
        instance->config_mask &= ~PID_ENABLE_TRAPEZOID_INTEGRAL;
    }
}
