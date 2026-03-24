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

## 三、实例注册机制（静态宏定义模式）

### 3.1 设计原则

本项目采用**静态宏定义 + 注册函数**的架构：

| 特点        | 说明                                      |
| ----------- | ----------------------------------------- |
| 静态定义    | 使用宏在编译时定义实例，无需动态内存分配  |
| 枚举索引    | 宏只存储板载枚举，注册时自动填充硬件映射  |
| 零 HAL 依赖 | APP/DRV 层只需使用枚举，无需接触 HAL 类型 |
| 编译时检查  | 枚举值是编译时常量，可用于静态初始化      |

### 3.2 架构流程

```
┌─────────────────────────────────────────────────────────────┐
│  bsp_cfg.h                                                  │
│  - 定义板载枚举（BoardGPIO_e / BoardUART_e 等）             │
│  - 定义硬件映射数组（gpio_map[] / uart_map[] 等）           │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│  APP/DRV 层使用                                             │
│  XXX_INSTANCE_DEF(name, ENUM, ...);  // 静态定义实例        │
│  XXXRegister(&name);                 // 注册时自动填充映射  │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 代码模板

#### 3.3.1 头文件（bsp_xxx.h）

```c
#ifndef __BSP_XXX_H
#define __BSP_XXX_H

#include "main.h"
#include "bsp_cfg.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief XXX工作模式枚举（如需要）
 */
typedef enum
{
    XXX_BLOCK_MODE = 0,
    XXX_IT_MODE,
    XXX_DMA_MODE,
} XXX_Work_Mode_e;

/**
 * @brief XXX实例结构体
 * @note 第一个成员必须是对应的板载枚举类型
 */
typedef struct XXXInstance
{
    BoardXXX_e xxx_e;                          // 板载枚举（注册时用于查找映射）
    XXX_HandleTypeDef *handle;                 // 硬件句柄（注册时自动填充）
    // ... 其他配置成员
    void (*callback)(struct XXXInstance *);    // 回调函数
} XXXInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义XXX实例
 * @param name    实例名称
 * @param xxx_idx 板载枚举（BoardXXX_e）
 * @param ...     其他参数
 *
 * @note 宏参数名必须不同于结构体成员名（用 xxx_idx 而非 xxx_e）
 *
 * @example
 *   XXX_INSTANCE_DEF(my_xxx, XXX_1, ...);
 */
#define XXX_INSTANCE_DEF(name, xxx_idx, ...) \
    XXXInstance name = {                     \
        .xxx_e = xxx_idx,                    \
        .handle = NULL,                      \
        /* ... 其他成员初始化 */             \
    }

/*------------- 外部接口声明 --------------*/

int8_t XXXRegister(XXXInstance *instance);
void XXXTransmit(XXXInstance *instance, uint8_t *data, uint16_t len);

#endif /* __BSP_XXX_H */
```

#### 3.3.2 源文件（bsp_xxx.c）

```c
#include "bsp_xxx.h"
#include "bsp_log.h"
#include <string.h>

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static XXXInstance *s_xxx_instance[XXX_NUM] = {NULL};

/*------------- 外部接口实现 --------------*/

int8_t XXXRegister(XXXInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[bsp_xxx] Instance is NULL!");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= XXX_NUM)
    {
        LOGERROR("[bsp_xxx] Exceeded max instance count!");
        return -1;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_xxx_instance[i]->handle == instance->handle)
        {
            LOGERROR("[bsp_xxx] Instance already registered!");
            return -1;
        }
    }

    // ⭐ 关键：根据枚举自动填充硬件句柄
    instance->handle = xxx_map[instance->xxx_e].handle;

    s_xxx_instance[s_idx++] = instance;

    LOGINFO("[bsp_xxx] Instance registered, idx=%d", s_idx - 1);
    return 0;
}
```

### 3.4 使用示例

```c
// app/src/app_xxx.c

#include "bsp_xxx.h"

// 定义接收回调
static void MyCallback(XXXInstance *instance)
{
    // 处理接收数据
}

// 静态定义实例
XXX_INSTANCE_DEF(my_xxx, XXX_1, XXX_DMA_MODE, 128, MyCallback);

