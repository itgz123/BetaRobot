/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include <string.h>
#include "bsp_dwt.h"
#include "bsp_uart_log.h"

/*============================ SPI通信宏定义 ============================*/

#define BMI088_SPI_READ_CMD 0x80   // R/W位=1（读）
#define BMI088_SPI_WRITE_CMD 0x7F  // R/W位=0（写）
#define BMI088_SPI_DUMMY_BYTE 0x55 // 哑指令（用于发送时填充）
#define SPI_BLOCK_TIMEOUT_MS 100

/* 基于数据手册的专用延时宏 */
#define BMI088_SPI_SWITCH_DELAY_S 0.002f // SPI模式切换延时  (2ms)
#define BMI088_ACC_RESET_DELAY_S 0.002f  // 加速度计软复位延时 (2ms，数据手册1ms+余量)
#define BMI088_GYRO_RESET_DELAY_S 0.035f // 陀螺仪软复位延时   (35ms，数据手册30ms+余量)

#define BMI088_WRITE_CHECK_INIT_S 0.001f    // 写验证初始等待    (1ms, DWT_Delay微秒)
#define BMI088_WRITE_CHECK_TIMEOUT_US 10000 // 写验证超时时间   (10ms, DWT_GetTimeUs微秒比较)

/*============================ 内部传感器类型定义 ============================*/

#define BMI088_SENSOR_ACC 0
#define BMI088_SENSOR_GYRO 1
#define BMI088_PENDING_ACC (1 << BMI088_SENSOR_ACC)
#define BMI088_PENDING_GYRO (1 << BMI088_SENSOR_GYRO)

/*============================ 温度转换参数 ============================*/

#define BMI088_TEMP_SENS 0.125f  // 温度灵敏度 (°C/LSB)
#define BMI088_TEMP_OFFSET 23.0f // 温度偏移 (°C)

/*============================ SPI 传输长度 ============================*/

#define BMI088_SPI_WRITE_LEN 2      // SPI写传输长度：1地址 + 1数据
#define BMI088_ACC_SPI_READ_LEN 8   // Acc读传输长度：1地址 + 1dummy + 6数据
#define BMI088_GYRO_SPI_READ_LEN 9  // Gyro读传输长度：1地址 + 8数据(6gyro+2temp)
#define BMI088_SPI_IT_TIMEOUT_MS 10 // SPI IT传输超时(ms)

/*============================ RX 缓冲偏移 ============================*/

#define BMI088_ACC_RX_DATA_OFF 2  // Acc数据在rx_buff中的起始偏移(1addr+1dummy)
#define BMI088_GYRO_RX_DATA_OFF 1 // Gyro数据在rx_buff中的起始偏移(1addr)
#define BMI088_ACC_DUMMY_BYTES 1  // Acc SPI读需要1个dummy字节
#define BMI088_GYRO_DUMMY_BYTES 0 // Gyro SPI读不需要dummy字节
#define BMI088_GYRO_RX_TEMP_L 7   // 陀螺仪温度低字节在rx_buff中的偏移
#define BMI088_GYRO_RX_TEMP_H 8   // 陀螺仪温度高字节在rx_buff中的偏移
#define BMI088_ACC_RX_TEMP_L 2    // Acc阻塞模式温度低字节在rx_buff中的偏移
#define BMI088_ACC_RX_TEMP_H 3    // Acc阻塞模式温度高字节在rx_buff中的偏移

/*============================ 插值阈值 ============================*/

#define BMI088_MIN_SAMPLE_COUNT 2 // 插值所需最小样本数
#define BMI088_INTERP_ROUND 0.5f  // 插值四舍五入

/*============================ 延时常量 ============================*/

#define BMI088_CS_RELEASE_DELAY_S 0.08f // CS释放后等待时间(s)

/*============================ 物理常量 ============================*/

#define BMI088_GRAVITY 9.80665f // 重力加速度 (m/s²)

/*============================ 灵敏度查找表 ============================*/

/* 加速度计灵敏度查找表：将原始值转换为 g，单位 (m/s²)/LSB */
const float BMI088_AccSenTable[BMI088_ACC_RANGE_NUM] = {
    0.0008974358974f,  // ±3g 量程灵敏度
    0.00179443359375f, // ±6g 量程灵敏度
    0.0035888671875f,  // ±12g 量程灵敏度
    0.007177734375f,   // ±24g 量程灵敏度
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

static void BMI088_ReadReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint16_t len, uint8_t dummy);
static void BMI088_WriteReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data);
static uint8_t BMI088_WriteRegWithCheck(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data);

