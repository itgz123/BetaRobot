# JSF AV C++ 偏离登记（MyCMakeLists.md）

本文件登记**本工程的编译配置与 JSF AV C++ 标准不一致的地方**，以及每条的理由和实测依据。
改根目录 `CMakeLists.txt` 的 JSF 段落时，请同步本文件。

- **标准版本**：*Joint Strike Fighter Air Vehicle C++ Coding Standards*，2RDU00001 Rev C（2005-12），约 220 条。
- **落点**：`CMakeLists.txt` 的「JSF AV C++ 严格编译配置」段 + `cmake/no-ssp-libs.specs` + `cmake/check_no_heap.cmake` + `bsp/bsp_stack_guard/`。
- **量化依据**：`scripts/jsf-av/audit_jsf_warnings.py`（用 `compile_commands.json` 重放编译、统计告警），本文所有数字都由它实测。

> **偏离类型**：`放宽` = 比标准松；`收紧` = 比标准严；`不适用` = 标准有该条但本工程无对应面；`勘误` = 标准条号/内容需要更正。

---

## D1 · 语言标准版本 —— `放宽`

| | |
|---|---|
| 标准 | **AV Rule 8**：*"All code shall conform to ISO/IEC 14882:2002(E) standard C++."*（即 C++03） |
| 本工程 | C 用 **C23**（`-std=c2x`，由 `CMAKE_C_STANDARD 23` + `CMAKE_C_EXTENSIONS OFF` 给出）；C++ 用 **C++17** |
| 类型 | 放宽（比 C++03 新很多） |

**理由**

1. 本仓 **292 个编译单元里只有 1 个是 C++**（`lib/lib_tflm/lib_tflm_port.cc`）。JSF 是 C++ 专用标准，把语言钉在 C++03 对 99% 的代码没有实际约束力。
2. C++03 编不过 TFLM —— `lib_tflm` 会 include TFLM 的 C++17 头。草案里写的 `-std=c++14` 同样不行，这就是改成 17 的原因。
3. 选 **C23 而不是 C11** 是为了能打开 `-Wpedantic`：项目既定写法 `typedef enum : uintN_t`（见 `bsp_common.h` 等）与 `0b` 二进制字面量、空初始化 `{}` 在 C11 里都是 GNU 扩展，`-Wpedantic` 会对**每一个**报
   `ISO C does not support specifying 'enum' underlying types before C23`。
   实测：这一条就占严格告警总数的 **55%（1058/1940）**；切到 C23 后 **1058 → 6**，且**零编译错误**、不需要改一行代码。

**影响**：语言特性集合比 JSF 基准新（C23 / C++17 的特性都可用了）。

---

## D2 · 规则只作用于一方代码，第三方整体豁免 —— `放宽`

| | |
|---|---|
| 标准 | JSF 通篇假设**所有**代码都受同样约束，没有"第三方豁免"条款 |
| 本工程 | 严格告警与 `-Werror` **只加在** app / lib / bsp / drv / drvlib + hal 模块；`cubemx/` 下的 **HAL / CMSIS / FreeRTOS / STM32 USB Device Library** 与 **TFLM / CMSIS-NN / flatbuffers** 不加任何额外告警 |
| 类型 | 放宽（等价于 MISRA / DO-178 里的 third-party deviation） |

**理由**

1. 这些代码由 CubeMX 生成或来自上游，**改了就无法再重新生成 / 无法跟上游升级**——JSF 本身也把"不修改第三方"当默认前提。
2. 实测这批文件在 `-Wextra -Wconversion` 下会产生 **43 条就地修不掉的告警**（`-Wunused-parameter` 21 / `-Wconversion` 12 / `-Wsign-conversion` 10，集中在 `cmsis_os.c`、`stm32f4xx_hal_flash_ex.c`、`usbd_cdc_if.c`、`freertos.c`）。加 `-Werror` 会被它们直接挡死构建。
   （`cubemx` 命令本身不带严格开关，复测要把它们显式加上：
   `python3 scripts/jsf-av/audit_jsf_warnings.py --scope cubemx --flags="-Wno-error -Wextra -Wconversion"`）

**实现**：本工程第三方与一方代码在**同一个 target** 里，靠 `set_source_files_properties(... COMPILE_OPTIONS ...)` 做源文件级施加（见 `CMakeLists.txt` 的 `jsf_av_first_party_sources`）。

