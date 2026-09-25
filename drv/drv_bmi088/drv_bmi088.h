/**
 * @file drv_bmi088.h
 * @brief BMI088 六轴 IMU 驱动（加速度计 + 陀螺仪）
 *
 * @note 本模块只负责与器件通信：寄存器读写/初始化/自检、原始数据 → 物理量
 *       （按数据手册灵敏度）、时间戳、温度、在线状态。不做零偏/标度/正交等
 *       标定，也不做姿态解算 —— 标定与滤波见 drvlib_bmi088_kalman。
 * @note 片选由 DRV 层通过 GPIO 接口控制
 * @note 中断模式使用循环缓冲 + 线性插值对齐 acc/gyro 时间戳
 *
 * @note `work_mode` 与 `spi_mode` 是两个正交的维度（同 drv_ist8310 的 work_mode/i2c_mode）：
 *       work_mode 决定**谁在什么时刻发起**一次采样（任务轮询 / DRDY 中断），
 *       spi_mode  决定**一笔传输怎么做**（阻塞 / 中断 / DMA）—— 它是"运行时默认值"，
 *       本层在每次 `SPITransmitReceive/SPITransmit` 调用时把它传给 bsp（bsp 不持有模式），
 *       而初始化序列这些必须同步完成的地方一律显式传 `BSP_BLOCK_MODE`。
 *       两者唯一的耦合是：`work_mode == INT` 时 `spi_mode` 不能是 BLOCK ——
 *       EXTI 里做阻塞 SPI 会死循环（tick 在中断里不前进）。
 *       其余组合都合法，比如 `POLLING + DMA`（调用方仍是同步地拿到结果，
 *       但传输期间 CPU 是空闲的）。
 *
 * @note TODO 加热器已从本模块移出（原先嵌在 IMU 的实例/配置/数据路径里）。
 *       后续独立为 drv_heater：温度由 BMI088GetTemperature 注入，加热控温
 *       由 app 任务节拍驱动，TIM8 OPM+RCR 安全链归 bsp_tim。
 *       旧实现见 git 历史 drv/drv_bmi088/drv_bmi088_heater.c。
 */

#ifndef __DRV_BMI088_H
#define __DRV_BMI088_H

#include "main.h"
#include "bsp_map.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include "bsp_spi.h"
#include "bsp_gpio.h"
#include "drv_daemon.h"
#include "bmi088_reg_def.h"
#include "bsp_log.h"

/**
 * @brief BMI088 工作模式枚举
 * @note 中断模式的优点：
 * @note 1. 精确时间戳（EXTI 触发时刻，不含 SPI 往返）
 * @note 2. 避免数据覆盖（自由运行下轮询间隔与 ODR 不同步时，轮询会漏帧）
 * @note 3. BMI088Read 在中断模式下不产生任何 SPI 传输，只取缓冲
 * @note 两种模式**器件侧的寄存器配置不同**（见 BMI088Config）：
 *       只有中断模式才配 DRDY 引脚与数据就绪中断映射，轮询模式保持复位默认值 ——
 *       轮询模式开着就绪中断只会白占一个 EXTI 槽位，而引脚却悬空/无回调。
 */
typedef enum : uint8_t
{
    BMI088_MODE_POLLING = 0, // 阻塞模式：主动轮询读取6轴数据
    BMI088_MODE_INT,         // 中断模式：数据就绪后实例自动读取
} BMI088_WorkMode_e;

/**
 * @brief 读取模式（**仅中断模式有意义**，轮询模式忽略）
 * @note 器件自由运行：acc 与 gyro 是两条独立的采样流，ODR 可以不同、时间戳也各自独立。
 *       融合算法对"要不要插值"的需求不同，所以在读取时由调用方指明：
 *       - BMI088_READ_LATEST：各自取最新一帧，保留独立时间戳（多速率融合用，如 Kalman）
 *       - BMI088_READ_INTERP：把两条流线性插值对齐到同一时刻（单速率姿态解算用，如 Mahony）
 * @note 轮询模式之所以忽略它：那一帧是"当场连读三笔"得来的，acc/gyro 本来就是同一
 *       时刻读的、时间戳同源，没有插值可言。
 */
typedef enum : uint8_t
{
    BMI088_READ_LATEST = 0, // 各自最新一帧，保留独立时间戳
    BMI088_READ_INTERP = 1, // 线性插值对齐到同一时间戳
} BMI088_ReadMode_e;

/*============================ 缓冲区大小定义 ============================*/

#define BMI088_BUFF_SIZE 10 // SPI缓冲区大小：1地址 + 1虚拟 + 6数据 + 2温度（最大）

