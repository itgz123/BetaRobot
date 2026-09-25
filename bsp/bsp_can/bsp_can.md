# bsp_can 开发总结

> 本文件记录 bsp_can（`bsp_can.h` / `bsp_bxcan.c` / `bsp_fdcan.c`）接口用法、
> 踩过的坑与处理方式、CubeMX 默认配置及原因。git 提交号为历史溯源参考。
>
> 返回值已于 bsp_usart / spi / iic 之后统一迁到 `BSP_Status_e`（见 §6），
> 旧版 `int8_t` 的 0/-1 现已废弃。

## 1. bsp_can 接口使用说明

### 1.1 四个入口

| 接口                                                                 | 作用                                        | 可重复调用                   |
| -------------------------------------------------------------------- | ------------------------------------------- | ---------------------------- |
| `CAN_INSTANCE_DEF(name)`                                             | 静态定义实例（仅身份绑定）                  | -                            |
| `CANRegister(instance)`                                              | 参数校验 + 防重入 static 管理数组           | 否（重复注册返回 BSP_PARAM_ERR） |
| `CANConfig(instance, config)`                                        | 填硬件映射/模式/过滤器 + 首次初始化外设     | 是（已初始化则跳过硬件步骤） |
| `CANTransmit(instance, pack, timeout_ms, tx_mailbox, tx_free_level)` | 发送一帧                                    | 是                           |
| `CANRecover(instance)`                                               | 任务上下文的总线/发送资源自恢复（见 §6.4）  | 是                           |

### 1.2 发送接口签名（注意！）

```c
BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack,
                         uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level);
```

- `timeout_ms`：BxCAN 邮箱 / FDCAN Tx FIFO 满时等待其空闲的上限；传 0 表示不等待、资源满立即返回失败。
  等待为**阻塞轮询**（DWT 计时），**在中断上下文里会被本层自动钳为 0**（ISR 不等待，只试一次，
  满即 `BSP_BUSY`，计 `tx_isr_clamp`）——见 §6.5。
- `tx_mailbox` 出参：BxCAN=邮箱索引 0~2 / FDCAN=**不透明的 MessageMarker**（内部编码见 §6.3），
  发送完成回调据此对应发送帧——只应原样回传比对，不要解释其数值。
- `tx_free_level` 出参：发送后剩余可发送数。
- 返回值：`BSP_OK` = **已加入发送资源**（不代表已发出，逐帧结果看 `tx_complete_callback`）；
  失败分 `BSP_BUSY`（资源满且不等待）/ `BSP_TIMEOUT`（等超时，实现已顺手取消占用的邮箱）/`BSP_PARAM_ERR` / `BSP_HW_ERR`。
- 逐帧结果回调（3 参数版，本轮新增结果参数）：

```c
void (*tx_complete_callback)(CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result);
/* result = BSP_OK      ：这一帧真的发到总线上了
 * result = BSP_HW_ERR  ：这一帧没发出去（仲裁丢失 ALST / 发送错误 TERR / 被取消 / Tx Event 丢失） */
```

- 相对旧版的变更（**只有返回类型与回调元数**，`CANTransmit` 的参数表本轮未动）：
  三个接口的返回值 `int8_t`（0 / -1）→ `BSP_Status_e`；
  `tx_complete_callback` 由 2 参数变 3 参数（新增 `result`）。
  上层（drvs_motor / drv_comm media）若仍按 `!= 0` 判返回值或写旧 2 参回调，需适配。

### 1.3 可选的 hal_can 重配置入口

一般情况不需要调用 hal_can。若想覆盖外设时序等配置，可在 app 层用 HAL 原生参数覆盖：

```c
FDCAN_InitTypeDef init = hfdcan->Init;
init.NominalPrescaler = ...; // 修改需要的项
HalCanReconfigureFdcan(hfdcan, &init);   // FDCAN (H7)
HalCanReconfigureBxcan(hcan, &init);     // BxCAN (F4)
```

## 2. 之前踩过的坑与处理

### A. 硬件坑

**A.1 FDCAN Tx FIFO 无总线信号时一直满 → 必须超时等待 + 高精度计时**（`f18a0e7 → 0811e28 → 5e9ecf9`）
无总线信号/对端离线时 FIFO 持续占满。发送前若 `free_level==0` 直接失败会误杀合法帧；死等会卡死任务。
处理：满则轮询等待，超时上限由 `timeout_ms` 控制（0 表示不等待、满即失败），
用 DWT 64 位微秒（`DWT_GetTimeUs()`，不受中断影响、无回绕）而非 `HAL_GetTick()`。

**A.2 Bus-off / 错误检测：使能错误中断；F4/H7 恢复机制不同；独立 if 防漏报**（`3ebbaa3`）

- 不使能错误状态中断（FDCAN `FDCAN_IT_BUS_OFF/ERROR_WARNING/ERROR_PASSIVE`，
  BxCAN `CAN_IT_BUSOFF/ERROR_WARNING/ERROR_PASSIVE/LAST_ERROR_CODE`）时总线异常无上报。
- H7 FDCAN 无软件手动恢复接口，bus-off 后**清 `CCCR.INIT` 触发硬件自动恢复**；
  F4 BxCAN 靠 CubeMX `AutoBusOff=ENABLE`（硬件自动重同步）+ `HAL_CAN_ResetError()` 清软件错误标志。
