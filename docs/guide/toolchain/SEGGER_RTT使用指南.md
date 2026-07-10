---
---

# SEGGER RTT 使用指南

本文档详细说明 SEGGER RTT 的移植、配置和使用方法，区分两种使用场景：**调试+日志** 和 **仅查看日志**。

---

## 一、概述

SEGGER RTT (Real Time Transfer) 是一种高速实时传输技术，允许在目标 MCU 运行时实时传输数据到主机。

### 优势

| 特性   | 说明                       |
| ------ | -------------------------- |
| 零开销 | 不占用 UART/USB 等通信接口 |
| 高速   | 传输速度可达数 MB/s        |
| 实时   | 不影响 MCU 正常运行        |
| 简单   | 仅需少量文件即可集成       |

### 适用场景

| 场景                         | 推荐方式                          |
| ---------------------------- | --------------------------------- |
| 开发调试、单步运行、查看日志 | **调试+日志模式**（VSCode F5）    |
| 仅查看运行日志、不中断程序   | **仅日志模式**（OpenOCD + PuTTY） |

---

## 二、移植步骤

### 2.1 文件清单

从 SEGGER 官网下载 RTT 源码包，复制以下文件：

| 文件名                    | 目标路径                                 | 说明                     |
| ------------------------- | ---------------------------------------- | ------------------------ |
| `SEGGER_RTT.c`            | `Middlewares/Third_Party/SEGGER/RTT/`    | RTT 核心实现             |
| `SEGGER_RTT.h`            | `Middlewares/Third_Party/SEGGER/RTT/`    | RTT 头文件               |
| `SEGGER_RTT_printf.c`     | `Middlewares/Third_Party/SEGGER/RTT/`    | printf 格式化支持        |
| `SEGGER_RTT_ASM_ARMv7M.s` | `Middlewares/Third_Party/SEGGER/RTT/`    | ARMv7-M 汇编优化（可选） |
| `SEGGER_RTT_Conf.h`       | `Middlewares/Third_Party/SEGGER/Config/` | 配置文件                 |

### 2.2 目录结构

```
Middlewares/Third_Party/SEGGER/
├── Config/
│   └── SEGGER_RTT_Conf.h    # 配置文件
└── RTT/
    ├── SEGGER_RTT.c         # 核心实现
    ├── SEGGER_RTT.h         # 头文件
    ├── SEGGER_RTT_printf.c  # printf 支持
    └── SEGGER_RTT_ASM_ARMv7M.s  # 汇编优化（可选）
```

### 2.3 Makefile 修改

#### 添加 C 源文件

在 `C_SOURCES` 变量中添加：

```makefile
Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_printf.c \
Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT.c \
```

#### 添加汇编源文件

在 `ASM_SOURCES` 变量中添加：

```makefile
Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_ASM_ARMv7M.s
```

#### 添加头文件路径

在 `C_INCLUDES` 变量中添加：

```makefile
-IMiddlewares/Third_Party/SEGGER/RTT \
-IMiddlewares/Third_Party/SEGGER/Config \
```

---

## 三、代码使用

### 3.1 基本初始化

```c
#include "SEGGER_RTT.h"

void AppInit(void)
{
    SEGGER_RTT_Init();
    // 其他初始化...
}
```

### 3.2 基本输出

```c
#include "SEGGER_RTT.h"

// 输出字符串
SEGGER_RTT_WriteString(0, "Hello RTT!\r\n");

// 格式化输出
SEGGER_RTT_printf(0, "Value: %d, Status: %s\r\n", 42, "OK");
```

### 3.3 日志封装层（bsp_log）

本项目封装了日志模块，位于 `bsp/Inc/bsp_log.h` 和 `bsp/Src/bsp_log.c`。

**初始化**：

```c
BSPLogInit();
```

**日志输出**：

```c
LOG("普通日志");                                    // 无颜色
LOGINFO("信息日志: %d", value);                     // 绿色
LOGWARNING("警告日志: %s", str);                    // 黄色
LOGERROR("错误日志: %d", error_code);               // 红色
PrintLog("自定义格式: %d, %s\n", num, str);         // 格式化打印
```

### 3.4 封装层实现参考

**bsp_log.h**：

```c
#ifndef _BSP_LOG_H
#define _BSP_LOG_H

#include "SEGGER_RTT.h"

#define BUFFER_INDEX 0

void BSPLogInit(void);
void Float2Str(char *str, float va);
int PrintLog(const char *fmt, ...);

// 日志输出宏
#define LOG_PROTO(type, color, format, ...)                       \
        SEGGER_RTT_printf(BUFFER_INDEX, "  %s%s" format "\r\n%s", \
                          color, type, ##__VA_ARGS__, RTT_CTRL_RESET)

#define LOG_CLEAR() SEGGER_RTT_WriteString(0, "  " RTT_CTRL_CLEAR)
#define LOG(format, ...) LOG_PROTO("", "", format, ##__VA_ARGS__)
#define LOGINFO(format, ...)    LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)
#define LOGWARNING(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
#define LOGERROR(format, ...)   LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)

#endif
```

