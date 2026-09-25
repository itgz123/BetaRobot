/**
 * @file drv_ist8310.h
 * @brief IST8310 三轴磁力计驱动（I2C）
 *
 * @note 本模块只负责与器件通信：寄存器读写/初始化、原始数据 → 物理量（µT）、
 *       时间戳、在线状态。不做硬铁/软铁标定，也不做倾斜补偿 —— 标定归 drvlib 层。
 * @note 器件**没有连续测量模式**（只有 Stand-By / 单次测量 / 自检，§3.1）：
 *       每一帧都必须先写 CNTL1=0x01 触发一次单次测量，等器件转回 Stand-By
 *       并把 DRDY 拉起来，再读输出寄存器。这就是本模块与 drv_bmi088 的根本差别：
 *       BMI088 自由运行（配好 ODR 就自己出数），IST8310 必须**问一次、答一次**。
 *
 * @note 采集模型按工作模式分成两条**互相独立**的通路：
 *
 *       - **轮询模式（POLLING）**：三件事完全由调用方编排，本层提供三个原语 ——
 *         `IST8310Request`（发一次测量请求）→ `IST8310CheckReady`（查 STAT1.DRDY）
 *         → `IST8310Read`（读六字节）。想省事就直接调 `IST8310Sample`，
 *         它把三步串起来（参数：轮询间隔、轮询超时）。
 *         三个原语的传输方式都取 `i2c_mode`，对调用方是同步的。
 *
 *       - **中断模式（INT）**：采集链**自维持**，不需要调用方周期性"续命" ——
 *         `IST8310Config` 末尾发出第一条请求；此后每次 DRDY 上升沿由 EXTI 发起
 *         一笔 **IT** 读；该读的**完成回调**发布数据并紧接着发出下一次测量请求。
 *         调用方的 `IST8310Read` 退化成单纯的"取最新已发布帧"。
 *         代价是链路可能悄悄停住（请求没发出去 / 边沿丢 / 完成回调不来），
 *         故 `IST8310Read` 兼作**链路看门狗**入口：停滞超过阈值就补发请求，
 *         连续失败再升级为整段恢复。因此中断模式下**仍须周期性调用 `IST8310Read`**，
 *         否则停滞不会被发现。
 *
 *         中断模式的传输方式**只能是 `BSP_IT_MODE`**（`IST8310Config` 强制，理由见那里的
 *         注释）：这条链的两笔传输都由中断发起，任务侧没有发起入口，也就没有"下一帧顺手
 *         补一刀"的机会 —— 唯一的补刀点是低频看门狗加上整段总线恢复，代价太大，
 *         不能承受 DMA 带来的那个与总线好坏无关的失败源。反过来说，
 *         **中断模式不需要该 I2C 的 DMA 流**，CubeMX 只要使能 event/error 中断即可。
 *
 * @note 无温度 API：数据手册虽给出 TEMP 寄存器（0x1C/0x1D），但未给 LSB/°C 系数，
 *       无法换算为摄氏度，故不提供（寄存器地址已在 reg_def 中登记）。
 *
 * @note DRDY 的极性**不做配置项**：初始化时固定写 CNTL2.DRP = 1（高电平有效），
 *       对应 CubeMX 侧该 EXTI 必须配**上升沿** —— DJI_A 的 PE3、DJI_C 的 PG3
 *       都已这么配（`GPIO_MODE_IT_RISING`）。边沿是由 CubeMX 定死的板上事实，
 *       再开一个软件开关只会制造"两边对不上"（现象是永无中断或中断风暴）。
 *
 * @note 前置条件（`i2c_mode != BSP_BLOCK_MODE` 时）：
 *       IT/DMA 都要求 CubeMX 已使能该 I2C 的 event/error 全局中断并生成
 *       I2Cx_EV_IRQHandler / I2Cx_ER_IRQHandler（HAL 的 IT 状态机就跑在事件中断里）；
 *       `POLLING + DMA` 另需该 I2C 的 TX/RX DMA 流及其全局中断。
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

/*============================ 配置结构体 ============================*/

