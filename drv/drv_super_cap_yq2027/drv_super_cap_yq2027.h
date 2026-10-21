/**
 * @file drv_super_cap_yq2027.h
 * @brief YQ2027 超级电容 CAN 驱动（机器人主控侧）
 *
 * @note 协议依据：drv/drv_super_cap_yq2027/supercap-can-interface.md（2026-10-21 版）
 *       - 总线：经典 CAN、11 位标准 ID、数据帧、DLC=8、小端
 *       - 主控 → 超电：0x061，8 字节命令
 *       - 超电 → 主控：0x052，8 字节紧凑反馈（仅新格式，无旧浮点反馈）
 *
 * @note 职责边界：只做「结构体 ↔ CAN 字节」的协议映射和在线检测；
 *       命令内容（功率限制、使能位等）由 app 按自己的策略填写。
 */

#ifndef __DRV_SUPER_CAP_YQ2027_H
#define __DRV_SUPER_CAP_YQ2027_H

#include <stdint.h>
#include "bsp_can.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "drv_daemon.h"

/* 命令/反馈帧均为 8 字节（经典 CAN 单帧） */
#define SUPER_CAP_YQ2027_FRAME_SIZE 8u

/* 协议固定 ID（可由 Config 覆盖，通常无需修改） */
#define SUPER_CAP_YQ2027_TX_ID_DEFAULT 0x061u
#define SUPER_CAP_YQ2027_RX_ID_DEFAULT 0x052u

typedef struct SuperCapYq2027 SuperCapYq2027_s;

/*============================================
 *              状态码解码枚举（0x052 D0）
 *============================================*/

/* bit5:4 无线充电状态 */
typedef enum : uint8_t
{
    SUPER_CAP_YQ2027_WIRELESS_ABNORMAL = 0, // 异常 / 无硬件
    SUPER_CAP_YQ2027_WIRELESS_OFF = 1,      // 关闭
    SUPER_CAP_YQ2027_WIRELESS_CHARGING = 2, // 充电中
    SUPER_CAP_YQ2027_WIRELESS_DONE = 3,     // 完成
} SuperCapYq2027WirelessState_e;

/* bit3:2 当前限制因素 */
typedef enum : uint8_t
{
    SUPER_CAP_YQ2027_LIMIT_UPSTREAM_POWER = 0,  // 上游功率
    SUPER_CAP_YQ2027_LIMIT_CAP_MAX_VOLTAGE = 1, // 最高电容电压
    SUPER_CAP_YQ2027_LIMIT_ACTIVE_CHARGE = 2,   // 主动充电目标
    SUPER_CAP_YQ2027_LIMIT_CAP_CURRENT = 3,     // 电容充放电限流
} SuperCapYq2027LimitReason_e;

/* bit1:0 故障等级 */
typedef enum : uint8_t
{
    SUPER_CAP_YQ2027_FAULT_NONE = 0,          // 无 / 警告
    SUPER_CAP_YQ2027_FAULT_AUTO_RECOVER = 1,  // 自动恢复
    SUPER_CAP_YQ2027_FAULT_MANUAL_CLEAR = 2,  // 手动清除
    SUPER_CAP_YQ2027_FAULT_UNRECOVERABLE = 3, // 不可清除
} SuperCapYq2027FaultLevel_e;

/*============================================
 *              控制命令（机器人 → 超电，0x061）
 *
 * 8 字节，小端，按 D0~D7 发送：
 *   D0 bit0   enable_dcdc
 *      bit1   system_restart
 *      bit4:2 resv0（发送时强制 0）
 *      bit5   clear_error
 *      bit6   enable_active_charging_limit
 *      bit7   resv1（保留，发送时强制 0）
 *   D1-D2 referee_power_limit   (uint16 LE, W)
 *   D3-D4 referee_energy_buffer (uint16 LE, J)
 *   D5    active_charging_limit_ratio (0-255)
 *   D6-D7 resv2 (int16 LE，发送时强制 0)
 *
 * @note 主动充电目标电压 = clamp(sqrt(ratio/255)*28.8, 10.0, 28.8) V，
 *       仅在 enable_active_charging_limit=1 时生效；停机请让 enable_dcdc=0。
 * @note 该结构体只是调用方填参的便利形式；Send 内部按 D0~D7 字节偏移与
 *       位掩码显式打包，不依赖 C 位域的编译器布局。
 *============================================*/
