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

| 职责          | 说明                                               |
| ------------- | -------------------------------------------------- |
| 业务逻辑      | 实现机器人的具体功能（底盘控制、云台控制、射击等） |
| 任务调度      | 创建和管理 FreeRTOS 任务                           |
| RTOS 对象管理 | 创建和管理队列、互斥量、信号量等                   |
| 数据解析      | 在任务中调用 DRV 解析函数                          |
| 消息传递      | 通过队列实现任务间通信                             |

### 1.3 APP 层不应做什么

- 不直接操作硬件寄存器（调用 DRV/BSP 接口）
- 不在 DRV 层创建 RTOS 对象
- 不在中断中进行复杂的数据解析
- 不在任务中阻塞等待多个队列（可能导致优先级反转）

### 1.4 APP 层的职责边界

**重要原则：APP 层是唯一使用 FreeRTOS 的层**

| 功能             | 职责归属   | 原因               |
| ---------------- | ---------- | ------------------ |
| 任务创建与管理   | **APP 层** | 任务是 RTOS 对象   |
| 队列创建与管理   | **APP 层** | 队列是 RTOS 对象   |
| 互斥量创建与管理 | **APP 层** | 互斥量是 RTOS 对象 |
| 信号量创建与管理 | **APP 层** | 信号量是 RTOS 对象 |
| 业务逻辑实现     | **APP 层** | 应用相关           |
| DRV 回调实现     | **APP 层** | 数据拷贝到队列     |

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

### 2.2 文件职责划分

| 文件                 | 职责                                 |
| -------------------- | ------------------------------------ |
| `app_cfg.h`          | 定义任务频率、开发板选择等配置       |
| `app_robot.h/c`      | 总控：任务创建、队列创建、初始化入口 |
| `app_chassis.h/c`    | 底盘控制：运动学计算、电机命令生成   |
| `app_motor.h/c`      | 电机控制：PWM 设置、方向控制         |
| `app_cmd.h/c`        | 命令处理：遥控数据解析、命令生成     |

### 2.3 app_cfg.h 模板

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

// 机器人参数
#define WHEEL_RADIUS 0.05f          // 轮子半径 (m)
#define WHEEL_BASE 0.4f             // 轮距 (m)
#define MAX_SPEED 2.0f              // 最大速度 (m/s)

#endif // !__APP_CFG_H
```

---

## 三、任务设计规范

### 3.1 任务创建流程

任务在 `robot.c` 的 `RTOSTaskInit()` 函数中创建，该函数在 `Core/Src/freertos.c` 中被调用：

```c
// robot.c

// 任务句柄
osThreadId_t MotorTaskHandle;
osThreadId_t ChassisTaskHandle;

// 任务属性
const osThreadAttr_t motor_Task_attributes = {
    .name = "motor_Task",
    .stack_size = 128 * 4,      // 栈大小（字节）
    .priority = (osPriority_t)30, // 优先级
};

const osThreadAttr_t chassis_Task_attributes = {
    .name = "chassis_Task",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)29,
};

// 任务函数声明
void StartMOTORTASK(void *argument);
void StartCHASSISTASK(void *argument);

// 任务初始化
void RTOSTaskInit(void)
{
    // 创建任务
    MotorTaskHandle = osThreadNew(StartMOTORTASK, NULL, &motor_Task_attributes);
    ChassisTaskHandle = osThreadNew(StartCHASSISTASK, NULL, &chassis_Task_attributes);

    // 创建队列
    chassis_cmd_queue = osMessageQueueNew(1, sizeof(Chassis_Cmd_t), NULL);
}
```

### 3.2 任务函数模板

每个任务遵循统一的结构：

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
        // 记录开始时间
        start = DWT_GetTimeline_ms();

        // 执行任务逻辑
        XXX_Task();

        // 检查任务是否超时
        dt = DWT_GetTimeline_ms() - start;
        if (dt > XXX_CONTROL_FREQ_MS)
        {
            LOGERROR("[freeRTOS] XXX Task is being DELAY! dt = [%f]", dt);
        }

        // 延时
        osDelay(XXX_CONTROL_FREQ_MS);
    }
}
```

