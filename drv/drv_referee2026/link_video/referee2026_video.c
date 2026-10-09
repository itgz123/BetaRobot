/**
 * @file referee2026_video.c
 * @brief 图传链路驱动实现：命令元信息表（唯一定义处）+ 快照/表长的编译期自校验
 *
 * 只有一样东西：`referee2026_video_cmd_info[]` —— 本条链路命令的元信息表，`const` 落 .rodata。
 * 紧随其后的两条 `_Static_assert` 把表长与快照宽度钉死在数据名枚举上。
 *
 * **收发接口一行都不在这儿**：`Referee2026Register` / `Referee2026Config` / `Referee2026Send`
 * 四条链路逐字同构，只定义一次，实现全在模块根的 drv_referee2026.c 里。本文件只提供
 * "本链路长什么样"——表、长度、快照成员的对应关系（含本链路**单向**的方向判定）。
 *
 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_VIDEO_USED`，且 HAL 的 UART 模块开着。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 referee2026_video.h 的说明）。
 */

#include "referee2026_video.h"

#include <stddef.h> /* offsetof —— 快照成员偏移的唯一出处 */

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_VIDEO_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_VIDEO_USED) && defined(HAL_UART_MODULE_ENABLED)

/*============================================
 *   一、命令元信息（4 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_VIDEO_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `dir == 0` / `freq == 0` / `snap_off == 0` 当"未填"哨兵 —— 这些 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_video_cmd_info[] = {
    [REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER] =
        {
            .cmd_id = 0x0302,
            .data_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER,
            .min_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER,
            .snap_off = (uint16_t)offsetof(Referee2026VideoSnapshot_t, custom_controller),
            .freq_hz = 0, /* 发送方触发发送，服务器不周期发 */
            /* 只收不发：自定义控制器的指令经"裁判系统 PC 软件 → 裁判系统 → 图传链路"到达机器人；
             * 本框架跑在机器人这侧，只会收到它。 */
            .freq_max_hz = 30,
            .dir = REFEREE2026_DIR_RX,
        },
    [REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER_RX] =
        {
            .cmd_id = 0x0309,
            .data_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX,
            .min_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX,
            /* 只发不收：机器人回给自定义控制器的数据（回程同样经裁判系统转发）⇒ 快照里没有它的位置 */
            .snap_off = 0,
            .freq_hz = 0,
            .freq_max_hz = 10,
            .dir = REFEREE2026_DIR_TX,
        },
    [REFEREE2026_VIDEO_DATA_ROBOT_TO_CLIENT] =
        {
            .cmd_id = 0x0310,
            .data_len = REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT,
            .min_len = REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT,
            /* 只发不收（本框架是机器人这侧）⇒ 快照里没有它的位置，偏移填 0 */
            .snap_off = 0,
            .freq_hz = 0,
            .freq_max_hz = 50,
            .dir = REFEREE2026_DIR_TX,
        },
    [REFEREE2026_VIDEO_DATA_CLIENT_TO_ROBOT] =
        {
            .cmd_id = 0x0311,
            .data_len = REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT,
            .min_len = REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT,
            .snap_off = (uint16_t)offsetof(Referee2026VideoSnapshot_t, client_to_robot),
            .freq_hz = 0,
            .freq_max_hz = 75,         /* 全协议速率上限最紧的一条 */
            .dir = REFEREE2026_DIR_RX, /* 只收不发（本框架是机器人这侧） */
        },
};

_Static_assert(sizeof(referee2026_video_cmd_info) / sizeof(referee2026_video_cmd_info[0]) == REFEREE2026_VIDEO_DATA_COUNT,
               "图传链路命令元信息表项数必须等于数据名枚举数");

/* 快照成员个数 == 本链路 `dir & RX` 的命令数。表里 `snap_off == 0` 的项有两种可能
 * （只发不收、或恰好排在快照开头），故这里换个角度钉：把快照总长钉死在"两条 30B 透传"上
 * （0x0302 / 0x0311 各 30B；0x0309 / 0x0310 只发不收，不入快照）。加一条 RX 命令却忘了加成员，
 * 这条断言就会挂。 */
_Static_assert(sizeof(Referee2026VideoSnapshot_t) == 2u * 30u, "图传链路快照成员数与 RX 命令数不符");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_VIDEO_USED && HAL_UART_MODULE_ENABLED */
