/**
 * @file drv_bmi088.c
 * @brief BMI088 六轴 IMU 驱动实现
 *
 */

#include "drv_bmi088.h"

/*============================ 私有宏定义 ============================*/

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/*============================ 私有变量 ============================*/

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

/* 预标定参数（可通过宏定义配置） */
#ifndef BMI088_PRE_GYRO_OFFSET_X
#define BMI088_PRE_GYRO_OFFSET_X 0.0f
#define BMI088_PRE_GYRO_OFFSET_Y 0.0f
#define BMI088_PRE_GYRO_OFFSET_Z 0.0f
#define BMI088_PRE_G_NORM 9.81f
#endif

/* 温度读取周期（每 N 次 IMU 读取读取一次温度） */
#define TEMP_READ_PERIOD 10

/*============================ 私有函数声明 ============================*/

static void BMI088_SPI_RxCallback(SPIInstance *spi);
static void BMI088_SPI_ErrorCallback(SPIInstance *spi);
static void BMI088_AccINTCallback(GPIOInstance *gpio);

static void BMI088_AccelWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data);
static uint8_t BMI088_AccelReadReg(BMI088Instance *inst, uint8_t reg);
static uint8_t BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg);

static uint8_t BMI088_AccelWriteAndVerify(BMI088Instance *inst, uint8_t reg,
                                          uint8_t value, uint8_t error_code);
static uint8_t BMI088_GyroWriteAndVerify(BMI088Instance *inst, uint8_t reg,
                                         uint8_t value, uint8_t error_code);

static uint8_t BMI088_AccelInit(BMI088Instance *inst);
static uint8_t BMI088_GyroInit(BMI088Instance *inst);

static void BMI088_StartAccelRead(BMI088Instance *inst);
static void BMI088_StartGyroRead(BMI088Instance *inst);
static void BMI088_StartTempRead(BMI088Instance *inst);

static void BMI088_ParseAccelData(BMI088Instance *inst);
static void BMI088_ParseGyroData(BMI088Instance *inst);
static void BMI088_ParseTempData(BMI088Instance *inst);

/*============================ 私有函数实现 ============================*/

/**
 * @brief 加速度计寄存器写入并验证
 * @param inst       实例指针
 * @param reg        寄存器地址
 * @param value      写入值
 * @param error_code 错误码
 * @retval 0 成功
 * @retval error_code 失败
 */
static uint8_t BMI088_AccelWriteAndVerify(BMI088Instance *inst, uint8_t reg,
                                          uint8_t value, uint8_t error_code)
{
    BMI088_AccelWriteReg(inst, reg, value);
    DWT_Delay(BMI088_INIT_DELAY_MS / 1000.0f);

    if (BMI088_AccelReadReg(inst, reg) != value)
    {
        LOGWARNING("[BMI088] Accel reg 0x%02X write failed", reg);
        return error_code;
    }
    return BMI088_NO_ERROR;
}

/**
 * @brief 陀螺仪寄存器写入并验证
 * @param inst       实例指针
 * @param reg        寄存器地址
 * @param value      写入值
 * @param error_code 错误码
 * @retval 0 成功
 * @retval error_code 失败
 */
static uint8_t BMI088_GyroWriteAndVerify(BMI088Instance *inst, uint8_t reg,
                                         uint8_t value, uint8_t error_code)
{
    BMI088_GyroWriteReg(inst, reg, value);
    DWT_Delay(BMI088_INIT_DELAY_MS / 1000.0f);

    if (BMI088_GyroReadReg(inst, reg) != value)
    {
        LOGWARNING("[BMI088] Gyro reg 0x%02X write failed", reg);
        return error_code;
    }
    return BMI088_NO_ERROR;
}

/**
 * @brief SPI DMA 接收完成回调
 */