// 初始化时注册
void AppInit(void)
{
    XXXRegister(&my_xxx);
}
```

### 3.5 关键规则

| 规则                      | 说明                                          |
| ------------------------- | --------------------------------------------- |
| 枚举成员必须在首位        | 结构体第一个成员是 `BoardXXX_e xxx_e`         |
| 宏参数名避免冲突          | 使用 `xxx_idx` 而非 `xxx_e`，否则宏展开会错误 |
| 句柄初始化为 NULL         | 宏中 `.handle = NULL`，注册时填充实际值       |
| 实例数量使用 bsp_cfg.h 宏 | `XXX_NUM` 在 bsp_cfg.h 中根据开发板自动配置   |

---

## 四、回调函数机制

### 4.1 回调函数的使用场景

| 场景     | 触发时机                  | 示例                  |
| -------- | ------------------------- | --------------------- |
| 接收完成 | DMA/IDLE 中断收到数据     | USART RX 回调         |
| 发送完成 | IT/DMA 发送完成           | USART TX 回调（可选） |
| 错误恢复 | 通信错误（帧错误/溢出等） | UART Error 回调       |
| 外部中断 | GPIO 外部中断触发         | EXTI 回调             |

### 4.2 HAL 回调分发实现

```c
/* bsp_xxx.c */

/**
 * @brief HAL 回调函数重写
 * @note 在 HAL 回调中遍历实例数组，找到对应实例并调用其回调
 */
void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hxxx == s_xxx_instance[i]->handle)
        {
            // 保存接收长度（如有）
            s_xxx_instance[i]->rx_len = ...;

            // 调用用户回调
            if (s_xxx_instance[i]->callback != NULL)
            {
                s_xxx_instance[i]->callback(s_xxx_instance[i]);
            }

            // 自动重启接收（如需要）
            XXXRestartReceive(s_xxx_instance[i]);
            return;
        }
    }
}
```

### 4.3 USART 回调示例

```c
/* bsp_usart.c */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (huart == s_usart_instance[i]->handle)
        {
            // 保存接收长度
            s_usart_instance[i]->rx_len = Size;

            // 调用用户回调
            if (s_usart_instance[i]->rx_callback != NULL)
            {
                s_usart_instance[i]->rx_callback(s_usart_instance[i]);
            }

            // 清空缓冲区并重启接收
            memset(s_usart_instance[i]->rx_buff, 0, Size);
            USARTRestartReceive(s_usart_instance[i]);
            return;
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (huart == s_usart_instance[i]->handle)
        {
            uint32_t error_code = huart->ErrorCode;
            LOGWARNING("[bsp_usart] Error detected, idx[%d], code=0x%lX", i, error_code);
            USARTRestartReceive(s_usart_instance[i]);
            return;
        }
    }
}
```

### 4.4 GPIO EXTI 回调示例

```c
/* bsp_gpio.c */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    GPIOInstance *gpio;
    for (uint8_t i = 0; i < s_idx; i++)
    {
        gpio = s_gpio_instance[i];
        // 通过引脚号匹配（注意：多个实例可能共用同一引脚）
        if (gpio->map.pin == GPIO_Pin && gpio->callback != NULL)
        {
            gpio->callback(gpio);
            return;
        }
    }
}
```

### 4.5 回调中的数据访问

用户在回调中可通过实例指针访问数据：

```c
// APP 层回调实现
static void MyUartCallback(USARTInstance *instance)
{
    // 访问接收数据
    uint8_t *data = instance->rx_buff;
    uint16_t len = instance->rx_len;

    // 处理数据...
}
```

### 4.6 回调机制优点

| 优点     | 说明                           |
| -------- | ------------------------------ |
| 模块解耦 | BSP 层不依赖具体业务逻辑       |
| 灵活扩展 | 不同模块可注册不同回调函数     |
| 统一接口 | 所有外设使用相同的回调分发模式 |
| 便于调试 | 回调中可添加日志，追踪数据流   |

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
├── inc/
│   ├── bsp_cfg.h       # BSP 配置（板载枚举、实例数量、硬件映射声明）
│   ├── bsp_gpio.h      # GPIO 外设接口
│   ├── bsp_usart.h     # USART 外设接口
│   ├── bsp_can.h       # CAN 外设接口
│   └── ...
└── src/
    ├── bsp_cfg.c       # 硬件映射数组定义
    ├── bsp_gpio.c      # GPIO 外设实现
    ├── bsp_usart.c     # USART 外设实现
    ├── bsp_can.c       # CAN 外设实现
    └── ...
```

