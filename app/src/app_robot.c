#include "app_robot.h"
#include "bsp_log.h"
#include "bsp_dwt.h"

/* 任务句柄定义 */
osThreadId errorTaskHandle;
osThreadId cmdTaskHandle;
osThreadId chassisTaskHandle;
osThreadId motorTaskHandle;

/* 任务栈缓冲区定义 */
uint32_t errorTaskBuffer[128];
uint32_t cmdTaskBuffer[256];
uint32_t chassisTaskBuffer[256];
uint32_t motorTaskBuffer[128];

/* 任务控制块定义 */
osStaticThreadDef_t errorTaskControlBlock;
osStaticThreadDef_t cmdTaskControlBlock;
osStaticThreadDef_t chassisTaskControlBlock;
osStaticThreadDef_t motorTaskControlBlock;

/**
 * @brief  应用任务初始化
 * @param  None
 * @retval None
 */
void function_in_main_c(void)
{
    __disable_irq();
    DWT_Init();
    BSPLogInit();
    __enable_irq();
    LOGINFO("[robot] DWT_Init() and BSPLogInit() done");

    // 创建任务
    osThreadStaticDef(errorTask, StartErrorTask, 0, 0, 128, errorTaskBuffer, &errorTaskControlBlock);
    errorTaskHandle = osThreadCreate(osThread(errorTask), NULL);

    osThreadStaticDef(cmdTask, StartCmdTask, 1, 0, 256, cmdTaskBuffer, &cmdTaskControlBlock);
    cmdTaskHandle = osThreadCreate(osThread(cmdTask), NULL);

    osThreadStaticDef(chassisTask, StartChassisTask, 2, 0, 256, chassisTaskBuffer, &chassisTaskControlBlock);
    chassisTaskHandle = osThreadCreate(osThread(chassisTask), NULL);

    osThreadStaticDef(motorTask, StartMotorTask, 3, 0, 128, motorTaskBuffer, &motorTaskControlBlock);
    motorTaskHandle = osThreadCreate(osThread(motorTask), NULL);
}