/* 中断模式私有函数 */
static void BMI088_IntCallback(GPIOInstance *gpio_inst);
static void BMI088_StartSensorIT(BMI088Instance *inst, uint8_t sensor_type);
static void BMI088_SPICpltCallback(SPIInstance *spi_inst);
static void BMI088_CheckPendingIT(BMI088Instance *inst);
/* 插值辅助函数声明 */
static void BMI088_InterpRaw(const uint8_t *raw_old, const uint8_t *raw_new, uint64_t t_old, uint64_t t_new, uint64_t t_target, uint8_t *out);
static uint8_t BMI088_FindBracket(const uint64_t *t_buf, uint16_t newest, uint16_t n, uint64_t target, uint16_t *out_older, uint16_t *out_newer);
static void BMI088_RawToAcc(const uint8_t *raw, float *out, BMI088_AccRange_e range, const float *offset);
static void BMI088_RawToGyro(const uint8_t *raw, float *out, BMI088_GyroRange_e range, const float *offset);
static BMI088_Data_t BMI088_PackData(BMI088Instance *inst, const uint8_t acc_raw[BMI088_RAW_DATA_SIZE], const uint8_t gyro_raw[BMI088_RAW_DATA_SIZE], uint64_t timestamp_us);

/*============================ 私有函数实现 ============================*/

/**
 * @brief 加速度计读取寄存器
 * @note 数据存入 inst->spi_inst.rx_buff，从 rx_buff[2] 开始为有效数据
 */
static void BMI088_ReadReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint16_t len, uint8_t dummy)
{
    if (len > BMI088_RAW_DATA_SIZE)
    {
        LOGWARNING("[drv_bmi088] Read len > 6: %d", len);
        return;
    }

    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;
    inst->tx_len = 1 + dummy + len;

    GPIOReset(cs);
    SPITransmitReceive(inst->spi_inst, inst->tx_buff, inst->tx_len, SPI_BLOCK_TIMEOUT_MS);
    GPIOSet(cs);
}

/**
 * @brief 写入寄存器（无验证）
 */
static void BMI088_WriteReg(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data)
{
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = data;
    inst->tx_len = BMI088_SPI_WRITE_LEN;

    GPIOReset(cs);
    SPITransmit(inst->spi_inst, inst->tx_buff, inst->tx_len, SPI_BLOCK_TIMEOUT_MS);
    GPIOSet(cs);
}

/**
 * @brief 写入寄存器并验证（超时轮询机制）
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_WriteRegWithCheck(BMI088Instance *inst, GPIOInstance *cs, uint8_t reg, uint8_t data)
{
    BMI088_WriteReg(inst, cs, reg, data);

    uint64_t start_us = DWT_GetTimeUs();
    DWT_Delay(BMI088_WRITE_CHECK_INIT_S);

    uint8_t rx_off = (cs == inst->cs_acc) ? BMI088_ACC_RX_DATA_OFF : BMI088_GYRO_RX_DATA_OFF;
    do
    {
        BMI088_ReadReg(inst, cs, reg, 1, (cs == inst->cs_acc) ? BMI088_ACC_DUMMY_BYTES : BMI088_GYRO_DUMMY_BYTES);
        if (inst->spi_inst->rx_buff[rx_off] == data)
        {
            return 0;
        }
    } while ((DWT_GetTimeUs() - start_us) < BMI088_WRITE_CHECK_TIMEOUT_US);

    LOGWARNING("[drv_bmi088] %s write 0x%02X verify failed (expected 0x%02X, got 0x%02X)",
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

    if (!inst->transfer_busy)
    {
        inst->int_timestamp = t_now;
        BMI088_StartSensorIT(inst, is_acc ? BMI088_SENSOR_ACC : BMI088_SENSOR_GYRO);
        inst->pending_mask &= (uint8_t)~(is_acc ? BMI088_PENDING_ACC : BMI088_PENDING_GYRO);
    }
    else
    {
        if (is_acc)
            inst->pending_t_acc = t_now;
        else
            inst->pending_t_gyro = t_now;
        inst->pending_mask |= (is_acc ? BMI088_PENDING_ACC : BMI088_PENDING_GYRO);
    }
}

/**
 * @brief 启动传感器 SPI IT 读取
 */