#pragma pack(push, 1)
typedef struct
{
    uint8_t enable_dcdc : 1;                  // D0 bit0：允许启动 DCDC
    uint8_t system_restart : 1;               // D0 bit1：关闭功率级并重启 MCU
    uint8_t resv0 : 3;                        // D0 bit4:2：保留，发送 0
    uint8_t clear_error : 1;                  // D0 bit5：清除可清除故障
    uint8_t enable_active_charging_limit : 1; // D0 bit6：启用 D5 主动充电目标
    uint8_t resv1 : 1;                        // D0 bit7：保留，发送 0

    uint16_t referee_power_limit;        // D1-D2：裁判/上游功率预算，W
    uint16_t referee_energy_buffer;      // D3-D4：能量缓冲，J
    uint8_t active_charging_limit_ratio; // D5：主动充电目标比例，0-255
    int16_t resv2;                       // D6-D7：保留，发送 0
} SuperCapYq2027Command_s;
#pragma pack(pop)

_Static_assert(sizeof(SuperCapYq2027Command_s) == SUPER_CAP_YQ2027_FRAME_SIZE, "SuperCapYq2027Command_s must be 8 bytes");

/*============================================
 *              反馈原始帧（0x052，仅用于文档/长度断言）
 *
 * @note 解析时按 D0~D7 显式读取，不直接解引用 packed 结构体。
 *============================================*/
#pragma pack(push, 1)
typedef struct
{
    uint8_t status_code;          // D0：状态位，bit6 固定为 1
    uint16_t chassis_power_raw;   // D1-D2：负载功率 raw；W = raw/64 - 256
    uint16_t referee_power_raw;   // D3-D4：上游输入功率 raw，编码同上
    uint16_t chassis_power_limit; // D5-D6：当前可用功率估算，W
    uint8_t cap_energy;           // D7：电容能量估算，250 = 100%
} SuperCapYq2027FeedbackFrame_s;
#pragma pack(pop)

_Static_assert(sizeof(SuperCapYq2027FeedbackFrame_s) == SUPER_CAP_YQ2027_FRAME_SIZE, "SuperCapYq2027FeedbackFrame_s must be 8 bytes");

/*============================================
 *              反馈数据（解码后）
 *============================================*/
typedef struct
{
    uint8_t status_code;                          // D0 原始值
    uint8_t dcdc_enabled;                         // bit7：DCDC 实际输出
    uint8_t protocol_valid;                       // bit6：协议版本（应为 1）
    SuperCapYq2027WirelessState_e wireless_state; // bit5:4
    SuperCapYq2027LimitReason_e limit_reason;     // bit3:2
    SuperCapYq2027FaultLevel_e fault_level;       // bit1:0
    float chassis_power_w;                        // 负载功率，W（负值表示回流）
    float referee_power_w;                        // 上游输入功率，W
    uint16_t chassis_power_limit_w;               // 当前可用功率估算，W
    uint8_t cap_energy_raw;                       // 0-255，250=100%
    float cap_energy_percent;                     // raw / 2.5；>100 表示超过额定满电
    uint64_t timestamp_us;                        // 帧到达时间戳；0 表示从未收到过
} SuperCapYq2027Data_s;

/*============================================
 *              实例
 *============================================*/
struct SuperCapYq2027
{
    /* 通信 */
    CANInstance *can;        // CAN 实例
    CAN_Filter_s can_filter; // 0x052 反馈过滤器
    DaemonInstance *daemon;  // 在线检测

    uint32_t tx_id; // 机器人 → 超电（默认 0x061）
    uint32_t rx_id; // 超电 → 机器人（默认 0x052）
    uint32_t timeout_ms;

