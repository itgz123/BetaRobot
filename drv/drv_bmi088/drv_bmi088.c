/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"
#include "app_cfg.h"

#ifdef DRV_BMI088_USED

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include <string.h>
#include <math.h>
#include "bsp_dwt.h"
#include "bsp_log.h"

/* bmi088 日志实例（日志关闭时宏为空、不分配，BSPLOG 空宏不引用） */
#ifndef DRV_BMI088_LOG_LIMIT
#define DRV_BMI088_LOG_LIMIT 10
#endif // !DRV_BMI088_LOG_LIMIT
LOG_INSTANCE_DEF(g_bmi088_log, "drv_bmi088", DRV_BMI088_LOG_LIMIT);

/* 基于数据手册的专用延时宏 */
#define BMI088_SPI_SWITCH_DELAY_S 0.002f   // SPI模式切换延时  (2ms)
#define BMI088_ACC_RESET_DELAY_S 0.002f    // 加速度计软复位延时 (2ms，数据手册1ms+余量)
#define BMI088_ACC_PWR_UP_DELAY_S 0.00045f // 加速度计上电等待 (450µs，数据手册§3)
#define BMI088_GYRO_RESET_DELAY_S 0.035f   // 陀螺仪软复位延时   (35ms，数据手册30ms+余量)

#define BMI088_WRITE_CHECK_INIT_S 0.001f    // 写验证初始等待    (1ms, DWT_Delay微秒)
#define BMI088_WRITE_CHECK_TIMEOUT_US 10000 // 写验证超时时间   (10ms, DWT_GetTimeUs微秒比较)
#define BMI088_CS_RELEASE_DELAY_S 0.08f     // CS释放后等待时间(s)
// SPI 传输超时统一取 Config 的 spi_timeout_ms（原先 BLOCK 用写死的 100ms、
// IT/DMA 用 Config 值，两套超时）；BLOCK 直接透传给 HAL，IT/DMA 用于等总线就绪
#define BMI088_TEMP_UPDATE_INTERVAL_US 1280000 // 温度读取间隔 (1.28s，数据手册§5.3.7)

/*============================ 失败处理阈值 ============================*/
#define BMI088_RECOVER_FAIL_TH 3  // 连续失败多少次请求 SPI 卡死自恢复
#define BMI088_RECOVER_STUCK_MS 1 // 事件驱动自恢复的"卡死"判据(ms)：见 BMI088_ServiceRecover

/*============================ SPI通信宏定义 ============================*/
#define BMI088_SPI_READ_CMD 0x80   // R/W位=1（读）
#define BMI088_SPI_WRITE_MASK 0x7F // 写掩码：清除 bit7（R/W位）
#define BMI088_SPI_DUMMY_BYTE 0x55 // 哑指令（用于发送时填充）

/*============================ SPI 传输长度 ============================*/
#define BMI088_SPI_WRITE_LEN 2     // SPI写传输长度：1地址 + 1数据
#define BMI088_ACC_SPI_READ_LEN 8  // Acc读传输长度：1地址 + 1dummy + 6数据
#define BMI088_GYRO_SPI_READ_LEN 7 // Gyro读传输长度：1地址 + 6数据

/*============================ RX 缓冲偏移 ============================*/
#define BMI088_ACC_RX_DATA_OFF 2  // Acc数据在rx_buff中的起始偏移(1addr+1dummy)
#define BMI088_GYRO_RX_DATA_OFF 1 // Gyro数据在rx_buff中的起始偏移(1addr)
#define BMI088_ACC_DUMMY_BYTES 1  // Acc SPI读需要1个dummy字节
#define BMI088_GYRO_DUMMY_BYTES 0 // Gyro SPI读不需要dummy字节
/* 温度在Acc rx_buff中的偏移（从TEMP_M(0x22)开始读：rx[2]=TEMP_MSB, rx[3]=TEMP_LSB） */
#define BMI088_ACC_RX_TEMP_L 3 // 温度低字节(TEMP_LSB)在rx_buff中的偏移
#define BMI088_ACC_RX_TEMP_H 2 // 温度高字节(TEMP_MSB)在rx_buff中的偏移

/*============================ 插值阈值 ============================*/
#define BMI088_MIN_SAMPLE_COUNT 2 // 插值所需最小样本数
#define BMI088_INTERP_ROUND 0.5f  // 插值四舍五入

/*============================ 物理常量 ============================*/
#define BMI088_GRAVITY 9.80665f // 重力加速度 (m/s²)

/*============================ 温度转换参数 ============================*/
#define BMI088_TEMP_SENS 0.125f  // 温度灵敏度 (°C/LSB)
#define BMI088_TEMP_OFFSET 23.0f // 温度偏移 (°C)

/*============================ 内部传感器类型定义 ============================*/
typedef enum : uint8_t
{
    BMI088_SENSOR_ACC = 0,
    BMI088_SENSOR_GYRO,
    BMI088_SENSOR_TEMP,
} BMI088_Sensor_e;

typedef enum : uint8_t
{
    BMI088_PENDING_ACC = 1 << BMI088_SENSOR_ACC,
    BMI088_PENDING_GYRO = 1 << BMI088_SENSOR_GYRO,
    BMI088_PENDING_TEMP = 1 << BMI088_SENSOR_TEMP,
} BMI088_Pending_e;

/*============================ 灵敏度查找表 ============================*/

/* 加速度计灵敏度查找表：将原始值转换为 m/s²，单位 (m/s²)/LSB */
/* 计算：range(g) × BMI088_GRAVITY / 32768 */
const float BMI088_AccSenTable[BMI088_ACC_RANGE_NUM] = {
    0.0008980417f, // ±3g:  3 × 9.80665 / 32768
    0.0017960835f, // ±6g:  6 × 9.80665 / 32768
    0.0035921669f, // ±12g: 12 × 9.80665 / 32768
    0.0071843337f, // ±24g: 24 × 9.80665 / 32768
};

/* 陀螺仪灵敏度查找表：将原始值转换为 rad/s，单位 (rad/s)/LSB */
const float BMI088_GyroSenTable[BMI088_GYRO_RANGE_NUM] = {
    0.00106526443603169529841533860381f,     // ±2000°/s 量程灵敏度
    0.00053263221801584764920766930190693f,  // ±1000°/s 量程灵敏度
    0.00026631610900792382460383465095346f,  // ±500°/s 量程灵敏度
    0.00013315805450396191230191732547673f,  // ±250°/s 量程灵敏度
    0.000066579027251980956150958662738366f, // ±125°/s 量程灵敏度
};

/*============================ 私有函数声明 ============================*/
// 读写操作（mode 每次传参：初始化序列一律 BSP_BLOCK_MODE，运行时透传 inst->spi_mode）
static int8_t BMI088_WaitXfer(BMI088Instance *inst);
static void BMI088_NoteFailure(BMI088Instance *inst);
static void BMI088_ServiceRecover(BMI088Instance *inst);
static int8_t BMI088_ReadReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint16_t len, uint8_t dummy, BSP_Transfer_Mode_e mode);
static int8_t BMI088_WriteReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data, BSP_Transfer_Mode_e mode);
static uint8_t BMI088_WriteRegWithCheck(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data, BSP_Transfer_Mode_e mode);
/* 中断模式私有函数 */
static void BMI088_IntCallback(GPIOInstance *gpio_inst);
static void BMI088_SpiAbort(BMI088Instance *inst);
static void BMI088_StartSensorDMA(BMI088Instance *inst, uint8_t sensor_type);
static void BMI088_SPICpltCallback(SPIInstance *spi_inst);
static void BMI088_SPITxCpltCallback(SPIInstance *spi_inst);
static void BMI088_SPIErrCallback(SPIInstance *spi_inst, SPI_ErrReason_e reason);
static void BMI088_CheckPendingIT(BMI088Instance *inst);
/* 插值辅助函数声明 */
static void BMI088_InterpRaw(const uint8_t *raw_old, const uint8_t *raw_new, uint64_t t_old, uint64_t t_new, uint64_t t_target, uint8_t *out);
static uint8_t BMI088_FindBracket(const uint64_t *t_buf, uint16_t newest, uint16_t n, uint64_t target, uint16_t *out_older, uint16_t *out_newer);
static void BMI088_RawToAcc(const uint8_t *raw, float *out, BMI088_AccRange_e range);
static void BMI088_RawToGyro(const uint8_t *raw, float *out, BMI088_GyroRange_e range);
static float BMI088_ParseTempCelsius(const uint8_t *rx_buff);
static void BMI088_PackRaw(BMI088Instance *inst, const uint8_t acc_raw[BMI088_RAW_DATA_SIZE],
                           const uint8_t gyro_raw[BMI088_RAW_DATA_SIZE],
                           uint64_t ts_a, uint64_t ts_g, BMI088_Data_t *out);
