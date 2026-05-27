# STM32H723 RAM 配置说明

本文档说明如何在 STM32H723 上配置多种 RAM 区域，包括 DMA RAM 和 ITCM RAM。

---

## 一、硬件内存布局

| 区域               | 地址范围                | 大小  | 用途                       |
| ------------------ | ----------------------- | ----- | -------------------------- |
| ITCMRAM            | 0x00000000 - 0x0000FFFF | 64KB  | 指令紧耦合内存，零等待执行 |
| DTCMRAM            | 0x20000000 - 0x2001FFFF | 128KB | 数据紧耦合内存，栈/堆      |
| RAM_D1 (AXI SRAM)  | 0x24000000 - 0x2404FFFF | 320KB | DMA 可访问，大缓冲区       |
| RAM_D2 (SRAM1/2/3) | 0x30000000 - 0x30007FFF | 32KB  | 外设 DMA                   |
| RAM_D3 (SRAM4)     | 0x38000000 - 0x38003FFF | 16KB  | 低功耗域                   |

---

## 二、修改的文件

### 2.1 链接脚本 `STM32H723XG_FLASH.ld`

**位置**：`cubemx/board_config/DM_MC02/STM32H723XG_FLASH.ld`

**添加内容**（在 `.bss` 段之后）：

```ld
/* RAM_D1 section - DMA accessible, 320KB AXI SRAM */
.ram_d1 :
{
  . = ALIGN(4);
  _sram_d1 = .;
  *(.ram_d1)
  *(.ram_d1*)
  . = ALIGN(4);
  _eram_d1 = .;
} >RAM_D1 AT> FLASH
_siram_d1 = LOADADDR(.ram_d1);

/* BSS for RAM_D1 - zero-initialized DMA buffers */
.bss_ram_d1 (NOLOAD) :
{
  . = ALIGN(4);
  _sbss_ram_d1 = .;
  *(.bss_ram_d1)
  *(.bss_ram_d1*)
  . = ALIGN(4);
  _ebss_ram_d1 = .;
} >RAM_D1

/* RAM_D2 section - Peripheral DMA, 32KB SRAM1/2/3 */
.ram_d2 :
{
  . = ALIGN(4);
  _sram_d2 = .;
  *(.ram_d2)
  *(.ram_d2*)
  . = ALIGN(4);
  _eram_d2 = .;
} >RAM_D2 AT> FLASH
_siram_d2 = LOADADDR(.ram_d2);

/* BSS for RAM_D2 */
.bss_ram_d2 (NOLOAD) :
{
  . = ALIGN(4);
  _sbss_ram_d2 = .;
  *(.bss_ram_d2)
  *(.bss_ram_d2*)
  . = ALIGN(4);
  _ebss_ram_d2 = .;
} >RAM_D2

/* RAM_D3 section - Low power domain, 16KB SRAM4 */
.ram_d3 :
{
  . = ALIGN(4);
  _sram_d3 = .;
  *(.ram_d3)
  *(.ram_d3*)
  . = ALIGN(4);
  _eram_d3 = .;
} >RAM_D3 AT> FLASH
_siram_d3 = LOADADDR(.ram_d3);

/* BSS for RAM_D3 */
.bss_ram_d3 (NOLOAD) :
{
  . = ALIGN(4);
  _sbss_ram_d3 = .;
  *(.bss_ram_d3)
  *(.bss_ram_d3*)
  . = ALIGN(4);
  _ebss_ram_d3 = .;
} >RAM_D3

/* ITCMRAM section - Fast code execution, 64KB */
.itcmram :
{
  . = ALIGN(4);
  _sitcmram = .;
  *(.itcmram)
  *(.itcmram*)
  . = ALIGN(4);
  _eitcmram = .;
} >ITCMRAM AT> FLASH
_siitcmram = LOADADDR(.itcmram);
```

**关键点**：
- `LOADADDR()` 必须在段定义**之后**调用
- 每个段需要三个符号：`_s`（起始地址）、`_e`（结束地址）、`_si`（Flash 加载地址）

---

### 2.2 启动文件 `startup_stm32h723xx.s`

**位置**：`cubemx/board_config/DM_MC02/startup_stm32h723xx.s`

**添加内容**（在 `.bss` 清零之后、`__libc_init_array` 之前）：

