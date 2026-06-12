/**
 * @file drv_pid.c
 * @brief PID 控制器驱动实现
 *
 * @note 功能特性：
 *       1. 积分限幅 (抗积分饱和)：限制积分项累加上限
 *       2. 梯形积分：提高积分精度
 *       3. 变速积分：误差大时减弱积分，兼具积分分离效果
 *       4. 比例先行：对测量值比例计算，避免设定值突变冲击
 *       5. 微分先行：仅对测量值微分，避免设定值突变冲击
 *       6. 微分滤波：一阶低通滤波抑制高频噪声
 *       7. 死区控制：误差小于死区时不输出
 *       8. 输出滤波：一阶低通滤波平滑输出
 *       9. 输出限幅：可配置输出范围
 *       10. 前馈控制：提高系统响应速度
 *
 * @note 拓展功能启用规则：
 *       - 所有高级功能统一通过掩码 config_mask 控制启用/禁用
 *       - 所有高级功能通过 PID_Init_Config_s 中的 config_mask 掩码位或配置
 */

#include "drv_pid.h"
#include "bsp_math.h"
#include <string.h>

/*------------- 私有函数声明 --------------*/
static void f_Trapezoid_Intergral(PIDInstance *pid);
static void f_Changing_Integration_Rate(PIDInstance *pid);
static void f_Integral_Limit(PIDInstance *pid);
static void f_Proportional_On_Measurement(PIDInstance *pid);
static void f_Derivative_On_Measurement(PIDInstance *pid);
static void f_Derivative_On_Error(PIDInstance *pid);
static void f_Derivative_Filter(PIDInstance *pid);
static void f_Output_Filter(PIDInstance *pid);

/*------------- 私有函数实现 --------------*/

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
        float abs_err = FABS(pid->error);
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
    if (pid->config_mask & PID_ENABLE_OUTPUT_LIMIT)
    {
        if (temp_output > pid->out_max || temp_output < pid->out_min)
        {
            // 使用符号宏判断积分累积方向
            int8_t i_sign = MATH_SIGN(pid->i_out);
            int8_t term_sign = MATH_SIGN(pid->i_term);
            if (i_sign == 0 || i_sign == term_sign)
            {
                pid->i_term = 0;
            }
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
 * @brief 比例先行
 *        仅对测量值比例计算，避免设定值突变引起的比例冲击
 *        Pout = Kp * (Last_Measure - Measure) / dt
 *
 * @note 与微分先行类似，但应用于比例项
 *       适用于设定值频繁突变的场景
 */
static void f_Proportional_On_Measurement(PIDInstance *pid)
{
    if (pid->dt > 0.0001f)
    {
        pid->p_out = pid->kp * (pid->last_measure - pid->measure) / pid->dt;
    }
    else
    {
        pid->p_out = 0;
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
 *       当 RC = 0 时，alpha = 1，滤波关闭，直接输出当前值
 */
static void f_Derivative_Filter(PIDInstance *pid)
{
    if (pid->d_lpf_rc <= 0.0f || pid->dt <= 0.0f)
    {
        // RC = 0 或 dt = 0 时，直接使用当前微分值（滤波关闭）
        return;
    }
    float alpha = pid->dt / (pid->d_lpf_rc + pid->dt);
    pid->d_out = pid->d_out * alpha + pid->last_d_out * (1.0f - alpha);
}

/**
 * @brief 输出滤波
 *        一阶低通滤波，平滑输出信号
 *        Output = Output * dt / (RC + dt) + Last_Output * RC / (RC + dt)
 *
 * @note 由 PID_ENABLE_OUTPUT_FILTER 掩码控制是否调用
 *       当 RC = 0 时，alpha = 1，滤波关闭，直接输出当前值
 */
static void f_Output_Filter(PIDInstance *pid)
{
    if (pid->out_lpf_rc <= 0.0f || pid->dt <= 0.0f)
    {
        // RC = 0 或 dt = 0 时，直接使用当前输出值（滤波关闭）
        return;
    }
    float alpha = pid->dt / (pid->out_lpf_rc + pid->dt);
    pid->output = pid->output * alpha + pid->last_output * (1.0f - alpha);
}

/*------------- 公开接口实现 --------------*/

void PIDInit(PIDInstance *instance, const PID_Init_Config_s *config)
{
    if (instance == NULL)
    {
        return;
    }

    if (config == NULL)
    {
        return;
    }

    // 清零所有字段
    memset(instance, 0, sizeof(PIDInstance));

    // 基本参数
    instance->kp = config->kp;
    instance->ki = config->ki;
    instance->kd = config->kd;

    // 高级功能参数
    instance->integral_limit = config->integral_limit;
    instance->coef_a = config->coef_a;
    instance->coef_b = config->coef_b;
    instance->d_lpf_rc = config->d_lpf_rc;
    instance->out_lpf_rc = config->out_lpf_rc;
    instance->deadband = config->deadband;

    // 输出限幅参数
    instance->out_max = config->out_max;
    instance->out_min = config->out_min;

    // 功能掩码
    instance->config_mask = config->config_mask;
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
    instance->last_output = 0.0f;
    instance->dt = 0.0f;
    instance->time_us = 0;
}

float PIDCalculate(PIDInstance *instance, float setpoint, float measure, float feedforward)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 计算时间间隔 (自动)
    uint64_t now_us = DWT_GetTimeUs();
    if (instance->time_us > 0)
    {
        instance->dt = (now_us - instance->time_us) * 1e-6f;
    }
    else
    {
        instance->dt = 0.0f; // 首次调用
    }
    instance->time_us = now_us;

    // 保存测量值
    instance->measure = measure;

    // 1. 计算误差
    instance->error = setpoint - measure;

    // 2. 死区控制
    if ((instance->config_mask & PID_ENABLE_DEADBAND) && FABS(instance->error) < instance->deadband)
    {
        instance->i_out = 0.0f;
        instance->last_error = instance->error;
        instance->last_measure = measure;
        instance->output = 0.0f;
        return 0.0f;
    }

    // 3. 比例项
    if (instance->config_mask & PID_ENABLE_PROPORTIONAL_ON_MEAS)
    {
        f_Proportional_On_Measurement(instance);
    }
    else
    {
        instance->p_out = instance->kp * instance->error;
    }

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

    // 6. 前馈控制 (直接使用传入的前馈值，不需要前馈时传 0)
    instance->feedforward_out = feedforward;

    // 7. 计算总输出
    instance->output = instance->p_out + instance->i_out + instance->d_out + instance->feedforward_out;

    // 8. 输出滤波
    if (instance->config_mask & PID_ENABLE_OUTPUT_FILTER)
    {
        f_Output_Filter(instance);
    }

    // 9. 输出限幅 (可配置)
    if (instance->config_mask & PID_ENABLE_OUTPUT_LIMIT)
    {
        instance->output = BSP_Math_Clamp(instance->output, instance->out_min, instance->out_max);
    }

    // 10. 保存状态
    instance->last_error = instance->error;
    instance->last_measure = measure;
    instance->last_d_out = instance->d_out;
    instance->last_output = instance->output;

    return instance->output;
}
