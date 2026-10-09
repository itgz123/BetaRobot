/**
 * @file referee2026_common.h
 * @brief **常规链路**的完整驱动：24 条命令（21 条有 RX / 3 条只发，其中 0x0301 收发双向）
 *
 * 一个链路一个公开头，**用哪条链路就 include 哪个**：
 *   link_common/referee2026_common.h   常规链路 24 条（本文件）
 *   link_video/referee2026_video.h     图传链路 4 条
 *   link_none/referee2026_none.h       非链路 1 条
 *   link_radar/referee2026_radar.h     雷达无线链路 6 条
 * 四条链路**互不依赖**：协议层公共的东西在 public/ 里（帧格式与 ID 见 referee2026_frame.h、
 * 公共数据块与元信息结构见 referee2026_cmd.h），链路无关的收发内核与四条链路**共用的**
 * Register/Config/Send 见模块根的 drv_referee2026.h —— 各链路不再各写一份接口。
 *
 * 本文件按顺序放七样东西：
 *   ① 命令数据段（24 条结构体 + `REFEREE2026_COMMON_LEN_<TAG>` 长度宏）
 *   ② 0x0301 命令的 8 种子内容（UI 图形 / 哨兵·雷达自主决策指令）
 *   ③ 长度自校验（数据段 + 子内容 + 位域宽度）
 *   ④ 数据名枚举 `Referee2026CommonDataId_e`（取值即元信息表下标）与元信息表声明
 *   ⑤ 接收过滤掩码 `Referee2026CommonFilter_e`
 *   ⑥ 快照结构体 `Referee2026CommonSnapshot_t` —— app 直接读它
 *   ⑦ 驱动实例（实例结构体类型 + 两个拼接别名）—— 实例定义宏与收发接口一律在模块根的
 *       drv_referee2026.h，本链路不定义
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）§1.2 常规链路数据说明
 *       （表 1-6 ~ 表 1-38）+ 表 1-5 命令码 ID 一览里"所属数据链路 = 常规链路"的 24 行；
 *       组内保持表 1-5 顺序。
 *
 * @note 本链路是四条里最长的一条（24/35 条命令、最长数据段 118B），元信息表 24 × 12 = 288B
 *       （真被引用时才进 `.rodata` —— 现在没有 app 落实例，它连同快照被 `--gc-sections` 丢掉）。
 * @note 公共约定（packed / 小端 / 位域 union 写法 / 文档笔误口径）见 public/referee2026_cmd.h ——
 *       那里放着跨链路的公共组成块，本文件 include 它。
 * @note 每条命令的**数据段长度**以 `REFEREE2026_COMMON_LEN_<TAG>` 宏为准（紧挨对应结构体上方，值取
 *       官方"字节偏移量字段表"的合计），文件末尾的长度断言与 referee2026_common.c 的元信息表
 *       **共用同一个宏** —— 一处定义、两处引用，改长度只改宏。
 * @note 本文件只依赖 <stdint.h>、public/referee2026_frame.h 与 public/referee2026_cmd.h
 *       （后两者同样只依赖 <stdint.h>），协议部分 PC 端可单独语法检查；驱动部分
 *       （第 ⑦ 节）要 HAL，故整节在 `HAL_UART_MODULE_ENABLED` 里。
 */

#ifndef __REFEREE2026_COMMON_H
#define __REFEREE2026_COMMON_H

#include <stdint.h>

#include "referee2026_cmd.h"   /* Referee2026CmdInfo_t / Referee2026BuffItem_t */
#include "referee2026_frame.h" /* REFEREE2026_FRAME_MAX 等帧层常量 */

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（24 条）
 *============================================*/

/*--------------------------------------------
 * 0x0001 比赛状态数据（11B）
 *--------------------------------------------*/

/* 0x0001 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_GAME_STATUS 11

/**
 * @brief 0x0001 比赛状态数据（表 1-6）
 * @note `game_type` / `game_progress` 是同一字节的高低半字节。
 * @note `game_type` 取值：1 超级对抗赛 / 2 高校单项赛 / 3 ICRA 高校人工智能挑战赛 /
 *       4 联盟赛 3V3 对抗 / 5 联盟赛步兵对抗。
 * @note `game_progress` 取值：0 未开始 / 1 准备阶段 / 2 十五秒自检 / 3 五秒倒计时 /
 *       4 比赛中 / 5 比赛结算中。
 */
typedef struct
{
    uint8_t game_type : 4;      //!< bit 0-3 比赛类型，取值见本结构体上方 @note
    uint8_t game_progress : 4;  //!< bit 4-7 当前比赛阶段，取值见本结构体上方 @note
    uint16_t stage_remain_time; //!< 当前阶段剩余时间，单位 s
    uint64_t sync_time_stamp;   //!< UNIX 时间戳，机器人连上裁判系统 NTP 服务器后生效
} Referee2026GameStatus_t;

/*--------------------------------------------
 * 0x0002 比赛结果数据（1B）
 *--------------------------------------------*/

/* 0x0002 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_GAME_RESULT 1

/**
 * @brief 0x0002 比赛结果数据（表 1-7）
 */
typedef struct
{
    uint8_t winner; //!< 0 平局 / 1 红方胜利 / 2 蓝方胜利
} Referee2026GameResult_t;

/*--------------------------------------------
 * 0x0003 机器人血量数据（20B）
 *--------------------------------------------*/

/* 0x0003 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_GAME_ROBOT_HP 20

/**
 * @brief 0x0003 机器人血量数据（表 1-8）
 * @note 未上场或被罚下的机器人血量为 0。`ally_2/3/4` 是工程与 3、4 号步兵，
 *       **没有 `ally_5/6`**：全表按 1、2、3、4、7 号机器人编号排列。
 */
typedef struct
{
    uint16_t ally_1_robot_hp;  //!< 己方 1 号英雄机器人血量
    uint16_t ally_2_robot_hp;  //!< 己方 2 号工程机器人血量
    uint16_t ally_3_robot_hp;  //!< 己方 3 号步兵机器人血量
    uint16_t ally_4_robot_hp;  //!< 己方 4 号步兵机器人血量
    int16_t damage_difference; //!< 己方全队总伤害与对方全队总伤害之差（有符号）
    uint16_t ally_7_robot_hp;  //!< 己方 7 号哨兵机器人血量
    uint16_t ally_outpost_hp;  //!< 己方前哨站血量
    uint16_t ally_base_hp;     //!< 己方基地血量
    uint16_t enemy_outpost_hp; //!< 对方前哨站血量
    uint16_t enemy_base_hp;    //!< 对方基地血量
} Referee2026GameRobotHp_t;

/*--------------------------------------------
 * 0x0101 场地事件数据（4B）
 *--------------------------------------------*/

/**
 * @brief 0x0101 场地事件数据的位域视图（表 1-9）
 * @note 占领状态类的两位字段取值：0 未被占领 / 1 被己方占领 / 2 被对方占领
 *       （`ally_*_buff` 那几项只用到 1，见各自注释）。
 */
typedef union
{
    struct
    {
        uint32_t supply_zone_occupied : 1;      //!< bit 0 己方补给区占领状态，1 为已占领
        uint32_t reserved0 : 1;                 //!< bit 1 保留位
        uint32_t supply_zone_occupied_rmul : 1; //!< bit 2 己方补给区占领状态（仅 RMUL）
        uint32_t small_energy_rune : 2;         //!< bit 3-4 己方小能量机关：0 未激活 / 1 已激活 / 2 正在激活
        uint32_t big_energy_rune : 2;           //!< bit 5-6 己方大能量机关，取值同上
        uint32_t ally_center_highland : 2;      //!< bit 7-8 己方中央高地占领状态
        uint32_t ally_trapezoid_highland : 2;   //!< bit 9-10 己方梯形高地占领状态
        uint32_t opponent_dart_hit_time : 9;    //!< bit 11-19 对方飞镖最后一次击中己方前哨站或基地的时间，单位 s
        uint32_t opponent_dart_hit_target : 3;  //!< bit 20-22 对方飞镖最后一次击中的具体目标
        uint32_t center_buff_zone : 2;          //!< bit 23-24 中心增益点占领状态
        uint32_t ally_fortress_buff : 2;        //!< bit 25-26 己方堡垒增益点占领状态
        uint32_t ally_outpost_buff : 2;         //!< bit 27-28 己方前哨站增益点占领状态
        uint32_t ally_base_buff : 1;            //!< bit 29 己方基地增益点占领状态，1 为已占领
        uint32_t reserved1 : 2;                 //!< bit 30-31 保留位
    };
    uint32_t value; //!< 32 位原始值
} Referee2026EventDataBits_t;

/* 0x0101 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_EVENT_DATA 4

/**
 * @brief 0x0101 场地事件数据（表 1-9）—— 数据段就是这一个 32 位字
 */
typedef Referee2026EventDataBits_t Referee2026EventData_t;

/*--------------------------------------------
 * 0x0104 裁判警告数据（3B）
 *--------------------------------------------*/

/* 0x0104 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_REFEREE_WARNING 3

/**
 * @brief 0x0104 裁判警告数据（表 1-10）
 */
typedef struct
{
    uint8_t level;              //!< 己方最后一次受判罚的等级：1 双方黄牌 / 2 黄牌 / 3 红牌 / 4 判负
    uint8_t offending_robot_id; //!< 违规机器人 ID（如红 1 为 1，蓝 1 为 101）；判负和双方黄牌时为 0
    uint8_t count;              //!< 该判罚等级下的违规次数，开局为 0
} Referee2026RefereeWarning_t;

/*--------------------------------------------
 * 0x0105 飞镖发射相关数据（3B）
 *--------------------------------------------*/

