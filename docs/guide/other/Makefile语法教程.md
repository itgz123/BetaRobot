---
---

# Makefile 语法教程

本文档基于项目的 Makefile 逐步讲解 Makefile 语法，适合只会 C 语言的开发者学习。

---

## 目录

1. [Makefile 是什么](#1-makefile-是什么)
2. [基本结构](#2-基本结构)
3. [变量定义与使用](#3-变量定义与使用)
4. [自动变量](#4-自动变量)
5. [条件判断](#5-条件判断)
6. [函数](#6-函数)
7. [模式规则](#7-模式规则)
8. [依赖规则](#8-依赖规则)
9. [伪目标](#9-伪目标)
10. [vpath 路径搜索](#10-vpath-路径搜索)
11. [include 包含](#11-include-包含)
12. [完整示例解析](#12-完整示例解析)

---

## 1. Makefile 是什么

Makefile 是一个构建脚本，告诉 `make` 工具如何编译和链接程序。

**核心思想**：
- 根据文件的**修改时间**判断是否需要重新编译
- 只编译修改过的文件，提高效率

**执行方式**：
```bash
mingw32-make        # 执行默认目标
mingw32-make clean  # 执行 clean 目标
mingw32-make -j4    # 4线程并行编译
```

---

## 2. 基本结构

Makefile 由一系列**规则**组成：

```makefile
目标: 依赖文件列表
	命令1
	命令2
```

**注意**：命令前必须是 **Tab 键**，不能用空格！

```makefile
# 示例：将 main.c 编译成 main.o
main.o: main.c
	gcc -c main.c -o main.o
```

**执行逻辑**：
1. 检查 `main.c` 是否比 `main.o` 新
2. 如果是，执行 `gcc -c main.c -o main.o`

---

## 3. 变量定义与使用

### 3.1 变量定义

```makefile
# 简单定义（递归展开，可能有问题）
TARGET = basic_framework

# 立即展开（推荐）
BUILD_DIR := build

# 追加变量
CFLAGS += -Wall
```

**`=` vs `:=` vs `?=` 的区别**：

| 方式 | 说明 | 示例 |
|------|------|------|
| `=` | 递归展开（使用时才展开） | `A = $(B)`，B 可以在后面定义 |
| `:=` | 立即展开（定义时就确定值） | `A := $(B)`，B 必须先定义 |
| `?=` | 条件赋值（未定义时才赋值） | `DEBUG ?= 1` |
| `+=` | 追加值 | `CFLAGS += -g` |

```makefile
# 项目中的示例
DEBUG = 1
OPT = -Og
TARGET = basic_framework
BUILD_DIR = build
```

### 3.2 变量使用

使用 `$(变量名)` 或 `${变量名}` 引用变量：

```makefile
# 项目中的示例
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
```

---

## 4. 自动变量

自动变量在规则命令中自动设置，非常常用：

| 变量 | 含义 | 示例 |
|------|------|------|
| `$@` | 目标文件名 | `$(BUILD_DIR)/main.o` |
| `$<` | 第一个依赖文件 | `main.c` |
| `$^` | 所有依赖文件 | `main.c utils.c` |
| `$*` | 模式匹配的部分（不含扩展名） | `main`（来自 `%.o: %.c`） |
| `$?` | 比目标新的所有依赖文件 | - |

**项目中的实际使用**：

```makefile
# $< 是第一个依赖（%.c），$@ 是目标（%.o）
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	@$(CC) -c $(CFLAGS) $< -o $@
```

**等价理解**：
```makefile
build/main.o: Src/main.c Makefile
	@arm-none-eabi-gcc -c $(CFLAGS) Src/main.c -o build/main.o
```

---

## 5. 条件判断

### 5.1 ifdef / ifndef

检查变量是否定义：

```makefile
# 项目中的示例
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
else
CC = $(PREFIX)gcc
endif
```

### 5.2 ifeq / ifneq

比较两个值是否相等：

```makefile
# 项目中的示例
ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif
```

**其他写法**：

```makefile
ifeq "$(DEBUG)" "1"
    CFLAGS += -g
endif

ifneq ($(DEBUG),)
    # DEBUG 不为空时执行
endif
```

---

## 6. 函数

Makefile 内置了许多实用函数。

### 6.1 文本处理函数

#### subst - 字符串替换

```makefile
$(subst 要替换的, 替换成, 源字符串)

# 示例
SRC := main.c utils.c
OBJ := $(subst .c,.o,$(SRC))
# 结果：main.o utils.o
```

#### patsubst - 模式替换

```makefile
$(patsubst 模式, 替换模式, 源列表)

# 示例
SRC := Src/main.c Src/utils.c
OBJ := $(patsubst %.c,%.o,$(SRC))
# 结果：Src/main.o Src/utils.o
```

### 6.2 文件名函数

#### notdir - 提取文件名（不含路径）

```makefile
$(notdir 文件路径列表)

# 示例
FILES := Src/main.c Drivers/gpio.c
NAMES := $(notdir $(FILES))
# 结果：main.c gpio.c
```

#### dir - 提取目录部分

```makefile
$(dir 文件路径列表)

# 示例
FILES := Src/main.c Drivers/gpio.c
DIRS := $(dir $(FILES))
# 结果：Src/ Drivers/
```

#### addprefix - 添加前缀

```makefile
$(addprefix 前缀, 文件列表)

# 项目中的示例
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
# 将 main.o 变成 build/main.o
```

#### wildcard - 通配符展开

```makefile
$(wildcard 模式)

# 项目中的示例
-include $(wildcard $(BUILD_DIR)/*.d)
# 包含 build 目录下所有 .d 文件
```

### 6.3 项目中的函数组合示例

```makefile
# 这行代码做了什么？
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
```

**分解步骤**：
1. `$(C_SOURCES:.c=.o)` - 将 `.c` 替换为 `.o`（简化的 patsubst）
   - `Src/main.c` → `Src/main.o`
2. `$(notdir ...)` - 提取文件名
   - `Src/main.o` → `main.o`
3. `$(addprefix $(BUILD_DIR)/,...)` - 添加 `build/` 前缀
   - `main.o` → `build/main.o`

---

## 7. 模式规则

模式规则使用 `%` 通配符，是 Makefile 的核心特性。

### 7.1 基本语法

```makefile
%.o: %.c
	$(CC) -c $< -o $@
```

**含义**：任意 `.o` 文件依赖于对应的 `.c` 文件。

### 7.2 项目中的模式规则

```makefile
# 编译 C 文件
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# 编译汇编文件
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@$(AS) -c $(CFLAGS) $< -o $@
```

**解析**：
- `$(BUILD_DIR)/%.o` - 目标：`build/xxx.o`
- `%.c` - 依赖：对应的 `xxx.c` 文件
- `Makefile` - 额外依赖：Makefile 修改后也要重新编译
- `| $(BUILD_DIR)` - **order-only 依赖**：只确保目录存在，不触发重建

---

## 8. 依赖规则

### 8.1 普通依赖

```makefile
main.o: main.c header.h
	gcc -c main.c -o main.o
```

如果 `main.c` 或 `header.h` 更新，则重新编译。

### 8.2 Order-Only 依赖（`|`）

```makefile
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR):
	@mkdir $@
```

**含义**：
- `| $(BUILD_DIR)` 表示只检查目录是否存在
- 目录创建时间不影响是否重新编译
- 避免每次都重新编译

### 8.3 多目标规则

```makefile
# 项目中的示例
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
```

**等价于**：
```makefile
all: build/basic_framework.elf build/basic_framework.hex build/basic_framework.bin
```

---

## 9. 伪目标

伪目标是不对应实际文件的目标，总是执行。

### 9.1 声明伪目标

```makefile
.PHONY: clean all download

clean:
	rm -rf $(BUILD_DIR)

all:
	# 构建所有文件
```

### 9.2 项目中的伪目标

项目没有显式声明 `.PHONY`，但以下目标实际上是伪目标：

```makefile
clean:
	rd $(BUILD_DIR) /s/q

download_dap:
	openocd -f openocd_dap.cfg -c init -c halt -c "flash write_image erase $(BUILD_DIR)/$(TARGET).bin 0x08000000" -c reset -c shutdown

download_jlink:
	JFlash -openprj'stm32.jflash' -open'$(BUILD_DIR)/$(TARGET).hex',0x8000000 -auto -startapp -exit
```

**建议添加**：
```makefile
.PHONY: all clean download_dap download_jlink
```

---

## 10. vpath 路径搜索

`vpath` 告诉 make 在哪些目录中查找源文件。

### 10.1 语法

```makefile
vpath 模式 目录列表
```

### 10.2 项目中的使用

```makefile
# 设置 .c 文件的搜索路径
vpath %.c $(sort $(dir $(C_SOURCES)))

# 设置 .s 文件的搜索路径
vpath %.s $(sort $(dir $(ASM_SOURCES)))
```

**工作原理**：
1. `$(dir $(C_SOURCES))` - 提取所有源文件的目录
2. `$(sort ...)` - 去重并排序
3. `vpath %.c ...` - make 会在这些目录中搜索 `.c` 文件

**示例**：
```makefile
C_SOURCES = Src/main.c Src/gpio.c Drivers/hal.c

# 提取目录：Src/ Src/ Drivers/
# 去重后：Drivers/ Src/
vpath %.c Drivers/ Src/

# 当需要 main.o 时，make 会自动在 Src/ 目录找到 main.c
```

---

## 11. include 包含

### 11.1 基本用法

```makefile
include other.mk
```

### 11.2 条件包含

```makefile
# 如果文件存在则包含，不存在不报错
-include $(wildcard $(BUILD_DIR)/*.d)
```

**`-` 的作用**：忽略文件不存在的错误。

### 11.3 依赖文件自动生成

项目使用 `-MMD -MP` 自动生成依赖关系：

```makefile
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
```

**生成的 `.d` 文件示例**：
```makefile
# build/main.d
build/main.o: Src/main.c Inc/main.h Inc/gpio.h
```

这样当头文件修改时，make 能自动检测到并重新编译。

---

## 12. 完整示例解析

让我们逐段分析项目的 Makefile：

### 12.1 目标和变量定义

```makefile
# 目标名称
TARGET = basic_framework

# 调试选项
DEBUG = 1
OPT = -Og

# 构建目录
BUILD_DIR = build
```

### 12.2 源文件列表

```makefile
# C 源文件（用反斜杠续行）
C_SOURCES =  \
Src/main.c \
Src/gpio.c \
...

# 汇编源文件
ASM_SOURCES =  \
startup_stm32f407xx.s \
...
```

### 12.3 工具链配置

```makefile
# 交叉编译工具链前缀
PREFIX = arm-none-eabi-

# 如果定义了 GCC_PATH，使用完整路径
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
else
CC = $(PREFIX)gcc
endif
```

### 12.4 编译标志

```makefile
# MCU 相关标志
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# 预处理器定义（-D 选项）
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32F407xx

# 头文件路径（-I 选项）
C_INCLUDES =  \
-IInc \
-IDrivers/STM32F4xx_HAL_Driver/Inc \
...

# 组合编译标志
CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -fdata-sections -ffunction-sections -Werror

# 如果是调试模式，添加调试信息
ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif
```

### 12.5 链接标志

```makefile
# 链接脚本
LDSCRIPT = STM32F407IGHx_FLASH.ld

# 库文件
LIBS = -lc -lm -lnosys -l:libCMSISDSP.a
LIBDIR = -LMiddlewares/ST/ARM/DSP/Lib

# 链接标志
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
          -Wl,--gc-sections
```

### 12.6 构建规则

```makefile
# 默认目标
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# 生成目标文件列表
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))

# 设置源文件搜索路径
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# 编译 C 文件的规则
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	@$(CC) -c $(CFLAGS) $< -o $@

# 编译汇编文件的规则
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@$(AS) -c $(CFLAGS) $< -o $@

# 链接生成 ELF
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# 生成 HEX
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

# 生成 BIN
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

# 创建构建目录
$(BUILD_DIR):
	@mkdir $@
```

### 12.7 清理和下载

```makefile
# 清理（Windows 命令）
clean:
	rd $(BUILD_DIR) /s/q

# DAP-Link 下载
download_dap:
	openocd -f openocd_dap.cfg -c init -c halt -c "flash write_image erase $(BUILD_DIR)/$(TARGET).bin 0x08000000" -c reset -c shutdown

# J-Link 下载
download_jlink:
	JFlash -openprj'stm32.jflash' -open'$(BUILD_DIR)/$(TARGET).hex',0x8000000 -auto -startapp -exit
```

---

## 补充知识

### 命令前缀

| 前缀 | 作用 |
|------|------|
| 无前缀 | 正常执行，显示命令 |
| `@` | 静默执行，不显示命令 |
| `-` | 忽略命令错误继续执行 |

```makefile
# 静默执行
@$(CC) -c $< -o $@

# 忽略错误
-rm -f *.o
```

### 特殊变量

| 变量 | 含义 |
|------|------|
| `MAKE` | 当前 make 程序名 |
| `MAKECMDGOALS` | 命令行指定的目标 |
| `MAKEFILE_LIST` | 已读取的 Makefile 列表 |
| `MAKEFLAGS` | 传递给 make 的标志 |

### 常用选项

```bash
# 显示执行的命令但不真正执行
make -n

# 忽略错误继续执行
make -i

# 强制重新编译所有文件
make -B

# 指定使用的 Makefile 文件
make -f MyMakefile

# 并行编译（4 个任务）
make -j4
```

### 常见错误

1. **Tab 错误**：命令必须以 Tab 开头
   ```
   Makefile:10: *** missing separator. Stop.
   ```

2. **变量未定义**：使用 `$(undefined_var)` 会展开为空

3. **路径问题**：Windows 路径分隔符问题
   ```makefile
   # 推荐使用正斜杠（Windows 也支持）
   BUILD_DIR = build
   # 而不是
   BUILD_DIR = build\
   ```

---

## 快速参考卡

```
┌─────────────────────────────────────────────────────────────┐
│                    Makefile 快速参考                         │
├─────────────────────────────────────────────────────────────┤
│ 变量                                                         │
│   VAR = value    递归展开                                    │
│   VAR := value   立即展开                                    │
│   VAR ?= value   未定义时赋值                                │
│   VAR += value   追加                                        │
│   $(VAR) 或 ${VAR} 引用                                      │
├─────────────────────────────────────────────────────────────┤
│ 自动变量                                                     │
│   $@   目标文件名                                            │
│   $<   第一个依赖                                            │
│   $^   所有依赖                                              │
│   $*   模式匹配部分                                          │
├─────────────────────────────────────────────────────────────┤
│ 函数                                                         │
│   $(subst from,to,text)      字符串替换                      │
│   $(patsubst %from,%to,text) 模式替换                        │
│   $(notdir names)            提取文件名                      │
│   $(dir names)               提取目录                        │
│   $(addprefix prefix,names)  添加前缀                        │
│   $(wildcard pattern)        通配符展开                      │
├─────────────────────────────────────────────────────────────┤
│ 条件                                                         │
│   ifdef VAR / ifndef VAR                                     │
│   ifeq (a,b) / ifneq (a,b)                                   │
│   else / endif                                               │
├─────────────────────────────────────────────────────────────┤
│ 模式规则                                                     │
│   %.o: %.c                                                   │
│       $(CC) -c $< -o $@                                      │
├─────────────────────────────────────────────────────────────┤
│ 特殊语法                                                     │
│   .PHONY: target     声明伪目标                              │
│   vpath %.c dir      设置搜索路径                            │
│   -include file      可选包含                                │
│   | dependency       order-only 依赖                         │
└─────────────────────────────────────────────────────────────┘
```

---

*本文档基于 basic_framework 项目的 Makefile 编写*
