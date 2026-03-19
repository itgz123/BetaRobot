# BSP 层开发指南

> 本文档描述 BSP 层特有的开发规范，跨层设计原则请参考《项目架构设计方案》。

---

## 一、BSP 层的定位与职责

### 1.1 BSP 层是什么

BSP（Board Support Package，板级支持包）层是对 **MCU 片上外设** 的封装，提供硬件抽象接口。

**重要区分：**

- BSP 层：封装 MCU 内部外设（CAN、USART、SPI、I2C、GPIO 等）
- DRV 层：封装 PCB 上集成的外部模块（电机、IMU、遥控器等）
- APP 层：业务逻辑

### 1.2 BSP 层的职责

| 职责     | 说明                              |
| -------- | --------------------------------- |
| 硬件抽象 | 封装 HAL 库，提供统一、简洁的接口 |
| 资源管理 | 管理外设实例，处理中断回调分发    |
| 平台无关 | 向上层隐藏硬件细节，便于移植      |
| 错误处理 | 提供基本的错误检测和日志输出      |

### 1.3 BSP 层不应做什么

- 不包含业务逻辑
- 不处理协议解析（由 DRV 层负责）
- 不直接操作外部设备
- 不包含复杂的状态机
- **不负责外设的具体硬件配置**（由 CubeMX/HAL 负责）

### 1.4 BSP 层与硬件配置的职责边界

**重要原则：BSP 层不负责外设的具体硬件配置**

| 配置内容                   | 职责归属       | 原因     |
| -------------------------- | -------------- | -------- |
| 引脚模式（上拉/下拉/浮空） | **CubeMX/HAL** | 硬件相关 |
| 中断使能/优先级            | **CubeMX/HAL** | 硬件相关 |
| 引脚复用功能               | **CubeMX/HAL** | 硬件相关 |
| 时钟配置                   | **CubeMX/HAL** | 硬件相关 |
| 实例注册与回调分发         | **BSP 层**     | 软件抽象 |

**正确的分层架构：**

```
┌─────────────────────────────────────────────────────────┐
│  应用层 (Application)                                    │
│  - 业务逻辑                                              │
│  - 调用 BSP 接口注册实例                                  │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  BSP 层 (Board Support Package)                          │
│  - 实例注册管理                                          │
│  - 回调分发                                              │
│  - 提供统一的操作接口                                    │
│  - 不负责硬件配置                                        │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  HAL 层 (Hardware Abstraction Layer)                     │
│  - CubeMX 生成的初始化代码                               │
│  - 硬件配置（上拉/下拉/中断/复用/时钟）                   │
└─────────────────────────────────────────────────────────┘
```

---

## 二、类型封装原则

### 2.1 避免过度封装

**重要原则：只封装操作接口，不封装 HAL 类型**

#### 问题示例

如果 BSP 层封装了 `BSP_GPIO_PinState_e`，但用户仍需使用 HAL 的 `GPIOA`、`GPIO_PIN_1`：

```c
// 用户代码 - 类型混用，体验割裂
GPIO_Init_Config_s config = {
    .GPIOx = GPIOA,                    // HAL 类型
    .GPIO_Pin = GPIO_PIN_1,            // HAL 类型
    .pin_state = BSP_GPIO_PIN_RESET,   // BSP 类型 - 为什么要单独封装？
};
```

#### 正确做法：直接使用 HAL 类型

```c
// bsp_gpio.h - 简化设计
typedef struct gpio_instance
{
    GPIO_TypeDef *GPIOx;        // 直接使用 HAL 类型
    uint16_t GPIO_Pin;          // 直接使用 HAL 类型
    GPIO_PinState pin_state;    // 直接使用 HAL 类型
    void (*callback)(struct gpio_instance *);
    void *id;
} GPIOInstance;
```

### 2.2 封装原则总结

