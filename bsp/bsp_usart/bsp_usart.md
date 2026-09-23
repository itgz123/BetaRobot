# bsp_usart 开发总结

> 本文件记录 bsp_usart（`bsp_usart.h` / `bsp_usart.c`，公共件见 `bsp/bsp_common/bsp_common.h`）
> 的接口用法、踩过的坑与处理方式、CubeMX 前置条件及原因。git 提交号为历史溯源参考。
> 本模块同时是 iic / spi 重构的模板。

## 1. bsp_usart 接口使用说明

### 1.1 对外入口

| 接口                                            | 作用                                     | 可重复调用                            |
| ----------------------------------------------- | ---------------------------------------- | ------------------------------------- |
| `USART_INSTANCE_DEF(name, buff_sz)`             | 静态定义实例 + 接收缓冲                  | -                                     |
| `USARTRegister(instance)`                       | 参数校验 + 防重 + 加入 static 管理数组   | 否（重复注册返回 `BSP_PARAM_ERR`）    |
| `USARTConfig(instance, config)`                 | 填硬件句柄 / parent / 三个回调，登记路由 | 是（重入会中止常开接收）              |
| `USARTTransmit(instance, data, len, mode, ms)`  | 发送（三模式每次传参）                   | 是                                    |
| `USARTReceive(instance, len, mode, ms)`         | 启动接收（三模式每次传参）               | 是（常开流已跑时返回 `BSP_BUSY`）     |
| `USARTRecoverRxIfStalled(instance, period_ms)`  | **接收**停摆自恢复（判据/限频/参数都在 bsp 内）| 是（幂等，任务上下文，见 §3.7）  |
| `USARTRecoverTxIfStuck(instance, stuck_ms)`     | **发送**卡死自恢复（判据/计时都在 bsp 内）| 是（幂等，任务上下文，见 §3.3.6）     |

除此之外**不对外提供任何东西**：没有 `USARTIsReady`、没有函数指针表、没有内部发送队列 / 缓冲池。
忙由 `BSP_BUSY` 表达；两条自恢复各只有一个入口，"什么时候看一眼"由上层定，"看到什么算故障、
看到故障做什么"全在 bsp 内。

### 1.2 统一状态码与模式（`bsp/bsp_common/bsp_common.h`）

```c
typedef enum : int8_t  { BSP_OK = 0, BSP_BUSY = -1, BSP_TIMEOUT = -2,
                         BSP_PARAM_ERR = -3, BSP_HW_ERR = -4 } BSP_Status_e;
typedef enum : uint8_t { BSP_BLOCK_MODE = 0, BSP_IT_MODE = 1, BSP_DMA_MODE = 2 } BSP_Transfer_Mode_e;
```

- 底层 `int8_t`、成功为 0、错误为负 —— 既支持 `!= BSP_OK`，也兼容旧代码 `0 / -1` 的写法，iic/spi 迁移期可共存。
- `BSP_TIMEOUT` 返回时 BSP **已顺手强制中止卡死的发送并复位状态**，调用方可以直接重试，不需要额外的恢复调用。
- 超时计时统一用 `BSP_Timeout_s`（内部是 DWT 64 位微秒），**不用 `HAL_GetTick()`**：不受中断影响、无回绕。

### 1.3 三种模式

| 模式            | 发送                          | 接收                                    |
| --------------- | ----------------------------- | --------------------------------------- |
| `BSP_BLOCK_MODE`| `HAL_UART_Transmit`，返回即发完 | `HAL_UART_Receive`，一次一帧，收完返回 |
| `BSP_IT_MODE`   | `HAL_UART_Transmit_IT`        | `HAL_UARTEx_ReceiveToIdle_IT`，常开流   |
| `BSP_DMA_MODE`  | `HAL_UART_Transmit_DMA`       | `HAL_UARTEx_ReceiveToIdle_DMA`，常开流  |

- 模式**每次调用传参**，不再 Config 期固定。
- `timeout_ms` 所有模式都有：**`0` = 不等待，忙即返回 `BSP_BUSY`**；`>0` = 轮询等就绪，超时 `BSP_TIMEOUT`。
  （IT/DMA 的 `timeout_ms` 只用于等"上一次发送结束"；BLOCK 直接透传给 HAL，0 在 HAL 里即"立即超时"，语义一致。）

### 1.4 常开接收（IT/DMA）

`USARTReceive(inst, len, BSP_DMA_MODE, 0)` 启动后进入**常开流**：每收到一帧（IDLE 或收满 `len`）
触发 `rx_callback`，BSP 随后按同样的 `len`/`mode` 自动续收，期间 `rx_armed == 1`。
重复调用返回 `BSP_BUSY`（属调用方错误，不是"再启动一次"）。

- **接收缓冲由 BSP 清零**：续收前（仅在真正启动新一次传输前）对 `rx_buff` 前 `rx_xfer_len` 字节清零，
  方便调试器看"这一帧是哪些字节"。
- **回调内必须同步消费 `rx_buff`**：回调返回后 BSP 就会重启 DMA，缓冲会被覆写。
  长度校验不过的帧直接丢弃（drv_dbus / drv_sbus 就是这么做的）。
- **BLOCK 接收与常开流互斥**：同实例只允许一种接收在跑（`RxState` 只有一个）。
  BLOCK 接收**不写** `rx_mode` / `rx_xfer_len`（那是常开流的重启参数，见 §3.7 第 4 条），
  所以一个"跑过常开流又偶发用 BLOCK 收一帧"的实例，其自恢复仍按原常开流参数重启 ——
  但两者仍不应混用：自恢复会先清 `RxState` 残留，可能把另一次挂起的 BLOCK 接收一起中止。
- **停摆由上层触发重启**：bsp 只负责续收与"报告停摆"，重启逻辑收在 `USARTRecoverRxIfStalled`
  （§3.7），由上层在**能安全自旋的上下文**把它调起来 —— ISR 与 `taskENTER_CRITICAL` 里都不能做
  （`HAL_UART_AbortReceive` 要自旋等 tick，见 §A.3）；入口自身也带上下文检查，误调不会出事。

### 1.5 缓冲区与生存期

| 场景        | 约束                                                                      |
| ----------- | ------------------------------------------------------------------------- |
| TX IT/DMA   | `data` 指向的缓冲须**生存到 `tx_callback` 返回**（DMA 直接读它）           |
| TX BLOCK    | 只在调用期内有效                                                          |
| RX          | `USART_INSTANCE_DEF` 静态分配的 `rx_buff`，常驻；H7 上自动放 `DMA_RAM`    |
| 多缓冲/排队 | **BSP 不做**，由上层自持（`bsp_log` 缓冲池、`drv_vofa` 三缓冲、`terminal_lite` 槽池）|

### 1.6 回调契约

```c
void (*rx_callback)(USARTInstance *);   // ISR：收到一帧（rx_buff / rx_len 有效）
void (*tx_callback)(USARTInstance *);   // ISR：IT/DMA 发送完成；此处 gState 已回到 READY，可直接续发
typedef enum { USART_ERR_HW, USART_ERR_TX_ABORT, USART_ERR_RX_STALLED } USART_ErrReason_e;
typedef void (*USART_ErrCallback)(USARTInstance *, USART_ErrReason_e);  // ISR 或任务上下文
```

- `parent` 由 **Config 写入**（DRV 不再直写 `instance->parent`），回调里用它取回上层实例。
- **`tx_callback` 内续发是允许的**（HAL 在调用回调前已把 `gState` 置回 `READY`），`bsp_log` / `drv_vofa` /
  `drv_terminal_lite` 的发送链依赖这一契约；但**不得在回调内调 `USARTConfig` / `USARTReceive`**。
- **`err_callback` 带 `reason`**，把"该归还哪种在途缓冲"写进参数，上层不必再靠 `gState` 反推：
  ① `USART_ERR_HW`——`HAL_UART_ErrorCallback`，**ISR 上下文**，错误位全在接收侧、发送**可能仍在途**；
  ② `USART_ERR_TX_ABORT`——`USART_RecoverTx()` 强止在途发送之后，**任务上下文**，`gState` 已复位为 `READY`；
  ③ `USART_ERR_RX_STALLED`——续收最终失败（RX 停摆），**ISR 上下文**，此后不再有 `rx_callback`。
  因此 handler 必须**无阻塞、可重入、幂等**，且不得在其中调 `USARTTransmit` / `USARTConfig` / `USARTReceive`。
  两个现成的分流范式（见 §3.5 第 6 点）：
  - TX 侧（`bsp_log` / `drv_vofa` / `drv_terminal_lite`）：`RX_STALLED` 直接返回；`TX_ABORT` 必然归还；
    `HW` 才需要看 `gState`；
  - RX 侧（`drv_dbus` / `drv_sbus`）：`TX_ABORT` 直接返回（本驱动不发），其余按 `rx_armed` 判停摆。