/**
 * @brief 0x0105 的 `dart_info` 位域视图（表 1-11 偏移 1，2B）
 * @note `recent_hit_target` 取值：0 无 / 1 前哨站 / 2 基地固定目标 / 3 基地随机固定目标 /
 *       4 基地随机移动目标 / 5 基地末端移动目标。
 */
typedef union
{
    struct
    {
        uint16_t recent_hit_target : 3;  //!< bit 0-2 最近一次己方飞镖击中的目标，取值见本 union 上方 @note
        uint16_t opponent_hit_count : 3; //!< bit 3-5 对方最近被击中的目标累计被击中次数，至多 4
        uint16_t selected_target : 3;    //!< bit 6-8 飞镖此时选定的击打目标
        uint16_t reserved : 7;           //!< bit 9-15 保留位
    };
    uint16_t value; //!< 16 位原始值
} Referee2026DartInfoBits_t;

/* 0x0105 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_DART_LAUNCH 3

/**
 * @brief 0x0105 飞镖发射相关数据（表 1-11）
 */
typedef struct
{
    uint8_t dart_remaining_time;         //!< 己方飞镖发射剩余时间，单位 s
    Referee2026DartInfoBits_t dart_info; //!< 飞镖击中/选定信息（见该 union 的位定义）
} Referee2026DartLaunch_t;

/*--------------------------------------------
 * 0x0201 机器人性能体系数据（17B）
 *--------------------------------------------*/

/* 0x0201 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_ROBOT_STATUS 17

/**
 * @brief 0x0201 机器人性能体系数据（表 1-12）
 * @note 末字节只有低 3 位有效，其余位**文档未定义、读出即未定义值**，
 *       不要拿 `memcmp` 比较整个结构体。
 */
typedef struct
{
    uint8_t robot_id;                            //!< 本机器人 ID
    uint8_t robot_level;                         //!< 机器人等级
    uint16_t current_hp;                         //!< 当前血量
    uint16_t maximum_hp;                         //!< 血量上限
    uint16_t shooter_barrel_cooling_value;       //!< 射击热量每秒冷却值
    uint16_t shooter_barrel_heat_limit;          //!< 射击热量上限
    uint16_t chassis_power_limit;                //!< 底盘功率上限
    float bullet_speed_limit;                    //!< 射击初速度上限
    uint8_t power_management_gimbal_output : 1;  //!< bit 0 gimbal 口输出，0 无输出 / 1 为 24V 输出
    uint8_t power_management_chassis_output : 1; //!< bit 1 chassis 口输出，同上
    uint8_t power_management_shooter_output : 1; //!< bit 2 shooter 口输出，同上
} Referee2026RobotStatus_t;

/*--------------------------------------------
 * 0x0202 实时底盘缓冲能量和射击热量数据（14B）
 *--------------------------------------------*/

/* 0x0202 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_POWER_HEAT 14

/**
 * @brief 0x0202 实时底盘缓冲能量和射击热量数据（表 1-13）
 *
 * @warning **官方给出的结构体是错的**：它写成 `uint16_t reserved; float reserved;`（共 6B），
 *          于是 `buffer_energy` 落在偏移 6 —— 但同一张表的字节偏移量列明写"保留位 0/2/4"、
 *          "缓冲能量 8"、"17mm 热量 10"、"42mm 热量 12"，且总表 data_length = 14。
 *          只有"2B + 2B + 4B 三段保留"才能让缓冲能量正好落在偏移 8、总长正好 14。
 *          本结构体按**字段表**修正为 3 段保留，并与 data_length 14 互相印证。
 */
typedef struct
{
    uint16_t reserved0;                //!< 偏移 0 保留位
    uint16_t reserved1;                //!< 偏移 2 保留位
    float reserved2;                   //!< 偏移 4 保留位（这段是 4B，故文档结构体多写了一个 2B 的坑）
    uint16_t buffer_energy;            //!< 偏移 8 缓冲能量，单位 J
    uint16_t shooter_17mm_barrel_heat; //!< 偏移 10 17mm 发射机构的射击热量
    uint16_t shooter_42mm_barrel_heat; //!< 偏移 12 42mm 发射机构的射击热量
} Referee2026PowerHeat_t;

/*--------------------------------------------
 * 0x0203 机器人位置数据（12B）
 *--------------------------------------------*/

/* 0x0203 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_ROBOT_POS 12

/**
 * @brief 0x0203 机器人位置数据（表 1-14）
 *
 * @warning **总表与字段表不一致的 3 条之一**：总表 data_length = 16，字段表只列到偏移 8
 *          的 4B 朝向（0/4/8 三个 float，共 12B），官方结构体也是 12B。
 *          本文件按**字段表**取 12。
 * @note 解析侧请按"字段表为最小可接受长度、总表为包络"处理（见 frame.h 的同名说明）：
 *       拿数据段指针强转本结构体是安全的（只读前 12B），若对方真发了 16B，多出的 4B 忽略即可。
 */
typedef struct
{
    float x;     //!< 本机器人位置 x 坐标，单位 m
    float y;     //!< 本机器人位置 y 坐标，单位 m
    float angle; //!< 本机器人测速模块的朝向，单位 度，正北为 0
} Referee2026RobotPos_t;

/*--------------------------------------------
 * 0x0204 机器人增益和底盘能量数据（8B）
 *--------------------------------------------*/

/**
 * @brief 0x0204 的 `remaining_energy` 位域视图（表 1-15 偏移 7，1B）
 * @note 以机器人初始能量为 100%，各位是"是否达到某档"的阈值标志，**不是**能量值本身。
 */
typedef union
{
    struct
    {
        uint8_t above_125_percent : 1; //!< bit 0 剩余能量 ≥125%
        uint8_t above_100_percent : 1; //!< bit 1 剩余能量 ≥100%
        uint8_t above_50_percent : 1;  //!< bit 2 剩余能量 ≥50%
        uint8_t above_30_percent : 1;  //!< bit 3 剩余能量 ≥30%
        uint8_t above_15_percent : 1;  //!< bit 4 剩余能量 ≥15%
        uint8_t above_5_percent : 1;   //!< bit 5 剩余能量 ≥5%
        uint8_t above_1_percent : 1;   //!< bit 6 剩余能量 ≥1%
        uint8_t reserved : 1;          //!< bit 7 保留位
    };
    uint8_t value; //!< 8 位原始值
} Referee2026RemainingEnergyBits_t;

/* 0x0204 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_BUFF 8

/**
 * @brief 0x0204 机器人增益和底盘能量数据（表 1-15）
 */
typedef struct
{
    Referee2026BuffItem_t buff_item;                   //!< 偏移 0-6 五项增益
    Referee2026RemainingEnergyBits_t remaining_energy; //!< 偏移 7 剩余能量档位标志
} Referee2026Buff_t;

/*--------------------------------------------
 * 0x0206 伤害状态数据（1B）
 *--------------------------------------------*/

/* 0x0206 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_ROBOT_HURT 1

/**
 * @brief 0x0206 伤害状态数据（表 1-16）—— 数据段就是这一个字节
 * @note 本地判定、即时发送，是否真受伤害以服务器最终判定为准。
 */
typedef union
{
    struct
    {
        uint8_t armor_id : 4;            //!< bit 0-3 扣血来源：装甲/测速模块的 ID 编号；其他原因扣血时为 0
        uint8_t hp_deduction_reason : 4; //!< bit 4-7 血量变化类型：0 被弹丸攻击 / 1 模块离线 / 5 受撞击
    };
    uint8_t value; //!< 8 位原始值
} Referee2026HurtData_t;

/*--------------------------------------------
 * 0x0207 实时射击数据（7B）
 *--------------------------------------------*/

/* 0x0207 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_SHOOT_DATA 7

/**
 * @brief 0x0207 实时射击数据（表 1-17）
 * @note `bullet_type` 与 `shooter_number` 官方声明为整字节，但语义是"位标志"：
 *       弹丸类型 bit1 = 17mm、bit2 = 42mm；发射机构 1 = 17mm、2 保留、3 = 42mm。
 *       本结构体保留官方声明（整字节），按位解释请在使用处自行 `& 0x02` 判读。
 */
typedef struct
{
    uint8_t bullet_type;         //!< 弹丸类型：bit 1 = 17mm，bit 2 = 42mm
    uint8_t shooter_number;      //!< 发射机构 ID：1 = 17mm，2 保留，3 = 42mm
    uint8_t launching_frequency; //!< 射频，单位 发/s
    float initial_speed;         //!< 弹丸初速度，单位 m/s
} Referee2026ShootData_t;

/*--------------------------------------------
 * 0x0208 允许发弹量与剩余金币数（8B）
 *--------------------------------------------*/

/* 0x0208 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW 8

/**
 * @brief 0x0208 允许发弹量与剩余金币数（表 1-18）
 */
typedef struct
{
    uint16_t projectile_allowance_17mm;     //!< 自身拥有的 17mm 弹丸允许发弹量
    uint16_t projectile_allowance_42mm;     //!< 自身拥有的 42mm 弹丸允许发弹量
    uint16_t remaining_gold_coin;           //!< 剩余金币数量
    uint16_t projectile_allowance_fortress; //!< 堡垒增益点提供的储备 17mm 允许发弹量（与实际是否占领堡垒无关）
} Referee2026ProjectileAllowance_t;

/*--------------------------------------------
 * 0x0209 机器人 RFID 模块状态（5B）
 *--------------------------------------------*/

/**
 * @brief 0x0209 的 `rfid_status` 位域视图（表 1-19 偏移 0，4B，共 32 个增益点）
 * @note 每位 1/0 表示"是否已检测到该增益点的 RFID 卡"。
 *       飞坡/中央高地/公路/隧道这几组按"己方侧 / 对方侧"成对出现，命名统一为
 *       `ally_*` / `enemy_*`；"下方/中间/上方"指在设施上的高度位置。
 */
