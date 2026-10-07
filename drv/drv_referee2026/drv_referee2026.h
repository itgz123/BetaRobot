/**
 * @file drv_referee2026.h
 * @brief 2026 赛季裁判系统驱动的**暴露接口** —— 共享收发内核 + 四条链路共用的生命周期/TX 函数
 *
 * 一个模块两个目录：
 *   - **本文件（模块根）**：与链路无关的收发内核 `Referee2026Core_t`、运行期配置
 *     `Referee2026Config_s`、以及四条链路**只定义一次**的接口
 *     {@link Referee2026Register} / {@link Referee2026Config} / {@link Referee2026Send}
 *     （实现见同目录的 `drv_referee2026.c`）；
 *   - **`public/`**：多条链路公共的协议定义 —— 帧格式（`referee2026_frame.h`）、跨链路公共
 *     数据块与命令元信息（`referee2026_cmd.h`）、HAL UART 参数表与三张 CRC 表
 *     （`referee2026_proto.{h,c}`）。
 *
 * **四条链路各只提供"本链路长什么样"**（数据段结构体、数据名枚举、长度宏、元信息表、
 * 过滤掩码、快照结构体、实例结构体类型），**不再各写一份收发接口，也不再各写一份实例定义宏**
 * —— 那四份 `Register`/`Config`/`Send` 与四份 `INSTANCE_DEF` 都曾逐字同构，现在统一到这里
 * （{@link REFEREE2026_INSTANCE_DEF} 靠链路名拼接）。用哪条链路就
 * `#include "referee2026_common.h"` / `_video.h` / `_none.h` / `_radar.h`。
 *
 * 内核提供四件事：
 *   - 生命周期：{@link Referee2026Register} → {@link Referee2026Config}（可重入）；
 *   - RX：{@link Referee2026RxIsr}（**纯函数**，可脱离硬件单测），bsp 的 `rx_callback` 适配
 *     是本文件的静态函数（从 `usart->parent` 取回内核）；
 *   - TX：{@link Referee2026Send}（限速 → 组帧 → DMA）+ 完成/错误/离线三个静态钩子；
 *   - 看门狗：内嵌 daemon（RX 喂狗 + 停摆后重启接收）。
 *
 * ## 与 sbus/dbus 类驱动的根本差异（为什么不能照抄它们）
 *   1. **收发频率不同**：RX 是 21+ 条命令按 1/3/10Hz 周期 + 事件触发混流而下；TX 的 7 条全是
 *      触发式（`freq_hz` 恒为 0，只有 `freq_max_hz` 上限）；且 0x0301 同时收发。
 *   2. **长度不固定**：数据段 0~300B，长度只在帧头的 `data_length` 里；且一个 DMA-IDLE 事件里
 *      可能有 0 个、1 个或多个完整帧（921600 的图传链路尤甚）。
 *   故 RX 侧必须有**跨事件重组**的状态机（`asm_*` 三个字段），不能"回调一次 = 一帧"。
 *
 * ## 快照的读法（**app 侧必读**）
 * 快照结构体由 ISR 直接改写、app 在任务里读。单条命令的数据段 memcpy 到一半被读走就会读到
 * "半新半旧"的值 —— 故 `tick[]` 被当成 **seqlock 的头尾序号**用：ISR 先写 tick、再写数据、
 * 最后再写一遍 tick；app 读 tick → 读数据 → 再读 tick，两次不一样就重读。
 * ```c
 * uint32_t t0 = inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT];
 * Referee2026PowerHeat_t v = inst.snapshot.power_heat;
 * if (t0 != 0 && t0 == inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT])
 * {
 *     // v 可用
 * }
 * ```
 * （示例里用 `//` 而不是块注释：块注释的起始符嵌在文档注释里会被 -Wcomment 逮住。）
 * `tick == 0` 兼作"**从未收到过**"（`DWT_GetTimeUs()` 恰好返回 0 只有上电后头 1µs）。
 * tick 单位 µs、截断成 `uint32_t`（71.6 分钟回绕），判"多久没来"一律用**差值**，别比大小。
 *
 * @warning seqlock 只能防"读到半帧"，防不了"两条命令之间不一致"（血量与热量分属两帧）——
 *          那是协议本身的时序，不是本模块能解决的。
 * @note 本文件由四条链路的公开头引用，app 跟着链路头走，**不必直接 include 它**；
 *       例外是它要谈"这条链路在不在线"时用 `DaemonIsOnline(&inst.core.daemon)`。
 *
 * @warning **ISR 里绝不能用 `HAL_GetTick()`**：本板 HAL tick 源是优先级数值 ≥ 外设中断的 TIM，
 *          中断里它**不前进**，时间戳会全部相同、seqlock 与超时判据一起失效。
 *          内核一律用 `DWT_GetTimeUs()`（不受中断影响，`bsp_bxcan.c` 已在 ISR 里这么用）。
 */

