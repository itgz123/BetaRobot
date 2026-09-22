/**
 * @file ist8310_reg_def.h
 * @brief IST8310 三轴磁力计寄存器地址和位定义
 *
 * @note 依据 IST8310 Datasheet Version 1.2（§6.4 Registers）整理
 * @note 本文件只放"器件事实"：地址、位域、灵敏度、时序。不含任何 HAL/BSP 依赖。
 */

#ifndef __IST8310_REG_DEF_H
#define __IST8310_REG_DEF_H

/*============================ 器件标识 ============================*/

/**
 * @brief I2C 从机地址
 * @note 由 CAD1/CAD0 引脚选择，共 4 组（§2.2 / §6.1.1）：
 *       CAD1=CAD0=VSS → 0x0C(7bit) / 0x18(8bit)
 *       CAD1=VSS,CAD0=VDD → 0x0D(7bit) / 0x1A(8bit)
 *       CAD1=VDD,CAD0=VSS → 0x0E(7bit) / 0x1C(8bit)
 *       CAD1=CAD0=VDD → 0x0F(7bit) / 0x1E(8bit)
 *       **两脚悬空时也是 0x0E/0x1C**，板卡即按悬空默认接法。
 */
#define IST8310_I2C_ADDR_7BIT 0x0E // 从机地址（7 位形式）

/*============================ 寄存器地址定义 ============================*/

#define IST8310_WAI_REG 0x00   // Who Am I（器件 ID）
#define IST8310_STAT1_REG 0x02 // 状态寄存器 1（DOR/DRDY）
#define IST8310_DATAXL_REG 0x03
#define IST8310_DATAXH_REG 0x04
#define IST8310_DATAYL_REG 0x05
#define IST8310_DATAYH_REG 0x06
#define IST8310_DATAZL_REG 0x07
#define IST8310_DATAZH_REG 0x08
#define IST8310_STAT2_REG 0x09   // 状态寄存器 2（INT 标志）
#define IST8310_CNTL1_REG 0x0A   // 控制寄存器 1（工作模式）
#define IST8310_CNTL2_REG 0x0B   // 控制寄存器 2（DREN/DRP/SRST）
#define IST8310_STR_REG 0x0C     // 自检寄存器
#define IST8310_TEMPL_REG 0x1C   // 温度低字节（只读）
#define IST8310_TEMPH_REG 0x1D   // 温度高字节（只读）
#define IST8310_AVGCNTL_REG 0x41 // 平均次数控制
#define IST8310_PDCNTL_REG 0x42  // set/reset 脉冲宽度控制

/* 连续读长度：0x03~0x08 六字节一次读完（地址自增，一次 START 取全三轴） */
#define IST8310_DATA_LEN 6

/*============================ 常量定义 ============================*/

#define IST8310_WAI_VALUE 0x10              // WAI 期望值
#define IST8310_SENSITIVITY_LSB_PER_UT 3.3f // 灵敏度：3.3 LSB/µT（§4.4，即 0.3 µT/LSB）
#define IST8310_AXIS_NUM 3                  // 轴数

/*============================ 寄存器位定义 ============================*/

/**
 * @brief STAT1 (0x02) 状态寄存器 1
 * @note bit[7:2] - reserved
 * @note bit[1] - DOR: 读取前发生过数据跳过；读任一输出寄存器后自动清 0
 * @note bit[0] - DRDY: 数据就绪；读任一输出数据寄存器或 STAT2 后自动清 0
 * @note 访问: RO
 * @note 重要：DRDY **只跟随物理 DRDY 引脚信号**，若 CNTL2.DREN=0 则恒为 0；
 *       且读 STAT1 本身不会清 DRDY，只有读输出寄存器/STAT2 才会清。
 */
typedef enum : uint8_t
{
    IST8310_STAT1_DOR = 1 << 1,  // 有数据被跳过（读取不及时）
    IST8310_STAT1_DRDY = 1 << 0, // 数据已就绪
} IST8310_Stat1_e;

/**
 * @brief STAT2 (0x09) 状态寄存器 2
 * @note bit[7:4] - reserved
 * @note bit[3] - INT: 三轴输出绝对值之和超过 1600µT 时置 1（强外磁提示）
 * @note bit[2:0] - reserved
 * @note 访问: RO；读 STAT2 会清 STAT1.DRDY
 */
typedef enum : uint8_t
{
    IST8310_STAT2_INT = 1 << 3, // 磁场超量程中断标志
} IST8310_Stat2_e;

/**
 * @brief CNTL1 (0x0A) 控制寄存器 1
 * @note bit[7:4] - reserved
 * @note bit[3:0] - mode: 工作模式
 * @note 访问: RW，复位值 0x00
 * @note 器件**没有连续测量模式**：每次采样都要重写一次 0x01。
 *       单次测量完成器件自动回到 Stand-By，CNTL1[3:0] 被硬件清零。
 */