- **三个错误状态可同时置位**，用 `else if` 只记录第一个，必须**独立 if** 分别记录。
- F4 专属：`HAL_CAN_IRQHandler` 错误分支以 **`IER.ERRIE`（`CAN_IT_ERROR`）为主开关**，
  漏配 `CAN_IT_ERROR` 时错误中断根本不触发。
  处理：FDCAN `HAL_FDCAN_ErrorStatusCallback()`（`GetProtocolStatus` + 清 `CCCR.INIT` + 独立 if 日志）；
  BxCAN `HAL_CAN_ErrorCallback()`（独立 if BOF/EPV/EWG + 协议错误汇总 + `HAL_CAN_ResetError()`）。

**A.3 BxCAN 16 位过滤器移位 bug（已由设计规避）**（`082ae7e`）
16 位滤波器标准帧 ID 需 `<<5` 而非 `<<3`；空槽填 0 会误匹配 ID=0。
处理：硬件过滤全通 + 软件过滤，绕开 16 位寄存器与移位细节。

**A.4 FDCAN DUAL 过滤器只能匹配 2 个 ID（已由设计规避）**
每个 Dual 过滤器只能精确匹配 2 个 ID，多 ID 需 `ceil(n/2)` 个过滤器，受 Message RAM 限制。
处理：硬件过滤全通，软件过滤器（MASK/LIST/RANGE）无成对/数量限制。

**A.5 HAL_FDCAN_ConfigInterruptLines 是覆盖式 → 同一中断线源必须合并**
对 ILS 是覆盖式，多次调用互相覆盖。处理：接收/发送事件/错误状态所有源合并进 `line0_ints` 后一次调用映射到 IT0。

**A.6 tx_len 超 FDCAN TxElmtSize → 越界写 Message RAM**
`HAL_FDCAN_AddMessageToTxFifoQ()` 内部 `FDCAN_CopyMessageToRAM()` 按 len 拷贝，超 TxElmtSize 会越界写 Message RAM（内存破坏）。
处理：`CANTransmit` 入口用 `FDCAN_ElmtSizeToBytes()` 校验，`pack->len > elmt_bytes` 时**硬拒绝**（仅警告不够）。
len>64 由 `FDCAN_BytesToDlc()` 返回 -1 兜底。

### B. 设计重构

- **register/config 拆分**：可重复调用、防重；结构体传参；硬件枚举放 config；移除 `CANSetDLC`。
- **async 发送回调**：`tx_complete_callback(instance, tx_mailbox, result)` 帧发完触发（成功与失败都触发，
  见 §6.3），支持 comm 异步分包续发。
- **tx_id 冲突降级为警告**（`a221ab3`）：DJI 多电机共用 `0x200`，硬拒绝会卡死；rx_id 冲突保持硬拒绝。
  当前精简版实例不登记固定 tx_id，逐帧指定，冲突责任交回调用方。
- **初始化标志**（`5d10a16`）：独立 `s_fdcan_started[]` / `s_can_started[]`，全部初始化成功后才置位，
  任一步失败可重试。HAL 的 `ConfigGlobalFilter`/`Start` 等要求 READY 态，中途失败可能停在 BUSY/LISTENING，
  重试前先 `HAL_FDCAN_Stop`/`HAL_CAN_Stop` 停回 READY 归一。

### C. 当前处理状态

| 项                     | 状态      | 说明                                       |
| ---------------------- | --------- | ------------------------------------------ |
| A.1 Tx FIFO 满         | ✅ 已处理 | 超时等待（DWT，`timeout_ms` 可配）         |
| A.2 Bus-off / 错误检测 | ✅ 已处理 | 错误中断 + 独立 if 上报 + 各自恢复机制     |
| A.3 BxCAN 16 位过滤器  | ✅ 已规避 | 硬件过滤全通 + 软件过滤                    |
| A.4 FDCAN DUAL 过滤器  | ✅ 已规避 | 软件过滤器无成对/数量限制                  |
| A.5 中断线覆盖式       | ✅ 已处理 | 源合并后一次 `ConfigInterruptLines`        |
| A.6 len 超 TxElmtSize  | ✅ 已处理 | 发送入口硬拒绝                             |
| B register/config 拆分 | ✅ 已落地 | 可重复调用、防重                           |
| B async 发送回调       | ✅ 已落地 | 溯源表：先登记再入队、先清槽再回调         |
| B 初始化标志           | ✅ 已落地 | `s_*_started` + 失败可重试（含 Stop 归一） |

## 3. 当前 cubemx 默认配置和原因

### FDCAN (H7, DM_MC02)

| key                 | value                   | 原因                                       |
| ------------------- | ----------------------- | ------------------------------------------ |
| Frame Format        | CAN_FRAME_FORMAT_FD_BRS | CAN_FRAME_FORMAT_FD_BRS模式下可以发送3种帧 |
| Auto Retransmission | Enable                  | 可以要                                     |
| Transmit Pause      | Disable                 | 减小延迟                                   |
| Protocol Exception  | Enable                  | 可以提高兼容性                             |

### BxCAN (F4, DJI_A / DJI_C)

| key                               | value   | 原因                                                                                                                                     |
| --------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Time Triggered Communication Mode | Disable | 没必要                                                                                                                                   |
| Automatic Bus-Off Management      | Enable  | 不然就要手动恢复，之前吃过亏                                                                                                             |
| Automatic Wake-Up Mode            | Disable | 不会休眠，没必要                                                                                                                         |
| Automatic Retransmission          | Enable  | 可以要                                                                                                                                   |
| Receive Fifo Locked Mode          | Disable | Disable情况下，FIFO满时新数据覆盖最旧的数据（控制情况下新数据价值高于旧数据），fdcan没有这个配置选项，默认情况下和bxcan的Disable行为一致 |
| Transmit Fifo Priority            | Enable  | 和fdcan的fifo保持一致                                                                                                                    |