static void BMI088_StartSensorIT(BMI088Instance *inst, uint8_t sensor_type)
{
    inst->transfer_busy = 1;
    inst->current_sensor = sensor_type;
    GPIOInstance *cs;

    if (sensor_type == BMI088_SENSOR_ACC)
    {
        inst->tx_buff[0] = BMI088_ACCEL_XOUT_L | BMI088_SPI_READ_CMD;
        inst->tx_len = BMI088_ACC_SPI_READ_LEN;
        cs = inst->cs_acc;
    }
    else
    {
        inst->tx_buff[0] = BMI088_GYRO_X_L | BMI088_SPI_READ_CMD;
        inst->tx_len = BMI088_GYRO_SPI_READ_LEN;
        cs = inst->cs_gyro;
    }

    GPIOReset(cs);
    SPITransmitReceive(inst->spi_inst, inst->tx_buff, inst->tx_len, BMI088_SPI_IT_TIMEOUT_MS);
}

/**
 * @brief SPI IT传输完成回调（SPI中断上下文）
 */
static void BMI088_SPICpltCallback(SPIInstance *spi_inst)
{
    BMI088Instance *inst = (BMI088Instance *)spi_inst->parent;
    uint8_t is_acc = (inst->current_sensor == BMI088_SENSOR_ACC);

    /* 步骤1：拉高片选 */
    if (is_acc)
        GPIOSet(inst->cs_acc);
    else
        GPIOSet(inst->cs_gyro);

    /* 步骤2：写入循环缓冲 */
    if (is_acc)
    {
        uint16_t i = inst->acc_wr_idx % BMI088_ACC_BUF_SIZE;
        uint8_t off = BMI088_ACC_RX_DATA_OFF;
        inst->acc_raw[i][0] = spi_inst->rx_buff[off + 0];
        inst->acc_raw[i][1] = spi_inst->rx_buff[off + 1];
        inst->acc_raw[i][2] = spi_inst->rx_buff[off + 2];
        inst->acc_raw[i][3] = spi_inst->rx_buff[off + 3];
        inst->acc_raw[i][4] = spi_inst->rx_buff[off + 4];
        inst->acc_raw[i][5] = spi_inst->rx_buff[off + 5];
        inst->t_acc[i] = inst->int_timestamp;
        inst->acc_wr_idx++;
        if (inst->acc_cnt < UINT8_MAX)
            inst->acc_cnt++;
    }
    else
    {
        uint16_t i = inst->gyro_wr_idx % BMI088_GYRO_BUF_SIZE;
        uint8_t off = BMI088_GYRO_RX_DATA_OFF;
        inst->gyro_raw[i][0] = spi_inst->rx_buff[off + 0];
        inst->gyro_raw[i][1] = spi_inst->rx_buff[off + 1];
        inst->gyro_raw[i][2] = spi_inst->rx_buff[off + 2];
        inst->gyro_raw[i][3] = spi_inst->rx_buff[off + 3];
        inst->gyro_raw[i][4] = spi_inst->rx_buff[off + 4];
        inst->gyro_raw[i][5] = spi_inst->rx_buff[off + 5];
        inst->gyro_temp_raw = (int16_t)((spi_inst->rx_buff[BMI088_GYRO_RX_TEMP_H] << 8) | spi_inst->rx_buff[BMI088_GYRO_RX_TEMP_L]);
        inst->t_gyro[i] = inst->int_timestamp;
        inst->gyro_wr_idx++;
        if (inst->gyro_cnt < UINT8_MAX)
            inst->gyro_cnt++;
    }

    /* 步骤3：清除传输忙 + 检查待处理 */
    inst->transfer_busy = 0;
    BMI088_CheckPendingIT(inst);
}

/**
 * @brief 检查并处理待读取的传感器
 */
