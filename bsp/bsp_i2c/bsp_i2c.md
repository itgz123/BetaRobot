# bsp_i2c 开发总结

> 本文件记录 bsp_i2c（`bsp_i2c.h` / `bsp_i2c.c`）接口用法、与 bsp_spi 的差异、
> 踩过的坑与处理方式、以及当前板级配置下**还不能跑**的部分。
> 起因为给 IST8310 三轴磁力计补 I2C 抽象层（原先 `bsp/bsp_iic/` 只是两个 0 字节占位）。

## 1. bsp_i2c 接口使用说明

### 1.1 三个入口

| 接口                              | 作用                              | 可重复调用            |
| --------------------------------- | --------------------------------- | --------------------- |
| `I2C_INSTANCE_DEF(name, buff_sz)` | 静态定义实例 + 收发缓冲           | -                     |
| `I2CRegister(instance)`           | 参数校验 + 防重入 static 管理数组 | 否（重复注册返回 -1） |
| `I2CConfig(instance, config)`     | 填硬件句柄/工作模式/回调          | 是（纯字段赋值）      |

`I2CConfig` 只改实例字段、不碰硬件，因此**可以在运行时反复切换工作模式**
（drv_ist8310 就靠这一点在中断模式里临时切阻塞写完再切回，见 §3.C）。

### 1.2 传输接口签名

```c
int8_t I2CMemRead (I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                   I2C_MemAddrSize_e mem_addr_size, uint16_t len, uint32_t timeout_ms);
int8_t I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                   I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                   uint32_t timeout_ms);
int8_t I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                         uint16_t len, uint32_t timeout_ms);
int8_t I2CMasterReceive (I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                         uint32_t timeout_ms);
int8_t I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                        uint32_t timeout_ms);
int8_t I2CBusRecover   (I2CInstance *instance);
```

- **收数据的接口不带出参**：读到的数据一律落在 `instance->rx_buff[0..len-1]`，
  长度上限是 `I2C_INSTANCE_DEF` 时定的 `buff_size`（超了直接 `-1` 拒绝，不会越界写）。
  这是照着 `bsp_spi` 的做法来的。
- `dev_addr` 是 **8 位形式**（HAL 的约定），用宏换算：`I2C_DEV_ADDR(0x0E) → 0x1C`。
- `mem_addr_size` 用自定义的 `I2C_MEM_ADDR_SIZE_8BIT/16BIT`，
  对应 HAL 的 `I2C_MEMADD_SIZE_*`，避免把 HAL 类型漏给 DRV 层。
- 返回值 0 = 已受理：**阻塞模式=已完成，IT/DMA 模式=已启动**（完成看 `rx_callback`）。

### 1.3 与 bsp_spi 的关键差异：从机地址不进实例

SPI 的"片选"是物理引脚，`bsp_spi` 把它交给 DRV 用 `bsp_gpio` 自己管；
I2C 的"片选"是 7 位从机地址，`bsp_i2c` 把它做成**每次调用传入的参数**。

这么设计的直接原因：同一根 I2C 总线上挂两个从机时，它们的 `handle` 是**同一个**。
若把地址放进 `Config`、按从机各注册一个 `I2CInstance`，`I2CFindInstanceByHandle`
（handle → instance 的查表，回调分发靠它）就无法区分，异步完成回调会派发到错误实例。
实例里只留 `last_dev_addr` / `last_mem_addr` 两个字段供日志与调试查看。

### 1.4 回调约定

| 回调                     | 触发时机                                               |
| ------------------------ | ------------------------------------------------------ |
| `rx_callback(instance)`  | 任一传输正常完成（含只写不读的传输）                   |
| `err_callback(instance)` | 启动失败 / 等就绪超时 / HAL 报错（**不会有完成回调**） |

**`err_callback` 是必须挂的**，不是可选装饰：一次传输若"没发起成功"，
完成回调永远不会来，DRV 层若只依赖完成回调清自己的传输标志，就会永久卡在 busy
（现象是数据不再更新、看似整个器件失联）。drv_bmi088 的 `BMI088_SPIErrCallback`
与 drv_ist8310 的 `IST8310_I2CErrCallback` 都是为此存在。

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
tick 中断优先级低于 EXTI，在中断上下文里 tick 不前进，任何阻塞 I2C 调用
要么立刻超时、要么卡死。**所有阻塞 I2C 只能在任务上下文调用**；
中断里只允许发起 IT/DMA 传输。`I2CMemRead` 自带的"等就绪"循环也是 busy-wait，
在 ISR 里同样要避免长时间等待（本层靠 `timeout_ms` 兜底）。

