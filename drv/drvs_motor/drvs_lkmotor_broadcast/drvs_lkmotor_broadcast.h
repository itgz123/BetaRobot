/**
 * @file drvs_lkmotor_broadcast.h
 * @brief LK（瓴控/翎控）MF 系列纯协议广播驱动（一拖四组帧）
 * @author TRW
 * @date 2026-09-21
 *
 * @note **纯协议**：本模块只负责"字节 ↔ 物理量"的映射和收发。
 *       - 发：把组内各电机的扭矩换算成 0x280 的 iq 原始值，组一帧 4 槽位下发
 *       - 收：把状态2 反馈帧解成位置/速度/扭矩/电流/温度
 *       不做闭环（PID 在 lib_axis 等算法层，app 负责把算法输出接到 SetRef）；
 *       不做滤波、不做方向取反、不做多圈累加、不做零点偏置——这些是"跨帧推断/策略"，
 *       不是协议，由 app 或算法层自己管。
 *
 * @note 无基类、无虚函数表：每个品牌自包含（自己的实例结构体 + 自己的函数），
 *       app 当胶水。命名带 Drvs 前缀是为了重构期间与旧 drv_motor 同树共存
 *       （避免符号冲突）；等 app 全部切到 drvs 后删掉旧 drv，再去掉前缀。
 *
 * @note 协议：广播模式一拖四，标准帧 11 位 ID，8 字节帧。
 *       - 发送：固定 ID 0x280，4 × int16 **小端** iqControl，第 id 号电机占 data[(id-1)*2 .. +1]
 *       - 接收：各电机仍以 0x140 + id 单独回复「状态2」帧（与一对一**逐字节相同**）
 *
 * @note ⚠️ 只有 0xA0~0xA8 控制命令与 0x9C 的回复是「状态2」格式；
 *       全零回显帧必须过滤，否则位置/速度会被误读为 0 造成跳变。ISR 里按回显字节过滤。
 *
 * @note **广播模式无模式命令**（无 0x80/0x88/0x9C 的主动下发），
 *       因此本模块**没有使能/失能接口**：扭矩完全由 SetRef 决定，
 *       停机 = SetRef(0) 后调一次 GroupSend。Config 会把 ref_torque 清零。
 *
 * @note MF 为直驱（无减速机），反馈与下发均为电机轴量，驱动内严禁乘减速比。
 *
 * @note 本文件与一对一驱动 drvs_lkmotor.{c,h} 完全独立（类型/常量自持），
 *       二者协议不兼容，切勿在同一电机上混用；两者可同时编进同一个 app。
 */

#ifndef DRVS_LKMOTOR_BROADCAST_H
#define DRVS_LKMOTOR_BROADCAST_H

#include <stdint.h>

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_daemon.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DrvsLKMotorBroadcast DrvsLKMotorBroadcast_s;
typedef struct DrvsLKMotorBroadcastGroup DrvsLKMotorBroadcastGroup_s;

/* 0x280 帧内的槽位数（一拖四） */
#define DRVS_LK_BC_SLOTS 4u

/*============================================
 *              CAN 帧联合体定义
 *============================================*/

/**
 * @brief LK 状态2 回复帧（8 字节，小端）
 *
 *   D[0]    ：命令回显（0xA1 / 0x9C）
 *   D[1]    ：温度 int8（℃）
 *   D[2..3] ：iq int16 小端（A/LSB 见 DrvsLKMotorBroadcastProtocolMap_s）
 *   D[4..5] ：转速 int16 小端（dps）
 *   D[6..7] ：编码器 uint16 小端（整圈 0~65535）
 *
 * @note 与一对一模块的帧格式逐字节相同，但类型独立定义：
 *       两个模块可以同时编进同一个 app，头文件级符号必须可区分。
 * @note 回显字节是唯一区分「状态2 帧」和「全零回显帧」的依据，解析前必须校验。
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t echo;         // [0]   命令回显
    int8_t temperature;   // [1]   温度 (℃)
    int16_t iq_le;        // [2-3] iq int16 小端
    int16_t speed_dps_le; // [4-5] 转速 int16 小端 (dps)
    uint16_t encoder_le;  // [6-7] 编码器 uint16 小端
} DrvsLKBroadcastStatusFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsLKBroadcastStatusFrameParts_s parts;
} DrvsLKBroadcastStatusFrame_u;

_Static_assert(sizeof(DrvsLKBroadcastStatusFrameParts_s) == 8, "LK 状态帧必须是 8 字节");

/**
 * @brief LK 0x280 力矩广播帧（8 字节，小端）
 *
 *   D[0..1]：电机 1 的 iqControl int16 小端 —— 槽位 0（motor_id = 1）
 *   D[2..3]：电机 2 —— 槽位 1
 *   D[4..5]：电机 3 —— 槽位 2
 *   D[6..7]：电机 4 —— 槽位 3
 *
 * @note 槽位 = motor_id - 1。打包走显式字节写（见 .c），结构体仅用于文档化布局。
 */