- **`err_callback` 的缓冲归还规则（重要）**：`USART_ERR_HW` 是纯接收侧错误（ORE/FE/NE），此时**若发送仍在途**
  （`instance->handle->gState != HAL_UART_STATE_READY`），**不能**归还"发送中"的缓冲——DMA 还在读它。
  只有当 `gState == HAL_UART_STATE_READY`（HAL 已结束该次发送、不会再补一次完成回调）时才归还。
  漏归还的残留会由下一次发送完成回调兜底清理，所以这条规则是"宁可晚一拍，绝不提前"。
  见 `bsp_log.c` 的 `LogUartErrHandler`、`drv_vofa.c` 的 `VofaErrHandler`、
  `drv_terminal_lite.c` 的 `TerminalLiteErrHandler`。
  同一个 `gState` 判据使得 `USART_RecoverTx()` **必须先把状态复位再通知**，否则会被上层拒绝归还。

### 1.7 对外接口速查

```c
BSP_Status_e USARTRegister(USARTInstance *instance);
BSP_Status_e USARTConfig(USARTInstance *instance, const USART_Config_s *config);  // 不再自动启动接收

BSP_Status_e USARTTransmit(USARTInstance *instance, const uint8_t *data, uint16_t len,
                           BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
BSP_Status_e USARTReceive (USARTInstance *instance, uint16_t len,
                           BSP_Transfer_Mode_e mode, uint32_t timeout_ms);

/* 接收停摆后的重启入口：四条 UART 链路（dbus/sbus/comm/terminal_lite）共用同一份实现，
 * 各自只在自己的任务上下文回调里传一个限频周期（见 §3.7） */
BSP_Status_e USARTRecoverRxIfStalled(USARTInstance *instance, uint32_t period_ms);
/* retval BSP_OK   = 本次刚重启起来（调用方通常据此打一条 WARNING）
 *        BSP_BUSY = 未动作：接收在跑 / 没有常开流可重启 / 还在限频期内 / 当前上下文不允许 Abort
 *        其它     = 尝试过但失败（USARTReceive 已计 err_* 并打 ERROR） */

/* 发送卡死后重启入口：各发送链在自己的"发送入口"每帧调一次（见 §3.3.6） */
BSP_Status_e USARTRecoverTxIfStuck(USARTInstance *instance, uint32_t stuck_ms);
/* retval BSP_OK   = 本次刚复位了一次卡死的发送（err_callback 已以 TX_ABORT 通知过、缓冲已归还）
 *        BSP_BUSY = 未动作：发送空闲（顺带刷新计时基准）/ 未到阈值 / 当前上下文不允许 Abort
 *        BSP_PARAM_ERR = 实例或句柄为空 */
```

典型用法：

```c
USART_INSTANCE_DEF(my_uart, 64);
static const USART_Config_s cfg = {
    .uart_e = UART_7, .parent = NULL,
    .rx_callback = MyRx, .tx_callback = MyTxCplt, .err_callback = MyErr,
};
USARTRegister(&my_uart);
USARTConfig(&my_uart, &cfg);
USARTReceive(&my_uart, my_uart.rx_buff_size, BSP_DMA_MODE, 0);  // 需接收才调
```

### 1.8 相对旧版的破坏性变更

| 旧                                            | 新                                                       |
| --------------------------------------------- | -------------------------------------------------------- |
| `USART_Work_Mode_e` + Config 期固定 tx/rx 模式 | 删除；模式每次调用传参 `BSP_Transfer_Mode_e`             |
| `USARTIsReady()`                              | 删除；忙由 `BSP_BUSY` 表达                               |
| `USARTRecoverTransmit()` / `USARTRestartReceive()` | 删除；BSP 内部按需自动纠正（`tx_recover` / `rx_restart`）。自恢复的对外形态是 §1.1 的两个 `USARTRecover*` 入口：判据与动作在 bsp，上层只提供"何时看一眼"的任务上下文时基 |
| `USARTConfig` 自动启动 DMA+IDLE 接收          | 不自动启动，需显式 `USARTReceive`                        |
| Config 强制要求 RX DMA（只发不收也被卡）      | 不检查；只有真正用 DMA 模式时才在调用点校验并报 `err_no_dma` |
| 发送/接收返回 `int8_t 0/-1`                   | 统一 `BSP_Status_e`                                      |
| `void *parent` 由 DRV 直写                    | 由 `USART_Config_s.parent` 写入                          |

## 2. 之前踩过的坑与处理

### A. 硬件 / HAL 坑

**A.1 IT/DMA 发送中途出错后 `gState` 停在 `BUSY_TX` → 发送永久静默**（`bada57b`）
症状是日志/VOFA 突然全没了，且再也恢复不了。处理：`USARTTransmit` 等待就绪失败时调
`USART_RecoverTx()`（`HAL_UART_AbortTransmit` + `gState` 兜底复位）+ `tx_recover++`。

**A.2 `timeout_ms == 0` 的调用方（log / vofa / terminal）没有等就绪的路径 → 卡死后无人复位**
这些调用方要的是"忙就丢/排队"，不能阻塞等待，于是永远走不到超时分支。
处理：为每个 UART 记 `s_tx_ready_us[]`（最近一次观察到 READY 的 DWT 时刻），非 READY 且已连续超过
`USART_TX_STUCK_TIMEOUT_MS`（默认 200 ms，远大于 115200 下 64B 的约 6 ms）就判定卡死并复位。

**A.3 不能在 ISR / 临界区里做会自旋的 Abort**
`HAL_UART_AbortTransmit` 内部经 `HAL_DMA_Abort`，按 `HAL_GetTick()` 自旋等 DMA 关闭。**两类上下文里
tick 都不前进**：① 高优先级中断（`IPSR != 0`）；② **临界区** —— `taskENTER_CRITICAL` 抬的是 BASEPRI
（FreeRTOS ARM_CM4F/CM7 端口的 `portDISABLE_INTERRUPTS` = `vPortRaiseBASEPRI`），SysTick 同样进不来。
处理：① 错误回调里完全不动发送状态，复位只发生在 `USARTTransmit` 的就绪失败路径；
② `USART_RecoverTx` / `USART_RecoverRx` 入口用 `USART_CanBlockingAbort()`（同时查 IPSR / PRIMASK /
BASEPRI，比 `bsp_i2c` 的 `I2C_InIsr` 多查后两者）判定，不允许时**什么都不做并返回 0**，且调用方
不刷新卡死判据的时间戳 —— 于是下一次任务上下文的收发自动补做真正的复位。
（实机动机：`drv_terminal_lite` 的发送提交整个包在 `taskENTER_CRITICAL` 里，只查 IPSR 挡不住。）

**A.4 错误回调里"看到 `gState != READY` 就判卡死"是错的（曾误伤正常发送）**
HAL 的 UART 错误位（PE/FE/NE/ORE/RTO/DMA）**全在接收侧**，此刻 `gState != READY` 的常见含义恰恰是
"有一次健康的发送正在途"；据此中止会把正常发送杀掉。处理：错误回调只做分类计数 + 快照 + 接收侧纠正 +
`err_callback`；发送卡死一律交由 A.2 的 DWT 时长判定。

**A.5 接收错误后 RX 停摆**
H7/F4 HAL 对"阻塞型错误"（`DMAR` 置位，或 ORE/RTO）会 `UART_EndRxTransfer` 把 `RxState` 置回 READY、
关 RXNEIE/IDLEIE，DMA 异步中止完才回调 `HAL_UART_ErrorCallback`。此时常开流已断，不重启就永久收不到。
处理：错误回调中 `rx_armed && RxState == READY` 时 `USART_ContinueReceive()`。
反过来非阻塞型错误（PE/FE/NE，无 DMA）HAL 不动 `RxState`、接收仍在继续，条件不成立、不会误重启。

