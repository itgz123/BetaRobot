# bsp_i2c 开发总结

> 本文件记录 bsp_i2c（`bsp_i2c.h` / `bsp_i2c.c`）接口用法、与 bsp_spi 的差异、
> 踩过的坑与处理方式、以及当前板级配置下**还不能跑**的部分。
> 起因为给 IST8310 三轴磁力计补 I2C 抽象层（原先 `bsp/bsp_iic/` 只是两个 0 字节占位），
> 后按 bsp_usart / bsp_spi 的模板统一重构（公共状态码 + 三模式传参 + 状态结构体 +
> 回调命名），三者的接口形状现已一致。

## 1. bsp_i2c 接口使用说明

### 1.1 入口

| 接口                              | 作用                                       | 可重复调用 |
| --------------------------------- | ------------------------------------------ | ---------- |
| `I2C_INSTANCE_DEF(name, buff_sz)` | 静态定义实例 + 收发缓冲                    | -          |
| `I2CRegister(instance)`           | 参数校验 + 防重入 static 管理数组          | 否         |
| `I2CConfig(instance, config)`     | 填硬件句柄 / parent / 三个回调，登记路由表 | 是         |

`I2CConfig` 只改实例字段、不碰硬件才是常态用法；**运行时切换传输模式不再需要它**
—— 模式现在是每次收发调用传参的（旧版 drv_ist8310 靠反复 `I2CConfig` 切模式，
见 §3.A）。

所有接口返回统一的 `BSP_Status_e`（`bsp_common.h`），不再是 0/-1：

| 返回码          | 含义                                                                     |
| --------------- | ------------------------------------------------------------------------ |
| `BSP_OK`        | 已受理：BLOCK=已完成，IT/DMA=已启动（完成看回调）                        |
| `BSP_BUSY`      | 总线忙（`timeout_ms == 0` 时的常态）或 HAL 报 `HAL_BUSY` 的可重试争用    |
| `BSP_TIMEOUT`   | 超时：等就绪超时，或 HAL 启动后即报超时类错误（`ErrorCode` 带 `TIMEOUT` / F4 的 `WRONG_START`）。此时 bsp 已复位句柄并回调了 `err_callback`，可直接重试 |
| `BSP_PARAM_ERR` | 参数非法 / `dev_addr` 越界 / len 越界 / mode 非法 / 该口没有对应方向的 DMA |
| `BSP_HW_ERR`    | HAL 启动失败的真故障（`ErrorCode` 是 NACK/BERR/ARLO/DMA…）               |

**超时之所以单独一个码**：`TIMEOUT` / `WRONG_START` 都表示"这一笔压根没发出去、
总线本身多半还是好的"（从机不应答是 `AF`，不在这里），上层据此可以立刻重试一次，
而不必当硬件故障进恢复流程。判据只能取 `ErrorCode`，**不能看 HAL 的返回值**——
H7 的超时是以 `HAL_ERROR` + `ErrorCode=TIMEOUT` 返回的，`HAL_TIMEOUT` 在那边
是另一回事（见 §2.A.8）。

### 1.2 传输接口签名

```c
BSP_Status_e I2CMemRead (I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                         I2C_MemAddrSize_e mem_addr_size, uint16_t len,
                         BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                         I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                         BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                               uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e I2CMasterReceive (I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                               BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                              uint32_t timeout_ms);
BSP_Status_e I2CBusRecover   (I2CInstance *instance);
```

- **`mode` 三选一，每次调用传参**：`BSP_BLOCK_MODE` / `BSP_IT_MODE` / `BSP_DMA_MODE`。
- **收数据的接口不带出参**：读到的数据一律落在 `instance->rx_buff[0..len-1]`，
  长度上限是 `I2C_INSTANCE_DEF` 时定的 `buff_size`（超了直接 `BSP_PARAM_ERR` 拒绝，
  不做静默截断）。这是照着 `bsp_spi` 的做法来的。
- `dev_addr` 传的是**设备地址本身**，不是 HAL 的 8 位形式：7 位寻址填 `0x00~0x7F`
  （IST8310 就是 `0x0E`），10 位寻址填 `0x000~0x3FF`；**编码（7 位左移一位）由 bsp
  按句柄的 `Init.AddressingMode` 自己做**，越界一律 `BSP_PARAM_ERR`，绝不静默截断。
  旧版的 `I2C_DEV_ADDR(addr7)` 宏已删除，原因见 §2.A.7。
- `mem_addr_size` 用自定义的 `I2C_MEM_ADDR_SIZE_8BIT/16BIT`，
  对应 HAL 的 `I2C_MEMADD_SIZE_*`，避免把 HAL 类型漏给 DRV 层。
- `timeout_ms` 只对 IT/DMA 用于"等总线就绪"；**BLOCK 模式透传给 HAL 当传输超时**。
  IT/DMA 传 0 = 只判一次，忙即 `BSP_BUSY`（中断里必须传 0，见 A.5）。

### 1.3 与 bsp_spi 的关键差异：从机地址不进实例

SPI 的"片选"是物理引脚，`bsp_spi` 把它交给 DRV 用 `bsp_gpio` 自己管；
I2C 的"片选"是 7 位从机地址，`bsp_i2c` 把它做成**每次调用传入的参数**。

这么设计的直接原因：同一根 I2C 总线上挂两个从机时，它们的 `handle` 是**同一个**。
若把地址放进 `Config`、按从机各注册一个 `I2CInstance`，handle → instance 的查表
（回调分发靠它）就无法区分，异步完成回调会派发到错误实例。
实例里不再留 `last_dev_addr` / `last_mem_addr`，这些现场连同计数器一起挪进了
调试用的 `s_i2c_status[]`（见 §1.5）。

### 1.4 回调约定