```assembly
/* Copy RAM_D1 from FLASH to RAM_D1 */
  ldr r0, =_sram_d1
  ldr r1, =_eram_d1
  ldr r2, =_siram_d1
  movs r3, #0
  b LoopCopyRamD1

CopyRamD1:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyRamD1:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyRamD1

/* Zero fill BSS for RAM_D1 */
  ldr r0, =_sbss_ram_d1
  ldr r1, =_ebss_ram_d1
  movs r2, #0
  b LoopFillZerobssRamD1

FillZerobssRamD1:
  str r2, [r0]
  adds r0, r0, #4

LoopFillZerobssRamD1:
  cmp r0, r1
  bcc FillZerobssRamD1

/* RAM_D2 和 RAM_D3 同上，省略... */

/* Copy ITCMRAM from FLASH to ITCMRAM */
  ldr r0, =_sitcmram
  ldr r1, =_eitcmram
  ldr r2, =_siitcmram
  movs r3, #0
  b LoopCopyItcmram

CopyItcmram:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyItcmram:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyItcmram
```

**关键点**：
- 复制顺序：先复制已初始化数据，再清零 BSS 段
- ITCMRAM 只需要复制（放代码），不需要清零

---

## 三、使用方法

### 3.1 DMA 缓冲区（RAM_D1）

```c
// 带初始值的 DMA 缓冲区（从 Flash 复制）
__attribute__((section(".ram_d1"))) uint8_t dma_tx_buffer[1024] = {0};

// 零初始化的 DMA 缓冲区（自动清零）
__attribute__((section(".bss_ram_d1"))) uint8_t dma_rx_buffer[1024];
```

**适用场景**：
- UART/SPI/CAN DMA 收发缓冲区
- 大型数据缓冲区
- 需要 DMA 访问的变量

### 3.2 ITCM 中的快速代码

```c
// 将函数放入 ITCM 执行（零等待）
__attribute__((section(".itcmram"))) void critical_task(void)
{
    // 高频执行的代码
}
```

**适用场景**：
- FreeRTOS 任务函数
- 中断处理函数
- 高频调用的算法函数

### 3.3 通过 bsp_map.h 使用宏

```c
// 在 bsp_map.h 中定义
#if CPU_CORE == CORTEX_M7
#define DMA_RAM __attribute__((section(".ram_d1")))
#define ITCM_RAM __attribute__((section(".itcmram")))
#else
#define DMA_RAM
#define ITCM_RAM
#endif

// 使用示例
ITCM_RAM void StartChassisTask(void *argument);
DMA_RAM uint8_t uart_dma_buffer[256];
```

---

## 四、推荐的 ITCM 放置策略

### 4.1 FreeRTOS 内核（自动）

在链接脚本中添加：

```ld
.itcmram :
{
  . = ALIGN(4);
  _sitcmram = .;
  
  /* FreeRTOS 内核代码 */
  */tasks.o(.text .text.*)
  */queue.o(.text .text.*)
  */list.o(.text .text.*)
  */timers.o(.text .text.*)
  */event_groups.o(.text .text.*)
  */stream_buffer.o(.text .text.*)
  */port.o(.text .text.*)
  */port_asm.o(.text .text.*)
  
  /* 用户代码 */
  *(.itcmram)
  *(.itcmram*)
  . = ALIGN(4);
  _eitcmram = .;
} >ITCMRAM AT> FLASH
```

### 4.2 任务函数（手动标记）

```c
// app_chassis.c
ITCM_RAM void StartChassisTask(void *argument) { ... }

// app_motor.c
ITCM_RAM void StartMotorTask(void *argument) { ... }

// app_sensor.c
ITCM_RAM void StartSensorTask(void *argument) { ... }
```

### 4.3 算法文件

将卡尔曼滤波等算法函数放入 ITCM：

```c
// kalman_filter.c
ITCM_RAM void kalman_update(kalman_t *kf, float measurement) { ... }
ITCM_RAM void kalman_predict(kalman_t *kf) { ... }
```

---

## 五、注意事项

1. **ITCM 大小限制**：64KB，不要超过
2. **DMA 不能访问 DTCM/ITCM**：DMA 缓冲区必须放在 RAM_D1/D2/D3
3. **缓存一致性**：RAM_D1 可被 CPU 缓存，DMA 操作前需要 SCB_CleanInvalidateDCache
4. **初始化顺序**：启动文件中的复制必须在调用 main() 之前完成
5. **链接脚本语法**：`LOADADDR()` 必须在段定义之后调用

---

## 六、验证方法

编译后检查 `.map` 文件：

```
.itcmram         0x00000000    0x1234
 *(.itcmram)
 .text.StartChassisTask
                 0x00000000    0x1234 app_chassis.o

.ram_d1          0x24000000    0x1000
 *(.ram_d1)
 dma_tx_buffer   0x24000000    0x400  main.o
```

确认：
- ITCMRAM 段 VMA = 0x00000000
- RAM_D1 段 VMA = 0x24000000
- LMA 在 Flash 区域（0x08000000 之后）
