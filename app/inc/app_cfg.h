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

#define WHEEL_RADIUS 0.034f // 轮半径 (m)

// 矩形底盘参数（麦轮X/O型、全向轮X型共用）
#define WHEELBASE_A 0.3f // 中心到前后轮的纵向距离 (m)
#define WHEELBASE_B 0.3f // 中心到左右轮的横向距离 (m)

// 中心到轮子距离：L = sqrt(a² + b²)
// 手动计算示例：若 a=0.2, b=0.2，则 L = sqrt(0.08) ≈ 0.2828
// 编译时提醒：若修改了 WHEELBASE_A 或 WHEELBASE_B，需重新计算 CHASSIS_L
// CHASSIS_L = sqrt(WHEELBASE_A^2 + WHEELBASE_B^2)
#define CHASSIS_L 0.4242f // 中心到轮子距离 (m)，手动计算
// #warning "WHEELBASE_A or WHEELBASE_B changed! Please recalculate CHASSIS_L = sqrt(a^2 + b^2)"

// 十字型全向轮参数（支持矩形）
#define OMNI_CROSS_A 0.2f // 中心到前后轮距离 (m)
#define OMNI_CROSS_B 0.2f // 中心到左右轮距离 (m)

// 解算系数宏定义
#define K_INVERSE (CHASSIS_L / WHEEL_RADIUS)    // 逆解系数：麦轮/全向轮X型
#define K_FORWARD (WHEEL_RADIUS / CHASSIS_L)    // 正解系数：麦轮/全向轮X型
#define K_CROSS_A (OMNI_CROSS_A / WHEEL_RADIUS) // 十字型逆解系数：前后轮
#define K_CROSS_B (OMNI_CROSS_B / WHEEL_RADIUS) // 十字型逆解系数：左右轮

// 全向轮X型 √2 系数
#define SQRT2 1.41421356f

/**
 * @brief 任务STACK大小
 */
#define CMD_STACK_SIZE 256     // 遥控256
#define CHASSIS_STACK_SIZE 256 // 底盘256
#define MOTOR_STACK_SIZE 256   // 电机256
#define ERROR_STACK_SIZE 256   // 错误256

#endif // __APP_CFG_H