### 6.2 头文件模板（bsp_xxx.h）

```c
/**
 * @file bsp_xxx.h
 * @brief XXX驱动封装，提供实例管理和回调分发功能
 *
 * @note 硬件配置由 CubeMX 负责，BSP 层只管理实例
 */

#ifndef __BSP_XXX_H
#define __BSP_XXX_H

#include "main.h"
#include "bsp_cfg.h"
#include "stdint.h"

/*------------- 类型定义 --------------*/

/**
 * @brief XXX工作模式枚举（如需要）
 */
typedef enum
{
    XXX_BLOCK_MODE = 0,
    XXX_IT_MODE,
    XXX_DMA_MODE,
} XXX_Work_Mode_e;

/**
 * @brief XXX实例结构体
 */
typedef struct XXXInstance
{
    BoardXXX_e xxx_e;                          // 板载枚举（注册时用于查找映射）
    XXX_HandleTypeDef *handle;                 // 硬件句柄（注册时自动填充）
    // ... 业务相关成员
    void (*callback)(struct XXXInstance *);    // 回调函数
} XXXInstance;

/*------------- 实例定义宏 --------------*/

/**
 * @brief 静态定义XXX实例
 * @param name    实例名称
 * @param xxx_idx 板载枚举（BoardXXX_e）
 * @param ...     其他参数
 *
 * @example
 *   XXX_INSTANCE_DEF(my_xxx, XXX_1, ...);
 */
#define XXX_INSTANCE_DEF(name, xxx_idx, ...) \
    XXXInstance name = {                     \
        .xxx_e = xxx_idx,                    \
        .handle = NULL,                      \
        /* ... 其他成员初始化 */             \
    }

/*------------- 外部接口声明 --------------*/

/**
 * @brief 注册XXX实例
 * @param instance XXX实例指针
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t XXXRegister(XXXInstance *instance);

/**
 * @brief 其他操作接口
 */
void XXXTransmit(XXXInstance *instance, uint8_t *data, uint16_t len);
void XXXRestartReceive(XXXInstance *instance);

#endif /* __BSP_XXX_H */
```

### 6.3 源文件模板（bsp_xxx.c）

```c
/**
 * @file bsp_xxx.c
 * @brief XXX驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置
 */

#include "bsp_xxx.h"
#include "bsp_log.h"
#include <string.h>

/*------------- 私有变量 --------------*/
static uint8_t s_idx = 0;
static XXXInstance *s_xxx_instance[XXX_NUM] = {NULL};

/*------------- HAL 回调函数重写 --------------*/

void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hxxx == s_xxx_instance[i]->handle)
        {
            if (s_xxx_instance[i]->callback != NULL)
            {
                s_xxx_instance[i]->callback(s_xxx_instance[i]);
            }
            return;
        }
    }
}

/*------------- 外部接口实现 --------------*/

int8_t XXXRegister(XXXInstance *instance)
{
    // 参数检查
    if (instance == NULL)
    {
        LOGERROR("[bsp_xxx] Instance is NULL!");
        return -1;
    }

    // 实例数量检查
    if (s_idx >= XXX_NUM)
    {
        LOGERROR("[bsp_xxx] Exceeded max instance count!");
        return -1;
    }

    // 重复注册检查
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (s_xxx_instance[i]->handle == instance->handle)
        {
            LOGERROR("[bsp_xxx] Instance already registered!");
            return -1;
        }
    }

    // 根据枚举自动填充硬件句柄
    instance->handle = xxx_map[instance->xxx_e].handle;

    s_xxx_instance[s_idx++] = instance;

    LOGINFO("[bsp_xxx] Instance registered, idx=%d", s_idx - 1);
    return 0;
}

void XXXTransmit(XXXInstance *instance, uint8_t *data, uint16_t len)
{
    if (instance == NULL || data == NULL || len == 0)
    {
        LOGWARNING("[bsp_xxx] Invalid transmit parameters!");
        return;
    }
    // ... 发送实现
}

void XXXRestartReceive(XXXInstance *instance)
{
    if (instance == NULL) return;
    // ... 重启接收实现
}
```

### 6.4 bsp_cfg.h 配置示例