### 错误中断 / 自恢复的板级前置条件

§6 的失败路径与自恢复**全部依赖这张表里的中断与参数**；漏配不会编译报错，只会让对应能力静默失效。

| 能力                              | 前置条件（CubeMX / 生成代码）                                                                                          | DJI_C (F4)                    | DM_MC02_HALF_RUDDER (H7)     |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------- | ----------------------------- | ---------------------------- |
| bus-off / error warning / passive | F4：`CAN_IT_ERROR`（IER.ERRIE 主开关）+ SCE 相关 IT；H7：`FDCAN_IT_BUS_OFF/ERROR_WARNING/ERROR_PASSIVE`                | ✅ 已开（`can.c` 的 IT 配置） | ✅ 已开                      |
| 逐帧失败结果（`result=BSP_HW_ERR`）| H7 必须 `TxEventsNbr > 0` 且 `FDCAN_IT_TX_EVT_FIFO_*` 已使能；F4 靠 RQCP/TXOK/ALST/TERR 位，无需额外配置             | ✅ 天然可用                   | ✅ `TxEventsNbr > 0` 已配    |
| 中断线映射                        | H7 `HAL_FDCAN_ConfigInterruptLines` 是**覆盖式**，所有源（RX / Tx Event / 错误）必须合并成一次调用映射到 IT0（见 A.5） | -                             | ✅ 已合并                    |
| `CANRecover` 的取消动作           | F4 `HAL_CAN_AbortTxRequest`：只写 `TSR.ABRQx`，对 ISR 安全；H7 `HAL_FDCAN_AbortTxRequest`：只写 `TXBCR`            | ✅ 无需额外配置               | ✅ 无需额外配置              |

## 4. can 的状态：错误码，错误计数等使用方式

当前实现只做「错误上报 + 总线恢复 + 逐帧失败回传」，尚未上虚拟错误帧/错误码分发（错误码之后再说）：

- **FDCAN (H7)**：`HAL_FDCAN_ErrorStatusCallback()` 读 `FDCAN_ProtocolStatusTypeDef`（`BusOff`/`ErrorPassive`/`Warning`/
  `LastErrorCode`/`DataLastErrorCode`），bus-off 清 `CCCR.INIT` 触发硬件自动恢复，三个状态独立 if 打日志。
  另注意：FDCAN 的 `HAL_FDCAN_ErrorCallback` 是**无参数回调**（不是"死代码"）——IRQHandler 末尾在
  `hfdcan->ErrorCode != HAL_FDCAN_ERROR_NONE` 时调用一次，但错误位信息不在回调参数里，而在 `hfdcan->ErrorCode`
  **粘滞位**：错误位（PEA/PED/ELO/WDI/ARA/RAM_ACCESS）由 IRQHandler 各分支 `ErrorCode |= 位` 累积，置位后保持不清，
  软件必须显式清零；不清零则只要 ErrorCode 非零，每次进 IRQHandler 都会重复触发 ErrorCallback（"非零即上报"而非"新增才上报"）。
  其中 `FDCAN_IT_RAM_ACCESS_FAILURE`（Message RAM 访问失败，配置级严重故障）已使能，并在 Rx/错误回调里
  采样计数 + `CLEAR_BIT` 清零（`err_ram_access`，读→计数→清 是处理粘滞位的标准姿势）；
  PEA/PED/ELO/WDI/ARA 因**未使能**对应 IT，IRQHandler 的 Errors 分支（`IR & MASK & IE`）为 0，不进 ErrorCode，
  不会粘滞；其中 PEA/PED 协议错误由 `err_event`（ECR.CEL 差值）覆盖，不单独使能 IT（避免位错误中断风暴）。
- **BxCAN (F4)**：`HAL_CAN_ErrorCallback()` 用 `HAL_CAN_GetError()` 读错误位（BOF/EPV/EWG/STF/FOR/ACK/BR/BD/CRC），
  从 `ESR` 提取 TEC（`>>16 & 0xFF`）/ REC（`>>24 & 0xFF`，REC 硬件 8 位，早期误用 `0x7F` 已修正）打日志，
  `HAL_CAN_ResetError()` 清软件标志。为使能丢帧统计，F4 中断增加了 `CAN_IT_RX_FIFO0/1_FULL` 与
  `CAN_IT_RX_FIFO0/1_OVERRUN`（FULL 进 `HAL_CAN_RxFifo0/1FullCallback` 计数；overrun 在错误回调经 `RX_FOV0/1` 计数）。

### 状态变量（调试用，调试器直接 Watch）

每个外设各有一个独立的 `volatile` 全局结构体数组，字段按该外设可获取的信息源设计（**两者刻意不一致**，
纯给调试看，不参与对外接口）。计数只增不清，需要清零可在调试器里直接写 0。与 `bsp_sys_status` 的系统状态计数互补
（sys_status 管初始化/调用错误与任务超时，这里管总线运行状态）。

**H7 (FDCAN)** — `s_fdcan_status[can_e]`（类型 `CAN_FdcanStatus_s`，定义于 `bsp_fdcan.c`）：

