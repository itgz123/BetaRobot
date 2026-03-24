/**
 * @file app_cfg.h
 * @brief APP层配置文件：开发板选择、应用参数配置
 *
 * @note 本文件职责：
 *       1. 选择开发板（DEVELOPMENT_BOARD）
 *       2. 配置应用层参数（任务频率、底盘参数等）
 *
 * @note 切换开发板：只需修改 DEVELOPMENT_BOARD 宏
 *       BSP层会自动适配硬件映射
 */

#ifndef __APP_CFG_H
#define __APP_CFG_H

/*============================================
 *              开发板选择
 *============================================*/

/**
 * @brief 开发板类型定义
 *
 * | 宏定义         | 主控型号        | hal目录名      |
 * |---------------|----------------|----------------|
 * | STM32F407VET6 | STM32F407VET6  | hal/STM32F407VET6 |
 * | DM_MC02       | STM32H723VGT6  | hal/DM_MC02    |
 * | DJI_A         | STM32F427IIH6  | hal/DJI_A      |
 * | DJI_C         | STM32F407IGH6  | hal/DJI_C      |
 */
#define STM32F407VET6 0
#define DM_MC02 1
#define DJI_A 2
#define DJI_C 3

// 修改此处切换开发板
#define DEVELOPMENT_BOARD DM_MC02

/*============================================
 *              应用参数配置
 *============================================*/

/**
 * @brief 任务频率设置
 */
#define CMD_FREQ_MS 100   // 10Hz 遥控
#define CHASSIS_FREQ_MS 5 // 200Hz 底盘
#define MOTOR_FREQ_MS 1   // 1000Hz 电机
#define ERROR_FREQ_MS 100 // 10Hz 错误

/**
 * @brief 任务STACK大小
 */
#define CMD_STACK_SIZE 256     // 遥控256
#define CHASSIS_STACK_SIZE 256 // 底盘256
#define MOTOR_STACK_SIZE 256   // 电机256
#define ERROR_STACK_SIZE 256   // 错误256

#endif // __APP_CFG_H