```c
/* bsp/inc/bsp_cfg.h */

/*------------- 板载枚举定义 --------------*/

typedef enum
{
    GPIO_LED_GREEN = 0,
    GPIO_LED_RED,
    // ...
    GPIO_NUM_MAX
} BoardGPIO_e;

typedef enum
{
    UART_SBUS = 0,
    // ...
    UART_NUM_MAX
} BoardUART_e;

/*------------- 硬件映射结构体 --------------*/

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} GPIO_Map_t;

typedef struct
{
    UART_HandleTypeDef *handle;
} UART_Map_t;

/*------------- 硬件映射数组声明 --------------*/

extern const GPIO_Map_t gpio_map[];
extern const UART_Map_t uart_map[];

/*------------- 实例数量配置（根据开发板自动适配）-------------*/

#if DEVELOPMENT_BOARD == STM32F407VET6
#define GPIO_NUM  10
#define UART_NUM  1
#endif
```

### 6.5 bsp_cfg.c 硬件映射定义

```c
/* bsp/src/bsp_cfg.c */

#include "bsp_cfg.h"
#include "usart.h"  // HAL 句柄声明
#include "gpio.h"

/*------------- GPIO 硬件映射 --------------*/
const GPIO_Map_t gpio_map[] = {
    [GPIO_LED_GREEN] = {GPIOE, GPIO_PIN_14},
    [GPIO_LED_RED]   = {GPIOA, GPIO_PIN_1},
    // ...
};

/*------------- UART 硬件映射 --------------*/
const UART_Map_t uart_map[] = {
    [UART_SBUS] = {&huart2},
};
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

## 九、Cortex-M7 内存与 DMA 注意事项

> 本节内容适用于 Cortex-M7 内核（如 STM32H7 系列），Cortex-M4 内核无需特殊处理。

### 9.1 DMA 缓冲区位置要求

**核心原则：DMA 缓冲区必须放入 RAM_D1 或 RAM_D2 区域。**

#### 9.1.1 内存区域与 DMA 支持

| 区域    | 地址       | DMA 支持 | 说明            |
| ------- | ---------- | -------- | --------------- |
| DTCMRAM | 0x20000000 | **否**   | 仅 CPU 可访问   |
| RAM_D1  | 0x24000000 | **是**   | 推荐 DMA 缓冲区 |
| RAM_D2  | 0x30000000 | **是**   | 外设 DMA 专用   |
| RAM_D3  | 0x38000000 | **是**   | 支持 DMA        |
| ITCMRAM | 0x00000000 | **否**   | 仅 CPU 可访问   |

#### 9.1.2 正确做法

```c
/* ✅ 正确：DMA 缓冲区放入 RAM_D1 */
uint8_t usart_rx_buf[512] __attribute__((section(".ram_d1")));
uint8_t usart_tx_buf[512] __attribute__((section(".ram_d1")));

/* ❌ 错误：默认在 DTCMRAM，DMA 无法访问 */
uint8_t usart_rx_buf[512];
```

#### 9.1.3 BSP 层实例定义示例

```c
/* bsp/inc/bsp_usart.h */

#if CPU_IS_M7
#define USART_INSTANCE_DEF(name, uart_idx, mode, buff_sz, cb)                         \
    static uint8_t name##_rx_buff[buff_sz] __attribute__((section(".ram_d1"))) = {0}; \
    static USARTInstance name = {                                                     \
        .uart_e = uart_idx,                                                           \
        .rx_buff = name##_rx_buff,                                                    \
        /* ... */                                                                     \
    }
#else
#define USART_INSTANCE_DEF(name, uart_idx, mode, buff_sz, cb) \
    static uint8_t name##_rx_buff[buff_sz] = {0};             \
    static USARTInstance name = {                             \
        .uart_e = uart_idx,                                   \
        .rx_buff = name##_rx_buff,                            \
        /* ... */                                             \
    }
