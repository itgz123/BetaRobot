/**
 * @file drvs_rsmotor.h
 * @brief 灵足时代 RS05 准直驱电机（RobStride 05，7.75:1 减速比）纯协议驱动
 * @author TRW
 * @date 2026-09-21
 *
 * @note **纯协议**：本模块只负责"字节 ↔ 物理量"的映射和收发。
 *       - 发：把扭矩值按 MIT 帧打包发到 CAN
 *       - 收：把反馈帧解成位置/速度/扭矩/温度/状态位
 *       - 模式命令：使能 / 失能 / 清错 / 置零 / 改 ID（SendCmd，见 DrvsRSMotorModeCmd_e）
 *       不做闭环（PID/MIT 在 lib_axis 等算法层，app 负责把算法输出接到 SetRef）；
 *       不做滤波、不做方向取反、不做多圈累加、不做零点偏置——这些是"跨帧推断/策略"，
 *       不是协议，由 app 或算法层自己管。
 *
 * @note **本模块不持有使能状态**：没有 Enable()/Disable()，也不记 enable 标志。
 *       想让电机转 = app 自己调 `SendCmd(inst, DRVS_RS_CMD_MOTOR_MODE)`；
 *       想停 = `SetRef(inst, 0)` 并调 `SendCmd(inst, DRVS_RS_CMD_RESET_MODE)`。
 *       模块不替 app 决定"该不该使能"，故 Send 也无失能早退（协议层由电机自己拒绝）。
 *
 * @note 无基类、无虚函数表：每个品牌自包含（自己的实例结构体 + 自己的函数），
 *       app 当胶水。命名带 Drvs 前缀是为了重构期间与旧 drv_motor 同树共存
 *       （避免符号冲突）；等 app 全部切到 drvs 后删掉旧 drv，再去掉前缀。
 *
 * @note 协议：CAN MIT 模式，标准帧 11 位 ID，1Mbps，8 字节帧，12 位跨字节位域，
 *       无符号整数线性映射。**控制帧与 DM 逐字节相同**，故 DrvsRSMotorControlFrame_u
 *       与 DrvsDMMotorControlFrame_u 布局一致（各自独立定义，不共享类型）。
 *
 * @note ⚠️ 硬前提：RS05 出厂默认私有协议（扩展帧），必须先用灵足上位机把 protocol_1
 *       标志位切换为 2（MIT）并**重新上电**，本驱动方可工作。
 *
 * @note 位置/速度/扭矩量程由灵足上位机设定，cfg 里的值必须与上位机一致；
 *       cfg 传 0 时取 RS05 出厂默认量程（12.57 rad / 50 rad/s / 5.5 Nm）。
 *
 * @note 反馈语义为输出轴（7.75:1 减速比已由电机固件折算），驱动内严禁再乘减速比。
 *
 * @note 本项目在上位机做控制，MIT 帧的板载 PD 不用（p_des=v_des=Kp=Kd=0），
 *       仅通过 t_ff 下发扭矩，因此对外只暴露扭矩一个接口。
 */

#ifndef DRVS_RSMOTOR_H
#define DRVS_RSMOTOR_H

#include <stdint.h>

#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_daemon.h"

/*============================================
 *              前向声明
 *============================================*/
typedef struct DrvsRSMotor DrvsRSMotor_s;

/*============================================
 *              CAN 帧联合体定义
 *
 * 因存在 12 位跨字节位域，联合体主要用于
 * 文档化帧布局，解析仍使用移位操作。
 *============================================*/

/**
 * @brief RS05 MIT 反馈帧（8 字节）
 *
 *   D[0]   ：电机 canid
 *   D[1..2]：位置 uint16 大端
 *   D[3]   ：速度 VEL[11:4]
 *   D[4]   ：bits[7:4]=速度 VEL[3:0], bits[3:0]=扭矩 T[11:8]
 *   D[5]   ：扭矩 T[7:0]
 *   D[6]   ：bits[7:6]=模式状态, bit[5]=故障, bit[4]=预警, bits[3:0]=绕组温度高 4 位
 *   D[7]   ：绕组温度低 8 位 TEMP[7:0]（值 = 温度℃ × 10）
 *
 * @note 与 DM 反馈帧的区别：D[0] 是 canid（DM 是 error+id）；D[6] 带模式/故障/预警位；
 *       温度是 12 位 ×10 定点（DM 是两个 int8 的 MOS/线圈温度）。
 */
