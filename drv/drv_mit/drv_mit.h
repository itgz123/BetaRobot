/**
 * @file drv_mit.h
 * @brief MIT 控制器驱动
 *
 * @note 功能特性：
 *       - 位置环 PD 控制
 *       - 力矩前馈控制
 *       - 输出限幅
 *
 * @note DRV 层职责：
 *       - 提供纯计算函数，无状态依赖
 *       - 不使用 FreeRTOS
 */

#ifndef __DRV_MIT_H
#define __DRV_MIT_H

#include <stdint.h>

/*------------- 配置结构体 --------------*/

/**
 * @brief MIT 初始化配置结构体
 */
typedef struct
{
    float kp;       // 位置比例系数
    float kd;       // 速度比例系数
    float out_max;  // 输出上限
    float out_min;  // 输出下限
} MIT_Init_Config_s;

/*------------- 类型定义 --------------*/

/**
 * @brief MIT 控制器实例结构体
 */
typedef struct MITInstance
{
    /*------------- 控制参数 -------------*/
    float kp; // 位置比例系数
    float kd; // 速度比例系数

    /*------------- 输出限幅 -------------*/
    float out_max; // 输出上限
    float out_min; // 输出下限

    /*------------- 状态变量 -------------*/
    float pos_error;   // 位置误差
    float speed_error; // 速度误差

    /*------------- 输出F变量 -------------*/
    float pos_output;   // 位置输出
    float speed_output; // 速度输出
    float feedforward;  // 前馈值

    float output; // 总输出
} MITInstance;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化 MIT 实例
 * @param instance MIT 实例指针
 * @param config   初始化配置结构体指针
 */
void MITInit(MITInstance *instance, const MIT_Init_Config_s *config);

/**
 * @brief MIT 控制计算
 * @param instance      MIT 实例指针
 * @param speed_set     速度设定值（rad/s）
 * @param speed_measure 速度实际值（rad/s）
 * @param pos_set       位置设定值（rad）
 * @param pos_measure   位置实际值（rad）
 * @param feedforward   前馈值
 * @return 控制输出
 *
 * @note MIT 控制公式：
 *       output = kp * (pos_set - pos_measure) + kd * (speed_set - speed_measure) + feedforward
 */
float MITCalculate(MITInstance *instance, float speed_set, float speed_measure, float pos_set, float pos_measure, float feedforward);

#endif /* __DRV_MIT_H */