/* 三条读取路径（由 BMI088Read 按 work_mode / read_mode 分派） */
static BMI088_Data_t BMI088_ReadPolling(BMI088Instance *inst);
static BMI088_Data_t BMI088_ReadInterp(BMI088Instance *inst);
static BMI088_Data_t BMI088_ReadLatest(BMI088Instance *inst);

/*============================ 私有函数实现 ============================*/
/**
 * @brief 等一笔异步（IT/DMA）传输落地
 * @retval 0 成功；-1 失败或超时
 *
 * @note 本函数是忙等，**只能在任务上下文调用**：等的是另一个中断里的回调，
 *       跑在中断里就是死循环。
 * @note 超时只记账返回，不在这里做收尾：此时 HAL 的传输可能还在飞，驱动无法安全地中止它。
 *       留下的 xfer_done/xfer_error 脏值不会污染下一笔传输（ReadReg/WriteReg 发起前都会清零），
 *       真卡死的句柄由 fail_count 攒够后的 recover_request → BMI088_ServiceRecover 复位。
 */
static int8_t BMI088_WaitXfer(BMI088Instance *inst)
{
    uint64_t start_us = DWT_GetTimeUs();
    uint64_t timeout_us = (uint64_t)inst->spi_timeout_ms * 1000ull;

    while (inst->xfer_done == 0)
    {
        if (inst->xfer_error != 0)
        {
            return -1;
        }
        if ((DWT_GetTimeUs() - start_us) > timeout_us)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "Async xfer timeout (%dms), frame dropped", (int)inst->spi_timeout_ms);
            return -1;
        }
    }

    return (inst->xfer_error != 0) ? -1 : 0;
}

/**
 * @brief 记一次失败：连续失败攒够阈值就请求 SPI 卡死自恢复
 * @note 阈值逻辑收在一处：三条失败路径（轮询读失败、EXTI 里发起失败、异步硬件错）
 *       共用同一份 fail_count，避免"一次失败记两笔"或"某条路径忘了置请求"。
 */
static void BMI088_NoteFailure(BMI088Instance *inst)
{
    if (inst->fail_count < UINT8_MAX)
    {
        inst->fail_count++;
    }
    if (inst->fail_count >= BMI088_RECOVER_FAIL_TH)
    {
        /* 真正的 Abort 要等 HAL tick，只能在任务上下文做（这里可能在 ISR 里），故只置请求 */
        inst->recover_request = 1;
    }
}

/**
 * @brief 消费一次"SPI 卡死"恢复请求（**任务上下文**，解引用前须判 inst 非空）
 *
 * @note 请求由 BMI088_SpiAbort / 轮询失败置位，置位点可能在 ISR 里（DRDY EXTI 发起的
 *       传输启动失败）——那里既不能等 HAL tick 也就无法真正 Abort，只能改期到这里。
 *       这与 bsp_spi.md 给 SPIRecoverTxIfStuck 定的用法一致。
 * @note 判据传 1ms 而不是 SPIRecoverTxIfStuck 的默认 20ms：置位意味着上一笔**已确认失败**、
 *       总线本应立即空闲，此刻仍非 READY 就只能是卡死（一笔 8 字节传输约 20µs）。
 * @note 请求标志无论结果如何都清掉：bsp 返回 BSP_BUSY 的另一半含义是"总线本来就空闲"，
 *       那种情况下本就没有要恢复的东西。真卡着的话失败会自重复（每个 DRDY 都再置位一次），
 *       即便这里丢掉一次尝试（比如当时在临界区被 bsp 拒绝）也不会漏掉恢复。
 */
static void BMI088_ServiceRecover(BMI088Instance *inst)
{
    if (inst->recover_request == 0)
    {
        return;
    }
    inst->recover_request = 0;

    if (SPIRecoverTxIfStuck(inst->spi_inst, BMI088_RECOVER_STUCK_MS) == BSP_OK)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "SPI stuck recovered (fail_count=%d)", (int)inst->fail_count);
    }
}

/**
 * @brief 读寄存器，结果在 inst->spi_inst->rx_buff[dummy .. dummy+len-1]
 * @param inst  BMI088实例
 * @param cs    片选（inst->cs_acc 或 inst->cs_gyro）
 * @param reg   寄存器地址
 * @param len   数据长度（不含 dummy 字节）
 * @param dummy dummy 字节数（Acc 读为 1，Gyro 读为 0）
 * @param mode  本次传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @retval 0 成功；-1 失败或超时
 *
 * @note 对调用方而言三种模式**语义完全一致**：返回 0 时缓冲里一定是新数据。
 *       异步模式下本函数自己等回调（BMI088_WaitXfer），所以上层代码不用区分模式，
 *       差别只在传输期间 CPU 是空闲的。
 * @note **片选跨整笔传输保持有效**：IT/DMA 下要等回调收尾之后才拉高。
 * @note **模式每次传参**而不是从 inst->spi_mode 取：初始化序列这类必须同步完成的地方
 *       显式传 BSP_BLOCK_MODE，与运行时默认值无关。
 * @note 只能传 IT/DMA 且运行在任务上下文时才安全；中断里要用 bsp 接口直接发起并传
 *       timeout_ms = 0（见 BMI088_StartSensorDMA）。
 */
static int8_t BMI088_ReadReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint16_t len, uint8_t dummy, BSP_Transfer_Mode_e mode)
{
    if (len > BMI088_RAW_DATA_SIZE)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "Read len > 6: %d", len);
        return -1;
    }

    memset(inst->tx_buff, BMI088_SPI_DUMMY_BYTE, BMI088_BUFF_SIZE);
    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;
    inst->tx_len = (uint8_t)(1 + dummy + len);

    if (mode != BSP_BLOCK_MODE)
    {
        /* 发起前清标志：否则上一笔残留的完成标记会让本笔立刻"成功"返回，
         * 调用方拿到的其实是上一次读剩下的旧缓冲 */
        inst->xfer_done = 0;
        inst->xfer_error = 0;
    }

    GPIOReset(cs);
    BSP_Status_e st = SPITransmitReceive(inst->spi_inst, inst->tx_buff, inst->tx_len, mode, inst->spi_timeout_ms);
    if (st == BSP_OK && mode != BSP_BLOCK_MODE)
    {
        st = (BMI088_WaitXfer(inst) == 0) ? BSP_OK : BSP_HW_ERR;
    }
    GPIOSet(cs);

    if (st != BSP_OK)
    {
        /* 启动失败不会有完成回调，bsp_spi 只按返回码上报（不像 bsp_i2c 会回调 err_callback），
         * 失败计数只能由调用方记（轮询一整帧记一次，见 BMI088_ReadPolling） */
        BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "%s read 0x%02X failed",
               (cs == inst->cs_acc) ? "Acc" : "Gyro", reg);
        return -1;
    }

    return 0;
}

/**
 * @brief 写寄存器（无验证）
 * @param mode 本次传输模式，语义同 BMI088_ReadReg
 * @retval 0 成功；-1 失败或超时
 *
 * @note data 按值拷进实例自带的静态 tx_buff，异步模式下缓冲生存期天然覆盖到回调结束，
 *       与 drv_ist8310 一样不存在"不能传栈变量"的约束。
 */
