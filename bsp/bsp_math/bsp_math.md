# bsp_math 使用规范

## 概述

`bsp_math` 是项目的统一数学函数封装模块，**项目中所有数学运算必须使用 bsp_math**，禁止直接使用 `<math.h>` 或其他数学库。

## 三层回退机制

```
┌─────────────────────────────────────────────────────────────┐
│  第一层：硬件计算模块（CORDIC/FMAC）                         │
│  - 仅 STM32H7/G4 系列支持                                   │
│  - 通过 HAS_CORDIC 宏判断                                   │
│  - 当前开发板（STM32F407）无此硬件，跳过此层                 │
└─────────────────────────────────────────────────────────────┘
                          ↓ 不可用时
┌─────────────────────────────────────────────────────────────┐
│  第二层：CMSIS-DSP 库                                        │
│  - arm_sin_f32/arm_cos_f32：512项查找表实现                 │
│  - arm_sqrt_f32：内联函数，使用 FPU VSQRT 指令              │
│  - arm_atan2_f32：优化算法实现                              │
│  - 通过 HAS_DSP 宏判断                                      │
└─────────────────────────────────────────────────────────────┘
                          ↓ 不可用时
┌─────────────────────────────────────────────────────────────┐
│  第三层：标准库 math.h                                       │
│  - sinf(), cosf(), sqrtf(), atan2f()                       │
│  - 兜底实现，始终可用                                        │
└─────────────────────────────────────────────────────────────┘
```

## 使用规则

### 1. 禁止直接包含 math.h

```c
// ❌ 错误：禁止直接使用 math.h
#include <math.h>
float x = sinf(theta);
float y = fabsf(value);

// ✅ 正确：使用 bsp_math.h
#include "bsp_math.h"
float x = BSP_Math_Sin(theta);
float y = MATH_FABS(value);
```

### 2. 简单运算优先使用宏

| 操作       | 宏                        | 说明              |
| ---------- | ------------------------- | ----------------- |
| 绝对值     | `MATH_FABS(x)`            | 浮点绝对值        |
| 平方       | `SQ(x)`                   | x \* x            |
| 限制范围   | `MATH_CLAMP(x, min, max)` | 限制在 [min, max] |
| 符号函数   | `MATH_SIGN(x)`            | 返回 1/-1/0       |
| 最大值     | `MAX(a, b)`               | -                 |
| 最小值     | `MIN(a, b)`               | -                 |
| 角度转弧度 | `DEG_TO_RAD(deg)`         | -                 |
| 弧度转角度 | `RAD_TO_DEG(rad)`         | -                 |

### 3. 数学函数使用 inline 函数

| 函数                             | 说明                |
| -------------------------------- | ------------------- |
| `BSP_Math_Sin(theta)`            | 正弦函数            |
| `BSP_Math_Cos(theta)`            | 余弦函数            |
| `BSP_Math_SinCos(theta, &s, &c)` | 同时计算 sin 和 cos |
| `BSP_Math_Atan2(y, x)`           | 反正切函数          |
| `BSP_Math_Sqrt(x)`               | 平方根              |
| `BSP_Math_Fabs(x)`               | 绝对值（函数形式）  |

### 4. 数学常量

| 常量      | 值         | 说明 |
| --------- | ---------- | ---- |
| `M_PI`    | 3.14159... | π    |
| `M_PI_2`  | 1.57079... | π/2  |
| `M_2PI`   | 6.28318... | 2π   |
| `M_E`     | 2.71828... | e    |
| `M_SQRT2` | 1.41421... | √2   |
| `M_SQRT3` | 1.73205... | √3   |

## 实现原理

### inline 函数设计

所有数学函数在 `bsp_math.h` 中使用 `static inline` 实现，编译时会内联展开，无函数调用开销：

```c
static inline float BSP_Math_Sin(float theta)
{
#if HAS_DSP
    return arm_sin_f32(theta);  // CMSIS-DSP 查表实现
#else
    return sinf(theta);         // 标准库兜底
#endif
}
```

### 性能说明

- **CMSIS-DSP sin/cos**：使用 512 项查找表，比标准库快约 3-5 倍
- **CMSIS-DSP sqrt**：使用 FPU 的 VSQRT 指令，单周期完成
- **inline 函数**：无函数调用开销，直接展开为底层实现

## 文件结构

```
bsp/bsp_math/
├── bsp_math.h    # 头文件：常量、宏、inline 函数
├── bsp_math.c    # 源文件：初始化、CRC 函数
└── bsp_math.md   # 本文档
```

## 初始化

在系统启动时调用 `BSP_Math_Init()` 进行初始化（已在 `app_robot.c` 的 `function_in_main_c()` 中调用）：

```c
void function_in_main_c(void)
{
    __disable_irq();
    DWT_Init();
    BSPLogInit();
    BSP_Math_Init();  // 数学模块初始化
    __enable_irq();
    // ...
}
```
