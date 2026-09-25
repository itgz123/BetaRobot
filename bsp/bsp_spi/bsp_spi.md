# bsp_spi 开发总结

> 本文件记录 bsp_spi（`bsp_spi.h` / `bsp_spi.c`，公共件见 `bsp/bsp_common/bsp_common.h`）
> 的接口用法、踩过的坑与处理方式、板级前置条件及原因。
> 本模块是 bsp_usart 重构模板在第二个协议上的落地，两者章节结构对齐（`bsp_usart.md` §1~§7）。

## 1. bsp_spi 接口使用说明

### 1.1 对外入口

| 接口                                                       | 作用                                        | 可重复调用                        |
| ---------------------------------------------------------- | ------------------------------------------- | --------------------------------- |
| `SPI_INSTANCE_DEF(name, buff_sz)`                          | 静态定义实例 + 接收缓冲                     | -                                 |
| `SPIRegister(instance)`                                    | 参数校验 + 防重 + 加入 static 管理数组      | 否（重复注册返回 `BSP_PARAM_ERR`）|
| `SPIConfig(instance, config)`                              | 填硬件句柄 / parent / 三个回调，登记路由    | 是（重入会中止在途传输）          |
| `SPITransmit(instance, data, len, mode, ms)`               | 只发不收（三模式每次传参）                  | 是                                |
| `SPIReceive(instance, len, mode, ms)`                      | 只收不发（三模式每次传参）                  | 是                                |
| `SPITransmitReceive(instance, tx_data, len, mode, ms)`     | 全双工（三模式每次传参）                    | 是                                |
| `SPIRecoverTxIfStuck(instance, stuck_ms)`                  | 传输卡死自恢复（判据/计时/动作都在 bsp 内） | 是（幂等，任务上下文，见 §3）     |

除此之外**不对外提供任何东西**：没有 `is_ready`、没有函数指针表、没有内部发送队列 / 缓冲池、
没有 `work_mode`。忙由 `BSP_BUSY` 表达；卡死自恢复只有一个入口，"什么时候看一眼"由上层定，
"看到什么算卡死、卡死了做什么"全在 bsp 内。

### 1.2 统一状态码与模式（`bsp/bsp_common/bsp_common.h`）

```c
typedef enum : int8_t  { BSP_OK = 0, BSP_BUSY = -1, BSP_TIMEOUT = -2,
                         BSP_PARAM_ERR = -3, BSP_HW_ERR = -4 } BSP_Status_e;
typedef enum : uint8_t { BSP_BLOCK_MODE = 0, BSP_IT_MODE = 1, BSP_DMA_MODE = 2 } BSP_Transfer_Mode_e;
```

与 bsp_usart 完全相同的一套（含 `BSP_Timeout_s`，内部 DWT 64 位微秒）：
成功为 0、错误为负；超时统一用 DWT 计时，**不用 `HAL_GetTick()`**。

### 1.3 三种模式

| 模式             | 发送                      | 接收                      | 全双工                               |
| ---------------- | ------------------------- | ------------------------- | ------------------------------------ |
| `BSP_BLOCK_MODE` | `HAL_SPI_Transmit`，返回即发完 | `HAL_SPI_Receive`，返回即收满 | `HAL_SPI_TransmitReceive`，返回即完成 |
| `BSP_IT_MODE`    | `HAL_SPI_Transmit_IT`     | `HAL_SPI_Receive_IT`      | `HAL_SPI_TransmitReceive_IT`         |
| `BSP_DMA_MODE`   | `HAL_SPI_Transmit_DMA`    | `HAL_SPI_Receive_DMA`     | `HAL_SPI_TransmitReceive_DMA`        |

- 模式**每次调用传参**（旧版在 `SPI_Config_s` 里固定，调用期切不了）。
- `timeout_ms` 语义（与 uart 一致）：IT/DMA 模式用于**等总线就绪**，`0` = 只判一次，忙即 `BSP_BUSY`；
  `>0` = 轮询等就绪，超时 `BSP_TIMEOUT`（此时 bsp 已在任务上下文强制复位卡死状态）。
  BLOCK 模式不做就绪预判（HAL 阻塞版自己会等），该值**直接透传**给 HAL。
- **上下文**：BLOCK 内部按 tick 自旋，**禁止在中断上下文调用**；IT/DMA 可在中断里发起，
  但中断里必须传 `timeout_ms = 0` —— 卡死复位要等 HAL tick，中断里做不了（见 §3.3）。

### 1.4 缓冲区与生存期

| 场景       | 约束                                                                   |
| ---------- | ---------------------------------------------------------------------- |
| TX IT/DMA  | `data` / `tx_data` 指向的缓冲须**生存到 `tx_callback` 返回**（DMA 直接读它）|
| TX BLOCK   | 只在调用期内有效                                                       |
| RX         | 结果固定写入 `SPIConfig` 登记的 `rx_buff`（`SPI_INSTANCE_DEF` 静态分配，H7 上自动 `DMA_RAM`）|
| 长度上限   | `len` 一律 ≤ `buff_size`：**超出直接 `BSP_PARAM_ERR` 拒绝，不静默截断**（见 §1.8）|
| 多缓冲/排队 | **BSP 不做**，由上层自持                                             |

片选（CS）不由本层管理：SPI 的"片选"是物理引脚，和 I2C 的"从机地址每次调用传参"是同一分工 ——
**BSP 只管总线，选谁由 DRV 用 `bsp_gpio` 自己管**。因此 `SPITransmitReceive` 的发起与 CS
拉低/拉高必须由 DRV 保证原子成对（BMI088 在 DRDY EXTI 里先 `GPIOReset(cs)` 再发）。

### 1.5 回调契约

