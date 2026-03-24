# Cortex-M7 内存使用注意事项

**这个项目禁止使用d-cache,mpu,ramecc,主控内部flash,cpu分支预测**

## 前置说明：必须手动处理的问题

### 必须手动处理

| 问题                 | 原因                            | 解决方案                         |
| -------------------- | ------------------------------- | -------------------------------- |
| **DMA 缓冲区位置**   | DTCMRAM/ITCMRAM 不支持 DMA 访问 | 放到 RAM_D1 或 RAM_D2            |
| **指针强制类型转换** | 可能导致非对齐访问              | 避免 `(uint32_t*)&buf[1]` 等写法 |

### 编译器自动处理（无需关心）

| 问题             | 说明                                   |
| ---------------- | -------------------------------------- |
| **结构体对齐**   | 编译器默认按自然对齐排列成员，自动填充 |
| **栈对齐**       | 编译器和链接脚本已保证 SP 8 字节对齐   |
| **全局变量对齐** | 编译器自动按类型对齐放置               |

---

## 一、内存区域概览

STM32H723 拥有多样化的内存架构，不同区域具有不同的特性和用途：

| 区域        | 地址       | 大小  | 总线  | DMA 支持 | 典型用途                 |
| ----------- | ---------- | ----- | ----- | -------- | ------------------------ |
| **DTCMRAM** | 0x20000000 | 128K  | D-Bus | 否       | 默认数据段、栈、堆       |
| **RAM_D1**  | 0x24000000 | 320K  | AXI   | 是       | DMA 缓冲区、大数组       |
| **RAM_D2**  | 0x30000000 | 32K   | AHB   | 是       | 外设 DMA（ADC/DAC/串口） |
| **RAM_D3**  | 0x38000000 | 16K   | AHB   | 是       | 低功耗域数据             |
| **ITCMRAM** | 0x00000000 | 64K   | I-Bus | 否       | 关键代码执行加速         |
| **FLASH**   | 0x08000000 | 1024K | -     | -        | 程序代码、常量           |

---

## 二、默认行为

未使用 `__attribute__` 指定的变量，默认存放在 **DTCMRAM**：

| 变量类型              | 默认位置 |
| --------------------- | -------- |
| 已初始化全局/静态变量 | DTCMRAM  |
| 未初始化全局/静态变量 | DTCMRAM  |
| 局部变量（栈上）      | DTCMRAM  |
| 动态分配内存（堆）    | DTCMRAM  |

---

## 三、自定义段使用方法

### 3.1 语法格式

```c
__attribute__((section(".段名"))) 变量声明;
```

### 3.2 各段使用示例

#### RAM_D1（DMA 缓冲区首选）

```c
/* DMA 缓冲区 - 支持 DMA 传输 */
uint8_t usart_dma_tx_buf[1024] __attribute__((section(".ram_d1")));
uint8_t usart_dma_rx_buf[1024] __attribute__((section(".ram_d1")));

/* 大数组 */
float kalman_matrix[100][100] __attribute__((section(".ram_d1")));

/* 已初始化变量（启动时会自动拷贝） */
int lookup_table[256] __attribute__((section(".ram_d1"))) = { /* ... */ };
```

#### RAM_D2（外设 DMA）

```c
/* ADC DMA 缓冲区 */
uint16_t adc_buffer[1024] __attribute__((section(".ram_d2")));

/* DAC 输出缓冲区 */
uint16_t dac_waveform[512] __attribute__((section(".ram_d2")));
```

#### RAM_D3（低功耗场景）

```c
/* 低功耗模式保留数据 */
uint8_t backup_config[128] __attribute__((section(".ram_d3")));
```

#### ITCMRAM（代码加速）

```c
/* 关键函数 - 放到 ITCM 加速执行 */
void critical_pid_control(void) __attribute__((section(".itcmram")));

/* 高频中断处理函数 */
void TIM2_IRQHandler(void) __attribute__((section(".itcmram")));
```

---

## 四、DMA 注意事项

