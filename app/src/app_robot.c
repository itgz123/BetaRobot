#include "app_robot.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"

/* 任务句柄定义 */
TaskHandle_t errorTaskHandle;
TaskHandle_t cmdTaskHandle;
TaskHandle_t chassisTaskHandle;
TaskHandle_t motorTaskHandle;

/* 任务栈定义 */
static StackType_t errorTaskStack[ERROR_STACK_SIZE];
static StackType_t cmdTaskStack[CMD_STACK_SIZE];
static StackType_t chassisTaskStack[CHASSIS_STACK_SIZE];
static StackType_t motorTaskStack[MOTOR_STACK_SIZE];

/* 任务控制块定义 */
static StaticTask_t errorTaskTCB;
static StaticTask_t cmdTaskTCB;
static StaticTask_t chassisTaskTCB;
static StaticTask_t motorTaskTCB;

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

    // 创建任务（静态分配）
    errorTaskHandle = xTaskCreateStatic(
        StartErrorTask, "errorTask", ERROR_STACK_SIZE,
        NULL, 0, errorTaskStack, &errorTaskTCB);

    cmdTaskHandle = xTaskCreateStatic(
        StartCmdTask, "cmdTask", CMD_STACK_SIZE,
        NULL, 1, cmdTaskStack, &cmdTaskTCB);

    chassisTaskHandle = xTaskCreateStatic(
        StartChassisTask, "chassisTask", CHASSIS_STACK_SIZE,
        NULL, 2, chassisTaskStack, &chassisTaskTCB);

    motorTaskHandle = xTaskCreateStatic(
        StartMotorTask, "motorTask", MOTOR_STACK_SIZE,
        NULL, 3, motorTaskStack, &motorTaskTCB);
}