static int8_t BMI088_WriteReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data, BSP_Transfer_Mode_e mode)
{
    memset(inst->tx_buff, BMI088_SPI_DUMMY_BYTE, BMI088_BUFF_SIZE);
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_MASK;
    inst->tx_buff[1] = data;
    inst->tx_len = BMI088_SPI_WRITE_LEN;

    if (mode != BSP_BLOCK_MODE)
    {
        inst->xfer_done = 0;
        inst->xfer_error = 0;
    }

    GPIOReset(cs);
    BSP_Status_e st = SPITransmit(inst->spi_inst, inst->tx_buff, inst->tx_len, mode, inst->spi_timeout_ms);
    if (st == BSP_OK && mode != BSP_BLOCK_MODE)
    {
        st = (BMI088_WaitXfer(inst) == 0) ? BSP_OK : BSP_HW_ERR;
    }
    GPIOSet(cs);

    if (st != BSP_OK)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "%s write 0x%02X failed",
               (cs == inst->cs_acc) ? "Acc" : "Gyro", reg);
        return -1;
    }

    return 0;
}

/**
 * @brief 写入寄存器并验证（超时轮询机制；初始化序列专用，mode 固定传 BSP_BLOCK_MODE）
 * @return 0-成功，非0-失败
 *
 * @note 读写自身的返回值**有意忽略**：本函数的判据是"读回来的值对不对"，
 *       读失败只会让 rx_buff 保持残留值 → 同样判为校验失败，语义不变。
 */
static uint8_t BMI088_WriteRegWithCheck(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data, BSP_Transfer_Mode_e mode)
{
    (void)BMI088_WriteReg(inst, cs, reg, data, mode);

    uint64_t start_us = DWT_GetTimeUs();
    DWT_Delay(BMI088_WRITE_CHECK_INIT_S);

    uint8_t rx_off = (cs == inst->cs_acc) ? BMI088_ACC_RX_DATA_OFF : BMI088_GYRO_RX_DATA_OFF;
    do
    {
        (void)BMI088_ReadReg(inst, cs, reg, 1, (cs == inst->cs_acc) ? BMI088_ACC_DUMMY_BYTES : BMI088_GYRO_DUMMY_BYTES, mode);
        if (inst->spi_inst->rx_buff[rx_off] == data)
        {
            return 0;
        }
    } while ((DWT_GetTimeUs() - start_us) < BMI088_WRITE_CHECK_TIMEOUT_US);

    BSPLOG(&g_bmi088_log, LOG_LEVEL_WARNING, "%s write 0x%02X verify failed (expected 0x%02X, got 0x%02X)",
           (cs == inst->cs_acc) ? "Acc" : "Gyro", reg, data, inst->spi_inst->rx_buff[rx_off]);
    return 1;
}

/*============================ 中断模式私有函数实现 ============================*/

/**
 * @brief EXTI中断回调（Acc/Gyro 共用，通过 gpio_inst 区分）
 */
static void BMI088_IntCallback(GPIOInstance *gpio_inst)
{
    BMI088Instance *inst = (BMI088Instance *)gpio_inst->parent;
    uint64_t t_now = DWT_GetTimeUs();

    uint8_t is_acc = (gpio_inst == inst->int_acc);

    /* 先登记本次中断（连同它自己的时间戳），再在总线空闲时统一派发。
     * 注意：新传输只能从这里（EXTI 中断）发起，不能从 BMI088_SPICpltCallback
     * （运行在 RX DMA 完成中断里）发起 —— 见该函数的注释。 */
    if (is_acc)
    {
        inst->pending_t_acc = t_now;
        inst->pending_mask |= BMI088_PENDING_ACC;
    }
    else
    {
        inst->pending_t_gyro = t_now;
        inst->pending_mask |= BMI088_PENDING_GYRO;
    }

    if (!inst->transfer_busy)
    {
        BMI088_CheckPendingIT(inst);
    }

    // 喂狗
    DaemonReload(inst->daemon);
}

/**
 * @brief 丢弃本次传输并释放总线（发起失败 / 传输出错共用的收尾）
 * @param inst BMI088实例
 *
 * @note 两处失败路径收尾动作完全相同，故收在一处：由 BMI088_StartSensorDMA 的同步
 *       失败分支与 BMI088_SPIErrCallback（异步错误）共同调用。
 * @note 只丢弃本次传输，不清环形缓冲和 acc_cnt/gyro_cnt：已采到的数据与
 *       ReadLatest 的可用性都不受影响。
 * @note 顺带记一次连续失败，攒够阈值后置恢复请求。两处调用点都可能在 ISR 里
 *       （DRDY EXTI 发起失败 / SPI_ERR_HW），真正的 Abort 只能改期到任务上下文，
 *       见 BMI088_ServiceRecover。
 */
static void BMI088_SpiAbort(BMI088Instance *inst)
{
    // 释放片选，避免总线被一直拉低
    GPIOSet(inst->cs_acc);
    GPIOSet(inst->cs_gyro);

    // 丢弃本次传输，等下一次 EXTI 重新发起
    inst->transfer_busy = 0;
    inst->pending_mask = 0;

    BMI088_NoteFailure(inst);
}

/**
 * @brief 启动传感器 SPI IT 读取
 */
static void BMI088_StartSensorDMA(BMI088Instance *inst, uint8_t sensor_type)
{
    inst->transfer_busy = 1;
    inst->current_sensor = sensor_type;
    GPIOInstance *cs;

    memset(inst->tx_buff, BMI088_SPI_DUMMY_BYTE, BMI088_BUFF_SIZE);

    if (sensor_type == BMI088_SENSOR_ACC)
    {
        inst->tx_buff[0] = BMI088_ACCEL_XOUT_L | BMI088_SPI_READ_CMD;
        inst->tx_len = BMI088_ACC_SPI_READ_LEN;
        cs = inst->cs_acc;
    }
    else if (sensor_type == BMI088_SENSOR_TEMP)
    {
        inst->tx_buff[0] = BMI088_TEMP_M | BMI088_SPI_READ_CMD;
        inst->tx_len = 1 + BMI088_ACC_DUMMY_BYTES + 2;
        cs = inst->cs_acc;
    }
    else /* BMI088_SENSOR_GYRO */
    {
        inst->tx_buff[0] = BMI088_GYRO_X_L | BMI088_SPI_READ_CMD;
        inst->tx_len = BMI088_GYRO_SPI_READ_LEN;
        cs = inst->cs_gyro;
    }

    GPIOReset(cs);
    /* 传输方式取运行时默认值 spi_mode（Config 已拒绝 INT + BLOCK）。
     * timeout_ms = 0：本函数跑在 DRDY EXTI 里，只判一次总线就绪，忙即放弃本次采样。
     * 绝不能在这里等就绪（旧版传 spi_timeout_ms，且 timeout_ms==0 时还是个死循环）：
     * 中断里等 HAL tick 等不到，卡死时就是永久死等。真卡死由任务上下文的
     * BMI088_ServiceRecover（事件驱动）+ BMI088Read 里的周期检查复位。
     * transfer_busy 已经保证不会与上一笔并发，所以这里的"忙"只可能是异常残留。 */
    if (SPITransmitReceive(inst->spi_inst, inst->tx_buff, inst->tx_len,
                           inst->spi_mode, 0) != BSP_OK)
    {
        /* 发起失败不会有 BMI088_SPICpltCallback，必须在这里就地收尾，
         * 让下一次 EXTI 重新发起（err_callback 只管"已经启动的传输"出的错） */
        BMI088_SpiAbort(inst);
    }
}

/**
 * @brief SPI 传输完成回调（运行在 RX DMA 完成中断里）
 * @note Acc 完成后再调度温度读取（限速1.28s）
 *
 * @note 本回调**只做记账并释放总线，绝不发起新传输**。
 *       DMA 全双工收发时本回调挂在 RX 流的完成回调链上
 *       （HAL: SPI_DMATransmitReceiveCplt → HAL_SPI_TxRxCpltCallback），
 *       而 TX 流的完成中断因与 RX 流同优先级、IRQ 号更大
 *       (DMA2_Stream3_IRQn=59 > DMA2_Stream0_IRQn=56) 此时尚未执行，
 *       hdmatx 的 State 仍是 HAL_DMA_STATE_BUSY。若在此发起下一笔传输，
 *       HAL_DMA_Start_IT 会返回 HAL_BUSY → HAL_SPI_TransmitReceive_DMA 置
 *       HAL_SPI_ERROR_DMA 直接返回且不复位 hspi->State（永久停在 BUSY_TX_RX），
 *       transfer_busy 再也清不掉，整个 BMI088 永久失联。
 *       因此新传输统一由下一次 EXTI（BMI088_IntCallback）经 CheckPendingIT 发起。
 * @note 与写完成**分成两个回调**（rx_callback / tx_callback）：本回调只管
 *       "全双工读回来的数据"，写完成走 BMI088_SPITxCpltCallback，两者不会互相误判。
 */