#ifndef __DRV_REFEREE2026_H
#define __DRV_REFEREE2026_H

#include "main.h"

/* 整个文件都在 `HAL_UART_MODULE_ENABLED` 里：`USARTInstance` 这个类型本身就由该开关决定
 * 存不存在（stm32*xx_hal_conf.h 不引 hal_uart.h 时连类型都没有）。开关用法与全仓一致 ——
 * 判据是"定义与否"，`HAL_UART_MODULE_ENABLED = 0` 仍是打开。
 * main.h 必须在上方先引入，否则该宏此刻还不可见（顺序颠倒会恒假）。 */
#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_usart.h" /* USARTInstance / USARTTransmit / BSP_Status_e（经 bsp_common.h） */

#include "drv_daemon.h" /* DaemonInstance / Daemon_Config_s / offline_callback */

#include "referee2026_cmd.h" /* Referee2026CmdInfo_t / Referee2026Dir_e（public/） */

/*============================================
 *   一、可调常量
 *============================================*/

/**
 * @brief USART 接收缓冲大小（`USART_INSTANCE_DEF` 的第二个参数）
 * @note **必须 ≥ REFEREE2026_FRAME_MAX**（309）：一次 DMA-IDLE 事件最多交付这么多字节，
 *       缓冲比最长帧还小的话，最长的 0x0310（300B 数据段）永远收不齐。
 *       取 512 = 留出"一包里挤进两条中等长度命令"的余量；再长的流会被拆成多次事件，
 *       状态机本来就能跨事件重组，不影响正确性。
 */
#ifndef REFEREE2026_RX_BUFF_SIZE
#define REFEREE2026_RX_BUFF_SIZE 512u
#endif

/**
 * @brief 重组缓冲 / 发送缓冲大小 = 最长整帧（帧头 5 + 命令码 2 + 数据段 300 + 帧尾 2）
 * @note 不能改小；改大没意义（协议封顶）。
 */
#define REFEREE2026_CORE_BUF_SIZE REFEREE2026_FRAME_MAX

/**
 * @brief 过滤掩码的"全选"哨兵
 * @note 传给 `Config` 时由内核按表的 `dir & RX` **逐条展开**成真掩码 —— 免得调用方手写
 *       20 个或项、还漏一条。展开后 `core->filter` 里绝不可能是这个值本身。
 */
#define REFEREE2026_FILTER_DEFAULT UINT32_MAX

/**
 * @brief 接收停摆自恢复的限频周期（ms），传给 `USARTRecoverRxIfStalled`
 * @note 100 沿用仓库既有配置：真停摆时刻意重试，但别在 1ms 的 daemon 节拍上反复 Abort。
 */
#ifndef REFEREE2026_RX_RESTART_PERIOD_MS
#define REFEREE2026_RX_RESTART_PERIOD_MS 100u
#endif

