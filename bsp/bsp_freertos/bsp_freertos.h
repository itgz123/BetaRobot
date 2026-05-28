/**
 * @file bsp_freertos.h
 * @brief FreeRTOS 任务和队列静态创建封装
 */

#ifndef __BSP_FREERTOS_H
#define __BSP_FREERTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*============================================
 *              任务封装
 *============================================*/

/**
 * @brief 任务实例结构体
 */
typedef struct TaskInstance
{
    TaskHandle_t handle; // 任务句柄
    StackType_t *stack;  // 栈缓冲区
    StaticTask_t *tcb;   // TCB 缓冲区
    const char *name;    // 任务名称
    uint32_t stack_size; // 栈大小（字）
} TaskInstance;

/**
 * @brief 任务实例定义宏
 * @param name_       实例名称（snake_case）
 * @param stack_sz    栈大小（单位：字，1字=4字节）
 */
#define TASK_INSTANCE_DEF(name_, stack_sz)      \
    static StackType_t name_##_stack[stack_sz]; \
    static StaticTask_t name_##_tcb;            \
    static TaskInstance name_ = {               \
        .handle = NULL,                         \
        .stack = name_##_stack,                 \
        .tcb = &name_##_tcb,                    \
        .name = #name_,                         \
        .stack_size = stack_sz,                 \
    }

/**
 * @brief 任务初始化配置结构体
 */
typedef struct
{
    TaskFunction_t func;  // 任务函数
    UBaseType_t priority; // 优先级
} Task_Init_Config_s;

/**
 * @brief 注册并创建任务
 * @param inst   任务实例指针（由 TASK_INSTANCE_DEF 定义）
 * @param config 初始化配置结构体指针
 * @return 任务句柄
 */
TaskHandle_t TaskRegister(TaskInstance *inst, const Task_Init_Config_s *config);

/*============================================
 *              队列封装
 *============================================*/

/**
 * @brief 队列实例结构体
 */
typedef struct QueueInstance
{
    QueueHandle_t handle;  // 队列句柄
    uint8_t *storage;      // 存储区缓冲区
    StaticQueue_t *buffer; // 队列控制块缓冲区
    UBaseType_t length;    // 队列长度
    UBaseType_t item_size; // 数据项大小（字节）
} QueueInstance;

/**
 * @brief 队列实例定义宏
 * @param name_      实例名称（snake_case）
 * @param len        队列长度
 * @param item_type  数据项类型
 */
#define QUEUE_INSTANCE_DEF(name_, len, item_type)            \
    static uint8_t name_##_storage[len * sizeof(item_type)]; \
    static StaticQueue_t name_##_buffer;                     \
    static QueueInstance name_ = {                           \
        .handle = NULL,                                      \
        .storage = name_##_storage,                          \
        .buffer = &name_##_buffer,                           \
        .length = len,                                       \
        .item_size = sizeof(item_type),                      \
    }

/**
 * @brief 注册并创建队列
 * @param inst 队列实例指针（由 QUEUE_INSTANCE_DEF 定义）
 * @return 队列句柄
 */
QueueHandle_t QueueRegister(QueueInstance *inst);

#endif // __BSP_FREERTOS_H