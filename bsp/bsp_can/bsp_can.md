# bsp_can 开发总结

> 本文件记录 bsp_can（`bsp_can.h` / `bsp_bxcan.c` / `bsp_fdcan.c`）接口用法、
> 踩过的坑与处理方式、CubeMX 默认配置及原因。git 提交号为历史溯源参考。

## 1. bsp_can 接口使用说明

### 1.1 三个入口

| 接口                                                                 | 作用                                    | 可重复调用                   |
| -------------------------------------------------------------------- | --------------------------------------- | ---------------------------- |
| `CAN_INSTANCE_DEF(name)`                                             | 静态定义实例（仅身份绑定）              | -                            |
| `CANRegister(instance)`                                              | 参数校验 + 防重入 static 管理数组       | 否（重复注册返回 -1）        |
| `CANConfig(instance, config)`                                        | 填硬件映射/模式/过滤器 + 首次初始化外设 | 是（已初始化则跳过硬件步骤） |
| `CANTransmit(instance, pack, timeout_ms, tx_mailbox, tx_free_level)` | 发送一帧                                | 是                           |

### 1.2 发送接口签名（注意！）

```c
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack,
                   uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level);
```

- `timeout_ms`：BxCAN 邮箱 / FDCAN Tx FIFO 满时等待其空闲的上限；传 0 表示不等待、资源满立即返回失败。
  等待为**阻塞轮询**，只能在任务/DRV 上下文调用，不能在中断里用。
- `tx_mailbox` 出参：BxCAN=邮箱索引 0~2 / FDCAN=MessageMarker 0~31，发送完成回调据此对应发送帧。
- `tx_free_level` 出参：发送后剩余可发送数。
- 相对旧版的变更：**由 `(instance, pack, uint32_t*, uint8_t*)` 变为 `(instance, pack, timeout_ms, uint32_t*, uint8_t*)`**。
  上层（drv_motor / drv_comm media）若仍调旧 2 参 `CANTransmit(can, timeout)`，需适配到新 5 参签名。

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
- **async 发送回调**：`tx_complete_callback(instance, tx_mailbox)` 帧发完触发，支持 comm 异步分包续发。
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

## 4. can 的状态：错误码，错误计数等使用方式

当前实现只做「错误上报 + 总线恢复」，尚未上虚拟错误帧/错误码分发（错误码之后再说）：

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
纯给调试看，不参与对外接口）。计数只增不清，需要清零可在调试器里直接写 0。与 `bsp_assert` 的系统断言计数互补
（assert 管初始化/调用错误，这里管总线运行状态）。

**H7 (FDCAN)** — `s_fdcan_status[can_e]`（类型 `CAN_FdcanStatus_s`，定义于 `bsp_fdcan.c`）：

| 字段                                                  | 含义                                                            |
| ----------------------------------------------------- | --------------------------------------------------------------- |
| `tx_ok` / `tx_fail`                                   | 发送完成次数（TxEventFifo 弹事件）/ CANTransmit 返回 -1 次数    |
| `rx_ok` / `rx_full` / `rx_lost`                       | 收帧成功 / RxFIFO0 满事件 / RxFIFO0 丢报文（MESSAGE_LOST）      |
| `tx_event_lost`                                       | TxEventFifo 满/丢失次数（溯源表可能丢回调）                     |
| `err_bus_off` / `err_passive` / `err_warning`         | 对应错误状态进入次数                                            |
| `err_event`                                           | 硬件累计错误事件数（ECR.CEL 差值累加，饱和 255 取增量不受影响） |
| `err_ram_access`                                      | Message RAM 访问失败次数（IR.IRA，配置级严重故障）              |
| `bus_off` / `error_passive` / `error_warning` / `lec` | 最近一次采样的 PSR 状态位 / 上次错误码                          |
| `tec` / `rec`                                         | 错误计数器（ECR.TEC / REC）                                     |
| `tx_free` / `rx_fifo0_fill`                           | 发送后空闲元素数 / 收帧入口 FIFO 填充数（突发深度）             |

**F4 (BxCAN)** — `s_bxcan_status[can_e]`（类型 `CAN_BxcanStatus_s`，定义于 `bsp_bxcan.c`）：

| 字段                                                  | 含义                                                         |
| ----------------------------------------------------- | ------------------------------------------------------------ |
| `tx_ok` / `tx_fail`                                   | 发送完成次数（邮箱 complete 回调）/ CANTransmit 返回 -1 次数 |
| `rx_ok` / `rx_full` / `rx_lost`                       | 收帧成功 / RxFIFO0/1 满事件 / overrun 丢帧（FOV0/FOV1）      |
| `err_bus_off` / `err_passive` / `err_warning`         | 对应错误状态进入次数                                         |
| `err_protocol`                                        | 协议错误次数（LEC: STF/FOR/ACK/BR/BD/CRC）                   |
| `err_tx`                                              | 发送错误次数（仲裁丢失/发送错误 ALST/TERR）                  |
| `bus_off` / `error_passive` / `error_warning` / `lec` | 最近一次采样的 ESR 状态位 / 上次错误码                       |
| `tec` / `rec`                                         | 错误计数器（ESR.TEC / REC）                                  |
| `tx_free` / `rx_fifo0_fill` / `rx_fifo1_fill`         | 空闲邮箱数 / 两个 RX FIFO 填充数                             |

- 后续扩展：参考 `ignore/完善bsp,hal_can/bsp_can/` 中按优先级（BUS_OFF > PASSIVE > WARNING > LEC）分发虚拟错误帧
  （`CANErrorID_e`）给订阅者的机制。