/*============================ 循环缓冲大小 ============================*/
// 用复杂状态机可以用3+3实现
// f_acc=12.5-1600hz
// f_gyro=100-2000hz
#define BMI088_ACC_BUF_SIZE 17
#define BMI088_GYRO_BUF_SIZE 17

/*============================ 数据尺寸常量 ============================*/
#define BMI088_AXIS_NUM 3      // 轴数
#define BMI088_RAW_DATA_SIZE 6 // 原始数据字节数 (3轴 × 2字节)

/*============================ 配置结构体 ============================*/

/**
 * @brief BMI088 配置结构体（用于 BMI088Config）
 *
 * @note 包含硬件枚举和传感器运行时参数。
 *       硬件枚举由 Config 传给子模块，Register 只负责注册。
 * @note 不带标定参数：本层输出的是"未补偿的物理量"，零偏/标度等由
 *       drvlib_bmi088_kalman 统一处理（原先的 gyro_offset/acc_offset 已移除）。
 */
typedef struct
{
    /* 硬件枚举（Config时传给子模块） */
    BoardSPI_e spi_e;       // 板载SPI枚举
    BoardGPIO_e cs_acc_e;   // 加速度计片选GPIO枚举
    BoardGPIO_e cs_gyro_e;  // 陀螺仪片选GPIO枚举
    BoardGPIO_e int_acc_e;  // 加速度计中断GPIO枚举
    BoardGPIO_e int_gyro_e; // 陀螺仪中断GPIO枚举

    /* 传感器运行时参数 */
    uint16_t daemon_reload;           // daemon 喂狗重载值，0 表示禁用
    DaemonFaultAction_e daemon_fault; // daemon 离线故障动作
    BMI088_AccRange_e acc_range;      // 加速度计量程
    uint8_t acc_bwp;                  // 加速度计低通滤波器带宽
    uint8_t acc_odr;                  // 加速度计输出数据速率
    BMI088_GyroRange_e gyro_range;    // 陀螺仪量程
    BMI088_GyroConf_e gyro_conf;      // 陀螺仪 ODR+BW 组合配置（见 BMI088_GyroConf_e）
    BMI088_WorkMode_e work_mode;      // 采样由谁驱动（轮询/中断）
    BSP_Transfer_Mode_e spi_mode;     // 传输怎么做（阻塞/IT/DMA），与 work_mode 正交
    uint32_t spi_timeout_ms;          // SPI 传输超时(ms)：BLOCK 透传 HAL，IT/DMA 用于等总线就绪
} BMI088_Config_s;
/**
 * @brief BMI088 三轴原始数据联合体
 * @note 加速度计/陀螺仪各 3 轴 × int16 = 6 字节
 *       小端字节序（与 Cortex-M 一致），可直接作为 int16 数组访问
 */
typedef union
{
    uint8_t bytes[6]; // 原始字节
    int16_t axis[3];  // 三轴数组: [x, y, z]
#pragma pack(push, 1)
    struct
    {
        int16_t x; // X 轴
        int16_t y; // Y 轴
        int16_t z; // Z 轴
    };
#pragma pack(pop)
} BMI088_AxisRaw_u;

/**
 * @brief IMU 多速率数据结构体（用于 Kalman 等需要独立时间戳的融合算法）
 */
typedef struct
{
    float gyro[BMI088_AXIS_NUM]; // 陀螺仪数据 (rad/s)
    float acc[BMI088_AXIS_NUM];  // 加速度计数据 (m/s²)
    uint64_t time_stamp_a;       // 加速度计时间戳 (μs)
    uint64_t time_stamp_g;       // 陀螺仪时间戳 (μs)
} BMI088_Data_t;

/**
 * @brief BMI088 实例结构体
 * @note 使用指针指向 BSP 实例；SPI 的 parent 由 SPIConfig 写入（不再由本驱动直写），
 *       GPIO 的 parent 仍由本驱动在注册时直写（bsp_gpio 尚未按同一模板迁移）
 */