| 需要封装                                | 不需要封装                       |
| --------------------------------------- | -------------------------------- |
| 实例管理（注册/注销）                   | GPIO 端口类型（`GPIO_TypeDef*`） |
| 回调分发机制                            | 引脚号宏（`GPIO_PIN_x`）         |
| 操作接口（Toggle/Set/Reset/Read/Write） | 引脚状态（`GPIO_PinState`）      |
| 错误检查和日志                          | 中断模式（由 CubeMX 配置）       |

---

## 三、实例注册机制

### 3.1 代码模板

```c
// 1. 定义最大实例数量（在 .h 文件中）
#define XXX_DEVICE_CNT 8

// 2. 定义实例结构体（在 .h 文件中）
typedef struct xxx_temp
{
    // 硬件句柄
    XXX_HandleTypeDef *handle;

    // 配置参数
    uint32_t config_param1;
    uint8_t config_param2;

    // 回调函数
    void (*callback)(struct xxx_temp *);

    // 实例标识
    void *id;

} XXXInstance;

// 3. 定义初始化配置结构体（在 .h 文件中）
typedef struct
{
    XXX_HandleTypeDef *handle;
    uint32_t config_param1;
    uint8_t config_param2;
    void (*callback)(XXXInstance *);
    void *id;
} XXX_Init_Config_s;

// 4. 实现实例数组和管理索引（在 .c 文件中）
static uint8_t s_idx = 0;
static XXXInstance *s_xxx_instance[XXX_DEVICE_CNT] = {NULL};

// 5. 实现注册函数（在 .c 文件中）
XXXInstance *XXXRegister(XXX_Init_Config_s *config)
{
    // 检查实例数量限制
    if (s_idx >= XXX_DEVICE_CNT)
    {
        LOGERROR("[bsp_xxx] Instance count exceeded max limit!");
        return NULL;
    }

    // 分配内存并清零
    XXXInstance *instance = (XXXInstance *)malloc(sizeof(XXXInstance));
    memset(instance, 0, sizeof(XXXInstance));

    // 初始化实例成员
    instance->handle = config->handle;
    instance->config_param1 = config->config_param1;
    instance->config_param2 = config->config_param2;
    instance->callback = config->callback;
    instance->id = config->id;

    // 保存到实例数组
    s_xxx_instance[s_idx++] = instance;

    return instance;
}
```

### 3.2 注册函数要点

1. **参数检查**：检查 config 是否为 NULL
2. **数量检查**：检查是否超过最大实例数
3. **内存分配**：使用 malloc 分配实例内存
4. **成员初始化**：从配置结构体复制参数
5. **保存实例**：添加到实例数组

---

## 四、回调函数机制

### 4.1 回调函数的使用场景

- 接收完成回调
- 发送完成回调
- 错误回调
- 外部中断回调

### 4.2 HAL 回调分发实现

```c
// 1. 在实例结构体中定义回调函数指针
typedef struct xxx_temp
{
    // ...其他成员
    void (*callback)(struct xxx_temp *);
} XXXInstance;

// 2. 在 HAL 回调函数中分发到对应的实例回调
void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_xxx_instance[i]->handle == hxxx)
        {
            if (s_xxx_instance[i]->callback != NULL)
            {
                s_xxx_instance[i]->callback(s_xxx_instance[i]);
            }
            return;
        }
    }
}
```

### 4.3 回调机制的优点

- 模块间解耦，BSP 层不依赖具体模块
- 灵活扩展，不同模块可注册不同回调
- 便于测试和调试

---

## 五、多工作模式支持

### 5.1 常见模式

| 模式                 | 特点     | 适用场景         |
| -------------------- | -------- | ---------------- |
| 阻塞模式（Blocking） | 简单可靠 | 低速场景         |
| 中断模式（IT）       | 非阻塞   | 中等速率         |
| DMA 模式             | 高效     | 高速大量数据传输 |

### 5.2 代码模板