typedef union
{
    struct
    {
        uint32_t ally_base : 1;                   //!< bit 0 己方基地增益点
        uint32_t ally_center_highland : 1;        //!< bit 1 己方中央高地增益点
        uint32_t enemy_center_highland : 1;       //!< bit 2 对方中央高地增益点
        uint32_t ally_trapezoid_highland : 1;     //!< bit 3 己方梯形高地增益点
        uint32_t enemy_trapezoid_highland : 1;    //!< bit 4 对方梯形高地增益点
        uint32_t ally_fly_slope_before : 1;       //!< bit 5 地形跨越（飞坡）靠近己方一侧·飞坡前
        uint32_t ally_fly_slope_after : 1;        //!< bit 6 地形跨越（飞坡）靠近己方一侧·飞坡后
        uint32_t enemy_fly_slope_before : 1;      //!< bit 7 地形跨越（飞坡）靠近对方一侧·飞坡前
        uint32_t enemy_fly_slope_after : 1;       //!< bit 8 地形跨越（飞坡）靠近对方一侧·飞坡后
        uint32_t ally_center_lower : 1;           //!< bit 9 地形跨越（中央高地）己方侧·下方
        uint32_t ally_center_upper : 1;           //!< bit 10 地形跨越（中央高地）己方侧·上方
        uint32_t enemy_center_lower : 1;          //!< bit 11 地形跨越（中央高地）对方侧·下方
        uint32_t enemy_center_upper : 1;          //!< bit 12 地形跨越（中央高地）对方侧·上方
        uint32_t ally_road_lower : 1;             //!< bit 13 地形跨越（公路）己方侧·下方
        uint32_t ally_road_upper : 1;             //!< bit 14 地形跨越（公路）己方侧·上方
        uint32_t enemy_road_lower : 1;            //!< bit 15 地形跨越（公路）对方侧·下方
        uint32_t enemy_road_upper : 1;            //!< bit 16 地形跨越（公路）对方侧·上方
        uint32_t ally_fortress : 1;               //!< bit 17 己方堡垒增益点
        uint32_t ally_outpost : 1;                //!< bit 18 己方前哨站增益点
        uint32_t ally_supply_zone_exclusive : 1;  //!< bit 19 己方与资源区不重叠的补给区 / RMUL 补给区
        uint32_t ally_supply_zone_overlap : 1;    //!< bit 20 己方与资源区重叠的补给区
        uint32_t ally_assembly : 1;               //!< bit 21 己方装配增益点
        uint32_t enemy_assembly : 1;              //!< bit 22 对方装配增益点
        uint32_t center_supply_zone : 1;          //!< bit 23 中心增益点（仅 RMUL 适用）
        uint32_t enemy_fortress : 1;              //!< bit 24 对方堡垒增益点
        uint32_t enemy_outpost : 1;               //!< bit 25 对方前哨站增益点
        uint32_t ally_tunnel_road_lower : 1;      //!< bit 26 地形跨越（隧道）己方侧公路区·下方
        uint32_t ally_tunnel_road_middle : 1;     //!< bit 27 地形跨越（隧道）己方侧公路区·中间
        uint32_t ally_tunnel_road_upper : 1;      //!< bit 28 地形跨越（隧道）己方侧公路区·上方
        uint32_t ally_tunnel_highland_low : 1;    //!< bit 29 地形跨越（隧道）己方侧梯形高地·较低处
        uint32_t ally_tunnel_highland_middle : 1; //!< bit 30 地形跨越（隧道）己方侧梯形高地·较中间
        uint32_t ally_tunnel_highland_high : 1;   //!< bit 31 地形跨越（隧道）己方侧梯形高地·较高处
    };
    uint32_t value; //!< 32 位原始值
} Referee2026RfidStatusBits_t;

/**
 * @brief 0x0209 的 `rfid_status_2` 位域视图（表 1-19 偏移 4，1B，对方侧隧道 6 个位置）
 */
typedef union
{
    struct
    {
        uint8_t enemy_tunnel_road_lower : 1;      //!< bit 0 隧道 对方侧公路区·下方
        uint8_t enemy_tunnel_road_middle : 1;     //!< bit 1 隧道 对方侧公路区·中间
        uint8_t enemy_tunnel_road_upper : 1;      //!< bit 2 隧道 对方侧公路区·上方
        uint8_t enemy_tunnel_highland_low : 1;    //!< bit 3 隧道 对方侧梯形高地·较低处
        uint8_t enemy_tunnel_highland_middle : 1; //!< bit 4 隧道 对方侧梯形高地·较中间
        uint8_t enemy_tunnel_highland_high : 1;   //!< bit 5 隧道 对方侧梯形高地·较高处
        uint8_t reserved : 2;                     //!< bit 6-7 保留位
    };
    uint8_t value; //!< 8 位原始值
} Referee2026RfidStatus2Bits_t;

/* 0x0209 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_RFID_STATUS 5

/**
 * @brief 0x0209 机器人 RFID 模块状态（表 1-19）
 */
typedef struct
{
    Referee2026RfidStatusBits_t rfid_status;    //!< 偏移 0 己方/对方各增益点（32 位）
    Referee2026RfidStatus2Bits_t rfid_status_2; //!< 偏移 4 对方侧隧道 6 个位置
} Referee2026RfidStatus_t;

/*--------------------------------------------
 * 0x020A 飞镖选手端指令数据（6B）
 *--------------------------------------------*/

/* 0x020A 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_DART_CLIENT_CMD 6

/**
 * @brief 0x020A 飞镖选手端指令数据（表 1-20）
 */
typedef struct
{
    uint8_t dart_launch_opening_status; //!< 当前飞镖发射站状态：0 已开启 / 1 关闭 / 2 正在开启或关闭中
    uint8_t reserved;                   //!< 偏移 1 保留位
    uint16_t target_change_time;        //!< 切换击打目标时的比赛剩余时间，单位 s；无/未切换为 0
    uint16_t latest_launch_cmd_time;    //!< 最后一次操作手确定发射指令时的比赛剩余时间，单位 s
} Referee2026DartClientCmd_t;

/*--------------------------------------------
 * 0x020B 地面机器人位置数据（40B）
 *--------------------------------------------*/

/* 0x020B 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS 40

/**
 * @brief 0x020B 地面机器人位置数据（表 1-21），仅发给己方哨兵
 *
 * @warning **官方给出的结构体是错的**：只写了一个 `float reserved`（共 36B），
 *          但字段表列到偏移 32 与 36 **两处** 4B 保留位，总表 data_length = 40。
 *          本结构体按字段表补成 `reserved[2]`。
 *
 * @note 坐标系：场地围挡在红方补给站附近的交点为原点，沿场地长边向蓝方为 X 正方向，
 *       沿场地短边向红方停机坪为 Y 正方向。
 */
typedef struct
{
    float hero_x;       //!< 己方英雄 x，单位 m
    float hero_y;       //!< 己方英雄 y，单位 m
    float engineer_x;   //!< 己方工程 x，单位 m
    float engineer_y;   //!< 己方工程 y，单位 m
    float standard_3_x; //!< 己方 3 号步兵 x，单位 m
    float standard_3_y; //!< 己方 3 号步兵 y，单位 m
    float standard_4_x; //!< 己方 4 号步兵 x，单位 m
    float standard_4_y; //!< 己方 4 号步兵 y，单位 m
    float reserved[2];  //!< 偏移 32 / 36 保留位
} Referee2026GroundRobotPosition_t;

/*--------------------------------------------
 * 0x020C 雷达标记进度数据（2B）
 *--------------------------------------------*/

/* 0x020C 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_RADAR_MARK 2

/**
 * @brief 0x020C 雷达标记进度数据（表 1-22）—— 数据段就是这一个 16 位字
 * @note 对方机器人：被标记进度 ≥100 时送 1，<100 送 0；
 *       己方机器人：被标记进度 ≥50 时送 1，<50 送 0。
 */
typedef union
{
    struct
    {
        uint16_t enemy_hero : 1;             //!< bit 0 对方 1 号英雄易伤情况
        uint16_t enemy_engineer : 1;         //!< bit 1 对方 2 号工程易伤情况
        uint16_t enemy_infantry_3 : 1;       //!< bit 2 对方 3 号步兵易伤情况
        uint16_t enemy_infantry_4 : 1;       //!< bit 3 对方 4 号步兵易伤情况
        uint16_t enemy_aerial : 1;           //!< bit 4 对方空中特殊标识情况
        uint16_t enemy_sentry : 1;           //!< bit 5 对方哨兵易伤情况
        uint16_t ally_hero : 1;              //!< bit 6 己方 1 号英雄特殊标识情况
        uint16_t ally_engineer : 1;          //!< bit 7 己方 2 号工程特殊标识情况
        uint16_t ally_infantry_3 : 1;        //!< bit 8 己方 3 号步兵特殊标识情况
        uint16_t ally_infantry_4 : 1;        //!< bit 9 己方 4 号步兵特殊标识情况
        uint16_t ally_aerial : 1;            //!< bit 10 己方空中特殊标识情况
        uint16_t ally_sentry : 1;            //!< bit 11 己方哨兵特殊标识情况
        uint16_t enemy_aerial_lasered : 1;   //!< bit 12 对方空中是否被己方雷达激光瞄准
        uint16_t enemy_aerial_countered : 1; //!< bit 13 对方空中当前是否处于被反制状态
        uint16_t ally_aerial_lasered : 1;    //!< bit 14 己方空中是否被对方雷达激光瞄准
        uint16_t ally_aerial_countered : 1;  //!< bit 15 己方空中是否处于被反制状态
    };
    uint16_t value; //!< 16 位原始值
} Referee2026RadarMarkData_t;

/*--------------------------------------------
 * 0x020D 哨兵自主决策信息同步（14B）
 *--------------------------------------------*/