| 字段                                                      | 含义                                                            |
| --------------------------------------------------------- | --------------------------------------------------------------- |
| `tx_ok` / `tx_fail`                                       | 发送完成次数（TxEventFifo 弹事件）/ CANTransmit 返回非 BSP_OK 次数 |
| `tx_busy` / `tx_timeout` / `tx_isr_clamp`                 | 失败中：资源满不等待 / 等 FIFO 超时 / 中断里被钳成不等待          |
| `tx_abort` / `tx_abort_late`                              | 被 CANRecover 取消的在途帧数 / 取消迟到仍发出去的事件数（见 §6.4） |
| `rx_ok` / `rx_full` / `rx_lost`                           | 收帧成功 / RxFIFO0 满事件 / RxFIFO0 丢报文（MESSAGE_LOST）      |
| `tx_event_lost` / `marker_reclaim`                        | TxEventFifo 满/丢失次数 / 因此回收的 marker 槽数（见 §6.3）      |
| `tx_result_stale`                                         | 代际不符被丢弃的迟到 Tx Event 数（槽位已回收复用，结果不能归给新占用者，见 §6.3） |
| `err_bus_off` / `err_passive` / `err_warning`             | 对应错误状态进入次数                                            |
| `err_event`                                               | 硬件累计错误事件数（ECR.CEL 差值累加，饱和 255 取增量不受影响） |
| `err_ram_access`                                          | Message RAM 访问失败次数（IR.IRA，配置级严重故障）              |
| `recover_ok` / `recover_fail`                             | CANRecover 后总线可用 / 仍不可用次数                            |
| `bus_off` / `error_passive` / `error_warning` / `lec`     | 最近一次采样的 PSR 状态位 / 上次错误码                          |
| `tec` / `rec`                                             | 错误计数器（ECR.TEC / REC）                                     |
| `tx_free` / `tx_owner_busy` / `rx_fifo0_fill`             | 发送后空闲元素数 / 未收口帧数 / 收帧入口 FIFO 填充数（突发深度） |
| `last_err_us`                                             | 最近一次错误回调的 DWT 时间戳（判断错误是否已停止）             |

**F4 (BxCAN)** — `s_bxcan_status[can_e]`（类型 `CAN_BxcanStatus_s`，定义于 `bsp_bxcan.c`）：

| 字段                                                      | 含义                                                         |
| --------------------------------------------------------- | ------------------------------------------------------------ |
| `tx_ok` / `tx_fail`                                       | 发送完成次数（邮箱 complete 回调）/ CANTransmit 返回非 BSP_OK 次数 |
| `tx_busy` / `tx_timeout` / `tx_isr_clamp`                 | 失败中：资源满不等待 / 等邮箱超时 / 中断里被钳成不等待        |
| `tx_abort` / `tx_drain`                                   | 被取消的在途帧数 / 任务上下文同步分发的完成事件数（见 §6.3）   |
| `rx_ok` / `rx_full` / `rx_lost`                           | 收帧成功 / RxFIFO0/1 满事件 / overrun 丢帧（FOV0/FOV1）      |
| `err_bus_off` / `err_passive` / `err_warning`             | 对应错误状态进入次数                                         |
| `err_protocol`                                            | 协议错误次数（LEC: STF/FOR/ACK/BR/BD/CRC）                   |
| `err_tx`                                                  | 发送失败帧数（仲裁丢失/发送错误 ALST/TERR）                  |
| `recover_ok` / `recover_fail`                             | CANRecover 后总线可用 / 仍不可用次数                         |
| `bus_off` / `error_passive` / `error_warning` / `lec`     | 最近一次采样的 ESR 状态位 / 上次错误码                       |
| `tec` / `rec`                                             | 错误计数器（ESR.TEC / REC）                                  |
| `tx_free` / `tx_owner_busy` / `rx_fifo0_fill` / `rx_fifo1_fill` | 空闲邮箱数 / 未收口帧数 / 两个 RX FIFO 填充数          |
| `last_err_us`                                             | 最近一次错误回调的 DWT 时间戳（判断错误是否已停止）          |

- 后续扩展（**尚未做，不再是"指向某份设计文档"而是本文件内的待办**）：参考 xrobot 的
  虚拟错误帧机制，按优先级（BUS_OFF > PASSIVE > WARNING > LEC）把错误当作特殊帧分发给订阅者
  （`CANErrorID_e`）。本轮**刻意没做**：那要给 `CAN_Pack_s` 加 `type` 字段、改所有 filter 回调的语义，
  而本仓库已把 `err_callback` 定为唯一错误上报通道，两套并存只会分裂（详见 §6.6）。

## 5. CAN 接收过滤：标准 ID LIST 模式查表加速

接收分发默认是双重循环线性扫描（实例 × filter），最坏 O(实例数×过滤器数)。当同一 CAN 上大量
LIST 模式（精确 ID）标准帧 filter 时可用查表加速：按标准 ID（0~0x7FF）直接索引命中实例，O(该实例 filter 数)。

- **开关**：`app_cfg.h` 定义 `BSP_CAN_LIST_LUT_USED`（关闭则注释掉该行，走纯循环判断，语义不变）。
- **数据结构**：`CANInstance *s_*_list_lut[CAN_NUM_MAX][0x800]`（每 CAN 2048 个槽，槽存"该标准 ID 的
  LIST filter 所属实例指针"）。**登记时机**：CANConfig 时直接把实例指针写进对应 ID 槽位——无构建、
  无 dirty 标志，天然支持增量注册/重配置改 ID。
- **调试辅助**：`volatile uint8_t s_*_list_lut_used[CAN_NUM_MAX]`——该 CAN 是否已有查表槽位被登记
  （Register 覆盖槽位时置 1，只增不清），调试器直接 Watch 确认查表路径是否生效。
