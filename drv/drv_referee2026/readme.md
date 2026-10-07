# drv_referee2026

2026 赛季裁判系统串口协议（V2.0.0 / 20260626）的驱动。全协议 35 条命令 = 常规 24（21 收 /
3 只发，其中 **0x0301 收发双向**）+ 图传 4（2 收 / 2 只发）+ 非链路 1（只发）+ 雷达 6（全收）。
**具体用哪条链路就 include 哪个头**；收发接口四条链路只写一次，在模块根。

## 目录（一个链路一个公开头，公共的只在 public）

```
drv_referee2026/
  drv_referee2026.{h,c}      ★ 暴露接口：四条链路共用的 Register / Config / Send + 收发内核
                               + REFEREE2026_INSTANCE_DEF（实例定义宏，靠链路名拼接）
  public/                      多条链路公共的协议定义
    referee2026_frame.h          帧格式常量（SOF/各段偏移/长度上限）、链路类型、ID 编号与换算
    referee2026_cmd.h            公共数据块 BuffItem_t、命令元信息 CmdInfo_t、方向 Dir_e
    referee2026_proto.{h,c}      HAL UART 参数表（各链路波特率）+ 三张 CRC 表
  link_common/referee2026_common.{h,c}   常规链路 24 条（21 收 / 3 只发，0x0301 双向；含 0x0301 的 8 种子内容）
  link_video/referee2026_video.{h,c}     图传链路 4 条（2 收 / 2 只发，本框架只在机器人侧）
  link_none/referee2026_none.{h,c}       非链路 1 条（0x0306，只发不收）
  link_radar/referee2026_radar.{h,c}     雷达无线链路 6 条（全收）
  tools/                                 PC 端自测（gcc + 同目录桩，见下）
```

四条链路**互不依赖**，每条只有一个 `.h` 一个 `.c`，`#include "referee2026_common.h"` 就够；
链路侧只提供"本链路长什么样"（数据段结构体、数据名枚举、长度宏、元信息表、过滤掩码、快照、
实例结构体类型，外加两个拼接别名），**一行收发函数、一行实例定义都不写** —— `Register` /
`Config` / `Send` 那四份逐字同构的薄壳、以及四份 `INSTANCE_DEF` 都已收进模块根的
`drv_referee2026.{h,c}`（调用时传 `&inst.core`）。该头由链路头自己带进来，app 不用直接 include。

## 怎么用（以常规链路为例）

```c
REFEREE2026_INSTANCE_DEF(referee_inst, COMMON);     /* 文件作用域，一次；第二个参数是链路名，全大写 */

/* 初始化（任务上下文，系统起来时一次） */
Referee2026Register(&referee_inst.core);            /* 仅一次，不可重入 */

const Referee2026Config_s cfg = {
    .uart_e = UART_1,                        /* 板载口，bsp 据此查映射；波特率见表 1-1（常规 115200 / 图传 921600） */
    .robot_id = 3,
    .filter = REFEREE2026_COMMON_FILTER_DEFAULT,  /* 本链路全部 RX；要挑就 |= 具体那几位 */
    .daemon_reload = 0,                      /* 0 = 用默认 300ms（本驱动挂了离线钩子，0 会被提升） */
    .daemon_fault = DAEMON_FAULT_BUZZER_SHORT,   /* 本链路无周期 RX 命令时被忽略（见图传链路说明） */
};
Referee2026Config(&referee_inst.core, &cfg);        /* 可重入；Config 类型四条链路共用 */

/* 发送（任务上下文；定长命令的长度必须等于表里的 data_len，0x0301 可取 6..118） */
Referee2026Send(&referee_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, &payload, sizeof(payload));

/* 接收：ISR 已经把数据写进快照，app 直接读字段 */
Referee2026PowerHeat_t v;
uint32_t t0;

do {
    t0 = referee_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT];
    v  = referee_inst.snapshot.power_heat;
} while (t0 == 0 || t0 != referee_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT]);
/* t0 == 0 = 从未收到过；两次 tick 不一样 = 读的过程中 ISR 改写过，重读 */
```