```c
void (*rx_callback)(SPIInstance *);   // ISR：接收 / 全双工完成（rx_buff、rx_len 有效）
void (*tx_callback)(SPIInstance *);   // ISR：IT/DMA 纯发送完成
typedef enum { SPI_ERR_HW, SPI_ERR_ABORT } SPI_ErrReason_e;
typedef void (*SPI_ErrCallback)(SPIInstance *, SPI_ErrReason_e);  // ISR 或任务上下文
```

- `parent` 由 **Config 写入**（DRV 不再直写 `spi_inst->parent`），回调里用它取回上层实例。
- **`err_callback` 是必须挂的，不是可选装饰**：一次传输若"没发起成功"或"中途出错"，
  完成回调永远不会来，DRV 若只依赖完成回调清自己的传输标志，就会永久卡在 busy
  （现象：数据不再更新、看似整个器件失联）。drv_bmi088 的 `BMI088_SPIErrCallback` 就为此存在。
- 两种 `reason` 的上下文不同，handler 必须对两者都安全：

  | reason          | 上下文   | 触发者                                    | 含义                                         |
  | --------------- | -------- | ----------------------------------------- | -------------------------------------------- |
  | `SPI_ERR_HW`    | ISR      | `HAL_SPI_ErrorCallback`                   | HAL 报硬件错；此时传输已被 HAL 收尾、不会再回调 |
  | `SPI_ERR_ABORT` | **任务** | `SPI_RecoverTx`（卡死自恢复 / Config 重入）| 在途传输被强止，`hspi->State` 已复位为 READY |

- handler 要求：**无阻塞、可重入、幂等**，且不得在其中调 `SPITransmit` / `SPIReceive` / `SPIConfig`。
  置标志、释放片选、清 `transfer_busy` 这类动作安全；`drv_bmi088` 两种 reason 的处理相同
  （都是丢弃本次传输），故收在 `BMI088_SpiAbort` 里，`(void)reason`。
- **发起失败（`SPITransmit*` 返回非 `BSP_OK`）不走 `err_callback`** —— 调用方当场就知道失败了。
  `err_callback` 只负责"已经启动、随后出事"的传输。两者的收尾动作通常相同，故 DRV 把它抽成
  一个共用的 abort 函数（`BMI088_SpiAbort`），发起失败分支与错误回调各调一次。
- 与旧版的一个语义变化：`HAL_SPI_TxCpltCallback` **不再并入 `rx_callback`**，改走 `tx_callback`。
  纯发送没有接收结果，混在一起会让上层分不清"收完了"和"发完了"（旧版还顺带把 `rx_len` 置 0）。
  本工程目前只发不收的 SPI 实例不存在，此改动不影响 BMI088。

### 1.6 对外接口速查

```c
BSP_Status_e SPIRegister(SPIInstance *instance);
BSP_Status_e SPIConfig  (SPIInstance *instance, const SPI_Config_s *config);

BSP_Status_e SPITransmit       (SPIInstance *instance, const uint8_t *data, uint16_t len,
                                BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e SPIReceive        (SPIInstance *instance, uint16_t len,
                                BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e SPITransmitReceive(SPIInstance *instance, const uint8_t *tx_data, uint16_t len,
                                BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/* 传输卡死入口：DRV 在自己任务上下文的读入口每周期调一次（见 §3.4） */
BSP_Status_e SPIRecoverTxIfStuck(SPIInstance *instance, uint32_t stuck_ms);
/* retval BSP_OK   = 本次刚复位了一次卡死的传输（err_callback 已以 SPI_ERR_ABORT 通知过）
 *        BSP_BUSY = 未动作：总线空闲（顺带刷新计时基准）/ 未到阈值 / 当前上下文不允许 Abort
 *        BSP_PARAM_ERR = 实例或句柄为空 */
```

典型用法：

```c
SPI_INSTANCE_DEF(my_spi, 16);
static const SPI_Config_s cfg = {
    .spi_e = SPI_BMI088, .parent = &my_inst,
    .rx_callback = MyRxCplt, .tx_callback = NULL, .err_callback = MyErr,
};
SPIRegister(&my_spi);
SPIConfig(&my_spi, &cfg);

/* 中断里发起：只判一次就绪，忙即放弃本次 */
GPIOReset(cs);
if (SPITransmitReceive(&my_spi, tx, len, BSP_DMA_MODE, 0) != BSP_OK)
    MyAbort();                       /* 发起失败不会有完成回调 */

/* 任务上下文里每周期一次：卡死自恢复 */
(void)SPIRecoverTxIfStuck(&my_spi, 0);
```

### 1.7 返回值一览

| 返回             | 含义                                                                      |
| ---------------- | ------------------------------------------------------------------------- |
| `BSP_OK`         | BLOCK = 已完成；IT/DMA = 已受理并启动，完成看 `rx_callback` / `tx_callback` |
| `BSP_BUSY`       | 总线忙（`timeout_ms == 0` 时的常态，上层应排队重试）。含两类窗口：①**"上一笔的 DMA 流尚未完全释放"**——该窗口由本层拦下，不会让 HAL 报错并锁死句柄（§2 A.6）；② HAL 启动时判"State 非 READY"而返回的 `HAL_BUSY`（HAL 不动 State、不置 ErrorCode，是可重试的争用；BLOCK 模式不做就绪预判，这类返回尤其常见）|
| `BSP_TIMEOUT`    | 等就绪超时（任务上下文下 bsp 已强制中止卡死的传输并复位状态，可直接重试）  |
| `BSP_PARAM_ERR`  | 参数非法 / `len` 为 0 或越界 / `mode` 非法 / 该口没有对应方向的 DMA        |
| `BSP_HW_ERR`     | HAL 启动传输**真失败**（HAL 已置 `ErrorCode`：DMA 流没释放、MODF/OVR… 或 BLOCK 超时）|

`BSP_BUSY` **不是错误**，是流控信号：上层要么丢、要么排队，下一轮再来。

### 1.8 相对旧版的破坏性变更