- **内存**：每 CAN 指针数组 2048×4B ≈ 8KB。F4（2 CAN）≈16KB，H7（3 CAN）≈24KB，由该宏控制是否编译。
- **覆盖范围**：只加速「标准帧 + LIST 模式」且 ID 合法的 filter——id0 必填（≤0x7FF）、id1 可空
  （CAN_ID_UNUSED 表示仅匹配 id0）；**任一真实 ID >0x7FF 属配置非法，整条 filter 跳过不入表**（LOGWARNING 告警）。
  id0==id1 或 id1=CAN_ID_UNUSED 只落一个槽，id0≠id1 两个都落。扩展帧 LIST / MASK / RANGE 不入表，走循环兜底。
- **分发（单入口，`#if/#else` 二选一）**：收帧后一次分发调用——LUT 启用时走 `*_ListLutDispatch`
  （先查表回调 LIST 标准帧，再循环兜底 MASK/RANGE/扩展/未登记，循环内跳过已由查表处理的 LIST 标准帧）；
  未启用时走 `*_LoopDispatch`（完整循环，全模式）。两函数互斥编译、互不传参，无重复回调。
- **语义**：NULL callback 不入表；同实例多 filter 同 ID（含 STD_DATA/STD_REMOTE 区分）查表扫描该实例
  全部触发（frame_type 二次校验）；重配置改 ID 后旧 ID 残留槽无害（分发以该实例当前 filters 为权威）。
- **差异**：LIST 回调始终先于循环 filter（查表在前，回调集合与循环路径一致）；**同 (CAN, ID) 被多个
  实例注册时后注册者优先**（槽被覆盖，前注册者的该 filter 不再回调，属配置冲突需自行避免）。

## 6. 失败路径与自恢复机制（发生时机 / 功能 / 原理）

> 本节是 bsp_usart / spi / iic 那一轮的收尾：CAN 此前只有"错误计数 + 日志"，
> 发送失败**不告诉发起者**，也没有任务上下文的恢复入口。

### 6.1 为什么需要自恢复：静默停摆

CAN 的典型故障不是"报错"，而是**不报错地发不出去**：

- 对端离线 / 线被拔掉 → 帧发出去没人 ACK → 错误计数上升但不产生完成事件，邮箱被占着；
- bus-off → 外设停在外设内部状态里，软件不主动清就一直是那样；
- FDCAN 的 marker 槽被泄漏（见 6.3）→ `All 32 TX markers in flight`，该实例再也发不出帧。

这三条的共同点是**发起者拿不到任何结果**，只能靠上层"连续 N 次发送仍见 busy"这种兜底去猜。
本轮的改造就是把这些结果**明确送回去**，并给一个任务上下文的恢复入口。

### 6.2 失败返回点

| 接口                | 失败返回                                                                                         |
| ------------------- | ------------------------------------------------------------------------------------------------ |
| `CANRegister`       | `BSP_PARAM_ERR`（实例为空 / 重复注册 / 超实例数）                                                 |
| `CANConfig`         | `BSP_PARAM_ERR`（参数/`can_e` 越界/模式非法、BxCAN 非 CLASSIC、FDCAN FrameFormat 不匹配）/ `BSP_HW_ERR`（板级映射缺失或 HAL 初始化失败） |
| `CANTransmit`       | `BSP_PARAM_ERR`（空指针/长度越界/帧类型非法/帧类型与工作模式不兼容）<br>`BSP_BUSY`（邮箱/FIFO 满且不等待或中断里不等待）<br>`BSP_TIMEOUT`（等资源耗尽 `timeout_ms`，已顺手取消占用的邮箱/FIFO 并逐帧通知发起者）<br>`BSP_HW_ERR`（`HAL_*_AddTxMessage` 失败，或实例尚未成功 `CANConfig`——handle 为 NULL） |
| `CANRecover`        | `BSP_PARAM_ERR`（实例/句柄为空）/ `BSP_BUSY`（**没动作**：外设没启动，或入口自证判据不成立——见 §6.4.1）/ `BSP_OK`（刚做过恢复动作且总线可用）/ `BSP_HW_ERR`（做了动作但恢复后仍不可用，已计 `recover_fail`） |

**发送资源类失败（`BSP_BUSY`/`BSP_TIMEOUT`）不走 `err_callback`** —— 它们由返回值和逐帧结果表达，
与 usart 一致（`err_callback` 只报硬件/总线级事件）。

### 6.3 逐帧结果上报（本轮最关键的行为变化）

`tx_complete_callback` 增加结果参数：

```c
void (*tx_complete_callback)(CANInstance *instance, uint32_t tx_mailbox, BSP_Status_e result);
```

| 情形                                        | 谁负责回报                                                   | `result`    |
| ------------------------------------------- | ------------------------------------------------------------ | ----------- |
| 帧真的发到总线（RQCP 置位 + TXOK）          | F4：`HAL_CAN_TxMailboxNCompleteCallback`；H7：TxEvent 弹事件 | `BSP_OK`    |
| 仲裁丢失 ALST / 发送错误 TERR               | F4：`HAL_CAN_ErrorCallback` 的 ALST/TERR 位（错误码位自带邮箱号） | `BSP_HW_ERR` |
| 被 `CANAbortAllTx` / `CANRecover` 取消      | 显式清槽后回调（不押在 HAL 那条路径上，见下）                | `BSP_HW_ERR` |
| Tx Event FIFO 满/丢失导致溯源丢失（H7 专有）| `HAL_FDCAN_TxEventFifoCallback` 的 FULL/LOST 分支回收并通知  | `BSP_HW_ERR` |

四条都会**先清溯源槽再回调**（回调里可能立刻再发并复用同一槽），因此同一邮箱被多条路径同时
观察到时最多只有一条能拿到非空槽，不会重复通知。