#endif
```

**注意：只需要缓冲区放入 RAM_D1，实例结构体保持在默认位置（DTCMRAM）访问更快。**

#### 9.1.4 常见问题

| 问题               | 原因                 | 解决方案           |
| ------------------ | -------------------- | ------------------ |
| DMA 传输数据全为 0 | 缓冲区在 DTCMRAM     | 移至 RAM_D1/RAM_D2 |
| DMA 传输数据异常   | 缓冲区在 ITCMRAM     | 移至 RAM_D1/RAM_D2 |
| 接收中断无数据     | DMA 无法写入目标地址 | 检查缓冲区位置     |

### 9.2 强制指针类型转换的危险

**核心原则：避免强制指针类型转换，防止非对齐访问。**

#### 9.2.1 问题场景

```c
/* ❌ 危险：可能导致非对齐访问 */
uint8_t buffer[100];

uint32_t *ptr = (uint32_t*)&buffer[1];  // 地址可能是奇数！
uint32_t value = *ptr;                   // 非对齐访问

uint16_t *ptr2 = (uint16_t*)&buffer[3]; // 地址不是2的倍数！
uint16_t val2 = *ptr2;                   // 非对齐访问
```

**原因：** `buffer` 的地址可能是任意值，`&buffer[1]` 或 `&buffer[3]` 不一定是 4 字节或 2 字节对齐。

#### 9.2.2 非对齐访问的后果

| 后果     | 说明                           |
| -------- | ------------------------------ |
| 性能下降 | CPU 需要多次内存访问           |
| 数据错误 | 某些架构可能读取错误数据       |
| 硬件异常 | 启用 UFSR.UNALIGNED 时触发异常 |

#### 9.2.3 正确做法

```c
/* ✅ 方法1：使用 memcpy（安全，编译器处理对齐） */
uint8_t buffer[100];
uint32_t value;
memcpy(&value, &buffer[1], sizeof(uint32_t));

/* ✅ 方法2：确保缓冲区对齐 */
uint8_t buffer[100] __attribute__((aligned(4)));  // 4 字节对齐
uint32_t *ptr = (uint32_t*)&buffer[0];  // buffer[0] 地址是 4 的倍数
uint32_t value = *ptr;  // 对齐访问

/* ✅ 方法3：逐字节解析（协议解析推荐） */
uint32_t value = ((uint32_t)buffer[1] << 24) |
                 ((uint32_t)buffer[2] << 16) |
                 ((uint32_t)buffer[3] << 8)  |
                 ((uint32_t)buffer[4]);
```

#### 9.2.4 BSP 层协议解析示例

```c
/* bsp/src/bsp_can.c */

/* ❌ 错误：强制转换可能导致非对齐 */
void CAN_ParseFrame(uint8_t *data, uint16_t len)
{
    uint32_t id = *(uint32_t*)&data[0];   // 危险！
    float value = *(float*)&data[4];       // 危险！
}

/* ✅ 正确：逐字节解析 */
void CAN_ParseFrame(uint8_t *data, uint16_t len)
{
    uint32_t id = ((uint32_t)data[0] << 24) |
                  ((uint32_t)data[1] << 16) |
                  ((uint32_t)data[2] << 8)  |
                  ((uint32_t)data[3]);

    // 或使用 memcpy
    float value;
    memcpy(&value, &data[4], sizeof(float));
}
```

### 9.3 检查清单

编写 BSP 层 DMA 相关代码时，请检查：

- [ ] DMA 缓冲区是否使用 `__attribute__((section(".ram_d1")))`（Cortex-M7）？
- [ ] 是否避免了强制指针类型转换 `(uint32_t*)&buf[x]`？
- [ ] 协议解析是否使用逐字节或 memcpy 方式？
- [ ] 接收缓冲区数组是否对齐（`aligned(4)`）？

---

## 十、常见问题与解决方案

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

## 十一、BSP 层检查清单

编写新的 BSP 模块时，请检查以下项目：

### 11.1 结构体设计

- [ ] 结构体第一个成员是否为板载枚举（`BoardXXX_e xxx_e`）？
- [ ] 硬件句柄成员是否初始化为 `NULL`（宏中）？
- [ ] 是否包含回调函数指针成员？
- [ ] 是否直接使用 HAL 类型而非过度封装？

### 11.2 宏定义

- [ ] 宏参数名是否不同于结构体成员名？（避免 `xxx_e` 作为参数名）
- [ ] 宏中是否只存储枚举值，不访问映射数组？
- [ ] 宏是否可以同时定义缓冲区（如需要）？
- [ ] （Cortex-M7）DMA 缓冲区是否使用 `__attribute__((section(".ram_d1")))`？

### 11.3 注册函数

- [ ] 是否检查参数 NULL？
- [ ] 是否检查实例数量上限？
- [ ] 是否检查重复注册？
- [ ] **关键**：是否根据枚举自动填充硬件映射？
- [ ] 是否记录日志？

### 11.4 回调分发

- [ ] HAL 回调中是否正确遍历实例数组？
- [ ] 回调前是否检查 callback != NULL？
- [ ] 是否自动重启接收/恢复错误？

### 11.5 配置文件

- [ ] 是否在 `bsp_cfg.h` 中添加板载枚举？
- [ ] 是否在 `bsp_cfg.h` 中添加硬件映射结构体？
- [ ] 是否在 `bsp_cfg.c` 中定义硬件映射数组？
- [ ] 是否在 `bsp_cfg.h` 中根据开发板配置实例数量？
- [ ] 枚举命名是否使用功能命名（如 `GPIO_LED_GREEN`）而非物理命名（如 `GPIO_PE14`）？
- [ ] 是否区分物理硬件数量（`XXX_HW_NUM`）和逻辑实例数量（`XXX_INSTANCE_NUM`）？

### 11.6 竞争问题

- [ ] 总线类外设是否提供状态查询接口（如 `XXX_IsReady()`）？
- [ ] 发送函数内部是否检查硬件状态？
- [ ] 文档是否说明发送前需检查状态？

### 11.7 使用示例（APP/DRV 层）

```c
// ✅ 正确用法
#include "bsp_xxx.h"

