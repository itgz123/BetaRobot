# DRV 层开发指南

> 本文档描述 DRV 层特有的开发规范，跨层设计原则请参考《项目架构设计方案》。

---

## 一、DRV 层的定位与职责

### 1.1 DRV 层是什么

DRV（Driver，驱动层）是对 **PCB 上集成的外部模块** 的封装，提供模块级别的抽象接口。

**重要区分：**

- BSP 层：封装 MCU 内部外设（CAN、USART、SPI、I2C、GPIO 等）
- DRV 层：封装 PCB 上集成的外部模块（电机、IMU、遥控器、裁判系统等）
- APP 层：业务逻辑，调用 DRV 接口

### 1.2 DRV 层的职责

| 职责     | 说明                                     |
| -------- | ---------------------------------------- |
| 模块抽象 | 封装外部模块，提供统一的操作接口         |
| 协议解析 | 解析模块通信协议（CAN 协议、串口协议等） |
| 数据处理 | 提供数据转换、滤波、校验等功能           |
| 回调传递 | 中断中只传递原始数据，任务中解析         |

### 1.3 DRV 层不应做什么

- 不直接操作硬件寄存器（调用 BSP 接口）
- 不包含业务逻辑（由 APP 层负责）
- 不在中断中进行复杂的数据解析
- 不定义硬件配置参数（由 BSP/HAL 负责）
- **默认不使用 FreeRTOS**（队列、互斥量、信号量等由 APP 层负责）
- **特殊情况**：守护进程等需要独立任务的基础服务模块，可在 DRV 层创建任务

### 1.4 DRV 层的职责边界

**重要原则：DRV 层是纯粹的驱动层，默认不涉及操作系统特性**

| 功能           | 职责归属   | 原因               |
| -------------- | ---------- | ------------------ |
| 实例注册       | **DRV 层** | 驱动管理           |
| 回调注册与传递 | **DRV 层** | 中断到 APP 的桥梁  |
| 协议解析函数   | **DRV 层** | 提供给 APP 调用    |
| 数据拷贝到队列 | **APP 层** | 队列是 RTOS 对象   |
| 互斥量保护数据 | **APP 层** | 互斥量是 RTOS 对象 |
| 任务调度       | **APP 层** | 任务是 RTOS 对象   |

### 1.5 DRV 层提供的内容

1. **注册函数**：创建实例，注册 BSP 回调
2. **回调函数**：接收 BSP 数据，继续传递到 APP（不解析）
3. **解析函数**：纯函数，供 APP 在任务中调用
4. **辅助函数**：数据转换、校验等

---

## 二、核心设计原则

### 2.1 实例注册机制

**设计原则：** 所有模块都通过注册函数创建实例。

**优点：**

- 支持多实例管理
- 便于资源追踪和调试
- 统一的初始化入口
- 便于移植到不同平台

**代码模板：**

```c
// 1. 定义最大实例数量（在 .h 文件中）
#define MOTOR_MAX_CNT 8

// 2. 定义实例结构体（在 .h 文件中公开定义）
typedef struct MotorInstance
{
    uint32_t device_id;       // 设备 ID
    uint8_t device_type;      // 设备类型
    float speed;              // 速度
    float angle;              // 角度
    void (*app_callback)(void *raw_data);  // APP 层回调
    void *app_id;             // APP 层实例标识
} MotorInstance;

// 3. 定义初始化配置结构体（在 .h 文件中）
typedef struct
{
    void *bsp_handle;         // BSP 实例句柄
    uint32_t device_id;       // 设备 ID
    uint8_t device_type;      // 设备类型
    void (*app_callback)(void *raw_data);  // APP 层回调
    void *app_id;             // APP 层实例标识
} Motor_Init_Config_s;

// 4. 注册函数声明（在 .h 文件中）
MotorInstance *MotorRegister(Motor_Init_Config_s *config);
```

### 2.2 回调链设计