    uint8_t registered;
    uint8_t configured;

    /* 接收双缓冲：ISR 写 data[data_idx]，任务读 data[!data_idx] */
    SuperCapYq2027Data_s data[2];
    volatile uint8_t data_idx;

    /* 调试计数（只增不清） */
    volatile uint32_t tx_cnt;      // CANTransmit 调用次数
    volatile uint32_t tx_fail;     // 入队失败 + 逐帧发送失败
    volatile uint32_t rx_cnt;      // 有效反馈帧数
    volatile uint32_t rx_len_err;  // 长度不是 8
    volatile uint32_t rx_id_err;   // 非预期 ID / 非标准数据帧
    volatile uint32_t bus_err_cnt; // CAN 错误回调次数
    volatile uint32_t bus_off_cnt; // 进入 bus-off 次数
    volatile uint8_t last_status_code;
};

/*============================================
 *              配置
 *
 * @note 板级值（CAN 总线、ID、超时、daemon 阈值）全部由调用方在 Config
 *       初始化式里给出，不设编译期宏。
 *============================================*/
typedef struct
{
    BoardCAN_e can_e; // 板载 CAN 枚举

    /* 协议默认值：tx=0x061、rx=0x052。
     * ID 必须是 11 位标准帧（≤0x7FF），且不能用 CAN_ID_UNUSED。 */
    uint32_t tx_id;
    uint32_t rx_id;

    uint32_t timeout_ms;              // CANTransmit 发送资源等待超时
    uint16_t reload_count;            // daemon 喂狗阈值(ms)，0=禁用监控
    DaemonFaultAction_e fault_action; // 离线故障动作
} SuperCapYq2027Config_s;

/*============================================
 *              实例定义宏
 *============================================*/
#define SUPER_CAP_YQ2027_INSTANCE_DEF(name)                                                                                                                    \
    CAN_INSTANCE_DEF(name##_can);                                                                                                                              \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                                                        \
    static SuperCapYq2027_s name = {                                                                                                                           \
        .can = &name##_can,                                                                                                                                    \
        .daemon = &name##_daemon,                                                                                                                              \
    }

/*============================================
 *              公共接口
 *============================================*/

/**
 * @brief 注册驱动（仅一次）：注册 CAN 实例与 daemon
 * @retval 0  成功
 * @retval -1 参数非法 / 重复注册 / CAN 注册失败
 */
int8_t SuperCapYq2027Register(SuperCapYq2027_s *inst);

/**
 * @brief 配置驱动（可重复调用）：ID、过滤器、超时、daemon
 * @note 必须在 Register 之后调用；未配置时 Send 返回 BSP_PARAM_ERR。
 * @retval 0  成功
 * @retval -1 参数非法 / 未 Register / ID 越界
 */
int8_t SuperCapYq2027Config(SuperCapYq2027_s *inst, const SuperCapYq2027Config_s *cfg);

/**
 * @brief 打包并发送一帧 8 字节控制命令
 * @note 协议建议每 10 ms 发送一次完整命令，由调用方按自己的周期调用。
 * @note 不自动周期发送、不重试；掉帧由调用方下一周期补发。
 * @note 发送前强制清零保留位 resv0 / resv1 / resv2。
 * @retval CANTransmit 的返回值（BSP_OK 仅表示已入队，逐帧结果看 tx_fail）
 * @retval BSP_PARAM_ERR 参数非法 / 未 Config
 */
BSP_Status_e SuperCapYq2027Send(SuperCapYq2027_s *inst, const SuperCapYq2027Command_s *cmd);

/**
 * @brief 取最新一帧反馈（任务上下文）
 * @note 收到首帧前返回全 0；用 timestamp_us == 0 判断"从未收到"。
 */
SuperCapYq2027Data_s SuperCapYq2027GetData(const SuperCapYq2027_s *inst);

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */
#endif /* __DRV_SUPER_CAP_YQ2027_H */