| 回调                        | 触发时机                                            |
| --------------------------- | --------------------------------------------------- |
| `rx_callback(instance)`     | **读**传输正常完成（Mem 读 / MasterReceive）        |
| `tx_callback(instance)`     | **写**传输正常完成（Mem 写 / MasterTransmit）       |
| `err_callback(instance, r)` | 启动失败 / 等就绪超时 / HAL 报错（**不会有完成回调**）|

读写分成两个回调（旧版只有一条）：旧版靠"当前是不是 INT 模式"来分辨一次完成该不该
当磁力帧发布，于是轮询模式下用 IT/DMA 写 CNTL1 时只能靠外层临时切阻塞来回避
（见 §3.A）。分开之后这个歧义从根上没有了。

**本层只做主模式（Master/Mem），不支持 MCU 作从机**：上表就是全部回调 —— HAL 的
`HAL_I2C_AddrCallback` / `SlaveTxCplt` / `SlaveRxCplt` / `ListenCplt` / `AbortCplt`
一概不实现（这些槽位交回 HAL 的 `__weak` 空实现），`HAL_I2C_Slave_*` /
`HAL_I2C_EnableListen_IT` 一族也不封装。板卡侧 `OwnAddress1` 全是 0，本就没有任何总线
把 MCU 当从机。从机与这里的模型不是一回事（**长驻 LISTEN 状态**、传输方向要到寻址
阶段才知道，故 HAL 才要求用 `Slave_Seq_*`），真要这个能力请另起模块。

`err_callback` 的第二个参数是 `I2C_ErrReason_e`：

- `I2C_ERR_HW`：HAL 报硬件错，运行在 `HAL_I2C_ErrorCallback` 里，**ISR 上下文**；
- `I2C_ERR_ABORT`：传输没发起成功或已被 bsp 强制收尾（启动失败 / 等就绪超时 /
  `I2CConfig` 重入收尾），**任务上下文**，此时 HAL `State` 已复位为 `READY`。

**`err_callback` 是必须挂的**，不是可选装饰：一次传输若"没发起成功"，
完成回调永远不会来，DRV 层若只依赖完成回调清自己的传输标志，就会永久卡在 busy
（现象是数据不再更新、看似整个器件失联）。drv_bmi088 的 `BMI088_SPIErrCallback`
与 drv_ist8310 的 `IST8310_I2CErrCallback` 都是为此存在。
handler 必须无阻塞、可重入且**幂等**（判自身状态再复位，重复调用无副作用）。

**上下文约束（三模式通用）**：

- BLOCK 内部按 HAL tick 自旋，**禁止在中断上下文调用**；
- IT/DMA 可在中断里调用，但**必须传 `timeout_ms = 0`**（忙即 `BSP_BUSY`）；
- `I2CIsDeviceReady` / `I2CBusRecover` 只能在任务上下文调用。

**传输互斥是调用方的责任**：同一实例同一时刻只允许一笔在途传输（BLOCK 亦然）。
本层做到的是"发起前先判一次总线归属，判不过就返回 `BSP_BUSY` 且**不写实例里的任何
现场**"（`I2C_ClaimBus`），本层**没有锁**。判完到真正调用 HAL 之间的窗口始终存在，
两个上下文真并发时 HAL 的 `State`/`Lock` 只能保证不重复启动，不保证 `rx_buff`
与快照不被覆盖。需要并发就在上层串行化 —— drv_ist8310 的 `transfer_busy` / `armed`
就是这个角色。

### 1.5 状态与计数（`s_i2c_status[]`）

每根总线一份 `volatile I2C_Status_s s_i2c_status[I2C_NUM_MAX]`，**只增不清**，
需要清零就在调试器里直接写 0。四组字段：

- 收发计数：`tx_ok` / `rx_ok` / `xfer_fail` / `xfer_busy` / `xfer_timeout`；
- 错误分类：`err_total` / `err_berr` / `err_arlo` / `err_af` / `err_ovr` / `err_dma` /
  `err_timeout` / `err_other` / `err_no_dma` / `err_start`；
- 收尾与恢复：`abort_reset` / `bus_rebuild` / `bus_recover` / `bus_recover_skip`。
  `bus_recover` 只计**真正动手**的次数；`bus_recover_skip` 计"调了但入口自证不成立、
  什么都没做"的次数（§2.B.1）。两个一起看就能判断上层是不是在白调 `I2CBusRecover`：
- 探测计数（`I2CIsDeviceReady`）：`probe_ok` / `probe_fail` / `probe_busy`。
  它唯一的使用者是恢复流程里那道"器件还在不在"的门禁，而现场最需要区分的正是
  "器件不应答"与"总线根本没让出来"这两条分支 —— 只看 `recover_count` 是看不出来的。
  之所以不拆 `probe_nack` / `probe_timeout`：两版 HAL 的返回对不上 —— F4 在
  "START 位超时"时返回 `HAL_TIMEOUT`、试满 trials 返回 `HAL_ERROR`，而 H7 两种
  情况**都**返回 `HAL_ERROR`，且"试满 trials 全是 NACK"这条路径在 H7 上同样会置
  `HAL_I2C_ERROR_TIMEOUT` 位，从返回值与 ErrorCode 都分不开；
- 实时快照：`state` / `last_mode` / `last_mem_addr_size` / `error_code` / `err_time_us` /
  `lock` / `dma_tx_capable` / `dma_rx_capable` / `dma_tx_state` / `dma_rx_state` /
  `dev_addr` / `mem_addr` / `xfer_len` / `rx_len`。
  其中 `dev_addr` 记的是**编码后**（真正喂给 HAL 的）形式：7 位寻址下 IST8310 显示为
  `0x1C` 而不是 `0x0E`，看快照时不必再自己左移一遍（§2.A.7）。

错误分类掩码 `I2C_ERROR_KNOWN_MASK`（BERR/ARLO/AF/OVR/DMA/TIMEOUT）已逐位对照过
F4 与 H7 两版 HAL 的头文件，**两者定义完全一致**（H7 另多一个
`HAL_I2C_ERROR_INVALID_PARAM`），故一份掩码两版共用；掩码外的位一律进 `err_other`，
保证 `err_total` 与各项明细自洽。