static void BMI088_SPICpltCallback(SPIInstance *spi_inst)
{
    BMI088Instance *inst = (BMI088Instance *)spi_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    /* 先记账：spi_mode 为 IT/DMA 时，轮询模式的调用方靠它让 BMI088_WaitXfer 收尾。
     * 那笔读可能是中间过程，调用方在 WaitXfer 返回后自己去取 rx_buff —— 下面绝不能
     * 把它当成传感器采样去发布，也不能动 fail_count。 */
    inst->xfer_done = 1;

    if (inst->work_mode != BMI088_MODE_INT)
    {
        return;
    }

    /* 一笔真读成功即视为链路健康 */
    inst->fail_count = 0;

    if (inst->current_sensor == BMI088_SENSOR_ACC)
    {
        /* === Acc SPI IT 完成 === */
        GPIOSet(inst->cs_acc);

        uint16_t i = inst->acc_wr_idx % BMI088_ACC_BUF_SIZE;
        uint8_t off = BMI088_ACC_RX_DATA_OFF;
        memcpy(inst->acc_raw[i], &spi_inst->rx_buff[off], BMI088_RAW_DATA_SIZE);
        inst->t_acc[i] = inst->int_timestamp;
        inst->acc_wr_idx++;
        if (inst->acc_cnt < UINT8_MAX)
            inst->acc_cnt++;

        /* 温度每 1.28s 更新一次（§5.3.7），通过 pending 机制调度 */
        uint64_t now = DWT_GetTimeUs();
        if (now - inst->last_temp_us >= BMI088_TEMP_UPDATE_INTERVAL_US)
        {
            inst->last_temp_us = now;
            inst->pending_mask |= BMI088_PENDING_TEMP;
        }

        /* 释放总线，下一笔传输由下一次 EXTI 经 CheckPendingIT 发起 */
        inst->transfer_busy = 0;
    }
    else if (inst->current_sensor == BMI088_SENSOR_GYRO)
    {
        /* === Gyro SPI IT 完成 === */
        GPIOSet(inst->cs_gyro);

        uint16_t i = inst->gyro_wr_idx % BMI088_GYRO_BUF_SIZE;
        uint8_t off = BMI088_GYRO_RX_DATA_OFF;
        memcpy(inst->gyro_raw[i], &spi_inst->rx_buff[off], BMI088_RAW_DATA_SIZE);
        inst->t_gyro[i] = inst->int_timestamp;
        inst->gyro_wr_idx++;
        if (inst->gyro_cnt < UINT8_MAX)
            inst->gyro_cnt++;

        inst->transfer_busy = 0;
    }
    else /* BMI088_SENSOR_TEMP */
    {
        /* === 温度 SPI IT 完成 === */
        GPIOSet(inst->cs_acc);

        /* 提取 11-bit 温度值（§5.3.7: Temp_uint11 = (TEMP_MSB*8) + (TEMP_LSB/32)） */
        inst->temperature = BMI088_ParseTempCelsius(spi_inst->rx_buff);

        inst->transfer_busy = 0;
    }
}

/**
 * @brief SPI 发送完成回调（只发不收的那笔）
 * @note 只记账：spi_mode 为 IT/DMA 时 BMI088_WaitXfer 靠它收尾。
 *       写没有"发布采样"这回事，也不该动 fail_count —— 一次中间写成功
 *       不能把前面真失败攒下的计数清零（同 BMI088_SPICpltCallback 的约定）。
 */
static void BMI088_SPITxCpltCallback(SPIInstance *spi_inst)
{
    BMI088Instance *inst = (BMI088Instance *)spi_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    inst->xfer_done = 1;
}

/**
 * @brief SPI 传输错误回调（bsp_spi 在已启动的传输出错 / 被强制收尾时调用）
 * @param spi_inst SPI 实例（parent 指向 BMI088Instance）
 * @param reason   出错原因（SPI_ERR_HW 硬件错 / SPI_ERR_ABORT 被 bsp 卡死自恢复中止）
 *
 * @note 这类失败不会再有 BMI088_SPICpltCallback，必须在这里复位传输状态，
 *       否则 transfer_busy 恒为 1，驱动永久失联（现象：acc_cnt/gyro_cnt 不再增长、
 *       pending_t_* 每次中断都被重写、euler 恒为 0）。
 *       两种原因的处理相同（都是丢弃本次传输），故收在 BMI088_SpiAbort 里；
 *       失败计数与恢复请求也在那条路径上统一记（BMI088_NoteFailure）。
 * @note 可能跑在 ISR（SPI_ERR_HW）也可能跑在任务上下文（SPI_ERR_ABORT，由
 *       BMI088Read → SPIRecoverTxIfStuck 触发），因此只能做置标志这类无阻塞动作。
 */
static void BMI088_SPIErrCallback(SPIInstance *spi_inst, SPI_ErrReason_e reason)
{
    BMI088Instance *inst = (BMI088Instance *)spi_inst->parent;

    (void)reason;

    if (inst == NULL)
    {
        return;
    }

    /* 先记账：轮询模式下调用方靠它知道这笔传输没成功
     * （bsp 那头也会返回非 BSP_OK，两条路都指向同一个结论） */
    inst->xfer_error = 1;

    BMI088_SpiAbort(inst);
}

/**
 * @brief 检查并处理待读取的传感器
 * @note 只允许在 EXTI 中断（BMI088_IntCallback）里调用，见 BMI088_SPICpltCallback 的注释
 */
static void BMI088_CheckPendingIT(BMI088Instance *inst)
{
    /* 温度必须排在最前，否则会永远饿死：本函数只在 BMI088_IntCallback 里调用，
     * 而那里在调用前一定刚置上了 ACC 或 GYRO 位。若先判 ACC/GYRO，
     * 每轮都会清掉它们并启动传输（transfer_busy=1 → 退出 while），
     * TEMP 位一直留在 mask 里轮不到，温度恒为 0。
     * 温度每 1.28s 才读一次（约 1/640 次传输），代价是延后一个 acc 样本，
     * 而 0x22/0x23 与加速度输出寄存器无关，不会污染数据 */
    while (inst->pending_mask != 0 && !inst->transfer_busy)
    {
        if (inst->pending_mask & BMI088_PENDING_TEMP)
        {
            inst->pending_mask &= (uint8_t)~BMI088_PENDING_TEMP;
            BMI088_StartSensorDMA(inst, BMI088_SENSOR_TEMP);
        }
        else if (inst->pending_mask & BMI088_PENDING_ACC)
        {
            inst->pending_mask &= (uint8_t)~BMI088_PENDING_ACC;
            inst->int_timestamp = inst->pending_t_acc;
            BMI088_StartSensorDMA(inst, BMI088_SENSOR_ACC);
        }
        else if (inst->pending_mask & BMI088_PENDING_GYRO)
        {
            inst->pending_mask &= (uint8_t)~BMI088_PENDING_GYRO;
            inst->int_timestamp = inst->pending_t_gyro;
            BMI088_StartSensorDMA(inst, BMI088_SENSOR_GYRO);
        }
    }
}

/*============================ 环形缓冲查找辅助 ============================*/

/**
 * @brief 在环形时间戳缓冲中查找 bracket 对（t_older ≤ target ≤ t_newer）
 * @param t_buf     时间戳缓冲
 * @param newest    最新有效索引
 * @param n         有效样本数（≤ buf_size）
 * @param target    目标时间戳
 * @param out_older 输出：较旧样本索引
 * @param out_newer 输出：较新样本索引
 * @return 1=找到, 0=未找到
 */