| 旧                                              | 新                                                          |
| ----------------------------------------------- | ----------------------------------------------------------- |
| `SPI_Work_Mode_e` + Config 期固定模式           | 删除；模式每次调用传参 `BSP_Transfer_Mode_e`                |
| 三张函数指针表（`transmit_receive_funcs` 等）    | 删除；改 `switch-case` 直接分派                             |
| 接口返回 `void`                                 | 统一返回 `BSP_Status_e`                                     |
| `len > buff_size` 静默截断                      | 一律 `BSP_PARAM_ERR` 拒绝（截断会让上层拿到长度不符的数据却毫无察觉）|
| `SPI_AbortOnError`（内部，用 H7 的空实现）       | 删除；改 `SPI_RecoverTx`（用 `HAL_SPI_Abort`）+ `SPIRecoverTxIfStuck` |
| 无错误回调                                      | 新增 `err_callback` + `SPI_ErrReason_e`                     |
| `HAL_SPI_TxCpltCallback` 走 `rx_callback`       | 改走独立的 `tx_callback`（纯发送不再伪装成"收到数据"）        |
| `parent` 由 DRV 直写                            | 由 `SPI_Config_s.parent` 写入                               |
| `s_spi_last_route`（最近命中缓存）              | `s_spi_inst_by_spi[]`（Config 时按下标登记，重配不会指向旧实例）|
| 无状态/计数                                     | `volatile SPI_Status_s s_spi_status[SPI_NUM_MAX]`（§5）     |

## 2. 之前踩过的坑与处理

### A. 硬件 / HAL 坑

**A.1 H7 的 `HAL_SPI_DMAStop` 是空实现**
它只 `SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_NOT_SUPPORTED)` 就 `return HAL_ERROR`
（见 `stm32h7xx_hal_spi.c`），**什么都不做**。旧版 `SPI_AbortOnError` 的 DMA 分支调的正是它，
于是真正起作用的只剩手写那句 `hspi->State = READY` —— 软件状态复位了，DMA 流还开着、
外设还停在 BUSY。处理：中止统一改用 `HAL_SPI_Abort`（两版 HAL 都实现，且会把 DMA 流与状态
一起收拾干净）。**看到 `HAL_SPI_DMAStop` 一律当"不存在"处理。**

**A.2 `HAL_SPI_Transmit_DMA` / `TransmitReceive_DMA` 启动失败时不复位 `State`**
`HAL_DMA_Start_IT` 返回 `HAL_BUSY`（例如另一路 DMA 流的完成中断尚未执行、DMA 流还占着）时，
HAL 只是 `SET_BIT(hspi->ErrorCode, HAL_SPI_ERROR_DMA); return HAL_ERROR;`，
**句柄被永久留在 `HAL_SPI_STATE_BUSY_TX_RX`**。此后每次传输都拿不到总线、等就绪永远超时，
DRV 也等不到完成回调、自己的 `transfer_busy` 永远清不掉 —— **整个从机失联且看不到任何错误**。
处理：`SPI_WaitReady` 超时路径与 `SPIRecoverTxIfStuck` 都会把状态复位回 READY（§3）。
这条是本次重构最核心的动因。

**A.3 旧版 `SPI_WaitReady` 在 `timeout_ms == 0` 时会死循环**
旧写法 `while (State != READY) { if (timeout_ms > 0 && 超时) {...} }` —— 超时判据被
`timeout_ms > 0` 短路，0 时永远不成立。而 INT 模式的传输恰恰是从 DRDY EXTI 里发起的，
一旦总线卡死就是**中断内永久死等**。处理：统一为"0 即不等待"，非 READY 立刻返回 `BSP_BUSY`。

**A.4 不能在 ISR / 临界区里做会自旋的 Abort**
`HAL_SPI_Abort` 内部对 DMA 流的收尾调的是阻塞版 `HAL_DMA_Abort`，按 `HAL_GetTick()` 自旋等
"流真的停下来"；F4 版 `HAL_SPI_Abort` 自带的计数器轮询同样要等 DMA 停。**两类上下文里 tick 都不前进**：
① 中断上下文（`IPSR != 0`）—— BMI088 的 INT 模式正是在 DRDY EXTI 里发起传输；
② 临界区 —— `taskENTER_CRITICAL` 抬的是 BASEPRI，SysTick 进不来。
处理：`SPI_CanBlockingAbort()` 同时查 IPSR / PRIMASK / BASEPRI，不允许时**什么都不做**，
且调用方不刷新卡死判据的时间戳 —— 于是下一次任务上下文的调用自动补做真正的复位。

**A.5 两套 HAL 的 SPI 错误位有差集**
F4 的 `0x80` 是 `HAL_SPI_ERROR_INVALID_CALLBACK`，H7 的 `0x80` 是 `HAL_SPI_ERROR_UDR` ——
位域不同。处理：分类计数只用**两版共有且判据一致**的交集
（`SPI_ERROR_COMMON_MASK` = MODF | CRC | OVR | FRE | DMA | FLAG | ABORT），
其余一律进 `err_other`。分类计数必须用**独立 `if`**（多个位可同时置位，`else-if` 会漏计）。

**A.6 「完成回调来了」≠「DMA 流已经释放」——实机上的 `SPI ... start failed` 就是这个窗口**
（2026-09 实机日志：`SPI transmit/receive start failed (spi_e=0, mode=2)` 之后恰好 ~21ms 出现
`SPI stuck >20ms, state reset`，反复出现。）

F4 的 `HAL_SPI_TransmitReceive_DMA` 有两条不对称设计（读 `stm32f4xx_hal_spi.c` 可核对）：

- 它是**先启动 RX 流、再启动 TX 流**，且把 TX 流的 `XferCpltCallback` 显式置为 `NULL`，
  源码注释写明收尾全在 RX 完成回调里做（"the communication closing is performed in DMA
  reception complete callback"）。也就是 `HAL_SPI_TxRxCpltCallback` **由 RX 流的中断调用**。