#pragma pack(push, 1)
typedef struct
{
    int16_t torque_id1_le; // [0-1] 电机 1（槽位 0）
    int16_t torque_id2_le; // [2-3] 电机 2（槽位 1）
    int16_t torque_id3_le; // [4-5] 电机 3（槽位 2）
    int16_t torque_id4_le; // [6-7] 电机 4（槽位 3）
} DrvsLKBroadcastTorqueFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsLKBroadcastTorqueFrameParts_s parts;
} DrvsLKBroadcastTorqueFrame_u;

_Static_assert(sizeof(DrvsLKBroadcastTorqueFrameParts_s) == 8, "LK 广播力矩帧必须是 8 字节");

/*============================================
 *              协议映射结构体
 *============================================*/
/**
 * @brief 协议映射（Config 时根据 cfg 预计算，存进实例）
 * @note  预计算是为热路径省掉浮点除法（Cortex-M7 FPU 除法约 14 周期）
 */
typedef struct
{
    float torque_constant; // Kt (Nm/A)，用户标定（直驱，无减速比）
    float a_per_lsb;       // iq 分辨率 (A/LSB)，MF 固定 33/4096
    float nm_per_lsb;      // = Kt * a_per_lsb，raw → Nm
    float inv_nm_per_lsb;  // = 1 / nm_per_lsb，Nm → raw
    float encoder_to_rad;  // = 2π / 65536，编码器 raw → rad
    float dps_to_radps;    // = π / 180，转速 dps → rad/s
} DrvsLKMotorBroadcastProtocolMap_s;

/*============================================
 *              反馈数据结构体
 *============================================*/
/**
 * @brief 一帧状态2 解出来的全部内容
 * @note  都是**协议原始量**：未累加、未偏置、未取反、未滤波。
 *        position 是单圈绝对值，只在 [0, 2π) 一个机械圈内（编码器整圈回绕），
 *        需要多圈累加请由 app/算法层自己做。
 */
typedef struct
{
    float position;        // 单圈位置 (rad)，[0, 2π)
    float speed;           // 转速 (rad/s)
    float torque;          // 扭矩 (Nm) = iq × Kt
    float current;         // q 轴电流 (A) = iq_raw × a_per_lsb
    int8_t temperature;    // 温度 (℃)
    uint8_t echo;          // 回显命令字（0xA1 或 0x9C）
    uint64_t timestamp_us; // CAN 帧到达时间戳 (us)
} DrvsLKMotorBroadcastData_s;

/*============================================
 *              LK 广播电机实例结构体
 *============================================*/
struct DrvsLKMotorBroadcast
{
    /* 通信 */
    CANInstance *can;        // CAN 实例指针
    CAN_Filter_s can_filter; // CAN 接收过滤器
    DaemonInstance *daemon;  // 守护进程实例（通信在线检测）
    uint32_t timeout_ms;     // CAN 发送超时 (ms)

    /* 标识 */
    uint8_t motor_id; // 电机 ID 1~4（一拖四），回复 ID = 0x140 + motor_id
    uint16_t rx_id;   // = 0x140 + motor_id，反馈帧 ID

    /* 协议映射 */
    DrvsLKMotorBroadcastProtocolMap_s proto_map;

    /* 控制量（纯扭矩，无使能开关） */
    float ref_torque; // 设定扭矩 (Nm)，由 SetRef 写入，GroupSend 打包下发

    /* 组链接 */
    DrvsLKMotorBroadcastGroup_s *group; // 所属组（Config 绑定，NULL=未绑定）
    uint8_t slot;                       // = motor_id - 1，0x280 帧内槽位

    /* 接收双缓冲：ISR 解析写一个，GetData 读另一个，读者永远拿到完整的一帧 */
    DrvsLKMotorBroadcastData_s data[2]; // data[data_idx] = ISR 正在写的
    volatile uint8_t data_idx;          // 当前 ISR 写入的缓冲区索引 (0/1)
};

/*============================================
 *              广播组结构体
 *
 * @note 组是 **app 自己的变量**（static 定义后传给 Config）：
 *       一条 CAN 总线一组，组内最多 4 个电机共用那一帧 0x280。
 *       发送挂在组上（DrvsLKMotorBroadcastGroupSend），单电机实例没有 Send，
 *       这样"一组只调一次"由函数名表达，也避免了"调用了某个成员却发了整组"的错觉。
 *============================================*/