/**
 * @brief IST8310 配置结构体（用于 IST8310Config）
 *
 * @note 包含硬件枚举和器件运行时参数。
 *       硬件枚举由 Config 传给子模块，Register 只负责注册。
 * @note 不带标定参数：本层输出的是"未补偿的磁感应强度"，硬铁/软铁补偿由 drvlib 处理。
 *
 * @note `work_mode` 与 `i2c_mode` 大致正交，但有一处**硬约束**：
 *       work_mode 决定**谁在什么时刻发起**一次采样（任务轮询 / DRDY 中断），
 *       i2c_mode  决定**一笔传输怎么做**（阻塞 / 中断 / DMA）—— 它是"运行时默认值"，
 *       本层在每次 `I2CMemRead/I2CMemWrite` 调用时把它传给 bsp（bsp 不再持有模式），
 *       而初始化序列/触发写这些必须同步完成的地方一律显式传 `BSP_BLOCK_MODE`。
 *
 *       **硬约束：`work_mode == INT` 时 `i2c_mode` 只能是 `BSP_IT_MODE`**，
 *       BLOCK 与 DMA 都被 `IST8310Config` 拒绝（判据与理由见那里的注释）：
 *       中断模式的传输发起与收尾全在 ISR 里，任务侧没有第二处发起入口，
 *       承受不起"在中断里做阻塞 I2C"（tick 不前进）或"DMA 流的残留只能靠整段恢复来收"。
 *       轮询模式则没有这个限制，`POLLING + DMA` 完全合法（调用方仍是同步地拿到结果，
 *       但传输期间 CPU 是空闲的）。
 *
 * @note 不再需要"临时切模式"：读到/写到一半要换传输方式时直接改变量即可，
 *       bsp 层的模式是每次调用传参的（旧版为按模式挂/摘回调而反复 I2CConfig）。
 */