### 3.3 任务优先级设计

| 任务          | 优先级 | 频率  | 说明               |
| ------------- | ------ | ----- | ------------------ |
| Motor         | 30     | 1kHz  | 最高，直接控制电机 |
| Chassis       | 29     | 200Hz | 高，运动控制       |
| RemoteControl | 28     | ~70Hz | 中，遥控处理       |
| Log           | 25     | 2Hz   | 低，日志输出       |

**原则：实时性要求越高的任务，优先级越高**

### 3.4 任务栈大小设计

| 任务类型     | 栈大小               | 说明         |
| ------------ | -------------------- | ------------ |
| 简单控制任务 | 128 \* 4 = 512 字节  | 少量局部变量 |
| 复杂计算任务 | 256 \* 4 = 1024 字节 | 有浮点运算   |
| 通信任务     | 512 \* 4 = 2048 字节 | 有缓冲区     |

---

## 四、队列设计规范

### 4.1 队列创建位置

**重要原则：不同类型的队列在不同位置创建**

| 队列类型         | 创建位置       | 说明                 |
| ---------------- | -------------- | -------------------- |
| 线程间通信队列   | `robot.c`      | 多任务共享，全局可见 |
| 中断到任务的队列 | 任务自己的文件 | 私有，static 修饰    |

### 4.2 队列长度设置

**重要原则：队列长度统一设置为 1**

原因：

- 底盘等高频任务需要最新数据
- 旧数据无意义
- 使用 `osMessageQueueGet()` 阻塞等待即可

```c
// 线程间通信队列（在 robot.c 中创建）
osMessageQueueId_t chassis_cmd_queue;
chassis_cmd_queue = osMessageQueueNew(1, sizeof(Chassis_Cmd_t), NULL);

// 中断到任务的队列（在任务自己的文件中创建）
static osMessageQueueId_t s_sbus_queue = NULL;
s_sbus_queue = osMessageQueueNew(1, sizeof(SBUS_RawFrame_t), NULL);
```

### 4.3 数据类型定义

队列传递的数据类型在 `drv_types.h` 中定义：

```c
// drv_types.h

// 底盘控制命令
typedef struct
{
    float vx;       // x 方向速度 (m/s)
    float vy;       // y 方向速度 (m/s)
    float wz;       // 旋转角速度 (rad/s)
    uint8_t mode;   // 控制模式
} Chassis_Cmd_t;

// 电机命令
typedef struct
{
    float pwm_duty[4];     // PWM 占空比
    uint8_t direction[4];  // 方向
} Motor_Cmd_t;
```

### 4.4 队列使用示例

**发送端：**

```c
// 发送命令（非阻塞）
osMessageQueuePut(chassis_cmd_queue, &chassis_cmd, 0, 0);
```

**接收端：**

```c
// 接收命令（阻塞等待）
if (osMessageQueueGet(s_sbus_queue, &frame, NULL, osWaitForever) == osOK)
{
    // 处理数据
}

// 接收命令（非阻塞，立即返回）
if (osMessageQueueGet(chassis_cmd_queue, &cmd, NULL, 0) == osOK)
{
    // 处理数据
}
```

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
// remote_control.c

// 私有队列
static osMessageQueueId_t s_sbus_queue = NULL;

// APP 层回调：拷贝原始数据到队列（不解析！）
static void SBUSDataCallback(void *raw_data)
{
    USARTInstance *usart = (USARTInstance *)raw_data;

    // 构造原始帧
    SBUS_RawFrame_t frame;
    frame.len = USARTGetRxLen(usart);
    frame.timestamp = 0;
    memcpy(frame.data, USARTGetRxBuffer(usart), frame.len);

    // 拷贝到队列（不解析！）
    osMessageQueuePut(s_sbus_queue, &frame, 0, 0);
}
```

### 5.3 任务中解析数据

```c
// remote_control.c

