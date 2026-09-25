/**
 * @file drvs_lkmotor.h
 * @brief LK（瓴控/翎控）MF 系列一体化直驱电机纯协议驱动（一对一 / 单电机点对点）
 * @author TRW
 * @date 2026-09-21
 *
 * @note **纯协议**：本模块只负责"字节 ↔ 物理量"的映射和收发。
 *       - 发：把扭矩换算成 iq，按 0xA1 转矩闭环帧下发
 *       - 收：把状态2 帧解成位置/速度/电流/温度
 *       - 模式命令：开启 / 关闭 / 停止 / 清错 / 读状态（SendCmd，见 DrvsLKMotorModeCmd_e）
 *       不做闭环（PID 在 lib_axis 等算法层，app 负责把算法输出接到 SetRef）；
 *       不做滤波、不做方向取反、不做多圈累加、不做零点偏置——这些是"跨帧推断/策略"，
 *       不是协议，由 app 或算法层自己管。
 *
 * @note **本模块不持有使能状态**：没有 Enable()/Disable()，也不记 enable 标志。
 *       想让电机转 = app 自己调 `SendCmd(inst, DRVS_LK_CMD_ENABLE)`（发 0x88）；
 *       想停 = `SetRef(inst, 0)` 并调 `SendCmd(inst, DRVS_LK_CMD_DISABLE)`（发 0x80）。
 *       模块不替 app 决定"该不该使能"，故 Send 也无失能早退（协议层由电机自己拒绝）。
 *
 * @note 无基类、无虚函数表：每个品牌自包含（自己的实例结构体 + 自己的函数），
 *       app 当胶水。命名带 Drvs 前缀是为了重构期间与旧 drv_motor 同树共存
 *       （避免符号冲突）；等 app 全部切到 drvs 后删掉旧 drv，再去掉前缀。
 *
 * @note 协议：一对一 CAN 总线通讯协议 V2.36，标准帧 11 位 ID，默认 1Mbps，8 字节帧。
 *       CAN ID = 0x140 + 电机ID(1~32)，命令与回复同 ID，无应用层 CRC。
 *
 * @note ⚠️ 只有 0xA0~0xA8 控制命令与 0x9C 的回复是「状态2」格式；
 *       0x80/0x88/0x81 的回复是「与主机发送相同」的全零回显，必须过滤，
 *       否则位置/速度会被误读为 0 造成跳变。本模块在 ISR 里按回显字节过滤。
 *
 * @note MF 为直驱（无减速机），反馈与下发均为电机轴量，驱动内严禁乘减速比。
 *
 * @note 本项目在上位机做控制，用 0xA1（转矩闭环）下发 iq；发 0xA1 后电机回复状态2 格式，
 *       故无需额外轮询。对外只暴露扭矩一个接口（Nm → iq 用 cfg 的 torque_constant）。
 *
 * @note 首版仅支持 MF 系列；MS 系列无转矩闭环（仅开环 power）且无相电流采样，暂不实现。
 *
 * @note 广播（一拖四）是另一套帧格式，见旧驱动 drv_lkmotor_broadcast；本模块只做一对一。
 */

#ifndef DRVS_LKMOTOR_H
#define DRVS_LKMOTOR_H

#include <stdint.h>

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_daemon.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DrvsLKMotor DrvsLKMotor_s;

/*============================================
 *              CAN 帧联合体定义
 *============================================*/

