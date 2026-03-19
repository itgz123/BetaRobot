# 架构调整待办清单

## 新架构说明

- **分层结构**：app → drv → bsp → hal
- **HAL层**：新建 `hal/` 目录，包含多种开发板配置（当前只需一种）
- **实例管理**：APP层使用DEF宏定义实例 + BSP层注册函数
- **DEF宏定义位置**：DEF宏定义在底层（BSP层），APP层调用宏创建实例
- **DRV访问BSP**：DRV结构体直接包含BSP结构体（值拷贝，非指针）
- **内存分配**：静态分配，不使用malloc
- **配置传递**：上层配置驱动下层，下层不依赖上层配置文件

## 配置文件职责

| 文件          | 位置       | 职责说明                                     |
| ------------- | ---------- | -------------------------------------------- |
| `app_cfg.h`   | `app/Inc/` | 开发板选择、应用参数（任务频率、底盘参数等） |
| `bsp_cfg.h`   | `bsp/Inc/` | 板载资源枚举、硬件映射、实例数量配置         |
| `drv_types.h` | `drv/`     | 跨层通用类型定义（不涉及硬件配置）           |

**切换开发板**：只需修改 `app/Inc/app_cfg.h` 中的 `DEVELOPMENT_BOARD` 宏

---

## 一、分层架构问题（高优先级）

### 1. APP层直接调用HAL层（违规）✅ 已解决

| 文件位置                    | 问题描述                                                              | 状态      |
| --------------------------- | --------------------------------------------------------------------- | --------- |
| `app/Src/app_motor.c:2`     | 包含 `#include "tim.h"` - HAL层头文件                                 | ✅ 已修复 |
| `app/Src/app_motor.c:21-30` | 直接使用HAL宏定义(`GPIOB`, `GPIO_PIN_12`等)和句柄(`&htim1`, `&htim2`) | ✅ 已修复 |
| `app/Src/app_cmd.c:2`       | 包含 `#include "usart.h"` - HAL层头文件                               | ✅ 已修复 |
| `app/Src/app_cmd.c:31`      | 直接使用 `&huart2` - HAL层串口句柄                                    | ✅ 已修复 |
| `app/Inc/app_robot.h:7`     | 包含 `#include "main.h"` - HAL层头文件                                | ✅ 已修复 |

**修改方案**：

- ✅ 新建 `bsp/Inc/bsp_cfg.h` 和 `bsp/Src/bsp_cfg.c` - 板级映射和配置
- ✅ 移动 `app/app_cfg.h` 到 `app/Inc/app_cfg.h`
- ✅ 在 `bsp_cfg.h` 定义板载资源枚举（`BoardGPIO_e`, `BoardTIM_e`, `BoardUSART_e`）
- ✅ 修改 `drv_dcmotor.h` 配置结构体使用枚举索引
- ✅ 修改 `drv_sbus.h` 配置结构体使用枚举索引
- ✅ APP层通过枚举索引配置外设，不再直接使用HAL类型

---

## 二、HAL层创建（高优先级）✅ 已调整

### 2. 板级映射已合并到 bsp_cfg

**原计划**：新建 `hal/` 目录

**实际实现**：将板级映射合并到 `bsp_cfg.h/c` 中，简化结构

```
bsp/
├── Inc/
│   └── bsp_cfg.h          # 板载资源枚举 + 硬件映射 + 实例数量配置
└── Src/
    └── bsp_cfg.c          # 硬件映射数组实现（按开发板条件编译）
```

**已完成事项**：

- ✅ `bsp/Inc/bsp_cfg.h` - 定义板载资源枚举、硬件映射接口
- ✅ `bsp/Src/bsp_cfg.c` - 按开发板条件编译的硬件映射数组

**切换开发板**：只需修改 `app/Inc/app_cfg.h` 中的 `DEVELOPMENT_BOARD` 宏

---

## 三、实例管理方式重构（高优先级）

### 3. BSP层改用静态数组 + 注册机制

**当前问题**（使用动态分配）：