static void BMI088_SPI_RxCallback(SPIInstance *spi)
{
    BMI088Instance *inst = container_of(spi, BMI088Instance, spi_inst);

    /* 重置超时计数器 */
    inst->state_timeout_cnt = 0;

    switch (inst->read_state)
    {
    case BMI088_STATE_ACC_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_acc);
        /* 解析加速度计数据 */
        BMI088_ParseAccelData(inst);
        inst->acc_updated = 1;

        /* 判断是否需要读取温度 */
        inst->temp_read_cnt++;
        if (inst->temp_read_cnt >= TEMP_READ_PERIOD)
        {
            inst->temp_read_cnt = 0;
            inst->read_state = BMI088_STATE_TEMP_READ;
            BMI088_StartTempRead(inst);
        }
        else
        {
            /* 启动陀螺仪读取 */
            inst->read_state = BMI088_STATE_GYRO_READ;
            BMI088_StartGyroRead(inst);
        }
        break;

    case BMI088_STATE_GYRO_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_gyro);
        /* 解析陀螺仪数据 */
        BMI088_ParseGyroData(inst);
        inst->gyro_updated = 1;

        /* 数据就绪 */
        inst->imu_ready = 1;
        inst->read_state = BMI088_STATE_IDLE;

        /* 调用 APP 回调 */
        if (inst->app_callback != NULL)
        {
            inst->app_callback(inst);
        }
        break;

    case BMI088_STATE_TEMP_READ:
        /* 释放 CS */
        GPIOSet(&inst->cs_acc);
        /* 解析温度数据 */
        BMI088_ParseTempData(inst);
        /* 温度控制 */
        if (inst->heater_enabled)
        {
            BMI088TempControl(inst);
        }
        /* 继续读取陀螺仪 */
        inst->read_state = BMI088_STATE_GYRO_READ;
        BMI088_StartGyroRead(inst);
        break;

    default:
        inst->read_state = BMI088_STATE_IDLE;
        break;
    }
}

/**
 * @brief SPI 错误回调
 * @note 在 SPI 传输错误时重置状态机
 */
static void BMI088_SPI_ErrorCallback(SPIInstance *spi)
{
    BMI088Instance *inst = container_of(spi, BMI088Instance, spi_inst);

    /* 释放 CS */
    GPIOSet(&inst->cs_acc);
    GPIOSet(&inst->cs_gyro);

    /* 重置状态机 */
    inst->read_state = BMI088_STATE_IDLE;
    inst->state_timeout_cnt = 0;

    LOGWARNING("[BMI088] SPI error detected, state machine reset");
}

/**
 * @brief 加速度计中断回调
 */
static void BMI088_AccINTCallback(GPIOInstance *gpio)
{
    BMI088Instance *inst = container_of(gpio, BMI088Instance, int_acc);

    /* 超时检测：如果状态机长时间未返回空闲，强制重置 */
    if (inst->read_state != BMI088_STATE_IDLE)
    {
        inst->state_timeout_cnt++;
        if (inst->state_timeout_cnt >= BMI088_STATE_TIMEOUT_MAX)
        {
            /* 强制重置状态机 */
            inst->read_state = BMI088_STATE_IDLE;
            inst->state_timeout_cnt = 0;
            GPIOSet(&inst->cs_acc);
            GPIOSet(&inst->cs_gyro);
            LOGWARNING("[BMI088] State machine timeout, forced reset");
        }
        return;
    }

    /* 清除标志位 */
    inst->imu_ready = 0;
    inst->acc_updated = 0;
    inst->gyro_updated = 0;
    inst->state_timeout_cnt = 0;

    /* 启动加速度计读取 */
    inst->read_state = BMI088_STATE_ACC_READ;
    BMI088_StartAccelRead(inst);
}

/**
 * @brief 写加速度计寄存器（阻塞模式）
 */
static void BMI088_AccelWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = {reg, data};

    GPIOReset(&inst->cs_acc);
    SPITransmit(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_acc);
}

/**
 * @brief 写陀螺仪寄存器（阻塞模式）
 */
static void BMI088_GyroWriteReg(BMI088Instance *inst, uint8_t reg, uint8_t data)
{
    uint8_t tx_buf[2] = {reg, data};

    GPIOReset(&inst->cs_gyro);
    SPITransmit(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_gyro);
}

/**
 * @brief 读加速度计寄存器（阻塞模式）
 * @note 加速度计读取需要 dummy byte
 */
static uint8_t BMI088_AccelReadReg(BMI088Instance *inst, uint8_t reg)
{
    uint8_t tx_buf[3] = {reg | BMI088_SPI_READ_FLAG, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE};

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 3);
    GPIOSet(&inst->cs_acc);

    return inst->spi_inst.rx_buff[2];
}

