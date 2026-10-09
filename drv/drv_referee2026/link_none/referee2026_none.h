/**
 * @file referee2026_none.h
 * @brief **非链路**（表 1-5 里"所属数据链路"写作 `-` 的那一档）的完整驱动：1 条命令，只发不收
 *
 * 一个链路一个公开头，**用哪条链路就 include 哪个**：
 *   link_common/referee2026_common.h   常规链路 24 条
 *   link_video/referee2026_video.h     图传链路 4 条
 *   link_none/referee2026_none.h       非链路 1 条（本文件）
 *   link_radar/referee2026_radar.h     雷达无线链路 6 条
 * 四条链路**互不依赖**：协议层公共的东西在 public/ 里（帧格式与 ID 见 referee2026_frame.h、
 * 公共数据块与元信息结构见 referee2026_cmd.h），链路无关的收发内核与四条链路**共用的**
 * Register/Config/Send 见模块根的 drv_referee2026.h —— 各链路不再各写一份接口。
 *
 * 本文件按顺序放五样东西：
 *   ① 数据段结构体（+ `REFEREE2026_NONE_LEN_<TAG>` 长度宏 + `_Static_assert`）
 *   ② 数据名枚举 `Referee2026NoneDataId_e`（取值即元信息表下标）与元信息表声明
 *   ③ 接收过滤掩码 `Referee2026NoneFilter_e`
 *   ④ 快照结构体 `Referee2026NoneSnapshot_t` —— app 直接读它
 *   ⑤ 驱动实例（实例结构体类型 + 两个拼接别名）—— 实例定义宏与收发接口一律在模块根的
 *       drv_referee2026.h，本链路不定义
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）§1.5 自定义控制器 /
 *       附录一（表 1-43）+ 表 1-5 命令码 ID 一览里"所属数据链路 = 非链路"的 1 行。
 *
 * @note 本链路的 0x0306 是**自定义控制器与己方选手端直连**的串口，不经过服务器也不经过图传。
 *       自定义控制器到机器人的**完整路径**是：自定义控制器 --(0x0306，本链路)--> 裁判系统
 *       PC 软件（选手端）--> 裁判系统 --> 图传链路 --> 机器人；本链路只管这条路上**第一段**，
 *       下游那两段是图传链路（0x0302 / 0x0311）的事。
 * @note 本框架所跑的板子在这里只当**发送方**（自定义控制器那侧），故整条链路
 *       **一条收得到的命令都没有**：内核据此不起接收常开流、也不挂看门狗
 *       （判据由表里的 `dir` 派生，见 drv_referee2026.h 的 `rx_enabled`）。
 * @note 本文件只依赖 <stdint.h>、public/referee2026_frame.h 与 public/referee2026_cmd.h
 *       （后两者同样只依赖 <stdint.h>），协议部分 PC 端可单独语法检查；驱动部分
 *       （第 ⑤ 节）要 HAL，故整节在 `HAL_UART_MODULE_ENABLED` 里。
 */

#ifndef __REFEREE2026_NONE_H
#define __REFEREE2026_NONE_H

#include <stdint.h>

#include "referee2026_cmd.h"   /* Referee2026CmdInfo_t / 公共组成块 */
#include "referee2026_frame.h" /* REFEREE2026_FRAME_MAX 等帧层常量 */

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（1 条）
 *============================================*/

/* 0x0306 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA 8

/**
 * @brief 0x0306 自定义控制器 ↔ 选手端直连数据（表 1-43，文档名 `custom_client_data_t`）
 *
 * @note 鼠标操作要求**三帧一组**：先发"未按下 + 指定位置"、再发"该位置 + 按下"、
 *       最后发"该位置 + 未按下"，依次描述一次移动点击。
 * @note 位域写法与字节布局的关系见 public/referee2026_cmd.h 的公共约定：`: N` 只影响可读性，
 *       布局由"字段表逐字段给出的偏移"钉死，故这里连 `uint16_t` 也显式写出来。
 */
typedef struct
{
    uint16_t key_value;       //!< 偏移 0 键盘按键通用键值
    uint16_t x_position : 12; //!< 偏移 2 鼠标 x 坐标
    uint16_t mouse_left : 4;  //!< 同一 16 位字的高 4 位：鼠标左键状态
    uint16_t y_position : 12; //!< 偏移 4 鼠标 y 坐标
    uint16_t mouse_right : 4; //!< 同一 16 位字的高 4 位：鼠标右键状态
    uint16_t reserved;        //!< 偏移 6 保留位
} Referee2026CustomClientData_t;

#pragma pack(pop)

/*============================================
 *   一之二、长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 public/referee2026_cmd.h 的公共约定）。
 * 放在头里而不是 .c 里：这样任何 include 本头的 TU 都会替我们校一遍，断言不会因为
 * "某个开关没开、.c 编成空 TU"而静默消失。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...) _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0306, sizeof(Referee2026CustomClientData_t) == REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA);

#undef REFEREE2026_CMD_STATIC_ASSERT

/*============================================
 *   二、数据名与命令元信息表
 *============================================*/

/**
 * @brief 非链路 1 条命令的数据名
 * @note **取值是顺序的 0..0**，末项 `_COUNT`；取值即 `referee2026_none_cmd_info[]` 的下标。
 *       只给首项标 `= 0`、其余交给编译器自增，取值必然连续无洞；顺序错位也不会串表
 *       （.c 里用的是指定初始化器 `[成员] = ...`）。
 * @note 成员名后缀逐字沿用原命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；**"发送方→接收方"是原 `Referee2026Receiver_e`
 *       字段的语义，那个字段已删（运行期零消费者），语义并到这里**（同下同）**。
 */
