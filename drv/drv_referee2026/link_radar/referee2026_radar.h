/**
 * @file referee2026_radar.h
 * @brief **雷达无线链路**的完整驱动：6 条命令，全部只收不发
 *
 * 一个链路一个公开头，**用哪条链路就 include 哪个**：
 *   link_common/referee2026_common.h   常规链路 24 条
 *   link_video/referee2026_video.h     图传链路 4 条
 *   link_none/referee2026_none.h       非链路 1 条
 *   link_radar/referee2026_radar.h     雷达无线链路 6 条（本文件）
 * 四条链路**互不依赖**：协议层公共的东西在 public/ 里（帧格式与 ID 见 referee2026_frame.h、
 * 公共数据块与元信息结构见 referee2026_cmd.h），链路无关的收发内核与四条链路**共用的**
 * Register/Config/Send 见模块根的 drv_referee2026.h —— 各链路不再各写一份接口。
 *
 * 本文件按顺序放五样东西：
 *   ① 数据段结构体（+ `REFEREE2026_RADAR_LEN_<TAG>` 长度宏 + `_Static_assert`）
 *   ② 数据名枚举 `Referee2026RadarDataId_e`（取值即元信息表下标）与元信息表声明
 *   ③ 接收过滤掩码 `Referee2026RadarFilter_e`
 *   ④ 快照结构体 `Referee2026RadarSnapshot_t` —— app 直接读它
 *   ⑤ 驱动实例（实例结构体类型 + 两个拼接别名）—— 实例定义宏与收发接口一律在模块根的
 *       drv_referee2026.h，本链路不定义
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）§1.6 雷达无线链路数据说明
 *       （表 1-44 ~ 表 1-49）+ 表 1-5 命令码 ID 一览里"所属数据链路 = 雷达无线链路"的 6 行。
 *
 * @note 协议里的"雷达无线链路"是**服务器 ↔ 雷达站**之间走电磁波、**不占串口**
 *       （故 `Referee2026LinkHasUart(REFEREE2026_LINK_TYPE_RADAR)` 返回 0、
 *       `referee2026_uart_init[]` 的雷达行是全 0 占位）。但从**这块板子**的角度看，数据仍是从
 *       雷达站的接收/计算单元经一根串口送进来的 —— 本驱动因此与另外三条链路同形，
 *       用哪个口由 app 在 `Config` 的 `uart_e` 里给。两者说的不是一回事，别把 uart_init 那一行
 *       的全 0 当成"本驱动不需要串口"。
 * @note 本文件只依赖 <stdint.h>、public/referee2026_frame.h 与 public/referee2026_cmd.h
 *       （后两者同样只依赖 <stdint.h>），协议部分 PC 端可单独语法检查；驱动部分
 *       （第 ⑤ 节）要 HAL，故整节在 `HAL_UART_MODULE_ENABLED` 里。
 */

#ifndef __REFEREE2026_RADAR_H
#define __REFEREE2026_RADAR_H

#include <stdint.h>

#include "referee2026_cmd.h"   /* Referee2026CmdInfo_t / Referee2026BuffItem_t */
#include "referee2026_frame.h" /* REFEREE2026_FRAME_MAX 等帧层常量 */

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（6 条）
 *============================================*/

/**
 * @brief 一个平面坐标点（单位 cm）—— 0x0A01 的组成单元
 * @note 雷达无线链路的所有坐标都用 `uint16_t` 厘米，与常规链路的 `float` 米不同。
 */
typedef struct
{
    uint16_t x; //!< x 轴坐标，单位 cm
    uint16_t y; //!< y 轴坐标，单位 cm
} Referee2026RadarPoint_t;

/* 0x0A01 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS 24

/**
 * @brief 0x0A01 对方机器人位置（表 1-44）—— 对方 6 台机器人的平面坐标
 */
typedef struct
{
    Referee2026RadarPoint_t hero;       //!< 对方英雄机器人位置
    Referee2026RadarPoint_t engineer;   //!< 对方工程机器人位置
    Referee2026RadarPoint_t infantry_3; //!< 对方 3 号步兵机器人位置
    Referee2026RadarPoint_t infantry_4; //!< 对方 4 号步兵机器人位置
    Referee2026RadarPoint_t aerial;     //!< 对方空中机器人位置
    Referee2026RadarPoint_t sentry;     //!< 对方哨兵机器人位置
} Referee2026RadarOpponentPos_t;