**A.6 续收时"忙"不等于失败**
错误回调与正常收发路径可能前后脚都要求续收，后一个会拿到 `HAL_BUSY`。若当成失败会清 `rx_armed`
让常开流停摆。处理：`USART_ContinueReceive` 把 `BSP_OK` 与 `BSP_BUSY` 同等视为"接收在跑"，
只有 `BSP_HW_ERR` / `BSP_PARAM_ERR` 才重试并计 `rx_restart_fail`。

**A.7 不需要 `HAL_UART_DMAStop`（旧版为"循环 DMA 回卷"加的）**
工程里 CubeMX 配的 DMA 全是 `DMA_NORMAL`。H7/F4 在 IDLE 事件与 DMA 完成路径上，HAL 自己就会关 DMAR、
置 `RxState=READY`、清 IDLEIE 并中止 DMA，回调里再 `DMAStop` 只会多调一次会自旋的 Abort。
（若以后改成 `DMA_CIRCULAR`，这个结论要重新审。）

**A.8 `ReceiveToIdle_DMA` 下 DMA 半传输也会进 `RxEventCallback`**
会把半帧当一帧上报。处理：DMA 模式启动后显式 `__HAL_DMA_DISABLE_IT(hdmarx, DMA_IT_HT)`，只保留 IDLE / 收满。

**A.9 没配 TX DMA 的口（如 `LOG_UART`）用 DMA 模式会静默无输出**
`HAL_UART_Transmit_DMA` 在 `hdmatx == NULL` 时直接返回 `HAL_ERROR`，旧代码忽略返回值 → 一条日志都没有。
处理：调用点显式校验 `hdmatx` / `hdmarx`，计数 `err_no_dma` + 报 `BSP_PARAM_ERR`；`USARTConfig`
不再强制要求 RX DMA（只发不收的实例因此不再被卡）。

**A.10 `gState` 判断与 `USARTTransmit` 之间存在 TOCTOU**
"先问就绪、再发送"中间可能被中断插入，导致两个缓冲同时交给 DMA。处理：**不再导出 `USARTIsReady`**，
把就绪判断收进 `USARTTransmit` 内部一次性完成；上层（`bsp_log` / `drv_vofa`）改用**自己的
"是否已有在途缓冲"**作为判据——同一时刻至多一个在途缓冲，无在途就必然没有完成回调能抢跑。

**A.11 `err_callback` 里无条件归还在途缓冲会毁掉正在发的帧**（见 1.6）
纯接收错误时会误归还 DMA 还在读的缓冲。处理：只在 `gState == HAL_UART_STATE_READY` 时归还。

**A.12 `USARTRegister` 期间打的日志会炸出一条自指的假错误**
`USARTConfig` 之前 `handle` 还是 NULL，于是 `USARTRegister` 里那条 `BSPLOG` 走
`USARTTransmit` 必然失败；失败路径又会 `BSPLOG` 一条 "Handle is NULL" —— 而这条日志同样发不出去，
会被 `LogBufSending()` 判成 `WAIT_SEND` **滞留**，直到下一次成功发送时由 `TxCplt` 补发。
现象是开机日志里出现**时间戳比前一行更早**的错误行：

```
[I][43][bsp_log]:你好！
[E][23][bsp_usart]:Handle is NULL, call USARTConfig first!   ← 23µs 早于 43µs
```

处理：`BSPLogV` 入口判 `s_log_uart.handle == NULL` 直接丢弃（传输层没就绪就是无处可发），
不要让失败一路传导。**排查同类问题的通用手法：日志时间戳与打印顺序不一致 ⇒ 这条是从
`WAIT_SEND` 补发的，去查它被格式化那一刻在干什么。**

**A.13 `%lX` 在 lib_format 里读 64 位 → 错误码日志是错的**
`lib_format` 曾把单个 `l` 也当 64 位（`va_arg(uint64_t)`）。在 arm-none-eabi 上这会生成
`ldrd`——**读 8 字节、推进 8 字节**，而 `ErrorCode` 是 `uint32_t`，于是多吃一个参数槽：
错误码高半是垃圾，**同一格式串后续所有 `%d` 全部错位**。
现象（实测）：`[E]SPI ... err=0x80065BB00000010` —— 真实值只有低半 `0x10`（`HAL_SPI_ERROR_DMA`）。
处理：改成只有 `ll` 才是 64 位、`l` 按 32 位（与目标 ABI 一致），一处改动修好全部站点。
回归测试：`lib/lib_format/tools/test_format.c`（**必须 `-m32` 编译**，64 位主机上 `long` 本来就是
64 位、测不出来；已实测旧版 5 项失败、新版全过）。

**A.14 发送卡死自恢复只复位了 bsp 状态，上层缓冲池自己堵死**
`USART_RecoverTx()` 原先中止在途发送后**不通知上层**。被 Abort 掉的那次发送不会再触发 `tx_callback`，
而上层发送链全靠 tx_callback 归还缓冲：`bsp_log` 的 `s_tx_buf` 永远指向 SEND 状态的槽、
`drv_vofa` 停在 `BUFF_ACTIVE`、`terminal_lite` 停在占用槽。结果是 **bsp 层的 `gState` 已复位，
上层却再也发不出东西** —— 这个机制要治的"静默停摆"原样复现在上一层，等于没修。
会漏的是**无错误位的纯中断丢失型卡死**（伴随 HAL 错误位的卡死会先经 `HAL_UART_ErrorCallback` 通知过上层）。
处理：`USART_RecoverTx` 改收 `instance`，在 **Abort + `gState` 复位之后**调 `err_callback`。
顺序不能颠倒——上层的归还判据就是 `gState == READY`，提前通知会被当成"发送仍在途"而拒绝归还（见 §1.6）。

### B. 设计重构

- **职责收敛**：只留"收发 + 回调 + 纠错 + 计数"，删掉函数指针表（改 `switch-case`）、
  删掉 `is_ready` / 恢复类接口、删掉内部发送队列。
- **模式提到调用期**：发送、接收都传 `BSP_Transfer_Mode_e`，Config 只留总线级参数。
- **统一状态码**：`BSP_Status_e` 放公共头 `bsp_common.h`，iic / spi 复用同一套。
- **register / config 拆分**：沿用 CAN 范式（可重入、防重、句柄→实例路由表）。
- **回调命名统一**：`parent` / `rx_callback` / `tx_callback` / `err_callback`，iic / spi 照此对齐。
- **索引法查表**：`USART_HuartToIndex()` 线性扫 `uart_map`，回调里先按索引计数、再取实例——
  即使实例路由缺失，错误也不漏计。`s_usart_inst_by_uart[]` 取代旧的 `s_usart_last_route`。

### C. 当前处理状态

| 项                                    | 状态      | 说明                                        |
| ------------------------------------- | --------- | ------------------------------------------- |
| A.1 TX 卡死无恢复                     | ✅ 已处理 | `USART_RecoverTx` + `tx_recover`            |
| A.2 timeout=0 调用方无自恢复通道      | ✅ 已处理 | `s_tx_ready_us` + `USART_TX_STUCK_TIMEOUT_MS`（调用点见 A.20）|
| A.3 ISR / 临界区内的自旋 Abort        | ✅ 已规避 | `USART_CanBlockingAbort()` 判定，不允许时不做且不刷新判据 |
| A.4 错误回调误判发送卡死              | ✅ 已修正 | 错误回调不再动发送状态                      |
| A.5 接收错误后 RX 停摆                | ✅ 已处理 | 按 `RxState` 条件续收 + `rx_restart`        |
| A.6 续收 BSP_BUSY 误判                | ✅ 已处理 | OK/BUSY 同等视为在跑                        |
| A.7 多余 `HAL_UART_DMAStop`           | ✅ 已删除 | DMA 全 `DMA_NORMAL`，HAL 自行收尾           |
| A.8 半传输事件被当一帧                | ✅ 已处理 | DMA 模式关 `DMA_IT_HT`                      |
| A.9 无 DMA 口静默失败                 | ✅ 已处理 | `err_no_dma` + `BSP_PARAM_ERR`              |
| A.10 IsReady→Transmit TOCTOU          | ✅ 已消除 | 不导出 is_ready，上层用自持判据             |
| A.11 err 回调误归还在途缓冲           | ✅ 已修正 | 按 `gState` 条件归还                        |
| A.12 Register 期自指假错误            | ✅ 已修正 | `BSPLogV` 传输层未就绪即丢弃                |
| A.13 `%lX` 读 64 位致日志错位         | ✅ 已修正 | lib_format：`l`=32 位、`ll`=64 位           |
| A.14 卡死自恢复不通知上层             | ✅ 已修正 | `USART_RecoverTx` 末尾调 `err_callback`     |
| A.15 回调无法区分错误原因             | ✅ 已修正 | `USART_ErrReason_e` 参数；dbus/sbus 不再把 TX 中止误当失联 |
| A.16 启动窗口内提前认领在途缓冲       | ✅ 已修正 | log/vofa/terminal：启动成功才置 SEND/认领，err 回调不碰未交给 DMA 的槽 |
| A.17 启动失败连监控一起丢             | ✅ 已修正 | dbus/sbus `Config` 把 `USARTReceive` 移到最末，失败时 daemon + 超时参数已就位 |
| A.18 terminal 接收启动失败静默        | ✅ 已修正 | 启动失败不再静默（Config 打 ERROR，自恢复兜住） |
| A.19 四条链路各写一份停摆自恢复       | ✅ 已收敛 | 判据/限频/重启参数收进 bsp 的 `USARTRecoverRxIfStalled`，上层只给任务上下文时基与周期 |
| A.20 发送卡死复位够不到（链路永久静默）| ✅ 已修正 | 各链自己的"发送中"状态让它们不再调 `USARTTransmit`；暴露 `USARTRecoverTxIfStuck` 并在四条发送链的发送入口自检（§3.3.6）|
| B 职责收敛 / 模式传参 / 统一状态码    | ✅ 已落地 | 见 1.8                                      |

