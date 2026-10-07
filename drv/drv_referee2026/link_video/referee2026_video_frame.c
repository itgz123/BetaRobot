/**
 * @file referee2026_video_frame.c
 * @brief 图传链路 4 条命令的元信息表（唯一定义处，const 落 .rodata）

 * include 本链路 referee2026_video_cmd.h 的目的：取 `REFEREE2026_VIDEO_LEN_<TAG>` 长度宏
 * —— data_len 的数值只在那个宏里写一次，本表与结构体的长度断言共用它；顺带把那批
 * `sizeof == 长度` 的 `_Static_assert` 真正编译一次（此前没有任何 .c include 过该头）。

 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_VIDEO_USED`。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 frame.h 的说明）。
 */

#include "referee2026_video_frame.h"

#include "referee2026_cmd.h"
#include "referee2026_video_cmd.h" /* REFEREE2026_VIDEO_LEN_* —— data_len 的唯一出处 */

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_VIDEO_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_VIDEO_USED)

/*============================================
 *   一、命令元信息（4 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_VIDEO_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `receiver == 0` / `freq == 0` 当"未填"哨兵 —— 这两者的 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_video_cmd_info[] = {
    [REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER] =
        {
            .cmd_id = 0x0302,
            .data_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER,
            .receiver = REFEREE2026_RX_TARGET_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 30,
        },
    [REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER_RX] =
        {
            .cmd_id = 0x0309,
            .data_len = REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX,
            .receiver = REFEREE2026_RX_CUSTOM_CONTROLLER,
            .freq_hz = 0,
            .freq_max_hz = 10,
        },
    [REFEREE2026_VIDEO_DATA_ROBOT_TO_CLIENT] =
        {
            .cmd_id = 0x0310,
            .data_len = REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT,
            .receiver = REFEREE2026_RX_CUSTOM_CLIENT,
            .freq_hz = 0,
            .freq_max_hz = 50,
        },
    [REFEREE2026_VIDEO_DATA_CLIENT_TO_ROBOT] =
        {
            .cmd_id = 0x0311,
            .data_len = REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT,
            .receiver = REFEREE2026_RX_SELF_ROBOTS,
            .freq_hz = 0,
            .freq_max_hz = 75,
        },
};

_Static_assert(sizeof(referee2026_video_cmd_info) / sizeof(referee2026_video_cmd_info[0]) ==
                   REFEREE2026_VIDEO_DATA_COUNT,
               "图传链路命令元信息表项数必须等于数据名枚举数");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_VIDEO_USED */