/**
 * @brief 读陀螺仪寄存器（阻塞模式）
 * @note 陀螺仪读取无 dummy byte
 */
static uint8_t BMI088_GyroReadReg(BMI088Instance *inst, uint8_t reg)
{
    uint8_t tx_buf[2] = {reg | BMI088_SPI_READ_FLAG, BMI088_DUMMY_BYTE};

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, tx_buf, 2);
    GPIOSet(&inst->cs_gyro);

    return inst->spi_inst.rx_buff[1];
}

/**
 * @brief 初始化加速度计
 */
static uint8_t BMI088_AccelInit(BMI088Instance *inst)
{
    uint8_t whoami = 0;
    uint8_t error = BMI088_NO_ERROR;

    /* 切换到 SPI 模式（需要一次 fake read） */
    BMI088_AccelReadReg(inst, BMI088_ACC_CHIP_ID_REG);
    DWT_Delay(BMI088_INIT_DELAY_MS / 1000.0f);

    /* 软复位 */
    BMI088_AccelWriteReg(inst, BMI088_ACC_SOFTRESET_REG, BMI088_ACC_SOFTRESET_VALUE);
    DWT_Delay(BMI088_COM_WAIT_TIME_MS / 1000.0f);

    /* 检查 Who Am I */
    whoami = BMI088_AccelReadReg(inst, BMI088_ACC_CHIP_ID_REG);
    if (whoami != BMI088_ACC_CHIP_ID_VALUE)
    {
        LOGERROR("[BMI088] Accel WhoAmI failed: 0x%02X (expect 0x%02X)",
                 whoami, BMI088_ACC_CHIP_ID_VALUE);
        return BMI088_NO_SENSOR;
    }

    /* 电源控制：使能加速度计（必须等待至少 5ms） */
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_ACC_PWR_CTRL_REG,
                                        BMI088_ACC_ENABLE, BMI088_ACC_PWR_CTRL_ERROR);
    DWT_Delay(BMI088_PWR_CTRL_DELAY_MS / 1000.0f); // 关键：必须等待至少 5ms

    /* 电源配置：激活模式 */
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_ACC_PWR_CONF_REG,
                                        BMI088_ACC_PWR_ACTIVE, BMI088_ACC_PWR_CONF_ERROR);

    /* 配置 ODR 和工作模式 */
    uint8_t acc_conf_val = BMI088_ACC_CONF_MUST_SET | BMI088_ACC_NORMAL_MODE | inst->acc_odr;
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_ACC_CONF_REG,
                                        acc_conf_val, BMI088_ACC_CONF_ERROR);

    /* 配置量程 */
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_ACC_RANGE_REG,
                                        inst->acc_range, BMI088_ACC_RANGE_ERROR);

    /* 设置灵敏度 */
    inst->acc_sen = BMI088_AccSenTable[inst->acc_range];

    /* INT1 引脚配置 */
    uint8_t int1_io_ctrl = BMI088_INT1_IO_ENABLE | BMI088_INT1_GPIO_PP | BMI088_INT1_GPIO_LOW;
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_INT1_IO_CTRL_REG,
                                        int1_io_ctrl, BMI088_INT1_IO_CTRL_ERROR);

    /* 中断映射 */
    error |= BMI088_AccelWriteAndVerify(inst, BMI088_INT_MAP_DATA_REG,
                                        BMI088_INT1_DRDY_INTERRUPT, BMI088_INT_MAP_DATA_ERROR);

    return error;
}

/**
 * @brief 初始化陀螺仪
 */
