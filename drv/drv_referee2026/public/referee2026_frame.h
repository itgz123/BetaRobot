/**
 * @file referee2026_frame.h
 * @brief 2026 赛季 RoboMaster 裁判系统**协议模型**：串口帧格式 / 四种链路档 / ID 编号
 *        （纯定义，零依赖）
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）
 *   §1.1 串口协议格式（表 1-1 波特率 / 表 1-2 通信协议格式 / 表 1-3 frame_header / 表 1-4 偏移）
 *   表 1-5 命令码 ID 一览的"所属数据链路"列
 *   （命令码一览本身已按链路拆到各 link 目录的 referee2026_<link>.h，本文件不再收录）
 *   附录二 ID 编号说明
 *
 * 本文件只回答"协议长什么样"，三样东西在这里一次写清、别处只管引用：
 *   ① 帧格式：Referee2026FrameOffset_e / Referee2026FrameConst_e（表 1-2/1-3/1-4）
 *   ② 数据链路类型：Referee2026LinkType_e（§1.1 正文，三种 + "非链路"一档）
 *   ③ ID 编号：Referee2026Color_e（阵营偏移 + 选手端 ID 基址）× Referee2026RobotType_e
 *      → 拼出机器人 ID 与选手端 ID + 换算函数
 * 命令码与每条命令的元信息（数据段长度 / 方向 / 发送频率）**不在本文件** —— 已按链路拆进
 * link_common / link_video / link_none / link_radar 各自的 referee2026_<link>.h。
 *
 * 表 1-5 的"发送方/接收方"列曾以位掩码（`Referee2026Receiver_e`）收在这里，现已删除：
 * 它运行期零消费者，而"这条命令发不发得出去"由各链路元信息表的 `dir` 字段回答。原语义逐条
 * 记在各链路数据名枚举成员的行尾注释里（`发送方→接收方`）。
 *
 * 本文件**只依赖 <stdint.h>**：不 include HAL / bsp / lib / app_cfg，PC 端可单独编译自检。
 * 需要 HAL 与 lib_crc 的对接层（串口外设配置、CRC 算法与查表）在 referee2026_proto.h。
 *
 * 帧格式（表 1-2 / 1-3 / 1-4）：
 *   frame_header(5B): SOF=0xA5(1) | data_length(u16 LE)(2) | seq(1) | CRC8(1)
 *   cmd_id(2B LE)
 *   data(data_length B)
 *   frame_tail(2B LE): CRC16（整包校验）
 *   两个校验的参数与查表见 referee2026_proto.h —— 那里才需要 HAL/lib。
 */

#ifndef __REFEREE2026_FRAME_H
#define __REFEREE2026_FRAME_H

#include <stdint.h>

/*============================================
 *      一、串口帧格式（§1.1 + 表 1-2 ~ 1-4）
 *============================================*/

/**
 * @brief 0xA5 帧的字节偏移（表 1-2 整帧布局 / 表 1-4 帧头偏移）
 * @note 帧尾 CRC16 紧跟在数据段之后，偏移 = REFEREE2026_OFF_DATA + data_length；
 *       数据段长度取自帧头 data_length（REFEREE2026_OFF_DATA_LEN，小端）。
 */
typedef enum : uint8_t
{
    REFEREE2026_OFF_SOF = 0,      //!< 1B  起始字节 0xA5
    REFEREE2026_OFF_DATA_LEN = 1, //!< 2B  LE，数据段长度（不含帧头/命令码/帧尾）
    REFEREE2026_OFF_SEQ = 3,      //!< 1B  包序号
    REFEREE2026_OFF_CRC8 = 4,     //!< 1B  帧头校验，覆盖前 4B
    REFEREE2026_OFF_CMD_ID = 5,   //!< 2B  LE，命令码 ID
    REFEREE2026_OFF_DATA = 7,     //!< nB  数据段
} Referee2026FrameOffset_e;

/**
 * @brief 帧层长度常量（表 1-2 / 1-3；供数组维度与静态断言使用）
 * @note DATA_MAX 取 0x0310 的数据段长度 300 —— 它是全协议最长的一条。
 *       0xA9 图传遥控帧（21B 定长，见 link_video）也须 ≤ FRAME_MAX。
 */