typedef struct BMI088Instance
{
    /* BSP 实例指针 */
    SPIInstance *spi_inst;  // SPI 实例
    GPIOInstance *cs_acc;   // 加速度计片选
    GPIOInstance *cs_gyro;  // 陀螺仪片选
    GPIOInstance *int_acc;  // 加速度计中断
    GPIOInstance *int_gyro; // 陀螺仪中断
    DaemonInstance *daemon; // 守护进程实例

    /* 发送缓冲区 */
    uint8_t *tx_buff; // 发送缓冲区指针
    uint8_t tx_len;   // 发送数据长度

    uint32_t spi_timeout_ms; // SPI 传输超时(ms)（Config 写入；IT/DMA 的 DRDY 中断发起固定传 0）

    /* 加速度计配置 */
    BMI088_AccRange_e acc_range; // 量程
    uint8_t acc_bwp;             // 低通滤波器带宽
    uint8_t acc_odr;             // 输出数据速率

    /* 陀螺仪配置 */
    BMI088_GyroRange_e gyro_range; // 量程
    BMI088_GyroConf_e gyro_conf;   // 陀螺仪 ODR+BW 组合配置（见 BMI088_GyroConf_e）

    /* 工作模式 */
    BMI088_WorkMode_e work_mode;   // 采样由谁驱动（轮询/中断）
    BSP_Transfer_Mode_e spi_mode;  // 传输怎么做（阻塞/IT/DMA），每次调用透传给 bsp

    /*============================ 中断模式字段 ============================*/

    /* --- EXTI/SPI 同步 --- */
    volatile uint8_t transfer_busy;  // SPI IT 传输进行中
    volatile uint8_t current_sensor; // 当前 SPI 读取的传感器 (BMI088_Sensor_e)
    uint8_t pending_mask;            // 待读取传感器掩码 (BMI088_Pending_e)

    /* --- 任务上下文的异步传输同步（spi_mode 为 IT/DMA 时用） --- */
    volatile uint8_t xfer_done;  // 当前这笔异步传输已完成（回调在中断里置位）
    volatile uint8_t xfer_error; // 当前这笔异步传输已失败（错误回调在中断里置位）

    /* --- 失败计数与恢复记账（仅调试/日志） --- */
    uint8_t fail_count;                // 连续失败次数，成功即清零
    volatile uint8_t recover_request;  // 任务上下文需执行 SPI 卡死自恢复（失败攒够阈值后置位）

    /* --- 中断时间戳缓存 --- */
    uint64_t int_timestamp;  // 当前 SPI 读取对应的 INT 触发时间
    uint64_t pending_t_acc;  // 暂存的 acc INT 时间（pending 用）
    uint64_t pending_t_gyro; // 暂存的 gyro INT 时间（pending 用）

    /* --- 循环缓冲（写入 ISR，读出配对） --- */
    uint8_t acc_raw[BMI088_ACC_BUF_SIZE][6];   // 加速度计原始环形缓冲
    uint8_t gyro_raw[BMI088_GYRO_BUF_SIZE][6]; // 陀螺仪原始环形缓冲
    uint64_t t_acc[BMI088_ACC_BUF_SIZE];       // 加速度计时间戳 (us)
    uint64_t t_gyro[BMI088_GYRO_BUF_SIZE];     // 陀螺仪时间戳 (us)
    volatile uint16_t acc_wr_idx;              // 加速度计写入索引（永远递增）
    volatile uint16_t gyro_wr_idx;             // 陀螺仪写入索引（永远递增）
    volatile uint8_t acc_cnt;                  // 已收到的 acc 样本数
    volatile uint8_t gyro_cnt;                 // 已收到的 gyro 样本数
    volatile float temperature;                // 实际温度（℃）
    uint64_t last_temp_us;                     // 上次温度读取时间戳 (us)，用于限速
} BMI088Instance;

/*============================ 实例定义宏 ============================*/

/**
 * @brief BMI088实例静态定义宏
 * @param name 实例名称
 *
 * @note 使用 BSP 层的实例定义宏；SPI 的 parent 由 SPIConfig 写入，
 *       GPIO 的 parent 在 BMI088Register 里设置
 *       中断模式字段由 BMI088Config 初始化
 *
 * @example
 *   BMI088_INSTANCE_DEF(bmi088);
 */
