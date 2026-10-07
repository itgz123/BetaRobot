/**
 * @file referee2026_none_cmd.h
 * @brief 非链路 1 条命令的数据段（0x0306 自定义控制器 ↔ 选手端直连）
 *
 * 依据：RoboMaster 2026 高校系列赛通信协议 V2.0.0 §1.5 自定义控制器 / 附录一（表 1-43）。
 * 命令码与元信息（长度 / 接收方 / 频率）见 referee2026_none_frame.h 的
 * `Referee2026NoneDataId_e`；元信息表的定义在同名的 .c 里。
 *
 * 公共约定（packed / 小端 / 位域 union 写法 / 文档笔误口径）见 core/referee2026_cmd.h ——
 * 那里放着跨链路的公共组成块，本文件 include 它。
 * 每条命令的**数据段长度**以 `REFEREE2026_NONE_LEN_<TAG>` 宏为准（紧挨对应结构体上方，值取
 * 官方"字节偏移量字段表"的合计），文件末尾的长度断言与 referee2026_none_frame.c 的元信息表
 * **共用同一个宏** —— 一处定义、两处引用，改长度只改宏。
 */

#ifndef __REFEREE2026_NONE_CMD_H
#define __REFEREE2026_NONE_CMD_H

#include <stdint.h>

#include "referee2026_cmd.h"

#pragma pack(push, 1)

/*============================================
 *   一、命令数据段（1 条）
 *============================================*/

/* 0x0306 数据段长度（字段表口径）—— 长度断言与命令元信息表共用，只写这一处。 */
#define REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA 8

/**
 * @brief 0x0306 自定义控制器 ↔ 选手端直连数据（表 1-43，文档名 `custom_client_data_t`）
 *
 * @note 这条**不占任何链路**：自定义控制器与选手端图传模块**直连**，因此走
 *       `REFEREE2026_LINK_TYPE_NONE`、接收方是己方选手端。
 * @note 鼠标操作要求**三帧一组**：先发"未按下 + 指定位置"、再发"该位置 + 按下"、
 *       最后发"该位置 + 未按下"，依次描述一次移动点击。
 */
typedef struct
{
    uint16_t key_value;       //!< 偏移 0 键盘按键通用键值
    uint16_t x_position : 12; //!< 偏移 2 鼠标 x 坐标
    uint16_t mouse_left : 4;  //!< 同一 16 位字的高 4 位：鼠标左键状态
    uint16_t y_position : 12; //!< 偏移 4 鼠标 y 坐标
    uint16_t mouse_right : 4; //!< 同一 16 位字的高 4 位：鼠标右键状态
    uint16_t reserved;        //!< 偏移 6 保留位
} Referee2026CustomClientData_t;

#pragma pack(pop)

/*============================================
 *   长度自校验
 *============================================*/

/* 逐条把 sizeof 钉死在"字节偏移量字段表"给出的总长上（理由见 core/referee2026_cmd.h 的公共约定）。 */
#define REFEREE2026_CMD_STATIC_ASSERT(cmd_id, ...)                                                                     \
    _Static_assert(__VA_ARGS__, "命令 0x" #cmd_id " 的数据段长度与字段表不符")

REFEREE2026_CMD_STATIC_ASSERT(0x0306, sizeof(Referee2026CustomClientData_t) == REFEREE2026_NONE_LEN_CUSTOM_CLIENT_DATA);

#undef REFEREE2026_CMD_STATIC_ASSERT

#endif /* __REFEREE2026_NONE_CMD_H */