typedef enum : uint16_t
{
    REFEREE2026_SOF = 0xA5,      //!< 起始字节
    REFEREE2026_HEADER_SIZE = 5, //!< SOF + data_length(2) + seq(1) + CRC8(1)
    REFEREE2026_CMD_ID_SIZE = 2, //!< cmd_id 长度
    REFEREE2026_TAIL_SIZE = 2,   //!< CRC16 长度
    REFEREE2026_DATA_MAX = 300,  //!< 数据段长度上限（0x0310）
    //! 最大整帧 = 帧头 + cmd_id + 数据段 + 帧尾 = 309B
    REFEREE2026_FRAME_MAX = REFEREE2026_HEADER_SIZE + REFEREE2026_CMD_ID_SIZE + REFEREE2026_DATA_MAX + REFEREE2026_TAIL_SIZE,
} Referee2026FrameConst_e;

/*============================================
 *   二、三种数据链路类型（§1.1 正文）
 *============================================*/

/**
 * @brief 数据链路类型
 *
 * 协议 §1.1 正文原话："裁判系统串口数据链路有三种：常规链路、图传链路、雷达无线链路。"
 *   常规链路    ：裁判系统服务器与主控模块转发，从**电源管理模块（PM02）的 User 串口**收发（115200）
 *   图传链路    ：裁判系统选手端与图传模块转发，从**图传发送端的串口**接收（921600）
 *   雷达无线链路：裁判系统信号发射源发送，**雷达接收电磁波**（不走串口）
 * 三条链路**物理上互不相干**（接在车上不同部件上），故"一个实例只绑一条链路"。
 *
 * @note _NONE 不是第四条链路，而是表 1-5 里"所属数据链路"一列写作 `-` 的那一档：
 *       0x0306 自定义控制器 ↔ 选手端，直连串口（115200），既不经过服务器也不经过图传。
 *       收录它是为了让命令码一览表能如实覆盖表 1-5 的全部 35 行。
 * @note 本枚举只描述**协议里的链路语义**；固件侧的编译开关是 `REFEREE2026_LINK_XXX_USED`
 *       （常规/图传/雷达三条，定义权在 app_cfg.h，定义 = 编进来）。
 */
typedef enum : uint8_t
{
    REFEREE2026_LINK_TYPE_COMMON = 0, //!< 常规链路（电源管理模块 PM02 的 User 串口）
    REFEREE2026_LINK_TYPE_VIDEO = 1,  //!< 图传链路（图传发送端串口，另有 0xA9 遥控帧）
    REFEREE2026_LINK_TYPE_RADAR = 2,  //!< 雷达无线链路（电磁波，无串口）
    REFEREE2026_LINK_TYPE_NONE = 3,   //!< 不属于上述三条（0x0306 直连串口）
    REFEREE2026_LINK_TYPE_COUNT = 4,  //!< 枚举个数（链路信息表 / 串口配置表的维度）
} Referee2026LinkType_e;

/*============================================
 *      三、ID 编号说明（附录二）
 *============================================*/

/**
 * @brief 阵营（附录二）——**同时是选手端 ID 基址表**
 *
 * 附录二里"红/蓝"这一个维度索引着两张表，故由本枚举一并承载：
 *   ① 机器人 ID 偏移：红方 0（ID = 类型本身，1~11）、蓝方 100（ID = 类型 + 100，101~111）。
 *      **蓝方的枚举值（100）就是阵营偏移本身**，故拼 ID / 拆 ID 都是一次加法，偏移没有第二个出处。
 *   ② 选手端 ID 基址：`选手端 ID = 基址 + 类型`，与 ① 的加法同构；
 *      红方 0x0100 + 类型 1~6 = 0x0101~0x0106，蓝方 0x0164 + 类型 = 0x0165~0x016A。
 *      两条基址也只差阵营偏移：`0x0164 = 0x0100 + 100`，故蓝方基址由红方基址加 COLOR_BLUE 得出。
 *
 * @note 基址取 **0x0100 / 0x0164**，而不是附录二表里第一个选手端的值（0x0101）：
 *       "基址"是加类型之前的那个底，这样 `基址 + 类型` 与 `类型 + 阵营` 写法完全一致，
 *       换算函数不必再减一。附录二列出的 0x0101~0x0106 就是 base + 类型的结果。
 * @note 只有英雄/工程/步兵 3~5/空中这 6 类配了选手端（类型 1~6），判据见 Referee2026RobotTypeHasClient。
 * @note `_CLIENT_ID_SERVER` 是唯一不属任何阵营的成员：它只是与两条基址同处"客户端 ID"域的
 *       第三个地址（哨兵/雷达的自主决策指令发给它），放这里是为了不再单开一个只有一项的枚举。
 */