**实现补丁（第三方头文件豁免，2026-10-12）**：只豁免第三方的**编译单元**不够——第三方的**头文件**会被一方 `.c` 拉进来，照样吃 `-Wconversion` / `-Wsign-conversion`，而且改不了（CubeMX 重新生成会覆盖）。所以在已有 `target_include_directories(${app_name} PRIVATE ... ${HAL_INCLUDE_PATHS} ...)` 之外，再加一条 `target_include_directories(${app_name} SYSTEM PRIVATE ${HAL_INCLUDE_PATHS})`，让这批 include 目录以 `-isystem`（系统头）身份进编译，GCC 不从中报诊断。

- 触发原因：`half_rudder_chassis`(DM_MC02 / H7) 的 `core_cm7.h` 在严格开关下产出 **1600+ 条** `-Wsign-conversion`；`example` 亦然。`half_rudder_gimbal`(DJI_C / F4) 的 `core_cm4.h` 恰好干净，所以此前没暴露。
- 实测：加 `-isystem` 后这部分**归零**，且 `half_rudder_gimbal` 仍 0 error / 0 warning（没有把一方代码误判成系统头）。

**例外**：`-fstack-protector-strong` 与 `-fstack-usage` **对全 target 生效（含第三方）**——它们只增加检查/测量代码，不引入"必须改源码才能过"的严格性，而 HAL 的 USB 栈缓冲恰恰值得护。

---

## D3 · AV Rule 119（禁递归）：直接调用链已自动检查，函数指针是盲区 —— `放宽`

| | |
|---|---|
| 标准 | **AV Rule 119**：*"Functions shall not call themselves, either directly or indirectly (i.e. recursion shall not be allowed)."* |
| 本工程 | `scripts/jsf-av/check_recursion.py`：用 `-fcallgraph-info=su,da` 生成调用图 `.ci`，合并全程序图后求强连通分量，有环即判递归。**直接调用链全覆盖、当前 3 个 app 全部 0 环**；**经函数指针的间接调用查不到目标**（见下），另给 `-fstack-usage` 的 `.su` 供人工核深调用链 |
| 类型 | 放宽（间接调用是盲区，未做到全自动） |

**实现**：脚本从 `build/compile_commands.json` 重放编译（与 `audit_jsf_warnings.py` 同一套路），给每条命令追加 `-w -Wno-error -fcallgraph-info=su,da`，把 `.ci` 收进临时目录解析。**不改 CMake、不进构建**，所以无需新增开关。

**四点实测约束**（都验证过，改脚本前先看）：

1. **只能在 Debug(-Og) 上分析**。`-O1` 就能把 `ping⇄pong` 的双向边整个吃掉（内联 + 尾调用优化），`-O2/-O3` 更甚。实测 `-Og` 边集完整。脚本只重放 Debug 那条命令，且发现优化级别高于 `-Og` 会直接报错退出。
2. **函数指针是盲区**。GCC 只给一个占位节点 `__indirect_call`（"Indirect Call Placeholder"），不展开可能的被调函数。本仓大量用函数表派发（`inst->vtable->xxx(inst)`、`Task_Init_Config_s.func`、daemon 的 offline 钩子），**经函数表形成的环看不见**。脚本会把每个间接调用点逐条列出（本轮：gimbal 47 处 / chassis 55 处 / example 13 处），那是需要人工核的部分。
3. **同名静态函数不会误判**。GCC 对静态函数自动加文件名限定（`bsp_log.c:BSPLogV`，CMake 传绝对路径时是全路径前缀），所以跨 TU 同名不会错误合并成假环。
4. **保守近似：只多报不漏报**。一条边只代表"存在调用点"，不代表运行时一定执行（`-Og` 不消除死分支）。对"函数不得调用自己"这种静态性质，方向是对的。

**首轮结果（2026-10-12，3 个 app 各按自己的 `app_cfg.h`）与修复**：

| APP_NAME | 首轮环 | 位置 | 处理 |
|---|---|---|---|
| `half_rudder_gimbal` | 1 处 | `Referee2026CoreRxRewind` ⇄ `Referee2026CoreRxByte`（`drv_referee2026.c`，帧头 CRC8 不过时把残留回喂状态机的回退重扫） | 已改：回退只写"待重扫缓冲区"（`Referee2026RxReplay_t`），由 `Referee2026RxIsr` 用显式 worklist 喂完 —— 递归变循环 |
| `half_rudder_chassis` | 1 处 | `USARTTransmit` ⇄ `BSPLogV`（`bsp_usart.c` ⇄ `bsp_log.c`） | 已改：拆出无日志内核，见下 |
| `example` | 1 处 | 同上 | 同上 |
| （全部三个 app 复测） | **0 处** | — | `check_recursion.py` exit 0 |