| 文件位置                 | 问题代码                              |
| ------------------------ | ------------------------------------- |
| `bsp/Src/bsp_gpio.c:63`  | `bsp_malloc(sizeof(GPIOInstance))`    |
| `bsp/Src/bsp_tim.c:52`   | `bsp_malloc(sizeof(PWMInstance))`     |
| `bsp/Src/bsp_tim.c:103`  | `bsp_malloc(sizeof(EncoderInstance))` |
| `bsp/Src/bsp_usart.c:70` | `bsp_malloc(sizeof(USARTInstance))`   |
| `drv/Src/drv_sbus.c:42`  | `bsp_malloc(sizeof(SBUSInstance))`    |

**修改方案**：

```c
// 新的BSP层实现方式（以GPIO为例）
// bsp_gpio.c

// 静态数组存储实例
static GPIOInstance s_gpio_instances[GPIO_NUM];
static uint8_t s_gpio_cnt = 0;

// 注册函数：将APP层定义的实例注册到数组
bool GPIORegister(GPIOInstance *instance)
{
    if (s_gpio_cnt >= GPIO_NUM) {
        return false;  // 超出数量限制
    }
    s_gpio_instances[s_gpio_cnt++] = *instance;  // 值拷贝
    return true;
}
```

**待办事项**：

- [ ] `bsp/Src/bsp_gpio.c` - 移除bsp_malloc，改用静态数组
- [ ] `bsp/Src/bsp_tim.c` - 移除bsp_malloc，改用静态数组
- [ ] `bsp/Src/bsp_usart.c` - 移除bsp_malloc，改用静态数组
- [ ] `bsp/Src/bsp_stdlib.c` - 移除动态内存分配函数（或保留但不使用）
- [ ] `drv/Src/drv_sbus.c` - 同样改用静态分配

### 4. 添加板级映射

**目标结构**：

```c
// bsp_gpio.c - 板载接口映射
static const struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} gpio_map[] = {
    [MOTOR_LF_IN1] = {GPIOB, GPIO_PIN_12},
    [MOTOR_LF_IN2] = {GPIOB, GPIO_PIN_13},
    // ...
};
```

**待办事项**：

- [ ] BSP层添加 `gpio_map[]` 数组
- [ ] BSP层添加 `usart_map[]` 数组
- [ ] BSP层添加 `tim_map[]` 数组
- [ ] 定义枚举索引（如 `MOTOR_LF_IN1`, `DEBUG_USART` 等）

### 5. DRV层结构体修改（值拷贝而非指针）

**当前问题**：

```c
// drv_dcmotor.h - 当前使用指针
typedef struct DCMotorInstance
{
    GPIOInstance *gpio_in1;   // ❌ 指针
    GPIOInstance *gpio_in2;   // ❌ 指针
    PWMInstance *pwm;         // ❌ 指针
    EncoderInstance *encoder; // ❌ 指针
} DCMotorInstance;
```

**修改为**：

```c
// drv_dcmotor.h - 改为值拷贝
typedef struct DCMotorInstance
{
    GPIOInstance gpio_in1;    // ✅ 直接包含结构体
    GPIOInstance gpio_in2;    // ✅ 直接包含结构体
    PWMInstance pwm;          // ✅ 直接包含结构体
    EncoderInstance encoder;  // ✅ 直接包含结构体
} DCMotorInstance;
```

**待办事项**：

- [ ] `drv/Inc/drv_dcmotor.h` - DCMotorInstance改为值拷贝BSP结构体
- [ ] 其他DRV结构体同步修改
- [ ] 更新DRV层初始化函数

---

## 四、DEF宏实现（中优先级）

### 6. 创建DEF宏和注册机制

**设计原则**：

- **DEF宏定义在底层（BSP层）**：宏的定义放在BSP层头文件中
- **APP层调用宏创建实例**：APP层使用宏来声明和初始化实例
- **注册到BSP层**：APP层调用注册函数将实例注册到BSP层数组

**BSP层定义DEF宏**（bsp_gpio.h）：