### 4.1 DMA 可访问的内存区域

| 区域    | DMA 支持 | 说明                |
| ------- | -------- | ------------------- |
| DTCMRAM | **否**   | 仅 CPU 可访问       |
| RAM_D1  | **是**   | 推荐 DMA 缓冲区位置 |
| RAM_D2  | **是**   | 外设 DMA 专用       |
| RAM_D3  | **是**   | 支持 DMA            |
| ITCMRAM | **否**   | 仅 CPU 可访问       |

### 4.2 常见问题

**问题**：DMA 传输数据全为 0 或异常

**原因**：缓冲区放在了 DTCMRAM 或 ITCMRAM

**解决**：将 DMA 缓冲区放到 `.ram_d1` 或 `.ram_d2`

```c
/* 错误示例 - DMA 无法访问 */
uint8_t dma_buf[256];  /* 默认在 DTCMRAM */

/* 正确示例 */
uint8_t dma_buf[256] __attribute__((section(".ram_d1")));
```

---

## 五、缓存一致性

Cortex-M7 具有 D-Cache，DMA 与 CPU 共享内存时需要处理缓存一致性问题。

### 5.1 问题原因

```
CPU 写数据 → D-Cache（未写入内存）→ DMA 读取内存（数据过期）
DMA 写数据 → 内存 → CPU 读取 D-Cache（数据过期）
```

### 5.2 解决方案：禁用 D-Cache

本项目统一采用禁用 D-Cache 的方式解决缓存一致性问题，简单可靠。

```c
/* 在 SystemInit 中禁用 D-Cache */
void SystemInit(void)
{
    /* ... 其他初始化 ... */
    SCB_DisableDCache();  /* 禁用数据缓存 */
}
```

**说明**：

- 禁用 D-Cache 后，CPU 直接访问内存，与 DMA 数据一致
- 性能有一定损失，但避免了复杂的缓存维护
- 对于嵌入式控制应用，影响可接受

---

## 六、内存屏障指令

### 6.1 指令说明

| 指令  | 作用         | 使用场景                         |
| ----- | ------------ | -------------------------------- |
| `DMB` | 数据内存屏障 | 确保内存访问顺序                 |
| `DSB` | 数据同步屏障 | 等待所有内存访问完成             |
| `ISB` | 指令同步屏障 | 刷新流水线，确保后续指令重新读取 |

### 6.2 使用示例

```c
/* 临界区保护 */
__disable_irq();
/* 临界区代码 */
__DMB();  /* 确保临界区内存操作完成 */
__enable_irq();

/* 修改中断向量表后 */
SCB->VTOR = 0x20000000;
__DSB();  /* 确保写入完成 */
__ISB();  /* 刷新流水线 */

/* 自定义内存屏障宏 */
#define MEM_BARRIER()  __DMB()
#define DATA_SYNC()    __DSB()
#define INST_SYNC()    __ISB()
```

---

## 七、数据对齐

### 7.1 什么是对齐

**对齐**是指数据的起始地址是其大小的整数倍：

| 数据类型 | 大小   | 对齐要求   | 有效地址示例        |
| -------- | ------ | ---------- | ------------------- |
| uint8_t  | 1 字节 | 1 字节对齐 | 任意地址            |
| uint16_t | 2 字节 | 2 字节对齐 | 0x00, 0x02, 0x04... |
| uint32_t | 4 字节 | 4 字节对齐 | 0x00, 0x04, 0x08... |
| float    | 4 字节 | 4 字节对齐 | 0x00, 0x04, 0x08... |
| double   | 8 字节 | 8 字节对齐 | 0x00, 0x08, 0x10... |

### 7.2 为什么会产生非对齐

**常见误解**：单独定义的变量会自动放在对齐的地址上。

**实际情况**：uint8_t 只需要 1 字节对齐，可以放在任意地址。问题出现在以下场景：

#### 场景一：连续定义的变量