链路是否在线：`DaemonIsOnline(&referee_inst.core.daemon)` —— 注意它答的是"最近 reload ms
内有没有收到过**合法帧**"，不是"对端开机没有"。默认 reload 取 300ms = **三个 10Hz 周期**
（常规链路最快的命令是 10Hz、周期恰 100ms；取 100 就是零余量，一点抖动就误报离线）。
某条命令的新鲜度一律用 `tick` 差值（单位 µs、截断成 `uint32_t`、71.6 分钟回绕，
**比较一律用差值，别比大小**）。

**没有周期 RX 命令的链路不报离线**：图传链路的两条 RX（0x0302 / 0x0311）全是事件触发
（`freq_hz == 0`），对端正常但没在下发时链路上本来就一条帧都没有 —— 拿"多久没收到"当故障
判据只会在空档里误鸣蜂鸣器。故 `Config` 对这类链路**强制 `daemon_fault = DAEMON_FAULT_NONE`**
（derived 字段 `core->rx_periodic`）。daemon 本身仍装着：它还挂着 RX 停摆自恢复的离线钩子。

## 设计要点

- **`dir` 是方向位掩码；本框架下 35 条里只有 `0x0301` 取两位**：本框架永远跑在机器人这侧，
  "自定义控制器 → 裁判系统 PC 软件 → 裁判系统 → 图传链路 → 机器人"这条路上机器人是终点，
  故 0x0302 / 0x0311 只收、0x0309 / 0x0310 只发。唯一的双向是 `0x0301` 机器人交互数据 ——
  它既能发给服务器/选手端（子内容 0x0100~0x01FF），也能被**队友机器人**发过来
  （表 1-25 的子内容 `0x0200~0x02FF` 明写"机器人之间通信"），故它既有快照成员、也在过滤掩码里有位。
  `Referee2026Send` 拿 `dir & REFEREE2026_DIR_TX` 挡掉本链路发不出去的命令；内核派发时先看
  `dir & DIR_RX`（非 RX 命令连快照成员都没有）再看过掩码，并由 `dir` 推出"本链路有没有收得到的
  命令"（`rx_enabled`）与"有没有周期命令"（`rx_periodic`）。只发不收的链路（非链路 0x0306）
  据此**不起接收常开流、也不挂 daemon** —— daemon 在离线期间每拍都重放故障动作，给一条永远
  收不到帧的链路配上它，蜂鸣器会一直叫；无周期 RX 的链路则不报离线，见上一节。
- **收发接口与实例定义宏都只此一份**：四条链路曾各写一份 `Register`/`Config`/`Send`、各写一份
  `INSTANCE_DEF`，都逐字同构；现在统一在 `drv_referee2026.{h,c}`。收在一处的实例宏靠链路名拼接
  （`REFEREE2026_INSTANCE_DEF(name, LINK)` → `REFEREE2026_<LINK>_INSTANCE_TYPE` / `_CMD_INFO` /
  `_DATA_COUNT`，后两个别名由链路头给）。链路实例的三个内部指针（`snap`/`tick`/`tx_last_us`）在
  那个宏里用**自引用**的指定初始化器接好，编译期常量、零运行期接线。
- **快照 + seqlock**：每条 RX 命令在快照结构体里占一个成员，`snap_off` 是它的字节偏移
  （`offsetof`），由 `.c` 里的 `_Static_assert` 钉死"RX 段密排 == 快照大小"。ISR 写数据前后
  各写一次 `tick[id]`，app 三次读即可发现撕裂。
- **RX 在 ISR 里全程完成**：逐字节状态机 → 帧头 CRC8（查表）→ 收满 → CRC16 → 查表得数据名 →
  方向/过滤/长度三道门 → `memcpy` 进快照。跨 DMA-IDLE 事件的半帧存在 `asm_buf` 里，
  故"一个事件 0/1/多帧"都能处理。
