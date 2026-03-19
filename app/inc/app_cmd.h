#ifndef __APP_CMD_H
#define __APP_CMD_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

void StartCmdTask(void const *argument);

#endif // !__APP_CMD_H
