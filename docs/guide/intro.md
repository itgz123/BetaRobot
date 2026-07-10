# BetaRobot 项目介绍

## 项目概述

BetaRobot 是一个基于 STM32 的嵌入式机器人控制框架，支持多开发板、多 Application 并行开发。

### 特点

1. **多开发板支持** — 通过一个宏定义即可切换开发板（STM32F407VET6 / DM-MC02 / DJI A / DJI C）
2. **多 App 支持** — 多个 Application 可同时开发，底层更新时所有使用者可直接更新
3. **静态内存分配** — 所有内存静态分配，无动态内存管理，适合实时嵌入式系统
4. **分层架构** — BSP/DRV/APP 三层分离，单向依赖
5. **开源透明** — 底层实现完全可见，便于理解和调试

### 使用方法

```bash
# 复制配置文件
cp user_cfg.h.example user_cfg.h

# 构建项目（使用 CMake）
mkdir build && cd build
cmake ..
make
```

### 开发板支持

| 开发板 | 主控芯片 | 宏定义 |
|--------|----------|--------|
| STM32F407VET6 | STM32F407VET6 | `STM32F407VET6` |
| DM-MC02 | STM32H723xx | `DM_MC02` |
| DJI A | STM32F427xx | `DJI_A` |
| DJI C | STM32F407xx | `DJI_C` |

### 分层架构速查

| 层级 | 职责 | 关键点 |
|------|------|--------|
| APP | 业务逻辑、任务调度 | 唯一使用 FreeRTOS 的层，管理队列/互斥量/任务 |
| DRV | 模块驱动、协议解析 | 封装外部模块，提供解析函数，默认不使用 RTOS |
| BSP | 外设封装、中断分发 | 管理 MCU 外设实例，回调分发，不配置硬件 |

### 仓库地址

- GitHub: [https://github.com/itgz123/BetaRobot](https://github.com/itgz123/BetaRobot)
- Gitee: [https://gitee.com/trwwrt/BetaRobot](https://gitee.com/trwwrt/BetaRobot)