static uint8_t BMI088_GyroInit(BMI088Instance *inst)
{
    uint8_t whoami = 0;
    uint8_t error = BMI088_NO_ERROR;

    /* 软复位 */
    BMI088_GyroWriteReg(inst, BMI088_GYRO_SOFTRESET_REG, BMI088_GYRO_SOFTRESET_VALUE);
    DWT_Delay(BMI088_COM_WAIT_TIME_MS / 1000.0f);

    /* 检查 Who Am I */
    whoami = BMI088_GyroReadReg(inst, BMI088_GYRO_CHIP_ID_REG);
    if (whoami != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGERROR("[BMI088] Gyro WhoAmI failed: 0x%02X (expect 0x%02X)",
                 whoami, BMI088_GYRO_CHIP_ID_VALUE);
        return BMI088_NO_SENSOR;
    }

    /* 配置量程 */
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_RANGE_REG,
                                       inst->gyro_range, BMI088_GYRO_RANGE_ERROR);

    /* 设置灵敏度 */
    inst->gyro_sen = BMI088_GyroSenTable[inst->gyro_range];

    /* 配置带宽 */
    uint8_t bw_val = BMI088_GYRO_BW_MUST_SET | inst->gyro_bw;
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_BANDWIDTH_REG,
                                       bw_val, BMI088_GYRO_BANDWIDTH_ERROR);

    /* 低功耗模式配置：正常模式 */
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_LPM1_REG,
                                       BMI088_GYRO_NORMAL_MODE, BMI088_GYRO_LPM1_ERROR);

    /* 控制寄存器：数据就绪中断使能 */
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_INT_CTRL_REG,
                                       BMI088_DRDY_ON, BMI088_GYRO_CTRL_ERROR);

    /* INT3/INT4 引脚配置 */
    uint8_t int_conf = BMI088_GYRO_INT3_PP | BMI088_GYRO_INT3_LOW;
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_INT3_INT4_IO_CONF_REG,
                                       int_conf, BMI088_GYRO_INT_CONF_ERROR);

    /* 中断映射 */
    error |= BMI088_GyroWriteAndVerify(inst, BMI088_GYRO_INT3_INT4_IO_MAP_REG,
                                       BMI088_DRDY_IO_INT3, BMI088_GYRO_INT_MAP_ERROR);

    return error;
}

/**
 * @brief 启动加速度计 DMA 读取
 */
static void BMI088_StartAccelRead(BMI088Instance *inst)
{
    /* 构造读取命令：加速度计需要 dummy byte，读取 6 字节数据 */
    inst->tx_buff[0] = BMI088_ACCEL_XOUT_L | BMI088_SPI_READ_FLAG;
    inst->tx_buff[1] = BMI088_DUMMY_BYTE;
    inst->tx_buff[2] = BMI088_DUMMY_BYTE;
    inst->tx_buff[3] = BMI088_DUMMY_BYTE;
    inst->tx_buff[4] = BMI088_DUMMY_BYTE;
    inst->tx_buff[5] = BMI088_DUMMY_BYTE;
    inst->tx_buff[6] = BMI088_DUMMY_BYTE;
    inst->tx_buff[7] = BMI088_DUMMY_BYTE;

    GPIOReset(&inst->cs_acc);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, 8);
    /* CS 在回调中释放 */
}

/**
 * @brief 启动陀螺仪 DMA 读取
 */
static void BMI088_StartGyroRead(BMI088Instance *inst)
{
    /* 构造读取命令：陀螺仪无 dummy byte，读取 6 字节数据 + 1 字节 CHIP_ID 校验 */
    inst->tx_buff[0] = BMI088_GYRO_CHIP_ID_REG | BMI088_SPI_READ_FLAG;
    inst->tx_buff[1] = BMI088_DUMMY_BYTE;
    inst->tx_buff[2] = BMI088_DUMMY_BYTE;
    inst->tx_buff[3] = BMI088_DUMMY_BYTE;
    inst->tx_buff[4] = BMI088_DUMMY_BYTE;
    inst->tx_buff[5] = BMI088_DUMMY_BYTE;
    inst->tx_buff[6] = BMI088_DUMMY_BYTE;
    inst->tx_buff[7] = BMI088_DUMMY_BYTE;

    GPIOReset(&inst->cs_gyro);
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, 8);
    /* CS 在回调中释放 */
}

/**
 * @brief 启动温度 DMA 读取
 */
static void BMI088_StartTempRead(BMI088Instance *inst)
{
    /* 构造读取命令：温度需要 dummy byte，读取 2 字节数据 */
    inst->tx_buff[0] = BMI088_TEMP_M | BMI088_SPI_READ_FLAG;
    inst->tx_buff[1] = BMI088_DUMMY_BYTE;
    inst->tx_buff[2] = BMI088_DUMMY_BYTE;
    inst->tx_buff[3] = BMI088_DUMMY_BYTE;

    GPIOReset(&inst->cs_acc); /* 温度传感器在加速度计上 */
    SPITransmitReceive(&inst->spi_inst, inst->tx_buff, 4);
    /* CS 在回调中释放 */
}

