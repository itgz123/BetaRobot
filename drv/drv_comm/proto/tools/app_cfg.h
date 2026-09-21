/**
 * @file app_cfg.h
 * @brief PC 端测试桩（仅供 drv/drv_comm/proto/tools 的 gcc 测试使用）
 *
 * comm_proto.h 会 #include "app_cfg.h"，真板上的那个会拉进 bsp_map.h / robot_def.h /
 * cubemx 头，PC 编译不动。本文件同目录先于任何 -I 路径被引用（引号包含先查包含者所在目录），
 * 只提供测试所需的开关宏。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __APP_CFG_PC_STUB_H
#define __APP_CFG_PC_STUB_H

/* 真 app_cfg.h 经 bsp_map.h / robot_def.h 间接带入的标准头，桩里要自己补上
 * （模块实现依赖它们提供 NULL / uintN_t） */
#include <stddef.h>
#include <stdint.h>

#define DRV_COMM_USED       /* 测试 comm_proto_ext 的协议层 */
#define LIB_HAMMING_USED    /* 汉明码纠错 */
#define LIB_CRC_USED        /* CRC8 */
#define LIB_CRC_TABLES_USED /* CRC Flash 表（走 TableCalc 路径，与固件一致） */

#endif /* __APP_CFG_PC_STUB_H */