- **`data_len` / `min_len` 一对上下界**（元信息表）：定长命令两者相等，`min_len` 存在的唯一
  理由是变长命令 `0x0301`（布局 = 6B 报文头 + ≤112B 子内容）。RX 侧数据段短于 `min_len` ⇒ 丢；
  长于 `data_len` ⇒ 截收前 `data_len` 字节；两者之间 ⇒ 收下收到的部分**并把快照成员余下清零**
  （不留上一帧的尾巴）。TX 侧 `Send` 要求 `min_len <= len <= data_len`。
- **TX 无队列**：一次一帧，在途或被限速就返回 `BSP_BUSY`（排队是上层的事）。入口先调
  `USARTRecoverTxIfStuck`，否则真卡死时再也走不到 `USARTTransmit`，链接永久静默。

## 计数（`inst.core.*`，ISR 写、任务读）

| 计数 | 含义 |
|---|---|
| `rx_valid` | 通过 CRC16 的全部帧 = `rx_ok + rx_filtered + rx_unknown`（daemon 拿它喂狗） |
| `rx_ok` | 认得且放行，**已写进快照** |
| `rx_filtered` | 认得但没写进快照：被方向/过滤掩码挡掉，或数据段比表里那条命令的 `min_len` 还短 |
| `rx_unknown` | CRC 过了但命令码不在本链路表里（多半是接错了链路） |
| `rx_crc_err` | CRC8/CRC16 失败（含噪声下重找起点时的重复计数） |
| `rx_len_err` | 长度异常：帧头 `data_length` 超上限，或比表里那条命令的 `min_len` 还短（原因计数） |
| `tx_ok` / `tx_dropped` | 已启动 DMA / 被限速或在途或外设忙而丢 |

## 开关与编译

- `app_cfg.h` 里 `DRV_REFEREE2026_USED` + 各链路 `REFEREE2026_LINK_<LINK>_USED`
  （`#define XXX_USED` 即开，**`#define XXX_USED 0` 仍是开**，一律用 `defined()`/`#ifdef`）。
  未启用时对应的 `.c` 编成空 TU，头文件里的声明仍在。
- 表与快照由各链路头里的 `_Static_assert` 自校：数据段长度、长度宏一致性、快照密排。
  这些断言放在**头文件**里，故任何 include 它的 TU 都会替我们校一遍，不会因为"开关没开、
  `.c` 编成空 TU"而静默消失。

## 坑（改这个模块前先看）

- **ISR 里绝不能用 `HAL_GetTick()`**：本板 HAL tick 源是优先级数值 ≥ 外设中断的 TIM，中断里
  它不前进，时间戳会全部相同、seqlock 与超时判据一起失效。内核一律用 `DWT_GetTimeUs()`。
- **`tick == 0` 兼作"从未收到"**（`DWT_GetTimeUs()` 恰好返回 0 只有上电后头 1µs）。
- **bsp 的 RX 缓冲每次续收前会被清**，回调内必须把 `rx_buff[0..rx_len)` 消费干净。
- **图传链路上 0xA5 与 0xA9 混跑**（图传遥控帧）：0xA9 不以 0xA5 打头，状态机天然跳过去。
  **解析 0xA9 不在本模块范围内**（见 `cubemx/board_config/硬件分配.md`）。
- **0xA9 遥控帧的 CRC 口径存疑、无权威出处**：`public/referee2026_proto.{h,c}` 备了一张
  非反射（CRC-16/CCITT-FALSE）表，而参考工程 `rm_referee` 的 `imageRoad.c` 用**反射**表校验
  `0xA9 0x53`；官方协议与附录里没有 0xA9 帧，无法裁决。那张表零运行期消费者 —— 真要实现
  0xA9 解析前先拿到"VT03·VT13 图传说明书"钉死方向，别照现有的表写。
- **`referee2026_uart_init[]` / `Referee2026LinkHasUart` 目前没有调用点**：运行期波特率来自
  CubeMX 生成的 `huart`，这张表只是"协议要求什么"的权威记录。风险在于 CubeMX 里把某个口配成
  别的波特率时**编译与运行都不报错**，只是链路静默收不到东西 —— 改 CubeMX 串口配置时回来核这张表。