void REMOTECONTROL_Task(void)
{
    SBUS_RawFrame_t frame;
    SBUS_Data_t sbus_data;

    for (;;)
    {
        // 从队列获取原始帧
        if (osMessageQueueGet(s_sbus_queue, &frame, NULL, osWaitForever) == osOK)
        {
            // 在任务中调用 DRV 解析函数
            sbus_data = SBUSDecodeFrame(frame.data, frame.len);

            // 执行业务逻辑
            // ...
        }
    }
}
```

### 5.4 回调注册

在模块初始化函数中注册回调：

```c
void REMOTECONTROL_Init(void)
{
    // 创建队列
    s_sbus_queue = osMessageQueueNew(1, sizeof(SBUS_RawFrame_t), NULL);

    // 注册 DRV 实例，传入 APP 回调
    SBUS_Init_Config_s sbus_config = {
        .bsp_instance = &huart2,
        .app_callback = SBUSDataCallback,  // APP 回调
        .app_id = NULL,
    };
    s_sbus_inst = SBUSRegister(&sbus_config);
}
```

---

## 六、初始化流程规范

### 6.1 初始化顺序

```c
// robot.c

void RobotInit(void)
{
    // 1. 关闭中断
    __disable_irq();

    // 2. 初始化底层服务
    DWT_Init();           // 高精度定时器
    BSPLogInit();         // 日志系统

    // 3. 开启中断
    __enable_irq();

    LOGINFO("[robot] RobotInit done");
}

void RTOSTaskInit(void)  // 在 freertos.c 中调用
{
    // 1. 创建任务
    MotorTaskHandle = osThreadNew(StartMOTORTASK, NULL, &motor_Task_attributes);
    // ...

    // 2. 创建线程间通信队列
    chassis_cmd_queue = osMessageQueueNew(1, sizeof(Chassis_Cmd_t), NULL);
    // ...
}
```

### 6.2 模块初始化

每个模块有自己的初始化函数，在任务开始时调用：

```c
void StartCHASSISTASK(void *argument)
{
    // 任务开始时初始化
    CHASSIS_Init();

    for (;;)
    {
        CHASSIS_Task();
        osDelay(CHASSIS_CONTROL_FREQ_MS);
    }
}
```

### 6.3 初始化注意事项

1. **初始化期间关闭中断**：防止初始化过程中发生中断
2. **不使用 HAL_Delay()**：使用 `DWT_Delay()` 替代
3. **模块初始化顺序**：先底层后上层

---

## 七、数据流示例

### 7.1 遥控器 → 底盘 完整数据流

```
┌─────────────────────────────────────────────────────────────┐
│                        中断上下文                            │
│                                                             │
│  UART RX 中断触发                                           │
│       │                                                     │
│       ↓                                                     │
│  HAL_UART_RxCpltCallback()                                  │
│       │                                                     │
│       ↓                                                     │
│  [BSP] bsp_usart.c: 调用 DRV 回调                           │
│       │                                                     │
│       ↓                                                     │
│  [DRV] drv_sbus.c: SBUSUARTRxCallback()                     │
│       │    └─ 继续调用 APP 回调                             │
│       ↓                                                     │
│  [APP] remote_control.c: SBUSDataCallback()                 │
│       │    └─ 拷贝原始帧到 s_sbus_queue                     │
│       │                                                     │
│       ↓                                                     │
│  中断结束                                                    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ↓ 队列传递
┌─────────────────────────────────────────────────────────────┐
│                        任务上下文                            │
│                                                             │
│  REMOTECONTROL_Task()                                       │
│       │                                                     │
│       ↓ 从队列获取原始帧                                    │
│  osMessageQueueGet(s_sbus_queue, &frame, ...)               │
│       │                                                     │
│       ↓ 调用 DRV 解析函数                                   │
│  SBUSDecodeFrame(frame.data, frame.len)                     │
│       │                                                     │
│       ↓ 业务逻辑处理                                        │
│  生成 Chassis_Cmd_t                                         │
│       │                                                     │
│       ↓ 发送到底盘队列                                      │
│  osMessageQueuePut(chassis_cmd_queue, &cmd, ...)            │
└─────────────────────────────────────────────────────────────┘
                          │
                          ↓ 队列传递
