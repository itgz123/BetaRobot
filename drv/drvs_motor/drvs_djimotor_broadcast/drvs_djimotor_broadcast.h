/**
 * @file drvs_djimotor_broadcast.h
 * @brief DJI 电机（M3508 / M2006 / GM6020）纯协议广播驱动（一拖四组帧）
 * @author TRW
 * @date 2026-09-21
 *
 * @note **纯协议**：本模块只负责"字节 ↔ 物理量"的映射和收发。
 *       - 发：把组内各电机的扭矩换算成电调电流原始值，按 rx_id 分组组帧下发
 *       - 收：把反馈帧解成位置/速度/电流/扭矩/温度/错误码
 *       不做闭环（PID 在 lib_axis 等算法层，app 负责把算法输出接到 SetRef）；
 *       不做滤波、不做方向取反、不做多圈累加、不做零点偏置——这些是"跨帧推断/策略"，
 *       不是协议，由 app 或算法层自己管。
 *
 * @note 无基类、无虚函数表：每个品牌自包含（自己的实例结构体 + 自己的函数），
 *       app 当胶水。命名带 Drvs 前缀是为了重构期间与旧 drv_motor 同树共存
 *       （避免符号冲突）；等 app 全部切到 drvs 后删掉旧 drv，再去掉前缀。
 *
 * @note 协议：CAN 标准帧 11 位 ID，8 字节帧，**大端**（MSB first）。
 *       控制帧一帧携带 4 个电机的 int16 电流原始值；反馈帧每电机各自回复。
 *
 * @note **分组策略（按 rx_id 分组，无接收 ID 冲突）**：
 *       - 组 0：rx_id 0x201~0x204（M3508/M2006 id1-4，tx 0x200）
 *       - 组 1：rx_id 0x205~0x208（M3508/M2006 id5-8 tx 0x1FF，或 GM6020 id1-4 tx 0x1FE）
 *       - 组 2：rx_id 0x209~0x20B（GM6020 id5-7，tx 0x2FE）
 *       组 1 可能同时挂两种 tx_id，故 GroupSend 按不同 tx_id 分别发帧（见 .c）。
 *
 * @note **本模块没有使能/失能接口**（广播模式无协议级使能帧）：
 *       扭矩完全由 SetRef 决定，停机 = SetRef(0) 后调一次 GroupSend。
 *       Config 会把 ref_torque 清零，重新绑定/换槽后必须重新 SetRef 才会再输出扭矩。
 */

#ifndef DRVS_DJIMOTOR_BROADCAST_H
#define DRVS_DJIMOTOR_BROADCAST_H

#include <stdint.h>

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_daemon.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DrvsDJIMotorBroadcast DrvsDJIMotorBroadcast_s;
typedef struct DrvsDJIMotorBroadcastGroup DrvsDJIMotorBroadcastGroup_s;

/* 0x200/0x204 系一帧 CAN 固定携带 4 个电机 */
#define DRVS_DJI_BC_SLOTS 4u

/*============================================
 *              型号枚举（模块自有）
 *
 * @note 不复用 drv_motor_base.h 的 DJIModel_e：那会拖进 MotorBase_s /
 *       虚函数表 / PID，破坏"无基类、纯协议"的约定。
 *       型号只决定"电调电流量程 + ID 基址 + tx_id"，是协议项。
 *============================================*/
typedef enum : uint8_t
{
    DRVS_DJI_MODEL_M3508 = 0, // C620 电调，±16384 raw = ±20 A
    DRVS_DJI_MODEL_M2006,     // C610 电调，±10000 raw = ±10 A
    DRVS_DJI_MODEL_GM6020,    // 内置电调，±16384 raw = ±3 A
    DRVS_DJI_MODEL_NUM,
} DrvsDJIModel_e;

/*============================================
 *              CAN 帧联合体定义
 *
 * @note 两帧都用于**文档化布局**：DJI 总线大端，字节序与 MCU 相反，
 *       故打包/解析一律走显式字节移位（见 .c），不经结构体成员写入。
 *============================================*/