/* 0x0A02 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP 12

/**
 * @brief 0x0A02 对方机器人血量（表 1-45）
 * @note 未上场或被罚下的对方机器人血量为 0。
 * @note 偏移 8 是保留位而不是漏了个机器人 —— 对方 6 台里没有 5 号步兵（对方视角的己方同号位
 *       由别的途径给出），字段表就是这么排的。
 */
typedef struct
{
    uint16_t hero_hp;       //!< 偏移 0 对方 1 号英雄机器人血量
    uint16_t engineer_hp;   //!< 偏移 2 对方 2 号工程机器人血量
    uint16_t infantry_3_hp; //!< 偏移 4 对方 3 号步兵机器人血量
    uint16_t infantry_4_hp; //!< 偏移 6 对方 4 号步兵机器人血量
    uint16_t reserved;      //!< 偏移 8 保留位
    uint16_t sentry_hp;     //!< 偏移 10 对方 7 号哨兵机器人血量
} Referee2026RadarOpponentHp_t;

/* 0x0A03 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO 10

/**
 * @brief 0x0A03 对方机器人允许发弹量（表 1-46）
 * @note 3 号步兵的值**含堡垒提供的储备允许发弹量**。
 */
typedef struct
{
    uint16_t hero_allowance;       //!< 偏移 0 对方 1 号英雄机器人允许发弹量
    uint16_t infantry_3_allowance; //!< 偏移 2 对方 3 号步兵机器人允许发弹量（含堡垒储备）
    uint16_t infantry_4_allowance; //!< 偏移 4 对方 4 号步兵机器人允许发弹量
    uint16_t aerial_allowance;     //!< 偏移 6 对方 6 号空中机器人允许发弹量
    uint16_t sentry_allowance;     //!< 偏移 8 对方 7 号哨兵机器人允许发弹量
} Referee2026RadarOpponentAmmo_t;

/**
 * @brief 0x0A04 的 32 位状态字位域视图（表 1-47 偏移 4，4B）
 * @note 占领状态类字段取值：0 未被占领 / 1 被对方占领 / 2 被己方占领 /（堡垒项还有）3 被双方占领；
 *       `*_fly_slope_*` 与 `*_cross_*` 这几位是 1/0 的"是否检测到对方机器人场地交互模块"。
 * @note 写法与 `DBUS_Key_t` 一致：位域给可读性、`value` 给整体赋值/拷贝；`: N` 写错不动字节布局
 *       （布局由 `value` 兜底），但"少写位"查不出来，故末尾显式补 `reserved` 填满 32 位。
 */
typedef union
{
    struct
    {
        uint32_t opponent_supply_zone : 1;          //!< bit 0 对方补给区占领状态
        uint32_t opponent_center_highland : 2;      //!< bit 1-2 对方中央高地占领状态
        uint32_t opponent_trapezoid_highland : 1;   //!< bit 3 对方梯形高地占领状态
        uint32_t opponent_fortress_buff : 2;        //!< bit 4-5 对方堡垒增益点占领状态
        uint32_t opponent_outpost_buff : 2;         //!< bit 6-7 对方前哨站增益点占领状态
        uint32_t opponent_base_buff : 1;            //!< bit 8 对方基地增益点占领状态
        uint32_t opponent_fly_slope_before : 1;     //!< bit 9 靠近对方一侧飞坡前（隧道）检测到对方交互模块
        uint32_t opponent_fly_slope_after : 1;      //!< bit 10 靠近对方一侧飞坡后（隧道）检测到对方交互模块
        uint32_t ally_fly_slope_before : 1;         //!< bit 11 靠近己方一侧飞坡前（隧道）检测到对方交互模块
        uint32_t ally_fly_slope_after : 1;          //!< bit 12 靠近己方一侧飞坡后（隧道）检测到对方交互模块
        uint32_t opponent_cross_highland_upper : 1; //!< bit 13 对方地形跨越（高地）上部检测到对方交互模块
        uint32_t opponent_cross_slope_after : 1;    //!< bit 14 对方地形跨越（飞坡）后部检测到对方交互模块
        uint32_t opponent_cross_road_upper : 1;     //!< bit 15 对方地形跨越（公路）上部检测到对方交互模块
        uint32_t reserved : 16;                     //!< bit 16-31 保留位
    };
    uint32_t value; //!< 32 位原始值
} Referee2026RadarOpponentStateBits_t;

