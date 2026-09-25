# bsp_usb 开发总结

> 本文件记录 bsp_usb（`bsp_usb.h` / `bsp_usb.c`）接口用法、踩过的坑与处理方式、
> 板级前置条件与自恢复机制。USB 的 CDC 中间件与描述符由 CubeMX 生成，本模块只做
> 实例管理 + 错误处理 + 自恢复。
>
> 本模块是 bsp_usart / spi / iic / can 之后的最后一个：返回值、`err_callback`、
> 状态快照、任务上下文恢复入口四项已与那几个模块对齐。

## 1. bsp_usb 接口使用说明

### 1.1 对外入口

| 接口                                          | 作用                                                     | 可重复调用 |
| --------------------------------------------- | -------------------------------------------------------- | ---------- |
| `USB_INSTANCE_DEF(name)`                      | 静态定义实例（内嵌 TX ring，放普通 RAM）                 | -          |
| `USBRegister(instance)`                       | 参数校验 + 防重入 static 管理数组                        | 否         |
| `USBConfig(instance, config)`                 | 写回调 / `parent` / `err_callback`，采集初始枚举状态      | 是         |
| `USBTransmit(instance, data, len)`            | 整帧写入 ring 并尝试立即发出一包                          | 是         |
| `USBRecoverTxIfStuck(instance, stuck_ms)`     | 任务上下文：判 TX 卡死，是则强制收尾并重试                | 是（幂等） |
| `USBRecoverRxIfStalled(instance, period_ms)`  | 任务上下文：判 RX 停摆，是则重新武装 OUT 端点             | 是（幂等） |
| `USBReenumerate(instance)`                    | 任务上下文：断开 D+ 再上拉，让主机重新枚举                | 是         |

其余一切（`is_ready` / 内部发送队列 / 多缓冲）都不对外暴露或不实现：忙由 `BSP_BUSY` 表达，
多缓冲与排队归上层。三条恢复入口只把**"何时看一眼"**交给上层，判据、限频、纠正动作都在这层。

### 1.2 统一状态码（`bsp/bsp_common/bsp_common.h`）

`USBTransmit` 的返回值把旧版压在同一个 `-1` 里的三件事拆开了：

| 返回码          | 含义                                                   | 上层该怎么做                     |
| --------------- | ------------------------------------------------------ | -------------------------------- |
| `BSP_OK`        | 整帧已入队并已尝试发出（**不代表已发出**）             | 继续下一帧                       |
| `BSP_BUSY`      | **环形缓冲放不下整帧，一个字节都没写**                 | 退避后重发**整帧**               |
| `BSP_HW_ERR`    | 未枚举（主机串口没打开 / 已拔出），整帧被拒            | 等枚举回来（或走 daemon 离线流程）|
| `BSP_PARAM_ERR` | `instance`/`data` 为空或 `len == 0`                    | 调用方 bug，不该重试             |

`USBRegister` / `USBConfig` 只返回 `BSP_OK` / `BSP_PARAM_ERR`（参数错与重复注册同一码），
不碰硬件（硬件初始化见 §4）。

自恢复入口的返回值约定与 `USARTRecoverTxIfStuck` / `USARTRecoverRxIfStalled` **一致**：

- `BSP_OK` = **本次刚做了恢复动作**（据此打"已恢复"日志）；
- `BSP_BUSY` = 未动作（链路健康 / 未超阈值 / 首次建立基准 / 未枚举）；
- `BSP_HW_ERR` = 试过但失败（或句柄未就绪）；`BSP_PARAM_ERR` = 无实例。

### 1.3 缓冲区与生存期

- **TX ring**：`USBInstance.tx_ring[APP_TX_DATA_SIZE]`（CubeMX 侧 `usbd_cdc_if.h` 给的 2048），
  留一格空位区分满/空 → 可用容量 **2047B**。`USBTransmit` 是**整帧原子入队**：
  放不下就一个字节都不写、返回 `BSP_BUSY`。上层可放心"失败就重发整帧"。