struct DrvsLKMotorBroadcastGroup
{
    DrvsLKMotorBroadcast_s *slots[DRVS_LK_BC_SLOTS]; // 槽位 = motor_id-1，NULL=空槽
    uint8_t member_count;                            // 已占用槽位数，0 表示尚未绑定总线
    BoardCAN_e can_e;                                // 首个成员确定，后续成员必须一致
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief LK 广播电机配置（Config 函数使用）
 * @note 只能在 Register 之后调用；可重复调用修改 ID/Kt/超时等。
 * @note 发送 ID 固定 0x280、回复 ID 由 motor_id 推导，均不单独暴露。
 * @note 无 model 字段：广播模式只支持 MF（MS/MH/MG 无 0x280 转矩广播语义）。
 * @note 组归属（group）也是配置项：一条总线一个组，组成员必须同 can_e。
 */
typedef struct
{
    DrvsLKMotorBroadcastGroup_s *group; // 广播组（app 持有的变量，一条总线一个）

    BoardCAN_e can_e; // 板载 CAN 枚举（用于查找硬件映射）

    uint8_t motor_id; // 电机 ID 1~4（一拖四），回复 ID = 0x140 + motor_id，槽位 = ID-1

    /* 物理量换算参数（iq 分辨率与编码器量纲对全系列 MF 固定，见驱动内常量） */
    float torque_constant; // 转矩常数 (Nm/A)，必须 > 0（直驱，无减速比）

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值），0=禁用
    DaemonFaultAction_e fault_action; // 离线故障动作

    uint32_t timeout_ms; // CAN 发送超时 (ms)
} DrvsLKMotorBroadcastConfig_s;

/*============================================
 *              单电机实例 / 广播组定义宏
 *============================================*/
#define DRVS_LKMOTOR_BROADCAST_INSTANCE_DEF(name) \
    CAN_INSTANCE_DEF(name##_can);                 \
    DAEMON_INSTANCE_DEF(name##_daemon);           \
    static DrvsLKMotorBroadcast_s name = {        \
        .can = &name##_can,                       \
        .daemon = &name##_daemon,                 \
    }

/**
 * @brief 定义 app 持有的广播组对象（一条总线一个）
 * @note  先定义再传给 `cfg.group`。
 *        定义出来即全零：四个槽位皆空、成员数为 0，can_e 由第一个成员的 Config 落定。
 */
#define DRVS_LKMOTOR_BROADCAST_GROUP_DEF(name) \
    static DrvsLKMotorBroadcastGroup_s name = {0}

/*============================================
 *              公共接口
 *============================================*/
int8_t DrvsLKMotorBroadcastRegister(DrvsLKMotorBroadcast_s *inst);

/**
 * @brief 配置电机并挂到广播组（可重复调用）
 * @param inst 电机实例
 * @param cfg  配置（含组归属 cfg->group，app 持有的组对象，一条总线一个）
 * @retval 0 成功；-1 参数非法 / 组总线不一致 / 槽位被他人占用 / CANConfig 失败
 * @note  槽位规则：空 → 占；属于自己 → 原地更新；属于别人 → -1
 * @note  换组/换槽会先释放旧槽位（旧组该槽位改为空 → 该槽位后续下发 0 扭矩）
 * @note  任何失败路径都不会改动组成员关系
 */
int8_t DrvsLKMotorBroadcastConfig(DrvsLKMotorBroadcast_s *inst,
                                  const DrvsLKMotorBroadcastConfig_s *cfg);

/**
 * @brief 设置扭矩设定值 (Nm)
 * @note  仅记录，不下发；需再调一次 DrvsLKMotorBroadcastGroupSend()
 * @note  方向取反（安装镜像）由调用方自己做，本模块不做 motor_direction
 */
void DrvsLKMotorBroadcastSetRef(DrvsLKMotorBroadcast_s *inst, float torque);

/**
 * @brief 发送一组 0x280 力矩广播帧（每个控制周期对该组调用一次）
 * @note  未占用槽位填 0（零扭矩）。
 * @note  重复调用 = 重复占总线，一个控制周期只调一次。
 * @note  组内成员数为 0 时直接返回。
 */
void DrvsLKMotorBroadcastGroupSend(DrvsLKMotorBroadcastGroup_s *group);

/**
 * @brief 取最新一帧反馈（在任务上下文调用）
 * @note  RX 回调（ISR）已过滤回显并解析好写进双缓冲，这里只读"另一个"缓冲区，
 *        不解析、不推断、不滤波，也不会阻塞 ISR。
 * @note  收到第一帧有效状态2 之前返回全 0；要判"是否收到过"看 timestamp_us == 0
 */
DrvsLKMotorBroadcastData_s DrvsLKMotorBroadcastGetData(const DrvsLKMotorBroadcast_s *inst);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif // DRVS_LKMOTOR_BROADCAST_H
