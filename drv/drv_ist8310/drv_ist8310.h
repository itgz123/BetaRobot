/**
 * @file drv_ist8310.h
 * @brief IST8310 三轴磁力计驱动（I2C）
 *
 * @note 本模块只负责与器件通信：寄存器读写/初始化、原始数据 → 物理量（µT）、
 *       时间戳、在线状态。不做硬铁/软铁标定，也不做倾斜补偿 —— 标定归 drvlib 层。
 * @note 器件**没有连续测量模式**（只有 Stand-By / 单次测量 / 自检，§3.1）：
 *       每一帧都必须先写 CNTL1=0x01 触发一次单次测量，等器件转回 Stand-By
 *       并把 DRDY 拉起来，再读输出寄存器。因此两种工作模式都带"触发"这一步。
 * @note 无温度 API：数据手册虽给出 TEMP 寄存器（0x1C/0x1D），但未给 LSB/°C 系数，
 *       无法换算为摄氏度，故不提供（寄存器地址已在 reg_def 中登记）。
 *
 * @note 中断模式的调用约定与 drv_bmi088 有一处关键不同：
 *       **BMI088ReadInt 是纯读取，IST8310ReadInt 有副作用** —— 它会在空闲时
 *       写 CNTL1 触发下一次测量。调用方必须按期望采样率周期性调用它，
 *       否则触发链会停在原地、DRDY 不再到来。
 *
 * @note 前置条件（`i2c_mode != I2C_BLOCK_MODE` 时）：
 *       IT/DMA 都要求 CubeMX 已使能该 I2C 的 event/error 全局中断并生成
 *       I2Cx_EV_IRQHandler / I2Cx_ER_IRQHandler（HAL 的 IT 状态机就跑在事件中断里）；
 *       DMA 另需该 I2C 的 TX/RX DMA 流及其全局中断。
 *       否则收不到完成回调，一笔传输会一直等到超时。
 */

#ifndef __DRV_IST8310_H
#define __DRV_IST8310_H

#include "main.h"
#include "bsp_map.h"

#if defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include "bsp_i2c.h"
#include "bsp_gpio.h"
#include "drv_daemon.h"
#include "ist8310_reg_def.h"
#include "bsp_log.h"

/*============================ 缓冲区大小定义 ============================*/

#define IST8310_BUFF_SIZE 8 // I2C 收发缓冲：单寄存器读 1 字节 / 三轴连读 6 字节，8 足够

/*============================ 工作模式 ============================*/

/**
 * @brief IST8310 工作模式枚举
 * @note 中断模式下由 DRDY 引脚上升沿（EXTI）自动发起 I2C 读取，
 *       时间戳取 EXTI 触发时刻，比轮询更接近真实采样时刻
 */
typedef enum : uint8_t
{
    IST8310_MODE_POLLING = 0, // 阻塞模式：任务里触发 → 等待 → 读，全程同步
    IST8310_MODE_INT,         // 中断模式：DRDY EXTI 发起读取，完成后回调发布数据
} IST8310_WorkMode_e;

/**
 * @brief DRDY 引脚有效极性（对应 CNTL2.DRP）
 * @note **必须与 CubeMX 中该 EXTI 的触发边沿一致**：
 *       高有效 → 配上升沿；低有效 → 配下降沿。不一致会导致中断风暴或永无中断。
 */
typedef enum : uint8_t
{
    IST8310_DRDY_ACTIVE_LOW = 0, // 低电平有效（CNTL2.DRP = 0），EXTI 应配下降沿
    IST8310_DRDY_ACTIVE_HIGH,    // 高电平有效（CNTL2.DRP = 1），EXTI 应配上升沿
} IST8310_DrdyPolarity_e;

/*============================ 配置结构体 ============================*/

/**
 * @brief IST8310 配置结构体（用于 IST8310Config）
 *
 * @note 包含硬件枚举和器件运行时参数。
 *       硬件枚举由 Config 传给子模块，Register 只负责注册。
 * @note 不带标定参数：本层输出的是"未补偿的磁感应强度"，硬铁/软铁补偿由 drvlib 处理。
 *
 * @note `work_mode` 与 `i2c_mode` 是两个正交的维度：
 *       work_mode 决定**谁在什么时刻发起**一次采样（任务轮询 / DRDY 中断），
 *       i2c_mode  决定**一笔传输怎么做**（阻塞 / 中断 / DMA）。
 *       两者唯一的耦合是：`work_mode == INT` 时 `i2c_mode` 不能是 BLOCK ——
 *       EXTI 里做阻塞 I2C 会死循环（tick 在中断里不前进）。
 *       其余组合都合法，比如 `POLLING + DMA`（调用方仍是同步地拿到结果，
 *       但传输期间 CPU 是空闲的）。器件初始化序列始终用阻塞，与这里无关。
 */