- **单包缓冲**：`tx_buf[USB_TX_BUF_SIZE]`（64 = CDC FS 单包上限），由 `bsp_usb_process_tx`
  从 ring 里取一段拷进来再交给 CDC。**它不是线程安全的**，靠 `tx_claim` 保证同一时刻只有一个
  上下文碰它（见 §2.B）。
- **RX 缓冲**：`instance->rx_buff` 指向 CubeMX 侧的 `UserRxBufferFS/HS`，每个 OUT 包由
  `CDC_Receive_HS` 重新挂同一块缓冲。**回调返回后该缓冲立刻可能被下一包覆盖**，
  所以 `rx_callback` 里必须同步 memcpy 走，不能存指针延后引用。
- USB **不走 DMA**（`usbd_conf.c` 里 `dma_enable = DISABLE`），所以缓冲放普通 RAM 即可，
  不需要 `DMA_RAM` 宏（旧版头文件那句"随实例整体放入 DMA_RAM"是错的，已改）。

### 1.4 回调契约

```c
typedef enum : uint8_t
{
    USB_ERR_HW = 0,             //!< 端点/PCD 层收尾失败；任务上下文。唯一触发点：TX 卡死收尾时
                                //!< HAL_PCD_EP_Abort 没成功（此时清 TxState 是唯一还起作用的动作）
    USB_ERR_TX_STUCK = 1,       //!< 在途 IN 传输卡死，已强制收尾并重试；任务上下文，TxState 已清
    USB_ERR_RX_STALLED = 2,     //!< 接收长期无进展，且重新武装最终失败；任务上下文
    USB_ERR_NOT_CONFIGURED = 3, //!< 枚举状态下降沿（未枚举/已拔出）：发送被拒，软件无从恢复
} USB_ErrReason_e;
```

- **全部在任务上下文触发**，且全部在**纠正动作之后**调用 —— 这是与 usart 的一处不同：
  USB 的中断（`CDC_Receive_HS` / `CDC_TransmitCplt_HS`）里不产生 `err_callback`，
  所以 handler 里可以直接做恢复动作，不必像 usart 那样区分 ISR / 任务上下文。
- handler 必须无阻塞、可重入、**幂等**（`USBRecover*` 会按阈值反复触发同一个 reason），
  且不得在其中调用 `USBTransmit` / `USBConfig` / 三条恢复入口（会在恢复路径上递归）。
- `rx_callback` / `tx_callback` 仍是 ISR 上下文，且 `rx_callback` 必须在回调内同步消费数据。
- `USBTransmit` 的失败**不从 `err_callback` 走**：它由返回码表达（与 usart/can 一致）。

### 1.5 相对旧版的破坏性变更

| 项                              | 旧                                     | 新                                                     |
| ------------------------------- | -------------------------------------- | ------------------------------------------------------ |
| `USBRegister` / `USBConfig` / `USBTransmit` 返回 | `int8_t` 0/-1               | `BSP_Status_e`（旧的 `ret != 0` 判断仍成立，ABI 兼容）  |
| ring 满                         | 写半截、丢尾，返回 -1                  | **零字节写入**，返回 `BSP_BUSY`                         |
| 未枚举                          | 与 ring 满同一个 -1                    | `BSP_HW_ERR`（单独计数 `tx_not_configured`）           |
| `CDC_Transmit` 的 `USBD_BUSY`   | 静默忽略                               | 计 `tx_busy` 并作为卡死判据的时间基准                   |
| 错误上报                        | 无                                     | `err_callback` + `s_usb_status[]`                       |
| 自恢复                          | 无                                     | `USBRecoverTxIfStuck` / `USBRecoverRxIfStalled` / `USBReenumerate` |

**`bsp_usb_rx_handler` / `bsp_usb_tx_complete_handler` / `bsp_usb_process_tx` 三个内部函数
的名字与签名刻意保持不变** —— 它们被四份 `cubemx/*/USB_DEVICE/App/usbd_cdc_if.c` 里的
`extern` 声明引用，一改名就要动 4 份 CubeMX 生成文件。

