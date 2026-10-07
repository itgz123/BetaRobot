/**
 * @file referee2026_radar.c
 * @brief 雷达无线链路驱动实现：命令元信息表（唯一定义处）+ 快照/表长的编译期自校验
 *
 * 只有一样东西：`referee2026_radar_cmd_info[]` —— 本条链路命令的元信息表，`const` 落 .rodata。
 * 紧随其后的两条 `_Static_assert` 把表长与快照宽度钉死在数据名枚举上。
 *
 * **收发接口一行都不在这儿**：`Referee2026Register` / `Referee2026Config` / `Referee2026Send`
 * 四条链路逐字同构，只定义一次，实现全在模块根的 drv_referee2026.c 里。本文件只提供
 * "本链路长什么样"——表、长度、快照成员的对应关系。
 *
 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_RADAR_USED`，且 HAL 的 UART 模块开着。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 referee2026_radar.h 的说明）。
 */

#include "referee2026_radar.h"

#include <stddef.h> /* offsetof —— 快照成员偏移的唯一出处 */

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_RADAR_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_RADAR_USED) && defined(HAL_UART_MODULE_ENABLED)

/*============================================
 *   一、命令元信息（6 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_RADAR_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `dir == 0` / `freq == 0` / `snap_off == 0` 当"未填"哨兵 —— 这些 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_radar_cmd_info[] = {
    [REFEREE2026_RADAR_DATA_RADAR_OPPONENT_POS] =
        {
            .cmd_id = 0x0A01,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_opponent_pos),
            .freq_hz = 10,     /* 服务器（雷达站）固定 10Hz 发 */
            .freq_max_hz = 10, /* 固定发送的行两列同值：上限就是它本身 */
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_RADAR_DATA_RADAR_OPPONENT_HP] =
        {
            .cmd_id = 0x0A02,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_opponent_hp),
            .freq_hz = 10,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_RADAR_DATA_RADAR_OPPONENT_AMMO] =
        {
            .cmd_id = 0x0A03,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_opponent_ammo),
            .freq_hz = 10,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_RADAR_DATA_RADAR_OPPONENT_STATE] =
        {
            .cmd_id = 0x0A04,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_opponent_state),
            .freq_hz = 10,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_RADAR_DATA_RADAR_OPPONENT_BUFF] =
        {
            .cmd_id = 0x0A05,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_opponent_buff),
            .freq_hz = 10,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_RADAR_DATA_RADAR_KEY] =
        {
            .cmd_id = 0x0A06,
            .data_len = REFEREE2026_RADAR_LEN_RADAR_KEY,
            .min_len = REFEREE2026_RADAR_LEN_RADAR_KEY,
            .snap_off = (uint16_t)offsetof(Referee2026RadarSnapshot_t, radar_key),
            .freq_hz = 10,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_RX,
        },
};

_Static_assert(sizeof(referee2026_radar_cmd_info) / sizeof(referee2026_radar_cmd_info[0]) ==
                   REFEREE2026_RADAR_DATA_COUNT,
               "雷达无线链路命令元信息表项数必须等于数据名枚举数");

/* 快照成员个数 == 本链路 `dir & RX` 的命令数（6 条全收）。把总长钉死在 6 个数据段的合计上：
 * 加一条 RX 命令却忘了在快照里加成员，这条断言就会挂（24+12+10+8+41+6）。 */
_Static_assert(sizeof(Referee2026RadarSnapshot_t) ==
                   REFEREE2026_RADAR_LEN_RADAR_OPPONENT_POS + REFEREE2026_RADAR_LEN_RADAR_OPPONENT_HP +
                       REFEREE2026_RADAR_LEN_RADAR_OPPONENT_AMMO + REFEREE2026_RADAR_LEN_RADAR_OPPONENT_STATE +
                       REFEREE2026_RADAR_LEN_RADAR_OPPONENT_BUFF + REFEREE2026_RADAR_LEN_RADAR_KEY,
               "雷达无线链路快照成员数与 RX 命令数不符");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_RADAR_USED && HAL_UART_MODULE_ENABLED */