```c
uint8_t  a;     /* 地址 0x20000000 */
uint8_t  b;     /* 地址 0x20000001 */
uint8_t  c;     /* 地址 0x20000002 */
uint32_t d;     /* 地址 0x20000004（编译器自动填充，对齐到4） */
```

编译器会在 `c` 和 `d` 之间插入 1 字节填充，使 `d` 对齐。

#### 场景二：结构体紧凑排列

```c
typedef struct {
    uint8_t  a;    /* 偏移 0，大小 1 */
    uint32_t b;    /* 偏移 ??? */
    uint8_t  c;    /* 偏移 ??? */
} MyStruct;
```

- **不使用 packed**：编译器自动填充，`b` 偏移 4，结构体大小 12
- **使用 packed**：`b` 偏移 1（非对齐！），结构体大小 6

#### 场景三：指针强制转换（最危险）

```c
uint8_t buffer[100];        /* 起始地址可能是任意值 */

uint32_t *ptr = (uint32_t*)&buffer[1];  /* 非对齐访问！ */
uint32_t value = *ptr;       /* 可能触发异常或性能下降 */

/* 正确做法：确保对齐 */
uint32_t *ptr = (uint32_t*)&buffer[0];  /* 如果 buffer 对齐到4 */
/* 或使用 memcpy */
uint32_t value;
memcpy(&value, &buffer[1], sizeof(uint32_t));  /* 安全，编译器处理对齐 */
```

#### 场景四：数组元素访问

```c
/* 数组首地址对齐，元素自然对齐 */
uint32_t arr[10];    /* 首地址 4 字节对齐 */
arr[0];              /* 地址对齐 */
arr[1];              /* 地址 +4，对齐 */

/* 但如果首地址不对齐 */
uint8_t buffer[100];
uint32_t *arr = (uint32_t*)buffer;  /* 如果 buffer 地址不是4的倍数 */
arr[0];              /* 非对齐访问！ */
```

### 7.3 非对齐访问的后果

| 后果     | 说明                           |
| -------- | ------------------------------ |
| 性能下降 | CPU 需要多次内存访问           |
| 数据错误 | 某些架构可能读取错误数据       |
| 硬件异常 | 启用 UFSR.UNALIGNED 时触发异常 |

### 7.4 结构体对齐最佳实践

```c
/* 方法一：让编译器自动处理（推荐） */
typedef struct {
    uint32_t b;    /* 偏移 0，4 字节 */
    uint8_t  a;    /* 偏移 4，1 字节 */
    uint8_t  c;    /* 偏移 5，1 字节 */
    /* 编译器自动填充 2 字节 */
} GoodStruct;      /* sizeof = 8 */

/* 方法二：显式填充（便于理解） */
typedef struct {
    uint8_t  a;    /* 偏移 0 */
    uint8_t  pad[3]; /* 填充 3 字节 */
    uint32_t b;    /* 偏移 4，对齐 */
    uint8_t  c;    /* 偏移 8 */
    uint8_t  pad2[3]; /* 填充到结构体大小为4的倍数 */
} ExplicitStruct;  /* sizeof = 12 */

/* 避免：紧凑结构体（除非必须节省内存） */
typedef struct {
    uint8_t  a;    /* 偏移 0 */
    uint32_t b;    /* 偏移 1，非对齐！ */
    uint8_t  c;    /* 偏移 5 */
} __attribute__((packed)) PackedStruct;  /* sizeof = 6，访问 b 非对齐 */
```

### 7.5 强制对齐

```c
/* 单个变量强制对齐 */
uint8_t buffer[256] __attribute__((aligned(4)));  /* 4 字节对齐 */

/* DMA 缓冲区对齐到缓存行 */
uint8_t dma_buf[1024] __attribute__((section(".ram_d1"), aligned(32)));

/* 结构体强制对齐 */
typedef struct {
    uint32_t data[4];
} __attribute__((aligned(16))) AlignedStruct;
```

---

## 八、栈对齐

### 8.1 为什么需要栈对齐

AAPCS（ARM 架构过程调用标准）要求栈指针 **8 字节对齐**，原因如下：

