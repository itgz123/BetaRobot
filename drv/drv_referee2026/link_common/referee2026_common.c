/**
 * @file referee2026_common.c
 * @brief 常规链路驱动实现：命令元信息表（唯一定义处）+ 快照/表长的编译期自校验
 *
 * 只有一样东西：`referee2026_common_cmd_info[]` —— 本条链路命令的元信息表，
 * `const` 落 .rodata（24 × 10B = 240B）。紧随其后的两条 `_Static_assert` 把表长与快照宽度
 * 钉死在数据名枚举上。
 *
 * **收发接口一行都不在这儿**：`Referee2026Register` / `Referee2026Config` / `Referee2026Send`
 * 四条链路逐字同构，只定义一次，实现全在模块根的 drv_referee2026.c 里。本文件只提供
 * "本链路长什么样"——表、长度、快照成员的对应关系。
 *
 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_COMMON_USED`，且 HAL 的 UART 模块开着。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 referee2026_common.h 的说明）。
 */

#include "referee2026_common.h"

#include <stddef.h> /* offsetof —— 快照成员偏移的唯一出处 */

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_COMMON_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_COMMON_USED) && defined(HAL_UART_MODULE_ENABLED)

/*============================================
 *   一、命令元信息（24 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_COMMON_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `dir == 0` / `freq == 0` / `snap_off == 0` 当"未填"哨兵 —— 这些 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_common_cmd_info[] = {
    [REFEREE2026_COMMON_DATA_GAME_STATUS] =
        {
            .cmd_id = 0x0001,
            .data_len = REFEREE2026_COMMON_LEN_GAME_STATUS,
            .min_len = REFEREE2026_COMMON_LEN_GAME_STATUS,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, game_status),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_GAME_RESULT] =
        {
            .cmd_id = 0x0002,
            .data_len = REFEREE2026_COMMON_LEN_GAME_RESULT,
            .min_len = REFEREE2026_COMMON_LEN_GAME_RESULT,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, game_result),
            .freq_hz = 0,     /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 0, /* 无速率约束 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_GAME_ROBOT_HP] =
        {
            .cmd_id = 0x0003,
            .data_len = REFEREE2026_COMMON_LEN_GAME_ROBOT_HP,
            .min_len = REFEREE2026_COMMON_LEN_GAME_ROBOT_HP,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, game_robot_hp),
            .freq_hz = 3,     /* 服务器固定 3Hz 发 */
            .freq_max_hz = 3, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_EVENT_DATA] =
        {
            .cmd_id = 0x0101,
            .data_len = REFEREE2026_COMMON_LEN_EVENT_DATA,
            .min_len = REFEREE2026_COMMON_LEN_EVENT_DATA,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, event_data),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_REFEREE_WARNING] =
        {
            .cmd_id = 0x0104,
            .data_len = REFEREE2026_COMMON_LEN_REFEREE_WARNING,
            .min_len = REFEREE2026_COMMON_LEN_REFEREE_WARNING,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, referee_warning),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_DART_LAUNCH] =
        {
            .cmd_id = 0x0105,
            .data_len = REFEREE2026_COMMON_LEN_DART_LAUNCH,
            .min_len = REFEREE2026_COMMON_LEN_DART_LAUNCH,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, dart_launch),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_STATUS] =
        {
            .cmd_id = 0x0201,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_STATUS,
            .min_len = REFEREE2026_COMMON_LEN_ROBOT_STATUS,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, robot_status),
            .freq_hz = 10,     /* 服务器固定 10Hz 发 */
            .freq_max_hz = 10, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_POWER_HEAT] =
        {
            .cmd_id = 0x0202,
            .data_len = REFEREE2026_COMMON_LEN_POWER_HEAT,
            .min_len = REFEREE2026_COMMON_LEN_POWER_HEAT,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, power_heat),
            .freq_hz = 10,     /* 服务器固定 10Hz 发 */
            .freq_max_hz = 10, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_POS] =
        {
            .cmd_id = 0x0203,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_POS,
            .min_len = REFEREE2026_COMMON_LEN_ROBOT_POS,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, robot_pos),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_BUFF] =
        {
            .cmd_id = 0x0204,
            .data_len = REFEREE2026_COMMON_LEN_BUFF,
            .min_len = REFEREE2026_COMMON_LEN_BUFF,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, buff),
            .freq_hz = 3,     /* 服务器固定 3Hz 发 */
            .freq_max_hz = 3, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_HURT] =
        {
            .cmd_id = 0x0206,
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_HURT,
            .min_len = REFEREE2026_COMMON_LEN_ROBOT_HURT,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, robot_hurt),
            .freq_hz = 0,     /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 0, /* 无速率约束 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_SHOOT_DATA] =
        {
            .cmd_id = 0x0207,
            .data_len = REFEREE2026_COMMON_LEN_SHOOT_DATA,
            .min_len = REFEREE2026_COMMON_LEN_SHOOT_DATA,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, shoot_data),
            .freq_hz = 0,     /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 0, /* 无速率约束 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_PROJECTILE_ALLOW] =
        {
            .cmd_id = 0x0208,
            .data_len = REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW,
            .min_len = REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, projectile_allow),
            .freq_hz = 10,     /* 服务器固定 10Hz 发 */
            .freq_max_hz = 10, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_RFID_STATUS] =
        {
            .cmd_id = 0x0209,
            .data_len = REFEREE2026_COMMON_LEN_RFID_STATUS,
            .min_len = REFEREE2026_COMMON_LEN_RFID_STATUS,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, rfid_status),
            .freq_hz = 3,     /* 服务器固定 3Hz 发 */
            .freq_max_hz = 3, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_DART_CLIENT_CMD] =
        {
            .cmd_id = 0x020A,
            .data_len = REFEREE2026_COMMON_LEN_DART_CLIENT_CMD,
            .min_len = REFEREE2026_COMMON_LEN_DART_CLIENT_CMD,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, dart_client_cmd),
            .freq_hz = 3,     /* 服务器固定 3Hz 发 */
            .freq_max_hz = 3, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_GROUND_ROBOT_POS] =
        {
            .cmd_id = 0x020B,
            .data_len = REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS,
            .min_len = REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, ground_robot_pos),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_RADAR_MARK] =
        {
            .cmd_id = 0x020C,
            .data_len = REFEREE2026_COMMON_LEN_RADAR_MARK,
            .min_len = REFEREE2026_COMMON_LEN_RADAR_MARK,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, radar_mark),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_SENTRY_INFO] =
        {
            .cmd_id = 0x020D,
            .data_len = REFEREE2026_COMMON_LEN_SENTRY_INFO,
            .min_len = REFEREE2026_COMMON_LEN_SENTRY_INFO,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, sentry_info),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_RADAR_INFO] =
        {
            .cmd_id = 0x020E,
            .data_len = REFEREE2026_COMMON_LEN_RADAR_INFO,
            .min_len = REFEREE2026_COMMON_LEN_RADAR_INFO,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, radar_info),
            .freq_hz = 1,     /* 服务器固定 1Hz 发 */
            .freq_max_hz = 1, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_ROBOT_INTERACTION] =
        {
            .cmd_id = 0x0301,
            /* 快照成员尺寸 = 6B 报文头 + 112B 子内容；真机的帧头 data_length 在
             * [min_len, data_len] 之间（子内容 0x0200~0x02FF 那段最长 112B） */
            .data_len = REFEREE2026_COMMON_LEN_ROBOT_INTERACTION,
            /* 变长命令：只有 6B 报文头（data_cmd_id / sender_id / receiver_id）也合法 */
            .min_len = 6,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, robot_interaction),
            .freq_hz = 0, /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 30,
            /* 全协议**唯一双向**的一条：既发给服务器/选手端（子内容 0x0100~0x01FF、
             * 0x0120/0x0121），也收队友机器人的"机器人之间通信"（0x0200~0x02FF）。 */
            .dir = (Referee2026Dir_e)(REFEREE2026_DIR_RX | REFEREE2026_DIR_TX),
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_INTERACT] =
        {
            .cmd_id = 0x0303,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT,
            .min_len = REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT,
            .snap_off = (uint16_t)offsetof(Referee2026CommonSnapshot_t, mini_map_interact),
            .freq_hz = 0,     /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 0, /* 无速率约束 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_RADAR] =
        {
            .cmd_id = 0x0305,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_RADAR,
            .min_len = REFEREE2026_COMMON_LEN_MINI_MAP_RADAR,
            /* 只发不收 ⇒ 快照里没有它的位置，偏移填 0 */
            .snap_off = 0,
            .freq_hz = 0, /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 5,
            .dir = REFEREE2026_DIR_TX,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_PATH] =
        {
            .cmd_id = 0x0307,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_PATH,
            .min_len = REFEREE2026_COMMON_LEN_MINI_MAP_PATH,
            /* 只发不收 ⇒ 快照里没有它的位置，偏移填 0 */
            .snap_off = 0,
            .freq_hz = 0, /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 1,
            .dir = REFEREE2026_DIR_TX,
        },
    [REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT] =
        {
            .cmd_id = 0x0308,
            .data_len = REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT,
            .min_len = REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT,
            /* 只发不收 ⇒ 快照里没有它的位置，偏移填 0 */
            .snap_off = 0,
            .freq_hz = 0, /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 3,
            .dir = REFEREE2026_DIR_TX,
        },
};