/**
 * @brief 0x020D 的 `sentry_info` 位域视图（表 1-23 偏移 0，4B）
 */
typedef union
{
    struct
    {
        uint32_t exchanged_ammo : 11;            //!< bit 0-10 除远程兑换外，成功兑换的允许发弹量
        uint32_t remote_ammo_exchange_count : 4; //!< bit 11-14 成功远程兑换允许发弹量的次数
        uint32_t remote_hp_exchange_count : 4;   //!< bit 15-18 成功远程兑换血量的次数
        uint32_t free_revive_available : 1;      //!< bit 19 当前是否可确认免费复活
        uint32_t instant_revive_available : 1;   //!< bit 20 当前是否可兑换立即复活
        uint32_t instant_revive_cost : 10;       //!< bit 21-30 当前兑换立即复活需花费的金币数
        uint32_t reserved : 1;                   //!< bit 31 保留位
    };
    uint32_t value; //!< 32 位原始值
} Referee2026SentryInfoBits_t;

/**
 * @brief 0x020D 的 `sentry_info_2` 位域视图（表 1-23 偏移 4，2B）
 */
typedef union
{
    struct
    {
        uint16_t out_of_combat : 1;      //!< bit 0 哨兵当前是否处于脱战状态
        uint16_t exchangeable_ammo : 11; //!< bit 1-11 队伍 17mm 允许发弹量的剩余可兑换数
        uint16_t posture : 2;            //!< bit 12-13 哨兵当前姿态：1 进攻 / 2 防御 / 3 移动
        uint16_t rune_activatable : 1;   //!< bit 14 己方能量机关是否能够进入正在激活状态
        uint16_t reinforced_posture : 1; //!< bit 15 哨兵当前姿态是否为强化姿态
    };
    uint16_t value; //!< 16 位原始值
} Referee2026SentryInfo2Bits_t;

/**
 * @brief 0x020D 的 `sentry_info_3`（表 1-23 偏移 6，8B）
 * @note 8 个 8 位字段，每个是"该姿态弱化前剩余可持续时长"，单位 s；24-31 与 56-63 为保留位。
 *       这里是**整齐的字节对齐**，故直接用 8 个 `uint8_t` 而非位域。
 */
typedef struct
{
    uint8_t attack_duration;             //!< bit 0-7 进攻姿态弱化前剩余可持续时长，单位 s
    uint8_t defence_duration;            //!< bit 8-15 防御姿态弱化前剩余可持续时长
    uint8_t move_duration;               //!< bit 16-23 移动姿态弱化前剩余可持续时长
    uint8_t reserved0;                   //!< bit 24-31 保留位
    uint8_t reinforced_attack_duration;  //!< bit 32-39 强化进攻姿态剩余可持续时长
    uint8_t reinforced_defence_duration; //!< bit 40-47 强化防御姿态剩余可持续时长
    uint8_t reinforced_move_duration;    //!< bit 48-55 强化移动姿态剩余可持续时长
    uint8_t reserved1;                   //!< bit 56-63 保留位
} Referee2026SentryInfo3_t;

/* 0x020D 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_SENTRY_INFO 14

/**
 * @brief 0x020D 哨兵自主决策信息同步（表 1-23）
 */
typedef struct
{
    Referee2026SentryInfoBits_t sentry_info;    //!< 偏移 0 兑换/复活相关
    Referee2026SentryInfo2Bits_t sentry_info_2; //!< 偏移 4 脱战/姿态/能量机关
    Referee2026SentryInfo3_t sentry_info_3;     //!< 偏移 6 各姿态剩余时长
} Referee2026SentryInfo_t;

/*--------------------------------------------
 * 0x020E 雷达自主决策信息同步（1B）
 *--------------------------------------------*/

/* 0x020E 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_RADAR_INFO 1

/**
 * @brief 0x020E 雷达自主决策信息同步（表 1-24）—— 数据段就是这一个字节
 */
typedef union
{
    struct
    {
        uint8_t double_vulnerability_chance : 2; //!< bit 0-1 雷达拥有触发双倍易伤的机会数，至多 2
        uint8_t double_vulnerability_active : 1; //!< bit 2 对方是否正在被触发双倍易伤
        uint8_t encryption_level : 2;            //!< bit 3-4 己方加密等级（对方干扰波难度），开局 1，最高 3
        uint8_t key_modifiable : 1;              //!< bit 5 当前是否可以修改密钥
        uint8_t reserved : 2;                    //!< bit 6-7 保留位
    };
    uint8_t value; //!< 8 位原始值
} Referee2026RadarInfo_t;

/*--------------------------------------------
 * 0x0301 机器人交互数据（118B）
 *--------------------------------------------*/

/* 0x0301 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_ROBOT_INTERACTION 118

/**
 * @brief 0x0301 机器人交互数据（表 1-25）—— 统一数据段头 + 最多 112B 的子内容
 *
 * @note **本命令是变长的**：`sizeof` 是最大形态（118B），真机的帧头 `data_length` 在
 *       `min_len`(6) 与 `data_len`(118) 之间，`user_data` 的实际长度 = `data_length - 6`。
 *       故本结构体只当"读法说明书 + 快照成员的尺寸"，别拿 `sizeof` 当"这帧有多长"。
 * @note 子内容按 `data_cmd_id` 解释，见第五节；`user_data` 里放的是**已对齐到偏移 6**
 *       的子内容结构体（即子内容自己的偏移 0 从这里开始）。
 * @note 带宽：每 1000ms 英雄/工程/步兵/空中/飞镖可收 3720B，雷达与哨兵可收 5120B；
 *       本 cmd_id 上行频率上限 30Hz。
 * @note 本命令**双向**（全协议唯一）：既由本机发给服务器/选手端，也接收队友机器人的
 *       `0x0200~0x02FF`（"机器人之间通信"）。方向的唯一出处仍是元信息表的 `dir`。
 */
typedef struct
{
    uint16_t data_cmd_id;   //!< 子内容 ID，见 Referee2026InteractionCmdId_e
    uint16_t sender_id;     //!< 发送者 ID，需与自身 ID 匹配（附录二）
    uint16_t receiver_id;   //!< 接收者 ID：仅限己方通信；若为选手端，除雷达外只可发给发送者对应的选手端
    uint8_t user_data[112]; //!< 内容数据段，长度由帧头 data_length 减 6 得出（最长 112）
} Referee2026RobotInteraction_t;

/*--------------------------------------------
 * 0x0303 选手端小地图交互数据（12B）
 *--------------------------------------------*/

/* 0x0303 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT 12

/**
 * @brief 0x0303 选手端小地图交互数据（表 1-35，文档名 `map_command_t`）
 *
 * @warning **总表与字段表不一致的 3 条之一**：总表 data_length = 15，字段表只列到偏移 10
 *          的 2B `cmd_source`（0/4/8/9/10，共 12B），官方结构体也是 12B。本文件按**字段表**取 12。
 */
typedef struct
{
    float target_position_x; //!< 目标位置 x 轴坐标，单位 m；发送目标机器人 ID 时该值为 0
    float target_position_y; //!< 目标位置 y 轴坐标，单位 m；发送目标机器人 ID 时该值为 0
    uint8_t cmd_keyboard;    //!< 云台手按下的键盘按键通用键值，无按键为 0
    uint8_t target_robot_id; //!< 对方机器人 ID；发送坐标数据时该值为 0
    uint16_t cmd_source;     //!< 信息来源 ID（附录二）
} Referee2026MiniMapCommand_t;

/*--------------------------------------------
 * 0x0305 小地图接收雷达数据（48B）
 *--------------------------------------------*/

/* 0x0305 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_MINI_MAP_RADAR 48

/**
 * @brief 0x0305 小地图接收雷达数据（表 1-36，文档名 `map_robot_data_t`）
 * @note 24 个 `uint16_t`，单位 cm，先对方 6 台（英雄/工程/3 号步兵/4 号步兵/空中/哨兵）、
 *       再己方同样 6 台，每台按 x、y 排列。
 * @note 当 x、y 超出边界时显示在对应边缘处；**x、y 均为 0 视为未发送此机器人坐标**。
 */
typedef struct
{
    uint16_t opponent[6][2]; //!< 对方：英雄、工程、3 号步兵、4 号步兵、空中、哨兵（各 x,y），单位 cm
    uint16_t ally[6][2];     //!< 己方：同上顺序，单位 cm
} Referee2026MiniMapRadar_t;

/*--------------------------------------------
 * 0x0307 小地图路径（105B）
 *--------------------------------------------*/

/* 0x0307 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_MINI_MAP_PATH 105

/**
 * @brief 0x0307 小地图路径（表 1-37，文档名 `map_data_t`）
 *
 * @warning **总表与字段表不一致的 3 条之一**，且是**唯一"总表更小"**的一条：
 *          总表 data_length = 103，而字段表列到偏移 103 的 2B `sender_id`
 *          （1+2+2+49+49+2 = 105），官方结构体也是 105。本文件按**字段表**取 105。
 * @note 坐标系：小地图左下角为原点，水平向右为 X 正方向，竖直向上为 Y 正方向；
 *       显示位置按场地尺寸与小地图尺寸等比缩放，超出边界者显示在边界处。
 */
typedef struct
{
    uint8_t intention;         //!< 1 到目标点攻击 / 2 到目标点防守 / 3 移动到目标点
    uint16_t start_position_x; //!< 路径起点 x 轴坐标，单位 dm
    uint16_t start_position_y; //!< 路径起点 y 轴坐标，单位 dm
    int8_t delta_x[49];        //!< 路径点 x 轴增量数组，单位 dm（相对上一个点位，共 49 个新点位）
    int8_t delta_y[49];        //!< 路径点 y 轴增量数组，单位 dm（与 delta_x 逐项配对成点位）
    uint16_t sender_id;        //!< 发送者 ID，需与自身 ID 匹配（附录二）
} Referee2026MiniMapPath_t;