## 2. 之前踩过的坑与处理

### A. 硬件 / 中间件坑

**A.1 CDC 只有 64B 单包，没有 DMA**（`usbd_conf.c`：F4 `dma_enable = DISABLE`，
H7 同样是 `DISABLE`）
USB 全速 bulk 端点最大包长 64B，中间件按包收发。故任何"写一次就发完"的假设都不成立：
- 发送侧：`bsp_usb_process_tx` 一次最多提交 64B，整帧由 TX 完成中断逐包续发（ring 承担排队）；
- 接收侧：一次 `CDC_Receive_HS` 最多给 64B，长协议帧由 media 层分包重组。
上层（`comm_media_usb`）按 63B/片分包、每片加 1B 序号，正是为了适配这一点。

**A.2 `CDC_Receive_HS` 先重挂再上抛 ⇒ 回调返回后 `Buf` 立即可被覆盖**
CubeMX 生成的 `CDC_Receive_HS` 顺序是 `SetRxBuffer` → `ReceivePacket` → `bsp_usb_rx_handler`。
"先重挂"是好设计（不丢下一包），代价是**本包数据在回调返回后就可能被下一次 OUT 覆盖**。
处理：所有接收处理必须在回调上下文内同步 memcpy 累积，不能存 `usb->rx_buff` 指针延后读。

**A.3 `CDC_Receive_HS` 把 `USBD_CDC_ReceivePacket` 的返回值丢了 ⇒ RX 可能永久静默**
生成代码里 `(void)USBD_CDC_SetRxBuffer(...); USBD_CDC_ReceivePacket(...);` 都不看返回。
一旦那次重挂失败，OUT 端点不再被武装，`rx_callback` **永远不会再来**，
而且没有任何中断/回调能让你知道这件事。
处理：`USBRecoverRxIfStalled` 用"距上次收帧超过 `period_ms`"作为可观测判据，
重新挂回同一缓冲（见 §3.4）。

**A.4 `CDC_Transmit` 只会返回 `USBD_BUSY`（以及罕见的 `USBD_FAIL`）**
生成代码里它的判据是 `hcdc->TxState != 0` → 直接返回 `USBD_BUSY`。所以：
- 写不进外设**不是错误**，是正常的背压（上一包还在途），`tx_tail` 不推进、等完成中断再来一次是对的；
- 反过来，**`TxState` 卡在 1 不清就是真卡死**（`CDC_Transmit` 会永远 `USBD_BUSY`）——
  这就是 §3.3 唯一能治的故障。

**A.5 整包长恰是 64 的整倍数时，`USBD_CDC_DataIn` 会先发一个 ZLP**
`usbd_cdc.c` 的 `USBD_CDC_DataIn`：若 `total_length % maxpacket == 0`，先发 ZLP 且
**不调 `TransmitCplt`**，`TxState` 保持 1；ZLP 完成后再进一次 DataIn 才清 `TxState` 并回调。
所以"提交成功到收到完成回调之间"的正常间隔可能比一个包的时间长一倍，`USB_TX_STUCK_DEFAULT_MS`
（200ms）远大于它，不会误判。

### B. 设计重构

- **register/config/transmit 三段式 + 静态实例管理**（沿用 usart 的模板）。
- **`tx_claim`：修掉 `tx_buf` 的并发覆写竞态**（本轮新发现并修复）。
  `bsp_usb_process_tx` 有两个调用上下文：`USBTransmit`（任务）与 `CDC_TransmitCplt_HS`（ISR）。
  两者都会"memcpy 进 `tx_buf` → 交给 CDC"，而 CDC 的 IN 端点指向的正是 `tx_buf`。
  旧代码没有互斥，存在这条时序：任务侧 memcpy 到一半被 TxCplt 中断 → ISR 把 `tx_buf` 提交出去
  → 任务侧回来接着写 → **在途数据被覆写**。现在用一个 `tx_claim` 标志让"先到的把它做完"：
  ISR 抢不到就跳过，而跳过不会漏发 —— 提交成功的那一包完成后必然再来一次 TxCplt。
  （只关中断不可行：`HAL_PCD_EP_Transmit` 内部经 `USB_FlushTxFifo` 有基于 `HAL_GetTick`
  的超时自旋，关中断会让它死等。）
