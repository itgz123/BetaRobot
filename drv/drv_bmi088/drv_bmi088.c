/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"

#if defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include <string.h>

/*============================ SPI通信宏定义 ============================*/

#define BMI088_SPI_READ_CMD 0x80   // R/W位=1（读）
#define BMI088_SPI_WRITE_CMD 0x7F  // R/W位=0（写）
#define BMI088_SPI_DUMMY_BYTE 0x55 // 哑指令（用于发送时填充）

/*============================ 延时时间宏定义 ============================*/

#define BMI088_DELAY_MS 0.08f // 统一延时80ms

/*============================ 标定参数宏定义 ============================*/

#define BMI088_CALIB_SAMPLES 100     // 默认标定采样次数
#define BMI088_CALIB_INTERVAL 0.002f // 标定采样间隔 (s)

/*============================ 温度转换参数 ============================*/

#define BMI088_TEMP_SENS 0.125f  // 温度灵敏度 (°C/LSB)
#define BMI088_TEMP_OFFSET 23.0f // 温度偏移 (°C)

/*============================ 物理常量 ============================*/

#define BMI088_GRAVITY 9.80665f // 重力加速度 (m/s²)

/*============================ 灵敏度查找表 ============================*/

/* 加速度计灵敏度查找表：将原始值转换为 g，单位 g/LSB */
const float BMI088_AccSenTable[BMI088_ACC_RANGE_NUM] = {
    0.0008974358974f,  // ±3g 量程灵敏度
    0.00179443359375f, // ±6g 量程灵敏度
    0.0035888671875f,  // ±12g 量程灵敏度
    0.007177734375f,   // ±24g 量程灵敏度
};

/* 陀螺仪灵敏度查找表：将原始值转换为 rad/s，单位 rad/s/LSB */
const float BMI088_GyroSenTable[BMI088_GYRO_RANGE_NUM] = {
    0.00106526443603169529841533860381f,     // ±2000°/s 量程灵敏度
    0.00053263221801584764920766930190693f,  // ±1000°/s 量程灵敏度
    0.00026631610900792382460383465095346f,  // ±500°/s 量程灵敏度
    0.00013315805450396191230191732547673f,  // ±250°/s 量程灵敏度
    0.000066579027251980956150958662738366f, // ±125°/s 量程灵敏度
};

/*============================ 私有函数声明 ============================*/

static void BMI088_AccReadReg(BMI088Instance *inst, uint8_t reg, uint16_t len);
static void BMI088_AccWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_AccWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data);
static void BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg, uint16_t len);
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_GyroWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data);

/*============================ 私有函数实现 ============================*/

/**
 * @brief 加速度计读取寄存器
 * @note 数据存入 inst->spi_inst.rx_buff，从 rx_buff[2] 开始为有效数据
 */
static void BMI088_AccReadReg(BMI088Instance *inst, uint8_t reg, uint16_t len)
{
    if (len > 6)
    {
        LOGWARNING("[drv_bmi088] Acc read len > 6: %d", len);
        return;
    }

    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;
    inst->tx_len = 1 + 1 + len;

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, inst->tx_len);
    GPIOSet(&inst->cs_acc);
}

/**
 * @brief 加速度计写入寄存器（无验证）
 */
static void BMI088_AccWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = data;
    inst->tx_len = 2;

    GPIOReset(&inst->cs_acc);
    SPITransmit(&inst->spi_inst, inst->tx_buff, inst->tx_len);
    GPIOSet(&inst->cs_acc);
}

/**
 * @brief 加速度计写入寄存器并验证
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_AccWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    BMI088_AccWriteReg(inst, reg, data);
    DWT_Delay(BMI088_DELAY_MS);

    BMI088_AccReadReg(inst, reg, 1);
    return (inst->spi_inst.rx_buff[2] != data);
}

/**
 * @brief 陀螺仪读取寄存器
 * @note 数据存入 inst->spi_inst.rx_buff，从 rx_buff[1] 开始为有效数据
 */