_Static_assert(sizeof(referee2026_common_cmd_info) / sizeof(referee2026_common_cmd_info[0]) == REFEREE2026_COMMON_DATA_COUNT,
               "常规链路命令元信息表项数必须等于数据名枚举数");

/* 快照成员个数 == 本链路 `dir & RX` 的命令数（24 条里 21 条；只发不收的 0x0305 / 0x0307 /
 * 0x0308 三条不在快照里）。把总长钉死在 21 个数据段的合计上：加一条 RX 命令却忘了在快照里
 * 加成员，这条断言就会挂。能这么算的前提是快照被 pack 成紧凑镜像（见 referee2026_common.h 第六节）。 */
_Static_assert(sizeof(Referee2026CommonSnapshot_t) ==
                   REFEREE2026_COMMON_LEN_GAME_STATUS + REFEREE2026_COMMON_LEN_GAME_RESULT + REFEREE2026_COMMON_LEN_GAME_ROBOT_HP +
                       REFEREE2026_COMMON_LEN_EVENT_DATA + REFEREE2026_COMMON_LEN_REFEREE_WARNING + REFEREE2026_COMMON_LEN_DART_LAUNCH +
                       REFEREE2026_COMMON_LEN_ROBOT_STATUS + REFEREE2026_COMMON_LEN_POWER_HEAT + REFEREE2026_COMMON_LEN_ROBOT_POS +
                       REFEREE2026_COMMON_LEN_BUFF + REFEREE2026_COMMON_LEN_ROBOT_HURT + REFEREE2026_COMMON_LEN_SHOOT_DATA +
                       REFEREE2026_COMMON_LEN_PROJECTILE_ALLOW + REFEREE2026_COMMON_LEN_RFID_STATUS + REFEREE2026_COMMON_LEN_DART_CLIENT_CMD +
                       REFEREE2026_COMMON_LEN_GROUND_ROBOT_POS + REFEREE2026_COMMON_LEN_RADAR_MARK + REFEREE2026_COMMON_LEN_SENTRY_INFO +
                       REFEREE2026_COMMON_LEN_RADAR_INFO + REFEREE2026_COMMON_LEN_ROBOT_INTERACTION + REFEREE2026_COMMON_LEN_MINI_MAP_INTERACT,
               "常规链路快照成员数与 RX 命令数不符");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_COMMON_USED && HAL_UART_MODULE_ENABLED */