static void MyCallback(XXXInstance *inst) { ... }
XXX_INSTANCE_DEF(my_inst, XXX_1, ...);
XXXRegister(&my_inst);

// ❌ 错误用法
XXX_INSTANCE_DEF(my_inst, xxx_map[XXX_1].handle, ...);  // 不能在宏中访问数组
```

---

## 十二、数学加速模块（bsp_math）

### 12.1 模块概述

`bsp_math` 模块提供统一的数学函数接口，根据硬件特性自动选择最优实现：

| 优先级 | 实现方式      | 适用芯片                 | 性能 |
| ------ | ------------- | ------------------------ | ---- |
| 1      | CORDIC 硬件   | STM32H7/G4 系列          | 最快 |
| 2      | CMSIS-DSP 库  | 有 DSP 指令集的 Cortex-M | 较快 |
| 3      | 标准库 math.h | 所有平台                 | 基础 |

### 12.2 重要原则：禁止直接使用 math.h

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

### 12.3 可用接口

| 函数                   | 功能                | 说明                     |
| ---------------------- | ------------------- | ------------------------ |
| `BSP_Math_Sin(theta)`  | 计算 sin(theta)     | theta 为弧度             |
| `BSP_Math_Cos(theta)`  | 计算 cos(theta)     | theta 为弧度             |
| `BSP_Math_SinCos(...)` | 同时计算 sin 和 cos | 比 separate 调用效率更高 |
| `BSP_Math_Atan2(y, x)` | 计算 atan2(y, x)    | 返回弧度，自动判断象限   |
| `BSP_Math_Sqrt(x)`     | 计算 sqrt(x)        | x 必须 >= 0              |

### 12.4 使用示例

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

### 12.5 常用数学常量

`bsp_math.h` 中定义了常用数学常量，应优先使用：

```c
#define M_PI    3.14159265358979323846f  // π
#define M_PI_2  1.57079632679489661923f  // π/2
#define M_E     2.71828182845904523536f  // e