/**
 * @brief daemon 的默认喂狗超时（ms）
 * @note 链路 `Config` 里的 `daemon_reload` 填 0（本义"禁用监控"）会被提升成本值 ——
 *       **本驱动挂了 daemon 离线钩子**（RX 停摆自恢复），0 若真禁用掉，那层保护就静默失效了
 *       （同 drv_dbus / comm 各后端的既有做法）。
 * @note **取 300 是"三个 10Hz 周期"**，不是随便定的：喂狗口径是"收到**任何**一条通过校验
 *       的帧就刷一次"（见 `rx_valid`），撞线的是**常规链路里最快的**那批 10Hz 命令
 *       （周期恰 100ms）。取 100 就是零余量 —— 一个 DMA-IDLE 事件的抖动、一次任务被抢占
 *       造成的喂狗延迟，都会让它误报离线而触发蜂鸣器；3 个周期才容得下抖动。
 *       反过来也不能取太大：它同时是这个自恢复钩子的响应延迟上界。
 * @note 判断"链路是否在线"请用 `DaemonIsOnline(&inst.core.daemon)`，但先想清语义：它答的是
 *       "最近 `REFEREE2026_DAEMON_RELOAD_MS` 有没有帧"，不是"对端开机没有"，也不是
 *       "那条 1Hz 的命令还来不来"。要判**某条命令**的新鲜度，一律用它的 `tick` 差值。
 * @note 图传链路的周期帧一条都没有（两条 RX 全是事件触发）⇒ 本判据对它**不成立**，
 *       `Config` 会把那种链路的故障动作强制成 NONE，见 `rx_periodic`。
 */
#ifndef REFEREE2026_DAEMON_RELOAD_MS
#define REFEREE2026_DAEMON_RELOAD_MS 300u
#endif

/*============================================
 *   二、重组状态机的状态（内核内部）
 *============================================*/

/**
 * @brief RX 重组状态
 * @note 逐字节喂入，跨 DMA-IDLE 事件保持；`asm_len` 是当前已攒的字节数。
 *       - SOF    ：还没对齐，等 0xA5（其余字节直接丢）；
 *       - HEADER ：攒满 5B 帧头即校 CRC8，过则算出本帧总长；
 *       - BODY   ：攒满 `asm_total` 即校 CRC16 并派发。
 * @note CRC8 失败**不整帧丢弃**：把已收的 5B 里 [1..5) 那 4B 退回重扫
 *       （真起点很可能就在其中），见 `Referee2026CoreRxRewind`。
 */
typedef enum : uint8_t
{
    REFEREE2026_RX_ST_SOF = 0,    //!< 找起点：等 0xA5
    REFEREE2026_RX_ST_HEADER = 1, //!< 收帧头 5B，满则校 CRC8
    REFEREE2026_RX_ST_BODY = 2,   //!< 收 cmd_id + 数据段 + 帧尾 CRC16
} Referee2026RxState_e;

/*============================================
 *   三、共享内核结构体
 *============================================*/

/**
 * @brief 一条链路的收发内核（作为链路实例结构体的**第一个成员**）
 *
 * 分三段：
 *   ① **绑定**（`usart` / `daemon` / `info` / `count` / `robot_id` / 三个指向实例内数组的指针）——
 *      全部由 `REFEREE2026_INSTANCE_DEF` 宏在编译期的指定初始化器里填好（自引用取 `&name.*`），
 *      之后**只读**；
 *   ② **缓冲**（`asm_buf` / `tx_buf`）—— 都由 `REFEREE2026_INSTANCE_DEF` 在文件作用域分配、这里只存指针。
 *      不能把数组直接放结构体里：`tx_buf` 要走 DMA，而 `DMA_RAM`（H7 上是
 *      `__attribute__((section(".ram_d1")))`）**只能加在文件作用域的变量上**，加不了成员；
 *   ③ **内部状态**（`asm_*` / `seq` / `tx_busy` / 计数）—— 外部别看，计数只读。
 *
 * @note 内核同时兼管本链路的 **daemon**（RX 喂狗 + 停摆后重启接收）。看门狗不是协议的
 *       东西，但四条链路的用法逐字相同，放链路层要抄四遍，故收在这里。
 */