typedef enum : uint16_t
{
    REFEREE2026_COLOR_RED = 0,    //!< 红方：机器人 ID = 类型本身，1~11
    REFEREE2026_COLOR_BLUE = 100, //!< 蓝方：机器人 ID = 类型 + 100，101~111

    REFEREE2026_CLIENT_ID_RED_BASE = 0x0100, //!< 红方选手端 ID 基址（+ 类型 1~6 → 0x0101~0x0106）
    //! 蓝方选手端 ID 基址（0x0164 = 红方基址 + 阵营偏移，故不写死第二个常数）
    REFEREE2026_CLIENT_ID_BLUE_BASE = REFEREE2026_CLIENT_ID_RED_BASE + REFEREE2026_COLOR_BLUE,

    REFEREE2026_CLIENT_ID_SERVER = 0x8080, //!< 裁判系统服务器（哨兵/雷达的自主决策指令发给它）
} Referee2026Color_e;

/**
 * @brief 机器人类型（附录二）——**枚举值取红方编号**
 * @note 蓝方同类型机器人的 ID = 本值 + REFEREE2026_COLOR_BLUE。
 * @note 光有类型不足以定位一台机器人（红蓝同型两台），线上要的是「阵营+类型」拼出的 ID；
 *       反之，ID 也能拆回阵营与类型（Referee2026ColorOfRobot / Referee2026TypeOfRobot）。
 */
typedef enum : uint8_t
{
    REFEREE2026_ROBOT_TYPE_HERO = 1,      //!< 英雄
    REFEREE2026_ROBOT_TYPE_ENGINEER = 2,  //!< 工程
    REFEREE2026_ROBOT_TYPE_INFANTRY3 = 3, //!< 3 号步兵
    REFEREE2026_ROBOT_TYPE_INFANTRY4 = 4, //!< 4 号步兵
    REFEREE2026_ROBOT_TYPE_INFANTRY5 = 5, //!< 5 号步兵
    REFEREE2026_ROBOT_TYPE_AERIAL = 6,    //!< 空中
    REFEREE2026_ROBOT_TYPE_SENTRY = 7,    //!< 哨兵
    REFEREE2026_ROBOT_TYPE_DART = 8,      //!< 飞镖
    REFEREE2026_ROBOT_TYPE_RADAR = 9,     //!< 雷达
    REFEREE2026_ROBOT_TYPE_BASE = 10,     //!< 基地
    REFEREE2026_ROBOT_TYPE_OUTPOST = 11,  //!< 前哨站
} Referee2026RobotType_e;

/* 选手端 ID 不再单开枚举：它 = Referee2026Color_e 的基址 + Referee2026RobotType_e，
 * 两条轴由上面两个枚举给全；换算函数见文件末尾的 Referee2026ClientIdOfRobot。
 * 红方 0x0100+1~6 = 0x0101~0x0106、蓝方 0x0164+1~6 = 0x0165~0x016A，与机器人 ID 1~6 / 101~106 一一对应。 */

/*============================================
 *   四、ID 换算函数（附录二）
 *============================================*/

/* 放在文件末尾：这一节是纯派生逻辑，一眼看完前面的枚举/表就够用了。
 * 五个函数只互相调用、只碰本文件的枚举，不依赖 HAL/lib，故 PC 端可单独编译自检。
 * 参数一律取 uint16_t 机器人 ID —— 它就是帧里那个 robot_id 字段的宽度。 */