/*--------------------------------------------
 * 0x0308 自定义消息下发到选手端（34B）
 *--------------------------------------------*/

/* 0x0308 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT 34

/**
 * @brief 0x0308 自定义消息下发到选手端（表 1-38，文档名 `custom_info_t`）
 * @note `user_data` 以 **UTF-16** 编码发送，支持中文；编码时注意数据的大小端问题。
 */
typedef struct
{
    uint16_t sender_id;    //!< 发送者 ID，需校验正确性
    uint16_t receiver_id;  //!< 接收者 ID，需校验正确性；仅支持发送己方选手端
    uint8_t user_data[30]; //!< 字符内容，UTF-16 编码
} Referee2026CustomInfo_t;

/*============================================
 *   二、0x0301 命令的子内容（8 种具名 + 1 段自定区间）
 *============================================*/

/**
 * @brief 0x0301 的 `data_cmd_id` 取值（表 1-26 ~ 表 1-34）
 * @note 只列**文档给了固定结构体**的子内容 ID；表里另行开放的 `0x0200~0x02FF`
 *       （"机器人之间通信"，`user_data` ≤ 112B，内容由各队自定）见下面的
 *       {@link REFEREE2026_INTERACTION_CMD_ROBOT_TO_ROBOT_MIN}，本模块不解释其内容，
 *       原样收进快照由 app 自己解。**不认识的值收到即忽略**（照常写快照，不报错）。
 * @note 与图传链路的 0xA9 自定义客户端协议是两套东西：这里是"机器人 ↔ 机器人/选手端"，
 *       0xA9 那套（GameStatus、MapClickCmd 等）走图传，不在本协议范围内。
 */
typedef enum : uint16_t
{
    REFEREE2026_INTERACTION_CMD_LAYER_DELETE = 0x0100, //!< 删除图层
    REFEREE2026_INTERACTION_CMD_FIGURE_1 = 0x0101,     //!< 绘制一个图形
    REFEREE2026_INTERACTION_CMD_FIGURE_2 = 0x0102,     //!< 绘制两个图形
    REFEREE2026_INTERACTION_CMD_FIGURE_5 = 0x0103,     //!< 绘制五个图形
    REFEREE2026_INTERACTION_CMD_FIGURE_7 = 0x0104,     //!< 绘制七个图形
    //! 自定义客户端绘制字符（表 1-32）。**常规链路的子内容**（发给本机器人对应的选手端），
    //! 与图传链路无关 —— 别去图传链路找它。
    REFEREE2026_INTERACTION_CMD_CLIENT_CHARACTER = 0x0110,
    REFEREE2026_INTERACTION_CMD_SENTRY_DECISION = 0x0120, //!< 哨兵自主决策指令（发往服务器 0x8080）
    REFEREE2026_INTERACTION_CMD_RADAR_DECISION = 0x0121,  //!< 雷达自主决策指令（发往服务器 0x8080）
} Referee2026InteractionCmdId_e;

/**
 * @brief `0x0200~0x02FF`：机器人之间通信的子内容 ID **区间**（表 1-25 里开放的整段）
 * @note 这一段协议**没有**给固定结构体：`user_data` ≤ 112B、内容由各队自定，故不能进上面那个
 *       枚举（枚举只能列有名字的常量）。本模块不解释它，收到就原样写进快照的
 *       `robot_interaction.user_data`，由 app 按 `data_cmd_id` 自己分发。
 * @note 判据写成 `cmd >= MIN && cmd <= MAX`；区间是**闭区间**（含两端）。
 */
#define REFEREE2026_INTERACTION_CMD_ROBOT_TO_ROBOT_MIN 0x0200u
#define REFEREE2026_INTERACTION_CMD_ROBOT_TO_ROBOT_MAX 0x02FFu

/**
 * @brief 子内容 0x0100 删除图层（表 1-26，文档名 `interaction_layer_delete_t`）
 */
typedef struct
{
    uint8_t delete_type; //!< 删除操作：1 删除单个图层 / 2 删除全部图层
    uint8_t layer;       //!< 图层号，范围 0-9
} Referee2026InteractionLayerDelete_t;

/**
 * @brief 子内容 0x0101 ~ 0x0104 的图形单元（表 1-27，文档名 `interaction_figure_t`）
 *
 * @note 12 个位域正好排成 3 个 32 位字，字段表里的偏移就是按这个切的：
 *       字 0 = `operate_type`…`details_b`（3+3+4+4+9+9 = 32），
 *       字 1 = `width`/`start_x`/`start_y`（10+11+11 = 32），
 *       字 2 = `details_c`/`details_d`/`details_e`（10+11+11 = 32）。
 * @note 官方字段名把 `operate_type` / `figure_type` **拼成了 `_tpye`**，此处已改正；
 *       位域名只影响可读性（布局由 `word` 兜底），但拼错会让人搜不到，故一并修掉。
 * @note `operate_type` 取值：0 空操作 / 1 增加 / 2 修改 / 3 删除。
 * @note `figure_type` 取值：0 直线 / 1 矩形 / 2 正圆 / 3 椭圆 / 4 圆弧 / 5 浮点数 / 6 整型数 / 7 字符。
 * @note `color` 取值：0 红/蓝（己方颜色）/ 1 黄色 / 2 绿色 / 3 橙色 / 4 紫红色 / 5 粉色 /
 *       6 青色 / 7 黑色 / 8 白色。
 * @note `details_a`~`details_e` 的含义随 `figure_type` 而变（直线/矩形/正圆/椭圆/圆弧/浮点数/整型数/字符
 *       各不相同），解释规则见协议 §1.3.3"图形数据介绍"与"表 1-28 图形细节参数说明"，本文件不复述。
 * @note `width` 是线宽（绘制字符时即字号），官方建议字号与线宽比例为 10:1。
 */
typedef struct
{
    uint8_t figure_name[3]; //!< 图形名，UTF-16 编码的 3 字节（不足则补 0）
    union
    {
        struct
        {
            uint32_t operate_type : 3; //!< 图形操作，取值见本结构体上方 @note
            uint32_t figure_type : 3;  //!< 图形类型，取值见本结构体上方 @note
            uint32_t layer : 4;        //!< 图层号，范围 0-9
            uint32_t color : 4;        //!< 颜色，取值见本结构体上方 @note
            uint32_t details_a : 9;    //!< 含义随 figure_type，见协议"图形数据介绍"
            uint32_t details_b : 9;    //!< 含义随 figure_type
            uint32_t width : 10;       //!< 线宽（绘制字符时为字号），单位 像素；0 表示不显示
            uint32_t start_x : 11;     //!< 起始点 x 坐标，单位 像素
            uint32_t start_y : 11;     //!< 起始点 y 坐标，单位 像素
            uint32_t details_c : 10;   //!< 含义随 figure_type
            uint32_t details_d : 11;   //!< 含义随 figure_type
            uint32_t details_e : 11;   //!< 含义随 figure_type（浮点/整数字符串类别时存放实际值）
        };
        uint32_t word[3]; //!< 3 个 32 位原始字（位域跨字，故用数组而非单个标量）
    };
} Referee2026InteractionFigure_t;

/**
 * @brief 子内容 0x0102 绘制两个图形（表 1-29，文档名 `interaction_figure_2_t`）
 */
typedef struct
{
    Referee2026InteractionFigure_t figure[2]; //!< 2 个图形单元
} Referee2026InteractionFigure2_t;

/**
 * @brief 子内容 0x0103 绘制五个图形（表 1-30，文档名 `interaction_figure_3_t`）
 */
typedef struct
{
    Referee2026InteractionFigure_t figure[5]; //!< 5 个图形单元
} Referee2026InteractionFigure3_t;

/**
 * @brief 子内容 0x0104 绘制七个图形（表 1-31，文档名 `interaction_figure_4_t`）
 */
typedef struct
{
    Referee2026InteractionFigure_t figure[7]; //!< 7 个图形单元
} Referee2026InteractionFigure4_t;

/**
 * @brief 子内容 0x0110 自定义客户端绘制字符（表 1-32，文档名 `ext_client_custom_character_t`）
 * @note 官方字段名把 `graphic_data_struct` 拼成了 `grapic_data_struct`，此处已改正。
 * @note `data` 是实际要显示的字符，编码由 `graphic_data_struct.color` 之后的 details 字段决定。
 */
typedef struct
{
    Referee2026InteractionFigure_t graphic_data_struct; //!< 图形配置，详见"图形数据介绍"
    uint8_t data[30];                                   //!< 字符内容
} Referee2026ExtClientCustomCharacter_t;

/**
 * @brief 子内容 0x0120 哨兵自主决策指令（表 1-33）—— 数据段就是这一个 32 位字
 *
 * @note 发往服务器（选手端 ID `0x8080`），频率上限 30Hz。
 * @note 服务器**按从低位到高位的顺序依次处理**，直到全部成功或无法继续为止（如金币不足）；
 *       被拒绝后其后的位一律不再处理，等下一组指令。
 * @note `exchange_ammo`、`remote_ammo_request_count`、`remote_hp_request_count` 三项
 *       **必须单调递增**（后者每次只能 +1），否则该指令视为不合法。
 * @note 修改的是"请求次数/目标值"，不是"扣多少钱"——满足规则时服务器才实际消耗金币。
 * @note `posture` 取值：1 进攻 / 2 防御 / 3 移动 / 4 强化进攻 / 5 强化防御 / 6 强化移动，默认 3。
 */