> **H7 的 marker 池泄漏（本轮修掉的一处真问题）**：`s_fdcan_tx_owner[can][32]` 旧版只在
> *弹出 Tx Event* 时释放。Tx Event FIFO 满/元素丢失时事件丢失、槽**永久占用**，攒够 32 次该实例
> 就再也发不出帧（`All 32 TX markers in flight`），而旧版只 `tx_event_lost++` 就往下走。
> 现在 FULL/LOST 分支与 `CANRecover` 都走同一条 `FDCAN_ReclaimMarkers`：逐个通知 `BSP_HW_ERR`、
> 清槽、计 `marker_reclaim`。

> **H7 的 marker 槽复用 → 迟到事件错认（同一处修复的另一半）**：槽位回收后会被新帧复用，
> 而**被回收那一轮**的帧仍可能写出 Tx Event（取消请求迟到 = 帧其实发出去了，见 `tx_abort_late`）。
> 那条事件排队到下一轮才弹出时，只凭 MessageMarker 无法区分"这是我这次发的帧"还是"上一轮的旧帧"，
> 会把旧帧的结果记到新发起者头上，并提前清掉它正在等结果的槽。
> 故 MessageMarker 编码为 **`代际 3 位 << 5 | 槽位 5 位`**（硬件是 8-bit，正好用满）：
> 回收一个槽时该槽的代际 +1，`FDCAN_TxResultHandler` 弹出事件时若代际与当前不符就丢弃并计
> `tx_result_stale`。3 位代际意味着一帧从入队到事件弹出之间要经历 8 次回收（≈256 个丢失事件）
> 才会撞车，而 8-bit 已是硬件上限。
> 对外（`CANTransmit` 的 `tx_mailbox` 出参与 `tx_complete_callback` 回传值）它是**不透明标记**，
> 只应原样比对，不要解释数值——上层按 `tx_mailbox` 区分帧来源的写法不受影响。

### 6.4 `CANRecover` 的入口自证与动作顺序（任务上下文）

#### 6.4.1 先判、后动：判据必须来自整条总线

`CANRecover` 作用于**整条总线**（取消该总线上**所有实例**的在途帧、必要时重停外设），
而调用方只看得到**自己这一条链路**。**"我这条链路没收到帧"不是总线级证据**：对端不发、
滤波器不匹配、流量被同总线别的实例挤掉都会造成它，而这三样本函数一个都治不好，代价却是
把别人正在发的帧全丢掉。所以判据必须在 bsp 入口内部自证 —— **调用点只负责"何时看一眼"
（限频），"值不值得动手"的判据在这层**（只有 bsp 看得见总线状态）。这与 `bsp_spi` 的
`SPIRecoverTxIfStuck` 是同一条分工原则，只是 SPI 恰好一口一实例、两者作用域天然重合。

判据**在任何动作之前**评，一条都不成立就 `return BSP_BUSY`（= "什么都没做"）且**不碰任何
在途帧** —— 健康链路上调用它是零代价的（多两次寄存器读）；调用方忽略 `BSP_BUSY`、不打日志。

| # | 判据 | 为什么它"对症" |
|---|------|----------------|
| ① | 停在 **bus-off**：F4 `ESR.BOFF` / H7 `ProtocolStatus.BusOff` | ISR 已清过 `CCCR.INIT`，此处仍为 1 = 硬件等不到"128×11 位隐性电平"，恢复序列自己走不出去。H7 的 Stop/Start 重启是唯一出路（总线没接回时重启也无效，走 `recover_fail` 返回 `BSP_HW_ERR`） |
| ② | **发送资源占满 且发送错误计数器越界**（F4 `ESR.TEC ≥ 128` 且三邮箱全占 / H7 `TxErrorCnt ≥ 128` 且 `TxFifoFreeLevel == 0`） | TEC 每失败一次发送 +8、成功一次 −1，≥128 即 error passive 区 → 说明帧确实**一直发不出去**（无 ACK 时硬件一直重传，既不完成也不报错），此时取消在途帧才能把资源腾出来 |
| ③ | 其余一切 | `BSP_BUSY`，不动手 |

两条判据的若干取舍（改判据前必须知道）：

- **为什么②必须再搭 TEC，不能只看"资源占满"**：单纯 Tx FIFO / 邮箱满在健康总线上是**正常
  瞬时状态**（一帧正在发），拿它当判据会在正常忙时误取消一批好帧。TEC 是"发送侧真的推不动"
  的直接观测量。
- **为什么用 TEC 而不是 `ProtocolStatus.ErrorPassive`**：后者是"TEC ≥128 **或** REC ≥128"，
  一个只是收帧受干扰（REC 高）而发送完全正常的总线也会被算进去 → 误伤。故 H7 走
  `HAL_FDCAN_GetErrorCounters().TxErrorCnt`（封装成 `FDCAN_TxErrorCount`）。
- **为什么不把"marker 池占满（`OwnerBusyCount == 32`）"当判据**：合法突发同样能占满 32 个
  （`TxEventsNbr` / `TxFifoQueueElmtsNbr` 都是 32），而真泄漏（Tx Event FIFO 满/丢）**已经在
  `FDCAN_TxEventFifoCallback` 里就地回收**（`FDCAN_ReclaimMarkers(…, 1)`，见 §6.2）——
  拿到恢复入口再判一次只会误伤一批正常在途帧。bxCAN 只有 3 个邮箱，没有这个问题。
- **为什么 `CANTransmit` 的超时分支可以例外地"自行取消"**：那里判据是"本次发送确认等不到
  FIFO 空间"这一**确定性事实**（`BSP_TIMEOUT` 已发生），动作虽是总线级但由"发送资源已死"
  直接触发，不是靠上层猜的；且它已有独立实现（`bsp_fdcan.c` 的 Tx FIFO 超时分支）。