/**
 * @brief 由「阵营 + 类型」拼出线上机器人 ID（附录二）
 * @note 协议里收发双方看到的就是这个 16 位 ID；阵营/类型是它的两个维度。
 *       阵营枚举值本身就是偏移，故拼接就是一次加法。
 * @example `Referee2026RobotIdOf(REFEREE2026_COLOR_BLUE, REFEREE2026_ROBOT_TYPE_INFANTRY5)` → 105
 */
static inline uint16_t Referee2026RobotIdOf(Referee2026Color_e color, Referee2026RobotType_e type)
{
    return (uint16_t)((uint16_t)type + (uint16_t)color);
}

/**
 * @brief 拆出 ID 的阵营维度（附录二）
 * @note 红方 1~11、蓝方 101~111 ⇒ 不小于蓝方阵营值即蓝方。ID 非法（如 0、100）时按红方返回，
 *       不做额外校验（上游拿到的 ID 来自协议字段，本来就是可信的）。
 */
static inline Referee2026Color_e Referee2026ColorOfRobot(uint16_t robot_id)
{
    return (robot_id >= REFEREE2026_COLOR_BLUE) ? REFEREE2026_COLOR_BLUE : REFEREE2026_COLOR_RED;
}

/**
 * @brief 拆出 ID 的类型维度（附录二）
 * @return 类型；ID 非法（去掉阵营偏移后不在 1~11 内）时返回 0 —— 不匹配任何枚举项，故调用方
 *         一律按 "0 = 非法"处理，不必再判一遍范围（Referee2026RobotTypeHasClient 对 0 返回 0）。
 */
static inline Referee2026RobotType_e Referee2026TypeOfRobot(uint16_t robot_id)
{
    uint8_t type = (uint8_t)(robot_id % REFEREE2026_COLOR_BLUE); // 蓝方阵营值 100 即阵营偏移

    return (type >= REFEREE2026_ROBOT_TYPE_HERO && type <= REFEREE2026_ROBOT_TYPE_OUTPOST) ? (Referee2026RobotType_e)type : (Referee2026RobotType_e)0;
}

/**
 * @brief 该类型有没有配套的选手端（附录二）
 * @retval 0 没有（哨兵/飞镖/雷达/基地/前哨站，以及非法类型 0）
 * @retval 1 有（英雄/工程/步兵 3~5/空中）
 * @note 附录二只给这 6 类机器人配了选手端，故能换算出选手端 ID 的只有它们。
 */
static inline uint8_t Referee2026RobotTypeHasClient(Referee2026RobotType_e type)
{
    return (type >= REFEREE2026_ROBOT_TYPE_HERO && type <= REFEREE2026_ROBOT_TYPE_AERIAL) ? 1u : 0u;
}

/**
 * @brief 由机器人 ID 推己方选手端 ID（附录二）
 * @param robot_id 机器人 ID
 * @return 对应的选手端 ID；该机器人没有选手端（哨兵/飞镖/雷达/基地/前哨站）或 ID 非法时返回 0
 * @note 0x0301 里 UI / 自定义消息的 receiver_id 必须是己方选手端 ID，调用方普遍需要这个换算。
 */
static inline uint16_t Referee2026ClientIdOfRobot(uint16_t robot_id)
{
    Referee2026RobotType_e type = Referee2026TypeOfRobot(robot_id);

    if (!Referee2026RobotTypeHasClient(type))
    {
        return 0; // 哨兵/飞镖/雷达/基地/前哨站没有选手端；类型非法（0）也已由 HasClient 挡住
    }

    uint16_t base =
        (Referee2026ColorOfRobot(robot_id) == REFEREE2026_COLOR_BLUE) ? (uint16_t)REFEREE2026_CLIENT_ID_BLUE_BASE : (uint16_t)REFEREE2026_CLIENT_ID_RED_BASE;

    /* 与 Referee2026RobotIdOf 同构：基址 + 类型，一次加法（基址已含"减一"）。 */
    return (uint16_t)(base + (uint16_t)type);
}

#endif /* __REFEREE2026_FRAME_H */