/**
 * @brief DJI 反馈帧（8 字节，大端）
 *
 *   D[0..1]：编码器 uint16 大端（14 位有效，8192 分辨率）
 *   D[2..3]：转速 int16 大端（rpm）
 *   D[4..5]：电流 int16 大端（电调原始值）
 *   D[6]   ：温度 int8（℃）
 *   D[7]   ：错误码 uint8
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t encoder_h;  // [0]   编码器高字节
    uint8_t encoder_l;  // [1]   编码器低字节
    uint8_t velocity_h; // [2]   转速高字节
    uint8_t velocity_l; // [3]   转速低字节
    uint8_t current_h;  // [4]   电流高字节
    uint8_t current_l;  // [5]   电流低字节
    int8_t temperature; // [6]   温度 (℃)
    uint8_t error_code; // [7]   错误码
} DrvsDJIBroadcastRxFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsDJIBroadcastRxFrameParts_s parts;
} DrvsDJIBroadcastRxFrame_u;

_Static_assert(sizeof(DrvsDJIBroadcastRxFrameParts_s) == 8, "DJI 反馈帧必须是 8 字节");

/**
 * @brief DJI 控制帧（8 字节，大端）
 *
 *   D[0..1]：通道 1 电流 int16 大端 —— 对应槽位 0
 *   D[2..3]：通道 2 电流 int16 大端 —— 对应槽位 1
 *   D[4..5]：通道 3 电流 int16 大端 —— 对应槽位 2
 *   D[6..7]：通道 4 电流 int16 大端 —— 对应槽位 3
 *
 * @note 槽位 = (rx_id - 0x201) % 4，恰好等于该电机在本帧中的通道序号。
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t ch1_h; // [0] 通道1 电流高字节
    uint8_t ch1_l; // [1] 通道1 电流低字节
    uint8_t ch2_h; // [2]
    uint8_t ch2_l; // [3]
    uint8_t ch3_h; // [4]
    uint8_t ch3_l; // [5]
    uint8_t ch4_h; // [6]
    uint8_t ch4_l; // [7]
} DrvsDJIBroadcastTxFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsDJIBroadcastTxFrameParts_s parts;
} DrvsDJIBroadcastTxFrame_u;

_Static_assert(sizeof(DrvsDJIBroadcastTxFrameParts_s) == 8, "DJI 控制帧必须是 8 字节");

/* 槽位公式不变量：槽位 == 帧内通道序号（取两端钉死，防止改 group 基址时漏改公式） */
_Static_assert(((0x201u - 0x201u) % DRVS_DJI_BC_SLOTS) == 0u &&
                   ((0x208u - 0x201u) % DRVS_DJI_BC_SLOTS) == (DRVS_DJI_BC_SLOTS - 1u),
               "DJI 槽位公式不变量被破坏");

/*============================================
 *              协议映射结构体
 *============================================*/
/**
 * @brief 协议映射（Config 时按型号 + 用户 Kt 预计算，存进实例）
 * @note  预计算是为热路径省掉浮点除法（Cortex-M7 FPU 除法约 14 周期）
 */
typedef struct
{
    float torque_constant; // Kt (Nm/A)，用户标定
    float raw_max;         // 电调电流原始值量程（±raw_max），发送限幅用

    float a_per_lsb;      // = current_max_a / raw_max，电流原始值 → A
    float nm_per_lsb;     // = Kt * a_per_lsb，电流原始值 → Nm
    float inv_nm_per_lsb; // = 1 / nm_per_lsb，Nm → 电流原始值

    float encoder_to_rad; // = 2π / 8192，编码器原始值 → rad
    float rpm_to_radps;   // = 2π / 60，转速 rpm → rad/s
} DrvsDJIMotorBroadcastProtocolMap_s;

/*============================================
 *              反馈数据结构体
 *============================================*/
/**
 * @brief 一帧反馈解出来的全部内容
 * @note  都是**协议原始量**：未累加、未偏置、未取反、未滤波。
 *        position 是单圈绝对值，只在 [0, 2π) 一个机械圈内（14 位编码器回绕），
 *        需要多圈累加请由 app/算法层自己做。
 */
