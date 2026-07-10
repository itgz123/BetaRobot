---
outline: deep
---

# APP 层开发指南

> 本文档描述 APP 层特有的开发规范，跨层设计原则请参考《项目架构设计方案》。

---

## 一、APP 层的定位与职责

### 1.1 APP 层是什么

APP（Application，应用层）是整个系统的最高层，负责 **业务逻辑** 和 **任务调度**。

**重要区分：**

- BSP 层：封装 MCU 内部外设（CAN、USART、SPI、GPIO 等）
- DRV 层：封装 PCB 上集成的外部模块（电机、IMU、遥控器等）
- APP 层：业务逻辑，调用 DRV 接口，管理 FreeRTOS 对象

### 1.2 APP 层的职责

| 职责 | 说明 |
|------|------|
| 业务逻辑 | 实现机器人的具体功能（底盘控制、云台控制、射击等） |
| 任务调度 | 创建和管理 FreeRTOS 任务 |
| RTOS 对象管理 | 创建和管理队列、互斥量、信号量等 |
| 数据解析 | 在任务中调用 DRV 解析函数 |
| 消息传递 | 通过队列实现任务间通信 |

### 1.3 APP 层不应做什么

- 不直接操作硬件寄存器（调用 DRV/BSP 接口）
- 不在 DRV 层创建 RTOS 对象
- 不在中断中进行复杂的数据解析
- 不在任务中阻塞等待多个队列（可能导致优先级反转）

### 1.4 APP 层的职责边界

**重要原则：APP 层是唯一使用 FreeRTOS 的层**

| 功能 | 职责归属 | 原因 |
|------|----------|------|
| 任务创建与管理 | **APP 层** | 任务是 RTOS 对象 |
| 队列创建与管理 | **APP 层** | 队列是 RTOS 对象 |
| 互斥量创建与管理 | **APP 层** | 互斥量是 RTOS 对象 |
| 信号量创建与管理 | **APP 层** | 信号量是 RTOS 对象 |
| 业务逻辑实现 | **APP 层** | 应用相关 |
| DRV 回调实现 | **APP 层** | 数据拷贝到队列 |

---

## 二、文件组织结构

### 2.1 标准目录结构

```
app/
├── app_cfg.h           # APP 层配置（任务频率、常量定义等）
├── Inc/
│   ├── app_robot.h     # 机器人总控（任务创建、队列声明）
│   ├── app_chassis.h   # 底盘控制模块
│   ├── app_motor.h     # 电机控制模块
│   ├── app_cmd.h       # 命令处理模块
│   └── app_log.h       # 日志模块
└── Src/
    ├── app_robot.c     # 机器人总控实现
    ├── app_chassis.c   # 底盘控制实现
    ├── app_motor.c     # 电机控制实现
    ├── app_cmd.c       # 命令处理实现
    └── app_log.c       # 日志实现
```

### 2.2 app_cfg.h 模板

```c
#ifndef __APP_CFG_H
#define __APP_CFG_H

// 开发板选择
#define TELESKY_VET6 0
#define DM_MC02 1
#define DJI_A 2
#define DJI_C 3
#define DEVELOPMENT_BOARD TELESKY_VET6

// 任务频率配置
#define MOTOR_CONTROL_FREQ_MS 1      // 1kHz 电机控制
#define CHASSIS_CONTROL_FREQ_MS 5    // 200Hz 底盘控制
#define LOG_FREQ_MS 500             // 2Hz 日志输出

#endif // !__APP_CFG_H
```

---

## 三、任务设计规范

### 3.1 任务创建流程

任务在 `robot.c` 的 `RTOSTaskInit()` 函数中创建。

### 3.2 任务函数模板

```c
void StartXXX_TASK(void *argument)
{
    static float start;
    static float dt;

    // 初始化
    XXX_Init();
    LOGINFO("[freeRTOS] XXX Task Start");

    for (;;)
    {
        start = DWT_GetTimeline_ms();
        XXX_Task();
        dt = DWT_GetTimeline_ms() - start;
        if (dt > XXX_CONTROL_FREQ_MS)
        {
            LOGERROR("[freeRTOS] XXX Task is being DELAY! dt = [%f]", dt);
        }
        osDelay(XXX_CONTROL_FREQ_MS);
    }
}
```

### 3.3 任务优先级设计

| 任务 | 优先级 | 频率 | 说明 |
|------|--------|------|------|
| Motor | 30 | 1kHz | 最高，直接控制电机 |
| Chassis | 29 | 200Hz | 高，运动控制 |
| RemoteControl | 28 | ~70Hz | 中，遥控处理 |
| Log | 25 | 2Hz | 低，日志输出 |

---

## 四、队列设计规范

### 4.1 队列创建位置

| 队列类型 | 创建位置 | 说明 |
|----------|----------|------|
| 线程间通信队列 | `robot.c` | 多任务共享，全局可见 |
| 中断到任务的队列 | 任务自己的文件 | 私有，static 修饰 |

### 4.2 队列长度设置

**重要原则：队列长度统一设置为 1**

原因：底盘等高频任务需要最新数据，旧数据无意义。

---

## 五、回调函数设计

### 5.1 回调链流程

```
[中断] HAL 回调
   │
   ↓
[BSP] 拷贝数据，调用 DRV 回调
   │
   ↓
[DRV] 继续调用 APP 回调（不解析）
   │
   ↓
[APP] 拷贝原始数据到队列（不解析）
   │
   ↓
[任务] 从队列获取，调用 DRV 解析函数
```

### 5.2 APP 层回调实现

**重要原则：中断中只拷贝数据到队列，不解析**

```c
static void SBUSDataCallback(void *raw_data)
{
    USARTInstance *usart = (USARTInstance *)raw_data;
    SBUS_RawFrame_t frame;
    frame.len = USARTGetRxLen(usart);
    memcpy(frame.data, USARTGetRxBuffer(usart), frame.len);

    // 拷贝到队列（不解析！）
    osMessageQueuePut(s_sbus_queue, &frame, 0, 0);
}
```

---

## 六、初始化流程规范

```c
void RobotInit(void)
{
    // 1. 关闭中断
    __disable_irq();
    // 2. 初始化底层服务
    DWT_Init();
    BSPLogInit();
    // 3. 开启中断
    __enable_irq();
}
```

---

## 七、数据流示例

```
中断：HAL → BSP → DRV → APP（拷贝原始帧入队列，不解析）
任务：APP（出队列）→ DRV 解析 → 业务逻辑 → DRV → BSP → HAL
```

---

## 八、APP 层检查清单

### 文件组织检查
- [ ] 是否创建了对应的 .h 和 .c 文件？
- [ ] 是否在 robot.c 中创建了任务？

### 任务设计检查
- [ ] 任务优先级是否合理？
- [ ] 任务栈大小是否足够？

### 队列设计检查
- [ ] 队列是否创建在正确的位置？
- [ ] 队列长度是否设置为 1？

### 回调设计检查
- [ ] 回调中是否只拷贝数据到队列？
- [ ] 回调中是否避免了数据解析？

---

_本文档适用于 test_my_frame 项目的 APP 层开发。_
