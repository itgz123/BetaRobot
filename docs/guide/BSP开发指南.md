---
outline: deep
---

# BSP 层开发指南

> 本文档描述 BSP 层特有的开发规范，跨层设计原则请参考《项目架构设计方案》。

---

## 一、BSP 层的定位与职责

BSP（Board Support Package，板级支持包）层是对 **MCU 片上外设** 的封装，提供硬件抽象接口。

### 职责

| 职责 | 说明 |
|------|------|
| 硬件抽象 | 封装 HAL 库，提供统一、简洁的接口 |
| 资源管理 | 管理外设实例，处理中断回调分发 |
| 平台无关 | 向上层隐藏硬件细节，便于移植 |
| 错误处理 | 提供基本的错误检测和日志输出 |

### BSP 层不应做什么

- 不包含业务逻辑
- 不处理协议解析（由 DRV 层负责）
- 不直接操作外部设备
- 不包含复杂的状态机
- **不负责外设的具体硬件配置**（由 CubeMX/HAL 负责）

### BSP 层与硬件配置的职责边界

| 配置内容 | 职责归属 | 原因 |
|----------|----------|------|
| 引脚模式（上拉/下拉/浮空） | **CubeMX/HAL** | 硬件相关 |
| 中断使能/优先级 | **CubeMX/HAL** | 硬件相关 |
| 引脚复用功能 | **CubeMX/HAL** | 硬件相关 |
| 时钟配置 | **CubeMX/HAL** | 硬件相关 |
| 实例注册与回调分发 | **BSP 层** | 软件抽象 |

---

## 二、类型封装原则

### 避免过度封装

**重要原则：只封装操作接口，不封装 HAL 类型**

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

| 需要封装 | 不需要封装 |
|----------|------------|
| 实例管理（注册/注销） | GPIO 端口类型（`GPIO_TypeDef*`） |
| 回调分发机制 | 引脚号宏（`GPIO_PIN_x`） |
| 操作接口（Toggle/Set/Reset/Read/Write） | 引脚状态（`GPIO_PinState`） |
| 错误检查和日志 | 中断模式（由 CubeMX 配置） |

---

## 三、实例注册机制

本项目采用**静态宏定义 + 注册函数**的架构：

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

### 代码模板

```c
// bsp_xxx.h - 实例结构体
typedef struct XXXInstance
{
    BoardXXX_e xxx_e;                          // 板载枚举（注册时用于查找映射）
    XXX_HandleTypeDef *handle;                 // 硬件句柄（注册时自动填充）
    void (*callback)(struct XXXInstance *);    // 回调函数
} XXXInstance;

// 实例定义宏
#define XXX_INSTANCE_DEF(name, xxx_idx, ...) \
    XXXInstance name = {                     \
        .xxx_e = xxx_idx,                    \
        .handle = NULL,                      \
        /* ... 其他成员初始化 */             \
    }
```

---

## 四、回调函数机制

### 使用场景

| 场景 | 触发时机 | 示例 |
|------|----------|------|
| 接收完成 | DMA/IDLE 中断收到数据 | USART RX 回调 |
| 发送完成 | IT/DMA 发送完成 | USART TX 回调（可选） |
| 错误恢复 | 通信错误（帧错误/溢出等） | UART Error 回调 |
| 外部中断 | GPIO 外部中断触发 | EXTI 回调 |

### HAL 回调分发实现

```c
void HAL_XXX_RxCpltCallback(XXX_HandleTypeDef *hxxx)
{
    for (uint8_t i = 0; i < s_idx; i++)
    {
        if (hxxx == s_xxx_instance[i]->handle)
        {
            s_xxx_instance[i]->rx_len = ...;
            if (s_xxx_instance[i]->callback != NULL)
            {
                s_xxx_instance[i]->callback(s_xxx_instance[i]);
            }
            return;
        }
    }
}
```

---

## 五、多工作模式支持

| 模式 | 特点 | 适用场景 |
|------|------|----------|
| 阻塞模式（Blocking） | 简单可靠 | 低速场景 |
| 中断模式（IT） | 非阻塞 | 中等速率 |
| DMA 模式 | 高效 | 高速大量数据传输 |

---

## 六、Cortex-M7 内存与 DMA 注意事项

### DMA 缓冲区位置要求

| 区域 | 地址 | DMA 支持 | 说明 |
|------|------|----------|------|
| DTCMRAM | 0x20000000 | **否** | 仅 CPU 可访问 |
| RAM_D1 | 0x24000000 | **是** | 推荐 DMA 缓冲区 |
| RAM_D2 | 0x30000000 | **是** | 外设 DMA 专用 |

```c
// DMA 缓冲区放入 RAM_D1
uint8_t usart_rx_buf[512] __attribute__((section(".ram_d1")));
```

---

## 七、数学加速模块（bsp_math）

项目中所有数学运算必须通过 `bsp_math` 接口进行：

| 函数 | 功能 | 说明 |
|------|------|------|
| `BSP_Math_Sin(theta)` | 计算 sin(theta) | theta 为弧度 |
| `BSP_Math_Cos(theta)` | 计算 cos(theta) | theta 为弧度 |
| `BSP_Math_SinCos(...)` | 同时计算 sin 和 cos | 比 separate 调用效率更高 |
| `BSP_Math_Atan2(y, x)` | 计算 atan2(y, x) | 返回弧度 |
| `BSP_Math_Sqrt(x)` | 计算 sqrt(x) | x >= 0 |

---

## 八、枚举命名与数量限制

**板载枚举使用功能命名，而非物理引脚命名。**

| 类型 | 示例 | 说明 |
|------|------|------|
| ✅ 功能命名 | `GPIO_LED_GREEN` | 表示功能（绿色 LED） |
| ❌ 物理命名 | `GPIO_PE14` | 表示物理引脚（与开发板耦合） |

**两种数量的区分：**

| 概念 | 说明 | 定义方式 |
|------|------|----------|
| 物理硬件数量 | MCU 上的物理外设数量 | 枚举 `_NUM_MAX` |
| 逻辑实例数量 | APP/DRV 层可注册的最大实例数 | 宏 `XXX_INSTANCE_NUM` |

---

_本文档适用于 test_my_frame 项目的 BSP 层开发。_