┌─────────────────────────────────────────────────────────────┐
│                     ChassisTask()                            │
│                                                             │
│  osMessageQueueGet(chassis_cmd_queue, &cmd, ...)            │
│       │                                                     │
│       ↓ 运动学计算                                          │
│  MecanumInverseKinematics(vx, vy, wz, wheel_speed)          │
│       │                                                     │
│       ↓ 生成电机命令                                        │
│  Motor_Cmd_t                                                │
│       │                                                     │
│       ↓ 发送到电机队列                                      │
│  osMessageQueuePut(motor_cmd_queue, &motor_cmd, ...)        │
└─────────────────────────────────────────────────────────────┘
```

---

## 八、模块开发示例

### 8.1 新增模块步骤

1. **创建头文件** `app/Inc/xxx.h`
2. **创建源文件** `app/Src/xxx.c`
3. **在 `robot.c` 中创建任务**
4. **在 `robot.h` 中定义数据类型（如需要）**

### 8.2 完整模块示例：底盘控制

#### app_chassis.h

```c
#ifndef __APP_CHASSIS_H
#define __APP_CHASSIS_H

void CHASSIS_Init(void);
void CHASSIS_Task(void);

#endif /* __APP_CHASSIS_H */
```

#### app_chassis.c

```c
#include "app_chassis.h"
#include "app_robot.h"
#include "bsp_log.h"
#include "cmsis_os2.h"
#include <math.h>

// 底盘参数
#define WHEEL_RADIUS 0.05f
#define WHEEL_BASE 0.4f
#define MAX_WHEEL_SPEED 5.0f

// 电机索引
#define MOTOR_LF 0
#define MOTOR_RF 1
#define MOTOR_LB 2
#define MOTOR_RB 3

// 私有函数
static void MecanumInverseKinematics(float vx, float vy, float wz, float wheel_speed[4]);
static float SpeedToPWM(float wheel_speed);

void CHASSIS_Init(void)
{
    // 初始化底盘相关资源
}

void CHASSIS_Task(void)
{
    Chassis_Cmd_t cmd;
    Motor_Cmd_t motor_cmd;
    float wheel_speed[4];

    // 从队列获取命令
    if (osMessageQueueGet(chassis_cmd_queue, &cmd, NULL, 0) == osOK)
    {
        // 运动学计算
        MecanumInverseKinematics(cmd.vx, cmd.vy, cmd.wz, wheel_speed);

        // 生成电机命令
        for (int i = 0; i < 4; i++)
        {
            motor_cmd.pwm_duty[i] = fabsf(SpeedToPWM(wheel_speed[i]));
            motor_cmd.direction[i] = (wheel_speed[i] >= 0) ? 1 : 0;
        }

        // 发送电机命令
        osMessageQueuePut(motor_cmd_queue, &motor_cmd, 0, 0);
    }
}

static void MecanumInverseKinematics(float vx, float vy, float wz, float wheel_speed[4])
{
    float factor = WHEEL_BASE / 2.0f;

    wheel_speed[MOTOR_LF] = vx - vy - wz * factor;
    wheel_speed[MOTOR_RF] = vx + vy + wz * factor;
    wheel_speed[MOTOR_LB] = vx + vy - wz * factor;
    wheel_speed[MOTOR_RB] = vx - vy + wz * factor;
}

static float SpeedToPWM(float wheel_speed)
{
    float pwm = wheel_speed / MAX_WHEEL_SPEED;
    if (pwm > 1.0f) pwm = 1.0f;
    if (pwm < -1.0f) pwm = -1.0f;
    return pwm;
}
```

---

## 九、常见问题与解决方案

### Q1: 任务间如何共享数据？

**场景：** 多个任务需要访问同一个电机状态。

**解决方案：** 使用互斥量保护，或通过队列传递。

```c
// 方案1：互斥量保护
static osMutexId_t s_motor_mutex = NULL;

void MOTOR_Init(void)
{
    s_motor_mutex = osMutexNew(NULL);
}