- **`tx_claim` 覆盖范围扩到整段 ring 写入**（本轮修的第二处，与上一条同一个根因）。
  提交逻辑（原 `bsp_usb_process_tx`）拆成"不加锁内核"`USB_ProcessTxLocked` + 公共包装
  （包装只负责判/持/放 `tx_claim`）；`USBTransmit` 改为**从写 ring 的第一个字节起就持 claim**，
  写完直接用 locked 版提交。否则 ISR 能在写入循环中途插进来，读到**写到一半的 `tx_head`**，
  把被截断的包提交给 CDC —— 对端按分包序号错位丢帧，末片还会凑出一帧坏数据。
  前提与上一条相同（单生产任务）；将来若有两任务并发调 `USBTransmit`，要改成 try-lock。
- **卡死判据改用"ring 有货 + 距上次成功入队超时"**，而不是"`TxState != 0`"：
  主机不读时 `TxState` 长期为 1 是正常现象（USB 全速 1ms 帧 + 主机轮询间隔），
  只有"明明有数据要发却迟迟一包都提交不出去"才是卡死。

### C. 当前处理状态

| 项                                    | 状态      | 说明                                                   |
| ------------------------------------- | --------- | ------------------------------------------------------ |
| A.1 64B 单包 / 无 DMA                 | ✅ 已适配 | 发送逐包续发（ring），接收由 media 分包重组            |
| A.2 回调返回后缓冲即失效              | ✅ 已约定 | 文档 + `rx_callback` 同步消费；media 层 memcpy 累积     |
| A.3 重挂返回值被丢弃 ⇒ RX 静默        | ✅ 已兜底 | `USBRecoverRxIfStalled`（§3.4）                        |
| A.4 `TxState` 卡死                    | ✅ 已处理 | `USBRecoverTxIfStuck`：EP_Abort + 清 `TxState`（§3.3） |
| A.5 64 整倍数发 ZLP                   | ✅ 已知   | 完成回调延迟一倍，不影响判据（阈值 200ms 远大于它）    |
| B `tx_buf` 并发覆写                   | ✅ 已修   | `tx_claim`                                             |
| B ring 写入中途被 ISR 提交 ⇒ 残帧     | ✅ 已修   | `tx_claim` 覆盖整段 ring 写入（`USB_ProcessTxLocked`） |
| B 环满写半截                          | ✅ 已改   | 整帧原子入队，放不下返回 `BSP_BUSY`                    |

## 3. 失败路径与自恢复机制（发生时机 / 功能 / 原理）

### 3.1 为什么需要自恢复：静默停摆

USB 的故障表现和串口一样是**静默**：

- TX：CDC IN 端点的在途传输卡住（`TxState` 停 1），之后每次 `CDC_Transmit` 都 `USBD_BUSY`
  → ring 只进不出 → 满 → `USBTransmit` 全部返回 `BSP_BUSY`。**没有任何中断会再来**，
  因为"再来"的前提正是"有一个包完成"。
- RX：`CDC_Receive_HS` 那次重挂失败 → OUT 端点不再被武装，`rx_callback` 永远不来（A.3）。
- 主机侧：串口被关掉 / 线被拔 → `dev_state` 掉出 `USBD_STATE_CONFIGURED`，发送全被拒。

三条的共同点是**链路侧拿不到周期性事件**，所以恢复动作必须由**上层自己的时基**来触发，
不能写在中断里（`bsp_usart.md` §3.6 的同一结论）。

