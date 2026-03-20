# FreeRTOS 静态内存分配 API 指南

> 本项目要求：不使用 CMSIS-OS，使用 FreeRTOS 原生 API，必须使用静态内存分配
> FreeRTOS 版本：V10.3.1

---

## 一、配置确认

在 `FreeRTOSConfig.h` 中已配置：

```c
#define configSUPPORT_STATIC_ALLOCATION          1    // 启用静态分配
#define configSUPPORT_DYNAMIC_ALLOCATION         0    // 禁用动态分配
```

**重要**：启用静态分配后，必须实现以下两个回调函数（由空闲任务和定时器任务调用）：

```c
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
```

---

## 二、静态内存分配 API 总览

| 对象类型   | 创建函数                                 | 静态结构体类型          |
| ---------- | ---------------------------------------- | ----------------------- |
| 任务       | `xTaskCreateStatic()`                    | `StaticTask_t`          |
| 队列       | `xQueueCreateStatic()`                   | `StaticQueue_t`         |
| 二值信号量 | `xSemaphoreCreateBinaryStatic()`         | `StaticSemaphore_t`     |
| 互斥量     | `xSemaphoreCreateMutexStatic()`          | `StaticSemaphore_t`     |
| 递归互斥量 | `xSemaphoreCreateRecursiveMutexStatic()` | `StaticSemaphore_t`     |
| 计数信号量 | `xSemaphoreCreateCountingStatic()`       | `StaticSemaphore_t`     |
| 软件定时器 | `xTimerCreateStatic()`                   | `StaticTimer_t`         |
| 事件组     | `xEventGroupCreateStatic()`              | `StaticEventGroup_t`    |
| 流缓冲区   | `xStreamBufferCreateStatic()`            | `StaticStreamBuffer_t`  |
| 消息缓冲区 | `xMessageBufferCreateStatic()`           | `StaticMessageBuffer_t` |

---

## 三、详细 API 说明

### 3.1 任务 (Task)

#### 函数原型

```c
TaskHandle_t xTaskCreateStatic(
    TaskFunction_t pxTaskCode,      // 任务函数指针
    const char * const pcName,      // 任务名称（调试用）
    const uint32_t ulStackDepth,    // 栈大小（单位：字，不是字节！）
    void * const pvParameters,      // 传递给任务的参数
    UBaseType_t uxPriority,         // 任务优先级
    StackType_t * const puxStackBuffer,    // 栈缓冲区
    StaticTask_t * const pxTaskBuffer      // 任务控制块缓冲区
);
```

#### 返回值

- 成功：返回任务句柄
- 失败：返回 `NULL`（当 `puxStackBuffer` 或 `pxTaskBuffer` 为 `NULL` 时）

#### 使用示例

```c
// 定义任务栈（栈大小为 200 字 = 800 字节）
#define TASK_STACK_SIZE 200
static StackType_t xTaskStack[ TASK_STACK_SIZE ];

// 定义任务控制块
static StaticTask_t xTaskTCB;

// 任务句柄
static TaskHandle_t xTaskHandle;

// 任务函数
void vMyTask( void *pvParameters )
{
    for( ;; )
    {
        // 任务代码
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }
}

// 创建任务
void vCreateTask( void )
{
    xTaskHandle = xTaskCreateStatic(
        vMyTask,            // 任务函数
        "MyTask",           // 任务名称
        TASK_STACK_SIZE,    // 栈大小（字）
        NULL,               // 参数
        tskIDLE_PRIORITY + 1,   // 优先级
        xTaskStack,         // 栈缓冲区
        &xTaskTCB           // TCB 缓冲区
    );

    // 静态分配不会返回 NULL（除非参数为 NULL）
    configASSERT( xTaskHandle != NULL );
}
```

#### 注意事项

- `ulStackDepth` 单位是**字（Word）**，不是字节。STM32F4 是 32 位 MCU，所以 1 字 = 4 字节
- 栈大小建议至少 128 字（512 字节）
- 栈空间用于：局部变量、函数调用、中断上下文保存

---

### 3.2 队列 (Queue)

#### 函数原型

