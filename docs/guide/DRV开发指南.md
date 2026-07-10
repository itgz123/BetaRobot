---
outline: deep
---

# DRV 层开发指南

> 本文档描述 DRV 层特有的开发规范，跨层设计原则请参考《项目架构设计方案》。

---

## 一、DRV 层的定位与职责

DRV（Driver，驱动层）是对 **PCB 上集成的外部模块** 的封装，提供模块级别的抽象接口。

### 职责

| 职责 | 说明 |
|------|------|
| 模块抽象 | 封装外部模块，提供统一的操作接口 |
| 协议解析 | 解析模块通信协议（CAN 协议、串口协议等） |
| 数据处理 | 提供数据转换、滤波、校验等功能 |
| 回调传递 | 中断中只传递原始数据，任务中解析 |

### DRV 层不应做什么

- 不直接操作硬件寄存器（调用 BSP 接口）
- 不包含业务逻辑（由 APP 层负责）
- **不在中断中进行复杂的数据解析**
- **默认不使用 FreeRTOS**（队列、互斥量、信号量等由 APP 层负责）

### 职责边界

| 功能 | 职责归属 | 原因 |
|------|----------|------|
| 实例注册 | **DRV 层** | 驱动管理 |
| 回调注册与传递 | **DRV 层** | 中断到 APP 的桥梁 |
| 协议解析函数 | **DRV 层** | 提供给 APP 调用 |
| 数据拷贝到队列 | **APP 层** | 队列是 RTOS 对象 |
| 互斥量保护数据 | **APP 层** | 互斥量是 RTOS 对象 |

---

## 二、核心设计原则

### 2.1 实例注册机制

所有模块都通过注册函数创建实例：

```c
// 实例结构体
typedef struct MotorInstance
{
    uint32_t device_id;
    uint8_t device_type;
    float speed;
    float angle;
    void (*app_callback)(void *raw_data);
    void *app_id;
} MotorInstance;

// 初始化配置结构体
typedef struct
{
    void *bsp_handle;
    uint32_t device_id;
    uint8_t device_type;
    void (*app_callback)(void *raw_data);
    void *app_id;
} Motor_Init_Config_s;

// 注册函数
MotorInstance *MotorRegister(Motor_Init_Config_s *config);
```

### 2.2 回调链设计

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

### 2.3 中断中操作限制

| 操作类型 | 是否允许 | 说明 |
|----------|----------|------|
| 数据拷贝 | ✓ 允许 | 中断的首要任务 |
| 加减乘除 | ✓ 允许 | 基本数学运算 |
| 单位转换 | ✓ 允许 | 如 RPM → rad/s |
| 简单滤波 | ✓ 允许 | 如一阶低通滤波 |
| 协议校验 | ✗ 不允许 | 复杂校验在任务中执行 |
| 复杂算法 | ✗ 不允许 | 如 FFT、矩阵运算 |

---

## 三、数据结构设计

### 原始帧类型（用于中断到任务的传递）

```c
// drv_types.h
typedef struct
{
    uint32_t device_id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t timestamp;
} CanRawFrame_t;

typedef struct
{
    uint8_t data[256];
    uint16_t len;
    uint32_t timestamp;
} UartRawFrame_t;
```

---

## 四、回调机制实现

### DRV 层回调（不解析）

```c
static void MotorCANRxCallback(CANInstance *can_inst)
{
    MotorInstance *motor = (MotorInstance *)can_inst->id;
    if (motor->app_callback != NULL)
    {
        motor->app_callback(can_inst);
    }
}
```

### 解析函数（在任务中调用）

```c
Motor_Data_t MotorDecodeFrame(const uint8_t *data, uint8_t len)
{
    Motor_Data_t result = {0};
    if (data == NULL || len < 7) return result;

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

## 五、典型模块开发模式

### 电机驱动模块

```
头文件（drv_motor.h）：类型定义、实例结构体、配置结构体、接口声明
源文件（drv_motor.c）：私有变量、回调实现、注册函数、解析函数
```

### 遥控器驱动模块

```
头文件（drv_rc.h）：遥控器类型枚举、实例结构体、配置结构体、数据解析
源文件（drv_rc.c）：注册函数、回调传递、协议解析
```

---

## 六、DRV 层检查清单

### 头文件检查
- [ ] 是否只声明了必要的公开函数？
- [ ] 是否在 .h 文件中公开定义了实例结构体？
- [ ] 是否定义了初始化配置结构体（`_s` 后缀）？

### 源文件检查
- [ ] 私有变量是否使用 `static` 限定？
- [ ] 私有函数是否使用 `static` 限定？
- [ ] 是否正确实现了回调传递？

### 职责边界检查
- [ ] 是否只调用 BSP 接口，不直接操作硬件？
- [ ] 是否不包含业务逻辑？
- [ ] 是否在任务上下文解析数据？
- [ ] 是否默认不使用 FreeRTOS 对象？

---

_本文档适用于 test_my_frame 项目的 DRV 层开发。_
