/**
 * @file drv_pid.h
 * @brief PID 驱动层：以原接口封装 lib_pid + DWT（自动计算 dt）
 *
 * @note 分层说明：
 *       - lib_pid (lib/lib_pid)：纯算法，无时间源依赖，dt 由调用方传入
 *       - drv_pid (drv/drv_pid)：保留历史 PIDInit/PIDReset/PIDCalculate 接口，
 *         内部调用 LibPID*，并用 DWT_GetTimeUs 自动测量本次调用间隔作为 dt
 *
 * @note 使用前提：需启用 bsp_dwt（bsp_dwt.c 提供 DWT_GetTimeUs 时间基准）
 * @note 功能裁切：纯算法 lib_pid 的启用由 app_cfg 的 LIB_PID_USED 决定（与其它 lib 模块一致）；
 *       本模块依赖 lib_pid + bsp_dwt，仅在使用“自动 dt 原接口”时引入（drv/CMakeLists.txt 已含）
 */

#ifndef __DRV_PID_H
#define __DRV_PID_H

#include <stdint.h>
#include "lib_pid.h" /* 类型透传：PIDInstance / PID_Init_Config_s / 掩码枚举 */

/*------------- 外部接口声明（原接口，供 PID 使用者沿用） --------------*/

/**
 * @brief 初始化 PID 实例（等价 lib_pid 的 LibPIDInit）
 * @param instance PID 实例指针
 * @param config   初始化配置结构体指针
 */
void PIDInit(PIDInstance *instance, const PID_Init_Config_s *config);

/**
 * @brief 重置 PID 状态（清零算法状态，并复位内部时间戳使下次计算 dt=0）
 * @param instance PID 实例指针
 */
void PIDReset(PIDInstance *instance);

/**
 * @brief PID 计算 (位置式，自动测量调用间隔作为 dt)
 * @param instance PID 实例指针
 * @param setpoint 目标值
 * @param measure 测量值
 * @param feedforward 前馈值
 * @return 控制输出
 *
 * @note dt 通过 DWT_GetTimeUs 自动计算：首次调用 dt=0，
 *       后续 dt = 距上次调用的时间间隔 (秒)
 */
float PIDCalculate(PIDInstance *instance, float setpoint, float measure, float feedforward);

#endif /* __DRV_PID_H */