```c
// 1. 定义工作模式枚举
typedef enum
{
    XXX_BLOCK_MODE = 0,
    XXX_IT_MODE,
    XXX_DMA_MODE,
} XXX_Work_Mode_e;

// 2. 在实例结构体中保存工作模式
typedef struct xxx_temp
{
    XXX_Work_Mode_e work_mode;
    // ...其他成员
} XXXInstance;

// 3. 根据模式选择不同的 HAL 函数
void XXXTransmit(XXXInstance *instance, uint8_t *data, uint16_t len)
{
    switch (instance->work_mode)
    {
    case XXX_BLOCK_MODE:
        HAL_XXX_Transmit(instance->handle, data, len, 100);
        break;
    case XXX_IT_MODE:
        HAL_XXX_Transmit_IT(instance->handle, data, len);
        break;
    case XXX_DMA_MODE:
        HAL_XXX_Transmit_DMA(instance->handle, data, len);
        break;
    default:
        LOGERROR("[bsp_xxx] Unknown work mode!");
        break;
    }
}
```

---

## 六、文件组织结构

### 6.1 标准目录结构

```
bsp/
├── bsp_cfg.h           # BSP 层配置（实例数量、常量定义等）
├── Inc/
│   ├── bsp_can.h       # CAN 外设接口
│   ├── bsp_usart.h     # USART 外设接口
│   ├── bsp_gpio.h      # GPIO 外设接口
│   └── ...
└── Src/
    ├── bsp_can.c       # CAN 外设实现
    ├── bsp_usart.c     # USART 外设实现
    ├── bsp_gpio.c      # GPIO 外设实现
    └── ...
```

### 6.2 头文件模板

```c
#ifndef BSP_XXX_H
#define BSP_XXX_H

#include "main.h"
#include "bsp_cfg.h"
#include "stdint.h"

/*------------- 宏定义 --------------*/
#define XXX_DEVICE_CNT      8
#define XXX_BUFF_SIZE       256

/*------------- 类型定义 --------------*/
// 枚举类型
typedef enum { ... } XXX_Mode_e;

// 实例结构体
typedef struct xxx_temp { ... } XXXInstance;

// 初始化配置结构体
typedef struct { ... } XXX_Init_Config_s;

/*------------- 外部接口声明 --------------*/
XXXInstance *XXXRegister(XXX_Init_Config_s *config);
void XXXTransmit(XXXInstance *instance, uint8_t *data, uint16_t len);

#endif // BSP_XXX_H
```

### 6.3 源文件模板

```c
#include "bsp_xxx.h"
#include "bsp_log.h"
#include <stdlib.h>
#include <string.h>

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static XXXInstance *s_xxx_instance[XXX_DEVICE_CNT] = {NULL};

/*------------- 私有函数声明 --------------*/
static void XXXServiceInit(void);

/*------------- HAL 回调函数重写 --------------*/
void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    // 回调分发逻辑
}

/*------------- 外部接口实现 --------------*/
XXXInstance *XXXRegister(XXX_Init_Config_s *config)
{
    // 注册实现
}

/*------------- 私有函数实现 --------------*/
static void XXXServiceInit(void)
{
    // 服务初始化
}
```

---

## 七、错误处理与日志

### 7.1 错误检测

```c
XXXInstance *XXXRegister(XXX_Init_Config_s *config)
{
    // 参数检查
    if (config == NULL)
    {
        LOGERROR("[bsp_xxx] Config is NULL!");
        return NULL;
    }

    // 数量检查
    if (s_idx >= XXX_DEVICE_CNT)
    {
        LOGERROR("[bsp_xxx] Exceeded max instance count!");
        return NULL;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_xxx_instance[i]->handle == config->handle)
        {
            LOGERROR("[bsp_xxx] Instance already registered!");
            return NULL;
        }
    }

    // ...初始化代码
}
```

### 7.2 错误恢复