#define DEG_TO_RAD(deg)  ((deg) * M_PI / 180.0f)   // 角度转弧度
#define RAD_TO_DEG(rad)  ((rad) * 180.0f / M_PI)   // 弧度转角度
```

### 12.6 硬件特性查询

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

### 12.7 各开发板加速情况

| 开发板       | CORDIC | CMSIS-DSP | 实际使用  |
| ------------ | ------ | --------- | --------- |
| TELESKY_VET6 | ❌     | ✅        | CMSIS-DSP |
| DM_MC02      | ✅     | ✅        | CORDIC    |
| DJI_A        | ❌     | ✅        | CMSIS-DSP |
| DJI_C        | ❌     | ✅        | CMSIS-DSP |

### 12.8 初始化

在使用数学函数前，需要在系统初始化时调用：

```c
void App_Robot_Init(void)
{
    BSP_Math_Init();  // 初始化数学加速模块
    // ...其他初始化
}
```

---

## 十三、枚举命名与数量限制设计

### 13.1 枚举命名规范

#### 13.1.1 功能命名 vs 物理命名

**原则：板载枚举使用功能命名，而非物理引脚命名。**

| 类型        | 示例             | 说明                         |
| ----------- | ---------------- | ---------------------------- |
| ✅ 功能命名 | `GPIO_LED_GREEN` | 表示功能（绿色 LED）         |
| ❌ 物理命名 | `GPIO_PE14`      | 表示物理引脚（与开发板耦合） |
| ✅ 功能命名 | `UART_SBUS`      | 表示功能（SBUS 遥控接收）    |
| ❌ 物理命名 | `UART_USART2`    | 表示物理外设（与开发板耦合） |

#### 13.1.2 目的

**切换开发板后 APP/DRV 层代码无需修改。**

```c
// app/src/app_led.c - 使用功能枚举

#include "bsp_gpio.h"

// ✅ 正确：使用功能枚举，切换开发板无需修改
GPIO_INSTANCE_DEF(led_green, GPIO_LED_GREEN, NULL);
GPIO_INSTANCE_DEF(led_red, GPIO_LED_RED, NULL);

// ❌ 错误：使用物理引脚，切换开发板后需要修改
// GPIO_INSTANCE_DEF(led_green, GPIO_PE14, NULL);  // DJI_C 可能是 PA5
```

#### 13.1.3 命名约定

| 外设类型 | 命名前缀 | 示例                                 |
| -------- | -------- | ------------------------------------ |
| GPIO     | `GPIO_`  | `GPIO_LED_GREEN`, `GPIO_MOTOR_1_IN1` |
| TIM      | `TIM_`   | `TIM_MOTOR_PWM`, `TIM_ENCODER_1`     |
| UART     | `UART_`  | `UART_SBUS`, `UART_DEBUG`            |
| CAN      | `CAN_`   | `CAN_MOTOR`, `CAN_IMU`（逻辑实例）   |
| I2C      | `I2C_`   | `I2C_IMU`, `I2C_EEPROM`              |
| SPI      | `SPI_`   | `SPI_FLASH`, `SPI_SCREEN`            |

### 13.2 竞争问题处理

#### 13.2.1 问题场景

总线类外设（CAN/I2C/SPI）可能被多个逻辑实例共用同一物理硬件：

```
┌─────────────────────────────────────────────────────────────┐
│  CAN1 物理硬件                                               │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  逻辑实例1：电机驱动（订阅 ID 0x201-0x204）              ││
│  │  逻辑实例2：IMU 驱动（订阅 ID 0x501）                    ││
│  │  逻辑实例3：裁判系统（订阅 ID 0x180）                    ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

**问题：** 如果多个实例同时发送，可能导致数据冲突或丢失。

#### 13.2.2 解决方案：发送前检查状态

**BSP 层必须提供状态查询接口：**

```c
/* bsp_can.h */

/**
 * @brief 检查 CAN 是否空闲可发送
 * @param instance CAN 实例指针
 * @retval 1 空闲，可发送
 * @retval 0 忙碌，不可发送
 */
uint8_t CAN_IsReady(CANInstance *instance);
```

**APP/DRV 层发送前必须检查：**

```c
/* drv/src/drv_motor.c */

void Motor_SendCommand(MotorInstance *motor, uint8_t *data, uint16_t len)
{
    // ✅ 正确：发送前检查状态
    if (CAN_IsReady(motor->can_instance))
    {
        CAN_Transmit(motor->can_instance, data, len);
    }
    else
    {
        // 处理发送失败（队列、重试或丢弃）
        LOGWARNING("[drv_motor] CAN busy, command dropped!");
    }
}
```

#### 13.2.3 状态检查实现示例