static uint8_t BMI088_FindBracket(const uint64_t *t_buf, uint16_t newest, uint16_t n,
                                  uint64_t target, uint16_t *out_older, uint16_t *out_newer)
{
    for (uint16_t s = 0; s < n - 1; s++)
    {
        uint16_t idx_newer = (newest + n - s) % n;
        uint16_t idx_older = (newest + n - 1 - s) % n;
        if (t_buf[idx_older] <= target && target <= t_buf[idx_newer])
        {
            *out_older = idx_older;
            *out_newer = idx_newer;
            return 1;
        }
    }
    return 0;
}

/*============================ 插值辅助函数 ============================*/

/**
 * @brief 原始数据线性插值（3 轴，小端 16-bit）
 * @param raw_old  较旧样本 [6]（从缓存读取的本地副本）
 * @param raw_new  较新样本 [6]
 * @param t_old    旧样本时间戳 (us)
 * @param t_new    新样本时间戳 (us)
 * @param t_target 目标时间戳 (us)
 * @param out      插值结果 [6]
 * @note 使用 BMI088_AxisRaw_u 联合体直接操作 int16 轴数据
 */
static void BMI088_InterpRaw(const uint8_t *raw_old, const uint8_t *raw_new, uint64_t t_old, uint64_t t_new, uint64_t t_target, uint8_t *out)
{
    if (t_new <= t_old)
    {
        memcpy(out, raw_old, BMI088_RAW_DATA_SIZE);
        return;
    }

    float ratio = (float)(t_target - t_old) / (float)(t_new - t_old);
    if (ratio < 0.0f)
        ratio = 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;

    const BMI088_AxisRaw_u *old = (const BMI088_AxisRaw_u *)raw_old;
    const BMI088_AxisRaw_u *new = (const BMI088_AxisRaw_u *)raw_new;
    BMI088_AxisRaw_u *result = (BMI088_AxisRaw_u *)out;

    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        int16_t v_out = (int16_t)((float)old->axis[i] + ((float)new->axis[i] - (float)old->axis[i]) * ratio + BMI088_INTERP_ROUND);
        result->axis[i] = v_out;
    }
}

/**
 * @brief 加速度计原始数据 → 物理单位 (m/s²)
 * @note 只做数据手册灵敏度换算，**不补偿零偏**（标定归 drvlib_bmi088_kalman）
 * @note 使用 BMI088_AxisRaw_u 联合体直接访问 int16 轴数据
 */
static void BMI088_RawToAcc(const uint8_t *raw, float *out, BMI088_AccRange_e range)
{
    const BMI088_AxisRaw_u *axis = (const BMI088_AxisRaw_u *)raw;
    float sen = BMI088_AccSenTable[range];
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        out[i] = (float)axis->axis[i] * sen;
    }
}

/**
 * @brief 陀螺仪原始数据 → 物理单位 (rad/s)
 * @note 只做数据手册灵敏度换算，**不补偿零偏**（标定归 drvlib_bmi088_kalman）
 * @note 使用 BMI088_AxisRaw_u 联合体直接访问 int16 轴数据
 */
static void BMI088_RawToGyro(const uint8_t *raw, float *out, BMI088_GyroRange_e range)
{
    const BMI088_AxisRaw_u *axis = (const BMI088_AxisRaw_u *)raw;
    float sen = BMI088_GyroSenTable[range];
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        out[i] = (float)axis->axis[i] * sen;
    }
}

/*============================ 数据打包辅助函数 ============================*/

static float BMI088_ParseTempCelsius(const uint8_t *rx_buff)
{
    uint16_t temp_raw = ((uint16_t)rx_buff[BMI088_ACC_RX_TEMP_H] << 8) | rx_buff[BMI088_ACC_RX_TEMP_L];
    uint16_t temp_u11 = temp_raw >> 5;
    int16_t temp_signed = (int16_t)(temp_u11 > 1023 ? temp_u11 - 2048 : temp_u11);
    return (float)temp_signed * BMI088_TEMP_SENS + BMI088_TEMP_OFFSET;
}

/**
 * @brief 原始 acc/gyro 六字节 → 物理量，填进多速率数据帧
 * @note 三条读取路径（轮询连读 / 中断插值 / 中断取最新）共用这一个出口：
 *       轮询与插值两条路把两个时间戳填成同一个值，取最新那条填各自的实际时刻。
 */
static void BMI088_PackRaw(BMI088Instance *inst, const uint8_t acc_raw[BMI088_RAW_DATA_SIZE],
                           const uint8_t gyro_raw[BMI088_RAW_DATA_SIZE],
                           uint64_t ts_a, uint64_t ts_g, BMI088_Data_t *out)
{
    BMI088_RawToAcc(acc_raw, out->acc, inst->acc_range);
    BMI088_RawToGyro(gyro_raw, out->gyro, inst->gyro_range);
    out->time_stamp_a = ts_a;
    out->time_stamp_g = ts_g;
}

/*============================ 传感器初始化辅助 ============================*/

/**
 * @brief 加速度计初始化序列
 * @return 0-成功，非0-失败
 *
 * @note 序列里全是"写完马上读回校验"，一律走 BSP_BLOCK_MODE：模式既然是每次调用传参的，
 *       这里显式传即可，与运行时默认的 spi_mode 无关。
 * @note 前几笔（切 SPI 模式 / 软复位）的返回值有意忽略：它们的判据是后面那笔 CHIP_ID
 *       读回比对，读失败只会让 rx_buff 保持残留值 → 照样判失败。
 */
static uint8_t BMI088_AccInit(BMI088Instance *inst)
{
    // 1. 切换到SPI模式
    (void)BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1, BSP_BLOCK_MODE);
    DWT_Delay(BMI088_SPI_SWITCH_DELAY_S);

    // 2. 软复位
    (void)BMI088_WriteReg(inst, inst->cs_acc, BMI088_ACC_SOFTRESET_REG, BMI088_ACC_SOFTRESET_CMD, BSP_BLOCK_MODE);
    DWT_Delay(BMI088_ACC_RESET_DELAY_S);

    // 3. 再次切换到SPI模式
    (void)BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1, BSP_BLOCK_MODE);
    DWT_Delay(BMI088_SPI_SWITCH_DELAY_S);

    // 4. 检查 CHIP_ID
    (void)BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1, BSP_BLOCK_MODE);
    if (inst->spi_inst->rx_buff[BMI088_ACC_RX_DATA_OFF] != BMI088_ACC_CHIP_ID_VAL)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Acc CHIP_ID error: 0x%02X", inst->spi_inst->rx_buff[2]);
        return 1;
    }

    // 5. 开启加速度计
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_PWR_CTRL_REG, BMI088_ACC_ENABLE_ON, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "acc_pwr_ctrl write failed");
        return 1;
    }
    DWT_Delay(BMI088_ACC_PWR_UP_DELAY_S); // 数据手册§3: 写 ACC_PWR_CTRL 后等 450µs

    // 6. 退出挂起模式
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_PWR_CONF_REG, BMI088_ACC_PWR_SAVE_ACTIVE, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "acc_pwr_conf write failed");
        return 1;
    }

    // 7. 写入量程配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_RANGE_REG, (uint8_t)inst->acc_range, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "acc_range write failed");
        return 1;
    }

    // 8. 写入滤波器和ODR配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_CONF_REG, inst->acc_bwp | inst->acc_odr, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "acc_conf write failed");
        return 1;
    }

    /* 9~10. 数据就绪引脚与中断映射：**只有中断模式才配**。
     * 轮询模式整段跳过，保持软复位后的默认值（INT1 输出关闭、无映射）——
     * 轮询模式没人收这个中断，配了只会白占一个 EXTI 槽位。
     * 注意这一步不能只靠"上层没挂回调"来保证：中断映射是**器件侧**的行为，
     * 配上了引脚就会一直翻转。 */
    if (inst->work_mode == BMI088_MODE_INT)
    {
        // 9. 配置 INT1 引脚
        if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_INT1_IO_CTRL_REG, BMI088_INT1_OUT | BMI088_INT1_LVL_HIGH, BSP_BLOCK_MODE) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int1_io_ctrl write failed");
            return 1;
        }

        // 10. 配置中断映射
        if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_INT_MAP_DATA_REG, BMI088_INT1_DRDY, BSP_BLOCK_MODE) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int_map_data write failed");
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 陀螺仪初始化序列（传输模式约定同 BMI088_AccInit）
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_GyroInit(BMI088Instance *inst)
{
    // 1. 软复位
    (void)BMI088_WriteReg(inst, inst->cs_gyro, BMI088_GYRO_SOFTRESET_REG, BMI088_GYRO_SOFTRESET_CMD, BSP_BLOCK_MODE);
    DWT_Delay(BMI088_GYRO_RESET_DELAY_S);

    // 2. 检查 CHIP_ID
    (void)BMI088_ReadReg(inst, inst->cs_gyro, BMI088_GYRO_CHIP_ID_REG, 1, 0, BSP_BLOCK_MODE);
    if (inst->spi_inst->rx_buff[BMI088_GYRO_RX_DATA_OFF] != BMI088_GYRO_CHIP_ID_VALUE)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Gyro CHIP_ID error: 0x%02X", inst->spi_inst->rx_buff[1]);
        return 1;
    }

    // 3. 写入量程配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_RANGE_REG, (uint8_t)inst->gyro_range, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "gyro_range write failed");
        return 1;
    }

    // 4. 写入带宽配置（使用组合值 OR BMI088_GYRO_BW_MUST_SET）
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_BANDWIDTH_REG, inst->gyro_conf | BMI088_GYRO_BW_MUST_SET, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "gyro_bandwidth write failed");
        return 1;
    }

    // 5. 写入电源模式
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_LPM1_REG, BMI088_GYRO_PM_NORMAL, BSP_BLOCK_MODE) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "gyro_lpm1 write failed");
        return 1;
    }

    /* 6~8. 数据就绪引脚与中断映射：**只有中断模式才配**（理由同 BMI088_AccInit 步骤 9~10）。
     * 轮询模式保持软复位默认值：INT_CTRL 关（不产生数据就绪中断）、
     * INT3/INT4 按默认的低有效开漏。 */
    if (inst->work_mode == BMI088_MODE_INT)
    {
        // 6. 配置中断控制
        if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT_CTRL_REG, BMI088_GYRO_INT_DATA_EN, BSP_BLOCK_MODE) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "gyro_int_ctrl write failed");
            return 1;
        }

        // 7. 配置 INT3 引脚（推挽输出 + 高电平有效，INT4 保持复位默认）
        if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT3_INT4_IO_CONF_REG, BMI088_INT4_OD_OPEN_DRAIN | BMI088_INT4_LVL_HIGH | BMI088_INT3_LVL_HIGH, BSP_BLOCK_MODE) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int3_int4_io_conf write failed");
            return 1;
        }

        // 8. 配置 INT3 中断映射
        if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT3_INT4_IO_MAP_REG, BMI088_INT3_DATA, BSP_BLOCK_MODE) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int3_int4_io_map write failed");
            return 1;
        }
    }
    return 0;
}