- TX 流的 TC 中断虽然仍会让 `HAL_DMA_IRQHandler` 把 TX 流置回 READY/解锁，但它是**另一次中断**。
  两个流在 DJI_C 上同优先级（都是 5,0，不能互相抢占），同时 pending 时按 IRQ 号服务，
  **RX(DMA2_Stream0, IRQ 56) 先于 TX(DMA2_Stream3, IRQ 59)**。

于是存在一个窗口：**DRV 已经在 `rx_callback` 里清掉 `transfer_busy`（宣布总线可用），
而 TX 流的收尾中断还没执行**（`hdmatx->State` 仍是 `BUSY_TX_RX`、`Lock` 仍是 `HAL_LOCKED`）。
窗口内若 DRDY EXTI 抢进来发起新传输：

```
HAL_SPI_TransmitReceive_DMA
  ├─ HAL_DMA_Start_IT(hdmarx)  ← 已释放，成功
  ├─ SET_BIT(CR2, RXDMAEN)
  └─ HAL_DMA_Start_IT(hdmatx)  ← 拿到 HAL_BUSY（F4 判据就是 hdma->State == READY）
        → SET_BIT(ErrorCode, HAL_SPI_ERROR_DMA); return HAL_ERROR;   ← 且**不复位 hspi->State**
```

后果比"这一笔失败"严重得多：句柄被留在 `BUSY_TX_RX`，此后每一拍 `SPI_WaitReady` 都返回
`BSP_BUSY`（`timeout_ms = 0`），采样白丢，直到 20ms 后 `SPIRecoverTxIfStuck` 才复位 ——
日志上是"失败 + 21ms 后的 stuck reset"，现象上是每几秒丢一段数据。

**处理**：把"能不能发起"的判据从 `hspi->State == READY` 扩展成
**`SPI_BusIsIdle()`**（外设 + 两路 DMA 流的 `State`/`Lock` 都空闲），不满足就返回 `BSP_BUSY`，
**根本不进 HAL** —— 于是既不会产生 `HAL_SPI_ERROR_DMA`，也不会弄脏 `hspi->State`，
下一拍 DRDY 重试即可（代价是丢这一拍采样，远好于 20ms 静默 + 日志刷屏）。

配套两点，缺一不可：

- `SPIRecoverTxIfStuck` 的"健康"判据必须**用同一个 `SPI_BusIsIdle`**。否则"外设 READY 但
  DMA 流卡住"会两头不讨好：收发侧永远 `BSP_BUSY`，自恢复侧却以为健康而不断刷新计时基准
  → **永久静默**，比不修还糟。
- `SPI_RecoverTx` 在 `HAL_SPI_Abort` 之后补一次 `SPI_ForceReleaseDma()`：`HAL_SPI_Abort`
  只 abort **CR2 里 DMA 请求已置位**的流，而上面那种失败恰恰是 **TX 流启动失败**（`TXDMAEN`
  要到函数末尾才置位，此时根本没置位）→ Abort 会**跳过 TX 流**，句柄表面复位、下一笔照样失败。
  `SPI_ForceReleaseDma` 逐路把仍忙的流 `HAL_DMA_Abort` + 清 EN + 解锁 + 置 READY。

**通用教训**：HAL 的完成回调只保证"外设把这一笔交了"，不保证"它用的 DMA 流/锁已经清干净"。
凡是要在回调之后紧接着复用同一硬件的地方，都要按硬件状态（而不是回调）判可用性。

**A.7 完成回调里该用哪个长度：`hspi->RxXferSize`**
HAL 在启动传输时写入 `RxXferSize`，传输过程中只递减 `RxXferCount`，**因此回调里它仍是本次
请求的长度**。用它可以省掉在实例里再存一份"本次长度"，避免多一处可能与 HAL 不一致的副本。
（旧版用的是自己缓存的 `last_xfer_len`，已删除。）

### B. 设计取舍

**B.1 为什么用 `switch-case` 取代函数指针表**
三张表（发送 / 接收 / 全双工）把"模式 → HAL 函数"的映射藏进全局数组，既占 RAM 又让
`mode` 非法时的行为不可见（越界取到别的东西）。改成 `switch` 后 `default` 分支明确返回
`BSP_PARAM_ERR`，且编译期就能看到全部分派。

**B.2 为什么 `SPIConfig` 重入时要先中止在途传输**
否则旧传输会继续往（可能已被复用的）`rx_buff` 里写，还占着总线让新配置一次都发不出去。
重入时先 `SPI_RecoverTx` 收尾 + 清旧路由槽，再写新字段 —— 因此**不要在该实例的回调内调
`SPIConfig`**。

**B.3 为什么保留 `buff_size` 的"超长即拒绝"**
旧版截断成缓冲大小。截断后上层拿到的数据长度与请求不符却毫无察觉（`rx_len` 还会被回调写成
真实请求长度），排查极其困难。缓冲不够就把 `SPI_INSTANCE_DEF` 开大。

**B.4 DRV 不再直写 `spi_inst->parent`**
读直接读、写必须用函数。`SPIRegister` 自身有防重，不需要 DRV 再靠 `parent` 是否非空来判重复注册
（与 `drv_dbus` 的既有做法一致）。

## 3. 失败路径与自恢复机制（发生时机 / 功能 / 原理）

> 设计目标只有一条：**任何一次瞬态失败都不允许演变成永久停摆。**
> SPI 的失效模式不是崩溃，是**链路悄悄死掉**：`transfer_busy` 恒为 1、
> `acc_cnt` / `gyro_cnt` 不再增长、姿态恒为初值，而日志一片安静。

### 3.1 为什么 SPI 比 UART 更容易永久停摆