## 3. 失败路径与自恢复机制（发生时机 / 功能 / 原理）

> 本章讲"出错时 bsp 做了什么、为什么这么做、什么时候触发"。设计目标只有一条：
> **任何一次瞬态失败都不允许演变成永久停摆。**
> 两条主线：发送侧守"出口"（发不出去），接收侧守"入口"（收不进来）。

### 3.1 为什么需要自恢复：静默停摆

UART 的失效模式不是崩溃，是**链路悄悄死掉**，而且死得没有声音：

- **发送侧**：`bsp_log` / `drv_vofa` / `drv_terminal_lite` 都用 `timeout_ms == 0` —— 它们
  **永远不等、也永远不报超时**，拿不到发送权就只拿 `BSP_BUSY`。而它们的排队（`bsp_log` 的
  `WAIT_SEND` 槽）依赖 `tx_callback` 来排空，那个回调**只有发送成功才会来**。闭环就此形成：

  > 发送卡死 → 没有 tx_callback → `WAIT_SEND` 永不排空 → 后续日志全堆在队列直到池满丢弃。

  现象是**"日志突然没了"，而不是"日志报错"** —— 用来诊断问题的手段本身失效了。
- **接收侧**：常开流续收失败后 RX 永久停摆。对 `dbus` / `sbus` 是"遥控器永久失效且不自愈"
  （操作手失去控制权），对机载链路是"对端看门狗判离线"（`drv_daemon … OFFLINE` 的形态）。

一次**微秒级**的瞬态失败，代价却是整条链路的**永久死亡**。这个不对称性是全部自恢复机制的由来。

### 3.2 总览

| 环节         | 发送侧（出口）                                       | 接收侧（入口）                                    |
| ------------ | ---------------------------------------------------- | ------------------------------------------------- |
| 失败表现     | `gState` 停在 `BUSY_TX`                              | `rx_armed` 被误清 / 回调不再触发                  |
| 检测手段     | `s_tx_ready_us[]` + `USART_TX_STUCK_TIMEOUT_MS` 的 DWT 时长 | `USART_StartReceiveRaw()` 的返回码         |
| 纠正动作     | `USART_RecoverTx()`：`HAL_UART_AbortTransmit` + 复位 `gState` | `USART_ContinueReceive()` 重试启动；上层再 `USARTReceive` 时由 `USART_RecoverRx()` 清残留状态 |
| 通知上层     | `err_callback`（归还"发送中"的缓冲）                 | `err_callback`（RX 停摆）+ `rx_restart_fail` 计数 |
| 计数         | `tx_recover` / `tx_timeout`                          | `rx_restart` / `rx_restart_fail` / `rx_recover`   |
| 触发时机     | **任务上下文**：上层发送入口每帧调 `USARTRecoverTxIfStuck`（§3.3.6），另在 `USARTTransmit` 就绪失败路径兜一次；绝不在 ISR | ISR 内续收；**任务上下文**重启由上层触发（§3.7） |

### 3.3 发送侧

#### 3.3.1 失败返回点

`USARTTransmit` 的全部失败点都经 `USART_TxFailThenRet()` 计数（宏 `BSP_RETURN_IF_TRUE_LOG`
会求值 `ret` 参数）：

| 条件                                                | 返回               | 计入                              | 性质                       |
| --------------------------------------------------- | ------------------ | --------------------------------- | -------------------------- |
| `instance == NULL` / `handle == NULL`               | `BSP_PARAM_ERR`    | `tx_fail`                         | 调用方错误（未 Config 就发）|
| `mode > BSP_DMA_MODE`                               | `BSP_PARAM_ERR`    | `tx_fail`                         | 调用方错误                 |
| `data == NULL` / `len == 0`                         | `BSP_PARAM_ERR`    | `tx_fail`                         | 调用方错误                 |
| DMA 模式但 `hdmatx == NULL`                         | `BSP_PARAM_ERR`    | `tx_fail` + **`err_no_dma`**      | 配置问题（板子没接 TX DMA）|
| IT/DMA、非 READY、`timeout_ms == 0`                 | `BSP_BUSY`         | `tx_fail` + `tx_busy`             | **常态**，上层排队重试即可 |
| IT/DMA、非 READY、已超 `USART_TX_STUCK_TIMEOUT_MS`  | `BSP_BUSY`（已复位）| `tx_fail` + `tx_busy` + **`tx_recover`** | 卡死已自愈          |
| IT/DMA、非 READY、`timeout_ms > 0` 等超时           | `BSP_TIMEOUT`（已复位）| `tx_fail` + `tx_timeout` + **`tx_recover`** | 卡死已自愈      |
| `HAL_UART_Transmit*` 返回非 OK                      | `BSP_HW_ERR`       | `tx_fail`                         | 启动失败                   |

注意 `BSP_BUSY` **不是错误**，是流控信号：上层要么丢、要么排队，下一轮再来。

#### 3.3.2 卡死是怎么产生的

`gState` 停在 `BUSY_TX` 永不回 `READY`，成因三类：

1. **完成中断丢失** —— DMA 完成中断没来（DMA 流被别的外设抢占、传输计数错乱、M7 上 DCache
   未刷导致 DMA 取到脏数据）。`TxCplt` 是 `gState` 回到 `READY` 的**唯一**路径，它不来就永远不回来。
2. **HAL 错误收尾不彻底** —— `UART_DMAError` 只在 `gState == HAL_UART_STATE_BUSY_TX` 时复位状态，
   各 family / 各 HAL 版本处理并不一致（本工程同时用 F4 与 H7 两套 HAL）。
3. **不能在 ISR / 临界区里 Abort** —— `HAL_UART_AbortTransmit` 内部经 `HAL_DMA_Abort`，按
   `HAL_GetTick()` **自旋**等 DMA 关闭；高优先级中断或 `taskENTER_CRITICAL`（抬 BASEPRI）下
   tick 不前进 ⇒ **死等**（同 `bsp_i2c.c:154` 的坑，见 §A.3）。

第 3 条是关键：卡死恢复**只能放到"能安全自旋"的上下文**做。而要识别"现在到底是不是卡死"，
就必须有一个时间判据 —— `USART_TX_STUCK_TIMEOUT_MS` 就是这个判据。
**它不是"超时"，是"允许在任务上下文做 Abort 的入场券"。**
判据不满足时（`USART_CanBlockingAbort() == 0`）本次**不刷新** `s_tx_ready_us`，所以入场券不会
被白白用掉：下一次任务上下文的发送会立刻再次命中同一分支并补做复位。

#### 3.3.3 判定原理

`s_tx_ready_us[idx]` 记录**最近一次观察到 `gState == READY` 的 DWT 微秒时刻**，采样点四处：
`USARTConfig` 结尾、`USARTTransmit` 发现 READY 时、等待就绪成功时，以及 `USARTRecoverTxIfStuck`
发现发送空闲时（见 §3.3.6 —— 少了这一处，长期空闲后第一次发送会被旧基准误判成卡死）。
所以计时窗口 = **一次完整发送的墙钟时长**：