static void BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg, uint16_t len)
{
    if (len > 6)
    {
        LOGWARNING("[drv_bmi088] Gyro read len > 6: %d", len);
        return;
    }

    inst->tx_buff[0] = reg | BMI088_SPI_READ_CMD;
    inst->tx_len = 1 + len;

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, inst->tx_len);
    GPIOSet(&inst->cs_gyro);
}

/**
 * @brief 陀螺仪写入寄存器（无验证）
 */
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    inst->tx_buff[0] = reg & BMI088_SPI_WRITE_CMD;
    inst->tx_buff[1] = data;
    inst->tx_len = 2;

    GPIOReset(&inst->cs_gyro);
    SPITransmit(&inst->spi_inst, inst->tx_buff, inst->tx_len);
    GPIOSet(&inst->cs_gyro);
}

/**
 * @brief 陀螺仪写入寄存器并验证
 * @return 0-成功，非0-失败
 */
static uint8_t BMI088_GyroWriteRegWithCheck(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    BMI088_GyroWriteReg(inst, reg, data);
    DWT_Delay(BMI088_DELAY_MS);

    BMI088_GyroReadReg(inst, reg, 1);
    return (inst->spi_inst.rx_buff[1] != data);
}

/*============================ 公开接口实现 ============================*/

int8_t BMI088Register(BMI088Instance *inst)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return -1;
    }

    if (SPIRegister(&inst->spi_inst) != 0)
    {
        LOGERROR("[drv_bmi088] SPI register failed");
        return -1;
    }

    if (GPIORegister(&inst->cs_acc) != 0)
    {
        LOGERROR("[drv_bmi088] cs_acc register failed");
        return -1;
    }

    if (GPIORegister(&inst->cs_gyro) != 0)
    {
        LOGERROR("[drv_bmi088] cs_gyro register failed");
        return -1;
    }

    // 某些板级默认将 CS 拉低，上电后先释放两个片选，避免总线冲突。
    // 同时给加速度计一个 CS 上升沿，触发其由 I2C 切换到 SPI。
    GPIOSet(&inst->cs_acc);
    GPIOSet(&inst->cs_gyro);
    DWT_Delay(BMI088_DELAY_MS);

    if (GPIORegister(&inst->int_acc) != 0)
    {
        LOGERROR("[drv_bmi088] int_acc register failed");
        return -1;
    }
    if (GPIORegister(&inst->int_gyro) != 0)
    {
        LOGERROR("[drv_bmi088] int_gyro register failed");
        return -1;
    }

    if (PWMRegister(&inst->heater_pwm) != 0)
    {
        LOGERROR("[drv_bmi088] heater_pwm register failed");
        return -1;
    }

    LOGINFO("[drv_bmi088] BMI088 instance registered");
    return 0;
}