/**
 * @brief LK 状态2 / 控制回复帧（8 字节，小端）
 *
 *   D[0]    ：命令回显（0xA1 / 0x9C）
 *   D[1]    ：温度 int8（℃）
 *   D[2..3] ：iq int16 小端（A/LSB 见 DrvsLKMotorProtocolMap_s）
 *   D[4..5] ：转速 int16 小端（dps）
 *   D[6..7] ：编码器 uint16 小端（整圈 0~65535）
 *
 * @note 与 DM/RS 的区别：没有 12 位跨字节位域，全是整字节有符号/无符号小端量。
 * @note 回显字节是唯一区分"状态2 帧"和"全零回显帧"的依据，解析前必须校验。
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t echo;         // [0]   命令回显
    int8_t temperature;   // [1]   温度 (℃)
    int16_t iq_le;        // [2-3] iq int16 小端
    int16_t speed_dps_le; // [4-5] 转速 int16 小端 (dps)
    uint16_t encoder_le;  // [6-7] 编码器 uint16 小端
} DrvsLKMotorStatusFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsLKMotorStatusFrameParts_s parts;
} DrvsLKMotorStatusFrame_u;

_Static_assert(sizeof(DrvsLKMotorStatusFrameParts_s) == 8, "LK 状态帧必须是 8 字节");

/**
 * @brief LK 转矩闭环控制帧（0xA1，8 字节）
 *
 *   D[0]   ：0xA1 命令字
 *   D[1..3]：0x00
 *   D[4..5]：iqControl int16 小端（-2048 ~ 2048）
 *   D[6..7]：0x00
 *
 * @note 与 DM/RS 的 MIT 帧完全不同：这里没有位置/速度/PD，只有电流。
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t cmd;        // [0]   命令字 0xA1
    uint8_t _reserved0; // [1]
    uint8_t _reserved1; // [2]
    uint8_t _reserved2; // [3]
    int16_t iq_le;      // [4-5] iqControl int16 小端
    uint8_t _reserved3; // [6]
    uint8_t _reserved4; // [7]
} DrvsLKMotorControlFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsLKMotorControlFrameParts_s parts;
} DrvsLKMotorControlFrame_u;

_Static_assert(sizeof(DrvsLKMotorControlFrameParts_s) == 8, "LK 控制帧必须是 8 字节");

/*============================================
 *              模式命令枚举
 * @note 帧格式：DATA[0]=命令码，其余字节 0x00（与 DM/RS 的前 7 字节 0xFF 不同）
 *============================================*/
typedef enum : uint8_t
{
    DRVS_LK_CMD_DISABLE = 0x80,      // 关闭：开启→关闭，清除电机内部圈数及旧指令，仍回复但不动作
    DRVS_LK_CMD_STOP = 0x81,         // 停止（不清除运行状态，可再次发控制指令）
    DRVS_LK_CMD_ENABLE = 0x88,       // 开启：关闭→开启（上电默认为开启）
    DRVS_LK_CMD_CLEAR_ERROR = 0x9B,  // 清除错误标志
    DRVS_LK_CMD_READ_STATUS2 = 0x9C, // 读取电机状态2（温度/iq/转速/编码器）
    DRVS_LK_CMD_TORQUE = 0xA1,       // 转矩闭环（仅 MF/MH/MG 实现），DATA[4..5]=iqControl
} DrvsLKMotorModeCmd_e;

/*============================================
 *              运行状态枚举（记录的已下达状态，非电机回读）
 *============================================*/
typedef enum : uint8_t
{
    DRVS_LK_STATE_DISABLED = 0, // 已关闭（0x80 后）
    DRVS_LK_STATE_ENABLED = 1,  // 已开启（0x88 后，上电默认为此）
} DrvsLKMotorState_e;

/*============================================
 *              协议映射结构体
 *============================================*/
/**
 * @brief 协议映射（初始化时根据 cfg 预计算，存进实例）
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
} DrvsLKMotorProtocolMap_s;

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
} DrvsLKMotorData_s;

/*============================================
 *              LK 电机实例结构体
 *============================================*/
struct DrvsLKMotor
{
    /* 通信 */
    CANInstance *can;        // CAN 实例指针
    CAN_Filter_s can_filter; // CAN 接收过滤器
    DaemonInstance *daemon;  // 守护进程实例（通信在线检测）
    uint32_t timeout_ms;     // CAN 发送超时 (ms)
    uint32_t tx_fail;        // 发送失败累计：CANTransmit 入队失败（返回非 BSP_OK）+ 逐帧失败
                             // （tx_complete_callback 报 result != BSP_OK，见 .c 的 TxHook）；只增不清，调试用