typedef enum : uint8_t
{
    IST8310_CNTL1_MODE_STANDBY = 0x00, // 待机（上电默认）
    IST8310_CNTL1_MODE_SINGLE = 0x01,  // 单次测量
} IST8310_Cntl1Mode_e;

/**
 * @brief CNTL2 (0x0B) 控制寄存器 2
 * @note bit[7:4] - reserved
 * @note bit[3] - DREN: DRDY 引脚总开关（1=使能）
 * @note bit[2] - DRP: DRDY 引脚极性（0=低有效, 1=高有效）
 * @note bit[1] - reserved
 * @note bit[0] - SRST: 软复位，写 1 启动 POR 流程，POR 结束后硬件自清 0
 * @note 访问: RW，复位值 0x0C（DREN=1, DRP=1, SRST=0）
 */
typedef enum : uint8_t
{
    IST8310_CNTL2_DREN = 1 << 3, // DRDY 功能使能（总开关）
    IST8310_CNTL2_DRP = 1 << 2,  // DRDY 高电平有效（0 = 低有效）
    IST8310_CNTL2_SRST = 1 << 0, // 软复位（POR），完成后自清零
} IST8310_Cntl2_e;

/**
 * @brief STR (0x0C) 自检寄存器
 * @note bit[6] - SELF_TEST: 置 1 进入自检模式，三轴输出极性翻转
 * @note 访问: RW，复位值 0x00
 * @note 本驱动未实现自检 API，位定义登记备用
 */
typedef enum : uint8_t
{
    IST8310_STR_SELF_TEST = 1 << 6, // 进入自检模式
} IST8310_Str_e;

/**
 * @brief AVGCNTL (0x41) 平均次数控制寄存器
 * @note bit[5:3] - y 轴平均次数；bit[2:0] - x/z 轴平均次数
 * @note 编码：0=不平均, 1=2次, 2=4次(复位默认), 3=8次, 4=16次, 其余按不平均处理
 * @note 访问: RW，复位值 0x00（但数据手册 §3.1.1 推荐低噪声用 0x24，即两处都 16 次）
 * @note 两次测量之间的**最小等待时间**取决于本寄存器：默认 4 次 → 5ms(200Hz)，
 *       16 次 → 6ms(166Hz)。驱动按此推导 meas_delay。
 */
typedef enum : uint8_t
{
    IST8310_AVG_NONE = 0x00, // 不平均
    IST8310_AVG_2 = 0x01,    // 2 次
    IST8310_AVG_4 = 0x02,    // 4 次（器件复位默认）
    IST8310_AVG_8 = 0x03,    // 8 次
    IST8310_AVG_16 = 0x04,   // 16 次（数据手册推荐的低噪声设置）
    IST8310_AVG_NUM = 5,     // 可选项数量
} IST8310_Avg_e;

/**
 * @brief PDCNTL (0x42) set/reset 脉冲宽度控制寄存器
 * @note bit[7:6] - Pulse duration: 2'b01=Long, 2'b11=Normal, 其余仅极端场合用
 * @note bit[5:0] - reserved
 * @note 访问: RW，复位值 0x00
 * @note 数据手册 §3.1.1：**初始配置必须写 0xC0（Normal）**以优化性能
 */
typedef enum : uint8_t
{
    IST8310_PD_PULSE_LONG = 0x01 << 6,   // 长脉冲
    IST8310_PD_PULSE_NORMAL = 0x03 << 6, // 标准脉冲（推荐，= 0xC0）
} IST8310_PdPulse_e;

/*============================ 寄存器位域联合体 ============================*/

/**
 * @brief AVGCNTL (0x41) 位域联合体
 * @note bit[2:0] avg_xz: x/z 轴平均次数
 *       bit[5:3] avg_y:  y 轴平均次数
 */
typedef union
{
    uint8_t raw;
    struct
    {
        uint8_t avg_xz : 3;   // bits[2:0]
        uint8_t avg_y : 3;    // bits[5:3]
        uint8_t reserved : 2; // bits[7:6]
    } bits;
} IST8310_AvgCntl_u;

/*============================ 时序常量 ============================*/

#define IST8310_POR_DELAY_MS 50     // 上电/软复位后等待 POR 完成（§4.5: POR max 50ms）
#define IST8310_RSTN_PULSE_MS 1     // RSTN 低电平保持时间
#define IST8310_SRST_TIMEOUT_MS 100 // 轮询 SRST 自清的超时上限
#define IST8310_WAI_RETRY 3         // WAI 校验重试次数

#endif /* __IST8310_REG_DEF_H */