```c
// 串口错误回调中的自动恢复
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_usart_instance[i]->handle == huart)
        {
            LOGWARNING("[bsp_usart] Error detected, reinitializing...");
            // 重新启动接收
            HAL_UARTEx_ReceiveToIdle_DMA(huart,
                s_usart_instance[i]->recv_buff,
                s_usart_instance[i]->recv_buff_size);
            return;
        }
    }
}
```

---

## 八、性能优化建议

### 8.1 减少中断处理时间

**原则：** 中断回调中只做必要的数据拷贝和标志设置。

```c
void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    // 快速找到实例
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_xxx_instance[i]->handle == hxxx)
        {
            // 只拷贝数据
            memcpy(s_xxx_instance[i]->rx_buff, temp_buff, len);
            // 调用回调
            if (s_xxx_instance[i]->callback)
                s_xxx_instance[i]->callback(s_xxx_instance[i]);
            return;
        }
    }
}
```

### 8.2 实例查找优化

对于实例较多的情况，可以考虑优化查找效率：

```c
// 方案1：为每个硬件外设维护独立数组
static XXXInstance *s_xxx1_instances[8];
static XXXInstance *s_xxx2_instances[8];

// 方案2：使用句柄值作为索引（如果句柄是连续的）
```

---

## 九、常见问题与解决方案

### Q1: 如何处理多实例共享同一硬件的情况？

**场景：** 多个模块共用一个 CAN 总线。

**解决方案：** 通过不同的 ID/过滤器区分，在回调中根据 ID 分发。

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    for (size_t i = 0; i < s_idx; i++)
    {
        if (s_can_instance[i]->can_handle == hcan &&
            rx_id == s_can_instance[i]->rx_id)
        {
            s_can_instance[i]->callback(s_can_instance[i]);
            return;
        }
    }
}
```

### Q2: 如何处理发送队列问题？

**场景：** 连续调用发送函数导致数据丢失。

**解决方案：**

1. 添加发送队列
2. 在发送前检查状态
3. 提供状态查询接口

```c
// 提供状态查询接口
uint8_t XXXIsReady(XXXInstance *instance)
{
    return (instance->handle->gState == HAL_XXX_STATE_READY);
}

// 使用
if (XXXIsReady(instance))
{
    XXXTransmit(instance, data, len);
}
```

### Q3: 如何在临界区中使用延时？

**场景：** 在关闭中断的情况下需要延时。

**解决方案：** 使用 DWT 计时器，它不受中断影响。

```c
// 使用 DWT 延时（可在临界区使用）
DWT_Delay(0.001f);  // 延时 1ms

// 不要使用 HAL_Delay()（会依赖 SysTick 中断）
```

---

## 十、BSP 层检查清单

编写新的 BSP 模块时，请检查以下项目：

- [ ] 是否使用实例注册机制？
- [ ] 是否提供了初始化配置结构体？
- [ ] 是否定义了最大实例数量并做检查？
- [ ] 是否正确实现了回调分发？
- [ ] 是否支持多种工作模式（如适用）？
- [ ] 是否添加了参数验证和错误处理？
- [ ] 是否使用了统一的日志接口？
- [ ] 是否直接使用 HAL 类型而非过度封装？
- [ ] 是否避免在 BSP 层配置硬件参数？

---

## 十一、数学加速模块（bsp_math）

### 11.1 模块概述

`bsp_math` 模块提供统一的数学函数接口，根据硬件特性自动选择最优实现：

| 优先级 | 实现方式      | 适用芯片                   | 性能   |
| ------ | ------------- | -------------------------- | ------ |
| 1      | CORDIC 硬件   | STM32H7/G4 系列            | 最快   |
| 2      | CMSIS-DSP 库  | 有 DSP 指令集的 Cortex-M   | 较快   |
| 3      | 标准库 math.h | 所有平台                   | 基础   |

### 11.2 重要原则：禁止直接使用 math.h

**项目中所有数学运算必须通过 `bsp_math` 接口进行，禁止直接调用 `<math.h>` 的函数。**

```c
// ❌ 错误：直接使用标准库
float result = sinf(theta);
float angle = atan2f(y, x);

