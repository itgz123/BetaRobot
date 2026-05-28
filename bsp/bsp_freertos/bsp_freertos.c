/**
 * @file bsp_freertos.c
 * @brief FreeRTOS 任务和队列静态创建封装实现
 */

#include "bsp_freertos.h"

TaskHandle_t TaskRegister(TaskInstance *inst, const Task_Init_Config_s *config)
{
    configASSERT(inst != NULL);
    configASSERT(config != NULL);
    configASSERT(config->func != NULL);

    inst->handle = xTaskCreateStatic(
        config->func,
        inst->name,
        inst->stack_size,
        NULL,
        config->priority,
        inst->stack,
        inst->tcb);

    return inst->handle;
}

QueueHandle_t QueueRegister(QueueInstance *inst)
{
    configASSERT(inst != NULL);

    inst->handle = xQueueCreateStatic(
        inst->length,
        inst->item_size,
        inst->storage,
        inst->buffer);

    return inst->handle;
}