#ifndef __APP_ROBOT_H
#define __APP_ROBOT_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
//
#include "app_cmd.h"
#include "app_chassis.h"
#include "app_motor.h"
#include "app_error.h"

/* 任务句柄声明 */
extern TaskHandle_t errorTaskHandle;
extern TaskHandle_t cmdTaskHandle;
extern TaskHandle_t chassisTaskHandle;
extern TaskHandle_t motorTaskHandle;

/* 任务创建函数 */
void function_in_main_c(void);

#endif // !__APP_ROBOT_H
