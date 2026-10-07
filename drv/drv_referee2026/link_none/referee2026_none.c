/**
 * @file referee2026_none.c
 * @brief 非链路驱动实现：命令元信息表（唯一定义处）+ 表长的编译期自校验
 *
 * 只有一样东西：`referee2026_none_cmd_info[]` —— 本条链路命令的元信息表，`const` 落 .rodata。
 * 紧随其后的 `_Static_assert` 把表长钉死在数据名枚举上。
 *
 * **收发接口一行都不在这儿**：`Referee2026Register` / `Referee2026Config` / `Referee2026Send`
 * 四条链路逐字同构，只定义一次，实现全在模块根的 drv_referee2026.c 里。本文件只提供
 * "本链路长什么样"——表、长度、快照成员的对应关系。
 *
 * 生效条件（两个开关都在 app_cfg.h 里开）：
 *   `DRV_REFEREE2026_USED` && `REFEREE2026_LINK_NONE_USED`，且 HAL 的 UART 模块开着。
 * 未开时本文件编译为空 TU；头文件里的声明仍在（见 referee2026_none.h 的说明）。
 */

#include "referee2026_none.h"

#include "app_cfg.h" /* DRV_REFEREE2026_USED / REFEREE2026_LINK_NONE_USED */

#if defined(DRV_REFEREE2026_USED) && defined(REFEREE2026_LINK_NONE_USED) && defined(HAL_UART_MODULE_ENABLED)

/*============================================
 *   一、命令元信息（1 条）
 *============================================*/

/* 一律用指定初始化器 `[枚举成员] = ...`：表项书写顺序与枚举声明顺序解耦，挪动某条不会串位。
 * 本处**故意不写数组维度** —— 漏掉最后一个成员时数组长度会短一格，与头文件声明的
 * REFEREE2026_NONE_DATA_COUNT 对不上，末尾的 _Static_assert 直接报错。
 * 注意漏**中间**某个成员是查不出来的（指定初始化器会把它补成全 0），故不得拿
 * `dir == 0` / `freq == 0` 当"未填"哨兵 —— 这些 0 都是合法值。 */
const Referee2026CmdInfo_t referee2026_none_cmd_info[] = {
    [REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA] =
        {
            .cmd_id = 0x0306,
            .data_len = REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA,
            .min_len = REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA,
            /* 本链路一条收得到的命令都没有（`dir` 里没有 RX），故快照里没有它的位置。
             * 内核据此连接收常开流都不起，这个 0 永远不会被当作偏移用。 */
            .snap_off = 0,
            .freq_hz = 0,      /* 发送方触发发送，服务器不周期发 */
            .freq_max_hz = 30, /* 表 1-5 说明列：频率上限 30Hz */
            .dir = REFEREE2026_DIR_TX,
        },
};

_Static_assert(sizeof(referee2026_none_cmd_info) / sizeof(referee2026_none_cmd_info[0]) == REFEREE2026_NONE_DATA_COUNT,
               "非链路命令元信息表项数必须等于数据名枚举数");

#endif /* DRV_REFEREE2026_USED && REFEREE2026_LINK_NONE_USED && HAL_UART_MODULE_ENABLED */