**设计原则：** 中断数据通过回调链逐层传递，每层只做自己的职责。

```
[MPU] interrupt
   │
   ↓ HAL 回调函数
   │
[BSP] hal_callback
   │    └─ 拷贝原始数据，查找实例，调用 DRV 回调
   ↓
[DRV] bsp_callback
   │    └─ 继续回调到 APP 层（不解析！）
   ↓
[APP] drv_callback
   │    └─ 拷贝原始数据到队列（不解析！）
   ↓
(interrupt finish) 中断结束
```

**重要原则：中断中不解析数据，只拷贝和传递**

### 2.3 接口设计

**头文件示例：**

```c
// drv_motor.h
#ifndef DRV_MOTOR_H
#define DRV_MOTOR_H

#include <stdint.h>

// 实例结构体（公开定义）
typedef struct MotorInstance
{
    uint32_t tx_id;
    uint32_t rx_id;
    uint8_t motor_type;
    float angle;
    float speed;
    float current;
    void (*app_callback)(void *raw_data);
    void *app_id;
} MotorInstance;

// 配置结构体（公开，用于初始化）
typedef struct
{
    void *bsp_instance;
    uint32_t rx_id;
    uint8_t motor_type;
    void (*app_callback)(void *raw_data);
    void *app_id;
} Motor_Init_Config_s;

// 公开接口
MotorInstance *MotorRegister(Motor_Init_Config_s *config);
void MotorEnable(MotorInstance *inst);
void MotorSetRef(MotorInstance *inst, float ref);

#endif // DRV_MOTOR_H
```

**源文件示例：**

```c
// drv_motor.c
#include "drv_motor.h"
#include "bsp_can.h"
#include "bsp_log.h"
#include <stdlib.h>
#include <string.h>

// ========== 私有变量（文件作用域）==========
static MotorInstance *s_motors[MOTOR_MAX_CNT] = {NULL};
static uint8_t s_motor_cnt = 0;

// ========== 私有函数（静态）==========
static void MotorBSPRxCallback(CANInstance *can_inst);
```

---

## 三、数据结构设计

### 3.1 常用数据结构

#### 原始帧类型（用于中断到任务的传递）

```c
// drv_types.h - 通用原始帧类型

typedef struct
{
    uint32_t device_id;    // 设备 ID
    uint8_t dlc;           // 数据长度
    uint8_t data[8];       // 原始数据
    uint32_t timestamp;    // 时间戳
} CanRawFrame_t;

typedef struct
{
    uint8_t data[256];     // 原始数据
    uint16_t len;          // 数据长度
    uint32_t timestamp;    // 时间戳
} UartRawFrame_t;
```

#### 模块配置结构体

```c
typedef struct
{
    // BSP 实例
    void *bsp_instance;

    // 设备参数
    uint32_t device_id;
    uint8_t device_type;

    // APP 回调
    void (*app_callback)(void *raw_data);
    void *app_id;
} Motor_Init_Config_s;
```

#### 解析后的数据结构

```c
typedef struct
{
    float angle;            // 角度（度）
    float speed;            // 速度（rpm）
    float current;          // 电流（A）
    uint8_t temperature;    // 温度（℃）
} Motor_Data_t;
```

---

## 四、文件组织结构

### 4.1 标准目录结构

```
drv/
├── drv_cfg.h           # DRV 层配置（实例数量、常量定义等）
├── Inc/
│   ├── drv_types.h     # 公共类型定义（原始帧类型等）
│   ├── drv_motor.h     # 电机驱动接口
│   ├── drv_imu.h       # IMU 驱动接口
│   └── drv_rc.h        # 遥控器驱动接口
└── Src/
    ├── drv_motor.c     # 电机驱动实现
    ├── drv_imu.c       # IMU 驱动实现
    └── drv_rc.c        # 遥控器驱动实现
```

### 4.2 drv_cfg.h 示例

