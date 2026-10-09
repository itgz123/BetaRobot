/**
 * @file referee2026_video.h
 * @brief **图传链路**的完整驱动：4 条命令，2 收 2 发（无双向）
 *
 * 一个链路一个公开头，**用哪条链路就 include 哪个**：
 *   link_common/referee2026_common.h   常规链路 24 条
 *   link_video/referee2026_video.h     图传链路 4 条（本文件）
 *   link_none/referee2026_none.h       非链路 1 条
 *   link_radar/referee2026_radar.h     雷达无线链路 6 条
 * 四条链路**互不依赖**：协议层公共的东西在 public/ 里（帧格式与 ID 见 referee2026_frame.h、
 * 公共数据块与元信息结构见 referee2026_cmd.h），链路无关的收发内核与四条链路**共用的**
 * Register/Config/Send 见模块根的 drv_referee2026.h —— 各链路不再各写一份接口。
 *
 * **方向按"本框架永远跑在机器人这侧"判定**（详见 public/referee2026_cmd.h 的
 * `Referee2026Dir_e`）：自定义控制器/自定义客户端发给机器人的（0x0302 / 0x0311）只收，
 * 机器人发给它们的（0x0309 / 0x0310）只发。故本链路**没有双向命令**。
 *
 * 本文件按顺序放五样东西：
 *   ① 数据段结构体（+ `REFEREE2026_VIDEO_LEN_<TAG>` 长度宏 + `_Static_assert`）
 *   ② 数据名枚举 `Referee2026VideoDataId_e`（取值即元信息表下标）与元信息表声明
 *   ③ 接收过滤掩码 `Referee2026VideoFilter_e`
 *   ④ 快照结构体 `Referee2026VideoSnapshot_t` —— app 直接读它
 *   ⑤ 驱动实例（实例结构体类型 + 两个拼接别名）—— 实例定义宏与收发接口一律在模块根的
 *       drv_referee2026.h，本链路不定义
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）§1.4 图传链路数据说明
 *       （表 1-39 ~ 表 1-42）+ 表 1-5 命令码 ID 一览里"所属数据链路 = 图传链路"的 4 行。
 *
 * @warning 这根线上 **0xA5 与 0xA9 混跑**（0xA9 是图传遥控帧，见 cubemx/board_config/硬件分配.md）。
 *          0xA9 不以 0xA5 打头，RX 状态机天然会跳过去；**本轮不解析 0xA9**。
 * @note 链路的物理侧是图传发送端的串口，波特率 921600（协议表 1-1，见 referee2026_proto.h
 *       的 `referee2026_uart_init`）。速率高 ⇒ 一个 DMA-IDLE 事件里可能连着好几帧，
 *       内核的状态机本来就是逐个派发的。
 * @note 本文件只依赖 <stdint.h>、public/referee2026_frame.h 与 public/referee2026_cmd.h
 *       （后两者同样只依赖 <stdint.h>），协议部分 PC 端可单独语法检查；驱动部分
 *       （第 ⑤ 节）要 HAL，故整节在 `HAL_UART_MODULE_ENABLED` 里。
 */

#ifndef __REFEREE2026_VIDEO_H
#define __REFEREE2026_VIDEO_H

#include <stdint.h>

#include "referee2026_cmd.h"   /* Referee2026CmdInfo_t / 公共组成块 */
#include "referee2026_frame.h" /* REFEREE2026_FRAME_MAX 等帧层常量 */

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（4 条）
 *============================================*/

/* 0x0302 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER 30

/**
 * @brief 0x0302 自定义控制器与机器人交互数据（表 1-39，文档名 `custom_robot_data_t`）
 * @note 内容是"自定义控制器"自定义的，本模块不认识，按裸字节透传。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026CustomRobotData_t;

/* 0x0309 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX 30

/**
 * @brief 0x0309 机器人向自定义控制器发送的数据（文档名 `robot_custom_data_t`）
 * @note 与 0x0302 方向相反、格式同样由使用者自定义。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026RobotCustomData_t;

/* 0x0310 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT 300

/**
 * @brief 0x0310 机器人向自定义客户端发送的数据（文档名 `robot_custom_data_2_t`）
 * @note 图传链路里最长的一条（也是全协议最长的一条，`REFEREE2026_DATA_MAX` 就是照它定的），
 *       纯透传。
 */
typedef struct
{
    uint8_t data[300]; //!< 自定义数据，最长 300B
} Referee2026RobotCustomData2_t;

