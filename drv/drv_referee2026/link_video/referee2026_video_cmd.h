/**
 * @file referee2026_video_cmd.h
 * @brief 图传链路 4 条命令的数据段（0x0302 / 0x0309 / 0x0310 / 0x0311）
 *
 * 依据：RoboMaster 2026 高校系列赛通信协议 V2.0.0 §1.4 图传链路数据说明（表 1-39 ~ 表 1-42）。
 * 命令码与元信息（长度 / 接收方 / 频率）见 referee2026_video_frame.h 的
 * `Referee2026VideoDataId_e`；元信息表的定义在同名的 .c 里。
 *
 * 公共约定（packed / 小端 / 位域 union 写法 / 文档笔误口径）见 core/referee2026_cmd.h ——
 * 那里放着跨链路的公共组成块，本文件 include 它。
 * 每条命令的**数据段长度**以 `REFEREE2026_VIDEO_LEN_<TAG>` 宏为准（紧挨对应结构体上方，值取
 * 官方"字节偏移量字段表"的合计），文件末尾的长度断言与 referee2026_video_frame.c 的元信息表
 * **共用同一个宏** —— 一处定义、两处引用，改长度只改宏。
 */

#ifndef __REFEREE2026_VIDEO_CMD_H
#define __REFEREE2026_VIDEO_CMD_H

#include <stdint.h>

#include "referee2026_cmd.h"

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（4 条）
 *============================================*/

/* 0x0302 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER 30

/**
 * @brief 0x0302 自定义控制器与机器人交互数据（表 1-39，文档名 `custom_robot_data_t`）
 * @note 内容是"自定义控制器"自定义的，本模块不认识，按裸字节透传。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026CustomRobotData_t;

/* 0x0309 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX 30

/**
 * @brief 0x0309 机器人向自定义控制器发送的数据（文档名 `robot_custom_data_t`）
 * @note 与 0x0302 方向相反、格式同样由使用者自定义。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026RobotCustomData_t;

/* 0x0310 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT 300

/**
 * @brief 0x0310 机器人向自定义客户端发送的数据（文档名 `robot_custom_data_2_t`）
 * @note 图传链路里最长的一条，纯透传。
 */
typedef struct
{
    uint8_t data[300]; //!< 自定义数据，最长 300B
} Referee2026RobotCustomData2_t;

/* 0x0311 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT 30

/**
 * @brief 0x0311 自定义客户端向机器人发送的数据（文档名同为 `robot_custom_data_t`）
 * @note 官方文档与 0x0309 用了**同名结构体**，这里按命令码方向另起名以便区分。
 */
typedef struct
{
    uint8_t data[30]; //!< 自定义数据，最长 30B
} Referee2026ClientToRobotData_t;

#pragma pack(pop)

/*============================================
 *   长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 core/referee2026_cmd.h 的公共约定）。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...)                                                                     \
    _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0302, sizeof(Referee2026CustomRobotData_t) == REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER);
REFEREE2026_CMD_STATIC_ASSERT(0x0309,
                              sizeof(Referee2026RobotCustomData_t) == REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER_RX);
REFEREE2026_CMD_STATIC_ASSERT(0x0310, sizeof(Referee2026RobotCustomData2_t) == REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT);
REFEREE2026_CMD_STATIC_ASSERT(0x0311, sizeof(Referee2026ClientToRobotData_t) == REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT);

#undef REFEREE2026_CMD_STATIC_ASSERT

#endif /* __REFEREE2026_VIDEO_CMD_H */