```c
#ifndef DRV_CFG_H
#define DRV_CFG_H

// 电机配置
#define MOTOR_MAX_CNT           8
#define MOTOR_MAX_CURRENT       16000.0f
#define MOTOR_MAX_SPEED         10000.0f

// IMU 配置
#define IMU_SAMPLE_RATE         1000
#define IMU_GYRO_RANGE          2000
#define IMU_ACCEL_RANGE         16

// 遥控器配置
#define RC_DATA_SIZE            18
#define RC_FRAME_SIZE           25

#endif // DRV_CFG_H
```

---

## 五、回调机制实现

### 5.1 回调注册流程

```
APP 调用 DRV 注册函数
       │
       ↓ 保存 APP 回调
DRV 调用 BSP 注册函数，传入 DRV 回调
       │
       ↓ 保存 DRV 回调
BSP 保存实例和回调指针
       │
       ↓
中断发生时，BSP 查找实例并调用 DRV 回调
       │
       ↓
DRV 回调继续调用 APP 回调
```

### 5.2 DRV 层回调（不解析）

```c
// drv_motor.c

// DRV 层回调：只传递，不解析
static void MotorCANRxCallback(CANInstance *can_inst)
{
    MotorInstance *motor = (MotorInstance *)can_inst->id;

    // 继续回调到 APP 层
    if (motor->app_callback != NULL)
    {
        motor->app_callback(can_inst);  // 传递 BSP 实例指针
    }
}
```

### 5.3 解析函数（在任务中调用）

```c
// drv_motor.c

Motor_Data_t MotorDecodeFrame(const uint8_t *data, uint8_t len)
{
    Motor_Data_t result = {0};

    if (data == NULL || len < 7)
    {
        LOGWARNING("[drv_motor] Invalid frame data");
        return result;
    }

    // DJI 电机协议解析
    uint16_t angle_raw = (data[0] << 8) | data[1];
    int16_t speed_raw = (data[2] << 8) | data[3];
    int16_t current_raw = (data[4] << 8) | data[5];

    result.angle = angle_raw * 360.0f / 8192.0f;
    result.speed = speed_raw;
    result.current = current_raw / 16384.0f * 20.0f;
    result.temperature = data[6];

    return result;
}
```

---

## 六、典型模块开发模式

### 6.1 电机驱动模块

#### drv_motor.h

```c
#ifndef DRV_MOTOR_H
#define DRV_MOTOR_H

#include "main.h"
#include "drv_cfg.h"
#include "stdint.h"

/*------------- 宏定义 --------------*/
#define MOTOR_MAX_CNT           8

/*------------- 类型定义 --------------*/

// 电机类型枚举
typedef enum
{
    MOTOR_TYPE_M3508 = 0,
    MOTOR_TYPE_M6020,
    MOTOR_TYPE_M2006,
} Motor_Type_e;

// 实例结构体（公开定义）
typedef struct MotorInstance
{
    uint32_t tx_id;
    uint32_t rx_id;
    Motor_Type_e type;
    float angle;
    float speed;
    float current;
    CANInstance *can_inst;
    void (*app_callback)(void *raw_data);
    void *app_id;
} MotorInstance;

// 初始化配置结构体
typedef struct
{
    void *bsp_instance;     // CAN 实例
    uint32_t tx_id;         // 发送 ID
    uint32_t rx_id;         // 接收 ID
    Motor_Type_e type;      // 电机类型
    void (*app_callback)(void *raw_data);  // APP 回调
    void *app_id;           // APP 实例标识
} Motor_Init_Config_s;

// 解析后的电机数据
typedef struct
{
    float angle;            // 角度（度）
    float speed;            // 速度（rpm）
    float current;          // 电流（A）
    uint8_t temperature;    // 温度（℃）
} Motor_Data_t;

/*------------- 外部接口声明 --------------*/

// 注册与初始化
MotorInstance *MotorRegister(Motor_Init_Config_s *config);

// 控制
void MotorEnable(MotorInstance *inst);
void MotorDisable(MotorInstance *inst);
void MotorSetCurrent(MotorInstance *inst, int16_t current);

// 数据解析（在任务中调用）
Motor_Data_t MotorDecodeFrame(const uint8_t *data, uint8_t len);

#endif // DRV_MOTOR_H
```