```c
QueueHandle_t xQueueCreateStatic(
    UBaseType_t uxQueueLength,      // 队列长度（可容纳的最大消息数）
    UBaseType_t uxItemSize,         // 每个消息的大小（字节）
    uint8_t *pucQueueStorageBuffer, // 队列存储区缓冲区
    StaticQueue_t *pxQueueBuffer    // 队列控制块缓冲区
);
```

#### 返回值

- 成功：返回队列句柄
- 失败：返回 `NULL`

#### 使用示例

```c
// 定义队列参数
#define QUEUE_LENGTH 10
#define ITEM_SIZE sizeof( uint32_t )

// 队列存储区：大小 = 队列长度 × 每项大小
static uint8_t ucQueueStorage[ QUEUE_LENGTH * ITEM_SIZE ];

// 队列控制块
static StaticQueue_t xQueueBuffer;

// 队列句柄
static QueueHandle_t xQueue;

// 创建队列
void vCreateQueue( void )
{
    xQueue = xQueueCreateStatic(
        QUEUE_LENGTH,           // 队列长度
        ITEM_SIZE,              // 每项大小
        ucQueueStorage,         // 存储区
        &xQueueBuffer           // 控制块
    );

    configASSERT( xQueue != NULL );
}

// 发送数据
void vSendData( uint32_t ulValue )
{
    xQueueSend( xQueue, &ulValue, portMAX_DELAY );
}

// 接收数据
BaseType_t xReceiveData( uint32_t *pulValue )
{
    return xQueueReceive( xQueue, pulValue, pdMS_TO_TICKS( 100 ) );
}
```

#### 相关操作函数

| 函数                       | 功能       | 备注     |
| -------------------------- | ---------- | -------- |
| `xQueueSend()`             | 发送到队尾 | 可阻塞   |
| `xQueueSendToFront()`      | 发送到队首 | 可阻塞   |
| `xQueueSendFromISR()`      | 中断中发送 | 不可阻塞 |
| `xQueueReceive()`          | 接收并移除 | 可阻塞   |
| `xQueueReceiveFromISR()`   | 中断中接收 | 不可阻塞 |
| `xQueuePeek()`             | 查看不移除 | 可阻塞   |
| `uxQueueMessagesWaiting()` | 查询消息数 | -        |

---

### 3.3 二值信号量 (Binary Semaphore)

#### 函数原型

```c
SemaphoreHandle_t xSemaphoreCreateBinaryStatic( StaticSemaphore_t *pxSemaphoreBuffer );
```

#### 返回值

- 成功：返回信号量句柄
- 失败：返回 `NULL`

#### 特性

- 创建后初始值为空（需要先 Give 才能 Take）
- 用于任务同步（中断-任务、任务-任务）
- 不支持优先级继承

#### 使用示例

```c
static StaticSemaphore_t xBinarySemaphoreBuffer;
static SemaphoreHandle_t xBinarySemaphore;

// 创建二值信号量
void vCreateBinarySemaphore( void )
{
    xBinarySemaphore = xSemaphoreCreateBinaryStatic( &xBinarySemaphoreBuffer );
    configASSERT( xBinarySemaphore != NULL );
}

// 任务等待信号量
void vWaitingTask( void *pvParameters )
{
    for( ;; )
    {
        // 等待信号量（无限等待）
        if( xSemaphoreTake( xBinarySemaphore, portMAX_DELAY ) == pdTRUE )
        {
            // 获取到信号量，处理事件
        }
    }
}

// 中断中释放信号量
void EXTI0_IRQHandler( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if( __HAL_GPIO_EXTI_GET_IT( GPIO_PIN_0 ) )
    {
        __HAL_GPIO_EXTI_CLEAR_IT( GPIO_PIN_0 );

        // 释放信号量
        xSemaphoreGiveFromISR( xBinarySemaphore, &xHigherPriorityTaskWoken );

        // 如果唤醒了更高优先级任务，触发上下文切换
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
```

---

### 3.4 互斥量 (Mutex)

#### 函数原型

```c
SemaphoreHandle_t xSemaphoreCreateMutexStatic( StaticSemaphore_t *pxMutexBuffer );
```