#pragma pack(push, 1)
typedef struct
{
    uint8_t can_id;               // [0]   电机 canid
    uint16_t position_be;         // [1-2] 位置 uint16 大端
    uint8_t vel_hi;               // [3]   速度 VEL[11:4]
    uint8_t vel_lo_and_torque_hi; // [4]   VEL[3:0] | TORQUE[11:8]
    uint8_t torque_lo;            // [5]   扭矩 TORQUE[7:0]
    uint8_t status_and_temp_hi;   // [6]   mode[7:6] | fault[5] | warn[4] | TEMP[11:8]
    uint8_t temp_lo;              // [7]   绕组温度 TEMP[7:0]（= ℃×10）
} DrvsRSMotorFeedbackFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsRSMotorFeedbackFrameParts_s parts;
} DrvsRSMotorFeedbackFrame_u;

_Static_assert(sizeof(DrvsRSMotorFeedbackFrameParts_s) == 8, "RS 反馈帧必须是 8 字节");

/**
 * @brief RS05 MIT 控制帧（8 字节，与 DM 逐字节相同）
 *
 *   D[0..1]：p_des uint16 大端
 *   D[2]   ：v_des[11:4]
 *   D[3]   ：bits[7:4]=v_des[3:0], bits[3:0]=Kp[11:8]
 *   D[4]   ：Kp[7:0]
 *   D[5]   ：Kd[11:4]
 *   D[6]   ：bits[7:4]=Kd[3:0], bits[3:0]=t_ff[11:8]
 *   D[7]   ：t_ff[7:0]
 *
 * @note 本模块 p_des/v_des/Kp/Kd 恒为 0（板载 PD 不用），只有 t_ff 有效。
 *       联合体保留完整布局只为文档化协议。
 */
#pragma pack(push, 1)
typedef struct
{
    uint16_t p_des_be;          // [0-1] 位置目标 uint16 大端
    uint8_t v_des_hi;           // [2]   v_des[11:4]
    uint8_t v_des_lo_and_kp_hi; // [3]   v_des[3:0] | kp[11:8]
    uint8_t kp_lo;              // [4]   kp[7:0]
    uint8_t kd_hi;              // [5]   kd[11:4]
    uint8_t kd_lo_and_tff_hi;   // [6]   kd[3:0] | t_ff[11:8]
    uint8_t tff_lo;             // [7]   t_ff[7:0]
} DrvsRSMotorControlFrameParts_s;
#pragma pack(pop)

typedef union
{
    uint8_t raw[8];
    DrvsRSMotorControlFrameParts_s parts;
} DrvsRSMotorControlFrame_u;

_Static_assert(sizeof(DrvsRSMotorControlFrameParts_s) == 8, "RS 控制帧必须是 8 字节");

/*============================================
 *              模式命令枚举
 *============================================*/
/**
 * @brief RS05 模式命令
 * @note 发送时前 7 字节填 0xFF，第 8 字节为命令码
 * @note 0xFA / 0x01 特殊：Byte6 填新 ID，第 8 字节才是命令码
 */
typedef enum : uint8_t
{
    DRVS_RS_CMD_CLEAR_ERROR = 0xFB,      // 清除电机错误
    DRVS_RS_CMD_MOTOR_MODE = 0xFC,       // 使能，进入 MIT 控制模式
    DRVS_RS_CMD_RESET_MODE = 0xFD,       // 停止电机
    DRVS_RS_CMD_ZERO_POSITION = 0xFE,    // 将当前位置设为编码器零点
    DRVS_RS_CMD_CHANGE_CAN_ID = 0xFA,    // 修改电机 CANID（调试用，Byte6=新 id）
    DRVS_RS_CMD_CHANGE_MASTER_ID = 0x01, // 修改主机 CANID（调试用，Byte6=新主机 id）
} DrvsRSMotorModeCmd_e;

/*============================================
 *              模式状态枚举（反馈帧 D[6] bits[7:6]）
 *============================================*/