## 2. 之前踩过的坑与处理

### A. 硬件/时序坑

**A.1 两套 HAL 的 I2C 中断宏名字完全不同**（F4 vs H7）
F4 的中断源在 **CR2**：`I2C_IT_EVT` / `I2C_IT_BUF` / `I2C_IT_ERR`；
H7 的中断源在 **CR1**：`I2C_IT_ERRI` / `I2C_IT_TCI` / `I2C_IT_STOPI` / `I2C_IT_NACKI`
/ `I2C_IT_ADDRI` / `I2C_IT_RXI` / `I2C_IT_TXI`，**没有 EVT/BUF/ERR 这三个**。
处理：`bsp_i2c.c` 的 `I2C_ResetHandle()` 里用 `#if CPU_CORE == CORTEX_M7` 分支。

**A.2 `XferISR` 是 H7 独有字段**，F4 的 `I2C_HandleTypeDef` 里根本没有。
处理：同上，复位句柄状态时用 `#if` 包起来。

**A.3 `I2C_STATE_NONE` 不是公开符号**
它是 `stm32xxxx_hal_i2c.c` 内部的私有宏，定义恰好等于 `HAL_I2C_MODE_NONE`。
写 `h->PreviousState = I2C_STATE_NONE;` **编译不过**。处理：直接用 `HAL_I2C_MODE_NONE`。

**A.4 `HAL_I2C_Mem_Read_IT/_DMA` 内部已经做了"写寄存器地址 → 重复起始 → 读"**
两段式（H7 `hal_i2c.c:2885-2958 / 3118-3250`），BSP 不需要手工拆成两笔传输。

**A.5 阻塞式 HAL I2C 用 `HAL_GetTick()` 计时 → 禁止在中断里调用**
tick 中断优先级低于 EXTI，在中断上下文里 tick 不前进：任何阻塞 I2C 调用
要么立刻超时、要么卡死（`HAL_DMA_Abort` 内部也按 `HAL_GetTick` 轮询等 EN 清零，
同款问题）。**所有阻塞与"复位收尾"动作只能在任务上下文做**。
本层的上下文判据是 `I2C_CanBlockingAbort()`：`IPSR == 0 && PRIMASK == 0 && BASEPRI == 0`
（临界区里 tick 同样冻住，只查 IPSR 不够）。

**A.6 HAL 的写接口形参不是 const**（F4/H7 都如此）
`HAL_I2C_Mem_Write*` / `HAL_I2C_Master_Transmit*` 的 `pData` 是 `uint8_t *`，
直接传 `const uint8_t *` 会报 `-Wdiscarded-qualifiers`。
处理：BSP 对外保持 `const uint8_t *`（与 bsp_spi / bsp_usart 一致），
在 `I2CMemWrite` / `I2CMasterTransmit` 里各转一次指针，注释说明 HAL 并不写这块内存。

**A.7 从机地址的两种"形式"混在一起，很容易静默寻址错**
HAL 的 `DevAddress` 形参要的是 **7 位地址左移一位**后的 8 位形式（手册注释原话是
"must be shifted to the left before calling the interface"），而器件手册给的是 7 位地址。
旧版 bsp_i2c 把这个左移甩给调用方，提供了一个 `I2C_DEV_ADDR(addr7)` 宏，于是有两个坑：

- **忘用宏**（直接传 `0x0E`）在 7 位寻址下会静默寻址到 `0x07`，现象只是 NACK，
  很难从现象反推到"少左移了一位"；
- **10 位寻址下这个宏恰恰是错的**：F4 用 `I2C_10BIT_HEADER_WRITE(DevAddress)`
  （取 `addr[9:8]`）、H7 用 `I2C_CR2_ADD10` + `SADD[9:0]`，两版要的都是**未左移**的裸地址。
  统一宏在两种寻址模式下不可能都对。

处理（本轮改动，与 xrobot/LibXR 的 `EncodeHalDevAddress` 约定一致）：
`dev_addr` 一律传**设备地址本身**，编码收进 `I2C_EncodeDevAddr()` 由 bsp 按
`Init.AddressingMode` 做，越界直接 `BSP_PARAM_ERR`；`I2C_DEV_ADDR` 宏删除。
`drv_ist8310` 侧因此也删掉了 `IST8310_I2C_ADDR` 别名，直接用 `IST8310_I2C_ADDR_7BIT`。

顺带记一笔 HAL 的限制：**F4 的 Mem 接口不支持 10 位寻址**——
`I2C_RequestMemoryWrite/Read` 里是无条件 `I2C_7BIT_ADD_WRITE(DevAddress)`。
要用 10 位从机，只能走 Master 收发、自己拼寄存器地址那一段时序。

**A.8 "超时"从 HAL 回来的形状不止一种，分类只能看 `ErrorCode`**

- **两版** HAL 的 `I2C_WaitOnFlagUntilTimeout` 超时后返回的都是 **`HAL_ERROR`** 而
  **不是 `HAL_TIMEOUT`**（F4 `stm32f4xx_hal_i2c.c:7234`、H7 同构；返回前 `State` 复位
  `READY`、`Mode` 清 `NONE`、`ErrorCode |= TIMEOUT`）——**光看返回值分不出超时**；
- F4 的 IT/DMA 入口还多一种：`State` 已是 `READY`、硬件 `BUSY` 标志却在
  `I2C_TIMEOUT_BUSY_FLAG` 内不落时，**置 `TIMEOUT` 后返回 `HAL_BUSY`**
  （H7 的对应分支不置位）——这种"忙"其实是总线卡死，不是争用；