#### drv_motor.c

```c
#include "drv_motor.h"
#include "bsp_can.h"
#include "bsp_log.h"
#include <stdlib.h>
#include <string.h>

/*------------- 私有变量 --------------*/
static MotorInstance *s_motors[MOTOR_MAX_CNT] = {NULL};
static uint8_t s_motor_cnt = 0;

/*------------- 私有函数声明 --------------*/
static void MotorCANRxCallback(CANInstance *can_inst);

/*------------- 公开接口实现 --------------*/

MotorInstance *MotorRegister(Motor_Init_Config_s *config)
{
    if (s_motor_cnt >= MOTOR_MAX_CNT)
    {
        LOGERROR("[drv_motor] Instance count exceeded");
        return NULL;
    }

    MotorInstance *inst = (MotorInstance *)malloc(sizeof(MotorInstance));
    memset(inst, 0, sizeof(MotorInstance));

    inst->tx_id = config->tx_id;
    inst->rx_id = config->rx_id;
    inst->type = config->type;
    inst->app_callback = config->app_callback;
    inst->app_id = config->app_id;

    // 注册 BSP 层实例
    CAN_Init_Config_s can_config = {
        .handle = config->bsp_instance,
        .rx_id = config->rx_id,
        .callback = MotorCANRxCallback,
        .id = inst,
    };
    inst->can_inst = CANRegister(&can_config);

    s_motors[s_motor_cnt++] = inst;
    LOGINFO("[drv_motor] Motor registered, rx_id=0x%03X", config->rx_id);
    return inst;
}

Motor_Data_t MotorDecodeFrame(const uint8_t *data, uint8_t len)
{
    Motor_Data_t result = {0};

    if (data == NULL || len < 7)
    {
        LOGWARNING("[drv_motor] Invalid frame data");
        return result;
    }

    // DJI 电机协议解析
    result.angle = (data[0] << 8 | data[1]) * 360.0f / 8192.0f;
    result.speed = (int16_t)(data[2] << 8 | data[3]);
    result.current = (int16_t)(data[4] << 8 | data[5]) / 16384.0f * 20.0f;
    result.temperature = data[6];

    return result;
}

/*------------- 私有函数实现 --------------*/

static void MotorCANRxCallback(CANInstance *can_inst)
{
    MotorInstance *motor = (MotorInstance *)can_inst->id;

    // 继续回调到 APP 层（不解析！）
    if (motor->app_callback != NULL)
    {
        motor->app_callback(can_inst);
    }
}
```

### 6.2 遥控器驱动模块

#### drv_rc.h

```c
#ifndef DRV_RC_H
#define DRV_RC_H

#include "main.h"
#include "drv_cfg.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

// 遥控器类型枚举
typedef enum
{
    RC_TYPE_DJI = 0,
    RC_TYPE_DT7,
} RC_Type_e;

// 实例结构体（公开定义）
typedef struct RCInstance
{
    USARTInstance *usart_inst;
    RC_Type_e type;
    void (*app_callback)(void *raw_data);
    void *app_id;
} RCInstance;

// 初始化配置结构体
typedef struct
{
    void *uart_instance;    // UART 实例
    RC_Type_e type;         // 遥控器类型
    void (*app_callback)(void *raw_data);
    void *app_id;
} RC_Init_Config_s;

// 遥控器数据
typedef struct
{
    int16_t ch0;            // 通道 0
    int16_t ch1;            // 通道 1
    int16_t ch2;            // 通道 2
    int16_t ch3;            // 通道 3
    uint8_t sw_l;           // 左拨杆
    uint8_t sw_r;           // 右拨杆
} RC_Data_t;

/*------------- 外部接口声明 --------------*/

RCInstance *RCRegister(RC_Init_Config_s *config);
RC_Data_t RCDecodeFrame(const uint8_t *data, uint8_t len);

#endif // DRV_RC_H
```