/**
 * @brief RS05 模式状态
 * @note mode_state == CALI 期间电机不接受 MIT 控制命令，
 *       需确认进入 MOTOR 后再使能扭矩。
 */
typedef enum : uint8_t
{
    DRVS_RS_STATE_RESET = 0, // 复位中
    DRVS_RS_STATE_CALI = 1,  // 标定中（不接受 MIT 控制）
    DRVS_RS_STATE_MOTOR = 2, // 正常运行（接受 MIT 控制）
} DrvsRSMotorModeState_e;

/*============================================
 *              协议映射结构体
 *============================================*/
/**
 * @brief 协议映射（初始化时根据量程预计算 scale，存进实例）
 * @note  uint→float: raw * to_float_scale + offset   (1 mul + 1 add)
 *        float→uint: (val + range) * to_uint_scale   (1 add + 1 mul)
 *        预计算是为热路径省掉浮点除法（Cortex-M7 FPU 除法约 14 周期）
 */
typedef struct
{
    float p_max;   // 位置范围 ±p_max (rad)，必须与灵足上位机一致
    float v_range; // 速度范围 ±v_range (rad/s)
    float t_range; // 扭矩范围 ±t_range (Nm)

    /* 预计算转换因子 */
    float pos_to_float_scale; // = 2*p_max / 65535    (uint16→float)
    float vel_to_float_scale; // = 2*v_range / 4095   (uint12→float)
    float vel_to_uint_scale;  // = 4095 / (2*v_range) (float→uint12)
    float t_to_float_scale;   // = 2*t_range / 4095   (uint12→float)
    float t_to_uint_scale;    // = 4095 / (2*t_range) (float→uint12)
} DrvsRSMotorProtocolMap_s;

/*============================================
 *              反馈数据结构体
 *============================================*/
/**
 * @brief 一帧反馈解出来的全部内容
 * @note  都是**协议原始量**：未累加、未偏置、未取反、未滤波。
 *        position 只在 [-p_max, +p_max] 一个量程内（RS 位置范围由上位机设定，
 *        跨过边界会回绕），需要多圈累加请由 app/算法层自己做。
 */
typedef struct
{
    float position;        // 位置 (rad)，协议原值，范围 [-p_max, +p_max]
    float speed;           // 速度 (rad/s)
    float torque;          // 扭矩 (Nm)
    float temperature;     // 绕组温度 (°C)，原始 12 位值 = ℃×10
    uint8_t mode_state;    // 模式状态，见 DrvsRSMotorModeState_e（D[6] bits[7:6]）
    uint8_t fault;         // 故障标志（D[6] bit5），1=故障
    uint8_t warn;          // 预警标志（D[6] bit4），1=预警
    uint64_t timestamp_us; // CAN 帧到达时间戳 (us)
} DrvsRSMotorData_s;

/*============================================
 *              RS 电机实例结构体
 *============================================*/
struct DrvsRSMotor
{
    /* 通信 */
    CANInstance *can;        // CAN 实例指针
    CAN_Filter_s can_filter; // CAN 接收过滤器
    DaemonInstance *daemon;  // 守护进程实例（通信在线检测）
    uint32_t timeout_ms;     // CAN 发送超时 (ms)
    uint32_t tx_fail;        // 发送失败累计：CANTransmit 入队失败（返回非 BSP_OK）+ 逐帧失败
                             // （tx_complete_callback 报 result != BSP_OK，见 .c 的 TxHook）；只增不清，调试用

    /* 标识 */
    uint16_t can_id;    // stm32->motor | tx
    uint16_t master_id; // motor->stm32 | rx

    /* 协议映射 */
    DrvsRSMotorProtocolMap_s proto_map;

    /* 控制量（纯扭矩） */
    float ref_torque; // 设定扭矩 (Nm)，由 SetRef 写入，Send 打包下发

    /* 接收双缓冲：ISR 解析写一个，GetData 读另一个，读者永远拿到完整的一帧 */
    DrvsRSMotorData_s data[2]; // data[data_idx] = ISR 正在写的
    volatile uint8_t data_idx; // 当前 ISR 写入的缓冲区索引 (0/1)
};

/*============================================
 *              初始化配置结构体
 *============================================*/