int8_t BMI088SetConfig(BMI088Instance *inst, BMI088_AccRange_e acc_range, uint8_t acc_bwp, uint8_t acc_odr, BMI088_GyroRange_e gyro_range, uint8_t gyro_odr, uint8_t gyro_bw, BMI088_WorkMode_e work_mode)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return -1;
    }

    // 保存用户配置
    inst->acc_range = acc_range;
    inst->acc_bwp = acc_bwp;
    inst->acc_odr = acc_odr;
    inst->gyro_range = gyro_range;
    inst->gyro_odr = gyro_odr;
    inst->gyro_bw = gyro_bw;
    inst->work_mode = work_mode;

    /*============================ 加速度计初始化 ============================*/

    // 1. 切换到SPI模式
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, 1);
    DWT_Delay(BMI088_DELAY_MS);

    // 2. 软复位
    BMI088_AccWriteReg(inst, BMI088_ACC_SOFTRESET_REG, BMI088_ACC_SOFTRESET_CMD);
    DWT_Delay(BMI088_DELAY_MS);

    // 3. 再次切换到SPI模式
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, 1);
    DWT_Delay(BMI088_DELAY_MS);

    // 4. 检查 CHIP_ID
    BMI088_AccReadReg(inst, BMI088_ACC_CHIP_ID_REG, 1);
    if (inst->spi_inst.rx_buff[2] != BMI088_ACC_CHIP_ID_VAL)
    {
        LOGERROR("[drv_bmi088] Acc CHIP_ID error: 0x%02X", inst->spi_inst.rx_buff[2]);
        return -1;
    }
    DWT_Delay(BMI088_DELAY_MS);

    // 5. 开启加速度计
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_PWR_CTRL_REG, BMI088_ACC_ENABLE_ON) != 0)
    {
        LOGERROR("[drv_bmi088] acc_pwr_ctrl write failed");
        return -1;
    }

    // 6. 退出挂起模式
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_PWR_CONF_REG, BMI088_ACC_PWR_SAVE_ACTIVE) != 0)
    {
        LOGERROR("[drv_bmi088] acc_pwr_conf write failed");
        return -1;
    }

    // 7. 写入量程配置
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_RANGE_REG, (uint8_t)acc_range) != 0)
    {
        LOGERROR("[drv_bmi088] acc_range write failed");
        return -1;
    }

    // 8. 写入滤波器和ODR配置
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_ACC_CONF_REG, acc_bwp | acc_odr) != 0)
    {
        LOGERROR("[drv_bmi088] acc_conf write failed");
        return -1;
    }

    // 9. 配置 INT1 引脚
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_INT1_IO_CTRL_REG, BMI088_INT1_OUT | BMI088_INT1_LVL_HIGH) != 0)
    {
        LOGERROR("[drv_bmi088] int1_io_ctrl write failed");
        return -1;
    }

    // 10. 配置中断映射
    if (BMI088_AccWriteRegWithCheck(inst, BMI088_INT_MAP_DATA_REG, BMI088_INT1_DRDY) != 0)
    {
        LOGERROR("[drv_bmi088] int_map_data write failed");
        return -1;
    }

    /*============================ 陀螺仪初始化 ============================*/

    // 1. 软复位
    BMI088_GyroWriteReg(inst, BMI088_GYRO_SOFTRESET_REG, BMI088_GYRO_SOFTRESET_CMD);
    DWT_Delay(BMI088_DELAY_MS);

    // 2. 检查 CHIP_ID
    BMI088_GyroReadReg(inst, BMI088_GYRO_CHIP_ID_REG, 1);
    if (inst->spi_inst.rx_buff[1] != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGERROR("[drv_bmi088] Gyro CHIP_ID error: 0x%02X", inst->spi_inst.rx_buff[1]);
        return -1;
    }

    // 3. 写入量程配置
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_RANGE_REG, (uint8_t)gyro_range) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_range write failed");
        return -1;
    }

    // 4. 写入带宽配置（bit[7]必须写入1）
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_BANDWIDTH_REG, gyro_odr | gyro_bw | BMI088_GYRO_BW_MUST_SET) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_bandwidth write failed");
        return -1;
    }

    // 5. 写入电源模式
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_LPM1_REG, BMI088_GYRO_PM_NORMAL) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_lpm1 write failed");
        return -1;
    }

    // 6. 配置中断控制
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT_CTRL_REG, BMI088_GYRO_INT_DATA_EN) != 0)
    {
        LOGERROR("[drv_bmi088] gyro_int_ctrl write failed");
        return -1;
    }

    // 7. 配置 INT3 引脚
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT3_INT4_IO_CONF_REG, BMI088_INT3_LVL_HIGH) != 0)
    {
        LOGERROR("[drv_bmi088] int3_int4_io_conf write failed");
        return -1;
    }

    // 8. 配置 INT3 中断映射
    if (BMI088_GyroWriteRegWithCheck(inst, BMI088_GYRO_INT3_INT4_IO_MAP_REG, BMI088_INT3_DATA) != 0)
    {
        LOGERROR("[drv_bmi088] int3_int4_io_map write failed");
        return -1;
    }

    // 执行零偏标定
    if (BMI088Calibrate(inst, BMI088_CALIB_SAMPLES) != 0)
    {
        LOGWARNING("[drv_bmi088] Calibration failed, using default offset");
    }

    // 中断模式
    if (work_mode == BMI088_MODE_INT)
    {
        LOGWARNING("[drv_bmi088] INT mode not support now!");
        // 初始化完毕，切换为dma模式
        // inst->spi_inst.work_mode = SPI_DMA_MODE;
        // 设置回调函数
        return -1;
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
    BMI088_Data_t data = {0};
    int16_t raw_data[3] = {0};
    int16_t raw_temp = 0;

    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return data;
    }

    /* 读取加速度计数据（X/Y/Z 三轴，6 字节） */
    BMI088_AccReadReg(inst, BMI088_ACCEL_XOUT_L, 6);
    raw_data[0] = (int16_t)((inst->spi_inst.rx_buff[3] << 8) | inst->spi_inst.rx_buff[2]);
    raw_data[1] = (int16_t)((inst->spi_inst.rx_buff[5] << 8) | inst->spi_inst.rx_buff[4]);
    raw_data[2] = (int16_t)((inst->spi_inst.rx_buff[7] << 8) | inst->spi_inst.rx_buff[6]);

    /* 转换为物理单位并减去标定偏移量 */
    float acc_sen = BMI088_AccSenTable[inst->acc_range];
    data.acc[0] = (float)raw_data[0] * acc_sen * BMI088_GRAVITY - inst->acc_offset[0];
    data.acc[1] = (float)raw_data[1] * acc_sen * BMI088_GRAVITY - inst->acc_offset[1];
    data.acc[2] = (float)raw_data[2] * acc_sen * BMI088_GRAVITY - inst->acc_offset[2];

    /* 读取陀螺仪数据（X/Y/Z 三轴，6 字节） */
    BMI088_GyroReadReg(inst, BMI088_GYRO_X_L, 6);
    raw_data[0] = (int16_t)((inst->spi_inst.rx_buff[2] << 8) | inst->spi_inst.rx_buff[1]);
    raw_data[1] = (int16_t)((inst->spi_inst.rx_buff[4] << 8) | inst->spi_inst.rx_buff[3]);
    raw_data[2] = (int16_t)((inst->spi_inst.rx_buff[6] << 8) | inst->spi_inst.rx_buff[5]);

    /* 转换为物理单位并减去标定偏移量 */
    float gyro_sen = BMI088_GyroSenTable[inst->gyro_range];
    data.gyro[0] = (float)raw_data[0] * gyro_sen - inst->gyro_offset[0];
    data.gyro[1] = (float)raw_data[1] * gyro_sen - inst->gyro_offset[1];
    data.gyro[2] = (float)raw_data[2] * gyro_sen - inst->gyro_offset[2];

    /* 读取温度数据（2 字节） */
    BMI088_AccReadReg(inst, BMI088_TEMP_L, 2);
    raw_temp = (int16_t)((inst->spi_inst.rx_buff[3] << 8) | inst->spi_inst.rx_buff[2]);

    /* 温度转换公式 */
    data.temp = (float)raw_temp * BMI088_TEMP_SENS + BMI088_TEMP_OFFSET;

    /* 记录时间戳 */
    data.time_stamp = DWT_GetTimeline_ms();

    return data;
}