/*============================ 公开接口实现 ============================*/

/**
 * @brief 注册BMI088实例（仅调用一次）
 * @note 注册 SPI/GPIO/PWM/Daemon 子模块。
 *       硬件配置由 BMI088Config 完成。
 */
int8_t BMI088Register(BMI088Instance *inst)
{

    if (inst == NULL)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return -1;
    }

    /* 不做本层的防重复注册检查：SPI 实例的 parent 改由 SPIConfig 写入（读直接读、
     * 写必须用函数），不能再用它当"已注册"标志；而各子模块自己都有防重
     * （SPIRegister / GPIORegister / DaemonRegister 都会拒绝重复注册并打日志），
     * 重复调用本函数的首个失败点就会被拦住。 */
    inst->cs_acc->parent = inst;
    inst->cs_gyro->parent = inst;
    inst->int_acc->parent = inst;
    inst->int_gyro->parent = inst;

    // 注册 SPI（只注册，Config 时设置硬件句柄）
    if (SPIRegister(inst->spi_inst) != BSP_OK)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "SPI register failed");
        return -1;
    }

    // 注册 CS/INT GPIO（只注册，Config 时填充映射）
    if (GPIORegister(inst->cs_acc) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "cs_acc register failed");
        return -1;
    }
    if (GPIORegister(inst->cs_gyro) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "cs_gyro register failed");
        return -1;
    }
    if (GPIORegister(inst->int_acc) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int_acc register failed");
        return -1;
    }
    if (GPIORegister(inst->int_gyro) != 0)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int_gyro register failed");
        return -1;
    }

    // 注册 daemon（占位，Config 更新运行参数）
    if (inst->daemon)
    {
        DaemonRegister(inst->daemon);
    }

    BSPLOG(&g_bmi088_log, LOG_LEVEL_INFO, "BMI088 registered, waiting for Config...");
    return 0;
}

/**
 * @brief 配置BMI088实例（填充硬件映射 + 传感器初始化，可重复调用）
 * @note 配置硬件枚举、daemon、传感器初始化。
 *       要求在 BMI088Register 之后调用。
 * @note 配置项一律先校验再写入。这不是防御性编程的洁癖：`acc_range`/`gyro_range`
 *       会被当作 `BMI088_AccSenTable[]`/`BMI088_GyroSenTable[]` 的下标用，
 *       越界是未定义行为；`INT + BSP_BLOCK_MODE` 则会让第一次 DRDY 中断就死循环。
 *       两者都是"配置错一个值、现象和驱动逻辑完全无关"的那类故障，在这里拦掉最省事。
 */