#### 特性

- 创建后初始值为可用
- **支持优先级继承**，可防止优先级反转
- **不能在中断中使用**
- 必须由获取者释放

#### 使用示例

```c
static StaticSemaphore_t xMutexBuffer;
static SemaphoreHandle_t xMutex;

// 创建互斥量
void vCreateMutex( void )
{
    xMutex = xSemaphoreCreateMutexStatic( &xMutexBuffer );
    configASSERT( xMutex != NULL );
}

// 保护共享资源
void vAccessSharedResource( void )
{
    // 获取互斥量（等待最多 100ms）
    if( xSemaphoreTake( xMutex, pdMS_TO_TICKS( 100 ) ) == pdTRUE )
    {
        // 访问共享资源
        // ...

        // 释放互斥量
        xSemaphoreGive( xMutex );
    }
}
```

---

### 3.5 递归互斥量 (Recursive Mutex)

#### 函数原型

```c
SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic( StaticSemaphore_t *pxMutexBuffer );
```

#### 前提条件

- 需要在 `FreeRTOSConfig.h` 中设置：`configUSE_RECURSIVE_MUTEXES = 1`

#### 特性

- 允许同一任务多次获取
- 必须获取 N 次就要释放 N 次
- 不能在中断中使用

#### 使用示例

```c
static StaticSemaphore_t xRecursiveMutexBuffer;
static SemaphoreHandle_t xRecursiveMutex;

void vRecursiveFunction( void )
{
    // 第一次获取
    xSemaphoreTakeRecursive( xRecursiveMutex, portMAX_DELAY );

    // 第二次获取（同一任务）
    xSemaphoreTakeRecursive( xRecursiveMutex, portMAX_DELAY );

    // 第三次获取
    xSemaphoreTakeRecursive( xRecursiveMutex, portMAX_DELAY );

    // ... 访问资源 ...

    // 必须释放三次
    xSemaphoreGiveRecursive( xRecursiveMutex );
    xSemaphoreGiveRecursive( xRecursiveMutex );
    xSemaphoreGiveRecursive( xRecursiveMutex );
}
```

---

### 3.6 计数信号量 (Counting Semaphore)

#### 函数原型

```c
SemaphoreHandle_t xSemaphoreCreateCountingStatic(
    UBaseType_t uxMaxCount,         // 最大计数值
    UBaseType_t uxInitialCount,     // 初始计数值
    StaticSemaphore_t *pxSemaphoreBuffer
);
```

#### 典型应用场景

1. **事件计数**
   - `uxMaxCount` = 期望的最大未处理事件数
   - `uxInitialCount` = 0

2. **资源管理**
   - `uxMaxCount` = 资源总数
   - `uxInitialCount` = 资源总数

#### 使用示例

```c
// 场景：管理 5 个缓冲区资源
#define BUFFER_COUNT 5

static StaticSemaphore_t xCountingSemaphoreBuffer;
static SemaphoreHandle_t xCountingSemaphore;

void vCreateCountingSemaphore( void )
{
    xCountingSemaphore = xSemaphoreCreateCountingStatic(
        BUFFER_COUNT,   // 最大计数
        BUFFER_COUNT,   // 初始计数（所有资源可用）
        &xCountingSemaphoreBuffer
    );
}

// 获取资源
BaseType_t xGetBuffer( void )
{
    return xSemaphoreTake( xCountingSemaphore, pdMS_TO_TICKS( 100 ) );
}

// 释放资源
void vReleaseBuffer( void )
{
    xSemaphoreGive( xCountingSemaphore );
}
```

---

### 3.7 软件定时器 (Software Timer)

#### 函数原型

```c
TimerHandle_t xTimerCreateStatic(
    const char * const pcTimerName,     // 定时器名称
    const TickType_t xTimerPeriodInTicks, // 周期（Tick）
    const UBaseType_t uxAutoReload,     // pdTRUE=周期性, pdFALSE=单次
    void * const pvTimerID,             // 定时器ID（用于回调识别）
    TimerCallbackFunction_t pxCallbackFunction, // 回调函数
    StaticTimer_t *pxTimerBuffer        // 定时器控制块
);
```

