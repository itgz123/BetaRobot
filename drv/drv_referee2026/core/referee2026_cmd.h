/**
 * @file referee2026_cmd.h
 * @brief 2026 赛季裁判系统**跨链路公共数据段结构体**
 *
 * 本文件只放**两条以上链路共用**的组成块。按链路分文件后，各条命令的数据段在：
 *   link_common/referee2026_common_cmd.h  常规链路 24 条 + 0x0301 的 8 种子内容
 *   link_video/referee2026_video_cmd.h    图传链路 4 条
 *   link_none/referee2026_none_cmd.h      非链路 1 条（0x0306）
 *   link_radar/referee2026_radar_cmd.h    雷达无线链路 6 条
 * 四个链路头都 include 本文件 —— 公共块只有这一份定义，**链路之间不互相依赖**。
 * 命令码与每条命令的元信息（长度 / 接收方 / 频率）见各链路自己的 frame 头：
 *   link_common/referee2026_common_frame.h    link_video/referee2026_video_frame.h
 *   link_none/referee2026_none_frame.h        link_radar/referee2026_radar_frame.h
 * 那是"数据名枚举（`Referee2026<Link>DataId_e`）+ 元信息数组"，按链路分家。
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）
 *
 * 公共约定（**四个链路文件同守，改动前先看这里**）：
 *   - 所有结构体都**紧凑**（`#pragma pack(push, 1)` … `pop`）：可以拿帧数据段的指针直接
 *     `(const Referee2026Xxx_t *)payload` 强转读（Cortex-M4/M7 支持非对齐访问），
 *     也可以 `memcpy` 进一个本地结构体。**别把它们按值传参** —— packed 结构体复制会逐字节。
 *   - 多字节标量一律**小端**（与 frame.h 的 `data_length` 同）。
 *   - 位域类字段用仓库既有写法（见 drv_dbus/drv_dbus.h 的 `DBUS_Key_t`）：**位域结构体与原始
 *     标量组成 union**，位域成员给可读性、`value` 给整体赋值/拷贝/比较。
 *     `: N` 写错只影响可读性、**不动字节布局**（布局由 union 的标量成员兜底）；但"漏写字段 /
 *     漏加数组下标"会实打实改 `sizeof` —— 故各链路文件末尾都拿 `_Static_assert` 逐条钉死。
 *   - 位域宽度的自检**只拦得住"多写"、拦不住"少写"**（写满 30 位与写满 32 位 sizeof 都是 4），
 *     故每个位域结构体末尾都显式补 `reserved` 把位填满到标量宽。
 *
 * @warning 官方文档本身有 3 处"总表 data_length 与字段表对不上"和 2 处结构体笔误，
 *          处理方式见各结构体的 `@warning`。**一律以"字节偏移量字段表"为准**
 *          （它是逐字段列出来的，是最可信的原始信息）。
 */

#ifndef __REFEREE2026_CMD_H
#define __REFEREE2026_CMD_H

#include <stdint.h>

#include "referee2026_frame.h" /* Referee2026Receiver_e（第二节的元信息结构要用） */

#pragma pack(push, 1)

/*============================================
 *   一、跨链路公共组成块
 *============================================*/

/**
 * @brief 机器人增益项（7B）—— 0x0204 的前 7 个字节，也是 0x0A05 里 5 个增益块之一的格式
 * @note 两处**逐字节同构**（回血 1B、热量冷却 2B、防御 1B、负防御 1B、攻击 2B），
 *       故共用一个类型；0x0204 只是在它后面多挂了一个 `remaining_energy`。
 */
typedef struct
{
    uint8_t recovery_buff;      //!< 回血增益（百分比，10 表示每秒恢复血量上限的 10%）
    uint16_t cooling_buff;      //!< 射击热量冷却增益具体值（直接值，x 表示冷却增加 x/s）
    uint8_t defence_buff;       //!< 防御增益（百分比，50 表示 50% 防御增益）
    uint8_t vulnerability_buff; //!< 负防御增益（百分比，30 表示 -30% 防御增益）
    uint16_t attack_buff;       //!< 攻击增益（百分比，50 表示 50% 攻击增益）
} Referee2026BuffItem_t;