```c
// bsp_gpio.h - DEF宏定义在BSP层

#define DEF_GPIO(name, port, pin)              \
    static GPIOInstance name = {               \
        .GPIOx = port,                         \
        .GPIO_Pin = pin,                       \
    };

// 注册函数声明
bool GPIORegister(GPIOInstance *instance);
```

**APP层使用DEF宏**（app_motor.c）：

```c
// app_motor.c - APP层调用宏创建实例

#include "bsp_gpio.h"

// 使用DEF宏定义实例
DEF_GPIO(motor_in1, GPIOB, GPIO_PIN_12);
DEF_GPIO(motor_in2, GPIOB, GPIO_PIN_13);

void MotorInit(void)
{
    // 注册到BSP层
    GPIORegister(&motor_in1);
    GPIORegister(&motor_in2);
}
```

**待办事项**：

- [ ] 在 `bsp/Inc/bsp_gpio.h` 中定义DEF_GPIO宏
- [ ] 在 `bsp/Inc/bsp_tim.h` 中定义DEF_PWM/DEF_ENCODER宏
- [ ] 在 `bsp/Inc/bsp_usart.h` 中定义DEF_USART宏
- [ ] BSP层提供注册函数接口
- [ ] APP层使用DEF宏定义实例并注册

---

## 五、FreeRTOS静态分配（中优先级）

### 7. 任务创建不符合静态分配要求

**问题文件**：`app/Src/app_robot.c:33-62`

**当前问题**：

```c
// ❌ 当前实现 - 缺少静态内存分配
const osThreadAttr_t motor_Task_attributes = {
    .name = "motor_Task",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)30,
};
MotorTaskHandle = osThreadNew(StartMOTORTASK, NULL, &motor_Task_attributes);
```

**修改为**：

```c
// ✅ 正确实现 - 提供静态内存
static uint32_t motor_Task_Buffer[128];
static osStaticThreadDef_t motor_Task_ControlBlock;
const osThreadAttr_t motor_Task_attributes = {
    .name = "motor_Task",
    .cb_mem = &motor_Task_ControlBlock,
    .cb_size = sizeof(motor_Task_ControlBlock),
    .stack_mem = motor_Task_Buffer,
    .stack_size = sizeof(motor_Task_Buffer),
    .priority = (osPriority_t)30,
};
```

**待办事项**：

- [ ] `app/Src/app_robot.c` - motor_Task 改为静态分配
- [ ] `app/Src/app_robot.c` - 其他任务同步修改

### 8. 队列创建不符合静态分配要求

**问题文件**：`app/Src/app_robot.c:72-80`

**待办事项**：

- [ ] `app/Src/app_robot.c` - chassis_cmd_queue 改为静态分配
- [ ] 为队列提供 `cb_mem` 和 `mq_mem`

---

## 六、配置检查（已符合要求）

| 配置项                             | 当前值 | 要求值 | 状态 |
| ---------------------------------- | ------ | ------ | ---- |
| `configSUPPORT_STATIC_ALLOCATION`  | 1      | 1      | ✅   |
| `configSUPPORT_DYNAMIC_ALLOCATION` | 0      | 0      | ✅   |

---

## 七、修改优先级排序

### 高优先级（架构核心）

1. 创建HAL层目录结构
2. 添加板级映射（gpio_map等）
3. BSP层改用静态数组
4. 修复APP层违规调用HAL层

### 中优先级（实例管理）

5. DRV层结构体改为值拷贝
6. 创建DEF宏和注册机制
7. FreeRTOS任务/队列静态分配

### 低优先级（清理）

8. 移除动态内存分配相关代码
9. 更新文档

---

## 八、注意事项

1. **CubeMX重新生成后**：heap_1.c ~ heap_5.c 可能被自动删除，需确认Makefile不包含这些文件
2. **多开发板支持**：当前只需实现一种开发板，其他预留空实现
3. **实例数量限制**：`bsp_cfg.h` 中定义的实例数量需要与实际使用数量匹配
4. **配置传递方向**：上层配置驱动下层，下层不依赖上层配置文件