/* 0x0311 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT 30

/**
 * @brief 0x0311 自定义客户端向机器人发送的数据（文档名同为 `robot_custom_data_t`）
 * @note 官方文档与 0x0309 用了**同名结构体**，这里按命令码方向另起名以便区分。
 * @note 它是全部 35 条里速率上限最紧的一条（75Hz）。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026ClientToRobotData_t;

#pragma pack(pop)

/*============================================
 *   一之二、长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 public/referee2026_cmd.h 的公共约定）。
 * 放在头里而不是 .c 里：这样任何 include 本头的 TU 都会替我们校一遍，断言不会因为
 * "某个开关没开、.c 编成空 TU"而静默消失。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...) _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0302, sizeof(Referee2026CustomRobotData_t) == REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER);
REFEREE2026_CMD_STATIC_ASSERT(0x0309, sizeof(Referee2026RobotCustomData_t) == REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX);
REFEREE2026_CMD_STATIC_ASSERT(0x0310, sizeof(Referee2026RobotCustomData2_t) == REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT);
REFEREE2026_CMD_STATIC_ASSERT(0x0311, sizeof(Referee2026ClientToRobotData_t) == REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT);

#undef REFEREE2026_CMD_STATIC_ASSERT

/*============================================
 *   二、数据名与命令元信息表
 *============================================*/

/**
 * @brief 图传链路 4 条命令的数据名
 * @note **取值是顺序的 0..3**，末项 `_COUNT`；取值即 `referee2026_video_cmd_info[]` 的下标。
 *       只给首项标 `= 0`、其余交给编译器自增，取值必然连续无洞；顺序错位也不会串表
 *       （.c 里用的是指定初始化器 `[成员] = ...`）。
 * @note 成员名后缀逐字沿用原命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；**"发送方→接收方"是原 `Referee2026Receiver_e`
 *       字段的语义，那个字段已删（运行期零消费者），语义并到这里**（同下同）。
 */
typedef enum : uint8_t
{
    /* --- 图传链路（4 条） --- */
    REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER = 0, //!< 自定义控制器与机器人交互数据，发送方触发发送，频率上限为 30Hz（自定义控制器→机器人）
    REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER_RX,  //!< 自定义控制器接收机器人数据，频率上限为 10Hz（机器人→自定义控制器）
    REFEREE2026_VIDEO_DATA_ROBOT_TO_CLIENT,       //!< 机器人发送给自定义客户端的数据，频率上限为 50Hz（机器人→自定义客户端）
    /* 全部 35 条里速率最紧的一条 */
    REFEREE2026_VIDEO_DATA_CLIENT_TO_ROBOT, //!< 自定义客户端发送给机器人的自定义指令，频率上限为
                                            //!< 75Hz（自定义客户端→机器人）
    REFEREE2026_VIDEO_DATA_COUNT,           //!< 命令条数（数组维度，非命令）
} Referee2026VideoDataId_e;

/**
 * @brief 图传链路命令的元信息表（定义在 referee2026_video.c）
 * @note 字段语义（`data_len` 的字段表口径、频率两列的三种取值、`dir`、`snap_off`）统一写在
 *       public/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_VIDEO_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报
 *       未定义 —— 这是设计使然：没编进来的链路本就不该被用。
 */
extern const Referee2026CmdInfo_t referee2026_video_cmd_info[REFEREE2026_VIDEO_DATA_COUNT];

/*============================================
 *   三、接收过滤掩码
 *============================================*/

/**
 * @brief 图传链路的接收过滤掩码
 * @note 取值一律 `1u << <数据名>`，故第 i 位 = "收不收 `referee2026_video_cmd_info[i]`"，
 *       与内核 `core->filter` 的位序严格一致（内核在 ISR 里就是拿下标去移位比对的）。
 * @note 四条链路的掩码**统一用 `uint32_t`**，免得内核还要按链路分宽度 —— 代价是每条链路
 *       的命令数上限 32（本协议最多的常规链路 24 条，够用）。
 * @note **只给收得到的命令设位**：0x0309 / 0x0310 只发不收（`dir` 里没有 RX），给了位也没意义
 *       —— 内核派发时会先看方向、再看掩码。故本链路只有 2 个位。
 */
typedef enum : uint32_t
{
    REFEREE2026_VIDEO_FILTER_NONE = 0, //!< 什么都不收
    //! 本链路全部 2 条 RX 命令 —— `Config` 里填它 = 默认全收（与 `REFEREE2026_FILTER_DEFAULT` 等价）
    REFEREE2026_VIDEO_FILTER_DEFAULT = 0xFFFFFFFFu,
    REFEREE2026_VIDEO_FILTER_CUSTOM_CONTROLLER = 1u << REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER, //!< 收 0x0302 自定义控制器→机器人
    REFEREE2026_VIDEO_FILTER_CLIENT_TO_ROBOT = 1u << REFEREE2026_VIDEO_DATA_CLIENT_TO_ROBOT,     //!< 收 0x0311 自定义客户端→机器人
} Referee2026VideoFilter_e;