#### 原因一：函数调用约定

```asm
/* 函数调用时，编译器可能使用以下指令 */
STMFD   SP!, {R4-R11}    ; 保存8个寄存器，SP减少32（8的倍数）
LDMFD   SP!, {R4-R11}    ; 恢复寄存器
```

如果 SP 不是 8 字节对齐，某些指令可能异常或性能下降。

#### 原因二：64 位数据访问

```c
void func(void)
{
    double d = 1.0;     /* 8 字节，需要 8 字节对齐 */
    /* 如果 SP 不对齐，d 的地址可能非 8 的倍数 */
}
```

#### 原因三：调试器要求

调试器在查看调用栈时，假设 SP 是 8 字节对齐的。

### 8.2 栈上的变量不一定在 8 的倍数地址

**重要区分**：

- **栈指针 SP**：必须 8 字节对齐
- **栈上的局部变量**：根据自身类型对齐，不一定是 8 的倍数

```c
void func(void)
{
    uint8_t  a;     /* 可能在 SP+0（SP是8的倍数，但a不是） */
    uint8_t  b;     /* 可能在 SP+1 */
    uint8_t  c;     /* 可能在 SP+2 */
    uint32_t d;     /* 可能在 SP+4（4字节对齐） */
    double   e;     /* 可能在 SP+8（8字节对齐） */
}
```

编译器会在栈帧中合理安排变量位置，确保每个变量按自身要求对齐。

### 8.3 FreeRTOS 任务栈

```c
/* 任务栈数组必须 8 字节对齐 */
static StackType_t task1_stack[128] __attribute__((aligned(8)));

/* 创建任务 */
xTaskCreateStatic(task1_func, "Task1", 128, NULL, 1, task1_stack, &task1_tcb);
```

`StackType_t` 在 Cortex-M7 上是 `uint32_t`，数组本身 4 字节对齐，但需要确保 8 字节对齐以满足 AAPCS。

### 8.4 主栈配置

链接脚本中确保栈 8 字节对齐：

```ld
._user_heap_stack :
{
    . = ALIGN(8);  /* 8 字节对齐 */
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
} >DTCMRAM
```

### 8.5 违反栈对齐的后果

| 后果         | 说明                       |
| ------------ | -------------------------- |
| 函数调用异常 | 某些指令要求 SP 对齐       |
| 浮点运算错误 | double 类型需要 8 字节对齐 |
| 调试困难     | 调用栈回溯失败             |

---

## 九、中断与共享变量

### 9.1 中断中使用 DMA 缓冲区

已禁用 D-Cache，DMA 与 CPU 数据直接一致，无需额外处理：

```c
/* DMA 接收完成中断 */
void DMA1_Stream0_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&hdma, DMA_FLAG_TCIF0_4))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma, DMA_FLAG_TCIF0_4);

        /* 直接处理接收数据，无需缓存维护 */
        process_rx_data(rx_buffer, rx_size);
    }
}
```

### 9.2 中断与主循环共享变量

```c
/* 共享变量声明 */
volatile uint32_t shared_flag;

/* 中断中设置 */
void TIM2_IRQHandler(void)
{
    shared_flag = 1;
    __DMB();  /* 确保写入完成 */
}

/* 主循环中检查 */
while (1)
{
    __DMB();  /* 确保读取最新值 */
    if (shared_flag)
    {
        shared_flag = 0;
        /* 处理事件 */
    }
}
```

**volatile 关键字**：防止编译器优化，确保每次都从内存读取。

````

---

## 十、性能优化建议

### 10.1 内存分配策略

| 数据类型       | 推荐位置        | 原因                  |
| -------------- | --------------- | --------------------- |
| DMA 缓冲区     | RAM_D1 / RAM_D2 | DMA 可访问            |
| 大数组/缓冲区  | RAM_D1          | 空间大，DMA 支持      |
| 高频执行代码   | ITCMRAM         | 零等待执行            |
| 临界变量       | DTCMRAM         | CPU 专用，无 DMA 冲突 |
| 低功耗保留数据 | RAM_D3          | 低功耗域供电          |