/* 0x0A04 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE 8

/**
 * @brief 0x0A04 对方机器人状态（表 1-47）
 */
typedef struct
{
    uint16_t remaining_gold_coin;              //!< 偏移 0 对方剩余金币数
    uint16_t total_gold_coin;                  //!< 偏移 2 对方累计总金币数
    Referee2026RadarOpponentStateBits_t state; //!< 偏移 4 场地占领/交互模块检测状态字
} Referee2026RadarOpponentState_t;

/* 0x0A05 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF 41

/**
 * @brief 0x0A05 对方机器人增益（表 1-48）
 * @note 前 35B 是 5 个 7B 增益块（英雄/工程/3 号步兵/4 号步兵/哨兵），格式与 0x0204 的
 *       `Referee2026BuffItem_t` **逐字节相同**，故共用 public/referee2026_cmd.h 里那个类型。
 * @note 状态码取值：0 存活 / 1 战亡 / 2 无敌但不虚弱 / 3 无敌且虚弱；异常离线不影响此处的状态判断。
 *       哨兵那一项还多一层姿态，取值见 `sentry_posture`。
 */
typedef struct
{
    Referee2026BuffItem_t buff[5]; //!< 偏移 0 对方 5 台机器人的增益块，依次为英雄、工程、3 号步兵、4 号步兵、哨兵
    uint8_t sentry_posture;        //!< 偏移 35 对方哨兵当前姿态：1 进攻 / 2 防御 / 3 移动 / 4 强化进攻 / 5 强化防御 / 6 强化移动
    uint8_t hero_state;            //!< 偏移 36 对方英雄机器人主要状态
    uint8_t engineer_state;        //!< 偏移 37 对方工程机器人主要状态
    uint8_t infantry_3_state;      //!< 偏移 38 对方 3 号步兵机器人主要状态
    uint8_t infantry_4_state;      //!< 偏移 39 对方 4 号步兵机器人主要状态
    uint8_t sentry_state;          //!< 偏移 40 对方哨兵机器人主要状态
} Referee2026RadarOpponentBuff_t;

/* 0x0A06 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_KEY 6

/**
 * @brief 0x0A06 雷达密钥（表 1-49）
 * @note 6 个字节均为 ASCII 码编码的字母或数字。
 */
typedef struct
{
    uint8_t key[6]; //!< 密钥，ASCII
} Referee2026RadarKey_t;

#pragma pack(pop)

/*============================================
 *   一之二、长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 public/referee2026_cmd.h 的公共约定）。
 * 放在头里而不是 .c 里：这样任何 include 本头的 TU 都会替我们校一遍，断言不会因为
 * "某个开关没开、.c 编成空 TU"而静默消失。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...) _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0A01, sizeof(Referee2026RadarOpponentPos_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS);
REFEREE2026_CMD_STATIC_ASSERT(0x0A02, sizeof(Referee2026RadarOpponentHp_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP);
REFEREE2026_CMD_STATIC_ASSERT(0x0A03, sizeof(Referee2026RadarOpponentAmmo_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO);
REFEREE2026_CMD_STATIC_ASSERT(0x0A04, sizeof(Referee2026RadarOpponentState_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE);
REFEREE2026_CMD_STATIC_ASSERT(0x0A05, sizeof(Referee2026RadarOpponentBuff_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF);
REFEREE2026_CMD_STATIC_ASSERT(0x0A06, sizeof(Referee2026RadarKey_t) == REFEREE2026_RADAR_LEN_RADAR_KEY);

#undef REFEREE2026_CMD_STATIC_ASSERT

/* 位域结构体自身的宽度自检：查"多写了位"（跨出标量宽）。
 * "少写"查不出来，故每个位域结构体末尾都补了 reserved —— 见 public/referee2026_cmd.h。 */
_Static_assert(sizeof(Referee2026RadarOpponentStateBits_t) == sizeof(uint32_t), "0x0A04 位域超出 32 位");

/*============================================
 *   二、数据名与命令元信息表
 *============================================*/

/**
 * @brief 雷达无线链路 6 条命令的数据名
 * @note **取值是顺序的 0..5**，末项 `_COUNT`；取值即 `referee2026_radar_cmd_info[]` 的下标。
 *       只给首项标 `= 0`、其余交给编译器自增，取值必然连续无洞；顺序错位也不会串表
 *       （.c 里用的是指定初始化器 `[成员] = ...`）。
 * @note 成员名后缀逐字沿用原命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；**"发送方→接收方"是原 `Referee2026Receiver_e`
 *       字段的语义，那个字段已删（运行期零消费者），语义并到这里**（同下同）。
 */
