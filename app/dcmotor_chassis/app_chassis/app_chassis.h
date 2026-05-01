#ifndef __APP_CHASSIS_H
#define __APP_CHASSIS_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

__attribute__((noreturn)) void StartChassisTask(void *argument);

#endif // !__APP_CHASSIS_H