```
[s_tx_ready_us 采样]──┬─ DMA 传输 ─┬─ TxCplt 中断响应 ─┬─ 上层在回调里续发的调度延迟 ─┬─ 判定
                      └────────── 这一段必须 < 阈值 ──────────────────────────────┘
```

`bsp_log` / `drv_vofa` 都在 `tx_callback` 里直接续发，帧与帧之间的"READY 窗口"几乎为零，
**任务调度抖动会被整个算进这个窗口**。因此阈值必须 `> 最长帧时长 × 安全系数`（取 5~10）。

`timeout_ms == 0` 的调用方走这条判定；`timeout_ms > 0` 的调用方有轮询路径，等不到就按超时恢复。

#### 3.3.4 恢复动作与顺序（`USART_RecoverTx`）

```
⓪ USART_CanBlockingAbort()? // 否 → 直接返回 0，什么都不做（ISR / 临界区）
① HAL_UART_AbortTransmit()   // 中止在途发送（能安全自旋的上下文）
② gState 兜底置 READY        // Abort 有版本差异，未复位就手动补
③ tx_recover++ / 状态快照
④ err_callback(instance, USART_ERR_TX_ABORT)  // 被中止的发送不会再有 tx_callback
⑤ return 1                   // 调用方据此决定是否刷新卡死判据 / 打日志
```

**第 ④ 步的顺序不能颠倒。** 上层的归还判据是 `gState == HAL_UART_STATE_READY`（见 §1.6）——
含义是"HAL 确已收尾、不会再补一次完成回调"。若在 ①② 之前通知，上层看到 `gState != READY`
会当成"发送仍在途"而**拒绝归还**，等于没通知。这一步传 `USART_ERR_TX_ABORT`，handler 因此
不必再靠 `gState` 反推"是哪种情况"，也就能区分"硬件错误"与"本次发送确已被中止"。

#### 3.3.5 阈值取值

当前链路实测最长帧（8N1，10 bit/byte；`LOG_LEN_MAX = 256`、vofa `TX_BUFF_SIZE = (1+25)*4+4 = 108`、
`TERMINAL_LITE_RING_SIZE = 128`）：

| 波特率                    | log 256B | vofa 108B | terminal 128B | 200 ms 余量      |
| ------------------------- | -------- | --------- | ------------- | ---------------- |
| **921600**（当前所有板子）| 2.8 ms   | 1.2 ms    | 1.4 ms        | **72×**          |
| 115200（常见调试配置）    | 22.2 ms  | 9.4 ms    | 11.1 ms       | 9×               |
| 57600                     | 44.4 ms  | 18.8 ms   | 22.2 ms       | 4.5×             |
| 38400                     | 66.7 ms  | 28.1 ms   | 33.3 ms       | 3×               |
| 19200                     | 133 ms   | 56 ms     | 67 ms         | 1.5× ← 勉强      |
| 9600                      | **267 ms ← 会误杀** | 112 ms | 133 ms  | 0.8×             |

- **下限（不误杀）**：`> 最长帧时长 × 5`。改波特率 / 帧长后按 `帧字节 × 10 ÷ 波特率 × 1000 × 5` 重算。
- **上限（恢复够快）**：这个值同时就是**故障恢复时间**。设 5000 意味着日志断 5 秒才开始自愈。
  合理带 **100~500 ms**，默认 200。
- **不能设 0 或很小**：会把正常发送误判成卡死，每帧打断一次。
- ⚠️ **本宏是全局单值**，一个值管所有 UART。当前链路同构所以没问题；若将来出现
  "9600 慢速长帧链路 + 921600 日志链路"共存，须移进 `USART_Config_s` 做 per-UART 配置。

#### 3.3.6 判定在哪里被触发：上层发送入口每帧一次自检

上面的判定若只写在 `USARTTransmit` 内部，会有**一大片够不到的死角**：各发送链的"能不能发"
用的是**自己**的缓冲状态，而不是外设状态 ——

| 模块 | 就绪判据 | 卡死后的行为 |
| ---- | -------- | ------------ |
| `bsp_log` | 池里有没有 `LOG_BUF_SEND` 槽 | 在途槽永远 SEND、新日志全转 WAIT_SEND，池满后连排都不排 |
| `drv_vofa` | 三缓冲里有没有 `BUFF_ACTIVE` | 在途槽永远 ACTIVE、新帧全转 PENDING，占满后 `SetChannel` 静默失效 |
| `drv_terminal_lite` | `s_tx_now` 是否为 NULL | 在途槽永远 SEND、新内容全转 WAIT_SEND，池满后"tx pool full"这条日志也发不出去 |
| `comm/media` | `handle->gState` 是否为 READY | 每帧直接 `return -1`（连 memcpy 都不做）|

它们的共同点：**一旦有帧卡在"发送中"，就再也不会调用 `USARTTransmit`** —— 于是写在
`USARTTransmit` 里的卡死复位永远没有机会执行，链接永久静默，且任何错误计数都不增长
（`tx_fail` 都不涨，只有上层的 drop 计数在涨）。这就是为什么需要 `USARTRecoverTxIfStuck`
这个独立入口。

**调用点**：每条发送链的**发送入口最前面**（每帧必经之处，与"这帧最终有没有发出去"无关）：

| 模块 | 调用点 | 备注 |
| ---- | ------ | ---- |
| `bsp_log` | `BSPLogV` 开头（早于限频与借槽） | 必须早于借槽：池满时 `BSPLogV` 会提前 return，放在后面就永远够不到 |
| `drv_vofa` | `VofaSend` 开头 | 早于写时间戳/判 `VofaHasActive()` |
| `drv_terminal_lite` | `TerminalLiteSend` 开头（早于借槽） | 同上；调用点仍在 TX 唯一写者（本函数）内，不破坏"单写者"约定 |
| `comm/media` | `MediaUsartSend` 开头（早于 `gState` 忙判定） | 复位后 `gState` 已是 READY，本帧即可照常发出 |

**为什么放在"发送入口"而不是"自己的错误回调 / 任务里"**：

1. 入口是每帧必经的，**不依赖任何周期性事件**（RX 停摆能用 daemon 做时基，是因为"没收到帧"
   本身就触发 daemon；发送侧没有这种现成时基）；
2. 它天然只在**真正有东西要发**的时候才触发检查，不做无谓轮询；
3. 从 ISR 打日志（`BSPLOG`）也安全：入口内部先判上下文，ISR / 临界区里直接返回 `BSP_BUSY`，
   不动作也不刷新基准，下一次任务上下文的调用照常补上。

**复位成功后的连锁反应**（这是调用点必须在入口、且必须在借槽之前的原因）：
`USART_RecoverTx()` → `err_callback(TX_ABORT)` → 该链的 handler 归还"发送中"的槽
（`s_tx_buf = NULL` / `BUFF_IDLE` / `s_tx_now = NULL`）→ 回到调用点后本模块的 `LogBufSending()` /
`VofaHasActive()` / `s_tx_now` 已经是"空闲"，**当前这一帧随即就能正常发出去**，无需再等一轮。

**代价**：被中止的那一帧在线上是残缺的（对端按校验丢帧）；积压的 WAIT_SEND / PENDING
在下一次发送完成回调时带出。相比"永久静默"，这是必要的取舍。

### 3.4 接收侧

#### 3.4.1 常开流状态机

`rx_armed` 是接收侧唯一的状态位：

```
USARTReceive(IT/DMA) ──▶ rx_armed = 1 ──▶ [收到一帧] ──▶ rx_callback
        ▲                     │                            │
        │                     │                    USART_ContinueReceive
        │                     │                    成功 → 保持 rx_armed = 1
        │                     │                    失败 → rx_armed = 0（rx_restart_fail++）
        │                     ▼
        └── rx_armed = 0 且非上层主动停止 ⇒ RX 已停摆
            （上层在任务上下文调 USARTRecoverRxIfStalled 重启，见 §3.7）
```

#### 3.4.2 启动失败（`USARTReceive`）