### 3.2 失败返回点

| 接口                | 失败返回                                                                              |
| ------------------- | ------------------------------------------------------------------------------------- |
| `USBRegister`       | `BSP_PARAM_ERR`（实例为空 / 重复注册 / 超实例数）                                      |
| `USBConfig`         | `BSP_PARAM_ERR`（参数非法 / 实例未注册）                                               |
| `USBTransmit`       | `BSP_PARAM_ERR`（空指针 / `len == 0`）<br>`BSP_BUSY`（ring 放不下整帧，**零字节写入**）<br>`BSP_HW_ERR`（未枚举） |
| `USBRecoverTxIfStuck` | `BSP_BUSY`（未动）/ `BSP_HW_ERR`（收尾后仍出不去）/ `BSP_PARAM_ERR`                   |
| `USBRecoverRxIfStalled` | `BSP_BUSY`（未动）/ `BSP_HW_ERR`（重新武装被拒）/ `BSP_PARAM_ERR`                   |
| `USBReenumerate`    | `BSP_HW_ERR`（句柄未就绪 / DevDisconnect\|DevConnect 失败）/ `BSP_PARAM_ERR`           |

### 3.3 TX 卡死：判定与收尾（`USBRecoverTxIfStuck`）

**判定**（三个条件同时成立）：

1. `ring_used != 0`（真的有数据要发）；
2. `tx_last_ok_us != 0`（已建立过基准；从未成功提交过任何一包时先记时刻、本轮不动手，
   避免开机瞬间误判）；
3. `now - tx_last_ok_us >= stuck_ms`（默认 `USB_TX_STUCK_DEFAULT_MS = 200ms`）。

`tx_last_ok_us` **只在 `CDC_Transmit` 返回 `USBD_OK` 时刷新**（`bsp_usb_process_tx` 里），
所以这个基准的含义精确地是"最近一次真的把一包交出去的时刻"。

**收尾动作（顺序固定）**：

```
① HAL_PCD_EP_Abort(hpcd, CDC_IN_EP)   中止端点上那段在途传输（返回值不丢：失败则记下）
② hcdc->TxState = 0                   强制解卡
③ err_callback(① 失败 ? USB_ERR_HW : USB_ERR_TX_STUCK)
                                      动作之后再通报（告诉上层：这一包没发出去，别等它的完成回调）
                                      —— ① 失败说明"纠正动作本身没做成"，与"链路卡了"是两回事，
                                      USB_ERR_HW 是它在全模块唯一的触发点（见 §1.4）
④ tx_last_ok_us = now                 刷基准 → 本入口按 stuck_ms 限频
⑤ bsp_usb_process_tx() 重试一次       按 tail 是否推进 / TxState 是否重新为 1 判"是否恢复"
   → recover_ok / recover_fail
```

两点必须写进注释的事实：

- **触碰 `hcdc->TxState` 是本仓库已有做法**（`usbd_cdc_if.c` 的 `CDC_Transmit_HS` 自己就这么读它），
  HAL/中间件没有公开的"清 TxState"接口；
- **强制清 `TxState` 才真正解卡**（`CDC_Transmit` 的判据就是它非 0），
  代价是主机侧可能看到被截断的一包 —— `comm_media_usb` 的分包序号重组会丢掉这一帧。
  这是"卡死 vs 丢一帧"的取舍，宁可丢一帧。
  （① 的 EP_Abort 对"端点被主机停掉"是治本的，对"纯 `TxState` 卡住"不起作用，所以才要 ②。）

### 3.4 RX 停摆：重新武装（`USBRecoverRxIfStalled`）

**判定**：已枚举，且 `now - rx_last_us >= period_ms`（默认 `USB_RX_STALL_DEFAULT_MS = 100ms`；
`comm_media_usb` 的离线钩子传 20ms，因为 daemon 每 1ms 调一次，全靠它限频）。
`rx_last_us` 在检测到停摆时也被刷新，所以本入口每 `period_ms` 最多动作一次。