typedef struct
{
    /* 硬件枚举（Config 时传给子模块） */
    BoardI2C_e i2c_e;   // 板载 I2C 枚举
    BoardGPIO_e drdy_e; // DRDY 中断 GPIO 枚举（轮询模式可填 GPIO_NUM_MAX）
    BoardGPIO_e rstn_e; // 复位 GPIO 枚举；**填 GPIO_NUM_MAX 表示未接**（无复位能力）

    /* 器件运行时参数 */
    uint16_t daemon_reload;               // daemon 喂狗超时（ms），0 表示禁用（不监控：DaemonIsOnline 恒报在线）
    DaemonFaultAction_e daemon_fault;     // daemon 离线故障动作
    IST8310_WorkMode_e work_mode;         // 采样由谁驱动（轮询/中断）
    I2C_Work_Mode_e i2c_mode;             // 传输怎么做（阻塞/IT/DMA），与 work_mode 正交
    IST8310_Avg_e avg;                    // 内部平均次数（影响噪声与最小测量间隔）
    IST8310_PdPulse_e pd_pulse;           // set/reset 脉冲宽度（推荐 IST8310_PD_PULSE_NORMAL）
    IST8310_DrdyPolarity_e drdy_polarity; // DRDY 有效极性（须与 CubeMX EXTI 边沿一致）
    uint32_t i2c_timeout_ms;              // I2C 传输超时(ms)
} IST8310_Config_s;

/*============================ 数据结构体 ============================*/

/**
 * @brief 磁力计数据结构体
 */
typedef struct
{
    float mag[IST8310_AXIS_NUM]; // 三轴磁感应强度 (µT)
    uint64_t time_stamp;         // 时间戳 (µs)
} IST8310_Data_t;

/**
 * @brief 三轴原始数据联合体
 * @note 3 轴 × int16 = 6 字节，二进制补码、小端（与 Cortex-M 一致）
 */
typedef union
{
    uint8_t bytes[IST8310_DATA_LEN]; // 原始字节
    int16_t axis[IST8310_AXIS_NUM];  // 三轴数组: [x, y, z]
#pragma pack(push, 1)
    struct
    {
        int16_t x; // X 轴
        int16_t y; // Y 轴
        int16_t z; // Z 轴
    };
#pragma pack(pop)
} IST8310_AxisRaw_u;

/*============================ 实例结构体 ============================*/

/**
 * @brief IST8310 实例结构体
 * @note 使用指针指向 BSP 实例，在注册时设置 parent
 */
typedef struct IST8310Instance
{
    /* BSP 实例指针 */
    I2CInstance *i2c_inst;  // I2C 实例
    GPIOInstance *drdy;     // 数据就绪中断
    GPIOInstance *rstn;     // 硬复位（低有效）；has_rstn=0 时不可用
    DaemonInstance *daemon; // 守护进程实例

    /* 器件配置（Config 写入） */
    IST8310_WorkMode_e work_mode;         // 采样由谁驱动（轮询/中断）
    I2C_Work_Mode_e i2c_mode;             // 传输怎么做（阻塞/IT/DMA）
    IST8310_Avg_e avg;                    // 平均次数
    IST8310_PdPulse_e pd_pulse;           // 脉冲宽度
    IST8310_DrdyPolarity_e drdy_polarity; // DRDY 极性
    uint32_t i2c_timeout_ms;              // I2C 超时(ms)
    uint16_t meas_delay_ms;               // 单次测量最小等待(ms)，由 avg 推导
    uint8_t has_rstn;                     // 1 = rstn_e 有效（非 GPIO_NUM_MAX）

    /*============================ 中断模式字段 ============================*/

    volatile uint8_t transfer_busy;   // I2C 传输进行中（INT 模式的 DRDY 采集链）
    volatile uint8_t armed;           // 已触发单次测量、等待 DRDY
    volatile uint8_t recover_request; // 任务上下文需执行恢复（由错误回调置位）
    volatile uint8_t xfer_done;       // 当前这笔异步传输已完成（回调在中断里置位）
    volatile uint8_t xfer_error;      // 当前这笔异步传输已失败（错误回调在中断里置位）
    uint64_t int_timestamp;           // 当前读取对应的 DRDY 触发时间 (µs)
    uint64_t arm_us;                  // 本次触发测量（armed=1）的时刻 (µs)，armed 超时看门狗用
    uint64_t arm_timeout_us;          // armed 最长容忍时长 (µs)，Config 时由 meas_delay_ms 推得

    /* --- 双缓冲发布：ISR 写 frame[frame_wr]，任务读 frame[latest_idx] --- */
    IST8310_Data_t frame[2];      // 两槽轮换，保证任务不会读到写了一半的帧
    volatile uint8_t frame_wr;    // ISR 当前写入槽
    volatile uint8_t latest_idx;  // 最新已发布槽（ISR 填完后原子切换）
    volatile uint8_t frame_valid; // 至少发布过一帧

    /* --- 失败计数与恢复记账（仅调试/日志） --- */
    uint8_t fail_count;       // 连续失败次数，成功即清零
    uint8_t recover_count;    // 恢复总次数
    uint64_t last_recover_us; // 上次恢复时间戳 (µs)，用于冷却
} IST8310Instance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief IST8310 实例静态定义宏
 * @param name 实例名称
 *
 * @note 使用 BSP 层的实例定义宏，parent 在注册时设置
 *
 * @example
 *   IST8310_INSTANCE_DEF(ist8310);
 */