- 而常规争用的 `HAL_BUSY`（入口判 `State`/`Lock` 不通过）**不置任何错误位**，
  F4 那条例外是唯一能靠 `ErrorCode` 与它区分开的。

所以 `I2C_StartFail()` 的分类顺序是：先看 `HAL_BUSY` 且 `ErrorCode` 无 `TIMEOUT` → 争用
（`BSP_BUSY`，不计错、不打日志）；否则按 `I2C_START_TIMEOUT_MASK` 分 `BSP_TIMEOUT` /
`BSP_HW_ERR`。注意 `ErrorCode` 必须在 `I2C_AbortOnError` **之前**采样，那里面
`I2C_ResetHandle` 会把它清零。

### B. 设计取舍

**B.1 恢复只做外设级，不翻转 SCL/SDA 引脚**
`I2CBusRecover()` = `I2C_ResetHandle()` + `HAL_I2C_DeInit()` + `HAL_I2C_Init()` + 兜底复位。

- `DeInit` 把 `State` 置 `RESET` 并调 `MspDeInit`；
- `Init` 见 `State==RESET` 会重新 `MspInit`（重开时钟、重配 GPIO/NVIC），
  并用 `hi2c->Init` 里的原始值重写外设寄存器 —— **F4 的 `ClockSpeed/DutyCycle`
  与 H7 的 `Timing` 字段差异因此自动抹平，BSP 不必区分平台**。

不做引脚翻转（bit-banging 9 个时钟脉冲）意味着：从机把 SDA 一直拉死时本层救不回来，
`I2CBusRecover` 会在结束时读 `I2C_FLAG_BUSY`，仍置位就返回 `BSP_HW_ERR`，
由 DRV 记 ERROR 并冷却重试。真解脱需要后续在 `bsp_map` 里给该总线加两根 GPIO，
或依赖从机自己的 RSTN 引脚（drv_ist8310 的恢复流程会顺带脉冲一次 RSTN）。

**B.1.1 `I2CBusRecover` 的入口自证：判据必须在 bsp 内、且在任何动作之前**

本函数是**总线级**动作（`DeInit`+`Init` 会把时钟、GPIO、NVIC 全放掉再重配，见上），
而它的触发者（drv_ist8310）看到的是**实例级**现象 —— "我这个从机没应答"。两者作用域
不同，所以判据不能交给调用点，只能由 bsp 在入口自己读总线状态：

| 判据                   | 读取                                                       | 为什么                                                                                                                                                             |
| ---------------------- | ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ① 总线被占             | `I2C_FLAG_BUSY` 置位                                        | 唯一的总线级观测量。总线已放开时（NACK / 从机挂了 / 一次总线噪声）它为 0                                                                                            |
| ② "占着总线那位"无人负责 | `I2C_BusIsIdle(h)`（`State == READY && Lock` 未锁）          | BUSY 置位却没有任何在途传输在负责 → 标志被闩住，或从机把 SCL/SDA 拉着。**正常传输同样让 BUSY 置位**，所以单看①会在传输途中误重建，② 不能省、也不能倒过来先看 |

两条**同时**成立 → 动手（`bus_recover++`）；否则 `return BSP_BUSY` = 什么都没做
（`bus_recover_skip++`），由调用点的失败计数 + 冷却决定下一次何时再看。

非任务上下文（`I2C_CanBlockingAbort()` 为 0）也走同一个出口返回 `BSP_BUSY`：
`DeInit`/`Init` 含 `MspDeInit`/`MspInit`，中断里做不了（§2.B.4）。三种原因对调用方
而言返回值语义相同（"未做任何事"），故共用一个出口，不拆成多个返回码。

**被否掉的判据**：

- **只看"器件不应答"**（= 调用点的现象）：**实例级**证据。对端不发、从机挂了、只是赶上
  一次总线噪声，都会成立，而这三样重建外设一个都治不好，代价却是把本总线上**别的实例**
  的在途传输一起打断。
- **加"卡住超过 N ms"的时长阈值**：没有可靠的计时起点 —— `State` 由 HAL 改，本层看不到
  它何时被置忙；而且 `I2C_ResetHandle` 每次失败都会把 `State` 复位，采不到"连续非 READY"
  这个量。真要计时就得再引入一份按外设的时间戳，为一条没有观测支撑的判据增状态不值
  （对比 SPI：那边的 `State` 是 HAL 在启动失败分支里**忘了**复位，所以"连续非 READY"本身
  就是故障信号，见 `bsp_spi.md`）。
- **用 `ErrorCode` 判**：它是**上一笔**传输的结果，而上一笔早被 `I2C_AbortOnError` 复位过
  （`I2C_ResetHandle` 会清 `ErrorCode`），读到的多半是 0，不代表当前总线状态。

**与调用点的分工**：drv_ist8310 只管"何时看一眼"（自己的 `fail_count` + 500ms 冷却），
bsp 只管"值不值得动手"。这与 `SPIRecoverTxIfStuck`（`bsp_spi.h`：只把"何时看一眼"交给
上层，判据与动作都在这层）是同一条原则在两条总线上的同一份实现。

**B.2 不用 `HAL_I2C_Master_Abort_IT` 收尾**
它是异步的、依赖 I2C 中断，总线卡死时永远不会完成（等于二次卡死）。
改用同步的 `HAL_DMA_Abort` + 关中断源 + 强写句柄状态；
中断里连 `HAL_DMA_Abort` 都跳过（A.5），留给任务上下文。

**B.3 `I2C_ResetHandle` 里必须把 `Lock` 也置回 `HAL_UNLOCKED`**
`Lock` 残留会让后续所有 HAL 调用直接返回 `HAL_BUSY`，而且从现象上看不出原因。
同理，`I2C_BusIsIdle()` 的判据是 `State == READY && Lock == HAL_UNLOCKED`，
把"HAL 入口因 `Lock` 而返回 `HAL_BUSY`"这种既不置 `ErrorCode` 也不动 `State` 的
情况提前判掉，而不是让 HAL 去返回它。