// ✅ 正确：使用 bsp_math 接口
float result = BSP_Math_Sin(theta);
float angle = BSP_Math_Atan2(y, x);
```

**原因：**
- 自动适配不同硬件平台
- 充分利用硬件加速特性
- 便于性能优化和调试

### 11.3 可用接口

| 函数                    | 功能               | 说明                         |
| ----------------------- | ------------------ | ---------------------------- |
| `BSP_Math_Sin(theta)`   | 计算 sin(theta)    | theta 为弧度                 |
| `BSP_Math_Cos(theta)`   | 计算 cos(theta)    | theta 为弧度                 |
| `BSP_Math_SinCos(...)`  | 同时计算 sin 和 cos | 比 separate 调用效率更高     |
| `BSP_Math_Atan2(y, x)`  | 计算 atan2(y, x)   | 返回弧度，自动判断象限       |
| `BSP_Math_Sqrt(x)`      | 计算 sqrt(x)       | x 必须 >= 0                  |

### 11.4 使用示例

```c
#include "bsp_math.h"

void chassis_control(float vx, float vy, float omega)
{
    float theta = 0.0f;
    
    // 计算运动分解
    for (int i = 0; i < 4; i++)
    {
        // ❌ 错误写法
        // float sin_val = sinf(theta);
        // float cos_val = cosf(theta);
        
        // ✅ 正确写法：同时计算 sin 和 cos（CORDIC 下更高效）
        float sin_val, cos_val;
        BSP_Math_SinCos(theta, &sin_val, &cos_val);
        
        // 计算轮子速度
        float wheel_speed = vx * cos_val + vy * sin_val + omega * radius;
        
        theta += M_PI_2;  // 使用 bsp_math.h 中定义的 M_PI_2
    }
}

float get_heading(float x, float y)
{
    // ✅ 使用 BSP_Math_Atan2 计算航向角
    return BSP_Math_Atan2(y, x);
}

float calculate_distance(float dx, float dy)
{
    // ✅ 使用 BSP_Math_Sqrt 计算距离
    return BSP_Math_Sqrt(dx * dx + dy * dy);
}
```

### 11.5 常用数学常量

`bsp_math.h` 中定义了常用数学常量，应优先使用：

```c
#define M_PI    3.14159265358979323846f  // π
#define M_PI_2  1.57079632679489661923f  // π/2
#define M_E     2.71828182845904523536f  // e

#define DEG_TO_RAD(deg)  ((deg) * M_PI / 180.0f)   // 角度转弧度
#define RAD_TO_DEG(rad)  ((rad) * 180.0f / M_PI)   // 弧度转角度
```

### 11.6 硬件特性查询

可通过以下接口查询当前平台的硬件特性：

```c
if (BSP_Math_HasCORDIC())
{
    // 当前平台有 CORDIC 硬件加速
}

if (BSP_Math_HasDSP())
{
    // 当前平台有 DSP 指令集
}
```

### 11.7 各开发板加速情况

| 开发板        | CORDIC | CMSIS-DSP | 实际使用 |
| ------------- | ------ | --------- | -------- |
| TELESKY_VET6  | ❌      | ✅         | CMSIS-DSP |
| DM_MC02       | ✅      | ✅         | CORDIC   |
| DJI_A         | ❌      | ✅         | CMSIS-DSP |
| DJI_C         | ❌      | ✅         | CMSIS-DSP |

### 11.8 初始化

在使用数学函数前，需要在系统初始化时调用：

```c
void App_Robot_Init(void)
{
    BSP_Math_Init();  // 初始化数学加速模块
    // ...其他初始化
}
```

---

_本文档适用于 test_my_frame 项目的 BSP 层开发。跨层设计原则请参考《项目架构设计方案》，命名规范请参考《代码规范》。_
