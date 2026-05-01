/**
 * @file app_sensor.h
 * @brief 传感器任务模块：IMU 数据采集与处理
 */

#ifndef __APP_SENSOR_H
#define __APP_SENSOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/**
 * @brief 传感器任务入口函数
 * @param argument 未使用
 */
__attribute__((noreturn)) void StartSensorTask(void *argument);

#endif // !__APP_SENSOR_H