int8_t BMI088Config(BMI088Instance *inst, const BMI088_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(inst == NULL, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->spi_e >= SPI_NUM_MAX, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "spi_e out of range!"));
    BSP_RETURN_IF_TRUE_LOG(config->cs_acc_e >= GPIO_NUM_MAX || config->cs_gyro_e >= GPIO_NUM_MAX, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "CS gpio_e out of range!"));
    BSP_RETURN_IF_TRUE_LOG(config->work_mode != BMI088_MODE_POLLING && config->work_mode != BMI088_MODE_INT, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid work_mode=%d!", (int)config->work_mode));
    BSP_RETURN_IF_TRUE_LOG(config->spi_mode > BSP_DMA_MODE, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid spi_mode=%d!", (int)config->spi_mode));
    /* 中断模式配阻塞传输 = 在 DRDY EXTI 里做阻塞 SPI：HAL 用 HAL_GetTick 计时，
     * tick 中断优先级低于该 EXTI，中断里 tick 不前进，从机一不响应就是死循环。
     * 这个组合没有合法用途，直接拒绝而不是留个上电就卡的实例 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == BMI088_MODE_INT && config->spi_mode == BSP_BLOCK_MODE, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "INT mode cannot use BSP_BLOCK_MODE (blocking SPI in EXTI)!"));
    BSP_RETURN_IF_TRUE_LOG(config->acc_range >= BMI088_ACC_RANGE_NUM, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid acc_range=%d!", (int)config->acc_range));
    BSP_RETURN_IF_TRUE_LOG(config->gyro_range >= BMI088_GYRO_RANGE_NUM, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid gyro_range=%d!", (int)config->gyro_range));
    BSP_RETURN_IF_TRUE_LOG(config->gyro_conf > BMI088_GYRO_CONF_100_32, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid gyro_conf=0x%02X!", (unsigned)config->gyro_conf));
    BSP_RETURN_IF_TRUE_LOG(config->spi_timeout_ms == 0, -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "spi_timeout_ms must be > 0!"));
    /* 中断模式没接 DRDY 就等于没有数据来源，直接拒绝配置而不是留个哑巴实例 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == BMI088_MODE_INT &&
                               (config->int_acc_e >= GPIO_NUM_MAX || config->int_gyro_e >= GPIO_NUM_MAX),
                           -1,
                           BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "INT mode requires valid int_acc_e/int_gyro_e!"));

    // 预存硬件枚举到子实例（Config 时填充映射）
    inst->spi_inst->spi_e = config->spi_e;
    inst->cs_acc->gpio_e = config->cs_acc_e;
    inst->cs_gyro->gpio_e = config->cs_gyro_e;
    inst->int_acc->gpio_e = config->int_acc_e;
    inst->int_gyro->gpio_e = config->int_gyro_e;

    // 配置 CS GPIO（填充映射，无回调）
    {
        GPIO_Config_s cs_gpio = {
            .gpio_e = config->cs_acc_e,
            .callback = NULL,
        };
        if (GPIOConfig(inst->cs_acc, &cs_gpio) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "cs_acc config failed");
            return -1;
        }
        cs_gpio.gpio_e = config->cs_gyro_e;
        if (GPIOConfig(inst->cs_gyro, &cs_gpio) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "cs_gyro config failed");
            return -1;
        }
    }

    // 释放 CS 上升沿，避免总线冲突
    GPIOSet(inst->cs_acc);
    GPIOSet(inst->cs_gyro);
    DWT_Delay(BMI088_CS_RELEASE_DELAY_S);

    // 保存传感器参数
    inst->acc_range = config->acc_range;
    inst->acc_bwp = config->acc_bwp;
    inst->acc_odr = config->acc_odr;
    inst->gyro_range = config->gyro_range;
    inst->gyro_conf = config->gyro_conf;
    inst->work_mode = config->work_mode;
    inst->spi_mode = config->spi_mode;             /* 运行时默认传输模式：每次 SPI 调用时透传给 bsp */
    inst->spi_timeout_ms = config->spi_timeout_ms; /* 完全按 Config 配置的超时时间使用 */

    /* 运行态清零（**必须赶在挂 SPI 回调之前**）：Config 是可重复调用的，重复配置时
     * 上一轮可能留着 transfer_busy=1，此时回调已挂着，一个迟到的 DRDY 边沿就会走进
     * 派发分支、凭空发一笔传输。与 drv_ist8310 同一约定。
     * 环形缓冲/采样计数这些只属于中断模式的字段在下面的 INT 分支里清。 */
    inst->transfer_busy = 0;
    inst->pending_mask = 0;
    inst->xfer_done = 0;
    inst->xfer_error = 0;
    inst->fail_count = 0;
    inst->recover_request = 0;

    /* 配置 SPI（模式不再在这里定：每次收发调用传参）。
     * parent 经 Config 写入，SPI 回调据此取回本实例。
     * 三个回调**两种工作模式都挂**：轮询模式一旦把 spi_mode 配成 IT/DMA，同样要靠
     * 完成回调才能让 BMI088_WaitXfer 收尾。回调内部按 work_mode 分流，不会把中间读
     * 当成采样发布；挂上也不影响初始化 —— 初始化序列全部显式传 BSP_BLOCK_MODE，
     * 而 BLOCK 传输不产生任何回调。 */
    SPI_Config_s spi_cfg = {
        .spi_e = config->spi_e,
        .parent = inst,
        .rx_callback = BMI088_SPICpltCallback,
        .tx_callback = BMI088_SPITxCpltCallback,
        .err_callback = BMI088_SPIErrCallback,
    };
    if (SPIConfig(inst->spi_inst, &spi_cfg) != BSP_OK)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "SPI config failed");
        return -1;
    }

    /* 更新 daemon 运行参数（可重入）：本层只上报"离线"状态与故障动作，
     * 不再挂载加热关断回调（加热器已移出本模块，见头文件 TODO） */
    if (inst->daemon)
    {
        Daemon_Config_s daemon_cfg = {
            .reload_count = config->daemon_reload,
            .fault_action = config->daemon_fault,
            .callback = NULL,
            .owner_id = inst,
        };
        DaemonConfig(inst->daemon, &daemon_cfg);
    }

    /*============================ 加速度计初始化 ============================*/
    if (BMI088_AccInit(inst) != 0)
        return -1;

    /*============================ 陀螺仪初始化 ============================*/
    if (BMI088_GyroInit(inst) != 0)
        return -1;

    /*============================ 中断模式设置 ============================*/
    if (inst->work_mode == BMI088_MODE_INT)
    {
        GPIO_Config_s int_gpio_cfg = {
            .gpio_e = config->int_acc_e,
            .callback = BMI088_IntCallback,
        };
        if (GPIOConfig(inst->int_acc, &int_gpio_cfg) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int_acc config callback failed");
            return -1;
        }
        int_gpio_cfg.gpio_e = config->int_gyro_e;
        if (GPIOConfig(inst->int_gyro, &int_gpio_cfg) != 0)
        {
            BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "int_gyro config callback failed");
            return -1;
        }

        /* SPI 的 rx/tx/err 三个回调已在上面 SPI 配置处一次性挂好（两种模式都挂），
         * 这里不再重复 SPIConfig —— 重入会先中止在途传输，多一次没有意义。 */

        // 初始化中断模式字段（transfer_busy/pending_mask 等共用字段已在上面清过）
        inst->current_sensor = BMI088_SENSOR_ACC;
        inst->int_timestamp = 0;
        inst->pending_t_acc = 0;
        inst->pending_t_gyro = 0;
        inst->acc_wr_idx = 0;
        inst->gyro_wr_idx = 0;
        inst->acc_cnt = 0;
        inst->gyro_cnt = 0;
        inst->temperature = 0.0f;
        inst->last_temp_us = 0;
        memset(inst->acc_raw, 0, sizeof(inst->acc_raw));
        memset(inst->gyro_raw, 0, sizeof(inst->gyro_raw));
        memset(inst->t_acc, 0, sizeof(inst->t_acc));
        memset(inst->t_gyro, 0, sizeof(inst->t_gyro));

        BSPLOG(&g_bmi088_log, LOG_LEVEL_INFO, "BMI088 INT mode initialized");
    }

    BSPLOG(&g_bmi088_log, LOG_LEVEL_INFO, "BMI088 config success");
    return 0;
}

/*============================ 数据读取接口 ============================*/

/**
 * @brief 轮询模式：当场连读一遍 Acc→Gyro→Temp
 * @return 两条流共用读数完成时刻的时间戳（本来就是同一时刻读的）；任一数据读失败返回全 0
 * @note 传输方式用 inst->spi_mode：配成 IT/DMA 时传输期间 CPU 空闲，但函数返回前仍在
 *       忙等回调，返回语义与 BLOCK 完全一致。
 * @note **任一数据读失败就整帧作废**：读失败时 rx_buff 里是残留/半新的数据，配上刚取的
 *       时间戳发出去，调用方会拿旧姿态当新的、且从外部完全看不出是旧数据（同
 *       drv_ist8310 的判据）。温度是辅助量，读失败不作废整帧，但也不刷新新鲜度 ——
 *       BMI088GetTemperature 会照旧返回上一次的值，而不是把陈温当新值。
 */
static BMI088_Data_t BMI088_ReadPolling(BMI088Instance *inst)
{
    /* 读取加速度计原始数据（X/Y/Z 三轴，6 字节），必须**先拷走**再发起下一笔：
     * rx_buff 是三个传感器共用的同一块缓冲 */
    uint8_t acc_buf[BMI088_RAW_DATA_SIZE];
    if (BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACCEL_XOUT_L, BMI088_RAW_DATA_SIZE,
                       BMI088_ACC_DUMMY_BYTES, inst->spi_mode) != 0)
    {
        BMI088_NoteFailure(inst);
        return (BMI088_Data_t){0};
    }
    memcpy(acc_buf, &inst->spi_inst->rx_buff[BMI088_ACC_RX_DATA_OFF], BMI088_RAW_DATA_SIZE);

    /* 读取陀螺仪原始数据（X/Y/Z 三轴，6 字节） */
    uint8_t gyro_buf[BMI088_RAW_DATA_SIZE];
    if (BMI088_ReadReg(inst, inst->cs_gyro, BMI088_GYRO_X_L, BMI088_RAW_DATA_SIZE,
                       BMI088_GYRO_DUMMY_BYTES, inst->spi_mode) != 0)
    {
        BMI088_NoteFailure(inst);
        return (BMI088_Data_t){0};
    }
    memcpy(gyro_buf, &inst->spi_inst->rx_buff[BMI088_GYRO_RX_DATA_OFF], BMI088_RAW_DATA_SIZE);

    /* 读取温度原始数据（2 字节），存入实例供 BMI088GetTemperature 使用。
     * 提 11-bit 温度值（数据手册 §5.3.7: Temp_uint11 = (TEMP_MSB*8) + (TEMP_LSB/32)）；
     * 读失败就保持上一次的值与新鲜度不变（新鲜度为 0 时 GetTemperature 仍返回 NAN） */
    if (BMI088_ReadReg(inst, inst->cs_acc, BMI088_TEMP_M, 2, BMI088_ACC_DUMMY_BYTES, inst->spi_mode) == 0)
    {
        inst->temperature = BMI088_ParseTempCelsius(inst->spi_inst->rx_buff);
        inst->last_temp_us = DWT_GetTimeUs(); /* 刷新新鲜度，供 GetTemperature 判定可用性 */
    }

    inst->fail_count = 0;

    BMI088_Data_t data = {0};
    uint64_t now = DWT_GetTimeUs();
    BMI088_PackRaw(inst, acc_buf, gyro_buf, now, now, &data);
    return data;
}