**动作**：`USBD_CDC_SetRxBuffer(&hdev, UserRxBufferFS/HS)` + `USBD_CDC_ReceivePacket(&hdev)`。
挂回的是 CubeMX 侧那块**非 static 全局** `UserRxBufferFS/HS`（与设备句柄一样由本文件自行 extern），
与 `CDC_Receive_HS` 每次重挂的是同一块。对已武装的端点重复 `HAL_PCD_EP_Receive` 不改变数据通路。

- 武装被**接受**只说明中间件收下了请求，**是否真恢复要等下一帧到来**（看 `s_usb_status[].rx_ok`
  是否继续增长），所以这条路径成功时**不**回调 `err_callback`（否则"对端本来就没发帧"会每次刷屏）；
- 武装被**拒** → `rx_rearm_fail++` + `err_callback(USB_ERR_RX_STALLED)` + 返回 `BSP_HW_ERR`。

### 3.5 枚举状态感知

`instance->dev_state` 保存最近一次采样的 `USBD_HandleTypeDef.dev_state`。
`USB_SampleDevState` 在**任务上下文**的入口（`USBTransmit` 与三条恢复入口）里比对，
检测到"已配置 → 非已配置"的**下降沿**时：`dev_state_drop++` + 日志 +
`err_callback(USB_ERR_NOT_CONFIGURED)`。

- `USBConfig` 会先采一次初始状态，避免开机时"尚未枚举"被算成一次下降沿；
- **收帧入口（ISR）不做边沿检测**：USB 中断里不产生 `err_callback`（见 §1.4），
  那里只刷新 `rx_last_us` 与快照。

这条给 daemon / app 一个干净的"对端不在"信号，替代旧版"只能从一串 `-1` 间接察觉"。

### 3.6 为什么 `USBReenumerate` 只提供、绝不自动调用

它的实现是 `HAL_PCD_DevDisconnect` → 忙等 20ms → `HAL_PCD_DevConnect`（两个 API 在 F4/H7
的 PCD HAL 里都有）。**它会让 PC 端 COM 口消失再出现**：主机侧打开着的串口会掉线，
需要重新打开，上位机若没处理会以为链路彻底断了。所以：

- `bsp_usb.c` 的**任何**恢复路径都不调它（`grep` 可确认只有一个定义 + 一个声明）；
- 是否用、什么时候用由 app 决定；
- 它阻塞约 20ms（DWT 忙等，不依赖 RTOS），**不要放进控制周期**。

### 3.7 谁在什么时基上触发这些恢复

| 触发点                                  | 调用的恢复入口                        | 时基                       |
| --------------------------------------- | ------------------------------------- | -------------------------- |
| `comm_media_usb(_simple)` 的发送入口     | `USBRecoverTxIfStuck(usb, 0)`         | comm 周期发送（本工程 2ms）|
| 同上，`vtable->offline` 钩子             | `USBRecoverTxIfStuck` + `USBRecoverRxIfStalled(usb, 20)` | `DaemonTask` 1ms（离线期间）|
| app（可选，本轮未接）                    | `USBReenumerate`                      | 自行决定                   |

`CommConfig` 会把 `daemon_reload == 0` 提升为默认值 —— 但**只对挂了 `offline` 钩子的后端**生效。
USB 两个后端现在都挂了钩子，所以这条自恢复通道不会被"禁用监控"误关
（`drv_comm.c` 的对应注释已同步更新）。

## 4. 板级前置条件与现状