**改法：结构拆分，不是加运行时重入闸**。重入闸（"已经在日志里就不再打"）**不会**从静态调用图上删掉那条边，AV Rule 119 查的正是图上的环，所以只能结构上断开：让日志链路的发送路径上的函数体里**一个 `BSPLOG` 都没有**。bsp 里因此多了两个"内核"入口，都只在头文件里注明"仅供 bsp_log / 内核使用"：

- `USARTTransmitCore()`：`USARTTransmit()` 去掉 4 条参数校验日志与 3 条失败日志后的本体，参数与返回值完全一致。`bsp_log.c` 的 `BSPLogV` 直发路径与 `LogUartTxCplt` 都改调它；`USARTTransmit()` 变成"参数校验（带日志）→ 转调内核 → 按返回码补日志"。**DMA 能力校验（`hdmatx == NULL`）必须留在内核里** —— HAL 的 `HAL_UART_Transmit_DMA` 不判空、直接解引用 `huart->hdmatx->XferCpltCallback`，绕过即写空指针。
- `USARTRecoverTxIfStuckCore()`：同上，供 `bsp_log.c` 每帧的自检和 `USARTTransmitCore` 的"忙就顺手判一次"分支使用；`USARTRecoverTxIfStuck()` 只在真复位后补一条 WARNING。

**代价（有意的）**：日志链路自己发不出去 / 被复位时不再打日志 —— 那正是成环的原因。证据改为计数：`s_usart_status[].tx_recover`、`err_callback`（`LogUartErrHandler` 本就刻意不打日志），与同因沉默的 `g_log_orphan_cnt` 是同一套做法。

**其余调用方一律仍走带日志的公开入口**（`drv_vofa` / `drv_terminal_lite` / `drv_referee2026` / `comm_media_usart`），语意不变；只有"忙且 timeout_ms==0 时那次兜底卡死复位"由内核代劳，故那条 WARNING 少了一句（计数仍在）。

**修复前的危害（留档）**：日志走 USART 发送、而发送函数失败时会打日志。`BSPLogV` 开头有 `s_log_uart.handle == NULL` 的早退闸，所以常规路径不会自己咬自己；但若选了一个**没配 TX DMA 的 `LOG_UART`**，`USARTTransmit` 里那条 "TX DMA not available" 的错误日志会立刻再进 `BSPLogV` → 再进 `USARTTransmit`，把"配错 UART"的后果从"一条清晰报错"变成"错误路径里栈溢出"。同理 `BSPLogV` 每帧调 `USARTRecoverTxIfStuck`、而它在真复位时要打 WARNING，也是一条 2 层自环。

**配置相关性（与 D7 同理）**：环是跟着 `app_cfg.h` 走的。gimbal 的板子是 `DJI_C`、该分支下 `LOG_UART` 被注释掉，日志整段 `#if` 掉，所以 **gimbal 没有日志环**；chassis/example 开了日志、没开裁判系统，所以反过来。

**工具自身的两个坑（都会报错而不是给错答案）**：
- `build/compile_commands.json` 是 CMake 配置期产物。改了 `user_cfg.h` 的 `APP_NAME` 却忘了重跑 `cmake --preset default`，就会拿**上一个 app 的图**当结论——脚本比对 db 里的 `-I.../app/<name>/` 与 `user_cfg.h`，不一致直接报错。
- `startup_*.s` 是汇编，`-fcallgraph-info` 不适用，单独归类为"汇编跳过"，不计入失败。

**为什么不用现成工具（2026-10-12 在本机实测，不是查文档抄的）**：结论是**没有一个现成工具能替掉这个脚本**，差别在两点：①能不能看见**跨 TU** 的环，②判的是"任意递归"（AV Rule 119 语义）还是只判"无界递归"。

