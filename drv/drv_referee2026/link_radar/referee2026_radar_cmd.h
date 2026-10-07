/**
 * @file referee2026_radar_cmd.h
 * @brief 雷达无线链路 6 条命令的数据段（0x0A01 ~ 0x0A06）
 *
 * 依据：RoboMaster 2026 高校系列赛通信协议 V2.0.0 §1.6 雷达无线链路数据说明（表 1-44 ~ 表 1-49）。
 * 命令码与元信息（长度 / 接收方 / 频率）见 referee2026_radar_frame.h 的
 * `Referee2026RadarDataId_e`；元信息表的定义在同名的 .c 里。
 *
 * 公共约定（packed / 小端 / 位域 union 写法 / 文档笔误口径）见 core/referee2026_cmd.h ——
 * 那里放着跨链路的公共组成块，本文件 include 它。
 * 每条命令的**数据段长度**以 `REFEREE2026_RADAR_LEN_<TAG>` 宏为准（紧挨对应结构体上方，值取
 * 官方"字节偏移量字段表"的合计），文件末尾的长度断言与 referee2026_radar_frame.c 的元信息表
 * **共用同一个宏** —— 一处定义、两处引用，改长度只改宏。
 */

#ifndef __REFEREE2026_RADAR_CMD_H
#define __REFEREE2026_RADAR_CMD_H

#include <stdint.h>

#include "referee2026_cmd.h"

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
 */
typedef struct
{
    uint16_t hero_hp;       //!< 对方 1 号英雄机器人血量
    uint16_t engineer_hp;   //!< 对方 2 号工程机器人血量
    uint16_t infantry_3_hp; //!< 对方 3 号步兵机器人血量
    uint16_t infantry_4_hp; //!< 对方 4 号步兵机器人血量
    uint16_t reserved;      //!< 偏移 8 保留位
    uint16_t sentry_hp;     //!< 对方 7 号哨兵机器人血量
} Referee2026RadarOpponentHp_t;

/* 0x0A03 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO 10

/**
 * @brief 0x0A03 对方机器人允许发弹量（表 1-46）
 * @note 3 号步兵的值**含堡垒提供的储备允许发弹量**。
 */
typedef struct
{
    uint16_t hero_allowance;       //!< 对方 1 号英雄机器人允许发弹量
    uint16_t infantry_3_allowance; //!< 对方 3 号步兵机器人允许发弹量（含堡垒储备）
    uint16_t infantry_4_allowance; //!< 对方 4 号步兵机器人允许发弹量
    uint16_t aerial_allowance;     //!< 对方 6 号空中机器人允许发弹量
    uint16_t sentry_allowance;     //!< 对方 7 号哨兵机器人允许发弹量
} Referee2026RadarOpponentAmmo_t;

/**
 * @brief 0x0A04 的 32 位状态字位域视图（表 1-47 偏移 4，4B）
 * @note 占领状态类字段取值：0 未被占领 / 1 被对方占领 / 2 被己方占领 /（堡垒项还有）3 被双方占领；
 *       `*_tunnel_*` 与 `*_cross_*` 这几位是 1/0 的"是否检测到对方机器人场地交互模块"。
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
 *       `Referee2026BuffItem_t` **逐字节相同**；后 6B 是 6 个 8 位状态码。
 * @note 状态码取值（末 5 个）：0 存活 / 1 战亡 / 2 无敌但不虚弱 / 3 无敌且虚弱；
 *       异常离线不影响此处的状态判断。
 */
typedef struct
{
    Referee2026BuffItem_t buff[5]; //!< 偏移 0 对方 5 台机器人的增益块，依次为英雄、工程、3 号步兵、4 号步兵、哨兵
    uint8_t
        sentry_posture; //!< 偏移 35 对方哨兵当前姿态：1 进攻 / 2 防御 / 3 移动 / 4 强化进攻 / 5 强化防御 / 6 强化移动
    uint8_t hero_state; //!< 偏移 36 对方英雄机器人主要状态
    uint8_t engineer_state;   //!< 偏移 37 对方工程机器人主要状态
    uint8_t infantry_3_state; //!< 偏移 38 对方 3 号步兵机器人主要状态
    uint8_t infantry_4_state; //!< 偏移 39 对方 4 号步兵机器人主要状态
    uint8_t sentry_state;     //!< 偏移 40 对方哨兵机器人主要状态
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
 *   长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 core/referee2026_cmd.h 的公共约定）。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...)                                                                     \
    _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0A01,
                              sizeof(Referee2026RadarOpponentPos_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS);
REFEREE2026_CMD_STATIC_ASSERT(0x0A02, sizeof(Referee2026RadarOpponentHp_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP);
REFEREE2026_CMD_STATIC_ASSERT(0x0A03,
                              sizeof(Referee2026RadarOpponentAmmo_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO);
REFEREE2026_CMD_STATIC_ASSERT(0x0A04,
                              sizeof(Referee2026RadarOpponentState_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE);
REFEREE2026_CMD_STATIC_ASSERT(0x0A05,
                              sizeof(Referee2026RadarOpponentBuff_t) == REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF);
REFEREE2026_CMD_STATIC_ASSERT(0x0A06, sizeof(Referee2026RadarKey_t) == REFEREE2026_RADAR_LEN_RADAR_KEY);

#undef REFEREE2026_CMD_STATIC_ASSERT

/* 位域结构体自身的宽度自检：查"多写了位"（跨出标量宽）。
 * "少写"查不出来，故每个位域结构体末尾都补了 reserved —— 见 core/referee2026_cmd.h。 */
_Static_assert(sizeof(Referee2026RadarOpponentStateBits_t) == sizeof(uint32_t), "0x0A04 位域超出 32 位");

#endif /* __REFEREE2026_RADAR_CMD_H */