typedef enum : uint8_t
{
    /* --- 雷达无线链路（6 条） --- */
    REFEREE2026_RADAR_DATA_RADAR_OPPONENT_POS = 0, //!< 对方机器人的位置坐标，以 10Hz 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_RADAR_OPPONENT_HP,      //!< 对方机器人的血量信息，以 10Hz 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_RADAR_OPPONENT_AMMO,    //!< 对方机器人的剩余发弹量信息，以 10Hz
                                                   //!< 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_RADAR_OPPONENT_STATE,   //!< 对方队伍的宏观状态信息，以 10Hz 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_RADAR_OPPONENT_BUFF,    //!< 对方各机器人当前增益效果，以 10Hz 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_RADAR_KEY,              //!< 对方干扰波密钥，以 10Hz 频率持续发送（雷达站→雷达机器人）
    REFEREE2026_RADAR_DATA_COUNT,                  //!< 命令条数（数组维度，非命令）
} Referee2026RadarDataId_e;

/**
 * @brief 雷达无线链路命令的元信息表（定义在 referee2026_radar.c）
 * @note 字段语义（`data_len` 的字段表口径、频率两列的三种取值、`dir`、`snap_off`）统一写在
 *       public/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_RADAR_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报
 *       未定义 —— 这是设计使然：没编进来的链路本就不该被用。
 */
extern const Referee2026CmdInfo_t referee2026_radar_cmd_info[REFEREE2026_RADAR_DATA_COUNT];

/*============================================
 *   三、接收过滤掩码
 *============================================*/

/**
 * @brief 雷达无线链路的接收过滤掩码
 * @note 取值一律 `1u << <数据名>`，故第 i 位 = "收不收 `referee2026_radar_cmd_info[i]`"，
 *       与内核 `core->filter` 的位序严格一致（内核在 ISR 里就是拿下标去移位比对的）。
 * @note 四条链路的掩码**统一用 `uint32_t`**，免得内核还要按链路分宽度 —— 代价是每条链路
 *       的命令数上限 32（本协议最多的常规链路 24 条，够用）。
 * @note 本链路 6 条**全部**可收、一条都发不出去，故每个数据名都有对应位。
 *       真要"只关心位置和血量"，`Config` 里或上 `_FILTER_RADAR_OPPONENT_POS | _FILTER_RADAR_OPPONENT_HP`
 *       即可 —— 少一次 41B 的 memcpy（0x0A05）在 ISR 里是实打实的省。
 */
typedef enum : uint32_t
{
    REFEREE2026_RADAR_FILTER_NONE = 0, //!< 什么都不收
    //! 本链路全部 6 条 RX 命令 —— `Config` 里填它 = 默认全收（与 `REFEREE2026_FILTER_DEFAULT` 等价）
    REFEREE2026_RADAR_FILTER_DEFAULT = 0xFFFFFFFFu,
    REFEREE2026_RADAR_FILTER_RADAR_OPPONENT_POS = 1u << REFEREE2026_RADAR_DATA_RADAR_OPPONENT_POS,     //!< 收 0x0A01 对方位置
    REFEREE2026_RADAR_FILTER_RADAR_OPPONENT_HP = 1u << REFEREE2026_RADAR_DATA_RADAR_OPPONENT_HP,       //!< 收 0x0A02 对方血量
    REFEREE2026_RADAR_FILTER_RADAR_OPPONENT_AMMO = 1u << REFEREE2026_RADAR_DATA_RADAR_OPPONENT_AMMO,   //!< 收 0x0A03 对方允许发弹量
    REFEREE2026_RADAR_FILTER_RADAR_OPPONENT_STATE = 1u << REFEREE2026_RADAR_DATA_RADAR_OPPONENT_STATE, //!< 收 0x0A04 对方宏观状态
    REFEREE2026_RADAR_FILTER_RADAR_OPPONENT_BUFF = 1u << REFEREE2026_RADAR_DATA_RADAR_OPPONENT_BUFF,   //!< 收 0x0A05 对方增益
    REFEREE2026_RADAR_FILTER_RADAR_KEY = 1u << REFEREE2026_RADAR_DATA_RADAR_KEY,                       //!< 收 0x0A06 干扰波密钥
} Referee2026RadarFilter_e;