float MotorGetSpeed(int index)
{
    float speed;
    osMutexAcquire(s_motor_mutex, osWaitForever);
    speed = s_motor_speed[index];
    osMutexRelease(s_motor_mutex);
    return speed;
}

// 方案2：队列传递（推荐）
osMessageQueuePut(motor_state_queue, &state, 0, 0);
```

### Q2: 如何处理任务超时？

**场景：** 任务执行时间超过预期。

**解决方案：** 记录任务执行时间，超时输出警告。

```c
void StartCHASSISTASK(void *argument)
{
    static float start, dt;

    for (;;)
    {
        start = DWT_GetTimeline_ms();
        CHASSIS_Task();
        dt = DWT_GetTimeline_ms() - start;

        if (dt > CHASSIS_CONTROL_FREQ_MS)
        {
            LOGERROR("[freeRTOS] CHASSIS Task is being DELAY! dt = [%f]", dt);
        }

        osDelay(CHASSIS_CONTROL_FREQ_MS);
    }
}
```

### Q3: 如何避免优先级反转？

**场景：** 低优先级任务持有互斥量，高优先级任务等待。

**解决方案：**

1. 使用 FreeRTOS 的互斥量（内置优先级继承）
2. 避免在任务中阻塞等待多个队列
3. 简化任务间的依赖关系

### Q4: 中断中可以使用哪些 RTOS API？

**场景：** 在中断回调中调用 RTOS 函数。

**解决方案：** 只有特定的 `FromISR` 后缀函数可以在中断中调用：

```c
// 队列：可以使用
osMessageQueuePut(queue, &msg, 0, 0);  // FreeRTOS CMSIS-RTOS2 封装已处理

// 信号量：可以使用
osSemaphoreRelease(sem);

// 任务通知：可以使用
osThreadFlagsSet(thread_id, flags);

// 互斥量：不能在中断中使用
// osMutexAcquire()  // 错误！
```

---

## 十、APP 层检查清单

编写新的 APP 模块时，请检查以下项目：

### 文件组织检查

- [ ] 是否创建了对应的 .h 和 .c 文件？
- [ ] 是否在 robot.c 中创建了任务？
- [ ] 是否在 robot.h 中定义了数据类型？

### 任务设计检查

- [ ] 任务优先级是否合理？
- [ ] 任务栈大小是否足够？
- [ ] 任务执行时间是否超时？

### 队列设计检查

- [ ] 队列是否创建在正确的位置？
- [ ] 队列长度是否设置为 1？
- [ ] 数据类型是否定义在 robot.h 中？

### 回调设计检查

- [ ] 回调中是否只拷贝数据到队列？
- [ ] 回调中是否避免了数据解析？
- [ ] 是否正确注册了回调？

### 初始化检查

- [ ] 是否在任务开始时调用初始化函数？
- [ ] 是否正确创建了 RTOS 对象？

---

## 十一、总结

### APP 层核心原则

1. **业务逻辑集中**：APP 层是业务逻辑的唯一实现层
2. **RTOS 对象管理**：APP 层是唯一使用 FreeRTOS 的层
3. **中断只拷贝**：回调中只拷贝数据，不解析
4. **任务中解析**：在任务上下文调用 DRV 解析函数
5. **队列传递数据**：任务间通过队列通信

### 数据流总结

```
中断：HAL → BSP → DRV → APP（拷贝原始帧入队列，不解析）
任务：APP（出队列）→ DRV 解析 → 业务逻辑 → DRV → BSP → HAL
```

### 分层职责对比

| 层级 | 职责               | 关键点                        |
| ---- | ------------------ | ----------------------------- |
| APP  | 业务逻辑、任务调度 | 调用 DRV 接口，管理 RTOS 对象 |
| DRV  | 模块驱动、协议解析 | 封装外部模块，提供解析函数    |
| BSP  | 外设封装、中断分发 | 管理实例，回调分发            |
| HAL  | 硬件操作           | CubeMX 生成                   |

---

_本文档适用于 test_my_frame 项目的 APP 层开发。跨层设计原则请参考《项目架构设计方案》，命名规范请参考《代码规范》。_
