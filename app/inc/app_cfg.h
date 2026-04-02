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
#define DEVELOPMENT_BOARD STM32F407VET6

/*============================================
 *              应用参数配置
 *============================================*/

/**
 * @brief 任务频率设置
 */
#define CMD_FREQ_MS 100   // 10Hz 遥控
#define CHASSIS_FREQ_MS 1 // 1000Hz 底盘
#define MOTOR_FREQ_MS 1   // 1000Hz 电机
#define ERROR_FREQ_MS 100 // 10Hz 错误

/*============================================
 *              底盘类型选择
 *============================================*/

/**
 * @brief 底盘类型定义（编译时选择）
 */
#define CHASSIS_MECANUM_X 0  // 麦轮 X 型
#define CHASSIS_MECANUM_O 1  // 麦轮 O 型
#define CHASSIS_OMNI_X 2     // 全向轮 X 型
#define CHASSIS_OMNI_CROSS 3 // 全向轮十字型

// 修改此处切换底盘类型
#define CHASSIS_TYPE CHASSIS_MECANUM_X

/*============================================
 *              底盘几何参数
 *============================================*/

#define WHEEL_RADIUS 0.068f // 轮半径 (m)
#define WHEELBASE_A 0.2f    // 麦轮: 前后轮距的一半 (m)
#define WHEELBASE_B 0.2f    // 麦轮: 左右轮距的一半 (m)
#define OMNI_L 0.2f         // 全向轮: 轮到中心距离 (m)
// 添加脉轮到中心的距离
// 全向轮改为左右和前后
/**
 * @brief 任务STACK大小
 */
#define CMD_STACK_SIZE 256     // 遥控256
#define CHASSIS_STACK_SIZE 256 // 底盘256
#define MOTOR_STACK_SIZE 256   // 电机256
#define ERROR_STACK_SIZE 256   // 错误256

#endif // __APP_CFG_H