---

## 七、常见问题与解决方案

### Q1: DRV 层如何处理多个相同模块？

**场景：** 4 个 DJI 电机通过同一个 CAN 总线通信。

**解决方案：** 每个电机注册独立的实例，通过不同的 rx_id 区分。

```c
// 注册 4 个电机
Motor_Init_Config_s config = {
    .bsp_instance = &hcan1,
    .tx_id = 0x200,
    .rx_id = 0x201,
    .type = MOTOR_TYPE_M3508,
    .app_callback = MotorDataCallback,
};
motor_lf = MotorRegister(&config);

config.rx_id = 0x202;
motor_rf = MotorRegister(&config);
```

### Q2: 如何处理复杂的协议解析？

**场景：** 协议需要校验和、多字节组装等。

**解决方案：** 在任务上下文中解析，提供独立的解析函数。

```c
IMU_Data_t IMUParseFrame(const uint8_t *data, uint16_t len)
{
    // 校验
    if (CalculateChecksum(data, len) != data[len - 1])
    {
        LOGWARNING("[drv_imu] Checksum error");
        return (IMU_Data_t){0};
    }

    // 解析
    IMU_Data_t result;
    result.accel_x = (int16_t)(data[0] << 8 | data[1]) * ACCEL_SCALE;
    // ...
    return result;
}
```

### Q3: DRV 层是否需要队列？

**场景：** 是否在 DRV 层创建消息队列？

**解决方案：** 一般不需要。DRV 层只负责解析和提供接口，队列由 APP 层管理。

```
APP 层：创建队列，从中断接收原始数据，在任务中解析
DRV 层：提供解析函数和数据访问接口
```

---

## 八、DRV 层检查清单

编写新的 DRV 模块时，请检查以下项目：

### 头文件检查

- [ ] 是否只声明了必要的公开函数？
- [ ] 是否在 .h 文件中公开定义了实例结构体？
- [ ] 是否定义了初始化配置结构体（`_s` 后缀）？
- [ ] 是否遵循了命名规范？

### 源文件检查

- [ ] 私有变量是否使用 `static` 限定？
- [ ] 私有函数是否使用 `static` 限定？
- [ ] 是否正确实现了回调传递？
- [ ] 是否在中断中避免了复杂解析？

### 接口设计检查

- [ ] 是否提供了数据解析接口（在任务中调用）？
- [ ] 是否正确注册了 BSP 层回调？
- [ ] 是否正确传递 APP 层回调？

### 职责边界检查

- [ ] 是否只调用 BSP 接口，不直接操作硬件？
- [ ] 是否不包含业务逻辑？
- [ ] 是否在任务上下文解析数据？
- [ ] 是否默认不使用 FreeRTOS 对象？（守护进程等特殊模块除外）

---

## 九、总结

### DRV 层核心原则

1. **模块抽象**：封装外部模块，提供统一接口
2. **协议解析**：在任务上下文解析，不在中断中解析
3. **回调传递**：中断中只传递原始数据
4. **结构体公开**：实例结构体在 .h 文件中公开定义，便于调试
5. **默认不使用 RTOS**：队列、互斥量等由 APP 层管理；守护进程等特殊模块可创建任务

### 数据流总结

```
中断：HAL → BSP → DRV → APP（传递原始数据，不解析）
任务：APP 调用 DRV 解析函数 → 解析数据 → 业务逻辑
```

---

_本文档适用于 test_my_frame 项目的 DRV 层开发。跨层设计原则请参考《项目架构设计方案》，命名规范请参考《代码规范》。_