/*============================================
 *   四、快照结构体（app 直接读这里）
 *============================================*/

/* 快照是"逐字节镜像"：成员偏移必须等于前面各段长度之和，故显式紧凑化。此处不 pack 其实也
 * 恰好是对的（叶子类型都声明在 `#pragma pack(1)` 里 ⇒ align 1），但那是间接事实 —— 哪天加一个
 * 未包的类型就会被悄悄插进填充、后面所有 `snap_off` 一起错位。显式写下来，理由同
 * referee2026_common.h 第六节。 */
#pragma pack(push, 1)

/**
 * @brief 雷达无线链路 RX 快照
 * @note 本链路 6 条命令全都能收，故 6 个成员齐备，顺序与数据名枚举一致。
 *       每个成员的字节偏移被记在元信息表的 `snap_off` 里（`.c` 里用 `offsetof` 算出来），
 *       内核 memcpy 的目标就是 `(uint8_t *)&snapshot + snap_off`。
 * @note 读法见 drv_referee2026.h 的"快照的读法"（tick 三步读，防读到半帧）：
 *       0x0A05 一帧 41B，memcpy 不是原子的。
 */
typedef struct
{
    Referee2026RadarOpponentPos_t radar_opponent_pos;     //!< 0x0A01 对方位置
    Referee2026RadarOpponentHp_t radar_opponent_hp;       //!< 0x0A02 对方血量
    Referee2026RadarOpponentAmmo_t radar_opponent_ammo;   //!< 0x0A03 对方允许发弹量
    Referee2026RadarOpponentState_t radar_opponent_state; //!< 0x0A04 对方宏观状态
    Referee2026RadarOpponentBuff_t radar_opponent_buff;   //!< 0x0A05 对方增益
    Referee2026RadarKey_t radar_key;                      //!< 0x0A06 干扰波密钥
} Referee2026RadarSnapshot_t;

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
 * @brief 雷达无线链路驱动实例
 * @note `core` **必须是第一个成员**（与仓库其余驱动的 vtable 习惯一致；本模块的代码本身不
 *       依赖这一点，回调一律从 `usart->parent` 取回内核 —— 那里存的是 `&inst->core`）。
 * @note 收发接口（Register / Config / Send）都是模块根那份 `Referee2026*`，调用时传
 *       `&inst.core` —— 例如 `Referee2026Config(&radar_inst.core, &cfg)`。
 * @note `tx_last_us` 在只收不发的链路上用不上（内核发不出去任何一帧），保留它是为了四个链路的
 *       实例结构体逐字同形。
 */
typedef struct
{
    Referee2026Core_t core;                            //!< 收发内核
    Referee2026RadarSnapshot_t snapshot;               //!< RX 快照
    uint32_t tick[REFEREE2026_RADAR_DATA_COUNT];       //!< 每条的最近收到时刻（seqlock 头尾序号）
    uint32_t tx_last_us[REFEREE2026_RADAR_DATA_COUNT]; //!< 每条的最近发送时刻（本链路恒 0）
} Referee2026Radar_t;

/**
 * @brief 实例结构体类型与元信息表的**拼接别名** —— 供模块根的 REFEREE2026_INSTANCE_DEF 使用
 * @note 实例定义宏四条链路只写一份（在 `drv_referee2026.h`），按链路名拼出三样东西，
 *       本头提供其中两样（`_DATA_COUNT` 本链路早就有了）：
 *       `REFEREE2026_<LINK>_INSTANCE_TYPE` / `_CMD_INFO` / `_DATA_COUNT`。
 *       故本头**一行收发/实例函数都不定义**，"本链路长什么样"全在这里、"怎么用"全在模块根。
 * @note 为什么要这层别名、而不是让内核宏自己拼：C 预处理器**不能转换大小写** ——
 *       `COMMON` 拼不出结构体名里的 `Common`，也拼不出表名里的小写 `common`。
 * @example
 *   REFEREE2026_INSTANCE_DEF(radar_inst, RADAR);   // 雷达无线链路
 */
#define REFEREE2026_RADAR_INSTANCE_TYPE Referee2026Radar_t
#define REFEREE2026_RADAR_CMD_INFO referee2026_radar_cmd_info

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __REFEREE2026_RADAR_H */
