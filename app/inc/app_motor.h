#ifndef __APP_MOTOR_H
#define __APP_MOTOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

__attribute__((noreturn)) void StartMotorTask(void *argument);

#endif // !__APP_MOTOR_H