**B.4 `HAL_DMA_Abort` 不能进中断，光复位句柄字段也救不回硬件 BUSY**

这两条都是"复位软件状态"覆盖不到的角落，处理办法是按上下文分流：

- **`HAL_DMA_Abort` 会死等**：它在 `State==BUSY` 且 DMA stream 的 EN 位不落时，
  用 `HAL_GetTick()` 轮询等 EN 清零（`while(...) + HAL_TIMEOUT_DMA_ABORT`）。
  处理：`I2C_ResetHandle()` 用 `I2C_CanBlockingAbort()` 判定上下文，中断里**跳过** DMA 中止。
  中断里本来也不该有 DMA 在飞的传输——**DMA 模式禁止从 ISR 发起**。
- **硬件 BUSY 不会被软件复位清掉**：HAL 在传输中途出错时，`I2C_FLAG_BUSY` 可能仍置位，
  此时把 `State` 强写成 `READY` 只骗过了 `I2C_WaitReady`，接下来每次调用一进 HAL
  立刻返回 `HAL_BUSY`（F4 的阻塞版还会先在 `I2C_TIMEOUT_BUSY_FLAG` 上白等 25 ms），
  形成"每次调用都失败"的循环。HAL 自己在 `I2C_ITError` / `I2C_DMAAbort` 里用的是
  `__HAL_I2C_DISABLE()`（清 PE）。
  处理：`I2C_AbortOnError()` 在**任务上下文**且 `I2C_FLAG_BUSY` 仍置位时，
  调 `I2C_RebuildPeriph()`（= `HAL_I2C_DeInit` + `HAL_I2C_Init`，与 `I2CBusRecover`
  同一份实现）把外设整个重建一次；正常 NACK 之类总线是空闲的，不做无谓重建。
  中断里不重建（含 MspDeInit/MspInit），交给任务上下文的 `I2CBusRecover`。

准确地说：**中断里只跳过"按 tick 自旋"和"重建外设"这两件事**
（`HAL_DMA_Abort` 与 `HAL_I2C_DeInit/Init`），**软件状态复位照做**
（关中断源 + `State`/`Mode`/`PreviousState`/`ErrorCode`/`Lock` 全部归位）。
后者不只是"收尾"，还是**作废在途传输**的关键一步：HAL 的 ISR 按 `State`/`Mode`
分派完成回调，这两个字段一归位，迟到的完成中断就不会再被分发到本层回调，
上层也就不会被一个"早已超时"的完成事件二次惊动。

剩下没解决的是硬件层面"外设可能还占着总线"（`I2C_FLAG_BUSY`）——那需要重建外设，
只能留在任务上下文（`I2C_AbortOnError` 任务上下文分支，或上层的 `I2CBusRecover`）。
`I2CBusRecover` 既是给上层的公开恢复入口，也是中断路径的兜底终点。

**B.5 句柄 → 实例用登记表，不用"最近命中缓存"**
旧版 `I2C_FindInstanceByHandle` 带一个 `s_i2c_last_route` 缓存（记住上次命中的实例）。
现在改成 `s_i2c_inst_by_i2c[I2C_NUM_MAX]`，`I2CConfig` 时按下标登记：
`i2c_map` 只有两三个表项，线性查找的成本远低于缓存失效的排查成本
（实例重配后缓存会指向旧实例）。`I2CConfig` 还会把旧句柄的路由槽清掉。

**B.6 发起的顺序：先判"总线归我"，再登记现场**
四个收发接口的顺序固定为：

```
参数校验 → DMA 能力校验 → I2C_ClaimBus（不是我就 BSP_BUSY，就此返回）
        → I2C_SnapshotReq（登记 dev/mem/xfer_len/mode）→ rx_len = 0 → 调 HAL
```

`I2C_ClaimBus` 对 BLOCK 也判一次（只判一次、不等待），这是与旧版的唯一行为差别：
旧版 BLOCK 不预判，直接让 HAL 去返回 `HAL_BUSY`。两者**返回值完全相同**
（`I2C_StartFail` 把不带 `ErrorCode` 的 `HAL_BUSY` 也归为 `BSP_BUSY`，
且同样只计 `xfer_fail`/`xfer_busy`、不打日志），差别只在旧版会先执行
`I2C_SnapshotReq` + `rx_len = 0`：`xfer_len` 是在途那笔的完成回调填 `rx_len` 的
唯一依据，被一笔注定失败的调用覆盖后，在途那笔的 `rx_len` 就再也对不上了。
BLOCK 传 `timeout_ms` 给 `I2C_ClaimBus` 是没意义的（那个参数对 BLOCK 是 HAL 的
传输超时），故它在 `I2C_ClaimBus` 里被显式忽略。

不改变"总线真卡死"的识别：那种情况 `State` 已是 `READY`、卡住的是硬件 BUSY 标志，
这一关照样放行，仍由 HAL 在 `I2C_TIMEOUT_BUSY_FLAG` 后带 TIMEOUT 报错、
进 `I2C_AbortOnError` 收尾（B.4）。

### C. 当前处理状态

- POLLING + BLOCK：**已实机验证**（app/half_rudder_gimbal 的 bring-up 测试能读到合理地磁）。
- POLLING + DMA：代码与板级配置均已就绪，**正在实机验证**。
- IT / DRDY 中断模式：板级中断已接出，尚未实跑，见 §4。
- 引脚级总线恢复：未做（见 B.1）。
- 从机地址冲突检测：`I2CRegister` 只防同实例重复注册，不做总线扫描；
  `I2CConfig` 额外拒绝"同一 handle 被两个实例占用"（否则回调分发会串台）。

## 3. drv_ist8310 里与 bsp 相关的约定