UART 的发送链每帧都要调一次 `USARTTransmit`（因此复位可以挂在调用路径上）；
**SPI 的传输由 DRDY EXTI 发起**（BMI088 的 `BMI088_IntCallback`），而 EXTI 里：

- 不能等 tick ⇒ `SPI_WaitReady` 只能传 `timeout_ms = 0`，拿不到总线就放弃本次采样；
- 不能 Abort ⇒ 即便发现总线卡死也救不回来（见 A.4）。

两句合起来：**中断里发起的传输一旦启动失败，就再也起不来**，且因为 `transfer_busy`
永远清不掉，连"下一次 EXTI 重新发起"这条路也被自己堵死了。这就是必须有任务上下文
自恢复入口的原因。

### 3.2 失败返回点（`SPITransmit` / `SPIReceive` / `SPITransmitReceive`）

| 条件                                              | 返回                    | 计入                       |
| ------------------------------------------------- | ----------------------- | -------------------------- |
| `instance` / `handle` / `rx_buff` 为 NULL         | `BSP_PARAM_ERR`         | `tx_fail`                  |
| `mode > BSP_DMA_MODE`                             | `BSP_PARAM_ERR`         | `tx_fail`                  |
| `data` / `tx_data` 为 NULL、`len == 0` 或越界     | `BSP_PARAM_ERR`         | `tx_fail`                  |
| DMA 模式但该方向 `hdmatx` / `hdmarx` 为 NULL      | `BSP_PARAM_ERR`         | `tx_fail` + **`err_no_dma`** |
| IT/DMA、总线不空闲（`SPI_BusIsIdle` 为假，含 DMA 流未释放）、`timeout_ms == 0` | `BSP_BUSY` | `tx_fail` + `tx_busy` |
| IT/DMA、非 READY、`timeout_ms > 0` 等超时         | `BSP_TIMEOUT`（已复位） | `tx_fail` + `tx_timeout` + **`tx_recover`** |
| 就绪判完到真正调 HAL 之间被抢占，HAL 返回 `HAL_BUSY`（**含 BLOCK 模式**）| `BSP_BUSY` | `tx_fail` + `tx_busy` |
| `HAL_SPI_*` 返回其它非 `HAL_OK`（`ErrorCode` 已置位 / BLOCK 超时）| `BSP_HW_ERR` | `tx_fail` + **`err_start`** |

全部失败点经 `SPI_FailThenRet()` 计数（宏 `BSP_RETURN_IF_TRUE_LOG` 会求值 `ret` 参数），
日志行为不变。三个接口的计数字段共用一份（`tx_fail` 语义 = "收发接口返回非 `BSP_OK` 的总次数"，
不区分方向）。

`HAL_BUSY` 与"真失败"的分界靠 `HAL_StatusTypeDef` 本身（`SPI_StartFail()`）：两版 HAL 的入口
判据都是"State 非 READY 即 `return HAL_BUSY`"，此时**不动 State、不置 `ErrorCode`**；真失败
则一定带着 `ErrorCode`。**为什么必须分开**：BLOCK 模式不做就绪预判，一次"总线此刻正被 ISR
占用时的 BLOCK 调用"必然拿到 `HAL_BUSY`；若把它记成 `err_start` 并打 `start failed` 告警，
日志里就会出现 `err=0x0` 的假象（与 A.6 那条一模一样），把排查引向错误方向。日志文案也随之
分档：BLOCK 失败打 `SPI transmit failed`（跑了但没成功），IT/DMA 才是 `SPI ... start failed`。

### 3.3 卡死是怎么产生的

`hspi->State` 停在 `BUSY_TX` / `BUSY_TX_RX` 永不回 `READY`，成因三类：

1. **完成中断丢失** —— DMA 完成中断没来（DMA 流被抢占、传输计数错乱、M7 上 DCache 未刷）。
   完成回调是 `State` 回到 `READY` 的**唯一**路径，它不来就永远不回来。
2. **HAL 启动失败不复位状态** —— 见 A.2，且这条**每次重试都复现**（`State` 已经是 BUSY，
   `HAL_DMA_Start_IT` 必然再返回 `HAL_BUSY`），所以不会自愈。
3. **不能在 ISR 里 Abort** —— 见 A.4：识别得出、但在中断里救不了。
4. **"完成回调"早于"DMA 流释放"**（A.6，实机已复现）—— DRV 在 `rx_callback` 里清
   `transfer_busy` 时，另一路 DMA 流的收尾中断可能还没跑；此刻发起新传输会让 HAL 拿到
   `HAL_BUSY` 并把 `State` 留在 `BUSY_TX_RX`。**这一条现在是"预防"而不是"自愈"**：
   `SPI_WaitReady` 用 `SPI_BusIsIdle()` 把窗口挡在 HAL 之外，返回 `BSP_BUSY` 让上层下一拍重试。

第 3 条是关键：复位**只能放到"能安全自旋"的上下文**做。而要识别"现在到底是不是卡死"，
就必须有一个时长判据 —— `SPI_TX_STUCK_TIMEOUT_MS` 就是这个判据。
**它不是"超时"，是"允许在任务上下文做 Abort 的入场券"。**

### 3.4 `SPIRecoverTxIfStuck`：判据与调用点

**判据**：`!SPI_BusIsIdle(hspi)`（外设 `State` 或任一路 DMA 流非空闲，见 A.6）且持续超过阈值。
计时基准是 bsp 内的
`static uint64_t s_spi_ready_us[SPI_NUM_MAX]`（每个口"最近一次观察到 READY"的 DWT 微秒），
采样点四处：`SPIConfig` 结尾、`SPI_WaitReady` 的就绪路径、以及本入口每次发现总线空闲时。

```
[s_spi_ready_us 刷新]──┬─ 一次正常传输（约 6µs）──┬─ 判定
                       └──── 这一段必须 < 阈值 ────┘
```