/*============================ 标定接口 ============================*/

/**
 * @brief 标定BMI088零偏
 * @param inst    BMI088实例指针
 * @param samples 采样次数（建议100~200）
 * @return 0成功，-1失败
 * @note 调用时传感器应处于静止状态
 */
int8_t BMI088Calibrate(BMI088Instance *inst, uint16_t samples)
{
    if (inst == NULL || samples == 0)
    {
        LOGERROR("[drv_bmi088] Invalid params for calibration");
        return -1;
    }

    float acc_sum[3] = {0.0f, 0.0f, 0.0f};
    float gyro_sum[3] = {0.0f, 0.0f, 0.0f};
    int16_t raw_data[3] = {0};

    LOGINFO("[drv_bmi088] Start calibration, samples: %d", samples);

    /* 采集 N 次静止状态下的数据 */
    for (uint16_t i = 0; i < samples; i++)
    {
        /* 读取加速度计数据 */
        BMI088_AccReadReg(inst, BMI088_ACCEL_XOUT_L, 6);
        raw_data[0] = (int16_t)((inst->spi_inst.rx_buff[3] << 8) | inst->spi_inst.rx_buff[2]);
        raw_data[1] = (int16_t)((inst->spi_inst.rx_buff[5] << 8) | inst->spi_inst.rx_buff[4]);
        raw_data[2] = (int16_t)((inst->spi_inst.rx_buff[7] << 8) | inst->spi_inst.rx_buff[6]);

        float acc_sen = BMI088_AccSenTable[inst->acc_range];
        acc_sum[0] += (float)raw_data[0] * acc_sen * BMI088_GRAVITY;
        acc_sum[1] += (float)raw_data[1] * acc_sen * BMI088_GRAVITY;
        acc_sum[2] += (float)raw_data[2] * acc_sen * BMI088_GRAVITY;

        /* 读取陀螺仪数据 */
        BMI088_GyroReadReg(inst, BMI088_GYRO_X_L, 6);
        raw_data[0] = (int16_t)((inst->spi_inst.rx_buff[2] << 8) | inst->spi_inst.rx_buff[1]);
        raw_data[1] = (int16_t)((inst->spi_inst.rx_buff[4] << 8) | inst->spi_inst.rx_buff[3]);
        raw_data[2] = (int16_t)((inst->spi_inst.rx_buff[6] << 8) | inst->spi_inst.rx_buff[5]);

        float gyro_sen = BMI088_GyroSenTable[inst->gyro_range];
        gyro_sum[0] += (float)raw_data[0] * gyro_sen;
        gyro_sum[1] += (float)raw_data[1] * gyro_sen;
        gyro_sum[2] += (float)raw_data[2] * gyro_sen;

        DWT_Delay(BMI088_CALIB_INTERVAL);
    }

    /* 计算平均值作为零偏 */
    inst->acc_offset[0] = acc_sum[0] / (float)samples;
    inst->acc_offset[1] = acc_sum[1] / (float)samples;
    inst->acc_offset[2] = acc_sum[2] / (float)samples;

    inst->gyro_offset[0] = gyro_sum[0] / (float)samples;
    inst->gyro_offset[1] = gyro_sum[1] / (float)samples;
    inst->gyro_offset[2] = gyro_sum[2] / (float)samples;

    LOGINFO("[drv_bmi088] Calibration done");

    return 0;
}

/*============================ 加热控制接口 ============================*/

/**
 * @brief 设置加热PWM占空比
 * @param inst        BMI088实例指针
 * @param duty_ratio  占空比（0~1）
 * @note 阻塞模式直接设置，中断模式预留PID控制接口
 */
void BMI088SetHeater(BMI088Instance *inst, float duty_ratio)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_bmi088] Instance is NULL");
        return;
    }

    /* 钳位占空比范围 */
    if (duty_ratio < 0.0f)
        duty_ratio = 0.0f;
    if (duty_ratio > 1.0f)
        duty_ratio = 1.0f;

    /* 阻塞模式：直接设置 PWM 占空比 */
    if (inst->work_mode == BMI088_MODE_POLLING)
    {
        PWMSetDutyRatio(&inst->heater_pwm, duty_ratio);
    }
    else
    {
        /* 中断模式：预留 PID 控制接口（后续实现） */
        PWMSetDutyRatio(&inst->heater_pwm, duty_ratio);
    }
}

#endif /* defined(HAL_SPI_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */