/**
 * @file drv_mit.c
 * @brief MIT 控制器驱动实现
 *
 * @note 功能特性：
 *       - 位置环 PD 控制
 *       - 力矩前馈控制
 *       - 输出限幅
 */

#include "drv_mit.h"
#include "app_cfg.h"

#ifdef DRV_MIT_USED

#include <string.h>
#include "bsp_math.h"

/*------------- 公开接口实现 --------------*/

void MITInit(MITInstance *instance, const MIT_Init_Config_s *config)
{
    if (instance == NULL || config == NULL)
    {
        return;
    }

    memset(instance, 0, sizeof(MITInstance));

    instance->kp = config->kp;
    instance->kd = config->kd;
    instance->out_max = config->out_max;
    instance->out_min = config->out_min;
}

float MITCalculate(MITInstance *instance, float speed_set, float speed_measure, float pos_set, float pos_measure, float feedforward)
{
    if (instance == NULL)
    {
        return 0.0f;
    }

    // 计算误差
    instance->pos_error = pos_set - pos_measure;
    instance->speed_error = speed_set - speed_measure;
    instance->feedforward = feedforward;

    // MIT 计算: output = kp * pos_error + kd * speed_error + feedforward
    instance->pos_output = instance->kp * instance->pos_error;
    instance->speed_output = instance->kd * instance->speed_error;
    instance->output = instance->pos_output + instance->speed_output + instance->feedforward;

    // 输出限幅
    instance->output = BSP_Math_Clamp(instance->output, instance->out_min, instance->out_max);

    return instance->output;
}

#endif /* DRV_MIT_USED */
