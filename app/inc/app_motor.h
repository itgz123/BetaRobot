#ifndef __APP_MOTOR_H
#define __APP_MOTOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

void StartMotorTask(void const *argument);

#endif // !__APP_MOTOR_H