#define IST8310_INSTANCE_DEF(name)                   \
    I2C_INSTANCE_DEF(name##_i2c, IST8310_BUFF_SIZE); \
    GPIO_INSTANCE_DEF(name##_drdy);                  \
    GPIO_INSTANCE_DEF(name##_rstn);                  \
    DAEMON_INSTANCE_DEF(name##_daemon);              \
    static IST8310Instance name = {                  \
        .i2c_inst = &name##_i2c,                     \
        .drdy = &name##_drdy,                        \
        .rstn = &name##_rstn,                        \
        .daemon = &name##_daemon}

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册 IST8310 实例（仅调用一次）
 * @param inst IST8310 实例指针
 * @return 0 成功，-1 失败
 *
 * @note 注册 I2C/GPIO/Daemon 子模块。不配置硬件参数（由 IST8310Config 负责）。
 */
int8_t IST8310Register(IST8310Instance *inst);

/**
 * @brief 配置 IST8310 实例（可重复调用）
 * @param inst   IST8310 实例指针
 * @param config 配置结构体指针（硬件枚举 + 器件参数 + daemon 配置）
 * @return 0 成功，-1 失败
 *
 * @note 填充子模块硬件映射、配置 daemon、执行器件初始化序列（含 POR 等待，
 *       阻塞约 100ms 以上）。可重复调用以重新初始化。
 *       要求在 IST8310Register 之后调用。
 */
int8_t IST8310Config(IST8310Instance *inst, const IST8310_Config_s *config);

/**
 * @brief 阻塞读取一帧磁力数据（轮询模式专用）
 * @param inst IST8310 实例指针
 * @return IST8310_Data_t；失败或模式不符返回全 0（time_stamp 也为 0）
 *
 * @note 内部串行完成：触发单次测量 → 等待 meas_delay → 轮询 STAT1.DRDY →
 *       连读 0x03~0x08 六字节。对调用方是同步的，耗时约 meas_delay + 几笔 I2C 往返。
 * @note `i2c_mode` 只影响这几笔传输怎么走（阻塞 / 中断 / DMA），**返回语义完全一致**：
 *       返回非 0 时缓冲里一定是刚读到的新数据。配置成 IT/DMA 时传输期间 CPU 空闲，
 *       但函数返回前仍是忙等回调（不阻塞任务调度中被人为 delay 之外的东西）。
 * @note **只能在任务上下文调用**：阻塞模式下用的是 HAL_GetTick 计时的 HAL 阻塞 API，
 *       中断里 tick 不前进会立刻超时；异步模式下这里是忙等另一个中断的回调，同样不能在中断里跑。
 * @note 中断模式下调用会返回空数据并记录警告。
 */
IST8310_Data_t IST8310ReadBlocking(IST8310Instance *inst);

/**
 * @brief 读取最新一帧磁力数据（中断模式专用）
 * @param inst IST8310 实例指针
 * @return IST8310_Data_t；尚无有效帧返回全 0（time_stamp 为 0）
 *
 * @note **有副作用**：会在总线空闲时触发下一次单次测量（写 CNTL1）。
 *       调用方需按期望采样率周期性调用，否则采集链会停住。
 * @note 只能在任务上下文调用（触发测量用的是阻塞 I2C 写）。
 * @note 内部带 armed 超时看门狗：DRDY 边沿丢失（引脚只在读输出寄存器时才拉低，
 *       所以一次读失败会让它停在高位、EXTI 再不来）时按失败论并走恢复链自愈，
 *       不需要调用方额外处理。
 */
IST8310_Data_t IST8310ReadInt(IST8310Instance *inst);

/**
 * @brief 查询实例在线状态
 * @param inst IST8310 实例指针
 * @return 1 在线，0 离线（daemon 判定或实例为空）
 */
uint8_t IST8310IsOnline(IST8310Instance *inst);

#endif /* defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* __DRV_IST8310_H */