| 工具 | 支持 C | "任意递归" | 跨 TU | 函数指针 | 结论 |
|---|---|---|---|---|---|
| clang-tidy `misc-no-recursion` | ✅ | ✅（连有界递归也报） | ❌ **只在本 TU 的 AST 上建图** | ❌ 官方文档明说不处理 | 最近的一个，但**看见不到本仓的两个真环**（都是跨 TU 的） |
| GCC `-Wanalyzer-infinite-recursion`（`-fanalyzer`） | ✅ | ❌ 只报**无界**递归（CWE-674） | — | ❌ | 语义不符：有界递归它放行，而 AV Rule 119 一律禁 |
| GCC `-Winfinite-recursion` | ✅ | ❌ 同上，且更弱 | ❌ | ❌ | 只报同 TU 里最直白的自调用 |
| cppcheck | ✅ | ❌ **没有递归检查** | （有 CTU，但无此检查） | — | 不适用 |
| CodeQL | ✅ | 需**自己写 QL**（`edges+(a,b)` 传递闭包），无内置"禁递归"查询 | ✅ 有全程序调用图 | 部分/尽力而为 | 要建库、要为 4 套板级配置各建一次，对私人仓还要 GHAS |
| semgrep / cflow / Frama-C / Infer | — | — | — | — | 要么是**语法**级（semgrep 写不出跨函数闭包）、要么只出文本调用图仍需自己判环、要么侧重内存安全 |

实测记录（跑在 `arm-none-eabi-gcc 15.2` 与 `clang-tidy 22.1.8` 上，样本含"直接无界/直接有界/相互无界/相互有界/经函数指针/同参自调用"六例）：

- clang-tidy 报出前 5 例里的 4 例（**含两例有界**），唯独**漏了函数指针**；把 `u1_enter ⇄ u2_enter` 拆成两个 `.c` 后**两边都不报** —— 这就是"跨 TU 不可见"的直接证据。
- GCC 两个开关只报"无界"的 3 例，有界的两例**放行**，函数指针同样漏。

本仓约 560 个节点 / 1000+ 条边里 47~55 处是经函数表的间接调用，所以**函数指针盲区是共同短板**，谁也躲不掉 —— 脚本把间接调用点列成清单交人工核，这部分工作量换任何工具都在。

**换不换的判据是"命中几处真环"**：clang-tidy 在本仓实测（87 个第一方文件，`-p build` 零额外参数跑通，3.55 s / 90 MB，本机版本不支持 `-j`）**确实抓到 referee 那一对**——因为那对函数在**同一个 `.c` 里**。但本轮三条真环的另外两条（`USARTTransmit` ⇄ `BSPLogV`、`BSPLogV` ⇄ `USARTRecoverTxIfStuck`）**跨 TU**，clang-tidy 全漏 ⇒ **命中 1/3**。它还有个易踩的细节：豁免只有 `NOLINT` 没有 attribute，且**互递归的一对要各自标一处**（只标一个，另一个照报）。

所以结论是**保留本脚本**（同算法且全程序，3/3），不引入 clang-tidy；将来若要一个"官方维护"的补充，代价是接受它只能替掉同文件内那部分。

参考版本：clang-tidy `misc-no-recursion` = LLVM **11.0.0**（2020-02，commit `49bffa5`）；GCC `-Wanalyzer-infinite-recursion` = **GCC 14**，前端 `-Winfinite-recursion` = **GCC 12**；cppcheck 的递归检查维护者明确未做（ticket **#14539**）。

**复测**：
```bash
cmake --preset default && cmake --build --preset Debug
python3 scripts/jsf-av/check_recursion.py --scope first-party   # exit 1 = 有环
python3 scripts/jsf-av/check_recursion.py --scope cubemx        # 第三方（顺手看：0 环）
```

---

## D4 · AV Rule 206（禁堆）实现得比标准更严 —— `收紧`

| | |
|---|---|
| 标准 | **AV Rule 206**：*"Allocation/deallocation from/to the free store (heap) shall not occur **after initialization**."*（初始化之前允许） |
| 本工程 | 链接后断言固件**完全不引用**堆符号（`malloc` / `free` / `calloc` / `realloc` / `_malloc_r` 系列 / `_sbrk` / `__wrap_malloc`），见 `cmake/check_no_heap.cmake` |
| 类型 | 收紧（比标准严一档） |

**理由**

- 本工程是裸机 + FreeRTOS，**没有"初始化期"用堆的需求**，全程禁用更简单也更安全。
- 实现上比 `-Wl,--wrap=malloc` 更可靠：`--wrap` 只能拦**本工程自己**写的调用，拦不住 newlib 内部（`printf` → `_malloc_r`）——那才是嵌入式里最常见的堆来源。nm 扫的是最终产物，两边都覆盖。
- 配合 `--gc-sections`，未被引用的分配函数会被回收，所以"能扫到"就等于"确实被引用"。