typedef union
{
    struct
    {
        uint32_t confirm_revive : 1;            //!< bit 0 是否确认复活（即使读条已完成，为 0 也不复活）
        uint32_t confirm_instant_revive : 1;    //!< bit 1 是否确认兑换立即复活
        uint32_t exchange_ammo : 11;            //!< bit 2-12 将要兑换的发弹量值，开局为 0，需单调递增
        uint32_t remote_ammo_request_count : 4; //!< bit 13-16 远程兑换发弹量的请求次数，每次只能 +1
        uint32_t remote_hp_request_count : 4;   //!< bit 17-20 远程兑换血量的请求次数，每次只能 +1
        uint32_t posture : 3;                   //!< bit 21-23 姿态，取值见本 union 上方 @note
        uint32_t confirm_rune_activation : 1;   //!< bit 24 是否确认使能量机关进入正在激活状态，默认 0
        uint32_t reserved : 7;                  //!< bit 25-31 保留位
    };
    uint32_t value; //!< 32 位原始值
} Referee2026SentryCmd_t;

/**
 * @brief 子内容 0x0121 雷达自主决策指令（表 1-34，文档名 `radar_cmd_t`）
 *
 * @note `double_vulnerability_request` **单调递增、每次只能 +1**；请求时若双倍易伤正在生效，
 *       第二次将在第一次结束后生效。
 * @note `password_cmd` = 1 更新己方加密密钥，= 2 把破解到的对方密钥交给服务器验证。
 *       仅开局、以及每次对方破解成功导致己方加密等级提高时才可修改，其余时间无效；
 *       `password_cmd` = 2 时，每次验证后 10s 内再次更新无效。
 */
typedef struct
{
    uint8_t double_vulnerability_request; //!< 偏移 0 触发双倍易伤的请求次数，开局 0，需单调递增
    uint8_t password_cmd;                 //!< 偏移 1 指令类型：1 更新己方密钥 / 2 验证破解到的对方密钥
    uint8_t password[6];                  //!< 偏移 2-7 密钥值，每字节为 ASCII 字母或数字
} Referee2026RadarCmd_t;

#pragma pack(pop)

/*============================================
 *   三、长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上。
 * 位域名写错不会动布局（布局由 union 的标量成员兜底），但**漏写字段、漏加/错加数组长度、
 * 或者哪天有人"顺手"给 packed 结构体加了对齐属性**，都会实打实地改 sizeof —— 这几条断言
 * 就是拿来在编译期把这类事故拦下来的。
 * 三条"总表 data_length 与字段表对不上"的命令按**字段表**断言（理由见各结构体的 @warning）：
 *   0x0203 sizeof 12 而总表 16（官方结构体也是 12，总表多算 4B）
 *   0x0303 sizeof 12 而总表 15（官方结构体也是 12，总表多算 3B）
 *   0x0307 sizeof 105 而总表 103（三条里唯一"总表更小"，字段表逐项相加为 105）
 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...) _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0001, sizeof(Referee2026GameStatus_t) == REFEREE2026_COMMON_LEN_GAME_STATUS);
REFEREE2026_CMD_STATIC_ASSERT(0x0002, sizeof(Referee2026GameResult_t) == REFEREE2026_COMMON_LEN_GAME_RESULT);
REFEREE2026_CMD_STATIC_ASSERT(0x0003, sizeof(Referee2026GameRobotHp_t) == REFEREE2026_COMMON_LEN_GAME_ROBOT_HP);
REFEREE2026_CMD_STATIC_ASSERT(0x0101, sizeof(Referee2026EventData_t) == REFEREE2026_COMMON_LEN_EVENT_DATA);
REFEREE2026_CMD_STATIC_ASSERT(0x0104, sizeof(Referee2026RefereeWarning_t) == REFEREE2026_COMMON_LEN_REFEREE_WARNING);
REFEREE2026_CMD_STATIC_ASSERT(0x0105, sizeof(Referee2026DartLaunch_t) == REFEREE2026_COMMON_LEN_DART_LAUNCH);
REFEREE2026_CMD_STATIC_ASSERT(0x0201, sizeof(Referee2026RobotStatus_t) == REFEREE2026_COMMON_LEN_ROBOT_STATUS);
REFEREE2026_CMD_STATIC_ASSERT(0x0202, sizeof(Referee2026PowerHeat_t) == REFEREE2026_COMMON_LEN_POWER_HEAT);
REFEREE2026_CMD_STATIC_ASSERT(0x0203, sizeof(Referee2026RobotPos_t) == REFEREE2026_COMMON_LEN_ROBOT_POS); // 总表 16，见其 @warning
REFEREE2026_CMD_STATIC_ASSERT(0x0204, sizeof(Referee2026Buff_t) == REFEREE2026_COMMON_LEN_BUFF);
REFEREE2026_CMD_STATIC_ASSERT(0x0206, sizeof(Referee2026HurtData_t) == REFEREE2026_COMMON_LEN_ROBOT_HURT);
REFEREE2026_CMD_STATIC_ASSERT(0x0207, sizeof(Referee2026ShootData_t) == REFEREE2026_COMMON_LEN_SHOOT_DATA);
REFEREE2026_CMD_STATIC_ASSERT(0x0208, sizeof(Referee2026ProjectileAllowance_t) == REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW);
REFEREE2026_CMD_STATIC_ASSERT(0x0209, sizeof(Referee2026RfidStatus_t) == REFEREE2026_COMMON_LEN_RFID_STATUS);
REFEREE2026_CMD_STATIC_ASSERT(0x020A, sizeof(Referee2026DartClientCmd_t) == REFEREE2026_COMMON_LEN_DART_CLIENT_CMD);
REFEREE2026_CMD_STATIC_ASSERT(0x020B, sizeof(Referee2026GroundRobotPosition_t) == REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS);
REFEREE2026_CMD_STATIC_ASSERT(0x020C, sizeof(Referee2026RadarMarkData_t) == REFEREE2026_COMMON_LEN_RADAR_MARK);
REFEREE2026_CMD_STATIC_ASSERT(0x020D, sizeof(Referee2026SentryInfo_t) == REFEREE2026_COMMON_LEN_SENTRY_INFO);
REFEREE2026_CMD_STATIC_ASSERT(0x020E, sizeof(Referee2026RadarInfo_t) == REFEREE2026_COMMON_LEN_RADAR_INFO);
REFEREE2026_CMD_STATIC_ASSERT(0x0301, sizeof(Referee2026RobotInteraction_t) == REFEREE2026_COMMON_LEN_ROBOT_INTERACTION);
REFEREE2026_CMD_STATIC_ASSERT(0x0303, sizeof(Referee2026MiniMapCommand_t) == REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT); // 总表 15，见其 @warning
REFEREE2026_CMD_STATIC_ASSERT(0x0305, sizeof(Referee2026MiniMapRadar_t) == REFEREE2026_COMMON_LEN_MINI_MAP_RADAR);
REFEREE2026_CMD_STATIC_ASSERT(0x0307, sizeof(Referee2026MiniMapPath_t) == REFEREE2026_COMMON_LEN_MINI_MAP_PATH); // 总表 103，见其 @warning
REFEREE2026_CMD_STATIC_ASSERT(0x0308, sizeof(Referee2026CustomInfo_t) == REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT);

#undef REFEREE2026_CMD_STATIC_ASSERT

/* 0x0301 子内容（8 种） */
_Static_assert(sizeof(Referee2026InteractionLayerDelete_t) == 2, "子内容 0x0100 长度不符");
_Static_assert(sizeof(Referee2026InteractionFigure_t) == 15, "子内容 0x0101 长度不符");
_Static_assert(sizeof(Referee2026InteractionFigure2_t) == 30, "子内容 0x0102 长度不符");
_Static_assert(sizeof(Referee2026InteractionFigure3_t) == 75, "子内容 0x0103 长度不符");
_Static_assert(sizeof(Referee2026InteractionFigure4_t) == 105, "子内容 0x0104 长度不符");
_Static_assert(sizeof(Referee2026ExtClientCustomCharacter_t) == 45, "子内容 0x0110 长度不符");
_Static_assert(sizeof(Referee2026SentryCmd_t) == 4, "子内容 0x0120 长度不符");
_Static_assert(sizeof(Referee2026RadarCmd_t) == 8, "子内容 0x0121 长度不符");

/* 位域结构体自身的宽度自检：查"多写了位"（跨出标量宽）。
 * "少写"查不出来，故每个位域结构体末尾都补了 reserved —— 见 public/referee2026_cmd.h。 */
_Static_assert(sizeof(Referee2026EventDataBits_t) == sizeof(uint32_t), "0x0101 位域超出 32 位");
_Static_assert(sizeof(Referee2026DartInfoBits_t) == sizeof(uint16_t), "0x0105 位域超出 16 位");
_Static_assert(sizeof(Referee2026RemainingEnergyBits_t) == sizeof(uint8_t), "0x0204 位域超出 8 位");
_Static_assert(sizeof(Referee2026HurtData_t) == sizeof(uint8_t), "0x0206 位域超出 8 位");
_Static_assert(sizeof(Referee2026RfidStatusBits_t) == sizeof(uint32_t), "0x0209 位域超出 32 位");
_Static_assert(sizeof(Referee2026RfidStatus2Bits_t) == sizeof(uint8_t), "0x0209_2 位域超出 8 位");
_Static_assert(sizeof(Referee2026RadarMarkData_t) == sizeof(uint16_t), "0x020C 位域超出 16 位");
_Static_assert(sizeof(Referee2026SentryInfoBits_t) == sizeof(uint32_t), "0x020D 位域超出 32 位");
_Static_assert(sizeof(Referee2026SentryInfo2Bits_t) == sizeof(uint16_t), "0x020D_2 位域超出 16 位");
_Static_assert(sizeof(Referee2026RadarInfo_t) == sizeof(uint8_t), "0x020E 位域超出 8 位");
_Static_assert(sizeof(Referee2026SentryCmd_t) == sizeof(uint32_t), "子内容 0x0120 位域超出 32 位");
_Static_assert(sizeof(Referee2026InteractionFigure_t) == 3 + 3 * sizeof(uint32_t), "子内容 0x0101 位域超出 96 位");

