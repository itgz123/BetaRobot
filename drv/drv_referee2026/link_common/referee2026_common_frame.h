/**
 * @file referee2026_common_frame.h
 * @brief 常规链路 24 条命令的**数据名枚举（兼数组下标）+ 命令元信息表**

 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）表 1-5 命令码 ID 一览里
 *       "所属数据链路 = 常规链路"的 24 行（组内保持表 1-5 顺序）；数据段字段表见 §1.2 常规链路数据说明（表 1-6 ~ 表
 1-38）。

 * 本文件只回答"这条链路上有哪些命令、各自的元信息是什么"，两样东西：
 *   ① `Referee2026CommonDataId_e` —— 数据名，**取值是顺序的 0..23**，末项 `_COUNT`，
 *      取值即 `referee2026_common_cmd_info[]` 的下标；
 *   ② `referee2026_common_cmd_info[]` —— 每条命令的
 *      `{cmd_id, data_len, receiver, freq_hz, freq_max_hz}`。

 * **表里不存所属链路**：链路由"本文件就是常规链路"隐含，{@link Referee2026CmdInfo_t} 若再存一份
 * 就是同一个事实写两遍。数据段结构体在 referee2026_common_cmd.h；本头**不 include** 它（只有 .c 才需要，
 * 因为 `REFEREE2026_COMMON_LEN_*` 宏才是 data_len 的出处）。

 * 字段语义（data_len 的字段表口径、频率两列的三种取值、receiver 位掩码、3 处总表/字段表冲突）
 * 统一写在 core/referee2026_cmd.h 的 {@link Referee2026CmdInfo_t} 上方，本文件不重复。

 * @note 本文件只依赖 <stdint.h> 与 core/referee2026_cmd.h（后者同样只依赖 <stdint.h>），PC 端可
 *       单独语法检查，与 core/referee2026_frame.h 的"零依赖"约定一脉相承。
 * @note 头文件**无条件声明**；"会不会真编出这张表"由 .c 里的 `DRV_REFEREE2026_USED` &&
 *       `REFEREE2026_LINK_COMMON_USED` 决定。未启用的链路若有人引用本数组，会在**链接期**报未定义
 *       —— 这是设计使然：没编进来的链路本就不该被用。
 */

#ifndef __REFEREE2026_COMMON_FRAME_H
#define __REFEREE2026_COMMON_FRAME_H

#include <stdint.h>

#include "referee2026_cmd.h" /* Referee2026CmdInfo_t / Referee2026Receiver_e */

/*============================================
 *   一、数据名（取值即 referee2026_common_cmd_info[] 的下标）
 *============================================*/

/**
 * @brief 常规链路 24 条命令的数据名
 * @note 成员后缀逐字沿用原 core 命令一览表的 tag，便于与官方表 1-5 逐行对照。
 * @note **只给首项标 `= 0`，其余交给编译器自增**：这样取值必然连续无洞，不会出现"手写漏了一号"。
 *       顺序错位也不会串表 —— .c 里用的是指定初始化器 `[成员] = ...`。
 * @note 行尾注释逐字抄自表 1-5 的"说明"列；对应长度以 `REFEREE2026_COMMON_LEN_<TAG>` 宏为准
 *       （定义在 referee2026_common_cmd.h，与结构体长度断言共用同一个数）。
 */
typedef enum : uint8_t
{
    /* --- 常规链路（24 条） --- */
    REFEREE2026_COMMON_DATA_GAME_STATUS = 0, //!< 比赛状态数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_GAME_RESULT,     //!< 比赛结果数据，比赛结束触发发送
    REFEREE2026_COMMON_DATA_GAME_ROBOT_HP,   //!< 机器人血量数据，固定以 3Hz 频率发送
    REFEREE2026_COMMON_DATA_EVENT_DATA,      //!< 场地事件数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_REFEREE_WARNING, //!< 裁判警告数据，己方判罚/判负时触发发送，其余时间以 1Hz 频率发送（固定
                                             //!< 1Hz + 事件插播）
    REFEREE2026_COMMON_DATA_DART_LAUNCH,     //!< 飞镖发射相关数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_ROBOT_STATUS,    //!< 机器人性能体系数据，固定以 10Hz 频率发送
    REFEREE2026_COMMON_DATA_POWER_HEAT,      //!< 实时底盘缓冲能量和射击热量数据，固定以 10Hz 频率发送
    /* 0x0203：总表 16B / 字段表 12B，见上方"长度自相矛盾的处理" */
    REFEREE2026_COMMON_DATA_ROBOT_POS,         //!< 机器人位置数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_BUFF,              //!< 机器人增益和底盘能量数据，固定以 3Hz 频率发送
    REFEREE2026_COMMON_DATA_ROBOT_HURT,        //!< 伤害状态数据，伤害发生后发送
    REFEREE2026_COMMON_DATA_SHOOT_DATA,        //!< 实时射击数据，弹丸发射后发送
    REFEREE2026_COMMON_DATA_PROJECTILE_ALLOW,  //!< 允许发弹量与剩余金币数，固定以 10Hz 频率发送
    REFEREE2026_COMMON_DATA_RFID_STATUS,       //!< 机器人 RFID 模块状态，固定以 3Hz 频率发送
    REFEREE2026_COMMON_DATA_DART_CLIENT_CMD,   //!< 飞镖选手端指令数据，固定以 3Hz 频率发送
    REFEREE2026_COMMON_DATA_GROUND_ROBOT_POS,  //!< 地面机器人位置数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_RADAR_MARK,        //!< 雷达标记进度数据，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_SENTRY_INFO,       //!< 哨兵自主决策信息同步，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_RADAR_INFO,        //!< 雷达自主决策信息同步，固定以 1Hz 频率发送
    REFEREE2026_COMMON_DATA_ROBOT_INTERACTION, //!< 机器人交互数据，发送方触发发送，频率上限为 30Hz
    /* 0x0303：总表 15B / 字段表 12B，见上方"长度自相矛盾的处理" */
    REFEREE2026_COMMON_DATA_MINI_MAP_INTERACT, //!< 选手端小地图交互数据，选手端触发发送
    REFEREE2026_COMMON_DATA_MINI_MAP_RADAR,    //!< 选手端小地图接收雷达数据，频率上限为 5Hz
    /* 0x0307：总表 103B / 字段表 105B —— 三条不一致里唯一"总表更小"的一条 */
    REFEREE2026_COMMON_DATA_MINI_MAP_PATH,  //!< 选手端小地图接收路径数据，频率上限为 1Hz
    REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, //!< 选手端小地图接收机器人数据，频率上限为 3Hz
    REFEREE2026_COMMON_DATA_COUNT,          //!< 命令条数（数组维度，非命令）
} Referee2026CommonDataId_e;

/*============================================
 *   二、命令元信息表（按 Referee2026CommonDataId_e 取用）
 *============================================*/

extern const Referee2026CmdInfo_t referee2026_common_cmd_info[REFEREE2026_COMMON_DATA_COUNT];

#endif /* __REFEREE2026_COMMON_FRAME_H */