| 条件                                          | 返回                          | 计入         |
| --------------------------------------------- | ----------------------------- | ------------ |
| `instance` / `handle` / `rx_buff` 为 NULL     | `BSP_PARAM_ERR`               | —            |
| `mode > BSP_DMA_MODE`                         | `BSP_PARAM_ERR`               | —            |
| `len == 0` / `len > rx_buff_size`             | `BSP_PARAM_ERR`               | —            |
| 常开流已在跑（`rx_armed`）                    | `BSP_BUSY`                    | —            |
| DMA 模式但 `hdmarx == NULL`                   | `BSP_PARAM_ERR`               | `err_no_dma` |
| BLOCK：HAL 返回 `HAL_BUSY` / `HAL_TIMEOUT`    | `BSP_BUSY` / `BSP_TIMEOUT`    | —            |
| HAL 启动失败（非 busy/timeout）               | `BSP_HW_ERR`                  | `err_start`  |
| `USART_StartReceiveRaw` 非 OK（非 BUSY）      | 原码                          | `err_start`  |
| 首次启动**非 OK**（含 `BSP_BUSY` 与 `BSP_HW_ERR`） | 复位后重试一次，仍失败即返回 | `rx_recover` |

最后一行是**上层重启能生效的前提**（`USART_RecoverRx`）：进入本函数时 `rx_armed == 0` 却启动失败，
说明 HAL 侧还残留着上次停摆的状态（DMA 流未清干净 / `RxState` 非 READY），
**不做强制复位的话，上层每次重启都只会拿到同样的失败，重启永远不生效**。
判据是 `ret != BSP_OK` 而非只认 `BSP_BUSY`：残留态也可能让 HAL 报 `HAL_ERROR`（DMA 流 EN 位
不自清那种，正是 ST 在 `HAL_DMA_Abort` 里留 *Errata 2.22* 补丁的状态），映射过来是 `BSP_HW_ERR`；
只认 busy 会漏掉这条**唯一没有恢复路径**的失败态。
复位动作与发送侧同源（`HAL_UART_AbortReceive` + 兜底置 `RxState = READY`），同样受
`USART_CanBlockingAbort()` 约束（`Abort` 经 `HAL_DMA_Abort` 按 `HAL_GetTick` 自旋，ISR / 临界区里会死等）。

#### 3.4.3 续收重试（`USART_RX_RESTART_RETRY`）

续收是"常开流"的下一帧启动，两种失败方式处理完全不同：

- **`BSP_BUSY` = 外设已经在收 ⇒ 必须当成成功。** 出现场景：错误回调与 `RxEventCallback` 前后脚
  都要求续收；或上层并发启动了 BLOCK 接收。**若当成失败会清 `rx_armed`，把一条健康的、正在跑的
  接收流误杀** —— 反而制造了停摆。所以 `USART_ContinueReceive` 把 `BSP_OK` 与 `BSP_BUSY` 同等视为
  "接收在跑"，只有 `BSP_HW_ERR` / `BSP_PARAM_ERR` 才重试并计 `rx_restart_fail`。
- **`HAL_ERROR` = 瞬态（重试可救）或持久（重试无效）。** `HAL_DMA_Abort_IT` 是**异步**的：错误路径
  发起 abort 后立刻返回，DMA 引擎可能还没真正停下，此刻马上重启会失败；重试给了一个时间窗。

取值：重试发生在 **ISR 内**，且是**同步紧密循环**（`USART_StartReceiveRaw` 是纯寄存器操作 +
`HAL_DMA_Start_IT`，不阻塞、不自旋），单次成本约微秒 —— **这就是值不能大的原因**。
默认 `3`（合理范围 2~4）；**不要设 0**（代码有兜底：`ret` 初值 `BSP_HW_ERR`，按失败处理）。

**能力边界**：这个机制**只能救瞬态**。要覆盖"需要时间才能停稳"的失败得在重试间插延时，而 ISR 里
不能 delay（`HAL_Delay` 依赖 tick，高优先级中断里 tick 不前进 → 和 Abort 同一个死锁坑）。
分工是：**重试负责瞬态，`rx_restart_fail` 负责告诉你瞬态救不了**。

**最终失败必须通知上层**：`USART_ContinueReceive` 返回 0 时由调用方负责通知 ——
`HAL_UARTEx_RxEventCallback` 会补一次 `err_callback`（这里没有别的通知路径）；
`HAL_UART_ErrorCallback` 忽略返回值（它末尾本来就会通知一次，重复虽幂等但没必要）。
漏掉这一步，上层对"RX 已停摆"就完全无感、只能靠数据超时兜底 —— 而 dbus / sbus 的失控检测
恰恰以数据流为触发源，会一起失效（见 §3.5）。

#### 3.4.4 错误后的续收条件

`HAL_UART_ErrorCallback` 里只在 `rx_armed && RxState == HAL_UART_STATE_READY` 时才续收：

- **阻塞型错误**（`DMAR` 置位，或 ORE/RTO）：HAL 已 `UART_EndRxTransfer` 把 `RxState` 置回 READY、
  关 RXNEIE/IDLEIE，DMA 异步中止完才回调 —— 常开流已断，**必须**重启。
- **非阻塞型错误**（PE/FE/NE，无 DMA）：HAL 不动 `RxState`、接收仍在继续，条件不成立、
  **不会误重启**（误重启会打断正在进行的接收）。

**错误回调里完全不动发送状态**：UART 的错误位（PE/FE/NE/ORE/RTO/DMA）**全在接收侧**，此刻
`gState != READY` 的常见含义恰恰是"有一次健康的发送正在途"，据此中止会杀掉正常发送。
发送卡死一律交由 §3.3 的 DWT 时长判定。

### 3.5 `err_callback` 契约（总结）

三种触发时机由 `reason` 参数区分，上层 handler 必须对三者都安全：

| reason                | 上下文   | 触发者                       | 含义                                          |
| --------------------- | -------- | ---------------------------- | --------------------------------------------- |
| `USART_ERR_HW`        | ISR      | `HAL_UART_ErrorCallback`     | 硬件错误；错误位全在接收侧，发送**可能仍在途** |
| `USART_ERR_TX_ABORT`  | **任务** | `USART_RecoverTx`            | 在途发送被强止，不会再有 tx_callback（gState 已复位） |
| `USART_ERR_RX_STALLED`| ISR      | `HAL_UARTEx_RxEventCallback` | RX 已停摆，不会再触发 `rx_callback`           |

要求：

1. **契约**：回调触发后不会再有本次传输的完成回调 ⇒ 上层须在此复位自身状态（归还"发送中"的缓冲）。
2. **幂等**：硬件错误可能连续触发，卡死恢复可能与硬件错误叠加；handler 必须"判自身状态再归还"，
   重复调用无副作用（现有 handler 都满足：`s_tx_buf == NULL` / `BUFF_ACTIVE` 与否 / `s_tx_now == NULL`
   先行返回，再判 reason）。
3. **无阻塞、可重入**，且不得在其中调 `USARTTransmit` / `USARTConfig` / `USARTReceive`。
4. **归还判据**：`TX_ABORT` 必归还（bsp 已确知本次发送终结）；`HW` 只在
   `gState == HAL_UART_STATE_READY` 时归还 —— 纯接收侧错误时发送可能仍在途，DMA 还在读那块缓冲。
   漏归还的残留由下一次发送完成回调兜底，即"宁可晚一拍，绝不提前"。
5. **不要在这里补发排队的帧**（`bsp_log` 的 WAIT_SEND / vofa 的 PENDING / terminal 的 WAIT_SEND）：
   时机②是在 `USARTTransmit` 的**调用栈内**触发的，补发等于嵌套调用 `USARTTransmit`，
   外层返回失败后还会写自己的 `s_tx_buf` / 状态数组，会把内层刚启动的槽覆盖掉 ——
   那个槽永久停在 SEND，反过来堵死整条发送链。积压的帧由下一次调用或 TxCplt 带出即可。
6. **按 reason 分流，别靠 `gState` 反推**：一个 handler 只对其中一种（最多两种）情况有意义——
   TX 侧模块（`bsp_log` / `drv_vofa` / `drv_terminal_lite`）只发不收，`RX_STALLED` 与它们无关，
   直接返回；RX 侧模块（`drv_dbus` / `drv_sbus`）根本不发，`TX_ABORT` 是**发送**被中止，与遥控器
   失联无关，若误当失联会让失控保护凭空动作，同样直接返回。
7. **接收停摆要靠它感知**：`rx_callback` 静默停掉时，上层若有"数据还在流"的判据必须在此判为异常。
   反面教材就是 dbus / sbus —— 失控检测原本只在 `RxCallback` 里更新，RX 停摆后回调不再触发，
   `signal_lost` 会**冻结在"正常"**，操作手失去控制权而系统毫不知情。
   两者现在都注册了 err_callback，并用 `rx_armed` 区分该不该反应：
   - `rx_armed == 0` ⇒ **真停摆**（续收最终失败、DMA 已停，此后不会再有任何一帧）⇒ 置 `signal_lost = 1`；
   - `rx_armed == 1` ⇒ 只是偶发硬件错误、bsp 已续收，数据流很快恢复 ⇒ **不动** ——
     否则一次总线噪声就会触发失控保护，属过激反应（这类错误只由 `s_usart_status[].err_*` 计数观察）。

   bsp 保证 `err_callback` 触发时 `rx_armed` 已反映本次续收结果（见 §3.4.4 的调用顺序）。