#### 前提条件

- 需要在 `FreeRTOSConfig.h` 中设置：`configUSE_TIMERS = 1`

#### 使用示例

```c
static StaticTimer_t xTimerBuffer;
static TimerHandle_t xPeriodicTimer;

// 定时器回调函数
void vTimerCallback( TimerHandle_t xTimer )
{
    // 定时器到期时执行的操作
    // 注意：回调在定时器服务任务中执行，不要阻塞！
}

// 创建并启动周期定时器
void vCreateTimer( void )
{
    xPeriodicTimer = xTimerCreateStatic(
        "PeriodicTimer",                // 名称
        pdMS_TO_TICKS( 1000 ),          // 周期 1000ms
        pdTRUE,                         // 周期性
        NULL,                           // Timer ID
        vTimerCallback,                 // 回调函数
        &xTimerBuffer                   // 控制块
    );

    // 启动定时器
    xTimerStart( xPeriodicTimer, 0 );
}

// 定时器控制函数
void vControlTimer( void )
{
    xTimerStart( xPeriodicTimer, pdMS_TO_TICKS( 100 ) );    // 启动
    xTimerStop( xPeriodicTimer, pdMS_TO_TICKS( 100 ) );     // 停止
    xTimerReset( xPeriodicTimer, pdMS_TO_TICKS( 100 ) );    // 重置
    xTimerChangePeriod( xPeriodicTimer, pdMS_TO_TICKS( 500 ), 0 ); // 改变周期
}
```

#### 注意事项

- 定时器回调在**定时器服务任务**中执行，不是在中断中
- 回调函数**不能阻塞**（不能调用 `vTaskDelay`、`xQueueReceive` 等带阻塞的函数）
- 定时器服务任务优先级由 `configTIMER_TASK_PRIORITY` 定义

---

### 3.8 事件组 (Event Group)

#### 函数原型

```c
EventGroupHandle_t xEventGroupCreateStatic( StaticEventGroup_t *pxEventGroupBuffer );
```

#### 特性

- 可用于多任务同步
- 可用位数取决于 `configUSE_16_BIT_TICKS`：
  - `configUSE_16_BIT_TICKS = 1`：8 位可用（bit 0-7）
  - `configUSE_16_BIT_TICKS = 0`：24 位可用（bit 0-23）

#### 使用示例

```c
// 定义事件位
#define BIT_0   ( 1 << 0 )
#define BIT_1   ( 1 << 1 )
#define BIT_2   ( 1 << 2 )

static StaticEventGroup_t xEventGroupBuffer;
static EventGroupHandle_t xEventGroup;

// 创建事件组
void vCreateEventGroup( void )
{
    xEventGroup = xEventGroupCreateStatic( &xEventGroupBuffer );
}

// 等待事件
void vWaitingTask( void *pvParameters )
{
    EventBits_t uxBits;

    for( ;; )
    {
        // 等待 BIT_0 或 BIT_1，任意一个满足即可
        uxBits = xEventGroupWaitBits(
            xEventGroup,        // 事件组句柄
            BIT_0 | BIT_1,      // 等待的位
            pdTRUE,             // 退出时清除位
            pdFALSE,            // 不需要全部满足
            portMAX_DELAY       // 无限等待
        );

        if( uxBits & BIT_0 )
        {
            // BIT_0 事件发生
        }
        if( uxBits & BIT_1 )
        {
            // BIT_1 事件发生
        }
    }
}

// 设置事件
void vSetEvent( void )
{
    xEventGroupSetBits( xEventGroup, BIT_0 );
}

// 中断中设置事件
void vSetEventFromISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xEventGroupSetBitsFromISR( xEventGroup, BIT_1, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
```

#### 同步点（Rendezvous）

```c
// 多任务同步：等待所有任务都到达同步点
#define TASK_0_BIT  ( 1 << 0 )
#define TASK_1_BIT  ( 1 << 1 )
#define ALL_SYNC_BITS ( TASK_0_BIT | TASK_1_BIT )

void vSyncTask0( void *pvParameters )
{
    for( ;; )
    {
        // 设置自己的位，等待其他任务
        xEventGroupSync( xEventGroup, TASK_0_BIT, ALL_SYNC_BITS, portMAX_DELAY );
        // 所有任务都到达后继续执行
    }
}
```

