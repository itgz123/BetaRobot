#ifndef __APP_ROBOT_H
#define __APP_ROBOT_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
//
#include "app_cmd.h"
#include "app_chassis.h"
#include "app_motor.h"
#include "app_error.h"

/* 任务句柄声明 */
extern osThreadId errorTaskHandle;
extern osThreadId cmdTaskHandle;
extern osThreadId chassisTaskHandle;
extern osThreadId motorTaskHandle;

/* 任务创建函数 */
void function_in_main_c(void);

#endif // !__APP_ROBOT_H