**阈值取值理由**：单帧最长 8 字节 = 64 bit；即便按 1MHz 的慢速时钟算也只有 64µs，
当前板级（DJI_C SPI1 预分频 8、DM_MC02 SPI2 预分频 32）都在 6~64µs 量级 ——
`20ms`（默认 `SPI_TX_STUCK_TIMEOUT_MS`）是它的 **300 倍以上**，不会误杀；
同时 BMI088 的 DRDY 最快 1600/2000Hz（0.5ms 一拍），20ms 相当于漏 10~40 拍之内就把总线救回来。
**改 SPI 时钟 / 帧长后按 `帧字节 × 8 ÷ 时钟 × 1000 × 100（倍余量）` 重算。**

**为什么总线空闲时也要刷新基准**：上层是"每个控制周期调一次"，中间可能隔很久没有传输；
不刷新的话基准会停在很久以前，下一次正常传输一开始就被判成卡死
（假的 `tx_recover` + 一次无谓的 Abort 中止掉刚启动的传输）。

**调用点**：DRV 自己的任务上下文读入口，**每次调用必过、且放在所有提前返回之前**：

| 模块          | 调用点                                | 备注                                                     |
| ------------- | ------------------------------------- | -------------------------------------------------------- |
| `drv_bmi088`  | `BMI088Read` 开头                     | 每控制周期必经；放在 `acc_cnt == 0` 等提前返回之前，否则没有可用配对时永远够不到 |
| `drv_ist8310` | —（I2C，走 `I2CBusRecover` 链）        | 不适用                                                   |

**动作（`SPI_RecoverTx`）与顺序**：

```
⓪ SPI_CanBlockingAbort()?    // 否 → 直接返回，什么都不做（ISR / 临界区）
① 保存现场 hspi->ErrorCode   // HAL_SPI_Abort 成功时会把 ErrorCode 清零，那是"为什么卡住"的唯一直观依据
② HAL_SPI_Abort()            // 中止在途传输（含 CR2 里 DMA 请求已置位的那些流）
③ State 兜底置 READY         // Abort 失败（如句柄被 __HAL_LOCK 锁住返回 HAL_BUSY）时的保险
④ SPI_ForceReleaseDma()      // 兜底 Abort 够不到的流（典型：TxRx 里 TX 流启动失败，TXDMAEN 从未置位，见 A.6）
⑤ tx_recover++ / 状态快照
⑥ err_callback(instance, SPI_ERR_ABORT)   // 被中止的传输不会再有完成回调
```

**第 ⑥ 步的顺序不能颠倒**：DRV 的收尾动作普遍以"HAL 已收尾"为前提，提前通知会让它按
"传输可能还在途"处理，等于没通知。（与 `bsp_usart` §3.3.4 同款约定。）

### 3.5 `HAL_SPI_ErrorCallback` 里为什么不做中止

HAL 的几条错误路径（`SPI_ITError` / `SPI_DMAError` / `EndRxTxTransaction` 失败）**自身都已
停掉传输并把 `State` 复位为 `READY`**；而在 ISR 里再调 `HAL_SPI_Abort` 会经 `HAL_DMA_Abort`
等 HAL tick —— 中断里 tick 不前进，直接死等（A.4）。所以错误回调里只做
**分类计数 + 状态快照 + 日志 + `err_callback(SPI_ERR_HW)`**。
万一 HAL 没复位干净，下一次收发/自恢复会兜住（那正是 `SPIRecoverTxIfStuck` 存在的意义）。

### 3.6 排查入口

见 §5 的状态变量表。最快的四条线索：

- `tx_recover > 0` 且在涨 ⇒ **SPI 卡死发生过**（且已自愈）。稳定不涨说明链路健康。
- `err_start` 在涨 ⇒ 启动失败反复发生（**只有真失败会计它**：`HAL_BUSY` 归 `tx_busy`，见 §3.2）。
  日志里已经带了现场（`spi=%d tx_dma=%d rx_dma=%d err=0x%lX`）：`tx_dma`/`rx_dma` 哪个是 `2`
  （`BUSY`）就是哪一路流没释放；`spi` 是 `2`（`BUSY_TX_RX`）说明 HAL 把句柄留在忙态、要靠
  20ms 后的自恢复兜——**本层已把 A.6 那个窗口挡在前面，正常情况下 `err_start` 应恒为 0**，
  还涨就说明另有一条够不到路径，按上面的字段继续查。
  另注意 `err` 一定非 0：**日志里出现 `err=0x0` 的 `start failed` 只可能来自旧固件**。
- `err_no_dma > 0` ⇒ 板子/工程配置问题（用了 DMA 模式但该口没接对应 DMA），不是运行期故障。
- `tx_ok` / `rx_ok` 停止增长而 `err_total` 也不涨 ⇒ 传输根本没发起（DRV 侧的 `transfer_busy`
  卡住了），去看 DS 层而非 BSP 层的计数。

## 4. 板级前置条件与当前状态

| 能力                          | 前置条件（CubeMX）                                              | DJI_C | DM_MC02 / HALF_RUDDER |
| ----------------------------- | --------------------------------------------------------------- | ----- | --------------------- |
| `BSP_BLOCK_MODE`              | 无                                                              | ✅    | ✅                    |
| `BSP_IT_MODE`                 | 勾该 SPIx 的全局中断，生成 `SPIx_IRQHandler`                    | ⚠️ 未配（SPI1 无 `SPI1_IRQHandler`）| ✅（SPI2 有 `SPI2_IRQHandler` + `HAL_NVIC_EnableIRQ(SPI2_IRQn)`）|
| `BSP_DMA_MODE`                | 上一条 + 该 SPI 的 TX/RX DMA 请求与对应 DMA 流中断              | ✅ 仅 `SPI_BMI088`（SPI1） | ✅ 仅 `SPI_BMI088`（SPI2） |
| BMI088 `BMI088_MODE_INT`      | 上面 + 两根 DRDY 引脚的 EXTI                                    | ✅    | ✅                    |