    /* 标识（一对一：tx 与 rx 同一 ID） */
    uint8_t motor_id; // 电机 ID 1~32（由 cfg 给出）
    uint16_t can_id;  // = 0x140 + motor_id，收发共用

    /* 协议映射 */
    DrvsLKMotorProtocolMap_s proto_map;

    /* 控制量（纯扭矩） */
    float ref_torque; // 设定扭矩 (Nm)，由 SetRef 写入，Send 换算成 iq 下发

    /* 接收双缓冲：ISR 解析写一个，GetData 读另一个，读者永远拿到完整的一帧 */
    DrvsLKMotorData_s data[2]; // data[data_idx] = ISR 正在写的
    volatile uint8_t data_idx; // 当前 ISR 写入的缓冲区索引 (0/1)
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief LK 电机配置（Config 函数使用）
 * @note 只能在 Register 之后调用；可重复调用修改参数。
 * @note CAN ID 由 motor_id 推导（0x140 + motor_id），不单独暴露。
 */
typedef struct
{
    BoardCAN_e can_e; // 板载 CAN 枚举（用于查找硬件映射）

    uint8_t motor_id; // 电机 ID 1~32

    /* 物理量换算参数（iq 分辨率与编码器量纲对全系列 MF 固定，见驱动内常量） */
    float torque_constant; // 转矩常数 (Nm/A)，必须 > 0（直驱，无减速比）

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值），0=禁用
    DaemonFaultAction_e fault_action; // 离线故障动作

    uint32_t timeout_ms; // CAN 发送超时 (ms)
} DrvsLKMotorConfig_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define DRVS_LKMOTOR_INSTANCE_DEF(name)                                                                                \
    CAN_INSTANCE_DEF(name##_can);                                                                                      \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                \
    static DrvsLKMotor_s name = {                                                                                      \
        .can = &name##_can,                                                                                            \
        .daemon = &name##_daemon,                                                                                      \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t DrvsLKMotorRegister(DrvsLKMotor_s *inst);
int8_t DrvsLKMotorConfig(DrvsLKMotor_s *inst, const DrvsLKMotorConfig_s *cfg);

/**
 * @brief 设置扭矩设定值 (Nm)
 * @note  仅记录，不发送；需再调 DrvsLKMotorSend()
 * @note  方向取反（安装镜像）由调用方自己做，本模块不做 feedback/motor_direction
 */
void DrvsLKMotorSetRef(DrvsLKMotor_s *inst, float torque);

/**
 * @brief 打包并发送一帧 0xA1 转矩闭环帧
 * @note  iq_raw = clamp(ref_torque / (Kt × a_per_lsb), ±2048)
 * @note  只要没被 SendCmd 关闭过，电机自己会接受这帧（模块不做软件拦截）
 */
void DrvsLKMotorSend(DrvsLKMotor_s *inst);

/**
 * @brief 取最新一帧反馈（在任务上下文调用）
 * @note  RX 回调（ISR）已过滤回显并解析好写进双缓冲，这里只读"另一个"缓冲区，
 *        不解析、不推断、不滤波，也不会阻塞 ISR。
 * @note  收到第一帧有效状态2 之前返回全 0；要判"是否收到过"看 timestamp_us == 0
 */
DrvsLKMotorData_s DrvsLKMotorGetData(const DrvsLKMotor_s *inst);

/**
 * @brief 发送模式命令（开启/关闭/停止/清错/读状态），命令码见 DrvsLKMotorModeCmd_e
 * @note  帧格式：DATA[0]=命令码，其余字节 0x00
 */
void DrvsLKMotorSendCmd(DrvsLKMotor_s *inst, uint8_t cmd);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif // DRVS_LKMOTOR_H