### B. 设计取舍

**B.1 恢复只做外设级，不翻转 SCL/SDA 引脚**
`I2CBusRecover()` = `I2C_ResetHandle()` + `HAL_I2C_DeInit()` + `HAL_I2C_Init()` + 兜底复位。

- `DeInit` 把 `State` 置 `RESET` 并调 `MspDeInit`；
- `Init` 见 `State==RESET` 会重新 `MspInit`（重开时钟、重配 GPIO/NVIC），
  并用 `hi2c->Init` 里的原始值重写外设寄存器 —— **F4 的 `ClockSpeed/DutyCycle`
  与 H7 的 `Timing` 字段差异因此自动抹平，BSP 不必区分平台**。

不做引脚翻转（bit-banging 9 个时钟脉冲）意味着：从机把 SDA 一直拉死时本层救不回来，
`I2CBusRecover` 会在结束时读 `I2C_FLAG_BUSY`，仍置位就返回 -1，由 DRV 记 ERROR 并冷却重试。
真解脱需要后续在 `bsp_map` 里给该总线加两根 GPIO，或依赖从机自己的 RSTN 引脚
（drv_ist8310 的恢复流程会顺带脉冲一次 RSTN）。

**B.2 不用 `HAL_I2C_Master_Abort_IT` 收尾**
它是异步的、依赖 I2C 中断，总线卡死时永远不会完成（等于二次卡死）；
且 `I2C_AbortOnError` 本身可能运行在错误回调的中断上下文里。改用同步的
DMA `HAL_DMA_Abort` + 关中断 + 强写句柄状态。

**B.3 `I2C_ResetHandle` 里必须把 `Lock` 也置回 `HAL_UNLOCKED`**
`Lock` 残留会让后续所有 HAL 调用直接返回 `HAL_BUSY`，而且从现象上看不出原因。

**B.4 `HAL_DMA_Abort` 不能进中断，光复位句柄字段也救不回硬件 BUSY**

这两条都是"复位软件状态"覆盖不到的角落，处理办法是按上下文分流：

- **`HAL_DMA_Abort` 会死等**：它在 `State==BUSY` 且 DMA stream 的 EN 位不落时，
  用 `HAL_GetTick()` 轮询等 EN 清零（`while(...) + HAL_TIMEOUT_DMA_ABORT`）。
  而 tick 中断优先级低于 EXTI，**在中断上下文里 `HAL_GetTick()` 根本不前进**，
  那个 `while` 就是死循环。
  处理：`I2C_ResetHandle()` 用 `__get_IPSR()` 判定上下文，中断里**跳过** DMA 中止。
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

也就是说：**中断里只做"轻量收尾"，能真正松开总线的重建一律留在任务上下文**。
`I2CBusRecover` 既是给上层的公开恢复入口，也是中断路径的兜底终点。

### C. 当前处理状态

- POLLING + BLOCK：**已实机验证**（app/half_rudder_gimbal 的 bring-up 测试能读到合理地磁）。
- POLLING + DMA：代码与板级配置均已就绪，**正在实机验证**。
- IT / DRDY 中断模式：板级中断已接出，尚未实跑，见 §4。
- 引脚级总线恢复：未做（见 B.1）。
- 从机地址冲突检测：`I2CRegister` 只防同实例重复注册，不做总线扫描。

## 3. drv_ist8310 里与 bsp 相关的三处约定

**A. 中断模式下"触发下一次测量"必须临时切阻塞**
IST8310 没有连续测量模式，每帧都要先写一次 `CNTL1=0x01`。若在 IT 模式下写，
写完成会进 `IST8310_I2CCpltCallback`，而那个回调被设计成"收到一帧数据就发布"，
于是把上一次读残留的缓冲当新帧发布（时间戳还是错的）。
处理：`IST8310_ArmSingle()` 里 `I2CConfig` 切 `I2C_BLOCK_MODE` 写完再切回 `inst->i2c_mode`
—— 正因为 `I2CConfig` 是纯字段赋值，这个切换才有意义且几乎零成本。
调用点由 `!armed && !transfer_busy` 保护，不会有在途传输被打断。
（触发写不到 1ms，临时阻塞一下无所谓；真正占时间的"等测量 + 读六字节"仍走配置的传输模式。）