/**
 * @brief 解析加速度计数据
 */
static void BMI088_ParseAccelData(BMI088Instance *inst)
{
    /* rx_buff[0-1] = dummy, rx_buff[2-7] = 数据 */
    uint8_t *rx = inst->spi_inst.rx_buff;

    for (int i = 0; i < 3; i++)
    {
        int16_t raw = (int16_t)((rx[BMI088_ACC_RX_DUMMY_LEN + 2 * i + 1] << 8) |
                                rx[BMI088_ACC_RX_DUMMY_LEN + 2 * i]);
        inst->data.acc[i] = raw * inst->acc_sen * inst->acc_scale;
    }
}

/**
 * @brief 解析陀螺仪数据
 */
static void BMI088_ParseGyroData(BMI088Instance *inst)
{
    /* rx_buff[0] = CHIP_ID, rx_buff[1-6] = 数据 */
    uint8_t *rx = inst->spi_inst.rx_buff;

    /* 验证 CHIP_ID */
    if (rx[BMI088_GYRO_RX_CHIPID_IDX] != BMI088_GYRO_CHIP_ID_VALUE)
    {
        LOGWARNING("[BMI088] Gyro CHIP_ID mismatch: 0x%02X", rx[BMI088_GYRO_RX_CHIPID_IDX]);
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        int16_t raw = (int16_t)((rx[BMI088_GYRO_RX_DATA_IDX + 2 * i + 1] << 8) |
                                rx[BMI088_GYRO_RX_DATA_IDX + 2 * i]);
        inst->data.gyro[i] = raw * inst->gyro_sen - inst->gyro_offset[i];
    }
}

/**
 * @brief 解析温度数据
 */
static void BMI088_ParseTempData(BMI088Instance *inst)
{
    /* rx_buff[0-1] = dummy, rx_buff[2-3] = 数据 */
    uint8_t *rx = inst->spi_inst.rx_buff;
    int16_t raw_temp = (int16_t)((rx[BMI088_TEMP_RX_DUMMY_LEN + 0] << 3) | (rx[BMI088_TEMP_RX_DUMMY_LEN + 1] >> 5));

    /* 处理负温度 */
    if (raw_temp > 1023)
    {
        raw_temp -= 2048;
    }

    inst->data.temp = raw_temp * BMI088_TEMP_FACTOR + BMI088_TEMP_OFFSET;
}

/*============================ 公开接口实现 ============================*/

/**
 * @brief 注册并初始化 BMI088
 */