typedef struct
{
    float position;        // 单圈位置 (rad)，[0, 2π)
    float speed;           // 转速 (rad/s)
    float torque;          // 扭矩 (Nm) = 电流原始值 × Kt
    float current;         // 电调电流 (A) = 电流原始值 × a_per_lsb
    int8_t temperature;    // 温度 (℃)
    uint8_t error;         // 错误码（0=正常）
    uint64_t timestamp_us; // CAN 帧到达时间戳 (us)
} DrvsDJIMotorBroadcastData_s;

/*============================================
 *              DJI 广播电机实例结构体
 *============================================*/
struct DrvsDJIMotorBroadcast
{
    /* 通信 */
    CANInstance *can;        // CAN 实例指针
    CAN_Filter_s can_filter; // CAN 接收过滤器
    DaemonInstance *daemon;  // 守护进程实例（通信在线检测）
    uint32_t timeout_ms;     // CAN 发送超时 (ms)
    /* 无 tx_fail：单电机实例没有发送口（发送挂在组上），失败计数只在 Group 的 tx_fail 里 */

    /* 标识 */
    DrvsDJIModel_e model; // 型号（决定电流量程与 ID 基址）
    uint8_t motor_id;     // 电机 ID（M3508/M2006: 1~8；GM6020: 1~7）
    uint16_t rx_id;       // = rx_id_base + motor_id，反馈帧 ID
    uint16_t tx_id;       // 控制帧 ID（同族 id1-4 与 id5-8 不同）

    /* 协议映射 */
    DrvsDJIMotorBroadcastProtocolMap_s proto_map;

    /* 控制量（纯扭矩，无使能开关） */
    float ref_torque; // 设定扭矩 (Nm)，由 SetRef 写入，GroupSend 打包下发

    /* 组链接 */
    DrvsDJIMotorBroadcastGroup_s *group; // 所属组（Config 绑定，NULL=未绑定）
    uint8_t slot;                        // 帧内槽位 = (rx_id - 0x201) % 4

    /* 接收双缓冲：ISR 解析写一个，GetData 读另一个，读者永远拿到完整的一帧 */
    DrvsDJIMotorBroadcastData_s data[2]; // data[data_idx] = ISR 正在写的
    volatile uint8_t data_idx;           // 当前 ISR 写入的缓冲区索引 (0/1)
};

/*============================================
 *              广播组结构体
 *
 * @note 组是 **app 自己的变量**（static 定义后传给 Config）：
 *       一条 CAN 总线一组，组内最多 4 个电机共用一帧控制报文。
 *       发送挂在组上（DrvsDJIMotorBroadcastGroupSend），单电机实例没有 Send，
 *       这样"一组只调一次"由函数名表达，也避免了"调用了某个成员却发了整组"的错觉。
 *============================================*/