---

### 3.9 流缓冲区 (Stream Buffer)

#### 函数原型

```c
StreamBufferHandle_t xStreamBufferCreateStatic(
    size_t xBufferSizeBytes,            // 缓冲区大小（字节）
    size_t xTriggerLevelBytes,          // 触发级别
    uint8_t *pucStreamBufferStorageArea,// 存储区
    StaticStreamBuffer_t *pxStaticStreamBuffer // 控制块
);
```

#### 特性

- 用于连续数据流传输
- **单写单读**模式（可以有多个写者/读者，但需要额外保护）
- 适合中断-任务、核间通信

#### 使用示例

```c
#define STREAM_BUFFER_SIZE  100

static uint8_t ucStreamBufferStorage[ STREAM_BUFFER_SIZE + 1 ]; // 需要 +1
static StaticStreamBuffer_t xStreamBufferStruct;
static StreamBufferHandle_t xStreamBuffer;

// 创建流缓冲区
void vCreateStreamBuffer( void )
{
    xStreamBuffer = xStreamBufferCreateStatic(
        STREAM_BUFFER_SIZE,     // 大小
        1,                      // 触发级别：1字节触发
        ucStreamBufferStorage,  // 存储区
        &xStreamBufferStruct    // 控制块
    );
}

// 发送数据
size_t xSendData( uint8_t *pData, size_t xLength )
{
    return xStreamBufferSend( xStreamBuffer, pData, xLength, pdMS_TO_TICKS( 100 ) );
}

// 接收数据
size_t xReceiveData( uint8_t *pBuffer, size_t xBufferLength )
{
    return xStreamBufferReceive( xStreamBuffer, pBuffer, xBufferLength, portMAX_DELAY );
}

// 中断中发送
void vSendFromISR( uint8_t *pData, size_t xLength )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xStreamBufferSendFromISR( xStreamBuffer, pData, xLength, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
```

---

### 3.10 消息缓冲区 (Message Buffer)

#### 函数原型

```c
MessageBufferHandle_t xMessageBufferCreateStatic(
    size_t xBufferSizeBytes,                // 缓冲区大小
    uint8_t *pucMessageBufferStorageArea,   // 存储区
    StaticMessageBuffer_t *pxStaticMessageBuffer // 控制块
);
```

#### 特性

- 基于流缓冲区实现
- 支持可变长度消息
- 每条消息额外占用 `sizeof(size_t)` 字节存储长度
- **单写单读**模式

#### 使用示例

```c
#define MESSAGE_BUFFER_SIZE  100

static uint8_t ucMessageBufferStorage[ MESSAGE_BUFFER_SIZE + 1 ];
static StaticMessageBuffer_t xMessageBufferStruct;
static MessageBufferHandle_t xMessageBuffer;

// 创建消息缓冲区
void vCreateMessageBuffer( void )
{
    xMessageBuffer = xMessageBufferCreateStatic(
        MESSAGE_BUFFER_SIZE,
        ucMessageBufferStorage,
        &xMessageBufferStruct
    );
}

// 发送消息
void vSendMessage( char *pcMessage )
{
    size_t xLength = strlen( pcMessage );
    xMessageBufferSend( xMessageBuffer, pcMessage, xLength, portMAX_DELAY );
}

// 接收消息
void vReceiveMessage( void )
{
    char cBuffer[ 50 ];
    size_t xReceivedBytes;

    xReceivedBytes = xMessageBufferReceive( xMessageBuffer, cBuffer, sizeof( cBuffer ), portMAX_DELAY );

    if( xReceivedBytes > 0 )
    {
        cBuffer[ xReceivedBytes ] = '\0';
        // 处理消息
    }
}
```

---

## 四、常用辅助宏与函数

### 4.1 时间转换

```c
// 毫秒转 Tick
TickType_t xTicks = pdMS_TO_TICKS( 100 );  // 100ms

// 无限等待
portMAX_DELAY

// 不等待（立即返回）
0
```