**A. 传输模式每次传参，不存在"切模式"一说**
IST8310 没有连续测量模式，每帧都要先写一次 `CNTL1=0x01`。旧版在 IT 模式下写时，
写完成会混进唯一的完成回调，而那个回调被设计成"收到一帧数据就发布"，
于是把上一次读残留的缓冲当新帧发布（时间戳还是错的）—— 当时的权宜之计是
`IST8310_ArmSingle()` 里先 `I2CConfig` 切阻塞、写完再切回 `inst->i2c_mode`。

重构后这条约定消失了：BSP 的模式是每次调用传参的，一次调用要什么语义就传什么模式
——初始化序列（`IST8310_InitDeviceBlocking`）显式传 `BSP_BLOCK_MODE`：
序列里全是"写完马上读回校验"，要的就是同步语义（BLOCK 不产生任何回调）。

**但运行时不是"每个调用点各挑一个模式"，而是统一取实例的 `i2c_mode`**，
所以两处都别记错：

- 同步/异步的区分靠 **`IST8310_TriggerMeas` 的 `wait` 形参**（等不等完成回调），
  不是靠传不同模式。`wait = 1`（任务上下文）走完再返回；`wait = 0`
  （仅由读完成回调调用）发起即返回。
- **绝不能在 `wait = 0` 那条路上传 `BSP_BLOCK_MODE`**。那是在 I2C 事件中断里做阻塞
  I2C：tick 不前进 → 永久自旋（§2.A.5）。`i2c_mode` 在中断模式里被 Config 拒绝取
  `BSP_BLOCK_MODE`，正是为了让这条路径不可能落到阻塞上。

**B. 新传输可以从完成回调发起，但必须"发起即返回"**
与 drv_bmi088「不从完成回调发起」**不同**，这一条在 I2C 侧是反过来的，原因是 bsp 的
完成回调契约不一样：

- **可以发起**：`bsp_i2c` 在派发 `rx_callback`/`tx_callback` 之前已经把 HAL 句柄的
  `State` 复位为 `READY`、总线 `Lock` 已释放，所以从回调里起一笔新传输不会撞上
  `HAL_BUSY`。（SPI 侧不行，是 DMA 流尚未释放，见 bsp_spi.md。）
- **绝不能在回调里等这笔传输完成**：完成回调本身就运行在 `I2Cx_EV_IRQHandler` 里，
  而新写的完成回调要靠**同一个中断**才能进来；同优先级不会重入，在那里等就是**永久自旋**。
  更要命的是 tick 仍在推进，看门狗照常喂，现场表现是"传感器静默、日志里一个错误都没有"。
  故 `IST8310_I2CRxCpltCallback` 续发测量请求时 **`timeout_ms` 必须传 0**。
- **发起即返回 ⇒ 请求字节不能放栈上**：HAL 在传输期间一直持有该指针，
  必须用实例里的常驻缓冲（`IST8310_TriggerMeas` 用 `inst->req_buff`，`DMA_RAM`）。
- **但"能发起"不等于"随便挑模式"**：中断模式的链子**只能配 `BSP_IT_MODE`**
  （`IST8310Config` 强制，BLOCK/DMA 都拒绝）。因为这两笔传输的发起与收尾全在 ISR 里，
  任务侧没有第二处发起入口 —— 也就没有"下一帧顺手补一刀"的机会。这条链唯一的补刀点是
  低频看门狗 + 整段恢复（`IST8310_Recover` → `I2CBusRecover`：器件重初始化 + 必要时
  DeInit/Init 重建整条总线，后者还要先过 §2.B.1.1 的入口自证），代价远超"丢一帧"。
  而 DMA 恰好多引入一个**与总线好坏无关**的
  失败源：DMA 流的 `State` 一旦非 READY，`HAL_DMA_Start_IT` 会让此后每一笔 DMA 都在
  启动阶段直接失败（确定性级联），而这类残留只有 tick 依赖的动作能清
  （`HAL_DMA_Abort` / `DeInit+Init`），在 ISR 里做不了。IT 没有流可留，下一帧能否成功
  只取决于总线是否已放开（NACK / 仲裁丢失之后总线通常是空闲的，链路往往自愈）。
  顺带记两点免得误伤：DMA **传输错误**（TE）时 DMA 中断自己就把流放开了（F4 用
  `SystemCoreClock/9600` 的周期计数做上界，与 tick 无关，ISR 安全），故级联只在
  "启动失败 / 等就绪超时 / 总线卡死"这几类残留上出现；反过来 IT 也不是"零残留"——
  硬件 BUSY 标志它一样可能留，只是少了 DMA 流这一个资源。
  轮询模式没有这个约束：每笔传输都由任务发起，调用方自己就是补刀点，`DMA` 无妨。

自维持链就是这样闭合的：`Config` 发第一条请求 → DRDY EXTI 发起数据读（`timeout_ms=0`）
→ 读完成回调发布数据并**立即续发下一条测量请求**（`timeout_ms=0`）→ ……
于是中断模式不需要任何任务侧"续命"，任务侧 `IST8310Read` 只负责取最新已发布帧。
另有一道门控 `trigger_pending`：请求写在途时挡住 DRDY 边沿，
避免"输出寄存器没被真正读走、DRDY 仍为高"时残留边沿立刻触发起一笔错位读。

**C. INT 模式必须有链路看门狗（DRV 侧，BSP 帮不上）**
本 BSP 只保证"一笔传输要么完成、要么走到 `err_callback`"，**不保证从机侧的状态是干净的**。
IST8310 手册 §3.3 明确：DRDY 引脚**只有读输出寄存器（或 STAT2）才会被拉低**，
测量完成本身不会先把它拉低。于是"一次 IT 读失败"会留下一个 BSP 完全看不见的死局：
数据寄存器没被读走 → DRDY 引脚停在高位 → 下次单次测量虽然按时完成，却**不产生新的上升沿**
→ 边沿触发的 EXTI 再也不进 → 驱动静默返回旧帧直到复位。