各口的 DMA 能力（`SPIConfig` 时记录进 `s_spi_status[].dma_tx_capable/dma_rx_capable`）：

| 板       | 枚举          | 句柄   | TX/RX DMA                                    |
| -------- | ------------- | ------ | -------------------------------------------- |
| DJI_C    | `SPI_BMI088`  | `hspi1`| ✅ `hdma_spi1_tx`(DMA2_Stream3) / `hdma_spi1_rx`(DMA2_Stream0)，两个 IRQ 均已使能 |
| DJI_C    | `SPI_EX_2`    | `hspi2`| ❌ 无 DMA（用 DMA 模式会得到 `err_no_dma` + `BSP_PARAM_ERR`）|
| DM_MC02 / HALF_RUDDER | `SPI_BMI088` | `hspi2` | ✅ `hdma_spi2_tx`(DMA1_Stream3) / `hdma_spi2_rx`(DMA1_Stream2) |
| DM_MC02  | `SPI_LCD_1`   | `hspi1`| ❌ 无 DMA；且 `hspi1.Init.DataSize = SPI_DATASIZE_4BIT`（LCD 用 4 位宽，**与 BMI088 的 8 位不通用**）|

- **RX 缓冲须在 DMA 可访问内存**：H7（Cortex-M7）上 `SPI_INSTANCE_DEF` 已用 `DMA_RAM`
  （映射到 `.ram_d1`）自动处理；F4 无此限制。
- **CubeMX 的 SPI 硬件参数决定一切**：时钟极性/相位/预分频/数据位宽全在 `spi.c` 里，
  与 BMI088 的要求（CPOL=High、CPHA=2Edge、8bit/MSB、≤10MHz）必须一致 ——
  BSP 层不做任何校验，配错只会表现为读到的 CHIP_ID 不对。
- `SPIRecoverTxIfStuck` 依赖 DWT（`DWT_GetTimeUs`），需 `bsp_dwt` 已初始化（与其它 bsp 模块一致）。

### 当前状态

- DJI_C / DM_MC02 的 `SPI_BMI088` + `BSP_DMA_MODE`：代码与板级配置均就绪，**正在实机验证**
  （`BMI088_MODE_INT`，见 `app/half_rudder_gimbal/app_gimbal/app_gimbal.c`）。
- BLOCK / IT 模式：代码就绪，当前无实例使用（初始化期的寄存器读写走的也是 BLOCK，
  经 `BMI088_ReadReg` / `BMI088_WriteReg`）。IT 模式在 DJI_C 上**因未配 SPI1 全局中断而不可用**。
- `SPI_EX_2` / `SPI_LCD_1`：无驱动实例化，本层不关心。

## 5. 状态变量：错误码、错误计数等使用方式（调试用，调试器直接 Watch）

`volatile SPI_Status_s s_spi_status[SPI_NUM_MAX]`（类型定义于 `bsp_spi.c`，按 `spi_e` 索引）。
纯调试辅助：**只增不清**，需要清零可在调试器里直接写 0。同类还有 CAN 的 `s_bxcan_status` /
`s_fdcan_status`、UART 的 `s_usart_status`。另有 `static uint64_t s_spi_ready_us[SPI_NUM_MAX]`
（不对外、非计数）：每个口最近一次"观察到总线空闲"的时刻，卡死判定的计时基准（§3.4）。

| 字段                                        | 含义                                                         |
| ------------------------------------------- | ------------------------------------------------------------ |
| `tx_ok`                                     | 传输成功完成次数（BLOCK 直接成功 + IT/DMA 的 TxCplt / TxRxCplt）|
| `tx_fail` / `tx_busy` / `tx_timeout`        | 收发接口失败总数 / 其中 `BSP_BUSY` / 其中 `BSP_TIMEOUT`       |
| `rx_ok`                                     | 收到数据的次数（BLOCK 收满 + IT/DMA 的 RxCplt / TxRxCplt；**只收不发不计 `tx_ok`**）|
| `err_total`                                 | 错误回调总次数                                               |
| `err_modf` / `err_crc` / `err_ovr` / `err_fre` / `err_dma` / `err_flag` / `err_other` | 按 `hspi->ErrorCode` 分类计数（**独立 `if`**，可同时置位）|
| `err_no_dma`                                | 调 DMA 模式但该口没有对应 DMA（配置问题，先看它）             |
| `err_start`                                 | HAL 启动传输**真失败**次数（`HAL_BUSY` 不算，它归 `tx_busy`，见 §3.2；`err_start ⊂ tx_fail`）|
| `tx_recover`                                | 卡死的传输被强制复位次数（复位后会调 `err_callback(SPI_ERR_ABORT)`）|
| `state` / `last_mode`                       | 最近一次采样的 HAL `State` / 最近一次传输模式                 |
| `error_code` / `err_time_us`                | 最近一次 `hspi->ErrorCode` / 出错或**启动失败**时刻（DWT 微秒）|
| `dma_tx_capable` / `dma_rx_capable`         | Config 时记录：该口是否配了 TX/RX DMA（排除 `err_no_dma` 时先看它）|
| `dma_tx_state` / `dma_rx_state`             | **最近一次启动失败时**两路 DMA 流的 `State`（`0xFF` = 该口无这一路 DMA）。出现 `err_start` 时先看它：`2` 表示"启动失败是因为这一路流没释放"（见 A.6）。**编号随 HAL 版本不同**：关键只看 `1`=`READY`、`2`=`BUSY`；其余为异常/终止态 —— F4 是 3=TIMEOUT / 4=ERROR / 5=ABORT，H7 是 3=ERROR / 4=ABORT（见两版 `stm32*xx_hal_dma.h` 的 `StateTypeDef`）|
| `tx_len` / `rx_len`                         | 最近一次发送 / 接收长度                                       |

**排查时的第一反应**：