typedef struct
{
    /*-------------- ① 绑定（REFEREE2026_INSTANCE_DEF 填好之后只读）--------------*/

    USARTInstance *usart;             //!< bsp 层实例（收发都经它）
    DaemonInstance *daemon;           //!< 本链路的看门狗（只发不收的链路不挂它）
    const Referee2026CmdInfo_t *info; //!< 本链路的元信息表
    uint16_t count;                   //!< 表长（== 本链路 `_DATA_COUNT`）
    uint16_t robot_id;                //!< 本机机器人 ID（`Config` 写入；payload 里的 sender_id 由调用方填）
    uint8_t *snap;                    //!< 本链路**快照结构体**基址（ISR 写、app 读）
    uint32_t *tick;                   //!< 长度 count；最近一次收到的时间戳 + seqlock 头尾序号
    uint32_t *tx_last_us;             //!< 长度 count；最近一次发送时刻（限速用，0 = 从未发过）
    uint32_t filter;                  //!< 展开后的过滤掩码：第 i 位 = 收不收 `info[i]`

    /*-------------- ② 缓冲（由 REFEREE2026_INSTANCE_DEF 在文件作用域分配）--------------*/

    uint8_t *asm_buf; //!< 跨事件重组缓冲，长度 REFEREE2026_CORE_BUF_SIZE（仅 CPU 访问）
    uint8_t *tx_buf;  //!< 组帧缓冲，长度同上；**DMA 直接读** ⇒ 必须落在 DMA_RAM

    /*-------------- ③ 内部状态（外部只读）--------------*/

    uint16_t asm_len;               //!< 重组缓冲里已攒的字节数
    uint16_t asm_total;             //!< 当前帧的整帧长 = 9 + data_length
    Referee2026RxState_e asm_state; //!< 重组状态
    uint8_t seq;                    //!< 发送包序号（本链路所有命令共用一个递增计数）
    volatile uint8_t tx_busy;       //!< 1 = 有一帧在 DMA 在途（tx_callback 里清）

    /* 1 = 本链路**有**收得到的命令（表里至少一条 `dir & RX`）。由 `Config` 从表里派生，
     * **不是**配置项：`dir` 已经是"协议上收不收得到"的唯一出处，再让调用方配一遍
     * 就是同一个事实写两遍。为 0 的链路（只发不收，如非链路 0x0306）**不起接收常开流、
     * 不配看门狗** —— 那种链路的 daemon 会永久喂不上狗，而 DaemonTask 在离线期间**每拍**都
     * 重放故障动作（蜂鸣器会一直叫），必须从根上不挂它。 */
    uint8_t rx_enabled;

    /* 1 = 本链路**有周期**的 RX 命令（表里至少一条 `dir & RX` 且 `freq_hz != 0`）。同样由
     * `Config` 从表里派生。它决定 daemon 的离线判据成不成立：
     *   - 有周期帧（常规链路：10Hz 的 0x0003/0x0006…，雷达链路：1Hz~10Hz 若干）⇒ "多久没
     *     收到"就是真信号，故障动作照调用方配的来；
     *   - **没有周期帧**（图传链路两条 RX —— 0x0302、0x0311 —— 全是事件触发，`freq_hz == 0`）
     *     ⇒ 对端正常但没在下发时，链路上本来就一条帧都没有；此时"多久没收到"不是故障判据，
     *     照配置执行只会让蜂鸣器在无事发生的空档里误报。故 `Config` 把这种链路的
     *     `daemon_fault` **强制成 `DAEMON_FAULT_NONE`**。
     * 注意**只清故障动作、不注销 daemon**：daemon 还挂着 RX 停摆自恢复的离线钩子
     * （它是唯一能在接收真停摆后把 DMA 重新拉起来的地方），把它一并去掉会把那层保护也带走。 */
    uint8_t rx_periodic;

    /* 计数：ISR 侧写、任务侧只读。前四个是**分类**计数（两两不重叠、加起来等于 rx_valid），
     * 后两个是**原因**计数（回答"为什么没进来"，故 rx_len_err 会与 rx_filtered 重叠）：
     *   rx_valid    通过 CRC16 的全部帧 = rx_ok + rx_filtered + rx_unknown
     *               （daemon 拿它喂狗 —— 被过滤/不认识的帧同样证明"链路还活着"）
     *   rx_ok       校验通过、认得、方向与过滤都放行 ⇒ **已写进快照**
     *   rx_filtered 校验通过、认得，但没写进快照：被方向或过滤掩码挡掉，或数据段比表里短
     *   rx_unknown  校验通过，但命令码不在本链路表里（多半是接错了链路）
     *   rx_crc_err  CRC8/CRC16 失败（含噪声下重新找一个起点时的重复计数）
     *   rx_len_err  长度存不下/收不齐，两种情形都归这里：帧头 data_length 超过数据段上限
     *               （噪声把它填成了巨值，此时帧头 CRC8 是过的 —— 故不计 rx_crc_err，而它
     *               也没通过 CRC16，故不计 rx_valid/rx_filtered），或比表里那条命令的
     *               **`min_len`** 还短（定长命令即 data_len；变长命令 0x0301 允许短到 6B，
     *               见 cmd.h 的口径说明；这一支同时计入 rx_filtered） */
    uint32_t rx_valid;
    uint32_t rx_ok;
    uint32_t rx_filtered;
    uint32_t rx_unknown;
    uint32_t rx_crc_err;
    uint32_t rx_len_err;
    uint32_t tx_ok;      //!< 已受理并启动 DMA 的帧数
    uint32_t tx_dropped; //!< 因限速/在途/外设忙而被丢的帧数
} Referee2026Core_t;