处理在 DRV 侧，且判据取**"距上次推进的时长"**（`link_us`：请求发出或一帧发布时刷新；
`IST8310_ReadInt` 里比 `link_timeout_us` = `meas_delay_ms` + 一个 DRDY 超时余量）
而不是某个标志位 —— 链路有好几种死法（续请求发起失败 / 错误回调丢帧 / 边沿丢失 /
IT 传输既不完成也不报错），共同现象都是"不再推进"。旧版只看 `armed`，
请求写失败时 armed=0，看门狗反而不响，恰好漏掉最常见的一种。
动作分两级：先补发一次请求（救得回"请求没发出去"），连续失败攒够 3 次走恢复链，
`IST8310_Recover` 的软复位才会把 DRDY 清回低位（后几种死法只能靠它）。

**加新 I2C 从机驱动时注意：凡"事件引脚 + 边沿触发"的器件都要自己准备这种超时兜底，
BSP 层无从代劳。**

**D. 轮询模式要跑 IT/DMA，得由 DRV 自己把异步等回同步**

BSP 的语义是"异步模式返回 `BSP_OK` 只代表已受理，结果看 `rx_callback`/`tx_callback`"。
这对 `IST8310_Sample` 这种"读完马上要 buf 里的数"的流程是没法直接用的。
处理：DRV 侧保留与 `work_mode` 大致正交的 `i2c_mode`（阻塞/IT/DMA；INT 模式只能取 IT，
见 §3.B），
`IST8310_ReadReg/WriteReg` 在异步模式下**发起前清 `xfer_done`/`xfer_error`、
发起后自旋等回调**，于是上层代码一行不改，三种模式返回语义完全一致。

配套的坑（都在 DRV 侧）：

- **回调按 `work_mode` 分流**：轮询模式下这些传输是 `ReadReg/WriteReg` 自己发的，
  里面既有 STAT1 这种中间读、也有 CNTL1 触发写。所以读完成回调**先记 `xfer_done`，
  再判 `work_mode != INT` 就返回**，只有 INT 模式那一笔才发布磁力帧、才动 `fail_count`
  （否则一次中间读成功就能把真失败攒下的计数清零）。
- **读完成与写完成分成两个回调**：读完成才是"一帧到手"，写完成只表示"请求落盘了"
  （INT 模式下它还是 `trigger_pending` 门控的释放信号）。bsp 按读/写分别派发，
  这个歧义从根上没有了；合用一个入口就得靠调用方临时切模式来分辨。
- **发起前必须清标志**，否则上一笔残留的完成标记会让本笔立刻"成功"返回旧缓冲。
- **超时只记账不做收尾**：此时 HAL 的传输可能还在飞，DRV 无法安全中止它。
  脏标志不会污染下一笔（每笔发起前都清零），真卡死的句柄交给
  `fail_count` 攒够后的 `IST8310_Recover → I2CBusRecover` 重建。
- **`INT` 模式的 `i2c_mode` 被钉死为 `BSP_IT_MODE`**（`IST8310Config` 校验，BLOCK/DMA
  都拒绝）：BLOCK 等于在 DRDY EXTI 里做阻塞 I2C（见 A.5）；DMA 的理由见 §3.B
  ——这条链没有任务侧的发起入口，承受不起 DMA 流的残留。

**E. 子模块的硬件枚举一律由 Config 写入，DRV 不直写实例字段**
`IST8310Config` 曾有一块"预存硬件枚举到子实例"——直接写 `i2c_inst->i2c_e`、
`drdy->gpio_e`、`rstn->gpio_e`。这三行全是冗余的：紧接着的 `I2CConfig` / `GPIOConfig`
就是拿同一份配置值写这三个字段（`bsp_i2c.c` 的 `instance->i2c_e = config->i2c_e`、
`bsp_gpio.c` 的 `instance->gpio_e = config->gpio_e`），中间也没有任何读者。
已整块删除，与 `IST8310Register` 里那条注释（"读直接读、写必须用函数"，
parent 也从直写改成了 Config 的 `.parent`）保持一致 —— 留着这种"Config 之外的写口"，
一旦 Config 将来加校验/拒绝，字段就先被写脏了。

## 4. 板级前置条件与当前状态

三种工作模式能不能跑，取决于 CubeMX 是否把对应中断都接了出来。当前状态：

| 工作模式           | 前置条件（CubeMX）                                                | DJI_C 现状 |
| ------------------ | ----------------------------------------------------------------- | ---------- |
| `BSP_BLOCK_MODE`   | 无                                                                | ✅ 可用    |
| `BSP_IT_MODE`      | 勾该 I2Cx 的 event + error 全局中断，生成 `I2Cx_EV/ER_IRQHandler` | ✅ 已具备  |
| `BSP_DMA_MODE`     | 上一条 + 该 I2C 的 TX/RX DMA 请求与 DMA 流中断                    | ✅ 已具备  |
| IST8310 `MODE_INT` | `BSP_IT_MODE` + DRDY 引脚的 EXTI（**必须配上升沿**，见下）；该模式只走 IT，**不需要 DMA 流** | ✅ 已具备  |

DJI_C 上（I2C3 = IST8310 总线）已确认：

- `i2c.c` 的 `MspInit` 里有 `HAL_NVIC_EnableIRQ(I2C3_EV_IRQn / I2C3_ER_IRQn)`，
  `MspDeInit` 里有对应的 `HAL_NVIC_DisableIRQ`；
- `stm32f4xx_it.c` 里有 `I2C3_EV_IRQHandler` / `I2C3_ER_IRQHandler`，转 `HAL_I2C_*_IRQHandler(&hi2c3)`；
- TX/RX DMA（`hdma_i2c3_tx` / `hdma_i2c3_rx`）已配。
- DRDY 引脚 **PG3 配的是 `GPIO_MODE_IT_RISING`（上升沿触发）**（DJI_A 的 PE3 同样）。
  drv_ist8310 初始化时固定写 `CNTL2.DRP = 1`（高电平有效）与之对应，**极性不做配置项**
  —— 边沿是 CubeMX 里定死的板上事实，两边对不上就是中断风暴或永无中断。

