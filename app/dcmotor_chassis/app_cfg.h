/**
 * @file app_cfg.h
 * @brief APP层配置文件：开发板选择、应用参数配 *
 * @note 本文件职责：
 *       1. 选择开发板（DEVELOPMENT_BOARD *
 *  2. 配置应用层参数（任务频率、底盘参数等 *
 * @note 切换开发板：只需修改 DEVELOPMENT_BOARD *
 *     BSP层会自动适配硬件映射
 */

#ifndef __APP_CFG_H
#define __APP_CFG_H

/*============================================
 *              开发板选择
 *============================================*/

/**
 * @brief 开发板类型定义
 *
 * | 宏定�?        | 主控型号        | hal目录�?     |
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

/*============================================
 *              任务频率设置
 *============================================*/

#define CMD_FREQ_MS 100   // 10Hz 遥控
#define CHASSIS_FREQ_MS 5 // 200Hz 底盘
#define MOTOR_FREQ_MS 1   // 1000Hz 电机
#define SENSOR_FREQ_MS 1  // 1000Hz 传感器
#define ERROR_FREQ_MS 100 // 10Hz 错误

/*============================================
 *              底盘类型选择
 *============================================*/

// 修改此处切换底盘类型
#define CHASSIS_TYPE CHASSIS_TYPE_MECANUM_X

/*============================================
 *              底盘几何参数
 *============================================*/

#define WHEEL_RADIUS 0.034f // 轮半�?(m)

// 矩形底盘参数（麦轮X/O型、全向轮X型共用）
#define WHEELBASE_A 0.3f // 中心到前后轮的纵向距�?(m)
#define WHEELBASE_B 0.3f // 中心到左右轮的横向距�?(m)

// 中心到轮子距离：L = sqrt(a² + b²)
#define CHASSIS_L 0.4242f // 中心到轮子距�?(m)

// 十字型全向轮参数（支持矩形）
#define OMNI_CROSS_A 0.2f // 中心到前后轮距离 (m)
#define OMNI_CROSS_B 0.2f // 中心到左右轮距离 (m)

// 底盘最大速度
#define CHASSIS_MAX_SPEED 2.5f
#define CHASSIS_MAX_W 0.2f

/*============================================
 *              任务STACK大小
 *============================================*/

#define CMD_STACK_SIZE 512
#define CHASSIS_STACK_SIZE 512
#define MOTOR_STACK_SIZE 512
#define SENSOR_STACK_SIZE 512
#define ERROR_STACK_SIZE 512

#endif // __APP_CFG_H