- `DMA_RAM` 是 `__attribute__((section(".ram_d1")))`，只能加在**文件作用域**变量上，加不了
  结构体成员 —— 所以 `asm_buf` / `tx_buf` 由 `REFEREE2026_INSTANCE_DEF` 在文件作用域分配，
  内核里只存指针。
- `.core.snap = ...` 这种写法**不行**：`snap`/`tick`/`tx_last_us` 从属于 `.core`，
  必须先 `.core = { ... }` 再在里面写，另起 `.core.xxx =` 会被 `-Woverride-init`（在
  `-Wextra` 里）判成重复初始化。

## PC 端自测

```bash
drv/drv_referee2026/tools/test_referee2026.sh      # 退出码 0 = PASS，日志见 tools/*.log
```

直接编译**会上板的那份代码**（`drv_referee2026.c` + `public/referee2026_proto.c` +
`link_common/*.c` + `link_video/*.c` + `link_none/*.c` + `lib_crc.c`），只有 `main.h` /
`bsp_usart.h` / `drv_daemon.h` / `bsp_dwt.h` / `app_cfg.h` 用同目录的桩
（避免拉进 cubemx / FreeRTOS 头）。三条链路样本覆盖三种形态：常规（周期 RX + 非周期 RX + 只发）、
图传（两条 RX 全事件触发 ⇒ 不报离线）、非链路（只发不收 ⇒ 不挂 daemon）；雷达链路的表与快照
由头文件里的 `_Static_assert` 在 ARM 构建里把关。
覆盖：表与快照的密排契约、配置路径与掩码展开、好帧、七种坏帧各归各的计数且快照不动、
变长命令 0x0301（6B 收下并清零余下 / 5B 丢 / 118B 整段收下）、跨事件重组（一包两帧 /
半帧+半帧 / 噪声里混假 0xA5 的回退重扫）、TX 的方向与长度区间校验、组帧逐字节比对
（含 seq 递增）、在途与限速、回调适配、只发不收链路不挂 daemon、无周期 RX 链路强制 NONE。
（`drv_referee2026.c` 里的四个 bsp/daemon 适配钩子是 `static`，测试经实例上报存的回调指针反调。）

## 已知未验证项（实机待做）

- **视频链路 921600 下最坏一次 RX 回调的耗时**：一包里可能连着好几帧（最长 309B/帧），
  逐字节状态机 + 两次查表 CRC。这是本方案唯一没把握的地方 —— 建议上板时用 DWT 卡一下，
  超 100µs 就退回"攒够再派发"。
- 各链路真实的命令流频率（`rx_valid` 的增量）与 daemon 的 `reload_count`（默认 300ms）是否合适。
- 本模块**的开关已在 `half_rudder_gimbal` 里打开**（`DRV_REFEREE2026_USED` +
  `REFEREE2026_LINK_COMMON_USED` + `REFEREE2026_LINK_VIDEO_USED`），各 `.c` 都实打实在编译；
  但**还没有任何 app 落实例、也没有任何 app 调 `Referee2026Config()`** ⇒ 元信息表、快照与
  整个内核都是**无人引用的符号**，被 `--gc-sections` 全部丢掉 ——
  `arm-none-eabi-nm build/Debug/half_rudder_gimbal.elf | grep -c referee2026` 现在是 **0**，
  本模块对镜像的当前贡献是**零字节**（不是"表占 288B"）。
  等下一轮接上 app（`REFEREE2026_INSTANCE_DEF` 落实例 + `Config` 传 uart/robot_id/过滤掩码）
  才会真正进 `.rodata`/`.bss`：届时常规链路是表 24 × 12 = 288B + 快照 307B + tick/tx_last_us
  各 24 × 4B，图传链路再加自己那一份。**在那之前所有实机行为都还没跑过。**
- **0x0301 的子内容分发**：内核只把它整段（≤118B）拷进快照，8 种子内容按报文头里的类型码
  怎么分发给上层，是 app 层的事，本模块不做。
