/**
 * @file referee2026_common_frame.c
 * @brief 常规链路 24 条命令的元信息表（唯一定义处，const 落 .rodata）

 * include 本链路 referee2026_common_cmd.h 的目的：取 `REFEREE2026_COMMON_LEN_<TAG>` 长度宏
 * —— data_len 的数值只在那个宏里写一次，本表与结构体的长度断言共用它；顺带把那批
 * `sizeof == 长度` 的 `_Static_assert` 真正编译一次（此前没有任何 .c include 过该头）。

 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_COMMON_USED`。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 frame.h 的说明）。
 */

#include "referee2026_common_frame.h"

#include "referee2026_cmd.h"
#include "referee2026_common_cmd.h" /* REFEREE2026_COMMON_LEN_* —— data_len 的唯一出处 */

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_COMMON_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_COMMON_USED)

/*============================================
 *   一、命令元信息（24 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_COMMON_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `receiver == 0` / `freq == 0` 当"未填"哨兵 —— 这两者的 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_common_cmd_info[] = {
    [REFEREE2026_COMMON_DATA_GAME_STATUS] =
        {
            .cmd_id = 0x0001,
            .data_len = REFEREE2026_COMMON_LEN_GAME_STATUS,
            .receiver = REFEREE2026_RX_ALL_ROBOTS,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_GAME_RESULT] =
        {
            .cmd_id = 0x0002,
            .data_len = REFEREE2026_COMMON_LEN_GAME_RESULT,
            .receiver = REFEREE2026_RX_ALL_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 0,
        },
    [REFEREE2026_COMMON_DATA_GAME_ROBOT_HP] =
        {
            .cmd_id = 0x0003,
            .data_len = REFEREE2026_COMMON_LEN_GAME_ROBOT_HP,
            .receiver = REFEREE2026_RX_ALL_ROBOTS,
            .freq_hz = 3,
            .freq_max_hz = 3,
        },
    [REFEREE2026_COMMON_DATA_EVENT_DATA] =
        {
            .cmd_id = 0x0101,
            .data_len = REFEREE2026_COMMON_LEN_EVENT_DATA,
            .receiver = REFEREE2026_RX_SELF_ROBOTS,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_REFEREE_WARNING] =
        {
            .cmd_id = 0x0104,
            .data_len = REFEREE2026_COMMON_LEN_REFEREE_WARNING,
            .receiver = REFEREE2026_RX_PENALIZED_ROBOTS,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_DART_LAUNCH] =
        {
            .cmd_id = 0x0105,
            .data_len = REFEREE2026_COMMON_LEN_DART_LAUNCH,
            .receiver = REFEREE2026_RX_SELF_ROBOTS,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_STATUS] =
        {
            .cmd_id = 0x0201,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_STATUS,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 10,
            .freq_max_hz = 10,
        },
    [REFEREE2026_COMMON_DATA_POWER_HEAT] =
        {
            .cmd_id = 0x0202,
            .data_len = REFEREE2026_COMMON_LEN_POWER_HEAT,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 10,
            .freq_max_hz = 10,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_POS] =
        {
            .cmd_id = 0x0203,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_POS,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_BUFF] =
        {
            .cmd_id = 0x0204,
            .data_len = REFEREE2026_COMMON_LEN_BUFF,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 3,
            .freq_max_hz = 3,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_HURT] =
        {
            .cmd_id = 0x0206,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_HURT,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 0,
        },
    [REFEREE2026_COMMON_DATA_SHOOT_DATA] =
        {
            .cmd_id = 0x0207,
            .data_len = REFEREE2026_COMMON_LEN_SHOOT_DATA,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 0,
        },
    [REFEREE2026_COMMON_DATA_PROJECTILE_ALLOW] =
        {
            .cmd_id = 0x0208,
            .data_len = REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW,
            .receiver = REFEREE2026_RX_SELF_HERO | REFEREE2026_RX_SELF_INFANTRY | REFEREE2026_RX_SELF_SENTRY |
                        REFEREE2026_RX_SELF_AERIAL,
            .freq_hz = 10,
            .freq_max_hz = 10,
        },
    [REFEREE2026_COMMON_DATA_RFID_STATUS] =
        {
            .cmd_id = 0x0209,
            .data_len = REFEREE2026_COMMON_LEN_RFID_STATUS,
            .receiver = REFEREE2026_RX_SELF_RFID,
            .freq_hz = 3,
            .freq_max_hz = 3,
        },
    [REFEREE2026_COMMON_DATA_DART_CLIENT_CMD] =
        {
            .cmd_id = 0x020A,
            .data_len = REFEREE2026_COMMON_LEN_DART_CLIENT_CMD,
            .receiver = REFEREE2026_RX_SELF_DART,
            .freq_hz = 3,
            .freq_max_hz = 3,
        },
    [REFEREE2026_COMMON_DATA_GROUND_ROBOT_POS] =
        {
            .cmd_id = 0x020B,
            .data_len = REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS,
            .receiver = REFEREE2026_RX_SELF_SENTRY,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_RADAR_MARK] =
        {
            .cmd_id = 0x020C,
            .data_len = REFEREE2026_COMMON_LEN_RADAR_MARK,
            .receiver = REFEREE2026_RX_SELF_RADAR,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_SENTRY_INFO] =
        {
            .cmd_id = 0x020D,
            .data_len = REFEREE2026_COMMON_LEN_SENTRY_INFO,
            .receiver = REFEREE2026_RX_SELF_SENTRY,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_RADAR_INFO] =
        {
            .cmd_id = 0x020E,
            .data_len = REFEREE2026_COMMON_LEN_RADAR_INFO,
            .receiver = REFEREE2026_RX_SELF_RADAR,
            .freq_hz = 1,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_INTERACTION] =
        {
            .cmd_id = 0x0301,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_INTERACTION,
            .receiver = REFEREE2026_RX_NONE,
            .freq_hz = 0,
            .freq_max_hz = 30,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_INTERACT] =
        {
            .cmd_id = 0x0303,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 0,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_RADAR] =
        {
            .cmd_id = 0x0305,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_RADAR,
            .receiver = REFEREE2026_RX_SELF_STATIONS,
            .freq_hz = 0,
            .freq_max_hz = 5,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_PATH] =
        {
            .cmd_id = 0x0307,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_PATH,
            .receiver = REFEREE2026_RX_OWN_STATION,
            .freq_hz = 0,
            .freq_max_hz = 1,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT] =
        {
            .cmd_id = 0x0308,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT,
            .receiver = REFEREE2026_RX_SELF_STATIONS,
            .freq_hz = 0,
            .freq_max_hz = 3,
        },
};

_Static_assert(sizeof(referee2026_common_cmd_info) / sizeof(referee2026_common_cmd_info[0]) ==
                   REFEREE2026_COMMON_DATA_COUNT,
               "常规链路命令元信息表项数必须等于数据名枚举数");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_COMMON_USED */