### 3.6 排查入口

见 §5 的状态变量表。最快的三条线索：

- `tx_recover` 在涨 ⇒ 发送卡死发生过（且已自愈）；稳定不涨说明链路健康。
- `rx_restart_fail > 0` ⇒ **RX 停摆发生过**；上层会自动重启（§3.7），看 `rx_recover` / `rx_start`
  是否继续增长即可判断是否已拉回来。同时 `dbus` / `sbus` 会把 `signal_lost` 置 1，属设计内的
  fail-safe 反应，数据一恢复就自动清零。
- `rx_recover > 0` ⇒ 重启时遇到过残留状态（HAL 说忙但软件侧没接收在跑），已强制复位后拉起来。
- `err_no_dma > 0` ⇒ 板子 / 工程配置问题（用了 DMA 模式但该口没接对应 DMA），不是运行期故障。

### 3.7 停摆后的重启由上层完成（任务上下文）

bsp 只能续收（ISR）和报告停摆，**重新启动接收必须由上层在任务上下文触发**，因为唯一的
"强制清残留状态"动作（`HAL_UART_AbortReceive`）不能在 ISR 里执行（见 §3.4.2）。

实现收在一个 bsp 入口 `USARTRecoverRxIfStalled(instance, period_ms)` 里：**判据（`rx_armed == 0`）、
上下文检查、限频、以及"按原参数重启"全部在 bsp 内完成**（重启参数取实例记着的
`rx_xfer_len` / `rx_mode`），上层只负责在自己已有的任务上下文时基上把它调起来、给一个限频周期。
这样四条链路的自恢复逻辑只有一份实现，行为不会各写各的而漂移：

| 模块                          | 任务上下文入口（不新增任务）        | 限频宏（作为 `period_ms` 传入）          |
| ----------------------------- | ---------------------------------- | ---------------------------------------- |
| `drv_dbus` / `drv_sbus`       | daemon 离线回调（`DaemonTask`，1ms）| `DRV_{DBUS,SBUS}_RX_RESTART_PERIOD_MS`   |
| `drv_comm`（USART 介质）      | 介质 `vtable.offline`（同上）      | `DRV_COMM_MEDIA_USART_RX_RESTART_PERIOD_MS` |
| `drv_terminal_lite`           | 模块小任务的空闲超时唤醒            | `TERMINAL_LITE_RX_RESTART_PERIOD_MS`     |
| `bsp_log` / `drv_vofa`        | 不需要（只发不收）                 | —                                        |

各调用点长这样（判据、参数、限频基准都不在调用方）：

```c
if (USARTRecoverRxIfStalled(dbus_inst->usart_inst, DRV_DBUS_RX_RESTART_PERIOD_MS) == BSP_OK)
    BSPLOG(&g_dbus_log, LOG_LEVEL_WARNING, "RX stalled, receive restarted");
```

几个共同的设计点：

1. **为什么用 daemon 做时基**：接收一旦停摆，`rx_callback` 不再触发，链路再无任何周期性事件可挂。
   而 daemon 恰好是"**没喂狗（没收到帧）**"才被触发的，且跑在 `DaemonTask`（任务上下文，1ms）——
   是这三个模块唯一现成的、与接收无关的任务上下文时基。代价是**依赖看门狗已启用**
   （`DaemonTask` 会跳过 `reload_count == 0` 的实例）—— 为此三个模块的 Config 都把
   `daemon_reload == 0` 提升为各自的默认值（`DRV_{DBUS,SBUS,COMM}_DAEMON_RELOAD_DEFAULT`，100），
   让"禁用监控"不再连带关掉自恢复（见 §7）。三个默认值都带 `_Static_assert(... != 0)`：
   若有人把它覆盖成 0，"提升"就成了空转、自恢复再次静默失效，这种配置直接**编不过**。
   `terminal_lite` 有自持小任务，用空闲超时唤醒即可，不依赖 daemon。
2. **判据是 `rx_armed == 0`，不是"daemon 离线"**：对端没开机、遥控器没开、用户没敲命令
   都会让 daemon 离线，那时接收本身是好的（`rx_armed == 1`），**不该去动它**。
   停摆时 bsp 已经把 `rx_armed` 清 0，两者可精确区分。
3. **必须限频**：`DaemonTask` 1ms一次，不限频就是 1000 次重启/秒（每次都 memset 接收缓冲 +
   试探 HAL），失败时还会刷屏日志。限频基准是 bsp 内的 `s_rx_restart_us[UART_NUM_MAX]`
   （按 UART 下标存"上次尝试时刻"，0 = 从未尝试、首次立即放行），所以上层不必各存一份时间戳。
   限频值取"明显小于上层判定链路失效的时间尺度"，本工程默认 20ms（comm 链路对端 2ms 发一帧）。
4. **按原参数重启**：重启用 bsp 实例里记着的 `rx_xfer_len` / `rx_mode`（即上次启动时的参数），
   而不是在各处重写 `rx_buff_size` + `BSP_DMA_MODE` —— 上层改了模式也不会重启错。
   为此 `USARTReceive` 在**所有失败分支之前**记录这两个字段：否则"Config 里启动接收失败"
   会让参数停在初值（长度 0、模式 BLOCK），此后的自恢复就永远起不来。
   反过来，BLOCK 分支**不写**这两个字段（只写 `s_usart_status[].rx_mode` 状态快照）：
   它们是"常开流的重启参数"，被 BLOCK 覆盖的话，停摆的常开流就再也重启不起来；
   只跑 BLOCK 的实例靠 Config 写入的初值（BLOCK / 0）就足以被本函数判成"没有可重启的流"。
5. **上下文不允许时只返回 `BSP_BUSY`，且不消费限频窗口**：`USARTRecoverRxIfStalled` 进
   `HAL_UART_AbortReceive` 前先过 `USART_CanBlockingAbort()`（IPSEL/PRIMASK/BASEPRI 全 0，
   见 §3.4.2）；被放到 ISR 或临界区里调用时安全地什么都不做，也不会白白吃掉一个重启周期。
6. **comm 侧没有用 `err_callback`**：它的接收现状本来就由链路看门狗（daemon 离线）表达，
   `rx_armed == 0` 这个判据在离线钩子里同样成立，故不再单独注册 err_callback。

## 4. 当前 cubemx 默认配置和原因

| key                         | value             | 原因                                                              |
| --------------------------- | ----------------- | ----------------------------------------------------------------- |
| Mode                        | Asynchronous      | 只用 UART 收发，不接同步时钟                                      |
| Baud Rate                   | 按外设定          | 日志/VOFA 用 921600 或 115200；两者都远快于卡死阈值的下限（取值方法见 §3.3.5）|
| Word Length                 | 8 Bits            | 通用                                                              |
| Parity                      | None              | 通用                                                              |
| DMA Request (TX)            | `DMA_NORMAL`      | 每次发送长度固定，无需循环；也避免循环 DMA 回卷语义               |
| DMA Request (RX)            | `DMA_NORMAL`      | 同上，配合 IDLE 事件由 HAL 自行收尾（见 A.7）                     |
| USART global interrupt      | Enable            | IT 模式与 IDLE 事件必需                                           |
| **DMA 中断（DMA 通道 IRQ）**| **Enable**        | 不使能则 `_DMA` 发送永远收不到完成回调（日志只发第一条就再无输出）|

- **RX 缓冲须在 DMA 可访问内存**：H7（Cortex-M7）上 `USART_INSTANCE_DEF` 已用 `DMA_RAM`
  （`bsp_map.h`，映射到 `.ram_d1`）自动处理；F4 无此限制。
- 只发不收的实例（如 `LOG_UART`）**可以不配 RX DMA**；若配了 DMA 模式却没配对应 DMA，
  调用点会报 `err_no_dma` + `BSP_PARAM_ERR`（见 A.9）。

## 5. 状态变量：错误码、错误计数等使用方式（调试用，调试器直接 Watch）