/*============================================
 *   四、配置结构体
 *============================================*/

/**
 * @brief 运行期配置（四条链路**共用同一个类型**，`Config` 用）
 * @note 板级值（用哪个 UART、本机 ID、过滤掩码）在这里给，不设编译期宏 —— 与全仓
 *       "配置走 Config 参数、开关用 #ifdef"的约定一致。
 * @note `filter` 的类型是 `uint32_t` 而不是各链路的 `Referee2026<Link>Filter_e`：那四个枚举
 *       本身就是 `uint32_t`，填 `REFEREE2026_<LINK>_FILTER_DEFAULT` / 具体位都能隐式转换；
 *       这样本结构体不必认识任何链路的过滤枚举。位序与内核 `core->filter` 严格一致
 *       （第 i 位 = 收不收 `info[i]`）。
 */
typedef struct
{
    BoardUART_e uart_e;               //!< 板载 UART（bsp 据此查硬件映射）
    uint16_t robot_id;                //!< 本机机器人 ID（存实例里备查；payload 里的 sender_id 由调用方填）
    uint32_t filter;                  //!< 接收过滤掩码；`REFEREE2026_FILTER_DEFAULT` = 本链路全部可取 RX 的命令
    uint16_t daemon_reload;           //!< daemon 喂狗重载值（ms）；0 会被提升为 REFEREE2026_DAEMON_RELOAD_MS（300）
    DaemonFaultAction_e daemon_fault; //!< daemon 离线故障动作；**无周期 RX 的链路（图传）会被强制成 DAEMON_FAULT_NONE**
} Referee2026Config_s;

/*============================================
 *   五、接口（四条链路共用，只此一份实现）
 *============================================*/

/**
 * @brief 注册底层实例（**仅一次、不可重入**）
 * @param core 内核（实例的 `core` 成员，即 `&inst.core`；`usart` / `daemon` 必须已由
 *             `REFEREE2026_INSTANCE_DEF` 就位）
 * @retval BSP_OK        两个实例都注册成功
 * @retval BSP_PARAM_ERR 内核为空或 `usart` 为空
 * @retval 其它           透传 `USARTRegister` 的失败码
 * @note 只登记、不配硬件（硬件参数由 `Config` 负责），故必须先 `Register` 再 `Config`。
 */
BSP_Status_e Referee2026Register(Referee2026Core_t *core);