### 4.2 任务通知（轻量级同步）

任务通知比信号量更快、更省内存，适合简单同步场景：

```c
// 发送通知（任务中）
xTaskNotifyGive( xTaskHandle );

// 发送通知（中断中）
vTaskNotifyGiveFromISR( xTaskHandle, &xHigherPriorityTaskWoken );

// 等待通知
ulTaskNotifyTake( pdTRUE, portMAX_DELAY );  // 清零并等待

// 设置通知值
xTaskNotify( xTaskHandle, ulValue, eSetBits );

// 等待通知值
uint32_t ulValue;
xTaskNotifyWait( 0, 0xFFFFFFFF, &ulValue, portMAX_DELAY );
```

### 4.3 临界区

```c
// 任务中
taskENTER_CRITICAL();
// 临界区代码
taskEXIT_CRITICAL();

// 中断中
UBaseType_t uxSavedStatus = taskENTER_CRITICAL_FROM_ISR();
// 临界区代码
taskEXIT_CRITICAL_FROM_ISR( uxSavedStatus );
```

### 4.4 延时

```c
// 相对延时
vTaskDelay( pdMS_TO_TICKS( 100 ) );  // 延时 100ms

// 绝对延时（周期性任务推荐）
TickType_t xLastWakeTime = xTaskGetTickCount();
const TickType_t xFrequency = pdMS_TO_TICKS( 100 );

for( ;; )
{
    // 等待下一个周期
    vTaskDelayUntil( &xLastWakeTime, xFrequency );
    // 任务代码
}
```

---

## 五、内存需求参考

### 5.1 各对象内存需求

| 对象类型   | 控制块大小（约） | 额外存储          |
| ---------- | ---------------- | ----------------- |
| 任务       | ~300 字节        | 栈（用户定义）    |
| 队列       | ~80 字节         | 队列长度 × 项大小 |
| 信号量     | ~80 字节         | 无                |
| 互斥量     | ~80 字节         | 无                |
| 定时器     | ~80 字节         | 无                |
| 事件组     | ~40 字节         | 无                |
| 流缓冲区   | ~60 字节         | 用户定义大小 + 1  |
| 消息缓冲区 | ~60 字节         | 用户定义大小 + 1  |

### 5.2 内存规划建议

```c
// 典型配置示例

// 任务
#define TASK1_STACK_SIZE    256     // 1024 字节
static StackType_t xTask1Stack[ TASK1_STACK_SIZE ];
static StaticTask_t xTask1TCB;

// 队列
#define CMD_QUEUE_LENGTH    20
#define CMD_ITEM_SIZE       sizeof( Cmd_t )
static uint8_t ucCmdQueueStorage[ CMD_QUEUE_LENGTH * CMD_ITEM_SIZE ];
static StaticQueue_t xCmdQueueBuffer;

// 互斥量
static StaticSemaphore_t xUartMutexBuffer;
```

---

## 六、常见错误与解决方案

### 6.1 忘记实现 Idle/Timer 任务内存回调

**现象**：程序卡死在 `vApplicationGetIdleTaskMemory` 断言
**解决**：实现两个回调函数（见第一章）

### 6.2 栈溢出

**现象**：HardFault 或随机崩溃
**解决**：

- 增大栈大小
- 启用栈溢出检测：`configCHECK_FOR_STACK_OVERFLOW = 2`
- 实现 `vApplicationStackOverflowHook()`

### 6.3 中断中使用了阻塞 API

**现象**：程序卡死
**解决**：使用 `FromISR` 后缀的 API，阻塞时间设为 0

### 6.4 互斥量在中断中使用

**现象**：断言失败
**解决**：互斥量只能在任务中使用，中断中使用二值信号量

### 6.5 定时器回调中阻塞

**现象**：定时器服务任务卡死
**解决**：回调函数中不要使用带阻塞的 API

---

## 七、参考链接

- [FreeRTOS 官方文档](https://www.freertos.org/Documentation/RTOS_book.html)
- [FreeRTOS API 参考](https://www.freertos.org/a00106.html)
- [静态内存分配](https://www.freertos.org/Static_Memory_Allocation.html)