`volatile USART_Status_s s_usart_status[UART_NUM_MAX]`（类型定义于 `bsp_usart.c`，按 `uart_e` 索引）。
纯调试辅助：**只增不清**，需要清零可在调试器里直接写 0。同类还有 CAN 的
`s_bxcan_status` / `s_fdcan_status`、以及管初始化/任务超时的 `bsp_sys_status`。
另有 `static uint64_t s_rx_restart_us[UART_NUM_MAX]`（不对外、非计数）：每个口最近一次
"停摆重启"的尝试时刻，`USARTRecoverRxIfStalled` 的限频基准（0 = 从未尝试过）；
`static uint64_t s_tx_ready_us[UART_NUM_MAX]`：每个口最近一次确认"发送空闲"的时刻，
发送卡死判定的计时基准（见 §3.3.3）。

| 字段                                                       | 含义                                                       |
| ---------------------------------------------------------- | ---------------------------------------------------------- |
| `tx_ok`                                                    | 发送成功次数（BLOCK 直接成功 + IT/DMA TxCplt）             |
| `tx_fail` / `tx_busy` / `tx_timeout`                       | `USARTTransmit` 失败总数 / 其中 BSP_BUSY / 其中 BSP_TIMEOUT |
| `rx_ok` / `rx_start`                                       | 收帧成功次数 / 常开接收成功启动次数                        |
| `err_total`                                                | 错误回调总次数                                             |
| `err_pe` / `err_fe` / `err_ne` / `err_ore` / `err_dma` / `err_other` | 按 `huart->ErrorCode` 分类计数（**独立 if**，可同时置位）|
| `err_no_dma`                                               | 调 DMA 模式但该口没有对应 DMA（配置问题）                  |
| `err_start`                                                | HAL 启动收发失败（非 busy/timeout）次数                    |
| `tx_recover`                                               | 卡死的发送被强制复位次数（复位后会调 `err_callback`，让上层归还"发送中"的缓冲）|
| `rx_restart` / `rx_restart_fail`                           | 自动续收成功次数 / 续收最终失败次数（**`rx_restart_fail` 应恒为 0**，>0 表示 RX 曾停摆并被上层重启）|
| `rx_recover`                                               | 上层重启接收时发现残留状态（HAL 说忙 / `RxState` 非 READY）而被强制复位的次数 |
| `g_state` / `rx_state` / `rx_armed`                        | 最近一次采样的 HAL 状态 / 常开流是否在跑                   |
| `tx_mode` / `rx_mode`                                      | 最近一次发送 / 接收模式                                    |
| `error_code` / `err_time_us`                               | 最近一次 `huart->ErrorCode` / 出错时刻（DWT 微秒）         |
| `dma_tx_capable` / `dma_rx_capable`                        | Config 时记录：该口是否配了 TX/RX DMA（排除 A.9 时先看它） |
| `tx_len` / `rx_len`                                        | 最近一次发送 / 接收长度                                    |

**排查时的第一反应**：

- 日志/VOFA 没输出 → 看 `err_no_dma`（没配 DMA）、`tx_busy` 是否暴涨（上层多缓冲不够）、`tx_recover` 是否在涨。
- 通信时断时续 → 看 `err_ore`（来不及取数据，接收侧处理太慢）、`rx_restart_fail`（RX 真停摆了）。
- `g_state` 长期不是 0（READY）→ 发送卡死。

## 6. 验证

本机无串口硬件，重构后靠"全 app 编译 + 静态核对 + 符号级"验证（`cmake --build --preset <Debug|Release> --target <app>`，
覆盖 DM_MC02_HALF_RUDDER / DJI_C / DM_MC02 三块板与 F4/H7 两套 HAL）：

1. 六个 目标（3 app × 2 config）零警告、零错误。
2. `arm-none-eabi-nm` 确认 `bsp_usart.o` 只导出 `USARTRegister` / `USARTConfig` / `USARTTransmit` /
   `USARTReceive` / `USARTRecoverRxIfStalled` / `USARTRecoverTxIfStuck`，其余全为 `static`
   （`s_usart_status` / `s_rx_restart_us` / `s_tx_ready_us` 的可见性见 §5）；旧的三个接口符号已消失。
3. 残留检查：`grep -rn "USARTRecoverTransmit\|USARTRestartReceive\|USARTIsReady\|USART_Work_Mode_e" bsp drv` 零匹配
   （只剩注释里对旧名字的历史说明）。
4. 回归被牵动的 DRV：`bsp_log` / `drv_vofa` / `drv_terminal_lite` / `drv_dbus` / `drv_sbus` /
   `drv_comm/media/comm_media_usart`，Config 返回值不再被忽略。
5. 重启链路：`arm-none-eabi-nm` 在各目标 `Debug` 产物里确认 `USART_RecoverRx` /
   `DBUSUARTDaemonCallback` / `SBUSUARTDaemonCallback` / `MediaUsartOfflineHook` 存在。
   发送侧自恢复的调用点用 `grep -rn "USARTRecoverTxIfStuck" bsp drv` 核对：四条发送链各一处，
   都在发送入口开头（`BSPLogV` / `VofaSend` / `TerminalLiteSend` / `MediaUsartSend`），
   外加 `USARTTransmit` 就绪失败路径的兜底调用与 bsp 内的定义各一处。
   `drv_terminal_lite` 在当前板（DJI_C）**被 `app_cfg.h` 的 `TERMINAL_LITE_UART` 关掉了**
   （该分支是注释状态），实际代码不在产物里 —— 故另用同一条编译命令加
   `-DTERMINAL_LITE_UART=UART_1` 单独编一次，确认 `TerminalLiteRxRecover` 正常编出、零警告。

**待现场验证（无硬件）**：`s_usart_status[]` 各计数在真实链路的增长、`BSP_BUSY` 重试行为、
ORE/FE 之后 `rx_restart_fail` 是否保持 0（RX 未停摆）、人为断流后 `rx_restart` / `rx_recover`
是否在限频周期内拉回接收（可用 `s_usart_status[].rx_armed` 与上层 `signal_lost` 观察）。

## 7. 已知问题 / 后续

- **`app/example/app_cfg.h` 的 DJI_A 分支已失效**：`#define LOG_UART UART_1`，但
  `bsp/bsp_map/DJI_A/bsp_map.h` 的 `BoardUART_e` 里没有 `UART_1`（只有 `UART_SBUS/UART_2/UART_3/UART_7/UART_8/UART_4Pin_6`），
  把 `example` 切到 DJI_A 会编译失败。与本次重构无关，属模板里的历史遗留。
- **RX 停摆后的自动重启由上层触发、实现收在 bsp**（`USARTRecoverRxIfStalled`，见 §3.7），
  但有两条边界：
  - `dbus` / `sbus` / `comm` 的重启搭在 daemon 上，而 `DaemonTask` 会跳过 `reload_count == 0`
    的实例。三个 `Config` 因此把 `daemon_reload` 配 0 提升为默认值（100，见 §3.7 第 1 点），
    代价是**"禁用监控"这个用法不再可用**（链路静默 100ms 即判离线、会触发 `daemon_fault` 动作），
    换来自恢复不会因为少配一个字段而静默失效；
  - 若 `DAEMON_USED` 未开（`DaemonTask` 为空实现）或该实例的 `daemon` 指针为 NULL，
    提升也无从落地，自恢复仍失效；
  - `terminal_lite` 的重启靠小任务的空闲超时唤醒，故小任务被删/饿死同样失效。
- `comm/media` 的 UART 后端默认不注册 `err_callback`（其 `tx_buff` 是常驻 staging、无在途状态需复位，
  发送失败由 `MediaUsartSend` 的返回值 + `BSP_TIMEOUT`/`BSP_HW_ERR` 重试一次处理；
  `MediaUsartSend` 开头的 `USARTRecoverTxIfStuck` 自检复位成功时，因为没有 err_callback，
  本次被中止的在途帧只是线上残缺、不需要归还任何缓冲）；
  它的接收停摆靠 daemon 离线钩子 + `rx_armed` 判据感知（§3.7），链路是否可用的对外表达仍是
  `CommIsOnline`。上层若确需，可在 `CommMediaUsartConfig_s.usart.err_callback` 里自带一个，
  media 原样透传给 bsp（签名即 §1.6 的两参数版）。
- 后续：iic / spi 照本模板迁移（公共头 + 三模式传参 + 状态结构体 + 回调命名统一）；
  can / usb 最后迁到 `BSP_Status_e`。