static void BMI088_CheckPendingIT(BMI088Instance *inst)
{
    while (inst->pending_mask != 0 && !inst->transfer_busy)
    {
        if (inst->pending_mask & BMI088_PENDING_ACC)
        {
            inst->pending_mask &= (uint8_t)~BMI088_PENDING_ACC;
            inst->int_timestamp = inst->pending_t_acc;
            BMI088_StartSensorIT(inst, BMI088_SENSOR_ACC);
        }
        else if (inst->pending_mask & BMI088_PENDING_GYRO)
        {
            inst->pending_mask &= (uint8_t)~BMI088_PENDING_GYRO;
            inst->int_timestamp = inst->pending_t_gyro;
            BMI088_StartSensorIT(inst, BMI088_SENSOR_GYRO);
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

    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        int16_t v_old = (int16_t)((raw_old[i * 2 + 1] << 8) | raw_old[i * 2]);
        int16_t v_new = (int16_t)((raw_new[i * 2 + 1] << 8) | raw_new[i * 2]);
        int16_t v_out = (int16_t)((float)v_old + ((float)v_new - (float)v_old) * ratio + BMI088_INTERP_ROUND);
        out[i * 2] = (uint8_t)(v_out & 0xFF);
        out[i * 2 + 1] = (uint8_t)((v_out >> 8) & 0xFF);
    }
}

/**
 * @brief 加速度计原始数据 → 物理单位 (m/s²)
 */
static void BMI088_RawToAcc(const uint8_t *raw, float *out, BMI088_AccRange_e range, const float *offset)
{
    float sen = BMI088_AccSenTable[range];
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        int16_t v = (int16_t)((raw[i * 2 + 1] << 8) | raw[i * 2]);
        out[i] = (float)v * sen - offset[i];
    }
}

/**
 * @brief 陀螺仪原始数据 → 物理单位 (rad/s)
 */
static void BMI088_RawToGyro(const uint8_t *raw, float *out, BMI088_GyroRange_e range, const float *offset)
{
    float sen = BMI088_GyroSenTable[range];
    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        int16_t v = (int16_t)((raw[i * 2 + 1] << 8) | raw[i * 2]);
        out[i] = (float)v * sen - offset[i];
    }
}

/*============================ 数据打包辅助函数 ============================*/

static BMI088_Data_t BMI088_PackData(BMI088Instance *inst, const uint8_t acc_raw[BMI088_RAW_DATA_SIZE], const uint8_t gyro_raw[BMI088_RAW_DATA_SIZE], uint64_t timestamp_us)
{
    BMI088_Data_t data = {0};
    BMI088_RawToAcc(acc_raw, data.acc, inst->acc_range, inst->acc_offset);
    BMI088_RawToGyro(gyro_raw, data.gyro, inst->gyro_range, inst->gyro_offset);
    data.time_stamp = timestamp_us;
    return data;
}

/*============================ 传感器初始化辅助 ============================*/

static uint8_t BMI088_AccInit(BMI088Instance *inst)
{
    // 1. 切换到SPI模式
    BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1);
    DWT_Delay(BMI088_SPI_SWITCH_DELAY_S);

    // 2. 软复位
    BMI088_WriteReg(inst, inst->cs_acc, BMI088_ACC_SOFTRESET_REG, BMI088_ACC_SOFTRESET_CMD);
    DWT_Delay(BMI088_ACC_RESET_DELAY_S);

    // 3. 再次切换到SPI模式
    BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1);
    DWT_Delay(BMI088_SPI_SWITCH_DELAY_S);

    // 4. 检查 CHIP_ID
    BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACC_CHIP_ID_REG, 1, 1);
    if (inst->spi_inst->rx_buff[BMI088_ACC_RX_DATA_OFF] != BMI088_ACC_CHIP_ID_VAL)
    {
        LOGERROR("[drv_bmi088] Acc CHIP_ID error: 0x%02X", inst->spi_inst->rx_buff[2]);
        return 1;
    }

    // 5. 开启加速度计
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_PWR_CTRL_REG, BMI088_ACC_ENABLE_ON) != 0)
    {
        LOGERROR("[drv_bmi088] acc_pwr_ctrl write failed");
        return 1;
    }

    // 6. 退出挂起模式
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_PWR_CONF_REG, BMI088_ACC_PWR_SAVE_ACTIVE) != 0)
    {
        LOGERROR("[drv_bmi088] acc_pwr_conf write failed");
        return 1;
    }

    // 7. 写入量程配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_RANGE_REG, (uint8_t)inst->acc_range) != 0)
    {
        LOGERROR("[drv_bmi088] acc_range write failed");
        return 1;
    }

    // 8. 写入滤波器和ODR配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_ACC_CONF_REG, inst->acc_bwp | inst->acc_odr) != 0)
    {
        LOGERROR("[drv_bmi088] acc_conf write failed");
        return 1;
    }

    // 9. 配置 INT1 引脚
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_INT1_IO_CTRL_REG, BMI088_INT1_OUT | BMI088_INT1_LVL_HIGH) != 0)
    {
        LOGERROR("[drv_bmi088] int1_io_ctrl write failed");
        return 1;
    }

    // 10. 配置中断映射
    if (BMI088_WriteRegWithCheck(inst, inst->cs_acc, BMI088_INT_MAP_DATA_REG, BMI088_INT1_DRDY) != 0)
    {
        LOGERROR("[drv_bmi088] int_map_data write failed");
        return 1;
    }
    return 0;
}