| 能力                  | 前置条件                                                        | DJI_A / DJI_C (F4) | DM_MC02 / DM_MC02_HALF_RUDDER (H7) |
| --------------------- | --------------------------------------------------------------- | ------------------ | ---------------------------------- |
| 外设 / 速度           | F4：`USB_OTG_FS` 原生 FS；H7：`USB_OTG_HS` + **内嵌 FS PHY**     | ✅ FS              | ✅ 实际也是 FS                     |
| 单包上限              | 统一按 FS 的 64B（`USB_TX_BUF_SIZE = CDC_DATA_FS_MAX_PACKET_SIZE`）| ✅               | ✅                                 |
| DMA                   | `usbd_conf.c` 的 `dma_enable` 必须为 `DISABLE`（本就是）——启用后缓冲要移进 RAM_D1/D2 | ✅ DISABLE | ✅ DISABLE |
| 端点 abort / 重枚举   | `HAL_PCD_EP_Abort` / `HAL_PCD_DevConnect` / `HAL_PCD_DevDisconnect` 在 F4/H7 的 PCD HAL 里都有 | ✅ | ✅ |
| 枚举状态              | `USBD_HandleTypeDef.dev_state`（中间件维护）                      | ✅                 | ✅                                 |
| 实例数                | `USB_INSTANCE_NUM = 1`（`bsp_map.h`）→ **本模块按单活动实例设计** | ✅                 | ✅                                 |

**硬件初始化时机**：`MX_USB_DEVICE_Init()` 由 CubeMX 生成在 `freertos.c` 的
`StartDefaultTask`（`osPriorityIdle`）里调用，**不是**旧注释说的"`osKernelStart()` 之前"。
真正有效的约束是**"必须早于第一次 `USBTransmit`"**：未初始化时 `hUsbDeviceHS` 是全零结构体，
`pClassData` 为 NULL，本层会把它判成"未枚举"（`USB_Config`/`pClassData` 判空）而**不会崩**。
实测可用（每个周期任务都会阻塞让出，idle 能跑到），故本轮只把注释改成与现状一致、不动 cubemx ——
搬初始化需要实机验证，而本机没有 USB 从机。

⚠️ 若 CubeMX 重新生成又在别处加一次 `USBD_Init`，必须删掉：重复 `USBD_Init` 会重新注册类并重启 PCD，
导致枚举异常。

## 5. 状态变量（调试用，调试器直接 Watch）

`volatile USB_Status_s s_usb_status[USB_INSTANCE_NUM]`（定义于 `bsp_usb.c`）。
纯调试辅助：只增不清，需要清零可在调试器里直接写 0。

| 字段                                                                        | 含义                                                     |
| --------------------------------------------------------------------------- | -------------------------------------------------------- |
| `tx_ok` / `tx_pkt_ok`                                                       | `USBTransmit` 整帧入队次数 / 单包成功提交给 CDC 次数      |
| `tx_fail` / `tx_ring_full` / `tx_param_err` / `tx_not_configured`            | `USBTransmit` 非 OK 总次数及其细分（环满 / 参数错 / 未枚举）|
| `tx_busy` / `tx_hw_fail`                                                    | `CDC_Transmit` 返回 `USBD_BUSY` / 其它非 OK 次数          |
| `rx_ok`                                                                     | 收帧次数                                                 |
| `tx_stuck` / `tx_recover_ok` / `tx_recover_fail`                            | TX 卡死判定次数 / 收尾后恢复 / 仍出不去                   |
| `rx_stalled` / `rx_recover_ok` / `rx_rearm_fail`                            | RX 停摆判定 / 重新武装被接受 / 被拒                       |
| `dev_state_drop`                                                            | 枚举状态下降沿次数（主机串口关闭 / 拔出）                 |
| `dev_state` / `dev_speed`                                                   | 最近一次采样的 `dev_state` / `dev_speed`                  |
| `tx_state` / `tx_claim`                                                     | `hcdc->TxState`（非 0 = 有包在途；0xFF = 句柄未就绪）/ 提交中标志 |
| `tx_head` / `tx_tail` / `ring_used`                                         | ring 生产/消费位置与待发量（**长期不减 = 发送卡住**）      |
| `rx_last_us` / `last_err_us`                                                | 最近一次收帧 / 最近一次错误或停滞的 DWT 时间戳            |