**实测（当前固件）**：`nm` 里 `malloc`/`free`/`calloc`/`realloc`/`_sbrk` **一个都没有**，本来就已经合规。

**旁注**：链接脚本 `cubemx/board_config/*/STM32F4xx_FLASH.ld` 仍保留 `_Min_Heap_Size = 0x200`（512 B）。它只是给 `_sbrk` 留的空间，当前无人引用因而白留；确认永不用堆后可以删掉。

---

## D5 · AV Rule 15（运行时防御性检查）：UBSan 完全不启用 —— `放宽`

| | |
|---|---|
| 标准 | **AV Rule 15** 一类"运行时防御"要求（草案把它映射到 `-fsanitize=undefined -fsanitize=bounds` + `-fstack-protector-strong`） |
| 本工程 | **只做 `-fstack-protector-strong`**；`-fsanitize=undefined` / `-fsanitize=bounds` **完全不启用** |
| 类型 | 放宽 |

**理由（实测，非推测）**

1. **工具链没有运行库**：`arm-none-eabi-gcc -print-file-name=libubsan.a` 原样吐回文件名 = 不存在；链接必报 `cannot find -lubsan`。
2. **即使自己补齐运行库也装不下**。`-lubsan` 焊死在驱动巨大的 `*link_command:` spec 段里（不像 `-lssp` 有独立的 `*link_ssp:` 段可覆盖），所以解法是"空 stub `libubsan.a` + 自写 handler"。这条路**验证过是通的**（全工程 **0 undefined reference**，本工程实际只用到 **9 个** handler），但——
   **非 trap 模式下 GCC 把 UBSan 描述符表放进 `.data..Lubsan_typeN` / `.data..Lubsan_dataN`，这些段算 RAM**：

   | 项 | 基线 | 完整 UBSan |
   |---|---|---|
   | `drv/drv_bmi088/drv_bmi088.c` 单文件 | 0 B RAM | **+10135 B RAM** |
   | 全工程链接后 | 56808 B RAM | **298696 B（128 KB RAM 的 227%）** ✗ |

   RAM 超出板载容量 167 KB，**物理上装不下**。（试过用 `INSERT AFTER .rodata` 的附加脚本把这些段挪去 FLASH，ld 仍把它们留在 `.data`；真要做只能改 `cubemx/board_config/*/STM32F4xx_*.ld`，且改完 FLASH 要占 68%、控制环里每条检查都是几个周期。）
3. **trap 模式（`-fsanitize-undefined-trap-on-error`）可行但当前不开**：它不需要任何运行库（驱动**自己就不加 `-lubsan`**，实测），RAM **零增长**，FLASH +65.6%（119820 → 198376）。代价是只有 `udf` 地址、没有"哪个文件第几行"的诊断。它的价值只是"调试档"，当前不需要。

**将来若要开**：建议 Debug/CI 档用 trap 模式，Release 关。**不要**常开——控制环跑 1 kHz，UB 检查的周期开销会咬人；而且 UB 真发生时正确的反应是"控制输出不可信 → 停"，不是打日志继续跑。

---

## D6 · `-fstack-protector-strong` 的自备运行期 —— `收紧`（相对"因为没库就不开"）

| | |
|---|---|
| 标准 | AV Rule 15 一类 |
| 本工程 | **开**。工具链没有 `libssp`，运行期由 `bsp/bsp_stack_guard/` 自备两个符号，链接期用 `cmake/no-ssp-libs.specs` 去掉驱动自动加的 `-lssp`/`-lssp_nonshared` |
| 类型 | 收紧 |

**实测代价**（Debug，对 `half_rudder_gimbal`）：

| 项 | 基线 | 开栈保护 |
|---|---|---|
| RAM | 56808 B | 56808 B（**不变**） |
| FLASH | 119820 B | 122428 B（**+2608 B，+2.2%**） |
| 受保护函数 | — | **96 个**（`bl __stack_chk_fail` 调用点计数） |

**两处有意为之的实现选择**（都在 `bsp/bsp_stack_guard.c` 里有说明）：