/*============================================
 *   四、数据名与命令元信息表
 *============================================*/

/**
 * @brief 常规链路 24 条命令的数据名
 * @note **取值是顺序的 0..23**，末项 `_COUNT`；取值即 `referee2026_common_cmd_info[]` 的下标。
 *       只给首项标 `= 0`、其余交给编译器自增，取值必然连续无洞；顺序错位也不会串表
 *       （.c 里用的是指定初始化器 `[成员] = ...`）。
 * @note 成员名后缀逐字沿用原命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；括号里的 **"发送方→接收方"是原 `Referee2026Receiver_e`
 *       字段的语义**，那个字段已删（运行期零消费者），语义并到这里。左端只写角色不写具体 ID
 *       （服务器 / 选手端 / 雷达 / 哨兵 / 机器人），右端同理；表 1-5 的"接收方"若写作 `-`
 *       （0x0301），则由发送方在报文内 `receiver_id` 里填，此处如实标注。
 * @note 右端角色与已删的 {@link Referee2026Receiver_e} 位掩码逐一对应，不再是位掩码而是文字。
 */
typedef enum : uint8_t
{
    /* --- 常规链路（24 条） --- */
    REFEREE2026_COMMON_DATA_GAME_STATUS = 0, //!< 比赛状态数据，固定以 1Hz 频率发送（服务器→全体机器人）
    REFEREE2026_COMMON_DATA_GAME_RESULT,     //!< 比赛结果数据，比赛结束触发发送（服务器→全体机器人）
    REFEREE2026_COMMON_DATA_GAME_ROBOT_HP,   //!< 机器人血量数据，固定以 3Hz 频率发送（服务器→全体机器人）
    REFEREE2026_COMMON_DATA_EVENT_DATA,      //!< 场地事件数据，固定以 1Hz 频率发送（服务器→己方全体机器人）
    REFEREE2026_COMMON_DATA_REFEREE_WARNING, //!< 裁判警告数据，己方判罚/判负时触发发送，其余时间以 1Hz
                                             //!< 频率发送（服务器→被判罚方全体机器人）
    REFEREE2026_COMMON_DATA_DART_LAUNCH,     //!< 飞镖发射相关数据，固定以 1Hz 频率发送（服务器→己方全体机器人）
    REFEREE2026_COMMON_DATA_ROBOT_STATUS,    //!< 机器人性能体系数据，固定以 10Hz 频率发送（服务器→对应机器人）
    REFEREE2026_COMMON_DATA_POWER_HEAT,      //!< 实时底盘缓冲能量和射击热量数据，固定以 10Hz 频率发送（服务器→对应机器人）
    /* 0x0203：总表 16B / 字段表 12B，见"三、长度自校验"里的说明 */
    REFEREE2026_COMMON_DATA_ROBOT_POS,         //!< 机器人位置数据，固定以 1Hz 频率发送（服务器→对应机器人）
    REFEREE2026_COMMON_DATA_BUFF,              //!< 机器人增益和底盘能量数据，固定以 3Hz 频率发送（服务器→对应机器人）
    REFEREE2026_COMMON_DATA_ROBOT_HURT,        //!< 伤害状态数据，伤害发生后发送（服务器→对应机器人）
    REFEREE2026_COMMON_DATA_SHOOT_DATA,        //!< 实时射击数据，弹丸发射后发送（服务器→对应机器人）
    REFEREE2026_COMMON_DATA_PROJECTILE_ALLOW,  //!< 允许发弹量与剩余金币数，固定以 10Hz
                                               //!< 频率发送（服务器→己方英雄/步兵/哨兵/空中）
    REFEREE2026_COMMON_DATA_RFID_STATUS,       //!< 机器人 RFID 模块状态，固定以 3Hz 频率发送（服务器→己方装有 RFID
                                               //!< 模块的机器人）
    REFEREE2026_COMMON_DATA_DART_CLIENT_CMD,   //!< 飞镖选手端指令数据，固定以 3Hz 频率发送（服务器→己方飞镖）
    REFEREE2026_COMMON_DATA_GROUND_ROBOT_POS,  //!< 地面机器人位置数据，固定以 1Hz
                                               //!< 频率发送，仅发给己方哨兵（服务器→己方哨兵）
    REFEREE2026_COMMON_DATA_RADAR_MARK,        //!< 雷达标记进度数据，固定以 1Hz 频率发送（服务器→己方雷达）
    REFEREE2026_COMMON_DATA_SENTRY_INFO,       //!< 哨兵自主决策信息同步，固定以 1Hz 频率发送（服务器→己方哨兵）
    REFEREE2026_COMMON_DATA_RADAR_INFO,        //!< 雷达自主决策信息同步，固定以 1Hz 频率发送（服务器→己方雷达）
    REFEREE2026_COMMON_DATA_ROBOT_INTERACTION, //!< 机器人交互数据，发送方触发发送，频率上限为 30Hz
                                               //!< （机器人↔机器人/选手端，接收方由报文内 receiver_id 指定；
                                               //!< **全协议唯一双向的一条**，本链路唯一变长的数据段）
    /* 0x0303：总表 15B / 字段表 12B，见"三、长度自校验"里的说明 */
    REFEREE2026_COMMON_DATA_MINI_MAP_INTERACT, //!< 选手端小地图交互数据，选手端触发发送（选手端→对应机器人）
    REFEREE2026_COMMON_DATA_MINI_MAP_RADAR,    //!< 选手端小地图接收雷达数据，频率上限为 5Hz（雷达→己方所有选手端）
    /* 0x0307：总表 103B / 字段表 105B —— 三条不一致里唯一"总表更小"的一条 */
    REFEREE2026_COMMON_DATA_MINI_MAP_PATH,  //!< 选手端小地图接收路径数据，频率上限为
                                            //!< 1Hz（哨兵/半自动机器人→对应操作手的选手端）
    REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, //!< 选手端小地图接收机器人数据，频率上限为 3Hz（机器人→己方所有选手端）
    REFEREE2026_COMMON_DATA_COUNT,          //!< 命令条数（数组维度，非命令）
} Referee2026CommonDataId_e;

/**
 * @brief 常规链路命令的元信息表（定义在 referee2026_common.c）
 * @note 字段语义（`data_len` 的字段表口径、频率两列的三种取值、`dir`、`snap_off`）统一写在
 *       public/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。
 * @note 24 条里**4 条带 TX 位**：0x0305 雷达→选手端、0x0307 哨兵→选手端、0x0308 机器人→选手端
 *       这 3 条只发不收，另加 **0x0301 机器人交互（双向）**；其余 20 条本框架只收不发 —— 往裁判
 *       系统总线上打一帧对方不收的命令是实打实的干扰，`dir` 就是拦这个的。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_COMMON_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报
 *       未定义 —— 这是设计使然：没编进来的链路本就不该被用。
 */
extern const Referee2026CmdInfo_t referee2026_common_cmd_info[REFEREE2026_COMMON_DATA_COUNT];

/*============================================
 *   五、接收过滤掩码
 *============================================*/

/**
 * @brief 常规链路的接收过滤掩码
 * @note 取值一律 `1u << <数据名>`，故第 i 位 = "收不收 `referee2026_common_cmd_info[i]`"，
 *       与内核 `core->filter` 的位序严格一致（内核在 ISR 里就是拿下标去移位比对的）。
 * @note 四条链路的掩码**统一用 `uint32_t`**，免得内核还要按链路分宽度 —— 代价是每条链路的
 *       命令数上限 32（本链路 24 条，够用）。
 * @note **只给收得到的命令设位**：0x0305 / 0x0307 / 0x0308 只发不收（`dir` 里没有 RX），
 *       给了位也没意义 —— 内核派发时会先看方向、再看掩码。故这里 21 位、3 个空洞。
 * @note 真要"只关心血量和热量"，`Config` 里或上 `_FILTER_GAME_ROBOT_HP | _FILTER_POWER_HEAT`
 *       即可 —— 少一次 118B 的 memcpy（0x0301）在 ISR 里是实打实的省。
 */