- **`instance` 参数是"哪条总线"的选择符，不是"作用对象"**：任何挂在该总线上的实例调它，
  效果完全一样。调用者容易误以为"我传的是我的实例，所以只影响我"——这正是判据必须内置的原因。

#### 6.4.2 判据成立后做的五件事（顺序固定，不能换）

```
① 取消全部在途发送
     F4: HAL_CAN_AbortTxRequest(hcan, MAILBOX0|1|2)   只置 TSR.ABRQx，无 tick 自旋
     H7: HAL_FDCAN_AbortTxRequest(hfdcan, 0xFFFFFFFF) 逐元素取消（TXBCR）
② 显式清溯源槽 + 逐帧通知 tx_complete_callback(..., BSP_HW_ERR)
     F4: 被成功取消的帧在 HAL 里走 TxMailbox*AbortCallback，但那条路径受 TX 邮箱中断使能门控、
         且只覆盖"RQCP 置位而 TXOK/ALST/TERR 均未置"这一种情形，不能把 owner 收尾押在它上面；
         （已核对 HAL 源码：该回调在取消成功时确实会触发，所以两条路径都会到——正因如此
          两条都是"先清槽再回调"，谁先到谁通知，后到的查到空槽跳过，不会重复。）
     H7: 被成功取消的帧**不产生 Tx Event**，槽不会被回调回收，不显式清就泄漏
③ 清软件错误标志
     F4: HAL_CAN_ResetError       H7: 无对应动作（错误位是粘滞的路由标志），只回读状态
④ 回读总线状态判断是否恢复（BOFF 仍置 / 邮箱·Tx FIFO 仍满 ⇒ 没恢复）
     H7 兜底：仍在 bus-off 时 HAL_FDCAN_Stop → ConfigInterruptLines → Start → ActivateNotification
     （Stop 会清掉中断线映射与已激活的中断，少一步恢复后一个中断都收不到）
⑤ 计 recover_ok / recover_fail，刷新状态快照
```

> **④ 的"仍占满"判据要等取消落地**：ABRQ / TXBCR 是硬件**异步**执行的，写完立刻回读可能还是旧值，
> 会把一次正常的恢复误判成失败。实现按 `CAN_RECOVER_DRAIN_US`（默认 1000us，`#ifndef` 可覆盖，
> 见 `bsp_can.h`）在任务上下文里有界轮询等它腾出来。
> 采样点还必须在可能的重启动作**之后**：H7 的 `HAL_FDCAN_Stop/Start` 会清空 Tx FIFO，
> 若像旧版那样在 Stop 之前采样，重启明明把 FIFO 清空了，却仍拿"重启前 FIFO 满"的旧值报失败。

**代价要写清楚**：恢复动作会**放弃在途帧**，上层（电机驱动 / comm media）会看到它们以
`BSP_HW_ERR` 失败并按各自协议重发 —— 即"用丢若干帧换回发送能力"。
`CANRecover` **不得在 ISR 里调用**。

> **接在了哪里**：`drv/drv_comm/media/comm_media_can_pkt0.c` 与 `comm_media_can_idseq.c`
> 各自的**发送入口**里（`MediaCanPkt0ProbeBus` / `MediaCanIdseqProbeBus`）调 `CANRecover`，
> 按 `DRV_COMM_MEDIA_CAN_RECOVER_PERIOD_MS`（默认 100ms）**按外设 `can_e` 限频**。
> 于是 FDCAN 的 Stop/Start 兜底真正可达，不再是"只提供不接线"。
>
> 为什么搭在发送入口：它是**任务上下文**（`CANRecover` 的上下文要求见下）、**每帧必经**
> （不依赖任何"上层恰好会调"的约定），且"发不出去"正是本函数要治的病——判据与动作对症。
> 调用点必须放在 `tx_active` 判断**之前**：总线上不来时逐包续发的完成回调也不会来，
> `tx_active` 会永久为 1，放它后面就永远够不到。
>
> **为什么不搭在 daemon 的离线钩子上（此前的一版实现）**：offline 钩子的触发条件是"这条
> 链路没收到帧"= **实例级**证据，而本函数是**总线级**动作，两者作用域不匹配（见 §6.4.1）。
> 改为发送入口后还有一个连带好处：`CommConfig` 对 `daemon_reload = 0` 的提升只对挂了
> `vtable->offline` 的后端生效，于是 **CAN 链路的 `daemon_reload = 0` 恢复成真正的"不监控"**
> ——此前它会被静默改写成 100ms，等于把 app 的"我知道这条链路会安静"变成"每秒 10 次去抢整条总线"。
>
> 限频按**外设**去重而不是按实例：`CANRecover` 作用于整个外设（取消的是该总线上所有实例的在途帧），
> 同一条总线上挂多个 media 时一个共享节拍才对（gimbal 的 `CAN_1` 就是底盘链路 + 电机共用）。
> 返回值只用于日志分级：`BSP_OK` 本层不打（bsp 已按 INFO 记账）、`BSP_HW_ERR` 补一条带链路
> 上下文的 ERROR（便于与"对端真掉线"区分）、`BSP_BUSY` 是**绝大多数时候**的返回值
> （判据不成立 = 没动作），不打日志、不刷屏。
>
> 在此之前，bus-off 的第一道自愈路径仍是硬件 AutoBusOff + ISR 里清 `CCCR.INIT`；
> 本入口只在总线级判据成立时再兜一层。若 app 想自己按别的判据（如 `recover_fail` 连续增长）
> 调它也可以，但**限频要自负**：判据成立时每次调用都会取消该总线上的在途帧，调太勤等于持续丢帧。