**B. 新传输只从 DRDY EXTI 与任务上下文发起，不从完成回调发起**
与 drv_bmi088 同一条约定。I2C 侧的原因与 SPI 侧略有不同：完成回调运行在
I2C 事件中断里，此时 HAL 句柄的 `State` 虽已置回 `READY`，但若在此发起下一笔，
撞上 `HAL_BUSY` 就会把驱动永久卡住（`I2C_AbortOnError` 正是为兜这种情况而写）。
`IST8310_I2CCpltCallback` 因此**只记账发布，不发传输**。

**C. INT 模式必须有 `armed` 超时看门狗（DRV 侧，BSP 帮不上）**
本 BSP 只保证"一笔传输要么完成、要么走到 `err_callback`"，**不保证从机侧的状态是干净的**。
IST8310 手册 §3.3 明确：DRDY 引脚**只有读输出寄存器（或 STAT2）才会被拉低**，
测量完成本身不会先把它拉低。于是"一次 IT 读失败"会留下一个 BSP 完全看不见的死局：
数据寄存器没被读走 → DRDY 引脚停在高位 → 下次单次测量虽然按时完成，却**不产生新的上升沿**
→ 边沿触发的 EXTI 再也不进 → 驱动静默返回旧帧直到复位。
处理在 DRV 侧：`IST8310ReadInt` 里用 `arm_timeout_us`（= `meas_delay_ms` + 一个 DRDY 超时余量）
兜底，超时即计一次失败、攒够 3 次走恢复链，`IST8310_Recover` 的软复位会把 DRDY 清回低位。
**加新 I2C 从机驱动时注意：凡"事件引脚 + 边沿触发"的器件都要自己准备这种超时兜底，
BSP 层无从代劳。**

**D. 轮询模式要跑 IT/DMA，得由 DRV 自己把异步等回同步**

BSP 的语义是"异步模式返回 0 只代表已受理，结果看 `rx_callback`"。这对
`IST8310_SampleBlocking` 这种"读完马上要 buf 里的数"的流程是没法直接用的。
处理：DRV 侧加一个与 `work_mode` 正交的 `i2c_mode`（阻塞/IT/DMA），
`IST8310_ReadReg/WriteReg` 在异步模式下**发起前清 `xfer_done`/`xfer_error`、
发起后自旋等回调**，于是上层代码一行不改，三种模式返回语义完全一致。

配套的坑（都在 DRV 侧）：

- **回调必须按 `work_mode` 分流**。轮询模式下这些传输是 `ReadReg/WriteReg` 自己发的，
  里面既有 STAT1 这种中间读、也有 CNTL1 触发写。若完成回调照旧"收到数据就发布成磁力帧"，
  会把状态寄存器的值当磁场，还会顺手把 `fail_count` 清零。所以完成/错误回调
  **先记 `xfer_done`/`xfer_error`，再判 `work_mode != INT` 就返回**。
- **发起前必须清标志**，否则上一笔残留的完成标记会让本笔立刻"成功"返回旧缓冲。
- **超时只记账不做收尾**：此时 HAL 的传输可能还在飞，DRV 无法安全中止它。
  脏标志不会污染下一笔（每笔发起前都清零），真卡死的句柄交给
  `fail_count` 攒够后的 `IST8310_Recover → I2CBusRecover` 重建。
- **`INT + BLOCK` 组合直接拒绝**：那等于在 DRDY EXTI 里做阻塞 I2C（见 A.5）。

## 4. 板级前置条件与当前状态

三种工作模式能不能跑，取决于 CubeMX 是否把对应中断都接了出来。当前状态：

| 工作模式           | 前置条件（CubeMX）                                                | DJI_C 现状 |
| ------------------ | ----------------------------------------------------------------- | ---------- |
| `I2C_BLOCK_MODE`   | 无                                                                | ✅ 可用    |
| `I2C_IT_MODE`      | 勾该 I2Cx 的 event + error 全局中断，生成 `I2Cx_EV/ER_IRQHandler` | ✅ 已具备  |
| `I2C_DMA_MODE`     | 上一条 + 该 I2C 的 TX/RX DMA 请求与 DMA 流中断                    | ✅ 已具备  |
| IST8310 `MODE_INT` | 上面 + DRDY 引脚的 EXTI，**边沿必须与 `drdy_polarity` 一致**      | ✅ 已具备  |

