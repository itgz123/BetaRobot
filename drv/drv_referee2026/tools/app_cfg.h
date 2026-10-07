/**
 * @file app_cfg.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 各驱动 .c 会 `#include "app_cfg.h"` 取开关，真板上那个会拉进 bsp_map.h / robot_def.h /
 * cubemx 头，PC 编译不动。本文件同目录先于任何 -I 路径被引用（引号包含先查包含者所在目录），
 * 只提供测试所需的开关宏。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __APP_CFG_PC_STUB_H
#define __APP_CFG_PC_STUB_H

/* 真 app_cfg.h 经 bsp_map.h / robot_def.h 间接带入的标准头，桩里要自己补上
 * （模块实现依赖它们提供 NULL / uintN_t） */
#include <stddef.h>
#include <stdint.h>

/* 开三条链路当样本 —— 四条链路的收发内核逐字相同，三种"形态"各验一条就够：
 *   常规   ：周期 RX + 非周期 RX + 只发，daemon 故障判据成立；
 *   图传   ：**两条 RX 全是事件触发**（`freq_hz == 0`）⇒ `rx_periodic == 0`，
 *            故障动作被 Config 强制成 NONE（这是它非开不可的理由）；
 *   非链路 ：只发不收 ⇒ 不挂 daemon、不起接收。
 * 雷达链路的 .c 不参与本测试，它的表与快照由头文件里的 _Static_assert 把关
 * （那些断言在 ARM 构建里跑）。 */
#define DRV_REFEREE2026_USED
#define REFEREE2026_LINK_COMMON_USED
#define REFEREE2026_LINK_VIDEO_USED
#define REFEREE2026_LINK_NONE_USED
#define LIB_CRC_USED

#endif /* __APP_CFG_PC_STUB_H */