#pragma pack(pop)

/*============================================
 *   二、命令元信息（跨链路同形，四个链路 frame 共用）
 *============================================*/

/**
 * @brief 一条命令的元信息 —— 官方表 1-5 的「数据段长度 / 接收方 / 说明列里的频率」
 *
 * 四个链路各有一张本类型的数组（`referee2026_<link>_cmd_info[]`），用各链路的
 * "数据名枚举"（`Referee2026<Link>DataId_e`）当下标取用。表里**没有 tag 也没有链路**：
 *   - tag  → 由"数组下标的那个枚举成员名"承担；
 *   - link → 由"表在哪个链路文件里"隐含 —— 同一个事实不写两遍。
 *
 * 【data_len 的口径】取**"字节偏移量字段表"的合计**，也就是数据段结构体的 `sizeof`
 * （各 `<link>_cmd.h` 里的 `REFEREE2026_<LINK>_LEN_<TAG>` 宏唯一给出，长度断言与下表共用它）。
 * 官方"表 1-5 命令码一览"的总表值只当**包络**，有 3 处与字段表对不上，一律以字段表为准：
 *   0x0203 总表 16B / 字段表 12B；0x0303 总表 15B / 字段表 12B；0x0307 总表 103B / 字段表 105B。
 * 真机上一帧的数据段长度一律**以帧头 `data_length` 为准**，本字段只作"最小可接受长度"。
 *
 * 【频率两列怎么读】表 1-5 的"说明"列把频率写成散文，这里拆成两个整数，三种取值：
 *   freq_hz = N, freq_max_hz = N  → 固定 N Hz 周期发送（0x0201 机器人性能体系等）；
 *   freq_hz = 0, freq_max_hz = N  → 触发发送 + 速率上限 N Hz（0x0311 自定义客户端指令等）；
 *   freq_hz = 0, freq_max_hz = 0  → 纯事件触发，协议没给频率（0x0002 比赛结束等）。
 * 固定发送的行两列同值 —— "上限"就是它本身，于是 `freq_max_hz` 可以**无条件**当速率预算用
 * （不必先判是不是触发式）。全部 35 条里速率最紧的是 0x0311，上限 75Hz。
 * 例外：0x0104 裁判警告是"判罚/判负时提前发一帧，其余时间 1Hz"，即固定 1Hz + 事件插播，填 (1, 1)。
 * 频率**不参与帧编解码**，本模块也不按它限速 —— 这两列是给上层算带宽预算与超时判据用的。
 *
 * @note **不加 `#pragma pack`**：本结构体是编译期元信息、不做线上字节覆盖，自然对齐即可
 *       （各链路都是 `const` 数组，落 .rodata）。别照本文件第一节的通则给它顺手加 packed。
 * @note 表项里的 0 **不是可靠哨兵**：`REFEREE2026_RX_NONE` 就是 0、`freq_hz`/`freq_max_hz`
 *       的 0 也都是合法取值。故"漏填一项"只能靠 `cmd_id` / `data_len` 非零来兜。
 */
typedef struct
{
    uint16_t cmd_id;                //!< 命令码（表 1-5 第 1 列）
    uint16_t data_len;              //!< 数据段长度（字段表口径，单位 B；见上方说明）
    Referee2026Receiver_e receiver; //!< 接收方位掩码（表 1-5 第 4 列 "→" 右侧）
    uint8_t freq_hz;                //!< 固定发送频率 Hz；0 = 非固定周期
    uint8_t freq_max_hz;            //!< 触发发送时的频率上限 Hz；0 = 无速率约束
} Referee2026CmdInfo_t;

/*============================================
 *   三、长度自校验
 *============================================*/

/* 本文件只有一块，直接钉死；链路上各命令的断言在各自的 cmd 文件里。 */
_Static_assert(sizeof(Referee2026BuffItem_t) == 7, "公共增益块长度不符");

#endif /* __REFEREE2026_CMD_H */