**bsp_log.c**：

```c
#include "bsp_log.h"
#include <stdarg.h>
#include <stdio.h>

void BSPLogInit(void)
{
    SEGGER_RTT_Init();
}

int PrintLog(const char *fmt, ...)
{
    int r;
    va_list args;
    va_start(args, fmt);
    r = SEGGER_RTT_vprintf(BUFFER_INDEX, fmt, &args);
    va_end(args);
    return r;
}

void Float2Str(char *str, float va)
{
    sprintf(str, "%.4f", va);
}
```

---

## 四、OpenOCD 配置文件

### 4.1 openocd_dap.cfg（调试配置）

用于 VSCode F5 调试，提供 GDB 调试服务：

```bash
# 调试器接口
source [find interface/cmsis-dap.cfg]

# 传输协议
transport select swd

# 目标芯片
source [find target/stm32f4x.cfg]

# 适配器速度
adapter speed 4000
```

### 4.2 openocd_rtt.cfg（仅日志配置）

用于独立查看 RTT 日志，不提供 GDB 调试：

```bash
# 调试器接口
source [find interface/cmsis-dap.cfg]

# 传输协议
transport select swd

# 目标芯片
source [find target/stm32f4x.cfg]

# 适配器速度
adapter speed 4000

# 初始化
init

# RTT 配置
# 起始地址: RAM 起始地址 (STM32F407 为 0x20000000)
# 搜索大小: 0x10000 (64KB)
# 标识字符串: 必须与源码中一致
rtt setup 0x20000000 0x10000 "SEGGER RTT"
rtt start

# 启动 RTT 服务器
# 端口 8888，通道 0
rtt server start 8888 0

echo "RTT Server started on port 8888"
echo "Connect with: telnet 127.0.0.1 8888"
```

### 4.3 RTT 控制块地址

RTT 控制块 `_SEGGER_RTT` 位于 RAM 中。如果自动搜索失败，可通过 `.map` 文件查找精确地址：

```bash
# 在 build/*.map 文件中搜索
_SEGGER_RTT
```

找到地址后可优化 `rtt setup` 参数：

```bash
rtt setup 0x20005080 0x100 "SEGGER RTT"
```

---

## 五、使用方法：调试+日志模式

**适用场景**：开发调试、需要断点、单步运行、同时查看日志

### 5.1 前提条件

- 已正确配置 `launch.json`
- 已正确配置 `openocd_dap.cfg`
- 程序包含 RTT 初始化代码

### 5.2 操作步骤

1. 连接 DAP-Link 调试器到目标板
2. 在 VSCode 中按 `F5` 启动调试
3. 程序自动编译、下载、运行
4. RTT 日志在 VSCode **调试控制台** 中显示

### 5.3 特点

| 优点               | 缺点                           |
| ------------------ | ------------------------------ |
| 一键启动，操作简单 | 占用调试器资源                 |
| 支持断点、单步调试 | 调试时程序暂停，日志中断       |
| 日志与调试同步     | cortex-debug 内置 RTT 支持有限 |

---

## 六、使用方法：仅日志模式

**适用场景**：程序正常运行，仅需查看日志，不中断程序执行

### 6.1 前提条件

- 程序已下载到目标板并运行
- 程序包含 RTT 初始化代码
- 有 `openocd_rtt.cfg` 配置文件

### 6.2 操作步骤

**步骤 1：启动 OpenOCD RTT 服务**

在终端中运行：

```bash
openocd -f openocd_rtt.cfg
```

看到以下输出表示成功：

```
RTT Server started on port 8888
```

**步骤 2：连接客户端查看日志**

有多种客户端可选，**推荐使用 PuTTY**。

---

## 七、日志查看方案对比

| 方案              | 难度 | 滚动缓冲       | 保存日志 | 推荐度 |
| ----------------- | ---- | -------------- | -------- | ------ |
| **PuTTY（推荐）** | 简单 | 支持 20000+ 行 | 自动保存 | ★★★★★  |
| Windows telnet    | 简单 | 与窗口绑定     | 不支持   | ★★☆☆☆  |
| Python 脚本       | 中等 | 无限制         | 自动保存 | ★★★★☆  |
| J-Link RTT Viewer | 简单 | 支持           | 支持     | ★★★★☆  |