typedef enum : uint32_t
{
    REFEREE2026_COMMON_FILTER_NONE = 0, //!< 什么都不收
    //! 本链路全部 21 条 RX 命令 —— `Config` 里填它 = 默认全收（与 `REFEREE2026_FILTER_DEFAULT` 等价）
    REFEREE2026_COMMON_FILTER_DEFAULT = 0xFFFFFFFFu,
    REFEREE2026_COMMON_FILTER_GAME_STATUS = 1u << REFEREE2026_COMMON_DATA_GAME_STATUS, //!< 收 0x0001 比赛状态
    REFEREE2026_COMMON_FILTER_GAME_RESULT = 1u << REFEREE2026_COMMON_DATA_GAME_RESULT, //!< 收 0x0002 比赛结果
    //! 收 0x0003 机器人血量
    REFEREE2026_COMMON_FILTER_GAME_ROBOT_HP = 1u << REFEREE2026_COMMON_DATA_GAME_ROBOT_HP,
    REFEREE2026_COMMON_FILTER_EVENT_DATA = 1u << REFEREE2026_COMMON_DATA_EVENT_DATA, //!< 收 0x0101 场地事件
    //! 收 0x0104 裁判警告
    REFEREE2026_COMMON_FILTER_REFEREE_WARNING = 1u << REFEREE2026_COMMON_DATA_REFEREE_WARNING,
    REFEREE2026_COMMON_FILTER_DART_LAUNCH = 1u << REFEREE2026_COMMON_DATA_DART_LAUNCH, //!< 收 0x0105 飞镖发射
    //! 收 0x0201 机器人性能体系
    REFEREE2026_COMMON_FILTER_ROBOT_STATUS = 1u << REFEREE2026_COMMON_DATA_ROBOT_STATUS,
    //! 收 0x0202 缓冲能量与射击热量
    REFEREE2026_COMMON_FILTER_POWER_HEAT = 1u << REFEREE2026_COMMON_DATA_POWER_HEAT,
    REFEREE2026_COMMON_FILTER_ROBOT_POS = 1u << REFEREE2026_COMMON_DATA_ROBOT_POS,   //!< 收 0x0203 机器人位置
    REFEREE2026_COMMON_FILTER_BUFF = 1u << REFEREE2026_COMMON_DATA_BUFF,             //!< 收 0x0204 增益与底盘能量
    REFEREE2026_COMMON_FILTER_ROBOT_HURT = 1u << REFEREE2026_COMMON_DATA_ROBOT_HURT, //!< 收 0x0206 伤害状态
    REFEREE2026_COMMON_FILTER_SHOOT_DATA = 1u << REFEREE2026_COMMON_DATA_SHOOT_DATA, //!< 收 0x0207 实时射击
    //! 收 0x0208 允许发弹量与剩余金币
    REFEREE2026_COMMON_FILTER_PROJECTILE_ALLOW = 1u << REFEREE2026_COMMON_DATA_PROJECTILE_ALLOW,
    //! 收 0x0209 RFID 模块状态
    REFEREE2026_COMMON_FILTER_RFID_STATUS = 1u << REFEREE2026_COMMON_DATA_RFID_STATUS,
    //! 收 0x020A 飞镖选手端指令
    REFEREE2026_COMMON_FILTER_DART_CLIENT_CMD = 1u << REFEREE2026_COMMON_DATA_DART_CLIENT_CMD,
    //! 收 0x020B 地面机器人位置
    REFEREE2026_COMMON_FILTER_GROUND_ROBOT_POS = 1u << REFEREE2026_COMMON_DATA_GROUND_ROBOT_POS,
    REFEREE2026_COMMON_FILTER_RADAR_MARK = 1u << REFEREE2026_COMMON_DATA_RADAR_MARK,   //!< 收 0x020C 雷达标记进度
    REFEREE2026_COMMON_FILTER_SENTRY_INFO = 1u << REFEREE2026_COMMON_DATA_SENTRY_INFO, //!< 收 0x020D 哨兵决策同步
    REFEREE2026_COMMON_FILTER_RADAR_INFO = 1u << REFEREE2026_COMMON_DATA_RADAR_INFO,   //!< 收 0x020E 雷达决策同步
    //! 收 0x0301 机器人交互（队友机器人发来的"机器人之间通信"段；本链路唯一双向的命令）
    REFEREE2026_COMMON_FILTER_ROBOT_INTERACTION = 1u << REFEREE2026_COMMON_DATA_ROBOT_INTERACTION,
    //! 收 0x0303 选手端小地图交互
    REFEREE2026_COMMON_FILTER_MINI_MAP_INTERACT = 1u << REFEREE2026_COMMON_DATA_MINI_MAP_INTERACT,
} Referee2026CommonFilter_e;

/*============================================
 *   六、快照结构体（app 直接读这里）
 *============================================*/

/* 快照是一个**逐字节的镜像**：每收到一条命令，内核就把数据段 memcpy 到
 * `(uint8_t *)&snapshot + snap_off`。故它必须与数据段一样是紧凑的 —— 一旦让编译器按自然对齐
 * 插填充，成员偏移就不再等于"前面各段长度之和"，末尾那条 `sizeof == Σ LEN` 的断言会当场挂掉
 * （常规链路混着 1/2/4/8 字节成员与 float，不 pack 必然被填出十几个空洞）。
 * 注意 pack 会让 float / uint64 落在非对齐偏移上，读取由编译器拆成合规的字节访问序列 ——
 * 这与上面那批数据段结构体（`Referee2026ShootData_t` 的 float 就在偏移 3）是同一回事。 */
#pragma pack(push, 1)

/**
 * @brief 常规链路 RX 快照
 * @note **只有收得到的命令有成员**（21 个），顺序与数据名枚举一致 —— 只发不收的
 *       0x0305 / 0x0307 / 0x0308 不在其中。每个成员的字节偏移被记在元信息表的
 *       `snap_off` 里（`.c` 里用 `offsetof` 算出来）。
 * @note `robot_interaction`（0x0301）是**变长**的：成员按最大 118B 留，收到的帧可能只有
 *       6~118B。内核只 memcpy 收到的那些字节、并把余下的**清零**，故读不到的尾部读出来是 0
 *       而不是上一帧的残留；`data_cmd_id` 决定该读哪一段（见第五节）。
 * @note 读法见 drv_referee2026.h 的"快照的读法"（tick 三步读，防读到半帧）：
 *       本链路没有哪条命令的 memcpy 是原子的（最小的 0x0002 也有 1B，而 0x0301 有 118B），
 *       故 tick 三步读不能省。
 */
typedef struct
{
    Referee2026GameStatus_t game_status;               //!< 0x0001 比赛状态
    Referee2026GameResult_t game_result;               //!< 0x0002 比赛结果
    Referee2026GameRobotHp_t game_robot_hp;            //!< 0x0003 机器人血量
    Referee2026EventData_t event_data;                 //!< 0x0101 场地事件
    Referee2026RefereeWarning_t referee_warning;       //!< 0x0104 裁判警告
    Referee2026DartLaunch_t dart_launch;               //!< 0x0105 飞镖发射
    Referee2026RobotStatus_t robot_status;             //!< 0x0201 机器人性能体系
    Referee2026PowerHeat_t power_heat;                 //!< 0x0202 缓冲能量与射击热量
    Referee2026RobotPos_t robot_pos;                   //!< 0x0203 机器人位置
    Referee2026Buff_t buff;                            //!< 0x0204 增益与底盘能量
    Referee2026HurtData_t robot_hurt;                  //!< 0x0206 伤害状态
    Referee2026ShootData_t shoot_data;                 //!< 0x0207 实时射击
    Referee2026ProjectileAllowance_t projectile_allow; //!< 0x0208 允许发弹量与剩余金币
    Referee2026RfidStatus_t rfid_status;               //!< 0x0209 RFID 模块状态
    Referee2026DartClientCmd_t dart_client_cmd;        //!< 0x020A 飞镖选手端指令
    Referee2026GroundRobotPosition_t ground_robot_pos; //!< 0x020B 地面机器人位置
    Referee2026RadarMarkData_t radar_mark;             //!< 0x020C 雷达标记进度
    Referee2026SentryInfo_t sentry_info;               //!< 0x020D 哨兵自主决策信息同步
    Referee2026RadarInfo_t radar_info;                 //!< 0x020E 雷达自主决策信息同步
    Referee2026RobotInteraction_t robot_interaction;   //!< 0x0301 机器人交互数据（变长，见上）
    Referee2026MiniMapCommand_t mini_map_interact;     //!< 0x0303 选手端小地图交互
} Referee2026CommonSnapshot_t;

#pragma pack(pop)

/*============================================
 *   七、驱动实例与接口
 *============================================*/

/* 这一节要 HAL（`USARTInstance` / `DMA_RAM`），故先把开关本身引进来再判它 ——
 * 顺序颠倒的话 `HAL_UART_MODULE_ENABLED` 此刻还不可见，判据恒假、整节静默消失。
 * 这一行的代价是本头从此要求 `main.h` 在包含路径上（与 drv_dbus.h 等既有驱动一致）；
 * 上面六节（协议部分）本身只依赖 <stdint.h>，与 HAL 无关。 */
#include "main.h"

#ifdef HAL_UART_MODULE_ENABLED

#include "drv_referee2026.h"

/**
 * @brief 常规链路驱动实例
 * @note `core` **必须是第一个成员**（与仓库其余驱动的 vtable 习惯一致；本模块的代码本身不
 *       依赖这一点，回调一律从 `usart->parent` 取回内核 —— 那里存的是 `&inst->core`）。
 * @note 收发接口（Register / Config / Send）都是模块根那份 `Referee2026*`，调用时传
 *       `&inst.core` —— 例如 `Referee2026Config(&common_inst.core, &cfg)`。
 */
typedef struct
{
    Referee2026Core_t core;                             //!< 收发内核
    Referee2026CommonSnapshot_t snapshot;               //!< RX 快照（21 条）
    uint32_t tick[REFEREE2026_COMMON_DATA_COUNT];       //!< 每条的最近收到时刻（seqlock 头尾序号）
    uint32_t tx_last_us[REFEREE2026_COMMON_DATA_COUNT]; //!< 每条的最近发送时刻（限速用）
} Referee2026Common_t;

/**
 * @brief 实例结构体类型与元信息表的**拼接别名** —— 供模块根的 REFEREE2026_INSTANCE_DEF 使用
 * @note 实例定义宏四条链路只写一份（在 `drv_referee2026.h`），按链路名拼出三样东西，
 *       本头提供其中两样（`_DATA_COUNT` 本链路早就有了）：
 *       `REFEREE2026_<LINK>_INSTANCE_TYPE` / `_CMD_INFO` / `_DATA_COUNT`。
 *       故本头**一行收发/实例函数都不定义**，"本链路长什么样"全在这里、"怎么用"全在模块根。
 * @note 为什么要这层别名、而不是让内核宏自己拼：C 预处理器**不能转换大小写** ——
 *       `COMMON` 拼不出结构体名里的 `Common`，也拼不出表名里的小写 `common`。
 * @example
 *   REFEREE2026_INSTANCE_DEF(common_inst, COMMON);   // 常规链路
 */
#define REFEREE2026_COMMON_INSTANCE_TYPE Referee2026Common_t
#define REFEREE2026_COMMON_CMD_INFO referee2026_common_cmd_info

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* __REFEREE2026_COMMON_H */