/*============================ 中断模式内部读取路径 ============================*/

/**
 * @brief 中断模式 BMI088_READ_INTERP：把两条流对齐到同一时刻（线性插值）
 * @return 两个时间戳相等；样本不足以构成插值区间时返回全 0
 * @note 取"更新的那条流的最新时刻"为对齐目标，只插值另一条 —— 不插值的那条直接用真值，
 *       少一次近似。两条流都插值只会引入两倍误差。
 */
static BMI088_Data_t BMI088_ReadInterp(BMI088Instance *inst)
{
    if (inst->acc_cnt < BMI088_MIN_SAMPLE_COUNT || inst->gyro_cnt < BMI088_MIN_SAMPLE_COUNT)
        return (BMI088_Data_t){0};

    /* 快照 volatile 状态 */
    uint16_t acc_wr = inst->acc_wr_idx;
    uint16_t gyro_wr = inst->gyro_wr_idx;
    uint16_t acc_n = (inst->acc_cnt < BMI088_ACC_BUF_SIZE) ? (uint16_t)inst->acc_cnt : BMI088_ACC_BUF_SIZE;
    uint16_t gyro_n = (inst->gyro_cnt < BMI088_GYRO_BUF_SIZE) ? (uint16_t)inst->gyro_cnt : BMI088_GYRO_BUF_SIZE;

    /* 最新帧索引 */
    uint16_t a_newest = (acc_wr + BMI088_ACC_BUF_SIZE - 1) % BMI088_ACC_BUF_SIZE;
    uint16_t g_newest = (gyro_wr + BMI088_GYRO_BUF_SIZE - 1) % BMI088_GYRO_BUF_SIZE;

    uint64_t t_acc_newest = inst->t_acc[a_newest];
    uint64_t t_gyro_newest = inst->t_gyro[g_newest];

    /* 检查最新帧时间戳有效性 */
    if (t_acc_newest == 0 || t_gyro_newest == 0)
        return (BMI088_Data_t){0};

    uint8_t best_acc[BMI088_RAW_DATA_SIZE], best_gyro[BMI088_RAW_DATA_SIZE];
    uint64_t best_ts;
    uint8_t found = 1;

    if (t_acc_newest >= t_gyro_newest)
    {
        /* Acc 比 Gyro 新 → 以最新 Gyro 时刻为 target，插值 Acc */
        best_ts = t_gyro_newest;
        memcpy(best_gyro, inst->gyro_raw[g_newest], BMI088_RAW_DATA_SIZE);

        uint16_t a_older, a_newer;
        if (BMI088_FindBracket(inst->t_acc, a_newest, acc_n, best_ts, &a_older, &a_newer))
        {
            BMI088_InterpRaw(inst->acc_raw[a_older], inst->acc_raw[a_newer],
                             inst->t_acc[a_older], inst->t_acc[a_newer],
                             best_ts, best_acc);
        }
        else
        {
            found = 0;
        }
    }
    else
    {
        /* Gyro 比 Acc 新 → 以最新 Acc 时刻为 target，插值 Gyro */
        best_ts = t_acc_newest;
        memcpy(best_acc, inst->acc_raw[a_newest], BMI088_RAW_DATA_SIZE);

        uint16_t g_older, g_newer;
        if (BMI088_FindBracket(inst->t_gyro, g_newest, gyro_n, best_ts, &g_older, &g_newer))
        {
            BMI088_InterpRaw(inst->gyro_raw[g_older], inst->gyro_raw[g_newer],
                             inst->t_gyro[g_older], inst->t_gyro[g_newer],
                             best_ts, best_gyro);
        }
        else
        {
            found = 0;
        }
    }

    if (!found)
        return (BMI088_Data_t){0};

    BMI088_Data_t data = {0};
    BMI088_PackRaw(inst, best_acc, best_gyro, best_ts, best_ts, &data);
    return data;
}

/**
 * @brief 中断模式 BMI088_READ_LATEST：各自取最新一帧，保留独立时间戳
 * @note 专为 Kalman 等需要多速率融合的算法设计 —— 插值会抹掉两条流的真实节奏，
 *       而 Kalman 正是要靠各自的 dt 做多速率预测/更新。
 */
static BMI088_Data_t BMI088_ReadLatest(BMI088Instance *inst)
{
    BMI088_Data_t data = {0};

    if (inst->acc_cnt == 0 || inst->gyro_cnt == 0)
        return data;

    /* 快照最新帧索引 */
    uint16_t a_idx = ((inst->acc_wr_idx + BMI088_ACC_BUF_SIZE - 1) % BMI088_ACC_BUF_SIZE);
    uint16_t g_idx = ((inst->gyro_wr_idx + BMI088_GYRO_BUF_SIZE - 1) % BMI088_GYRO_BUF_SIZE);

    uint64_t ts_a = inst->t_acc[a_idx];
    uint64_t ts_g = inst->t_gyro[g_idx];
    if (ts_a == 0 || ts_g == 0)
        return (BMI088_Data_t){0};

    BMI088_PackRaw(inst, inst->acc_raw[a_idx], inst->gyro_raw[g_idx], ts_a, ts_g, &data);
    return data;
}

/*============================ 公开读取接口 ============================*/

BMI088_Data_t BMI088Read(BMI088Instance *inst, BMI088_ReadMode_e read_mode)
{
    if (inst == NULL)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return (BMI088_Data_t){0};
    }
    if (read_mode > BMI088_READ_INTERP)
    {
        BSPLOG(&g_bmi088_log, LOG_LEVEL_ERROR, "Invalid read_mode=%d", (int)read_mode);
        return (BMI088_Data_t){0};
    }

    /* 传输卡死自恢复（每控制周期一次，放在所有提前返回之前 —— 卡死的表现正是
     * acc_cnt/gyro_cnt 不再增长，若放在下面那些判据之后，最需要它的场景恰好走不到）。
     * INT 模式的传输由 DRDY 中断发起，那里不能等 HAL tick、也就无法真正 Abort；
     * 本函数是任务上下文里唯一"每周期必经"的入口，把复位搬到这里。
     * 总线健康时它只做一次 DWT 读，开销可忽略。 */
    (void)SPIRecoverTxIfStuck(inst->spi_inst, 0);

    /* 错误路径提出的恢复请求就地处理（可能含真正的 SPI Abort，只能在任务上下文做） */
    BMI088_ServiceRecover(inst);

    if (inst->work_mode == BMI088_MODE_INT)
    {
        /* 中断模式只取缓冲，不起传输；read_mode 在这里才有意义 */
        return (read_mode == BMI088_READ_INTERP) ? BMI088_ReadInterp(inst) : BMI088_ReadLatest(inst);
    }

    /* 轮询模式：read_mode 被忽略（两条流本来就是同一时刻连读的），
     * 每调一次就是一次新的采样，故成功即喂狗 */
    BMI088_Data_t data = BMI088_ReadPolling(inst);
    if (data.time_stamp_a != 0)
    {
        DaemonReload(inst->daemon);
    }
    return data;
}

/*============================ 温度接口 ============================*/

float BMI088GetTemperature(const BMI088Instance *inst)
{
    if (inst == NULL)
        return NAN;
    /* last_temp_us == 0 表示本次上电还没成功采到温度：
     * 此时 inst->temperature 仍是初值 0，会把 0℃ 当成真实温度上报，
     * 对温补/加热这类消费者是危险的输入，故返回 NAN 明确表示"不可用" */
    if (inst->last_temp_us == 0)
        return NAN;

    return inst->temperature;
}

#endif /* defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* DRV_BMI088_USED */