DJI_C 上（I2C3 = IST8310 总线）已确认：

- `i2c.c` 的 `MspInit` 里有 `HAL_NVIC_EnableIRQ(I2C3_EV_IRQn / I2C3_ER_IRQn)`，
  `MspDeInit` 里有对应的 `HAL_NVIC_DisableIRQ`；
- `stm32f4xx_it.c` 里有 `I2C3_EV_IRQHandler` / `I2C3_ER_IRQHandler`，转 `HAL_I2C_*_IRQHandler(&hi2c3)`；
- TX/RX DMA（`hdma_i2c3_tx` / `hdma_i2c3_rx`）已配。
- DRDY 引脚 **PG3 配的是 `GPIO_MODE_IT_RISING`（上升沿触发）**，
  因此 `IST8310_Config_s::drdy_polarity` **必须填 `IST8310_DRDY_ACTIVE_HIGH`**
  （对应 CNTL2.DRP=1）。填成 `ACTIVE_LOW` 会导致中断风暴或永无中断。

**注意 `MspDeInit` 里的 `HAL_NVIC_DisableIRQ` 与 `I2CBusRecover` 有交互**：
本层的恢复流程走的正是 DeInit → Init，中断会被正确地关掉再重开，不需要额外处理。

DM_MC02（H7）没有任何 I2C 的 NVIC 配置；不过那块板上也没有 `I2C_IST8310` 枚举，
drv_ist8310 在那里只能编译、无法实例化运行，暂不需要。

### 仍然存在的限制

- **`I2CIsDeviceReady` 是阻塞轮询**（HAL 内部用 `HAL_GetTick`），不能进中断。
- `I2CBusRecover` 含 `MspDeInit`/`MspInit`（时钟与 GPIO/NVIC 重配），只能在任务上下文调用。
- **DMA 模式禁止从 ISR 发起**：出错时 `I2C_ResetHandle` 在中断里会跳过 DMA 中止
  （原因见 B.4），DMA 就只能等任务上下文的 `I2CBusRecover` 来收。
- 中断上下文里 `I2C_AbortOnError` 只做轻量收尾，**硬件 BUSY 不一定被清掉**；
  中断路径的兜底是"DRV 攒够失败次数 → 任务上下文调 `I2CBusRecover`"。
  也就是说中断里的一次失败**不保证当场自愈，但保证不会永久卡住**。
- 三种模式的**代码都能编过、板级配置也齐了，但尚未实机验证**。

## 5. 后续要做的验证

已做：

1. ~~app 层实例化~~：`app/half_rudder_gimbal/app_gimbal/app_gimbal.c` 里已有一整块
   临时 bring-up 测试（`IST8310_INSTANCE_DEF` + `IST8310Register` + `IST8310Config`，
   `i2c_e = I2C_IST8310`、`rstn_e = GPIO_IST8310_RSTN`）。注意 `IST8310Config` 特意推迟到
   任务上下文：`function_in_main_c()` 用 `__disable_irq()` 包住了整个初始化段，
   而 HAL 的阻塞 I2C 靠 `HAL_GetTick()` 计时，那里 tick 不前进 → 接线一错就启动死循环。
2. ~~POLLING + BLOCK 读到合理地磁~~：已确认。
3. POLLING + DMA 实机验证（看 `g_ist8310_test_ok/fail/us`，见 app 内注释）。
4. 中断模式（DRDY 是否真来、`frame_valid` 是否置起）；`drdy_polarity` 必须与
   PG3 的上升沿触发一致。
5. 破坏性验证恢复链：拔线 / 短接 SDA 到地，看 `fail_count` 与 `recover_count` 的走向，
   以及 `I2CBusRecover` 是否被 `I2C_FLAG_BUSY` 判定为"被拉死"。

验证完记得删掉 app 里那块临时测试代码。DMA 侧另注意：`I2C_INSTANCE_DEF` 的缓冲已带
`DMA_RAM`，DJI_C 是 F4，该宏为空、缓冲在普通 SRAM，本就 DMA 可访问。