**注意 `MspDeInit` 里的 `HAL_NVIC_DisableIRQ` 与 `I2CBusRecover` 有交互**：
本层的恢复流程走的正是 DeInit → Init，中断会被正确地关掉再重开，不需要额外处理。

DM_MC02（H7）没有任何 I2C 的 NVIC 配置；不过那块板上也没有 `I2C_IST8310` 枚举，
drv_ist8310 在那里只能编译、无法实例化运行，暂不需要。

### 仍然存在的限制

- **不支持从机模式**：见 §1.4。HAL 的从机 API/回调一个都没包，`OwnAddress1` 也没配。
- **`I2CIsDeviceReady` 是阻塞轮询**（HAL 内部用 `HAL_GetTick`），不能进中断。
- `I2CBusRecover` 含 `MspDeInit`/`MspInit`（时钟与 GPIO/NVIC 重配），只能在任务上下文调用；
  中断/临界区里调用会直接返回 `BSP_BUSY`（入口自证，§2.B.1.1），不会硬撑。
- `I2CBusRecover` **不保证每次调用都重建**：总线没被占住时它返回 `BSP_BUSY` 什么都不做
  （同样见 §2.B.1.1）。也就是说"调了恢复"≠"外设已经重来一遍"。
- **DMA 模式禁止从 ISR 发起**：出错时 `I2C_ResetHandle` 在中断里会跳过 DMA 中止
  （原因见 B.4），DMA 就只能等任务上下文的 `I2CBusRecover` 来收。
  注意这条只对"没有任务侧发起入口"的链路才是致命的（`IST8310` 的中断模式即如此，
  已在 `IST8310Config` 里钉死为只能 IT，详见 §3.B）；像 `POLLING + DMA` 这种每笔都由
  任务发起的链路，下一帧调用本身就是补刀点，不受影响。
- 中断上下文里 `I2C_AbortOnError` 会复位全部软件状态（在途传输就此作废），
  但**跳过了 DMA 中止与外设重建**，**硬件 BUSY 不一定被清掉**；
  中断路径的兜底是"DRV 攒够失败次数 → 任务上下文调 `I2CBusRecover`"。
  也就是说中断里的一次失败**不保证当场自愈，但保证不会永久卡住**。
- **同一实例的两个上下文真并发时没有保护**：本层只保证"入口判不过就不碰现场"
  （B.6），判完到调 HAL 之间的窗口、以及 `rx_buff` 被第二笔覆盖，都只能靠调用方
  自己串行化（见 §1.4 的传输互斥）。本层不引入锁 —— 会被从 ISR 调用的接口没法用
  RTOS 互斥量（不能在中断里 Take）。
- 三种模式的**代码都能编过、板级配置也齐了，但尚未实机验证**。

## 5. 后续要做的验证

已做：

1. ~~app 层实例化~~：`app/half_rudder_gimbal/app_gimbal/app_gimbal.c` 里曾有一整块
   临时 bring-up 测试（`IST8310_INSTANCE_DEF` + `IST8310Register` + `IST8310Config`，
   `i2c_e = I2C_IST8310`、`rstn_e = GPIO_IST8310_RSTN`），验证完已删除。
   注意 `IST8310Config` 特意推迟到任务上下文：`function_in_main_c()` 用 `__disable_irq()`
   包住了整个初始化段，而 HAL 的阻塞 I2C 靠 `HAL_GetTick()` 计时，那里 tick 不前进
   → 接线一错就启动死循环。
2. ~~POLLING + BLOCK 读到合理地磁~~：已确认。
3. ~~编译验证本轮重构~~：临时在 `half_rudder_gimbal`(F4/M4) 与 `half_rudder_chassis`
   (H7/M7) 的 `app_cfg.h` 里打开 `BSP_I2C_USED` / `DRV_IST8310_USED`，
   Debug×3 + Release×1 四个目标零警告通过，覆盖两版 HAL 的 `#if CPU_CORE == CORTEX_M7`
   分支（验证完已撤销这两个开关 —— 目前两个模块**默认不参与编译**）。

待做（**需要实机**）：

4. POLLING + DMA 实机验证（看 §1.5 的 `s_i2c_status[]`：`xfer_fail` / `xfer_timeout`
   与 `err_*` 的分类增长）。
5. 中断模式（DRDY 是否真来、`frame_valid` 是否置起）；`CNTL2.DRP=1` 与 PG3 的
   上升沿触发这一对必须一致（换板/改 CubeMX 时重点核对）。
6. 破坏性验证恢复链：拔线 / 短接 SDA 到地，看 `fail_count` 与 `recover_count` 的走向，
   以及 `s_i2c_status[]` 的 `err_*` / `abort_reset` / `bus_rebuild` 增长情况。
   重点核对 §2.B.1.1 的入口自证：
   - **短接 SDA 到地**（总线被占）→ `bus_recover` 与 `bus_rebuild` 都涨，
     `I2CBusRecover` 末尾仍读回 BUSY → 返回 `BSP_HW_ERR`；
   - **拔掉从机**（只是 NACK，总线空闲）→ `bus_recover_skip` 涨、`bus_recover` 不涨，
     恢复靠 DRV 的探测 + RSTN 脉冲走通 —— 这正是"证据作用域对齐"要挡掉的无谓重建。
7. 重开 `BSP_I2C_USED` / `DRV_IST8310_USED` 并把实例接回 app 时，别忘同步开启。

DMA 侧另注意：`I2C_INSTANCE_DEF` 的缓冲已带 `DMA_RAM`，DJI_C 是 F4，
该宏为空、缓冲在普通 SRAM，本就 DMA 可访问。
