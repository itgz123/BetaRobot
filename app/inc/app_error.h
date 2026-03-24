#ifndef __APP_ERROR_H
#define __APP_ERROR_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

__attribute__((noreturn)) void StartErrorTask(void *argument);

#endif // !__APP_ERROR_H