### 10.2 性能对比

| 内存区域 | 访问延迟 | 适用场景 |
| -------- | -------- | -------- |
| ITCMRAM  | 0 等待   | 关键代码 |
| DTCMRAM  | 0 等待   | 临界数据 |
| RAM_D1   | 1-2 等待 | 大缓冲区 |
| RAM_D2   | 1-2 等待 | 外设 DMA |

---

## 十一、编译选项

### 11.1 Cortex-M7 专用选项

```makefile
# CPU 和 FPU
CFLAGS += -mcpu=cortex-m7
CFLAGS += -mfpu=fpv5-d16        # 硬件 FPU（双精度）
CFLAGS += -mfloat-abi=hard      # 硬件浮点 ABI
CFLAGS += -mthumb

# 优化级别
CFLAGS += -O2                    # 推荐：平衡速度和大小
# CFLAGS += -O3                  # 最大速度优化
# CFLAGS += -Os                  # 最小大小优化

# 对齐选项
CFLAGS += -mno-unaligned-access  # 禁止非对齐访问（提高兼容性）

# 其他推荐选项
CFLAGS += -fomit-frame-pointer   # 省略帧指针（减小代码）
CFLAGS += -ffunction-sections    # 每个函数独立段
CFLAGS += -fdata-sections        # 每个数据独立段
````

### 11.2 LTO 链接时优化

```makefile
CFLAGS += -flto                  # 链接时优化
LDFLAGS += -flto
```

---

## 十二、完整示例

### 12.1 头文件定义

```c
/* memory_sections.h */
#ifndef MEMORY_SECTIONS_H
#define MEMORY_SECTIONS_H

/* RAM_D1 - DMA 缓冲区 */
#define RAM_D1 __attribute__((section(".ram_d1")))

/* RAM_D2 - 外设 DMA */
#define RAM_D2 __attribute__((section(".ram_d2")))

/* RAM_D3 - 低功耗域 */
#define RAM_D3 __attribute__((section(".ram_d3")))

/* ITCMRAM - 代码加速 */
#define ITCMRAM __attribute__((section(".itcmram")))

/* 对齐宏 */
#define ALIGN_4  __attribute__((aligned(4)))
#define ALIGN_32 __attribute__((aligned(32)))

#endif /* MEMORY_SECTIONS_H */
```

### 12.2 使用示例

```c
#include "memory_sections.h"

/* DMA 缓冲区 - 放到 RAM_D1 */
uint8_t usart_tx_buf[512] RAM_D1 ALIGN_4;
uint8_t usart_rx_buf[512] RAM_D1 ALIGN_4;

/* 发送数据（已禁用 D-Cache，无需缓存维护） */
void usart_send(uint8_t *data, uint32_t len)
{
    memcpy(usart_tx_buf, data, len);
    HAL_UART_Transmit_DMA(&huart, usart_tx_buf, len);
}

/* 接收完成回调（已禁用 D-Cache，无需缓存维护） */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    process_data(usart_rx_buf);
}
```

---

## 十三、注意事项清单

| 问题           | 解决方案                             |
| -------------- | ------------------------------------ |
| DMA 缓冲区位置 | 放到 RAM_D1 或 RAM_D2                |
| 缓存一致性     | 禁用 D-Cache                         |
| 内存访问顺序   | DMB/DSB/ISB                          |
| 数据对齐       | 结构体自然对齐，避免强制指针类型转换 |
| 栈对齐         | SP 必须 8 字节对齐                   |
| 中断共享变量   | volatile + 内存屏障                  |

---

## 十四、相关文件

| 文件     | 路径                                | 说明             |
| -------- | ----------------------------------- | ---------------- |
| 链接脚本 | `hal/DM_MC02/STM32H723XG_FLASH.ld`  | 内存区域和段定义 |
| 启动文件 | `hal/DM_MC02/startup_stm32h723xx.s` | 数据段初始化代码 |