1. **canary 用编译期固定值、最低字节为 0**。固定值对"局部数组意外越界"（本工程真正要防的）完全够用；低位 0 让 `strcpy` 类逐字节溢出写到这里就被 NUL 截断，无法"连 canary 一起伪造"。真正的抗攻击 canary 需要每次上电随机（MCU UID / RNG），代价与收益不成比例。
2. **`__stack_chk_fail` 默认动作是"弱钩子 → `NVIC_SystemReset()`"**，不是 `for(;;)`。带着被破坏的栈继续跑比复位危险得多。app 可以覆盖 `BSP_StackGuardFailHook()` 做"停在安全态"（停电机等）——安全动作是 app 的语义，bsp 只负责发现并上报。

**局限（重要）**：栈保护**只覆盖**「局部数组/取地址局部量越界写穿栈帧」，**不覆盖「整个任务栈被打穿」**（递归、深调用链）——后者才是嵌入式里更常见的栈故障，要靠 `-fstack-usage` + FreeRTOS `uxTaskGetStackHighWaterMark()`。两者互补，不冲突。

---

## D7 · 一方代码零告警（`-Werror` 已通过） —— `符合`

| | |
|---|---|
| 目标 | AV Rule 218 的"由项目策略定"落成最严一档：一方代码零告警 |
| 现状 | `JSF_AV_WERROR` 默认 `ON`，**全部 3 个 app × Debug / Release 两档 = 0 error、0 warning**（2026-10-12） |

> **"零告警"是配置相关的**：`-Werror` 只检查**当前 `APP_NAME` 那一套**实际参与编译的一方文件（各 app 的 `app_cfg.h` 决定了启用哪些模块）。所以验收必须逐 app 切 `APP_NAME` 跑，不能凭一个 app 通过就下全仓结论。

**逐 app 验收结果**（2026-10-12，`cmake --preset default` + `cmake --build --preset <cfg> -- -k 0`）：

| APP_NAME | Debug | Release |
|---|---|---|
| `half_rudder_gimbal` | 0 error / 0 warning | 0 error / 0 warning |
| `half_rudder_chassis` | 0 error / 0 warning | 0 error / 0 warning |
| `example` | 0 error / 0 warning | 0 error / 0 warning |

**收敛过程**。首轮 `-Werror` 下失败集中在 3 个头文件，ninja 报 `cannot make progress`；清完头文件后，`half_rudder_gimbal` 配置暴露出 **37 条**一方告警（`app` 18 / `drv` 14 / `bsp` 5）。把验收面扩到另外两个 app 后又暴露 **35 条**（先是 2 条 `APP_TASK_DEF` 尾分号，再是 chassis 独有的 5 个文件共 33 条）。合计 **72 条**，逐类清理：

| 类别 | 条数 | 处理 |
|---|---|---|
| `-Wfloat-conversion` | 38 | double 字面量补 `f` 后缀；`DrvlibMotor_Data_t.position` 是 **double**（多圈累加，有意为之），在消费侧显式 `(float)`，不动结构体 |
| `-Wconversion` | 22 | 显式窄化转换，值域逐条核对（`bmi088` 的 `% n` 必 < n；`sbus` 的 `bit_off/8` ≤ 13；`bxcan` 的 `(esr>>N)&0xFF`；`bsp_tim` 的 ARR→float；`bsp_usart` 的 `HAL_UART_StateTypeDef` 值域 ≤ 0xE0 → `uint8_t`；`drv_djimotor_broadcast` 的 `(rx_id-0x201)/4` 与 `%4`） |
| `-Wpedantic` | 5 | 删掉 `APP_TASK_DEF(...)` 的尾分号——宏展开成**函数定义**，分号是多余的（3 个 app 各 1~3 处） |
| `-Wold-style-declaration` | 5 | `const static MotorVTable_s` → `static const MotorVTable_s`（`drv_dmmotor` / `drv_rsmotor` / `drv_djimotor_broadcast` / `drv_lkmotor` / `drv_lkmotor_broadcast`） |
| `-Wtype-limits` | 1 | `bsp_tim.c` 的 `s_encoder_idx >= ENCODER_INSTANCE_NUM` 在所有板子上恒真（`ENCODER_INSTANCE_NUM==0`）。改按配置 `#if` 分支，**保留**"无槽位即拒绝注册"的语义（去掉整个判据会让下面写空指针） |
| `-Wunused-parameter` | 1 | `(void)argument;` |