typedef struct
{
    /* 硬件枚举（Config 时传给子模块） */
    BoardI2C_e i2c_e;   // 板载 I2C 枚举
    BoardGPIO_e drdy_e; // DRDY 中断 GPIO 枚举（轮询模式可填 GPIO_NUM_MAX）
    BoardGPIO_e rstn_e; // 复位 GPIO 枚举；**填 GPIO_NUM_MAX 表示未接**（无复位能力）

    /* 器件运行时参数 */
    uint16_t daemon_reload;           // daemon 喂狗超时（ms），0 表示禁用（不监控：DaemonIsOnline 恒报在线）
    DaemonFaultAction_e daemon_fault; // daemon 离线故障动作
    IST8310_WorkMode_e work_mode;     // 采样由谁驱动（轮询/中断）
    BSP_Transfer_Mode_e i2c_mode;     // 传输怎么做（阻塞/IT/DMA）；INT 模式**只能填 BSP_IT_MODE**
    IST8310_Avg_e avg;                // 内部平均次数（影响噪声与最小测量间隔）
    IST8310_PdPulse_e pd_pulse;       // set/reset 脉冲宽度（推荐 IST8310_PD_PULSE_NORMAL）
    uint32_t i2c_timeout_ms;          // I2C 传输超时(ms)
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

    /* 测量请求字节的常驻发送缓冲（由 IST8310_INSTANCE_DEF 分配，DMA_RAM）
     * 中断模式下请求是**发起即返回**的异步写，HAL 在传输期间一直持有这个指针，
     * 因此不能用调用方的栈变量；DMA 模式还要求它位于 DMA 可访问的 RAM。 */
    uint8_t *req_buff;

    /* 器件配置（Config 写入） */
    IST8310_WorkMode_e work_mode; // 采样由谁驱动（轮询/中断）
    BSP_Transfer_Mode_e i2c_mode; // 传输怎么做（阻塞/IT/DMA），每次调用透传给 bsp
    IST8310_Avg_e avg;            // 平均次数
    IST8310_PdPulse_e pd_pulse;   // 脉冲宽度
    uint32_t i2c_timeout_ms;      // I2C 超时(ms)
    uint16_t meas_delay_ms;       // 单次测量最小等待(ms)，由 avg 推导
    uint8_t has_rstn;             // 1 = rstn_e 有效（非 GPIO_NUM_MAX）

    /*============================ 采集链字段 ============================*/

    /* --- 轮询模式的就绪记录 --- */
    volatile uint8_t data_ready; // CheckReady 的结论：1 = 已确认就绪、可 Read。
                                 // Request 清零；没 Check 就 Read 会打告警（读到的是上一帧）

    /* --- 中断模式：DRDY EXTI 发起的数据读 --- */
    volatile uint8_t transfer_busy;   // 一笔数据读在途
    volatile uint8_t trigger_pending; // 一笔测量请求写在途：写完成前挡住残留的 DRDY 边沿
    volatile uint8_t armed;           // 已发出测量请求、等这一帧落地（EXTI 的放行条件）
    volatile uint8_t recover_request; // 任务上下文需执行恢复（由错误回调置位）
    volatile uint8_t xfer_done;       // 当前这笔异步传输已完成（回调在中断里置位）
    volatile uint8_t xfer_error;      // 当前这笔异步传输已失败（错误回调在中断里置位）
    uint64_t int_timestamp;           // 当前读取对应的 DRDY 触发时间 (µs)

    /* --- 链路看门狗（中断模式）---
     * 采集链自维持，停住时不会自己出声：请求没发出去、DRDY 边沿丢、IT 传输既不完成
     * 也不报错…… 共同现象都是"链路不再推进"，所以判据取"距上次推进的时长"，
     * 而不是某个具体标志位（旧版只看 armed，请求写失败时 armed=0，看门狗反而不响）。 */
    uint64_t link_us;         // 采集链最近一次推进的时刻 (µs)：请求发出、或一帧发布
    uint64_t link_timeout_us; // 停滞判据 (µs)，Config 时由 meas_delay_ms 推得

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
#define IST8310_INSTANCE_DEF(name)                                                                                     \
    I2C_INSTANCE_DEF(name##_i2c, IST8310_BUFF_SIZE);                                                                   \
    static uint8_t name##_req_buff[1] DMA_RAM = {0};                                                                   \
    GPIO_INSTANCE_DEF(name##_drdy);                                                                                    \
    GPIO_INSTANCE_DEF(name##_rstn);                                                                                    \
    DAEMON_INSTANCE_DEF(name##_daemon);                                                                                \
    static IST8310Instance name = {.i2c_inst = &name##_i2c,                                                            \
                                   .req_buff = name##_req_buff,                                                        \
                                   .drdy = &name##_drdy,                                                               \
                                   .rstn = &name##_rstn,                                                               \
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
 * @note 中断模式在本函数末尾**发出第一条测量请求**：器件没有连续测量模式，
 *       没有这一帧"种子"自维持链就永远不会开始转（此后每次 DRDY 由 EXTI 发起读、
 *       读完成回调发布并续请求，不再需要调用方参与）。
 * @note `work_mode` 还会改变**器件寄存器的配置序列**：只有中断模式才挂 DRDY EXTI 回调，
 *       轮询模式挂上只会白占一个槽位。DRDY 的极性固定为高有效（写 CNTL2.DRP=1），
 *       对应 CubeMX 侧该 EXTI 必须是上升沿 —— 板上事实，不做配置项。
 */
int8_t IST8310Config(IST8310Instance *inst, const IST8310_Config_s *config);

/**
 * @brief 发送一次单次测量请求（写 CNTL1 = SINGLE）
 * @param inst IST8310 实例指针
 * @return 0 已发出；-1 失败
 *
 * @note 传输方式取 `i2c_mode`（BLOCK 同步发完；IT/DMA 发起后等完成回调）。
 *       两种工作模式都用它：轮询模式由调用方编排后接 `IST8310CheckReady`；
 *       中断模式由 `IST8310Config` / 恢复流程 / 链路看门狗在任务上下文调用 ——
 *       正常运行时链路由 I2C 完成回调自己续，调用方不必也不该每周期调它。
 * @note 请求字节放在实例的常驻缓冲（`req_buff`）里而不是调用方的栈上：
 *       中断模式下的请求是**发起即返回**的异步写，HAL 在传输期间一直持有该指针。
 * @note 只能在任务上下文调用（`i2c_mode` 为 IT/DMA 时本函数要等完成回调；
 *       中断里那条免等路径是私有的 `IST8310_TriggerMeas(inst, 0)`，由读完成回调调用）。
 */
int8_t IST8310Request(IST8310Instance *inst);

/**
 * @brief 检查数据是否就绪（读 STAT1.DRDY，轮询模式专用）
 * @param inst IST8310 实例指针
 * @retval 1  已就绪，可以 `IST8310Read`
 * @retval 0  尚未就绪（过一会儿再查）
 * @retval -1 读状态寄存器失败（总线错误，已记入失败计数）
 *
 * @note 传输方式取 `i2c_mode`（对调用方是同步的，三种模式返回语义一致）。
 * @note 读 STAT1 **不会**清 DRDY（只有读输出寄存器或 STAT2 才会），所以可以反复查。
 * @note 中断模式下本函数无意义（就绪由 DRDY 中断表达），会返回 -1 并记录警告。
 */
int8_t IST8310CheckReady(IST8310Instance *inst);

/**
 * @brief 读取一帧磁力数据
 * @param inst IST8310 实例指针
 * @return IST8310_Data_t；失败或尚无有效帧返回全 0（time_stamp 也为 0）
 *
 * @note 行为按工作模式分流：
 *       - **轮询模式**：按 `i2c_mode` 连读 0x03~0x08 六字节并换算、打时间戳；
 *         若实例内部的就绪记录（`CheckReady` 的结论）为"未就绪"，说明调用方跳过了
 *         `IST8310CheckReady`，会**打告警**——这时读到的是上一帧的旧磁场值。
 *       - **中断模式**：返回双缓冲里最新已发布的一帧，不做任何 I2C 传输。
 * @note `i2c_mode` 只影响轮询模式这几笔传输怎么走，返回语义完全一致。
 * @note **只能在任务上下文调用**，且**中断模式下必须周期性调用**：本函数兼作链路
 *       看门狗 —— 距上次推进超过 `link_timeout_us`（采集链停住：请求没发出去 /
 *       DRDY 边沿丢 / 完成回调不来）就补发一次测量请求，连续失败再升级为整段恢复。
 *       中断模式下不再周期性调用它，链路停住就不会被任何人发现。
 * @note 调用前可能执行失败恢复（含阻塞 I2C 与 DeInit/Init），耗时可达百毫秒。
 */
IST8310_Data_t IST8310Read(IST8310Instance *inst);

/**
 * @brief 轮询模式的一站式封装：发请求 → 轮询就绪 → 读数据（轮询模式专用）
 * @param inst             IST8310 实例指针
 * @param poll_interval_ms 两次"检查就绪"之间的间隔(ms)；0 = 用 1ms
 * @param timeout_ms       等待就绪的总超时(ms)；0 = 用默认值 20ms
 * @return IST8310_Data_t；任一步失败或超时返回全 0（time_stamp 也为 0）
 *
 * @note 等价的展开式：`IST8310Request` → 等 meas_delay → 反复 `IST8310CheckReady`
 *       直到就绪 → `IST8310Read`。触发后会先等满器件要求的最小测量间隔
 *       （由 `avg` 档位推得，手册 §3.1.2），再按 `poll_interval_ms` 复查就绪 ——
 *       这样既不会在器件还没测完时白跑 I2C，也不必让调用方自己算这个间隔。
 * @note 超时那一帧**不作为有效数据返回**：会顺手把输出寄存器读掉一次（手册里唯一
 *       能拉低 DRDY 的途径），让器件回到干净状态，但返回全 0 让调用方按失败处理。
 *       否则调用方拿到的是上一帧的旧磁场值却配着新时间戳，从外部完全看不出。
 * @note 入口与 `IST8310Read` 一样会先消费错误回调提出的恢复请求（见
 *       `IST8310_ServiceRecover`）：连续失败累计出的恢复请求不会因为"每次都在失败出口
 *       直接返回"而永远执行不到 —— 那正是最该恢复的场景。
 * @note **只能在任务上下文调用**（内含阻塞式 DWT_Delay 与 I2C 传输）。
 */
IST8310_Data_t IST8310Sample(IST8310Instance *inst, uint32_t poll_interval_ms, uint32_t timeout_ms);

#endif /* defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* __DRV_IST8310_H */