/**
 * @brief 配置并启动（可重复调用）
 *
 * 依次做四件事：存 `robot_id` → 展开过滤掩码、派生 `rx_enabled`/`rx_periodic`、复位状态与计数
 * → 装 bsp 的三个回调（`parent` 指回内核）→ 装 daemon、`USARTReceive` 起常开流。
 *
 * @param core 内核（`&inst.core`；绑定与缓冲区都已由 `REFEREE2026_INSTANCE_DEF` 就位）
 * @param cfg  硬件、ID 与看门狗参数
 * @retval BSP_OK 已启动
 * @retval BSP_PARAM_ERR 内核或配置为空 / `usart` 为空
 * @retval 其它 透传 `USARTConfig` / `USARTReceive` 的失败码
 * @note `daemon_reload` 填 0 会被提升为 REFEREE2026_DAEMON_RELOAD_MS（本驱动挂了离线钩子）。
 * @note **本链路没有周期 RX 命令**时（`core->rx_periodic == 0`，如只跑事件触发的图传链路），
 *       `cfg->daemon_fault` 被强制成 `DAEMON_FAULT_NONE` —— 那种链路空闲时本来就没帧，
 *       照配置报离线只会误鸣蜂鸣器。daemon 本身仍装上（离线钩子还要用）。
 * @note 本链路的命令**一条都收不到**时（`core->rx_enabled == 0`）只做 `USARTConfig`
 *       （发送仍要走它查硬件句柄），**不配 daemon、不起接收**，直接返回 `BSP_OK`。
 */
BSP_Status_e Referee2026Config(Referee2026Core_t *core, const Referee2026Config_s *cfg);

/**
 * @brief 发一条命令（**任务上下文**）
 *
 * 依次校验：下标范围 → **方向**（`dir & REFEREE2026_DIR_TX`）→ 数据段长度 → 限速 → 外设空闲，
 * 通过后组帧（`A5 | data_length(LE) | seq++ | CRC8 | cmd_id(LE) | data | CRC16(LE)`）并启动 DMA。
 *
 * @param core    内核（`&inst.core`）
 * @param data_id 本链路的**数据名**（`Referee2026<Link>DataId_e`），不是命令码
 * @param data    数据段指针（结构体地址即可，内部按字节拷进 `tx_buf`）
 * @param len     数据段长度，须落在 `[info[data_id].min_len, info[data_id].data_len]`。
 *                定长命令两者相等 ⇒ 等价于"**必须恰好等于**"；变长命令（0x0301）可取区间内
 *                任意值 —— 子内容多长就发多长，不必补到 118B。
 * @retval BSP_OK        已受理（DMA 已启动，完成走 `tx_callback`）
 * @retval BSP_PARAM_ERR 空指针 / 下标越界 / **该命令本链路发不出去** / 长度超出该区间
 * @retval BSP_BUSY      限速窗口内，或上一帧仍在途，或外设忙（计数 `tx_dropped` 后返回）
 * @retval 其它          透传 `USARTTransmit` 的失败码（同样计入 `tx_dropped`）
 *
 * @note **无发送队列**，与 bsp 的既定策略一致（忙就报忙，排队是上层的事）：一帧在途时
 *       再来一帧直接被拒。调用方应把发送放在自己的周期任务里，并容忍偶发 `BSP_BUSY`。
 * @note 限速依据 `info[data_id].freq_max_hz`（0 = 不限速）：最小间隔 `1000000 / freq_max_hz` µs。
 *       固定周期发送的行（`freq_hz == freq_max_hz`）因此天然按协议上限节流。
 * @note 发送缓冲是内核自持的 `tx_buf`（DMA 异步读，绝不能用调用方的栈缓冲）。
 * @note 入口先调一次 `USARTRecoverTxIfStuck`：发送卡死时 `tx_busy` 永远不会被清，
 *       而本函数的在途/限速判断都在调 `USARTTransmit` 之前 —— 没有这一步就再也回不到
 *       `USARTTransmit`，卡死复位永远没机会执行，链接永久静默（见 bsp_usart.h 的长注释）。
 */
BSP_Status_e Referee2026Send(Referee2026Core_t *core, uint16_t data_id, const void *data, uint16_t len);