#define BMI088_INSTANCE_DEF(name)                                  \
    static uint8_t name##_tx_buff[BMI088_BUFF_SIZE] DMA_RAM = {0}; \
    SPI_INSTANCE_DEF(name##_spi, BMI088_BUFF_SIZE);                \
    GPIO_INSTANCE_DEF(name##_cs_acc);                              \
    GPIO_INSTANCE_DEF(name##_cs_gyro);                             \
    GPIO_INSTANCE_DEF(name##_int_acc);                             \
    GPIO_INSTANCE_DEF(name##_int_gyro);                            \
    DAEMON_INSTANCE_DEF(name##_daemon);                            \
    static BMI088Instance name = {                                 \
        .spi_inst = &name##_spi,                                   \
        .cs_acc = &name##_cs_acc,                                  \
        .cs_gyro = &name##_cs_gyro,                                \
        .int_acc = &name##_int_acc,                                \
        .int_gyro = &name##_int_gyro,                              \
        .daemon = &name##_daemon,                                  \
        .tx_buff = name##_tx_buff}

/*============================ 公开接口声明 ============================*/

/**
 * @brief 注册BMI088实例（仅调用一次）
 * @param inst BMI088实例指针
 * @return 0成功，-1失败
 *
 * @note 注册 SPI/GPIO/PWM/Daemon 子模块。
 *       不配置硬件参数（由 BMI088Config 负责）。
 */
int8_t BMI088Register(BMI088Instance *inst);

/**
 * @brief 配置BMI088实例（可重复调用）
 * @param inst   BMI088实例指针
 * @param config 配置结构体指针（硬件枚举 + 传感器参数 + daemon 配置）
 * @return 0成功，-1失败
 *
 * @note 填充子模块硬件映射，配置 daemon 和传感器初始化。
 *       可重复调用以重新配置传感器参数。
 *       要求在 BMI088Register 之后调用。
 * @note `work_mode` 会改变**器件寄存器的配置序列**：只有中断模式才写
 *       acc 的 INT1_IO_CTRL/INT_MAP_DATA 与 gyro 的 INT_CTRL/INT3_INT4_IO_CONF/
 *       INT3_INT4_IO_MAP（数据就绪引脚与中断映射），轮询模式整段跳过、
 *       保持软复位后的默认值（中断关闭）。同理，EXTI 回调也只在中断模式挂。
 */
int8_t BMI088Config(BMI088Instance *inst, const BMI088_Config_s *config);

/**
 * @brief 读取一帧 IMU 数据
 * @param inst      BMI088实例指针
 * @param read_mode 插值模式（BMI088_ReadMode_e）；**轮询模式忽略它**
 * @return BMI088_Data_t；失败或尚无有效数据返回全 0（两个时间戳也为 0）
 *
 * @note 行为按工作模式分流：
 *       - **轮询模式**：按 Acc→Gyro→Temp 串行读一遍，传输走 `spi_mode`
 *         （BLOCK / IT / DMA 对调用方语义一致：返回时缓冲里一定是刚读到的新数据）。
 *         时间戳是读完那一刻的 DWT 值，两条流共用 —— 它们本来就是同一时刻读的。
 *       - **中断模式**：**不做任何 SPI 传输**，只从环形缓冲取数（采样由 DRDY 中断驱动）。
 *         `BMI088_READ_LATEST` 各自取最新一帧、保留独立时间戳；
 *         `BMI088_READ_INTERP` 按时间戳把其中一条流插值到另一条的最新时刻上，
 *         两个时间戳相等。插值需要两条流各自至少两个样本，不够时返回全 0。
 * @note **任一数据读失败就整帧作废**（轮询模式，返回全 0），绝不把残留缓冲配上新时间戳
 *       发出去 —— 那样调用方拿到的会是上一帧的旧姿态，从外部完全看不出。温度是辅助量，
 *       读失败不作废整帧，但也不刷新新鲜度（BMI088GetTemperature 仍返回上一次的值）。
 * @note 只在成功时喂 daemon：全失败还喂狗会让看门狗对断链视而不见。
 * @note **只能在任务上下文调用**：本函数兼作两条自愈入口 —— SPI 传输卡死
 *       （SPIRecoverTxIfStuck，见 bsp_spi.md）与错误回调攒够失败后的恢复请求，
 *       内部都可能真正中止/重建 SPI 硬件。每控制周期调一次即可；
 *       总线健康时开销是一次 DWT 读。
 */
BMI088_Data_t BMI088Read(BMI088Instance *inst, BMI088_ReadMode_e read_mode);

/**
 * @brief 读取最近一次采到的 BMI088 温度
 * @param inst BMI088实例指针
 * @return 温度 (℃)；尚无有效读数（从未采到，或 inst 为空）返回 NAN
 * @note 温度在中断模式由 acc 完成回调限速读取（数据手册 §5.3.7，约 1.28s 一次），
 *       轮询模式在 BMI088Read 的轮询分支里更新，两者都会刷新新鲜度时间戳。
 *       限速意味着返回值最多滞后约 1.28s；对温度这类慢变量无影响。
 * @note 供温补/独立加热模块使用：返回 NAN 即"温度不可用"，
 *       调用方（如加热器看门狗）应据此走安全侧。
 */
float BMI088GetTemperature(const BMI088Instance *inst);

#endif /* defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* __DRV_BMI088_H */