```c
/* bsp/src/bsp_can.c */

uint8_t CAN_IsReady(CANInstance *instance)
{
    if (instance == NULL || instance->handle == NULL)
    {
        return 0;
    }

    // 检查 CAN 硬件状态
    return (instance->handle->State == HAL_CAN_STATE_READY);
}

void CAN_Transmit(CANInstance *instance, uint8_t *data, uint16_t len)
{
    // 内部也做状态检查
    if (!CAN_IsReady(instance))
    {
        LOGWARNING("[bsp_can] Hardware not ready!");
        return;
    }

    // 执行发送
    CAN_TxHeaderTypeDef header = {0};
    header.StdId = instance->tx_id;
    header.IDE = CAN_ID_STD;
    header.DLC = len;

    uint32_t mailbox;
    HAL_CAN_AddTxMessage(instance->handle, &header, data, &mailbox);
}
```

### 13.3 数量限制设计

#### 13.3.1 两种数量的区分

| 概念         | 说明                         | 定义方式              | 示例               |
| ------------ | ---------------------------- | --------------------- | ------------------ |
| 物理硬件数量 | MCU 上的物理外设数量         | 枚举 `_NUM_MAX`       | `CAN_HW_NUM_MAX`   |
| 逻辑实例数量 | APP/DRV 层可注册的最大实例数 | 宏 `XXX_INSTANCE_NUM` | `CAN_INSTANCE_NUM` |

#### 13.3.2 配置示例

```c
/* bsp/inc/bsp_cfg.h */

#if DEVELOPMENT_BOARD == STM32F407VET6

/*---------- 逻辑实例数量（APP/DRV 层可注册的最大实例数）----------*/
/* 总线类外设：多个逻辑实例可共用同一物理硬件 */
#define CAN_INSTANCE_NUM 4   // 最多 4 个 CAN 订阅者
#define I2C_INSTANCE_NUM 2   // 最多 2 个 I2C 设备

/* 独占类外设：逻辑实例与物理硬件一一对应 */
#define GPIO_INSTANCE_NUM 10  // GPIO 实例数量
#define UART_INSTANCE_NUM 1   // UART 实例数量
#define TIM_INSTANCE_NUM 5    // TIM 实例数量（1 PWM + 4 编码器）

#endif
```

#### 13.3.3 使用场景

**物理硬件数量（枚举 `_NUM_MAX`）：**

- 由枚举自动统计，无需手动定义
- 用于索引硬件映射数组

```c
/* bsp/inc/bsp_cfg.h */
typedef enum {
    GPIO_LED_GREEN = 0,
    GPIO_LED_RED,
    // ...
    GPIO_NUM_MAX  // 自动统计物理硬件数量
} BoardGPIO_e;

/* bsp/src/bsp_cfg.c */
const GPIO_Map_t gpio_map[GPIO_NUM_MAX] = {
    [GPIO_LED_GREEN] = {GPIOE, GPIO_PIN_14},
    [GPIO_LED_RED]   = {GPIOA, GPIO_PIN_1},
};

/* bsp/src/bsp_gpio.c */
int8_t GPIO_Register(GPIOInstance *instance)
{
    // 检查枚举值是否有效
    if (instance->gpio_e >= GPIO_NUM_MAX)
    {
        LOGERROR("[bsp_gpio] Invalid GPIO enum!");
        return -1;
    }

    // 根据枚举填充硬件映射
    instance->map = gpio_map[instance->gpio_e];
    // ...
}
```

**逻辑实例数量（`XXX_INSTANCE_NUM`）：**

- 用于定义实例数组大小
- 用于注册时检查数量上限

```c
/* bsp/src/bsp_gpio.c */
static uint8_t s_idx = 0;
static GPIOInstance *s_gpio_instances[GPIO_INSTANCE_NUM];

int8_t GPIO_Register(GPIOInstance *instance)
{
    // 检查逻辑实例数量上限
    if (s_idx >= GPIO_INSTANCE_NUM)
    {
        LOGERROR("[bsp_gpio] Exceeded max instance count!");
        return -1;
    }
    // ...
}
```

#### 13.3.4 外设类型分类

| 类型   | 特点                     | 示例          | 说明                     |
| ------ | ------------------------ | ------------- | ------------------------ |
| 总线类 | 多实例可共用同一物理硬件 | CAN/I2C/SPI   | `INSTANCE_NUM >= 枚举数` |
| 独占类 | 实例与硬件一一对应       | GPIO/UART/TIM | `INSTANCE_NUM = 枚举数`  |

---

_本文档适用于 test_my_frame 项目的 BSP 层开发。跨层设计原则请参考《项目架构设计方案》，命名规范请参考《代码规范》。_