### 6.5 上下文限制：中断里不等待

`CANTransmit` 入口用 `__get_IPSR() == 0 && __get_PRIMASK() == 0 && __get_BASEPRI() == 0`
判上下文（与 `USART_CanBlockingAbort` 同式）。中断上下文里把 `timeout_ms` 视作 0：只试一次，
满即 `BSP_BUSY`，并计 `tx_isr_clamp`。

- 事实依据：等待循环用 `DWT_GetTimeUs()`（不是 tick 自旋），所以**不会死锁**；
  但 comm media 的续发钩子在 CAN 中断里以 `timeout_ms=1` 调它，总线拥塞时就是每次 TxComplete
  在 ISR 里空转 1ms —— 钳掉它是在"不改变失败语义"的前提下拿回实时性。
- 续发失败本来就有既定处置：media 记 `tx_fail`、接收端按序号跳号丢帧重同步。

### 6.6 `err_callback` 契约（总结）

```c
typedef enum : uint8_t
{
    CAN_ERR_BUS_OFF = 0,       //!< 进入 bus-off（ISR）
    CAN_ERR_ERROR_PASSIVE = 1, //!< 进入 error passive（ISR）
    CAN_ERR_ERROR_WARNING = 2, //!< 进入 error warning（ISR）
    CAN_ERR_PROTOCOL = 3,      //!< 协议错误 / 发送失败 ALST·TERR（ISR，细分见 s_*_status[].lec）
    CAN_ERR_RX_OVERFLOW = 4,   //!< RxFIFO 满/溢出丢帧（ISR）
    CAN_ERR_HW = 5,            //!< 外设级硬件错误：Message RAM 访问失败 / Tx Event FIFO 丢失（H7 专有）
} CAN_ErrReason_e;
```

- 在**错误分类计数、状态快照、纠正动作之后**调用；handler 必须无阻塞、可重入、**幂等**，
  且不得在其中调用 `CANTransmit` / `CANConfig`。
- **错误是外设级的、广播给同一条总线上的所有实例**（同一 handle 可挂多个实例），
  因此 handler 里不能有任何"只对自己成立"的假设。**特别是：不要在 `err_callback` 里清
  自己的异步发送状态** —— 同一个句柄上 A 实例的错误会清掉 B 实例正在途的发送，
  而 B 那帧随后可能正常完成，于是拿着被重置的进度继续，造成重复发送。
  逐帧的 `result != BSP_OK` 已经覆盖了"这一帧没发出去"，这才是上层该挂的状态复位点。

## 7. 验证（本机只能用"全 app 编译 + 静态核对"）

本机没有 CAN 硬件（也无报文回环），所以：

1. **编译**：`cmake --preset default`，然后
   `cmake --build --preset {Debug,Release} --target {half_rudder_gimbal,half_rudder_chassis,example}`
   = 6 配置全绿、零警告零错误（gimbal=DJI_C/`BXCAN`，chassis=DM_MC02_HALF_RUDDER/`FDCAN`，
   example 覆盖第三块板）。重点看 `-Wall` 下：`BSPLOG` 关闭时的未使用变量、
   `CAN_ErrCallback` 未用参数、`BSP_Status_e` 窄化赋值。
2. **残留检查**（只读 grep）：
   - `int8_t CANRegister|CANConfig|CANTransmit|CANRecover` → 零匹配；
   - `CANTransmit(` 在 `drv/` 里的裸调用都是**合法**的：`comm_media_can_pkt0.c` /
     `comm_media_can_idseq.c` 各一处（media 后端不是电机，没有电机实例可挂 `tx_fail`），
     `drvs_motor/*` 各驱动自己的发送点（各自判 `!= BSP_OK` 后累加本实例/本组的 `tx_fail`）。
     它们都不经过 `MotorCanTransmit` —— 那个 helper 只服务**待删除**的旧 `drv_motor/`，
     `drvs_motor` 已全部改用 `CANTransmit` 直呼（见 `drvs_*.c` 的发送点注释）。
     等 `drv_motor/` 连同 `drv_motor_can.h` 一起删掉后，`drv/` 里就只剩这 9 个直呼点；
   - `CAN_TX_MAILBOX_FREE_BASE`（定义后从未使用的死物）→ 零匹配。
3. **符号级要查 `.o`，不要查 `.elf`**：本工程开了 `-ffunction-sections` + `-Wl,--gc-sections`
   （`CMakeLists.txt:226,243`），**零调用者的函数会被链接器丢掉**。所以零调用者的函数
   （如 USB 的 `USBReenumerate`）在 `.elf` 的 `nm -g` 里查不到，这是预期现象，不代表没编译。
   正确查法是查目标文件：
   `arm-none-eabi-nm -S build/CMakeFiles/<app>.dir/Debug/bsp/bsp_can/bsp_bxcan.c.obj`
   → 应有 `T CANRecover` 与 `B s_bxcan_status`（FDCAN 版同理，见 chassis 的 `bsp_fdcan.c.obj`）。
   `CANRecover` 现在有真实调用点（CAN media 的发送入口探测），故 `DRV_COMM_USED` 打开的 app
   在 `.elf` 里也应能查到它——查不到就说明 comm 没编进来或钩子被 `gc-sections` 割掉了，
   属于需要排查的现象。
4. **待现场验证（无硬件）**：拔线进 bus-off → 接回看 `recover_ok` / `err_bus_off` 增长与发送自愈；
   邮箱占满时 ISR clamp 的实际效果；FDCAN marker 回收后能继续发帧。