**注意 `-Werror` 会掩盖同文件里的后续告警**：`drv_djimotor_broadcast.c` 第 374 行 `(rx_id-0x201)/4` 修好后，第 375 行的 `%4` 才冒出来。清理时要用 `-- -k 0`（`ninja` 不早停）或直接看 `audit_jsf_warnings.py` 的全量列表，否则会来回好几轮。

**勘误（工具）**：`build/compile_commands.json` 在 Ninja Multi-Config 下每个源文件有 Debug / Release **两条**记录，`scripts/jsf-av/audit_jsf_warnings.py` 原先按**条目**计数，把所有数字翻了一倍（一度误报 74）。已在脚本里按文件去重——本文以及 D2 的数字均以去重后为准，复测时请用同一版本脚本。

**复测方式**（改了 JSF 段落或清了新告警之后）：
- 构建验收 → `cmake --build --preset Debug`（`-Werror` 默认开，能过即零告警）。
- 单独量化 → `python3 scripts/jsf-av/audit_jsf_warnings.py --scope first-party --flags=-Wno-error`
  （`--flags` 是**叠加**在 `compile_commands.json` 已有选项上的，`-Wno-error` 用来把 `-Werror` 抵消掉、把告警还原成告警）。
- 要临时放宽 → `cmake --preset default -DJSF_AV_WERROR=OFF`（只降级为"照常告警"，其余严格度不受影响）。

---

## D8 · 草案注释里的条号勘误 —— `勘误`

`sanitize`/JSF 的对应关系有几处常见误标，以本文为准：

| 常见写法 | 实际 |
|---|---|
| "禁用异常 —— AV Rule 30" | **AV Rule 208**：*"C++ exceptions shall not be used (i.e. throw, catch and try shall not be used.)"* |
| "禁用 RTTI —— AV Rule 49" | 未证实有此条。相关的是 **AV Rule 178**（限制向下转型） |
| "`-Wall -Werror` 强制代码洁净 —— AV Rule 8" | **AV Rule 8** 只讲"符合 ISO 标准"；告警等级是 **AV Rule 218**：*"Compiler warning levels will be set in compliance with project policies."* —— 即**由项目自己定**。"零告警"这条硬要求出自 Holzmann 的 **Power of 10 Rule 10**，不是 JSF 条款 |
| "运行时防御性检查 —— AV Rule 15" | 未能证实 Rule 15 原文。JSF 里能对应的是 **AV Rule 119**（禁递归）/ **AV Rule 206**（初始化后禁堆） |

---

## D9 · C++ 专属条款对本仓基本不适用 —— `不适用`

本仓 **292 个编译单元里只有 1 个 C++**（`lib/lib_tflm/lib_tflm_port.cc`），且 TFLM 本体属第三方。
因此 JSF 里绝大多数面向 C++ 的条款——类设计、继承（含 **AV Rule 88** 多重继承限制）、运算符重载、模板、虚函数与 **AV Rule 178** 的向下转型限制、异常安全、构造/析构次序（AV Rule 210-213）等——在本仓**没有适用面**。

真正落地的只有三条语言级开关：`-fno-exceptions`（AV Rule 208）、`-fno-rtti`（AV Rule 178）、`-fno-threadsafe-statics`（避免静态量初始化的隐式同步）。

---

## 附：开关速查

| CMake 选项 | 默认 | 作用 |
|---|---|---|
| `JSF_AV_WERROR` | `ON` | 一方代码告警当错误。当前 3 个 app 全绿，见 D7 |
| `JSF_AV_STACK_PROTECTOR` | `ON` | `-fstack-protector-strong` + `bsp/bsp_stack_guard/` + `cmake/no-ssp-libs.specs` |
| `JSF_AV_STACK_USAGE` | `ON` | `-fstack-usage`，为每个编译单元生成 `.su` 栈用量 |
| `JSF_AV_NO_HEAP` | `ON` | 链接后 nm 断言固件不引用堆（AV Rule 206） |

```bash
cmake --preset default                      # 配置（app 自动发现）
cmake --build --preset Debug                # 构建
cmake --preset default -DJSF_AV_WERROR=OFF  # 临时关掉 -Werror

# 逐 app 验收（必须全跑，见 D7 的"零告警是配置相关的"）
for app in half_rudder_gimbal half_rudder_chassis example; do
    sed -i "s/^#define APP_NAME .*/#define APP_NAME $app /" user_cfg.h
    cmake --preset default
    for cfg in Debug Release; do
        cmake --build --preset $cfg -- -k 0   # -k 0：不早停，一次看全所有告警
    done
done
```