对应的业务侧镜像：`comm_media_usb(_simple)` 的 `tx_fail` / `tx_busy` / `err_count` / `last_err`。

## 6. 验证（本机只能用"全 app 编译 + 静态核对"）

本机没有 USB 从机（也没有 CAN），所以：

1. **编译**：`cmake --preset default`，然后
   `cmake --build --preset {Debug,Release} --target {half_rudder_gimbal,half_rudder_chassis,example}`
   = 6 配置全绿、零警告零错误。重点看 `-Wall` 下 `err_callback` 未用参数、
   `BSP_Status_e` 窄化赋值、`BSPLOG` 关闭时的未使用变量。
2. **残留检查**（只读 grep）：
   - `int8_t USBRegister|USBConfig|USBTransmit` → 零匹配；
   - `USBIsReady` → 零匹配；
   - `USBReenumerate` 在 `bsp_usb.c` 内只有一个定义、无内部调用点。
3. **符号级要查 `.o`，不要查 `.elf`**：本工程开了 `-ffunction-sections` + `-Wl,--gc-sections`
   （`CMakeLists.txt:226,243`），**零调用者的函数会被链接器丢掉**。所以 `USBReenumerate`
   在 `.elf` 的 `nm -g` 里查不到（`USBRecoverTxIfStuck` / `USBRecoverRxIfStalled` 被 media
   调用了，故在 `.elf` 里可见）。正确查法是查目标文件：
   `arm-none-eabi-nm -S build/CMakeFiles/half_rudder_gimbal.dir/Debug/bsp/bsp_usb/bsp_usb.c.obj`
   → 应有 `T USBRecoverTxIfStuck`(0x1C4) / `T USBRecoverRxIfStalled`(0x174) /
   `T USBReenumerate`(0xB0) / `B s_usb_status`(0x60)。
4. **待现场验证（无硬件）**：
   - 卡死注入：调试器强写 `hcdc->TxState = 1`，看 `USBRecoverTxIfStuck` 能否解开、`tx_stuck`/`tx_recover_ok` 增长；
   - 拔插：`dev_state_drop` 与 daemon 离线钩子的联动、`err_callback(USB_ERR_NOT_CONFIGURED)` 的时机；
   - RX 停摆：让主机停发，看 `rx_stalled` / `rx_recover_ok` 是否按 20ms 周期增长；
   - 真实视觉链路上各计数是否按预期增长（正常应只有 `tx_ok` / `tx_pkt_ok` / `rx_ok` 增长）。

## 7. 已知问题 / 后续

- **多设备枚举未做**：`USB_INSTANCE_NUM = 1`，本模块按单活动实例设计
  （`s_active_inst` + CubeMX 侧唯一的 `UserRxBuffer/HCDC` 句柄）。多实例是待办，
  不是本模块已支持的用法。
- **ZLP 语义**：整包长恰为 64 的整倍数时，`USBD_CDC_DataIn` 会多发一个 ZLP（见 A.5）。
  对端如果是"按固定长度读"的上位机，多出来的 ZLP 会被当成一个 0 长度包 ——
  `comm_media_usb` 的短帧透传版对长度不符的包直接丢弃并计 `lost_frames`，
  所以不会错位，但那一帧会被丢。本轮不动。
- **`tx_buf` 的截断代价**：`USBRecoverTxIfStuck` 强制清 `TxState` 时，主机侧可能看到被截断的一包。
  已由 media 的序号重组兜住（丢一帧），但若将来有"单包透传、无序号"的链路，
  这一帧会以"长度不符"被丢 —— 同 ZLP 那条，属可接受损失。
- **`err_callback` 没有 ISR 触发源**：这是有意的（USB 中断里不做恢复动作），
  但也意味着"PCD 层 suspend/resume/disconnect 回调"本层**没有接**，
  `dev_state` 只能靠任务侧主动采样。若将来需要在拔出瞬间就做出反应，
  应去 `usbd_conf.c` 的 `HAL_PCD_*Callback` 里挂钩子。