static uint8_t BMI088_GyroInit(BMI088Instance *inst)
{
    // 1. 软复位
    BMI088_WriteReg(inst, inst->cs_gyro, BMI088_GYRO_SOFTRESET_REG, BMI088_GYRO_SOFTRESET_CMD);
    DWT_Delay(BMI088_GYRO_RESET_DELAY_S);

    // 2. 检查 CHIP_ID
    BMI088_ReadReg(inst, inst->cs_gyro, BMI088_GYRO_CHIP_ID_REG, 1, 0);
    if (inst->spi_inst->rx_buff[BMI088_GYRO_RX_DATA_OFF] != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGERROR("[drv_bmi088] Gyro CHIP_ID error: 0x%02X", inst->spi_inst->rx_buff[1]);
        return 1;
    }

    // 3. 写入量程配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_RANGE_REG, (uint8_t)inst->gyro_range) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_range write failed");
        return 1;
    }

    // 4. 写入带宽配置
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_BANDWIDTH_REG, inst->gyro_odr | inst->gyro_bw | BMI088_GYRO_BW_MUST_SET) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_bandwidth write failed");
        return 1;
    }

    // 5. 写入电源模式
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_LPM1_REG, BMI088_GYRO_PM_NORMAL) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_lpm1 write failed");
        return 1;
    }

    // 6. 配置中断控制
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT_CTRL_REG, BMI088_GYRO_INT_DATA_EN) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_int_ctrl write failed");
        return 1;
    }

    // 7. 配置 INT3 引脚
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT3_INT4_IO_CONF_REG, BMI088_INT3_LVL_HIGH) != 0)
    {
        LOGERROR("[drv_bmi088] int3_int4_io_conf write failed");
        return 1;
    }

    // 8. 配置 INT3 中断映射
    if (BMI088_WriteRegWithCheck(inst, inst->cs_gyro, BMI088_GYRO_INT3_INT4_IO_MAP_REG, BMI088_INT3_DATA) != 0)
    {
        LOGERROR("[drv_bmi088] int3_int4_io_map write failed");
        return 1;
    }
    return 0;
}

/*============================ 公开接口实现 ============================*/

