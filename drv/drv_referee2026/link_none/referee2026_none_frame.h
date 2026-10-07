/**
 * @file referee2026_none_frame.h
 * @brief 非链路 1 条命令的**数据名枚举（兼数组下标）+ 命令元信息表**

 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）表 1-5 命令码 ID 一览里
 *       "所属数据链路 = 非链路"的 1 行（组内保持表 1-5 顺序）；数据段字段表见 §1.5 自定义控制器 / 附录一（表 1-43）。

 * 本文件只回答"这条链路上有哪些命令、各自的元信息是什么"，两样东西：
 *   ① `Referee2026NoneDataId_e` —— 数据名，**取值是顺序的 0..0**，末项 `_COUNT`，
 *      取值即 `referee2026_none_cmd_info[]` 的下标；
 *   ② `referee2026_none_cmd_info[]` —— 每条命令的
 *      `{cmd_id, data_len, receiver, freq_hz, freq_max_hz}`。

 * **表里不存所属链路**：链路由"本文件就是非链路"隐含，{@link Referee2026CmdInfo_t} 若再存一份
 * 就是同一个事实写两遍。数据段结构体在 referee2026_none_cmd.h；本头**不 include** 它（只有 .c 才需要，
 * 因为 `REFEREE2026_NONE_LEN_*` 宏才是 data_len 的出处）。

 * 字段语义（data_len 的字段表口径、频率两列的三种取值、receiver 位掩码、3 处总表/字段表冲突）
 * 统一写在 core/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。

 * @note 本文件只依赖 <stdint.h> 与 core/referee2026_cmd.h（后者同样只依赖 <stdint.h>），PC 端可
 *       单独语法检查，与 core/referee2026_frame.h 的"零依赖"约定一脉相承。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_NONE_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报未定义
 *       —— 这是设计使然：没编进来的链路本就不该被用。
 */

#ifndef __REFEREE2026_NONE_FRAME_H
#define __REFEREE2026_NONE_FRAME_H

#include <stdint.h>

#include "referee2026_cmd.h" /* Referee2026CmdInfo_t / Referee2026Receiver_e */

/*============================================
 *   一、数据名（取值即 referee2026_none_cmd_info[] 的下标）
 *============================================*/

/**
 * @brief 非链路 1 条命令的数据名
 * @note 成员后缀逐字沿用原 core 命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note **只给首项标 `= 0`，其余交给编译器自增**：这样取值必然连续无洞，不会出现"手写漏了一号"。
 *       顺序错位也不会串表 —— .c 里用的是指定初始化器 `[成员] = ...`。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；对应长度以 `REFEREE2026_NONE_LEN_<TAG>` 宏为准
 *       （定义在 referee2026_none_cmd.h，与结构体长度断言共用同一个数）。
 */
typedef enum : uint8_t
{
    /* --- 非链路（1 条）：自定义控制器 ↔ 选手端直连，0x0306 --- */
    REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA = 0, //!< 自定义控制器与选手端交互数据，发送方触发发送，频率上限为 30Hz
    REFEREE2026_NONE_DATA_COUNT,                  //!< 命令条数（数组维度，非命令）
} Referee2026NoneDataId_e;

/*============================================
 *   二、命令元信息表（按 Referee2026NoneDataId_e 取用）
 *============================================*/

extern const Referee2026CmdInfo_t referee2026_none_cmd_info[REFEREE2026_NONE_DATA_COUNT];

#endif /* __REFEREE2026_NONE_FRAME_H */