/**
 * @brief 喂入一段刚收到的字节（**ISR 上下文**的纯函数，可脱离硬件单测）
 *
 * bsp 每次交付 `usart->rx_buff[0 .. usart->rx_len)`，且**续收前会把该缓冲清掉**
 * （见 bsp_usart.c 的 `USART_StartReceiveRaw`），故本函数必须把 `len` 个字节全部消费完 ——
 * 它也确实这么做：逐字节喂进重组状态机，跨事件留在 `asm_buf` 里的半帧不受影响
 * （**不依赖 bsp 缓冲留存任何东西**）。
 *
 * @param core 内核
 * @param buf  收到的字节（可为 bsp 的缓冲，本函数只读）
 * @param len  字节数；0 是合法的（IDLE 事件无数据），此时什么都不做
 * @note 运行时由 bsp 的 `rx_callback` 经静态适配函数调进来（本文件自带，无需调用方挂）；
 *       暴露它是为了 PC 端单测能直接喂字节，以及将来可能有的非 DMA 接收通路。
 * @note 时间复杂度 O(len)，每条命令最多两次查表 CRC（256 项表，`lib_crc`）；
 *       实机待测项见 readme.md。
 */
void Referee2026RxIsr(Referee2026Core_t *core, const uint8_t *buf, uint16_t len);

/*============================================
 *   六、实例定义宏（四条链路共用，只此一份）
 *============================================*/

/**
 * @brief 静态定义某条链路的驱动实例（含 UART 与 daemon 的底层实例、两块内部缓冲）
 *
 * 四条链路曾经各写一份本宏，逐字同构、只有三处 token 不同，故合成一份、靠 `LINK` 拼接 ——
 * 那三处由链路头随类型一起提供：
 *   - 实例结构体类型 `REFEREE2026_<LINK>_INSTANCE_TYPE`
 *   - 元信息表 `REFEREE2026_<LINK>_CMD_INFO`
 *   - 数据名计数 `REFEREE2026_<LINK>_DATA_COUNT`
 * 故本宏**不必认识任何链路**，加一条链路也只是在它的头里多定义两个别名。
 *
 * @param name 实例名称（文件作用域标识符）
 * @param LINK 链路名，**必须全大写**：`COMMON` / `VIDEO` / `NONE` / `RADAR`
 * @note `asm_buf` 只被 CPU 访问，`tx_buf` 走 DMA 故必须 `DMA_RAM`；两者都由本宏在**文件
 *       作用域**分配 —— `DMA_RAM` 是 `__attribute__((section(...)))`，加不到结构体成员上。
 * @note `snap`/`tick`/`tx_last_us` 三个指针用**自引用**的指定初始化器接好
 *       （`(uint8_t *)&name.snapshot` 等，写法已在 PC 上验过 `-Wpedantic -Werror` 干净）：
 *       它们是编译期常量，没有任何运行期接线，`Referee2026Config` 也不必再管它们。
 *       三者都从属于 `.core`，故**必须写在 `.core = { ... }` 块内部** ——
 *       另起 `.core.snap = ...` 会被 `-Woverride-init`（在 `-Wextra` 里）判成重复初始化。
 * @note 只能用在**文件作用域**，且该文件必须先 include 对应链路的公开头（别名与类型都来自它）。
 * @example
 *   REFEREE2026_INSTANCE_DEF(common_inst, COMMON);   // 用常规链路
 */
#define REFEREE2026_INSTANCE_DEF(name, LINK)                                                                           \
    USART_INSTANCE_DEF(name##_usart, REFEREE2026_RX_BUFF_SIZE);                                                        \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                \
    static uint8_t name##_asm_buf[REFEREE2026_CORE_BUF_SIZE];                                                          \
    static uint8_t name##_tx_buf[REFEREE2026_CORE_BUF_SIZE] DMA_RAM = {0};                                             \
    static REFEREE2026_##LINK##_INSTANCE_TYPE name = {                                                                 \
        .core =                                                                                                        \
            {                                                                                                          \
                .usart = &name##_usart,                                                                                \
                .daemon = &name##_daemon,                                                                              \
                .info = REFEREE2026_##LINK##_CMD_INFO,                                                                 \
                .count = REFEREE2026_##LINK##_DATA_COUNT,                                                              \
                .snap = (uint8_t *)&name.snapshot,                                                                     \
                .tick = name.tick,                                                                                     \
                .tx_last_us = name.tx_last_us,                                                                         \
                .asm_buf = name##_asm_buf,                                                                             \
                .tx_buf = name##_tx_buf,                                                                               \
            },                                                                                                         \
    }

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __DRV_REFEREE2026_H */