int8_t BMI088Register(BMI088Instance *inst, const BMI088_Init_Config_s *config)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return -1;
    }

    if (config == NULL)
    {
        LOGERROR("[drv_bmi088] Config is NULL");
        return -1;
    }

    if (inst->spi_inst == NULL)
    {
        LOGERROR("[drv_bmi088] spi_inst is NULL");
        return -1;
    }
    if (inst->cs_acc == NULL)
    {
        LOGERROR("[drv_bmi088] cs_acc is NULL");
        return -1;
    }
    if (inst->cs_gyro == NULL)
    {
        LOGERROR("[drv_bmi088] cs_gyro is NULL");
        return -1;
    }
    if (inst->int_acc == NULL)
    {
        LOGERROR("[drv_bmi088] int_acc is NULL");
        return -1;
    }
    if (inst->int_gyro == NULL)
    {
        LOGERROR("[drv_bmi088] int_gyro is NULL");
        return -1;
    }
    if (inst->heater_pwm == NULL)
    {
        LOGERROR("[drv_bmi088] heater_pwm is NULL");
        return -1;
    }

    // 设置 parent 指针
    inst->spi_inst->parent = inst;
    inst->cs_acc->parent = inst;
    inst->cs_gyro->parent = inst;
    inst->int_acc->parent = inst;
    inst->int_gyro->parent = inst;

    SPI_Init_Config_s spi_cfg = {.spi_e = config->spi_e, .work_mode = SPI_BLOCK_MODE, .rx_callback = NULL};
    if (SPIRegister(inst->spi_inst, &spi_cfg) != 0)
    {
        LOGERROR("[drv_bmi088] SPI register failed");
        return -1;
    }

    GPIO_Init_Config_s gpio_cs_acc = {.gpio_e = config->cs_acc_e, .callback = NULL};
    if (GPIORegister(inst->cs_acc, &gpio_cs_acc) != 0)
    {
        LOGERROR("[drv_bmi088] cs_acc register failed");
        return -1;
    }

    GPIO_Init_Config_s gpio_cs_gyro = {.gpio_e = config->cs_gyro_e, .callback = NULL};
    if (GPIORegister(inst->cs_gyro, &gpio_cs_gyro) != 0)
    {
        LOGERROR("[drv_bmi088] cs_gyro register failed");
        return -1;
    }

    // 某些板级默认将 CS 拉低，上电后先释放两个片选，避免总线冲突。
    // 同时给加速度计一个 CS 上升沿，触发其由 I2C 切换到 SPI。
    GPIOSet(inst->cs_acc);
    GPIOSet(inst->cs_gyro);
    DWT_Delay(BMI088_CS_RELEASE_DELAY_S); // 修改为合适时间，使用宏定义

    PWM_Init_Config_s pwm_heater = {.tim_e = config->heater_e};
    if (PWMRegister(inst->heater_pwm, &pwm_heater) != 0)
    {
        LOGERROR("[drv_bmi088] heater_pwm register failed");
        return -1;
    }

    // 保存用户配置
    inst->acc_range = config->acc_range;
    inst->acc_bwp = config->acc_bwp;
    inst->acc_odr = config->acc_odr;
    inst->gyro_range = config->gyro_range;
    inst->gyro_odr = config->gyro_odr;
    inst->gyro_bw = config->gyro_bw;
    inst->work_mode = config->work_mode;

    /*============================ 加速度计初始化 ============================*/

    if (BMI088_AccInit(inst) != 0)
        return -1;

    /*============================ 陀螺仪初始化 ============================*/

    if (BMI088_GyroInit(inst) != 0)
        return -1;

    // 中断模式
    if (inst->work_mode == BMI088_MODE_INT)
    {
        GPIO_Init_Config_s gpio_int_acc = {.gpio_e = config->int_acc_e, .callback = BMI088_IntCallback};
        if (GPIORegister(inst->int_acc, &gpio_int_acc) != 0)
        {
            LOGERROR("[drv_bmi088] int_acc register failed");
            return -1;
        }
        GPIO_Init_Config_s gpio_int_gyro = {.gpio_e = config->int_gyro_e, .callback = BMI088_IntCallback};
        if (GPIORegister(inst->int_gyro, &gpio_int_gyro) != 0)
        {
            LOGERROR("[drv_bmi088] int_gyro register failed");
            return -1;
        }
        // 挂接SPI IT传输完成回调，切换为IT模式
        inst->spi_inst->rx_callback = BMI088_SPICpltCallback;
        inst->spi_inst->work_mode = SPI_DMA_MODE;

        // 初始化中断模式字段
        inst->transfer_busy = 0;
        inst->current_sensor = BMI088_SENSOR_ACC;
        inst->pending_mask = 0;
        inst->int_timestamp = 0;
        inst->pending_t_acc = 0;
        inst->pending_t_gyro = 0;
        inst->acc_wr_idx = 0;
        inst->gyro_wr_idx = 0;
        inst->acc_cnt = 0;
        inst->gyro_cnt = 0;
        inst->gyro_temp_raw = 0;
        memset(inst->acc_raw, 0, sizeof(inst->acc_raw));
        memset(inst->gyro_raw, 0, sizeof(inst->gyro_raw));
        memset(inst->t_acc, 0, sizeof(inst->t_acc));
        memset(inst->t_gyro, 0, sizeof(inst->t_gyro));

        LOGINFO("[drv_bmi088] BMI088 INT mode initialized");
        return 0;
    }
    else
    {
        GPIO_Init_Config_s gpio_int_acc = {.gpio_e = config->int_acc_e, .callback = NULL};
        if (GPIORegister(inst->int_acc, &gpio_int_acc) != 0)
        {
            LOGERROR("[drv_bmi088] int_acc register failed");
            return -1;
        }
        GPIO_Init_Config_s gpio_int_gyro = {.gpio_e = config->int_gyro_e, .callback = NULL};
        if (GPIORegister(inst->int_gyro, &gpio_int_gyro) != 0)
        {
            LOGERROR("[drv_bmi088] int_gyro register failed");
            return -1;
        }
    }

    LOGINFO("[drv_bmi088] BMI088 init success");
    return 0;
}

