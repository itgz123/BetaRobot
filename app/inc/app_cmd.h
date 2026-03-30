/**
 * @file app_cmd.h
 * @brief 遥控器任务模块头文件
 */

#ifndef __APP_CMD_H
#define __APP_CMD_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

extern float ch1;
extern float ch2;
extern float ch3;
extern float ch4;

/*------------- 任务函数 --------------*/
__attribute__((noreturn)) void StartCmdTask(void *argument);

#endif // !__APP_CMD_H