/*============================================
 *   四、快照结构体（app 直接读这里）
 *============================================*/

/* 快照是"逐字节镜像"：成员偏移必须等于前面各段长度之和，故显式紧凑化（理由同
 * referee2026_common.h 第六节；本链路两个成员都是 30B 裸字节数组，不 pack 也恰好无填充，
 * 但别让"恰好"当保证）。 */
#pragma pack(push, 1)

/**
 * @brief 图传链路 RX 快照
 * @note **只有收得到的命令有成员**，顺序与数据名枚举一致（0x0309 / 0x0310 只发不收，
 *       故不在其中）。每个成员的字节偏移被记在元信息表的 `snap_off` 里
 *       （`.c` 里用 `offsetof` 算出来），内核 memcpy 的目标就是
 *       `(uint8_t *)&snapshot + snap_off`。
 * @note 读法见 drv_referee2026.h 的"快照的读法"（tick 三步读，防读到半帧）：
 *       这两条都是裸字节，30B 的 memcpy 在 ISR 里是原子的吗？**不是** —— 图传链路 921600
 *       下一包可能连着几帧，写快照的过程照样会被更高优先级中断打断，故 tick 三步读不能省。
 */
typedef struct
{
    Referee2026CustomRobotData_t custom_controller; //!< 0x0302 自定义控制器→机器人
    Referee2026ClientToRobotData_t client_to_robot; //!< 0x0311 自定义客户端→机器人
} Referee2026VideoSnapshot_t;

#pragma pack(pop)

/*============================================
 *   五、驱动实例
 *============================================*/

/* 这一节要 HAL（`USARTInstance` / `DMA_RAM`），故先把开关本身引进来再判它 ——
 * 顺序颠倒的话 `HAL_UART_MODULE_ENABLED` 此刻还不可见，判据恒假、整节静默消失。
 * 这一行的代价是本头从此要求 `main.h` 在包含路径上（与 drv_dbus.h 等既有驱动一致）；
 * 上面四节（协议部分）本身只依赖 <stdint.h>，与 HAL 无关。 */
#include "main.h"

#ifdef HAL_UART_MODULE_ENABLED

#include "drv_referee2026.h"

/**
 * @brief 图传链路驱动实例
 * @note `core` **必须是第一个成员**（与仓库其余驱动的 vtable 习惯一致；本模块的代码本身不
 *       依赖这一点，回调一律从 `usart->parent` 取回内核 —— 那里存的是 `&inst->core`）。
 * @note 收发接口（Register / Config / Send）都是模块根那份 `Referee2026*`，调用时传
 *       `&inst.core` —— 例如 `Referee2026Config(&video_inst.core, &cfg)`。
 */
typedef struct
{
    Referee2026Core_t core;                            //!< 收发内核
    Referee2026VideoSnapshot_t snapshot;               //!< RX 快照（2 条）
    uint32_t tick[REFEREE2026_VIDEO_DATA_COUNT];       //!< 每条的最近收到时刻（seqlock 头尾序号）
    uint32_t tx_last_us[REFEREE2026_VIDEO_DATA_COUNT]; //!< 每条的最近发送时刻（限速用）
} Referee2026Video_t;

/**
 * @brief 实例结构体类型与元信息表的**拼接别名** —— 供模块根的 REFEREE2026_INSTANCE_DEF 使用
 * @note 实例定义宏四条链路只写一份（在 `drv_referee2026.h`），按链路名拼出三样东西，
 *       本头提供其中两样（`_DATA_COUNT` 本链路早就有了）：
 *       `REFEREE2026_<LINK>_INSTANCE_TYPE` / `_CMD_INFO` / `_DATA_COUNT`。
 *       故本头**一行收发/实例函数都不定义**，"本链路长什么样"全在这里、"怎么用"全在模块根。
 * @note 为什么要这层别名、而不是让内核宏自己拼：C 预处理器**不能转换大小写** ——
 *       `COMMON` 拼不出结构体名里的 `Common`，也拼不出表名里的小写 `common`。
 * @example
 *   REFEREE2026_INSTANCE_DEF(video_inst, VIDEO);   // 图传链路
 */
#define REFEREE2026_VIDEO_INSTANCE_TYPE Referee2026Video_t
#define REFEREE2026_VIDEO_CMD_INFO referee2026_video_cmd_info

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __REFEREE2026_VIDEO_H */