/*============================ 数据读取接口 ============================*/

/**
 * @brief 阻塞读取BMI088数据
 * @param inst BMI088实例指针
 * @return BMI088_Data_t 结构体，包含加速度、陀螺仪、温度和时间戳
 * @note 此函数会阻塞等待数据读取完成
 */
BMI088_Data_t BMI088ReadBlocking(BMI088Instance *inst)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return (BMI088_Data_t){0};
    }

    if (inst->work_mode == BMI088_MODE_INT)
    {
        LOGWARNING("[drv_bmi088] BMI088ReadBlocking called in INT mode, use BMI088ReadInt instead");
        return (BMI088_Data_t){0};
    }

    /* 读取加速度计原始数据（X/Y/Z 三轴，6 字节） */
    BMI088_ReadReg(inst, inst->cs_acc, BMI088_ACCEL_XOUT_L, BMI088_RAW_DATA_SIZE, BMI088_ACC_DUMMY_BYTES);
    uint8_t acc_buf[BMI088_RAW_DATA_SIZE];
    for (uint8_t i = 0; i < BMI088_RAW_DATA_SIZE; i++)
        acc_buf[i] = inst->spi_inst->rx_buff[BMI088_ACC_RX_DATA_OFF + i];

    /* 读取陀螺仪原始数据（X/Y/Z 三轴，6 字节） */
    BMI088_ReadReg(inst, inst->cs_gyro, BMI088_GYRO_X_L, BMI088_RAW_DATA_SIZE, BMI088_GYRO_DUMMY_BYTES);
    uint8_t gyro_buf[BMI088_RAW_DATA_SIZE];
    for (uint8_t i = 0; i < BMI088_RAW_DATA_SIZE; i++)
        gyro_buf[i] = inst->spi_inst->rx_buff[BMI088_GYRO_RX_DATA_OFF + i];

    /* 读取温度原始数据（2 字节），存入实例供 heater 控制使用 */
    BMI088_ReadReg(inst, inst->cs_acc, BMI088_TEMP_L, 2, BMI088_ACC_DUMMY_BYTES);
    inst->gyro_temp_raw = (int16_t)((inst->spi_inst->rx_buff[BMI088_ACC_RX_TEMP_H] << 8) | inst->spi_inst->rx_buff[BMI088_ACC_RX_TEMP_L]);

    return BMI088_PackData(inst, acc_buf, gyro_buf, DWT_GetTimeUs());
}

/*============================ 中断模式公开接口实现 ============================*/

BMI088_Data_t BMI088ReadInt(BMI088Instance *inst)
{
    if (inst == NULL)
        return (BMI088_Data_t){0};
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

    return BMI088_PackData(inst, best_acc, best_gyro, best_ts);
}

/*============================ 多速率数据读取接口 ============================*/

BMI088_MultiRateData_t BMI088ReadLatest(BMI088Instance *inst)
{
    BMI088_MultiRateData_t data = {0};
    if (inst == NULL)
        return data;
    if (inst->acc_cnt == 0 || inst->gyro_cnt == 0)
        return data;

    /* 快照最新帧索引 */
    uint16_t a_idx = ((inst->acc_wr_idx + BMI088_ACC_BUF_SIZE - 1) % BMI088_ACC_BUF_SIZE);
    uint16_t g_idx = ((inst->gyro_wr_idx + BMI088_GYRO_BUF_SIZE - 1) % BMI088_GYRO_BUF_SIZE);

    data.time_stamp_a = inst->t_acc[a_idx];
    data.time_stamp_g = inst->t_gyro[g_idx];

    if (data.time_stamp_a == 0 || data.time_stamp_g == 0)
        return (BMI088_MultiRateData_t){0};

    BMI088_RawToAcc(inst->acc_raw[a_idx], data.acc, inst->acc_range, inst->acc_offset);
    BMI088_RawToGyro(inst->gyro_raw[g_idx], data.gyro, inst->gyro_range, inst->gyro_offset);

    return data;
}

#endif /* defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */