---
---

# STM32CubeMX FreeRTOS 配置指南

> 本文档按照重要性排序，帮助你在 CubeMX 中正确配置 FreeRTOS。

---

## 目录

1. [关键前置配置（必看）](#1-关键前置配置必看)
2. [内核基础配置](#2-内核基础配置)
3. [内存配置](#3-内存配置)
4. [任务与调度配置](#4-任务与调度配置)
5. [同步与通信机制配置](#5-同步与通信机制配置)
6. [软件定时器配置](#6-软件定时器配置)
7. [调试与诊断配置](#7-调试与诊断配置)
8. [钩子函数配置](#8-钩子函数配置)
9. [中断优先级配置](#9-中断优先级配置)
10. [API 函数裁剪配置](#10-api-函数裁剪配置)
11. [配置验证与调试技巧](#11-配置验证与调试技巧)

---

## 1. 关键前置配置（必看）

### 1.1 时基源设置（Timebase Source）⭐⭐⭐⭐⭐

**这是使用 FreeRTOS 时最容易出错的地方！**

当启用 FreeRTOS 后，**必须为 HAL 库设置一个非 SysTick 的定时器作为时基源**。因为 FreeRTOS 会强制使用 SysTick 作为自己的心跳。

**配置步骤：**
- 进入 `System Core` → `SYS`
- 将 `Timebase Source` 从 `SysTick` 改为其他定时器（推荐 `TIM6` 或 `TIM7`）

**原因：**
- SysTick 被 FreeRTOS 占用，优先级最低
- 如果 HAL 时基也用 SysTick，在高优先级中断中调用 `HAL_Delay()` 会导致死锁

### 1.2 接口版本选择（CMSIS-RTOS V1 vs V2）⭐⭐⭐⭐

在 `Middleware` → `FREERTOS` 中选择接口版本：

| 版本 | 特点 | 适用场景 |
|------|------|----------|
| **CMSIS_V1** | 内存占用小，API 简洁 | Cortex-M3/M4/M7，资源受限项目 |
| **CMSIS_V2** | 功能更全，兼容更多硬件 | 需要更多功能，资源充足的项目 |

**建议：** 对于 STM32F4 系列，选择 **CMSIS_V1** 即可满足大多数需求。

---

## 2. 内核基础配置

### 2.1 调度模式（USE_PREEMPTION）⭐⭐⭐⭐⭐

| 值 | 模式 | 说明 |
|----|------|------|
| `1`（Enable） | **抢占式调度**（推荐） | 高优先级任务可打断低优先级任务 |
| `0`（Disable） | 协作式调度 | 任务需主动调用 `taskYIELD()` 让出 CPU |

**建议：** 默认使用抢占式调度 `1`。

### 2.2 时钟节拍频率（TICK_RATE_HZ）⭐⭐⭐⭐

```c
#define configTICK_RATE_HZ ((TickType_t)1000)
```

**含义：** FreeRTOS 的时钟节拍频率，设置为 1000 表示 1 秒中断 1000 次，即每 1ms 进行一次任务调度。

**建议值：**
- 一般应用：`1000`（1ms 时间片）
- 低功耗应用：`100`（10ms 时间片）

**注意：** 值越大，任务切换越频繁，CPU 开销增加。

### 2.3 最大优先级数（MAX_PRIORITIES）⭐⭐⭐⭐

```c
#define configMAX_PRIORITIES ( 56 )
```

**含义：** 任务可用的优先级数量。例如设为 7，则优先级范围为 0~6。

**建议：**
- 简单应用：`5~7` 足够
- 复杂应用：`10~15`
- 数值越大，内核开销越大

---

## 3. 内存配置

### 3.1 总堆大小（TOTAL_HEAP_SIZE）⭐⭐⭐⭐⭐

```c
#define configTOTAL_HEAP_SIZE ((size_t)15360)  // 15KB
```

**这是 FreeRTOS 最关键的内存配置！**

**计算方法：**
```
总堆大小 = 内核基础开销（1~2KB）
         + 所有任务栈大小总和
         + 所有队列/信号量开销
         + 30% 安全余量
```

**典型配置范围：**

| 应用场景 | 建议堆大小 |
|----------|------------|
| 极简系统（1-2 个任务） | 4KB ~ 8KB |
| 中等复杂度（多任务+队列） | 8KB ~ 16KB |
| 复杂系统（网络协议栈） | 16KB ~ 64KB |

### 3.2 空闲任务栈大小（MINIMAL_STACK_SIZE）⭐⭐⭐

```c
#define configMINIMAL_STACK_SIZE ((uint16_t)128)
```

**含义：** 空闲任务（IDLE Task）的栈大小，单位为字（word）。

**建议值：** 64~768 之间，根据实际需求调整。

### 3.3 内存分配方式⭐⭐⭐

```c
#define configSUPPORT_STATIC_ALLOCATION   1  // 静态分配
#define configSUPPORT_DYNAMIC_ALLOCATION  1  // 动态分配
```

| 方式 | 优点 | 缺点 |
|------|------|------|
| **动态分配** | 灵活，按需创建 | 可能有内存碎片 |
| **静态分配** | 确定性高，无碎片 | 需预先分配，不够灵活 |

**CubeMX 默认使用 heap_4 方案**，适合大多数应用，支持内存碎片合并。

### 3.4 任务栈溢出检测（CHECK_FOR_STACK_OVERFLOW）⭐⭐⭐⭐

```c
#define configCHECK_FOR_STACK_OVERFLOW  0  // 禁用
// 或
#define configCHECK_FOR_STACK_OVERFLOW  1  // 方法1：检测栈指针越界
// 或
#define configCHECK_FOR_STACK_OVERFLOW  2  // 方法2：检测栈被破坏（推荐）
```

**建议：** 开发阶段设置为 `2`，便于发现栈溢出问题。

**注意：** 启用后需实现钩子函数：
```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
```

---

## 4. 任务与调度配置

### 4.1 任务名称最大长度（MAX_TASK_NAME_LEN）⭐⭐

```c
#define configMAX_TASK_NAME_LEN ( 16 )
```

**建议：** 12~255 之间，根据调试需求设置。

### 4.2 空闲任务让出 CPU（IDLE_SHOULD_YIELD）⭐⭐

```c
#define configIDLE_SHOULD_YIELD  1
```

**含义：** 当使能时，空闲任务在执行完空闲工作后会让出 CPU 给其他同优先级的用户任务。

### 4.3 时间片调度（USE_TIME_SLICING）⭐⭐⭐

```c
#define configUSE_TIME_SLICING  1  // 默认开启
```

**含义：** 使能后，同优先级的任务会在每个 Tick 中轮流执行。

### 4.4 16位 Tick 类型（USE_16_BIT_TICKS）⭐⭐

```c
#define configUSE_16_BIT_TICKS  0
```

| 值 | TickType_t 类型 | 适用场景 |
|----|-----------------|----------|
| `0` | 32位无符号整数 | 32位 MCU（推荐） |
| `1` | 16位无符号整数 | 8/16位 MCU |

---

## 5. 同步与通信机制配置

### 5.1 互斥量（USE_MUTEXES）⭐⭐⭐⭐

```c
#define configUSE_MUTEXES  1
```

**建议：** 开启，用于保护共享资源，防止优先级翻转。

### 5.2 递归互斥量（USE_RECURSIVE_MUTEXES）⭐⭐⭐

```c
#define configUSE_RECURSIVE_MUTEXES  1
```

**含义：** 允许同一个任务多次获取同一个互斥量。

### 5.3 计数信号量（USE_COUNTING_SEMAPHORES）⭐⭐⭐

```c
#define configUSE_COUNTING_SEMAPHORES  1
```

**建议：** 开启，用于资源计数和事件同步。

### 5.4 任务通知（USE_TASK_NOTIFICATIONS）⭐⭐⭐⭐

```c
#define configUSE_TASK_NOTIFICATIONS  1
```

**特点：** 
- 比信号量/队列更轻量
- 每个 Task 都有一个 32 位通知值
- **强烈建议开启**

### 5.5 队列注册表大小（QUEUE_REGISTRY_SIZE）⭐⭐

```c
#define configQUEUE_REGISTRY_SIZE  8
```

**含义：** 可注册的队列和信号量数量，用于调试时查看队列名称。

---

## 6. 软件定时器配置

### 6.1 使能软件定时器（USE_TIMERS）⭐⭐⭐

```c
#define configUSE_TIMERS  1
```

**建议：** 开启，提供软件定时器功能。

### 6.2 定时器任务优先级（TIMER_TASK_PRIORITY）⭐⭐⭐

```c
#define configTIMER_TASK_PRIORITY  ( 2 )
```

**建议：** 设置为中等优先级，不宜过高或过低。

### 6.3 定时器命令队列长度（TIMER_QUEUE_LENGTH）⭐⭐

```c
#define configTIMER_QUEUE_LENGTH  10
```

### 6.4 定时器任务栈深度（TIMER_TASK_STACK_DEPTH）⭐⭐

```c
#define configTIMER_TASK_STACK_DEPTH  256
```

---

## 7. 调试与诊断配置

### 7.1 跟踪调试（USE_TRACE_FACILITY）⭐⭐⭐

```c
#define configUSE_TRACE_FACILITY  1
```

**含义：** 启用可视化跟踪调试功能，配合 `vTaskList()` 使用。

### 7.2 队列注册表（QUEUE_REGISTRY_SIZE）⭐⭐

```c
#define configQUEUE_REGISTRY_SIZE  8
```

---

## 8. 钩子函数配置

| 配置项 | 钩子函数 | 用途 |
|--------|----------|------|
| `configUSE_IDLE_HOOK` | `vApplicationIdleHook()` | 空闲时执行（如进入低功耗） |
| `configUSE_TICK_HOOK` | `vApplicationTickHook()` | 每个 Tick 执行 |
| `configUSE_MALLOC_FAILED_HOOK` | `vApplicationMallocFailedHook()` | 内存分配失败时执行 |

```c
#define configUSE_IDLE_HOOK         0  // 空闲钩子
#define configUSE_TICK_HOOK         0  // Tick钩子
```

---

## 9. 中断优先级配置

### 9.1 关键中断优先级定义⭐⭐⭐⭐

```c
// 最低中断优先级
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15

// FreeRTOS API 可调用的最高中断优先级
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
```

**重要规则：**
- 中断优先级数值越大，优先级越低
- 调用 FreeRTOS API 的中断优先级必须在 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 和 `configLIBRARY_LOWEST_INTERRUPT_PRIORITY` 之间
- **不要**在优先级高于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 的中断中调用 FreeRTOS API

---

## 10. API 函数裁剪配置

通过 `INCLUDE_` 宏控制是否编译特定 API 函数，减少代码体积：

```c
#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_uxTaskGetStackHighWaterMark  1  // 获取栈高水位（重要调试）
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_eTaskGetState                1
```

**建议：** 调试阶段全部开启，发布时可裁剪不需要的功能。

---

## 11. 配置验证与调试技巧

### 11.1 查看任务运行状态（必用调试代码）

```c
char CPU_RunInfo[600];

// 清零缓冲区
memset(CPU_RunInfo, 0, 600);
// 获取任务状态信息
vTaskList((char *)&CPU_RunInfo);

printf("\r\n---------------------------------------------\r\n");
printf("任务名\t任务状态\t优先级\t剩余栈\t任务序号\r\n");
printf("%s", CPU_RunInfo);
printf("---------------------------------------------\r\n");

// 获取任务运行时间统计
memset(CPU_RunInfo, 0, 600);
vTaskGetRunTimeStats((char *)&CPU_RunInfo);
printf("任务名\t运行计数\t使用率\r\n");
printf("%s", CPU_RunInfo);
printf("---------------------------------------------\r\n");
```

### 11.2 查看剩余堆空间

```c
size_t freeHeap = xPortGetFreeHeapSize();
printf("剩余堆空间: %d 字节\r\n", freeHeap);
```

### 11.3 查看任务栈高水位

```c
UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
printf("任务栈剩余最小值: %d 字\r\n", highWaterMark);
```

### 11.4 常见问题排查

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 任务创建失败 | 堆空间不足 | 增大 `configTOTAL_HEAP_SIZE` |
| 栈溢出 | 任务栈太小 | 增大任务栈大小，启用栈溢出检测 |
| 系统卡死 | 中断优先级配置错误 | 检查中断优先级设置 |
| 优先级翻转 | 未使用互斥量 | 使用 Mutex 替代二值信号量 |

---

## 附录：当前项目配置现状

```c
// 当前配置（来自 FreeRTOSConfig.h）
#define configUSE_PREEMPTION                     1
#define configTICK_RATE_HZ                       1000
#define configMAX_PRIORITIES                     56
#define configMINIMAL_STACK_SIZE                 128
#define configTOTAL_HEAP_SIZE                    15360  // 15KB
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                2
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             256
```

---

## 参考资料

- [FreeRTOS 官方文档](https://www.freertos.org/a00110.html)
- [STM32CubeMX FreeRTOS 配置详解](https://developer.aliyun.com/article/992044)
- [FreeRTOS 内核参数配置](https://blog.csdn.net/m0_49968063/article/details/130400565)
- [FreeRTOSConfig.h 配置文件详解](https://blog.csdn.net/Ningjianwen/article/details/90648064)