/**
 * @brief RS 电机配置（Config 函数使用）
 * @note 只能在 Register 之后调用；可重复调用修改量程/超时等。
 * @note pos_max/vel_range/t_range 传 0 时自动取 RS05 出厂默认量程
 *       （12.57 rad / 50 rad/s / 5.5 Nm）。
 */
typedef struct
{
    BoardCAN_e can_e; // 板载 CAN 枚举（用于查找硬件映射）

    uint16_t can_id;    // stm32->motor | tx
    uint16_t master_id; // motor->stm32 | rx

    /* 协议量程（必须与灵足上位机一致，传 0 取默认） */
    float pos_max;   // 位置范围 ±pos_max (rad)，默认 12.57
    float vel_range; // 速度范围 ±vel_range (rad/s)，默认 50
    float t_range;   // 扭矩范围 ±t_range (Nm)，默认 5.5

    /* daemon 设置 */
    uint16_t reload_count;            // 重载值（喂狗超时阈值），0=禁用
    DaemonFaultAction_e fault_action; // 离线故障动作

    uint32_t timeout_ms; // CAN 发送超时 (ms)
} DrvsRSMotorConfig_s;

/*============================================
 *              单电机实例定义宏
 *============================================*/
#define DRVS_RSMOTOR_INSTANCE_DEF(name)                                                                                \
    CAN_INSTANCE_DEF(name##_can);                                                                                      \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                \
    static DrvsRSMotor_s name = {                                                                                      \
        .can = &name##_can,                                                                                            \
        .daemon = &name##_daemon,                                                                                      \
    }

/*============================================
 *              公共接口
 *============================================*/
int8_t DrvsRSMotorRegister(DrvsRSMotor_s *inst);
int8_t DrvsRSMotorConfig(DrvsRSMotor_s *inst, const DrvsRSMotorConfig_s *cfg);

/**
 * @brief 设置扭矩设定值 (Nm)
 * @note  仅记录，不发送；需再调 DrvsRSMotorSend()
 * @note  方向取反（安装镜像）由调用方自己做，本模块不做 feedback/motor_direction
 */
void DrvsRSMotorSetRef(DrvsRSMotor_s *inst, float torque);

/**
 * @brief 打包并发送一帧 MIT 控制帧
 * @note  p_des=v_des=Kp=Kd=0，t_ff = clamp(ref_torque, ±t_range)
 * @note  只要没被 SendCmd 使能过，电机自己会忽略这帧（模块不做软件拦截）
 */
void DrvsRSMotorSend(DrvsRSMotor_s *inst);

/**
 * @brief 取最新一帧反馈（在任务上下文调用）
 * @note  RX 回调（ISR）已解析好写进双缓冲，这里只读"另一个"缓冲区，
 *        不解析、不推断、不滤波，也不会阻塞 ISR。
 * @note  收到第一帧之前返回全 0；要判"是否收到过"看 timestamp_us == 0
 */
DrvsRSMotorData_s DrvsRSMotorGetData(const DrvsRSMotor_s *inst);

/**
 * @brief 发送模式命令（使能/失能/清错/置零），命令码见 DrvsRSMotorModeCmd_e
 * @note  帧格式：前 7 字节 0xFF，第 8 字节命令码
 */
void DrvsRSMotorSendCmd(DrvsRSMotor_s *inst, uint8_t cmd);

/**
 * @brief 修改电机 CANID（调试用，指令 0xFA）
 * @note  帧 ID 用电机"当前" canid 发出（一对一定址）；Byte6 = 新 id
 * @note  改完后电机只响应新 ID，本实例的 tx/rx 不会自动更新，
 *        必须重新 DrvsRSMotorConfig 才能继续通信；断电保存需另发保存命令（未封装）
 */
void DrvsRSMotorChangeCanID(DrvsRSMotor_s *inst, uint16_t can_id);

/**
 * @brief 修改主机 CANID（调试用，指令 0x01）
 * @note  同上，Byte6 = 新主机 id；改完后需重新 DrvsRSMotorConfig
 */
void DrvsRSMotorChangeMasterCanID(DrvsRSMotor_s *inst, uint16_t can_id);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif // DRVS_RSMOTOR_H