int8_t BMI088Register(BMI088Instance *inst)
{
    uint8_t error = BMI088_NO_ERROR;

    if (inst == NULL)
    {
        LOGERROR("[BMI088] Instance is NULL");
        return -1;
    }

    /* 暂时使用阻塞模式进行初始化 */
    // SPI_Work_Mode_e saved_mode = inst->spi_inst.work_mode;
    inst->spi_inst.work_mode = SPI_BLOCK_MODE;

    /* 注册 SPI 实例 */
    if (SPIRegister(&inst->spi_inst) != 0)
    {
        LOGERROR("[BMI088] SPI register failed");
        return -1;
    }

    /* 注册 GPIO 实例 */
    if (GPIORegister(&inst->cs_acc) != 0)
    {
        LOGERROR("[BMI088] CS_ACC register failed");
        return -1;
    }
    if (GPIORegister(&inst->cs_gyro) != 0)
    {
        LOGERROR("[BMI088] CS_GYRO register failed");
        return -1;
    }

    /* 初始状态：CS 拉高 */
    GPIOSet(&inst->cs_acc);
    GPIOSet(&inst->cs_gyro);

    /* 初始化加速度计和陀螺仪 */
    error |= BMI088_AccelInit(inst);
    error |= BMI088_GyroInit(inst);

    if (error != BMI088_NO_ERROR)
    {
        LOGERROR("[BMI088] Init failed, error=0x%02X", error);
        return -1;
    }

    /* 标定 */
    if (inst->cali_mode == BMI088_CALI_ONLINE)
    {
        BMI088Calibrate(inst);
    }
    else
    {
        /* 使用预标定参数 */
        inst->gyro_offset[0] = BMI088_PRE_GYRO_OFFSET_X;
        inst->gyro_offset[1] = BMI088_PRE_GYRO_OFFSET_Y;
        inst->gyro_offset[2] = BMI088_PRE_GYRO_OFFSET_Z;
        inst->g_norm = BMI088_PRE_G_NORM;
        inst->acc_scale = 9.81f / inst->g_norm;
    }

    /* 注册加热 PWM（如果启用） */
    if (inst->heater_pwm.tim_e < TIM_NUM_MAX)
    {
        if (PWMRegister(&inst->heater_pwm) == 0)
        {
            inst->heater_enabled = 1;
            LOGINFO("[BMI088] Heater PWM registered");
        }
    }

    /* 恢复工作模式 */
    if (inst->work_mode == BMI088_DMA_MODE)
    {
        inst->spi_inst.work_mode = SPI_DMA_MODE;
        inst->spi_inst.rx_callback = BMI088_SPI_RxCallback;
        inst->spi_inst.error_callback = BMI088_SPI_ErrorCallback;

        /* 注册中断 GPIO */
        inst->int_acc.callback = BMI088_AccINTCallback;
        if (GPIORegister(&inst->int_acc) != 0)
        {
            LOGWARNING("[BMI088] INT_ACC register failed, falling back to polling");
            inst->work_mode = BMI088_BLOCK_MODE;
            inst->spi_inst.work_mode = SPI_BLOCK_MODE;
        }

        /* 陀螺仪中断（可选） */
        if (inst->int_gyro.gpio_e < GPIO_NUM_MAX)
        {
            GPIORegister(&inst->int_gyro);
        }
    }

    LOGINFO("[BMI088] Registered successfully, mode=%s", inst->work_mode == BMI088_DMA_MODE ? "DMA" : "BLOCK");

    return 0;
}

/**
 * @brief 获取 BMI088 数据
 */
uint8_t BMI088GetData(BMI088Instance *inst, BMI088_Data_t *data)
{
    if (inst == NULL || data == NULL)
    {
        return 0;
    }

    if (inst->imu_ready)
    {
        data->gyro[0] = inst->data.gyro[0];
        data->gyro[1] = inst->data.gyro[1];
        data->gyro[2] = inst->data.gyro[2];
        data->acc[0] = inst->data.acc[0];
        data->acc[1] = inst->data.acc[1];
        data->acc[2] = inst->data.acc[2];
        data->temp = inst->data.temp;
        inst->imu_ready = 0;
        return 1;
    }

    return 0;
}

/**
 * @brief 在线标定 BMI088
 */