- IMU 数据不再更新 → 看 `tx_ok` 是否还在涨。不涨且 `tx_recover` 在涨 ⇒ 卡死自愈在反复发生；
  两者都不动 ⇒ 问题在 DRV 层（`transfer_busy` 卡住、EXTI 没来）。
- `state` 长期不是 1（`HAL_SPI_STATE_READY`）⇒ 传输卡死。
- 数据错乱 / CHIP_ID 不对 ⇒ 先看 CubeMX 的 CPOL/CPHA（BSP 不校验，见 §4）。

## 6. 验证

本机无 SPI 硬件，重构后靠"全 app 编译 + 静态核对 + 符号级"验证：

1. `cmake --preset default`，然后 `cmake --build --preset Debug --target half_rudder_gimbal` /
   `--target half_rudder_chassis` / `--target example`，以及 `--preset Release --target half_rudder_gimbal`：
   全部 rc=0、**零警告零错误**；构建日志确认 `bsp_spi.c` / `drv_bmi088.c` 确实被重新编译。
2. `arm-none-eabi-nm -g --defined-only build/CMakeFiles/half_rudder_gimbal.dir/Debug/bsp/bsp_spi/bsp_spi.c.obj`
   输出恰为：`s_spi_status`(B)、`g_spi_log`(D)、四个 HAL 回调、`SPIConfig` / `SPIReceive` /
   `SPIRecoverTxIfStuck` / `SPIRegister` / `SPITransmit` / `SPITransmitReceive` ——
   **旧接口（`SPI_AbortOnError`、`SPI_Transmit` 等）符号已消失**。
3. 残留检查：`grep -rn "SPI_Work_Mode_e\|last_xfer_len\|SPI_DMAStop\|SPI_AbortOnError\|s_spi_last_route" bsp drv app`
   在 `.c` / `.h` 里只剩 `bsp_spi.c` 一句说明性注释（"中止必须用 `HAL_SPI_Abort`，不能用
   `HAL_SPI_DMAStop`"），以及 `bsp_i2c` 自己的同名 `last_xfer_len` 字段（I2C 的，与 SPI 无关）。
4. 人工确认：`SPIReceive` 的 BLOCK 成功路径只计 `rx_ok`（不计 `tx_ok`）；
   `SPI_CheckDmaCapability` 在三个接口都有调用；`HAL_SPI_ErrorCallback` 为独立 `if` 分类；
   `s_spi_status` 为 `volatile`；三个接口的启动分支都经 `SPI_StartFail`（`HAL_BUSY` → `BSP_BUSY`，
   不计 `err_start`）。
5. **启动失败日志格式的 PC 端回归**：该格式串（两个 `%s` + 5 个 `%d` + 1 个 `%lX`）错位一次，
   后面所有字段就全对不上（本次 A.6 排查正是靠这几个数）。`lib/lib_format/tools/test_format.c`
   已加入该串与 BLOCK 变体两个用例，按文件头的命令（`gcc -m32 …`）编译运行应全 PASS。
6. **实机回归（2026-09 A.6 修复后）**：
   - 日志里 `SPI ... start failed` / `SPI stuck >20ms, state reset` **应完全消失**；
     `s_spi_status[SPI_BMI088].err_start` 与 `tx_recover` 都应为 0；
   - `tx_busy` 会**有**增长（A.6 的窗口现在以 `BSP_BUSY` 返回，属于正常流控，不是故障），
     它的量级就是"被 DMA 收尾窗口挡掉的采样次数"；
   - 若 `err_start` 仍在涨：看日志里 `tx_dma` / `rx_dma` 哪个是 `2`（§5 的排查入口），
     那说明还有一条 `SPI_WaitReady` 够不到的失败路径（此时 `err` 必非 0；若见到 `err=0x0`，
     说明固件不是本版，或走的是旧固件那条把 `HAL_BUSY` 当失败的老路径）。
7. **待现场验证（无硬件时的静态项）**：`BSP_BUSY` 的实际出现频率、`err_no_dma` 是否为 0。
   破坏性验证 `SPIRecoverTxIfStuck`：在调试器里把 `hspi1.State` 强行写成
   `HAL_SPI_STATE_BUSY_TX`（或把 `hdmatx->State` 写成 `HAL_DMA_STATE_BUSY`），
   看 `tx_recover` 是否在 `BMI088Read` 的下一个周期 +20ms 内自增、`transfer_busy`
   是否随之清零、采样是否恢复。

## 7. 已知问题 / 后续

- **`SPI_TX_STUCK_TIMEOUT_MS` 是全局单值**，一个值管所有 SPI。当前各口帧长同构所以没问题；
  若将来出现"慢速长帧 + 高速短帧"共存，须移进 `SPI_Config_s` 做 per-SPI 配置。
- **`drv_bmi088` 的 INT 模式依赖 `BMI088Read` 被周期调用**：自恢复挂在它上面，
  若某条链路只在需要数据时才读，卡死后的恢复也会相应地晚。这是"把复位放任务上下文"的
  必然代价（中断里做不了）。
- **`SPIReceive` 的 BLOCK 成功路径不更新 `instance->rx_len` 之外的状态**：长度上限一律以
  `buff_size` 为准，上层不要再按"收到了多长"去改缓冲大小。
- DJI_C 的 `SPI_EX_2` 与 DM_MC02 的 `SPI_LCD_1` 都没有 DMA；将来要用 DMA 须先在 CubeMX 补
  DMA 请求与流中断，否则只会拿到 `err_no_dma`。
- 后续：can / usb 最后迁到 `BSP_Status_e`（iic 已按本模板迁完，见 `bsp_i2c.md`；
  I2C 侧没有 `SPIRecoverTxIfStuck` 的同款接口 —— 它已有 `I2CBusRecover` 作为任务上下文的
  恢复入口，DRV 侧的失败计数会触发它）。