---

## 八、推荐方案：PuTTY

### 8.1 下载安装

从 [PuTTY 官网](https://www.putty.org/) 下载，无需安装，直接运行。

### 8.2 配置步骤

#### Step 1：基本连接配置

打开 PuTTY，在 **Session** 页面配置：

| 配置项                    | 值            |
| ------------------------- | ------------- |
| Host Name (or IP address) | `127.0.0.1`   |
| Port                      | `8888`        |
| Connection type           | 选择 `Telnet` |

#### Step 2：配置滚动缓冲区

切换到 **Terminal** 页面：

| 配置项              | 值      | 说明                  |
| ------------------- | ------- | --------------------- |
| Lines of scrollback | `20000` | 可显示 2 万行历史日志 |

#### Step 3：配置日志保存

切换到 **Logging** 页面：

| 配置项          | 值                            |
| --------------- | ----------------------------- |
| Session logging | 选择 `Printable output`       |
| Log file name   | 点击 `Browse...` 选择保存路径 |

建议使用时间戳命名：

```
C:\logs\rtt_log_&Y-&M-&D_&h-&m-&s.log
```

#### Step 4：保存配置

回到 **Session** 页面：

1. 在 `Saved Sessions` 输入名称，如 `RTT_Log`
2. 点击 `Save` 保存配置

下次直接双击 `RTT_Log` 即可连接。

### 8.3 使用方法

```bash
# 终端 1：启动 OpenOCD RTT 服务
openocd -f openocd_rtt.cfg

# 终端 2：双击 PuTTY 保存的 Session 连接
# 或在 PuTTY 中点击 Open 按钮
```

### 8.4 PuTTY 优点

| 特性         | 说明                                  |
| ------------ | ------------------------------------- |
| 大滚动缓冲区 | 可配置 20000 行以上，历史日志不会丢失 |
| 自动保存日志 | 所有输出自动保存到文件                |
| 配置保存     | Session 可保存，下次直接使用          |
| 免费开源     | 无需安装，绿色便携                    |
| 稳定可靠     | 连接稳定，不丢失数据                  |

---

## 九、备选方案

### 9.1 Python 脚本保存

适合需要自动化处理日志的场景。

在项目根目录创建 `save_rtt_log.py`：

```python
#!/usr/bin/env python3
"""
RTT 日志保存脚本
使用方法：
1. 先启动 OpenOCD：openocd -f openocd_rtt.cfg
2. 再运行本脚本：python save_rtt_log.py
3. Ctrl+C 停止记录
"""

import socket
import sys
from datetime import datetime

HOST = '127.0.0.1'
PORT = 8888

def main():
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    log_filename = f"rtt_log_{timestamp}.txt"

    try:
        log_file = open(log_filename, "w", encoding='utf-8')
    except IOError as e:
        print(f"无法创建日志文件: {e}")
        sys.exit(1)

    try:
        print(f"正在连接 RTT 服务器 {HOST}:{PORT}...")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            s.connect((HOST, PORT))
            s.settimeout(None)

            print(f"连接成功！日志保存到: {log_filename}")
            print("按 Ctrl+C 停止记录\n")
            print("-" * 50)

            while True:
                data = s.recv(4096)
                if not data:
                    print("\n服务器断开连接")
                    break

                text = data.decode('utf-8', errors='replace')
                print(text, end='')
                log_file.write(text)
                log_file.flush()

    except socket.timeout:
        print("连接超时，请确认 OpenOCD RTT 服务已启动")
    except ConnectionRefusedError:
        print("连接被拒绝，请确认 OpenOCD RTT 服务正在运行")
        print("启动命令: openocd -f openocd_rtt.cfg")
    except KeyboardInterrupt:
        print("\n\n记录已停止")
    finally:
        log_file.close()
        print(f"日志已保存到: {log_filename}")

if __name__ == "__main__":
    main()
```

### 9.2 J-Link RTT Viewer

如果使用 J-Link 调试器，可使用官方工具：

1. 打开 J-Link RTT Viewer
2. 配置连接参数：
   - Connection: USB
   - Device: STM32F407IG
   - Interface: SWD
   - Speed: 4000 kHz
3. 点击 Connect 连接目标

---

## 十、增大 RTT 缓冲区

如果日志量大或输出速度快，可增大缓冲区。

修改 `SEGGER_RTT_Conf.h`：

```c
#ifndef   BUFFER_SIZE_UP
  #define BUFFER_SIZE_UP  (4096)  // 从 1024 改为 4096 或更大
#endif
```

**注意**：缓冲区占用 RAM，需确保 MCU 有足够内存。

| 缓冲区大小  | 适用场景   |
| ----------- | ---------- |
| 1KB（默认） | 少量日志   |
| 4KB         | 中等日志量 |
| 8KB         | 大量日志   |
| 16KB        | 高频输出   |

---

## 十一、配置参数说明

在 `SEGGER_RTT_Conf.h` 中可调整：

| 参数                              | 默认值 | 说明                       |
| --------------------------------- | ------ | -------------------------- |
| `SEGGER_RTT_MAX_NUM_UP_BUFFERS`   | 3      | 上行缓冲区数量（MCU→主机） |
| `SEGGER_RTT_MAX_NUM_DOWN_BUFFERS` | 3      | 下行缓冲区数量（主机→MCU） |
| `BUFFER_SIZE_UP`                  | 1024   | 上行缓冲区大小（字节）     |
| `BUFFER_SIZE_DOWN`                | 16     | 下行缓冲区大小（字节）     |
| `SEGGER_RTT_PRINTF_BUFFER_SIZE`   | 64     | printf 缓冲区大小          |

---

## 十二、注意事项

### 12.1 浮点格式化限制

**RTT 不支持 `%f` 格式化！**

```c
// 错误
SEGGER_RTT_printf(0, "Value: %f", 3.14f);

// 正确：使用 Float2Str 转换
char str[32];
Float2Str(str, 3.14f);
SEGGER_RTT_printf(0, "Value: %s", str);
```

### 12.2 日志开关

在 Makefile 中通过宏定义控制：

```makefile
# 禁用日志系统（release 版本）
C_DEFS += -DDISABLE_LOG_SYSTEM

# 启用日志系统（debug 版本）- 注释掉上面的宏定义
```

### 12.3 调试器要求

| 调试器             | RTT 支持                         |
| ------------------ | -------------------------------- |
| J-Link             | 原生支持，推荐                   |
| CMSIS-DAP/DAP-Link | 需配置 OpenOCD（版本 >= 0.11.0） |

### 12.4 中断中使用

RTT 可以在中断中安全使用，但应避免在中断中输出大量数据，以免影响实时性。

---

## 十三、常见问题

### Q1: `rtt: No control block found`

**原因**：

- 程序未运行
- RTT 控制块不在搜索范围内
- RTT 未正确初始化

**解决**：

1. 确保程序正在运行（不是 halt 状态）
2. 检查 `.map` 文件中 `_SEGGER_RTT` 地址
3. 确保调用了 `SEGGER_RTT_Init()` 或使用了 RTT 输出函数

### Q2: 调试器连接冲突

**原因**：DAPLink 同时只能被一个进程访问

**解决**：

- 关闭其他 OpenOCD 实例
- 不要同时使用 F5 调试和 `openocd_rtt.cfg`

### Q3: Windows telnet 历史日志消失

**原因**：Windows telnet 的滚动缓冲区与窗口大小绑定

**解决**：

- **使用 PuTTY 替代 telnet**（推荐）
- 使用 Python 脚本保存日志到文件

### Q4: 日志乱码

**原因**：编码不匹配

**解决**：

- 确保 RTT 输出使用 UTF-8 编码
- PuTTY 中设置 `Window` → `Translation` → `Remote character set` 为 UTF-8

### Q5: 日志输出缓慢

**原因**：缓冲区太小，频繁阻塞

**解决**：

- 增大 `BUFFER_SIZE_UP`
- 减少单次输出数据量

---

## 十四、快速参考

### 两种使用模式对比

```
┌─────────────────────────────────────────────────────────────┐
│                    调试+日志模式                             │
│                                                             │
│  VSCode F5 → OpenOCD → GDB调试 → cortex-debug显示日志       │
│                                                             │
│  适用：开发调试、断点、单步运行                              │
│  配置：openocd_dap.cfg + launch.json                        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    仅日志模式                                │
│                                                             │
│  OpenOCD → RTT Server(8888) → PuTTY → 显示/保存日志         │
│                                                             │
│  适用：运行日志监控、不中断程序                              │
│  配置：openocd_rtt.cfg + PuTTY                              │
└─────────────────────────────────────────────────────────────┘
```

### 常用命令速查

```bash
# 启动 RTT 服务
openocd -f openocd_rtt.cfg

# PuTTY 连接（需先启动 RTT 服务）
# 打开 PuTTY，连接 127.0.0.1:8888

# 验证 RTT 服务是否启动
netstat -an | findstr 8888
```

---

## 参考链接

- [SEGGER RTT 官方文档](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/)
- [SEGGER RTT 下载](https://www.segger.com/downloads/jlink/#RTT)
- [OpenOCD 官方文档](https://openocd.org/documentation/)
- [PuTTY 官网](https://www.putty.org/)