void BMI088Calibrate(BMI088Instance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    LOGINFO("[BMI088] Starting calibration...");

    /* 暂存工作模式 */
    SPI_Work_Mode_e saved_mode = inst->spi_inst.work_mode;
    inst->spi_inst.work_mode = SPI_BLOCK_MODE;

    float gyro_sum[3] = {0};
    float g_norm_sum = 0;
    float gyro_max[3] = {0}, gyro_min[3] = {0};
    float g_norm_max = 0, g_norm_min = 0;

    uint8_t tx_acc[8] = {
        BMI088_ACCEL_XOUT_L | BMI088_SPI_READ_FLAG,
        BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE,
        BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE};
    uint8_t tx_gyro[8] = {
        BMI088_GYRO_CHIP_ID_REG | BMI088_SPI_READ_FLAG,
        BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE,
        BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE, BMI088_DUMMY_BYTE};

    for (uint16_t i = 0; i < BMI088_CALI_SAMPLES; i++)
    {
        /* 读取加速度计 */
        GPIOReset(&inst->cs_acc);
        SPITransmitReceive(&inst->spi_inst, tx_acc, 8);
        GPIOSet(&inst->cs_acc);

        uint8_t *rx_acc = inst->spi_inst.rx_buff;
        float acc[3];
        for (int j = 0; j < 3; j++)
        {
            int16_t raw = (int16_t)((rx_acc[BMI088_ACC_RX_DUMMY_LEN + j * 2 + 1] << 8) |
                                    rx_acc[BMI088_ACC_RX_DUMMY_LEN + j * 2]);
            acc[j] = raw * inst->acc_sen;
        }

        float g_norm = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
        g_norm_sum += g_norm;

        /* 读取陀螺仪 */
        GPIOReset(&inst->cs_gyro);
        SPITransmitReceive(&inst->spi_inst, tx_gyro, 8);
        GPIOSet(&inst->cs_gyro);

        uint8_t *rx_gyro = inst->spi_inst.rx_buff;
        float gyro[3];
        for (int j = 0; j < 3; j++)
        {
            int16_t raw = (int16_t)((rx_gyro[BMI088_GYRO_RX_DATA_IDX + j * 2 + 1] << 8) |
                                    rx_gyro[BMI088_GYRO_RX_DATA_IDX + j * 2]);
            gyro[j] = raw * inst->gyro_sen;
            gyro_sum[j] += gyro[j];
        }

        /* 更新最大最小值 */
        if (i == 0)
        {
            g_norm_max = g_norm_min = g_norm;
            for (int j = 0; j < 3; j++)
            {
                gyro_max[j] = gyro_min[j] = gyro[j];
            }
        }
        else
        {
            if (g_norm > g_norm_max)
                g_norm_max = g_norm;
            if (g_norm < g_norm_min)
                g_norm_min = g_norm;
            for (int j = 0; j < 3; j++)
            {
                if (gyro[j] > gyro_max[j])
                    gyro_max[j] = gyro[j];
                if (gyro[j] < gyro_min[j])
                    gyro_min[j] = gyro[j];
            }
        }

        /* 检查运动幅度 */
        float g_diff = g_norm_max - g_norm_min;
        if (g_diff > BMI088_CALI_ACC_THRESHOLD)
        {
            LOGWARNING("[BMI088] Calibration interrupted by motion, retrying...");
            i = 0;
            gyro_sum[0] = gyro_sum[1] = gyro_sum[2] = 0;
            g_norm_sum = 0;
            g_norm_max = g_norm_min = 0;
            DWT_Delay(0.1f);
            continue;
        }

        for (int j = 0; j < 3; j++)
        {
            if (gyro_max[j] - gyro_min[j] > BMI088_CALI_GYRO_THRESHOLD)
            {
                LOGWARNING("[BMI088] Calibration interrupted by motion, retrying...");
                i = 0;
                gyro_sum[0] = gyro_sum[1] = gyro_sum[2] = 0;
                g_norm_sum = 0;
                g_norm_max = g_norm_min = 0;
                DWT_Delay(0.1f);
                continue;
            }
        }

        DWT_Delay(BMI088_CALI_INTERVAL_MS / 1000.0f);
    }

    /* 计算平均值 */
    for (int j = 0; j < 3; j++)
    {
        inst->gyro_offset[j] = gyro_sum[j] / BMI088_CALI_SAMPLES;
    }
    inst->g_norm = g_norm_sum / BMI088_CALI_SAMPLES;
    inst->acc_scale = 9.81f / inst->g_norm;

    /* 恢复工作模式 */
    inst->spi_inst.work_mode = saved_mode;

    LOGINFO("[BMI088] Calibration done: gyro_off=[%.4f,%.4f,%.4f], g_norm=%.3f",
            inst->gyro_offset[0], inst->gyro_offset[1], inst->gyro_offset[2], inst->g_norm);
}

/**
 * @brief 温度控制（简单阈值控制）
 */
void BMI088TempControl(BMI088Instance *inst)
{
    if (inst == NULL || !inst->heater_enabled)
    {
        return;
    }

    if (inst->data.temp < inst->target_temp - BMI088_HEATER_HYSTERESIS)
    {
        /* 温度过低，开启加热 */
        PWMSetDutyRatio(&inst->heater_pwm, BMI088_HEATER_DUTY_RATIO);
    }
    else if (inst->data.temp >= inst->target_temp)
    {
        /* 温度达到目标，关闭加热 */
        PWMSetDutyRatio(&inst->heater_pwm, 0.0f);
    }
}