typedef enum : uint8_t
{
    /* --- 非链路（1 条） --- */
    REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA = 0, //!< 自定义控制器与选手端交互数据，发送方触发发送，频率上限为 30Hz（自定义控制器→己方选手端）
    REFEREE2026_NONE_DATA_COUNT,                  //!< 命令条数（数组维度，非命令）
} Referee2026NoneDataId_e;

/**
 * @brief 非链路命令的元信息表（定义在 referee2026_none.c）
 * @note 字段语义（`data_len` 的字段表口径、频率两列的三种取值、`dir`、`snap_off`）统一写在
 *       public/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_NONE_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报
 *       未定义 —— 这是设计使然：没编进来的链路本就不该被用。
 */
extern const Referee2026CmdInfo_t referee2026_none_cmd_info[REFEREE2026_NONE_DATA_COUNT];

/*============================================
 *   三、接收过滤掩码
 *============================================*/

/**
 * @brief 非链路的接收过滤掩码
 * @note 取值一律 `1u << <数据名>`，故第 i 位 = "收不收 `referee2026_none_cmd_info[i]`"，
 *       与内核 `core->filter` 的位序严格一致（内核在 ISR 里就是拿下标去移位比对的）。
 * @note 四条链路的掩码**统一用 `uint32_t`**，免得内核还要按链路分宽度 —— 代价是每条链路
 *       的命令数上限 32（本协议最多的常规链路 24 条，够用）。
 * @note 本链路**没有可接收的命令**，故这里除 0 之外没有别的成员。四条链路的 `Config` 已统一
 *       成模块根的 `Referee2026Config_s`（`filter` 是裸 `uint32_t`），保留本枚举只是给本链路
 *       留一个具名常量，不再有"凑接口同形"的义务。
 */
typedef enum : uint32_t
{
    REFEREE2026_NONE_FILTER_NONE = 0, //!< 什么都不收（本链路仅此一项合法）
    //! 同 `REFEREE2026_FILTER_DEFAULT` 与 `_FILTER_NONE`：本链路没有任何 RX 命令，
    //! 内核展开时恒得 0，故填这个"全 1"与填 0 是一回事。留着只为四份接口同形。
    REFEREE2026_NONE_FILTER_DEFAULT = 0xFFFFFFFFu,
} Referee2026NoneFilter_e;

/*============================================
 *   四、快照结构体（app 直接读这里）
 *============================================*/

/* 快照是"逐字节镜像"：成员偏移必须等于前面各段长度之和，故显式紧凑化（理由同
 * referee2026_common.h 第六节；本链路只有一个占位字节，这里就只是保持四份同形）。 */
#pragma pack(push, 1)

/**
 * @brief 非链路 RX 快照
 * @note 本链路一条命令都收不到，故**没有成员**；C 不允许空结构体，留一个占位字节。
 *       别的链路里这里是"每条 RX 命令一个成员，顺序与数据名枚举一致"，
 *       每个成员的字节偏移被记在元信息表的 `snap_off` 里（`offsetof` 算出来）。
 * @note 读法见 drv_referee2026.h 的"快照的读法"（tick 三步读，防读到半帧）。
 */
typedef struct
{
    uint8_t unused; //!< 占位：本链路无可接收命令
} Referee2026NoneSnapshot_t;

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
 * @brief 非链路驱动实例
 * @note `core` **必须是第一个成员**（与仓库其余驱动的 vtable 习惯一致；本模块的代码本身不
 *       依赖这一点，回调一律从 `usart->parent` 取回内核 —— 那里存的是 `&inst->core`）。
 * @note 收发接口（Register / Config / Send）都是模块根那份 `Referee2026*`，调用时传
 *       `&inst.core` —— 例如 `Referee2026Config(&none_inst.core, &cfg)`。
 * @note `tick` 在只发不收的链路上用不上，保留它是为了四个链路的实例结构体逐字同形
 *       （`tx_last_us` 仍要用：0x0306 有 30Hz 的速率上限）。
 */
typedef struct
{
    Referee2026Core_t core;                           //!< 收发内核
    Referee2026NoneSnapshot_t snapshot;               //!< RX 快照（本链路为空）
    uint32_t tick[REFEREE2026_NONE_DATA_COUNT];       //!< 每条的最近收到时刻（seqlock 头尾序号）
    uint32_t tx_last_us[REFEREE2026_NONE_DATA_COUNT]; //!< 每条的最近发送时刻（限速用）
} Referee2026None_t;

/**
 * @brief 实例结构体类型与元信息表的**拼接别名** —— 供模块根的 REFEREE2026_INSTANCE_DEF 使用
 * @note 实例定义宏四条链路只写一份（在 `drv_referee2026.h`），按链路名拼出三样东西，
 *       本头提供其中两样（`_DATA_COUNT` 本链路早就有了）：
 *       `REFEREE2026_<LINK>_INSTANCE_TYPE` / `_CMD_INFO` / `_DATA_COUNT`。
 *       故本头**一行收发/实例函数都不定义**，"本链路长什么样"全在这里、"怎么用"全在模块根。
 * @note 为什么要这层别名、而不是让内核宏自己拼：C 预处理器**不能转换大小写** ——
 *       `COMMON` 拼不出结构体名里的 `Common`，也拼不出表名里的小写 `common`。
 * @example
 *   REFEREE2026_INSTANCE_DEF(none_inst, NONE);   // 非链路
 */
#define REFEREE2026_NONE_INSTANCE_TYPE Referee2026None_t
#define REFEREE2026_NONE_CMD_INFO referee2026_none_cmd_info

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __REFEREE2026_NONE_H */