struct DrvsDJIMotorBroadcastGroup
{
    DrvsDJIMotorBroadcast_s *slots[DRVS_DJI_BC_SLOTS]; // 槽位占用表，NULL=空槽
    uint8_t member_count;                              // 已占用槽位数，0 表示尚未绑定总线
    BoardCAN_e can_e;                                  // 首个成员确定，后续成员必须一致
    uint32_t tx_fail;                                  // 组播帧发送失败累计：CANTransmit 入队失败（返回非 BSP_OK）
                                                       // + 逐帧失败（tx_complete_callback 报 result != BSP_OK，
                                                       // 见 .c 的 TxHook）。一帧带 4 个电机，失败只记在组上，不摊到各电机
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief DJI 广播电机配置（Config 函数使用）
 * @note 只能在 Register 之后调用；可重复调用修改型号/ID/Kt/超时等。
 *       tx_id / rx_id / 槽位均由 model + motor_id 推导，不单独暴露。
 * @note 组归属（group）也是配置项：一条总线一个组，组成员必须同 can_e。
 */
typedef struct
{
    DrvsDJIMotorBroadcastGroup_s *group; // 广播组（app 持有的变量，一条总线一个）

    BoardCAN_e can_e; // 板载 CAN 枚举（用于查找硬件映射）

    DrvsDJIModel_e model; // 型号
    uint8_t motor_id;     // 电机 ID（M3508/M2006: 1~8；GM6020: 1~7）

    float torque_constant; // 转矩常数 (Nm/A)，必须 > 0

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值），0=禁用
    DaemonFaultAction_e fault_action; // 离线故障动作

    uint32_t timeout_ms; // CAN 发送超时 (ms)
} DrvsDJIMotorBroadcastConfig_s;

/*============================================
 *              单电机实例 / 广播组定义宏
 *============================================*/
#define DRVS_DJIMOTOR_BROADCAST_INSTANCE_DEF(name) \
    CAN_INSTANCE_DEF(name##_can);                  \
    DAEMON_INSTANCE_DEF(name##_daemon);            \
    static DrvsDJIMotorBroadcast_s name = {        \
        .can = &name##_can,                        \
        .daemon = &name##_daemon,                  \
    }

/**
 * @brief 定义 app 持有的广播组对象（一条总线一个）
 * @note  先定义再传给 `cfg.group`。
 *        定义出来即全零：四个槽位皆空、成员数为 0，can_e 由第一个成员的 Config 落定。
 */
#define DRVS_DJIMOTOR_BROADCAST_GROUP_DEF(name) \
    static DrvsDJIMotorBroadcastGroup_s name = {0}

/*============================================
 *              公共接口
 *============================================*/
int8_t DrvsDJIMotorBroadcastRegister(DrvsDJIMotorBroadcast_s *inst);

/**
 * @brief 配置电机并挂到广播组（可重复调用）
 * @param inst 电机实例
 * @param cfg  配置（含组归属 cfg->group，app 持有的组对象，一条总线一个）
 * @retval 0 成功；-1 参数非法 / 组总线不一致 / 槽位被他人占用 / CANConfig 失败
 * @note  槽位规则：空 → 占；属于自己 → 原地更新；属于别人 → -1
 * @note  换组/换槽会先释放旧槽位（旧组该槽位改为空 → 该槽位后续下发 0 扭矩）
 * @note  任何失败路径都不会改动组成员关系
 */
int8_t DrvsDJIMotorBroadcastConfig(DrvsDJIMotorBroadcast_s *inst,
                                   const DrvsDJIMotorBroadcastConfig_s *cfg);

/**
 * @brief 设置扭矩设定值 (Nm)
 * @note  仅记录，不下发；需再调一次 DrvsDJIMotorBroadcastGroupSend()
 * @note  方向取反（安装镜像）由调用方自己做，本模块不做 motor_direction
 */
void DrvsDJIMotorBroadcastSetRef(DrvsDJIMotorBroadcast_s *inst, float torque);

/**
 * @brief 发送一组控制帧（每个控制周期对该组调用一次）
 * @note  组内按出现过的不同 tx_id 分别发帧（组 1 可混挂 id5-8 与 GM6020）；
 *        不属于该 tx_id 的槽位一律填 0，未占用槽位填 0。
 * @note  重复调用 = 重复占总线，一个控制周期只调一次。
 * @note  组内成员数为 0 时直接返回。
 */
void DrvsDJIMotorBroadcastGroupSend(DrvsDJIMotorBroadcastGroup_s *group);

/**
 * @brief 取最新一帧反馈（在任务上下文调用）
 * @note  RX 回调（ISR）已解析好写进双缓冲，这里只读"另一个"缓冲区，
 *        不解析、不推断、不滤波，也不会阻塞 ISR。
 * @note  收到第一帧之前返回全 0；要判"是否收到过"看 timestamp_us == 0
 */
DrvsDJIMotorBroadcastData_s DrvsDJIMotorBroadcastGetData(const DrvsDJIMotorBroadcast_s *inst);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif // DRVS_DJIMOTOR_BROADCAST_H
